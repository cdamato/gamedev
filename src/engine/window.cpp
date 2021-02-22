#include <common/parser.h>
#include <fstream>
#include "engine.h"

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
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
	bool poll_events(event& e);
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
  	XGCValues             values;

   std::vector<char> data;
    software_backend(size<u16> _resolution) : x11_window(_resolution) {
        size<u16> resolution = get_drawable_resolution();
        gc = XCreateGC(dpy, win, 0, &values);
        data = std::vector<char>(resolution.w * resolution.h * 4, 255);
        image = XCreateImage(dpy, vi->visual, vi->depth, ZPixmap, 0, data.data(), resolution.w, resolution.h, 8, 0);
    }

    // I would REALLY like to find a more efficient way than double-copying the buffers.

    void swap_buffers(renderer_base& r) {
        auto& frame = reinterpret_cast<renderer_software&>(r).get_framebuffer();
        // X11 is a despicable protocol.
        // This is the singular worst line of code I have ever written in my entire life, and nothing else comes close.
        // I am BEGGING for any better solution that doesn't require an extra buffer copy.
        image->data = reinterpret_cast<char*>(const_cast<u8*>(frame.data()));
        XPutImage(dpy, win, gc, image, 0, 0, 0, 0, frame.size().w, frame.size().h);
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
	}

    root = DefaultRootWindow(dpy);
	Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom value = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	XChangeProperty(dpy, root, type, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&value), 1);
	vi = glXChooseVisual(dpy, 0, att);

    if(vi == NULL) {
        printf("\n\tno appropriate visual found\n\n");
	    exit(0);
    }
    else {
	 	printf("\n\tvisual %p selected\n", (void *)vi->visualid); /* %p creates hexadecimal output like in glxinfo */
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

void window_manager::set_resolution(size<u16> resolution) {
   // settings.resolution = context->get_drawable_resolution();
}


void window_manager::set_vsync(bool state) {
	//SDL_GL_SetSwapInterval(1);
}

void window_manager::set_fullscreen(bool state) {
	fullscreen = state;
	//auto flag = state ? SDL_WINDOW_FULLSCREEN : 0;
//	SDL_SetWindowFullscreen(mainWindow, flag);
}


window_manager::~window_manager() {
	//SDL_GL_DeleteContext(mainContext);
	//SDL_DestroyWindow(mainWindow);
//	SDL_Quit();
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

bool x11_window::poll_events(event& e) {
	e = event();
	e.set_active(false);
	XEvent xev;

	if (XPending(dpy) == 0) return false;
 	XNextEvent(dpy, &xev);

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
		e.set_type(event_flags::terminate);
		break;
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

	}
	return true;
}
#endif //__linux__


#ifdef _WIN32
#include <windows.h>
#include "window.h"

#pragma comment (lib, "opengl32.lib")

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

struct x11_window {
	HWND hwnd;
	HINSTANCE hInstance;
	WNDCLASS wc      = {0};

	x11_window(size<u16> resolution);
};

x11_window::x11_window(size<u16> resolution) {
	hInstance = GetModuleHandle(NULL);
	WNDCLASS wc      = {0};
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = L"oglversionchecksample";
	wc.style = CS_OWNDC;
	if( !RegisterClass(&wc) )
		throw "error";
	hwnd = CreateWindowEx(0, wc.lpszClassName,L"openglversioncheck",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,640,480,0,0,hInstance,0);
}


bool window_manager::poll_events(event& e) {
	return false;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_CREATE:
		{
		PIXELFORMATDESCRIPTOR pfd =
		{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
			32,                   // Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                   // Number of bits for the depthbuffer
			8,                    // Number of bits for the stencilbuffer
			0,                    // Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};

		HDC ourWindowHandleToDeviceContext = GetDC(hWnd);

		int  letWindowsChooseThisPixelFormat;
		letWindowsChooseThisPixelFormat = ChoosePixelFormat(ourWindowHandleToDeviceContext, &pfd);
		SetPixelFormat(ourWindowHandleToDeviceContext,letWindowsChooseThisPixelFormat, &pfd);

		HGLRC ourOpenGLRenderingContext = wglCreateContext(ourWindowHandleToDeviceContext);
		wglMakeCurrent (ourWindowHandleToDeviceContext, ourOpenGLRenderingContext);


		//wglMakeCurrent(ourWindowHandleToDeviceContext, NULL); Unnecessary; wglDeleteContext will make the context not current
	//	wglDeleteContext(ourOpenGLRenderingContext);
		//PostQuitMessage(0);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;

}


void window_manager::swap_buffers() {
	SwapBuffers(GetDC(context->hwnd));
}


window_manager::window_manager() {
	context = new x11_window(settings.resolution);

	//set_vsync(settings.flags.test(window_flags::vsync));
}
#endif //_WIN32



