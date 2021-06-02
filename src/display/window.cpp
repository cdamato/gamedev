#include <common/parser.h>
#include <common/event.h>
#include <cstring>
#include <fstream>
#include "display_impl.h"

namespace display {
#ifdef __linux__

/////////////////////////////////////
/*     X11 base windowing code     */
/////////////////////////////////////
x11_window::x11_window(screen_coords resolution) {
    int att[5] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    dpy = XOpenDisplay(NULL);
	Colormap cmap;
	XSetWindowAttributes swa;
	Window root;

	if(dpy == NULL) {
	    printf("\n\tcannot connect to X server\n\n");
		exit(0);
	}
	if(XShmQueryExtension(dpy) != True) {
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

x11_window::~x11_window() {
    XCloseDisplay(dpy);
}

screen_coords x11_window::get_drawable_resolution() {
	XWindowAttributes retval;
	XGetWindowAttributes(dpy, win, &retval);
	return screen_coords(retval.width, retval.height);
}


bool x11_window::poll_events() {
	XEvent xev;
    const auto shm_completion_event = XShmGetEventBase(dpy) + ShmCompletion;
    while(XPending(dpy)) {
    	XNextEvent(dpy, &xev);
        switch (xev.type) {
            case KeyPress:
            case KeyRelease: {
                event_keypress e(XLookupKeysym(&xev.xkey, 0), xev.type == KeyRelease);
                event_callback(e);
                break;
            }
            case ButtonPress:
            case ButtonRelease: {
                event_mousebutton ev(screen_coords(xev.xbutton.x, xev.xbutton.y), xev.type == ButtonPress);
                event_callback(ev);
                break;
            }
            case MotionNotify: {
                event_cursor e(screen_coords(xev.xbutton.x, xev.xbutton.y));
                event_callback(e);
                break;
            }
            // Apparently, the X server sends window destroy messages this way?
            // I don't get it, why not have a dedicated window-destroy event?
            case ClientMessage:
                if(xev.xclient.data.l[0] == static_cast<long>(XInternAtom(dpy, "WM_DELETE_WINDOW", True))) {
                    return false;
                }
                break;
            case ConfigureNotify: {
                XConfigureEvent xce = xev.xconfigure;
                if (xce.width != resolution.x || xce.height != resolution.y) {
                    resolution = screen_coords(xce.width, xce.height);
                    resize_callback(resolution);
                    // If dynamic_cast returns null, it's not a software renderer. If it is, update its internal image buffer dimensions
                    software_backend* bck = dynamic_cast<software_backend*>(this);
                    if (bck != nullptr) {
                        bck->regen = true;
                    }
                }
                break;
            }
            default:
                if(xev.type == shm_completion_event) {
                    _renderer_busy = false;
                }
        }
    }
	return true;
}

#ifdef OPENGL

///////////////////////////////////////
/*     X11 OpenGL windowing code     */
///////////////////////////////////////
void gl_backend::swap_buffers(renderer& r) { glXSwapBuffers(dpy, win); }
void gl_backend::set_resolution(screen_coords coords) { glViewport(0, 0, coords.x, coords.y); }

gl_backend::gl_backend(screen_coords resolution) : x11_window(resolution) {
    glc = glXCreateContext(dpy, vi, NULL, true);
    glXMakeCurrent(dpy, win, glc);
}

gl_backend::~gl_backend() {
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
}

void gl_backend::set_vsync(bool state) {
    std::string extensions(glXQueryExtensionsString(dpy, 0));
    size_t index = 0;
    size_t new_index = 0;
    bool ext_found = false;
    while(index < std::string::npos) {
        new_index = extensions.find(" ", index);
        std::string extension_name = extensions.substr(index, new_index - index);
        printf("Extension found: %s\n Index is %i, new index is %i\n", extension_name.c_str(), index, new_index);
        index = new_index + 1;
        if (extension_name == std::string("GLX_EXT_swap_control")) {
            ext_found = true;
            break;
        }
    }

    if(ext_found) {
        glXSwapIntervalEXT(dpy, win, state ? 1 : 0);
    }
}
#endif //OPENGL

#include <errno.h>

//////////////////////////////////////////////////
/*     X11 Software Renderer windowing code     */
//////////////////////////////////////////////////

XImage* generate_XImage(Display* dpy, XShmSegmentInfo* shminfoaddr, screen_coords resolution, int image_depth) {
    return XShmCreateImage(dpy, CopyFromParent, 24, ZPixmap, nullptr, shminfoaddr, resolution.x, resolution.y);
}

void generate_shm_buffer(Display* dpy, XShmSegmentInfo* shminfoptr, XImage* image, screen_coords resolution) {
    shminfoptr->shmid = shmget(IPC_PRIVATE, static_cast<unsigned>(image->bytes_per_line * image->height), IPC_CREAT | 0666);
    if (shminfoptr->shmid == -1) {
        printf("shmget failed with errno %i\n", errno);
    }
    shminfoptr->readOnly = False;
    shminfoptr->shmaddr = image->data = static_cast<char*>(shmat(shminfoptr->shmid, nullptr, 0));
    if(XShmAttach(dpy, shminfoptr) != True) {
        fputs("Attaching shared memory failed", stderr);
        exit(0);
    }
    // Mark this shared memory as destroyable - it won't actually be destroyed until the last process detaches.
    // This requires either a program crash or a call to XShmDetach, thus ensuring the segment is cleaned up in case of a crash.
    shmctl(shminfoptr->shmid, IPC_RMID, NULL);
}

void destroy_XImage_shm(Display* dpy, XImage** imageptr, XShmSegmentInfo* shminfoptr) {
    XShmDetach(dpy, shminfoptr);
    XDestroyImage(*imageptr);
    *imageptr = nullptr;
    *shminfoptr = XShmSegmentInfo{};
}

software_backend::software_backend(screen_coords _resolution) : x11_window(_resolution) {
    resolution = get_drawable_resolution();
    gc = XCreateGC(dpy, win, 0, &values);
    image = generate_XImage(dpy, &shminfo, resolution, vi->depth);
    generate_shm_buffer(dpy, &shminfo, image, resolution);
}

software_backend::~software_backend() {
    XFreeGC(dpy, gc);
    destroy_XImage_shm(dpy, &image, &shminfo);
}

void software_backend::attach_shm(framebuffer& fb) {
    screen_coords resolution = get_drawable_resolution();
    fb = framebuffer(reinterpret_cast<u8*>(shminfo.shmaddr), resolution);
    image->data = reinterpret_cast<char*>(fb.data());
}

void software_backend::swap_buffers(renderer& r) {
    if (regen) {
        // X11 seemingly errors if an old shared memory address is reused for a new XImage/buffer - meaning resizing the window can crash.
        // Generating the new buffer before deleting the old ensures a unique address.
        // Also, the new XImage gets passed the address of the (in-use) object-scope shminfo since it's needed at render time, not creation time.
        XImage* new_image = generate_XImage(dpy, &shminfo, resolution, vi->depth);
        XShmSegmentInfo new_info;
        generate_shm_buffer(dpy, &new_info, new_image, resolution);
        destroy_XImage_shm(dpy, &image, &shminfo);
        image = new_image;
        shminfo = new_info;
    }
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
    regen = true;
}

void software_backend::set_vsync(bool state) {
    //no-op for right now
}

#endif //__linux__

#ifdef _WIN32
#include <GL/glew.h>
#include <GL/wglew.h>

///////////////////////////////////////
/*     Win32 base windowing code     */
///////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
int software_renderer_paint(HWND hwnd);

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

win32_window* window_data(HWND hwnd) {
    return (win32_window*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	win32_window *wndptr = window_data(hwnd);
	switch(message)	{
	    case WM_CREATE: {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*)lParam)->lpCreateParams);
            return true;
        }
        case WM_PAINT:
            return software_renderer_paint(hwnd);
        case WM_KEYUP:
        case WM_KEYDOWN: {
            event_keypress e(wParam, message == WM_KEYUP);
            wndptr->event_callback(e);
            break;
        }
        case WM_LBUTTONDOWN:
	    case WM_LBUTTONUP: {
	        event_mousebutton ev(screen_coords(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), message == WM_LBUTTONDOWN);
            wndptr->event_callback(ev);
	        break;
        }
        case WM_MOUSEMOVE: {
            event_cursor e(screen_coords(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            wndptr->event_callback(e);
            break;
        }
	    case WM_SIZE: {
	        if(wndptr->resize_callback) {
	            screen_coords new_size(LOWORD(lParam), HIWORD(lParam));
                wndptr->resize_callback(new_size);
                wndptr->resolution = new_size;
            }
	        software_backend* bck = dynamic_cast<software_backend*>(wndptr);
	        if (bck != nullptr) {
	            bck->regen = true;
	        }
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            wndptr->quit_message = true;
        }
        default:
		    return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

/////////////////////////////////////////
/*     Win32 OpenGL windowing code     */
/////////////////////////////////////////

gl_backend::gl_backend(screen_coords resolution) : win32_window(resolution) {
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
    	32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0};
    HDC hdc = GetDC(hwnd); // This DC maintains the OpenGL context and must not be freed.
	int pf = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc,pf, &pfd);
	gl_context = wglCreateContext(hdc);
	wglMakeCurrent (hdc, gl_context);
}

gl_backend::~gl_backend() {
	HDC hdc = GetDC(hwnd);
	wglMakeCurrent(hdc, NULL);
	wglDeleteContext(gl_context);
    ReleaseDC(hwnd, hdc);
}

void gl_backend::swap_buffers(renderer& r) {
    HDC hdc = GetDC(hwnd);
    SwapBuffers(hdc);
    ReleaseDC(hwnd, hdc);
}

void gl_backend::set_vsync(bool state) {
    std::string extensions(wglGetExtensionsStringEXT());
    size_t index = 0;
    size_t new_index = 0;
    bool ext_found = false;
    while(index < std::string::npos) {
        new_index = extensions.find(" ", index);
        std::string extension_name = extensions.substr(index, new_index - index);
        printf("Extenust exist for the length sion found: %s\n Index is %i, new index is %i\n", extension_name.c_str(), index, new_index);
        index = new_index + 1;
        if (extension_name == std::string("WGL_EXT_swap_control")) {
            ext_found = true;
            break;
        }
    }

    if(ext_found) {
        wglSwapIntervalEXT(state ? 1 : 0);
    }
}

////////////////////////////////////////////////////
/*     Win32 Software Renderer windowing code     */
////////////////////////////////////////////////////

void software_backend::generate_buffer(screen_coords resolution) {
    bmih.bmiHeader = BITMAPINFOHEADER { sizeof(BITMAPINFOHEADER), resolution.x, -resolution.y, 1, 32, BI_RGB, resolution.x * resolution.y * 4 };
    bitmap = CreateDIBSection(GetDC(hwnd), &bmih, DIB_RGB_COLORS, reinterpret_cast<void**>(&fb_ptr), NULL, NULL);
}

void software_backend::destroy_buffer() {
    bmih.bmiHeader = BITMAPINFOHEADER {};
    if(DeleteObject(bitmap) == 0) {
        printf("deleting bitmap failed\n");
    }
}

software_backend::software_backend(screen_coords resolution_in) : win32_window(resolution_in) {
    generate_buffer(resolution);
}

void software_backend::attach_buffer(framebuffer& fb) {
    fb = framebuffer(fb_ptr, resolution);
}

void software_backend::set_vsync(bool) {
    // no-op for right now
}

void software_backend::swap_buffers(renderer& r) {
    if (regen) {
        destroy_buffer();
        generate_buffer(resolution);
    }
    auto& fb = reinterpret_cast<renderer_software&>(r).get_framebuffer();
    if (fb_ptr != fb.data() || regen) {
        attach_buffer(fb);
        regen = false;
        return;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}


int software_renderer_paint(HWND hwnd) {
    auto* window = reinterpret_cast<software_backend*>(window_data(hwnd));
    screen_coords resolution = window->resolution;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    SetDIBitsToDevice(hdc, 0, 0, resolution.x, resolution.y, 0, 0, 0, resolution.y, window->fb_ptr,  &window->bmih,   DIB_RGB_COLORS);
    EndPaint(hwnd, &ps);
    return 0;
}

#endif //_WIN32
}