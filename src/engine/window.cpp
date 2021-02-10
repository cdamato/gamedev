#include <common/parser.h>
#include <fstream>
#include "engine.h"

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

struct window_impl {
	Display                 *dpy;
	Window                  root;
	int                     att[5] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	XVisualInfo             *vi;
	Colormap                cmap;
	XSetWindowAttributes    swa;
	Window                  win;
	GLXContext              glc;
	XWindowAttributes       gwa;

	window_impl(size<u16> resolution);
};

window_impl::window_impl(size<u16> resolution) {
	 dpy = XOpenDisplay(NULL);

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
		 win = XCreateWindow(dpy, root, 0, 0, resolution.w, resolution.h, 0,
				 vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
		 XMapWindow(dpy, win);
		 XStoreName(dpy, win, "VERY SIMPLE APPLICATION");

		 glc = glXCreateContext(dpy, vi, NULL, true);
		 glXMakeCurrent(dpy, win, glc);


		   Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

		 XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);

		 XSelectInput(dpy, win,
		                  (FocusChangeMask | EnterWindowMask | LeaveWindowMask |
		                  ExposureMask | ButtonPressMask | ButtonReleaseMask |
		                  PointerMotionMask | KeyPressMask | KeyReleaseMask |
		                  PropertyChangeMask | StructureNotifyMask |
		                  KeymapStateMask ));

}

void window_manager::swap_buffers() {
	glXSwapBuffers(context->dpy, context->win);
}



void window_manager::set_resolution(size<u16> resolution) {
	XWindowAttributes retval;
	XGetWindowAttributes(context->dpy, context->win, &retval);
	//settings.resolution = size<u16>(retval.width, retval.height);
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
	context = new window_impl(settings.resolution);
	set_resolution(settings.resolution);
	set_vsync(settings.flags.test(window_flags::vsync));
}

bool window_manager::poll_events(event& e) {
	e = event();
	e.set_active(false);
	XEvent xev;

	if (XPending(context->dpy) == 0) return false;
 	XNextEvent(context->dpy, &xev);

	switch (xev.type) {
	case Expose:
    	XGetWindowAttributes(context->dpy, context->win, &context->gwa);
        glViewport(0, 0, context->gwa.width, context->gwa.height);
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

struct window_impl {
	HWND hwnd;
	HINSTANCE hInstance;
	WNDCLASS wc      = {0};

	window_impl(size<u16> resolution);
};

window_impl::window_impl(size<u16> resolution) {
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
	context = new window_impl(settings.resolution);

	//set_vsync(settings.flags.test(window_flags::vsync));
}
#endif //_WIN32



