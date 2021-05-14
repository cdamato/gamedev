#include <common/parser.h>
#include <common/event.h>
#include <cstring>
#include <fstream>
#include "display_impl.h"

namespace display {
#ifdef __linux__
#ifdef OPENGL
void gl_backend::swap_buffers(renderer& r) { glXSwapBuffers(dpy, win); }
void gl_backend::set_resolution(screen_coords coords) { glViewport(0, 0, coords.x, coords.y); }
gl_backend::gl_backend(screen_coords resolution) : x11_window(resolution) {
    glc = glXCreateContext(dpy, vi, NULL, true);
    glXMakeCurrent(dpy, win, glc);
}
#endif //OPENGL


// Note: this code has problems, as does the windows version.
// I believe the buffer needs to be recreated upon window resizing.
// This isn't as simple as it sounds - when XShmCreateImage is put in its own function, the code fails.
// This suggests UB, which I don't want to track down right now.
software_backend::software_backend(screen_coords _resolution) : x11_window(_resolution) {
    screen_coords resolution = get_drawable_resolution();
    gc = XCreateGC(dpy, win, 0, &values);
    image = XShmCreateImage(dpy, CopyFromParent, vi->depth, ZPixmap, nullptr, &shminfo, resolution.x, resolution.y);
    shminfo.shmid = shmget(IPC_PRIVATE, static_cast<unsigned>(image->bytes_per_line * image->height), IPC_CREAT | 0666);
    shminfo.readOnly = False;
    shminfo.shmaddr = image->data = static_cast<char*>(shmat(shminfo.shmid, nullptr, 0));
    if(XShmAttach(dpy, &shminfo) != True) {
        fputs("Attaching shared memory failed", stderr);
        exit(0);
    }
}

software_backend::~software_backend() {
    XShmDetach(dpy, &shminfo);
    XFreeGC(dpy, gc);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, NULL);
}

void software_backend::attach_shm(framebuffer& fb) {
    screen_coords resolution = get_drawable_resolution();
    fb = framebuffer(reinterpret_cast<u8*>(shminfo.shmaddr), resolution);
    image->data = reinterpret_cast<char*>(fb.data());
}

void software_backend::swap_buffers(renderer& r) {
    auto& frame = reinterpret_cast<renderer_software&>(r).get_framebuffer();
    char* frame_buffer = reinterpret_cast<char*>(frame.data());
    if (image->data != frame_buffer || regen == true) {
        attach_shm(frame);
        regen = false;
        return;
    }
    _renderer_busy = true;
    XShmPutImage(dpy, win, gc, image, 0, 0, 0, 0, frame.size().x, frame.size().y, true);
}
void software_backend::update_resolution() {
    image->width = resolution.x;
    image->height = resolution.y;
    regen = true;
}
x11_window::x11_window(screen_coords resolution) {
    int att[5] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    dpy = XOpenDisplay(NULL);
	Colormap cmap;
	XSetWindowAttributes swa;

	Window                  root;

	if(dpy == NULL) {
	    printf("\n\tcannot connect to X server\n\n");
		exit(0);
	}    if(XShmQueryExtension(dpy) != True) {
        fputs("X11 shared memory unavailable", stderr);
        exit(0);
    }

    root = DefaultRootWindow(dpy);

	vi = glXChooseVisual(dpy, 0, att);

    if(vi == NULL) {
        printf("\n\tno appropriate visual found\n\n");
	    exit(0);
    }

    cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | PointerMotionMask;
	win = XCreateWindow(dpy, root, 0, 0, resolution.x, resolution.y, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "VERY SIMPLE APPLICATION");

	Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);

	XSelectInput(dpy, win,
              (FocusChangeMask | EnterWindowMask | LeaveWindowMask |
              ExposureMask | ButtonPressMask | ButtonReleaseMask |
              PointerMotionMask | KeyPressMask | KeyReleaseMask |
              PropertyChangeMask | StructureNotifyMask | KeymapStateMask ));
}


screen_coords x11_window::get_drawable_resolution() {
	XWindowAttributes retval;
	XGetWindowAttributes(dpy, win, &retval);

	return screen_coords(retval.width, retval.height);

}


bool x11_window::poll_events() {
	XEvent xev;
	event e;
    const auto shm_completion_event = XShmGetEventBase(dpy) + ShmCompletion;
    while(XPending(dpy)) {
    	XNextEvent(dpy, &xev);
    	e = event();
        switch (xev.type) {
            case Expose:
                XGetWindowAttributes(dpy, win, &gwa);
                glViewport(0, 0, gwa.width, gwa.height);
                break;

            case KeyPress:
                e.set_active(true);
                [[fallthrough]];
            case KeyRelease: {
                e.ID = XLookupKeysym(&xev.xkey, 0);
                e.set_type(event_flags::key_press);
                break;
            }
            // Apparently, the X server sends window destroy messages this way?
            // I don't get it, why not have a dedicated window-destroy event?
            case ClientMessage:
                return false;
            case ButtonPress:
                e.set_active(true);
                [[fallthrough]];
            case ButtonRelease:
                e.set_type(event_flags::button_press);
                e.pos =  screen_coords(xev.xbutton.x, xev.xbutton.y);;
                break;
            case MotionNotify:
                e.pos =  screen_coords(xev.xbutton.x, xev.xbutton.y);;
                e.set_type(event_flags::cursor_moved);
                e.set_active(true);
                break;
            case ConfigureNotify: {
                XConfigureEvent xce = xev.xconfigure;
                if (xce.width != resolution.x || xce.height != resolution.y) {

                    resolution = get_drawable_resolution();
                    printf("xce.x = %i, res.x = %i\n", xce.width, resolution.x);
                    resize_callback(resolution);
                    // If dynamic_cast returns null, it's not a software renderer. If it is, update its internal image buffer dimensions
                    software_backend* bck = dynamic_cast<software_backend*>(this);
                    if (bck != nullptr) {
                        bck->update_resolution();
                    }
                }
                break;
            }
            default:
                if(xev.type == shm_completion_event) {
                    _renderer_busy = false;
                }
        }
        event_callback(e);
    }


	return true;
}
#endif //__linux__

