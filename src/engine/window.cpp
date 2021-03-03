#include <common/parser.h>
#include <fstream>
#include "engine.h"

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <cstring>



struct x11_window : public window_impl{
	Display                 *dpy;
	XVisualInfo             *vi;
	Window                  win;
	XWindowAttributes       gwa;

	x11_window(size<u16> resolution);
    size<u16> get_drawable_resolution() ;
	bool poll_events();
};

struct gl_backend : public x11_window {
	GLXContext glc;
    gl_backend(size<u16> resolution) : x11_window(resolution) {
        glc = glXCreateContext(dpy, vi, NULL, true);
        glXMakeCurrent(dpy, win, glc);
    }
    void swap_buffers(renderer_base& r) {
	    glXSwapBuffers(dpy, win);
	}
};

struct software_backend : public x11_window {
    XImage* image;
  	GC                    gc;
    XShmSegmentInfo shminfo;
  	XGCValues             values;

    software_backend(size<u16> _resolution) : x11_window(_resolution) {
        size<u16> resolution = get_drawable_resolution();
        gc = XCreateGC(dpy, win, 0, &values);
        image = XShmCreateImage(dpy, CopyFromParent, vi->depth, ZPixmap, nullptr, &shminfo, resolution.w, resolution.h);

    }

    void attach_shm(framebuffer& fb) {
        size<u16> resolution = get_drawable_resolution();
        image = XShmCreateImage(dpy, vi->visual, vi->depth, ZPixmap, 0, &shminfo, resolution.w, resolution.h);
        shminfo.shmid = shmget(IPC_PRIVATE, static_cast<unsigned>(image->bytes_per_line * image->height), IPC_CREAT | 0666);
        shminfo.readOnly = False;
        shminfo.shmaddr = image->data = static_cast<char*>(shmat(shminfo.shmid, nullptr, 0));
        if(XShmAttach(dpy, &shminfo) != True) {
            fputs("Attaching shared memory failed", stderr);
            exit(0);
        }
        fb = framebuffer(reinterpret_cast<u8*>(shminfo.shmaddr), resolution);
    }

    void swap_buffers(renderer_base& r) {
        auto& frame = reinterpret_cast<renderer_software&>(r).get_framebuffer();
        char* frame_buffer = reinterpret_cast<char*>(frame.data());
        if (image->data != frame_buffer) {
            attach_shm(frame);
        }
        renderer_busy = true;
        XShmPutImage(dpy, win, gc, image, 0, 0, 0, 0, frame.size().w, frame.size().h, true);
	}
};

x11_window::x11_window(size<u16> resolution) {
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
	win = XCreateWindow(dpy, root, 0, 0, resolution.w, resolution.h, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
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


size<u16> x11_window::get_drawable_resolution() {
	XWindowAttributes retval;
	XGetWindowAttributes(dpy, win, &retval);
	return size<u16>(retval.width, retval.height);

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
                e.pos =  point<f32>(xev.xbutton.x, xev.xbutton.y);;
                break;
            case MotionNotify:
                e.pos =  point<f32>(xev.xbutton.x, xev.xbutton.y);;
                e.set_type(event_flags::cursor_moved);
                e.set_active(true);
                break;
            default:
                if(xev.type == shm_completion_event) {
                    renderer_busy = false;
                }
        }
        event_callback(e);
    }


	return true;
}
#endif //__linux__


#ifdef _WIN32

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

