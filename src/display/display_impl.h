#ifndef DISPLAY_IMPL_H
#define DISPLAY_IMPL_H

#include "display.h"
/*****************************************************************************************************************************/
/*     display_impl.h - renderer, window, and texture manager declarations, to separate them from the rest of the engine     */
/*****************************************************************************************************************************/

namespace display {
#ifdef OPENGL
class texture_manager_gl : public texture_manager {
public:
    void update(texture*);
private:
    u32 get_new_id();
};

class renderer_gl : public renderer {
public:
    renderer_gl();
    void clear_screen();
    void set_viewport(screen_coords);
    void set_camera(vec2d<f32>);

    static void initialize_opengl();
private:
    void render_batch(texture*, render_layers, texture_manager&);
    void update_texture_data(texture*);

    class shader {
    public:
        shader(u32 vert, u32 frag);
        ~shader();
        void register_uniform(std::string name);
        void bind();
        template <typename... Args>
        void update_uniform(std::string name, Args... args);
    private:
        [[no_unique_address]] no_copy disable_copy;
        [[no_unique_address]] no_move disable_move;
        u32 _ID = 65535;
        std::unordered_map<std::string, int> attributes = std::unordered_map<std::string, int>();
    };
    static shader gen_shader(std::string, std::string);
    renderer_gl::shader& get_shader(render_layers layer);
    renderer_gl::shader text_shader = gen_shader("shaders/ui.vert", "shaders/text.frag");
    renderer_gl::shader ui_shader = gen_shader("shaders/ui.vert", "shaders/shader.frag");
    renderer_gl::shader map_shader = gen_shader("shaders/world.vert", "shaders/shader.frag");

    size<f32> viewport;
    std::vector<f32> camera = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
};

#endif //OPENGL

class texture_manager_software : public texture_manager {
public:
    void update(texture*);
    std::array<image, 2048> textures_2x_scaled;
    std::array<image, 2048> textures_4x_scaled;
private:
    u32 get_new_id();
    size_t texture_counter = 0;
};

class renderer_software : public renderer {
public:
    renderer_software();
    void clear_screen();
    texture* add_texture(std::string name);
    void set_viewport(screen_coords);
    void set_camera(vec2d<f32>);
    framebuffer& get_framebuffer() { return fb;}
private:
    void render_batch(texture*, render_layers, texture_manager&);
    framebuffer fb;
    vec2d<f32> camera;
};

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

struct x11_window : public window_impl {
	virtual ~x11_window() = default;
	x11_window(screen_coords resolution);
    screen_coords get_drawable_resolution();
	bool poll_events();

	Display* dpy;
	XVisualInfo* vi;
	Window win;
	XWindowAttributes gwa;
};

#ifdef OPENGL
struct gl_backend : public x11_window {
	GLXContext glc;
    gl_backend(screen_coords resolution);
    void swap_buffers(renderer& r);
	void set_resolution(screen_coords coords);
};
#endif //OPENGL

struct software_backend : public x11_window {
    XImage* image;
  	GC gc;
    XShmSegmentInfo shminfo;
  	XGCValues values;
    bool regen = true;

    software_backend(screen_coords _resolution);
    ~software_backend();
    void attach_shm(framebuffer& fb);
    void swap_buffers(renderer& r);
	void update_resolution();
};
#endif //__linux__


#ifdef _WIN32
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

struct win32_window : public window_impl, no_copy, no_move {
	HWND hwnd;
	HINSTANCE hInstance;
	WNDCLASSEX wcx = {0};
    bool quit_message = false;

    screen_coords get_drawable_resolution();
	bool poll_events();
	win32_window(screen_coords resolution);
};

struct gl_backend : public win32_window {
	HGLRC gl_context = 0;
    gl_backend(screen_coords resolution);
    ~gl_backend();
    void swap_buffers(renderer& r);
};


struct software_backend : public win32_window {
    BITMAPINFO bmih;
    HANDLE hdcMem;
    HBITMAP bitmap;
    u8* fb_ptr;

    software_backend(screen_coords resolution_in);
    void attach_buffer(framebuffer& fb);
    void swap_buffers(renderer& r);
};
#endif //_WIN32
}
#endif //DISPLAY_IMPL_H