#ifdef _WIN32
win32_window::win32_window(screen_coords resolution) {
	hInstance = (HINSTANCE) GetModuleHandle(NULL);

    wcx.cbSize = sizeof(wcx);   // redraw if size changes
    wcx.lpfnWndProc = WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcx.lpszClassName = L"MainWClass";

    if (RegisterClassEx(&wcx) == 0)
        printf("Failed to register WC\n");

    hwnd = CreateWindowEx(0, wcx.lpszClassName, L"openglversioncheck", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, resolution.x, resolution.y, 0, 0, hInstance, this);
    ShowWindow(hwnd, 1);
    resolution = get_drawable_resolution();

}


bool win32_window::poll_events() {
    MSG msg = { };
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !quit_message;
}

screen_coords win32_window::get_drawable_resolution() {
    RECT rect;
    GetClientRect(hwnd, &rect);
    resolution = screen_coords(rect.right - rect.left, rect.bottom - rect.top);
	return resolution;
}


gl_backend::gl_backend(screen_coords resolution) : win32_window(resolution) {
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
    	32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0};
    HDC hdc = GetDC(hwnd);
	int pf = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc,pf, &pfd);
	gl_context = wglCreateContext(hdc);
	wglMakeCurrent (hdc, gl_context);
}
gl_backend::~gl_backend() {
	HDC hdc = GetDC(hwnd);
	wglMakeCurrent(hdc, NULL);
	wglDeleteContext(gl_context);
}
void gl_backend::swap_buffers(renderer& r) {
    SwapBuffers(GetDC(hwnd));
}



software_backend::software_backend(screen_coords resolution_in) : win32_window(resolution_in) {
    bmih.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmih.bmiHeader.biWidth = resolution.x;
    bmih.bmiHeader.biHeight = -resolution.y;
    bmih.bmiHeader.biPlanes = 1;
    bmih.bmiHeader.biCompression = BI_RGB;
    bmih.bmiHeader.biBitCount = 32;
    bmih.bmiHeader.biSizeImage = resolution.x * resolution.y * 4;

    bitmap = CreateDIBSection(GetDC(hwnd), &bmih, DIB_RGB_COLORS, reinterpret_cast<void**>(&fb_ptr), NULL, NULL);
}

void software_backend::attach_buffer(framebuffer& fb) {
    fb = framebuffer(fb_ptr, resolution);
}

void software_backend::swap_buffers(renderer& r) {
    auto& fb = reinterpret_cast<renderer_software&>(r).get_framebuffer();
    if (fb_ptr != fb.data()) {
        attach_buffer(fb);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

win32_window* window_data(HWND hwnd) {
    return (win32_window*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	event e;
	win32_window *wndptr = window_data(hwnd);
	switch(message)	{
	    case WM_CREATE: {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*)lParam)->lpCreateParams);
            return true;
        }
        case WM_PAINT: {
            auto* window = reinterpret_cast<software_backend*>(wndptr);
            screen_coords resolution = window->resolution;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            _mm_sfence();
            SetDIBitsToDevice(hdc, 0, 0, resolution.x, resolution.y, 0, 0, 0, resolution.y, window->fb_ptr,  &window->bmih,   DIB_RGB_COLORS);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            e.set_active(true);
            [[fallthrough]];
	    case WM_LBUTTONUP:
	        e.set_type(event_flags::button_press);
	        e.pos = screen_coords(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	        break;


		//wglMakeCurrent(ourWindowHandleToDeviceContext, NULL);
	//	wglDeleteContext(ourOpenGLRenderingContext);
		//PostQuitMessage(0);

	    case WM_SIZE: {
	        if(wndptr->resize_callback) {
	            screen_coords new_size(LOWORD(lParam), HIWORD(lParam));
                wndptr->resize_callback(new_size);

            }
            return 0;
        }
        case WM_KEYDOWN:
            e.set_active(true);
            [[fallthrough]];
        case WM_KEYUP: {
            e.ID = wParam;
            e.set_type(event_flags::key_press);
            break;
        }
        default:
		    return DefWindowProc(hwnd, message, wParam, lParam);
	}

	wndptr->event_callback(e);
	return 0;
}
#endif //_WIN32
}