struct win32_window : public window_impl{
	HWND hwnd;
	HINSTANCE hInstance;
	WNDCLASS wc = {0};
	HGLRC gl_context = 0;
    size<u16> resolution;
	win32_window(size<u16> resolution);
    size<u16> get_drawable_resolution() ;
	bool poll_events();
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
win32_window::win32_window(size<u16> resolution) {
	hInstance = (HINSTANCE)GetModuleHandle(NULL);

    WNDCLASSEX wcx;

    wchar_t classname[256] = L"MainWClass";
    wcx.cbSize = sizeof(wcx);          // size of structure
    wcx.style = 0;                  // redraw if size changes
    wcx.lpfnWndProc = WndProc;     // points to window procedure
    wcx.cbClsExtra = 0;                // no extra class memory
    wcx.cbWndExtra = 0;                // no extra window memory
    wcx.hInstance = hInstance;   // handle to instance
    wcx.hIcon = NULL; // predefined app. icon
    wcx.hCursor = LoadCursorW(nullptr, IDC_ARROW);                    // white background brush
    wcx.hbrBackground = NULL;
    wcx.lpszMenuName = NULL;    // name of menu resource
    wcx.lpszClassName = classname;
    wcx.hIconSm = NULL;

    if (RegisterClassEx(&wcx) == 0)
        printf("Failed to register WC\n");

    hwnd = CreateWindowEx(0, wcx.lpszClassName, L"openglversioncheck", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1920, 1080, 0, 0, hInstance, this);
    ShowWindow(hwnd, 1);

    resolution = get_drawable_resolution();
}




bool win32_window::poll_events() {
    MSG msg = { };
    while (PeekMessage(&msg, NULL, 0, 0, 0)) {
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

struct gl_backend : public win32_window {
    gl_backend(size<u16> resolution) : win32_window(resolution) {}
    void swap_buffers(renderer_base& r) {
        SwapBuffers(GetDC(hwnd));
	}
};


size<u16> win32_window::get_drawable_resolution() {
    RECT rect;
    GetClientRect(hwnd, &rect);
    resolution = size<u16>(rect.right - rect.left, rect.bottom - rect.top);
	return resolution;
}

struct software_backend : public win32_window {
    BITMAPINFO bmih;
    HDC hdcMem;

    u8* fb_ptr;
    HBITMAP bitmap;

    software_backend(size<u16> resolution_in) : win32_window(resolution_in) {
        bmih.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmih.bmiHeader.biWidth = resolution.w;
        bmih.bmiHeader.biHeight = -resolution.h;
        bmih.bmiHeader.biPlanes = 1;
        bmih.bmiHeader.biCompression = BI_BITFIELDS;
        bmih.bmiHeader.biBitCount = 32;
        bmih.bmiHeader.biSizeImage = resolution.w * resolution.h * 4;
        bmih.bmiColors[0].rgbRed =   0xff;
        bmih.bmiColors[1].rgbGreen = 0xFF;
        bmih.bmiColors[2].rgbBlue =  0xff;

        bitmap = CreateDIBSection(GetDC(hwnd), &bmih, DIB_RGB_COLORS, reinterpret_cast<void**>(&fb_ptr), NULL, NULL);
    }
    void attach_buffer(framebuffer& fb) {
        fb = framebuffer(fb_ptr, resolution);
    }
    void swap_buffers(renderer_base& r) {
        auto& fb = reinterpret_cast<renderer_software&>(r).get_framebuffer();
        if (fb_ptr != fb.data()) {
            attach_buffer(fb);
        }
        InvalidateRect (hwnd, NULL, NULL);
	}
};











#include <common/png.h>
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	event e;

	win32_window *wndptr = (win32_window*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch(message)	{
	    case WM_CREATE: {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCT*)lParam)->lpCreateParams);
            SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

            // For the sake of simplicity, perform these tasks for both renderers.
            // Windows guarantees OpenGL 1.1, so this should always succeed.
		    PIXELFORMATDESCRIPTOR pfd =	{ sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
    			32, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0,	24, 8,	0,	PFD_MAIN_PLANE,	0,	0, 0, 0	};
    		HDC hdc = GetDC(hwnd);
		    int pf = ChoosePixelFormat(hdc, &pfd);
		    SetPixelFormat(hdc,pf, &pfd);
    		HGLRC gl_context = wglCreateContext(hdc);
    		wglMakeCurrent (hdc, gl_context);
    		return true;
        }
        case WM_LBUTTONDOWN:
            e.set_active(true);
            [[fallthrough]];
	    case WM_LBUTTONUP:
	        e.set_type(event_flags::button_press);
	        e.pos = point<f32>(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	        break;

        case WM_PAINT: {

            PAINTSTRUCT ps;

            size<u16> resolution = wndptr->resolution;
            auto* window = reinterpret_cast<software_backend*>(wndptr);

            HDC hdc = BeginPaint(hwnd, &ps);


            HDC hdcMem = CreateCompatibleDC(hdc);
            HANDLE hOld = SelectObject(hdcMem, window->bitmap);

            BitBlt(hdc, 0, 0, resolution.w, resolution.h, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, hOld);
            EndPaint(hwnd, &ps);
            return 0;
        }

		//wglMakeCurrent(ourWindowHandleToDeviceContext, NULL);
	//	wglDeleteContext(ourOpenGLRenderingContext);
		//PostQuitMessage(0);

	    case WM_SIZE: {
	        // Test if valid window initialization has happened
	        if(wndptr->resize_callback) {
	            size<u16> new_size(LOWORD(lParam), HIWORD(lParam));
                wndptr->resize_callback(new_size);
            }
            return 0;
        }
	    default:
		    return DefWindowProc(hwnd, message, wParam, lParam);
	}


	wndptr->event_callback(e);

	return 0;
}
#endif //_WIN32








void window_manager::set_resolution(size<u16> resolution) {
}


void window_manager::set_vsync(bool state) {
}

void window_manager::set_fullscreen(bool state) {
    fullscreen = state;
}


window_manager::~window_manager() {
}


window_manager::window_manager(settings_manager& settings) {
    if (settings.flags.test(window_flags::use_software_render)) {
        context = new software_backend(settings.resolution);
    } else {
        context = new gl_backend(settings.resolution);
    }
    set_resolution(settings.resolution);
    settings.resolution = context->get_drawable_resolution();
    set_vsync(settings.flags.test(window_flags::vsync));
}