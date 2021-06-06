#ifndef DISPLAY_IMPL_H
#define DISPLAY_IMPL_H

#include "display.h"
#include <SDL2/SDL.h>

/*****************************************************************************************************************************/
/*     display_impl.h - renderer, window, and texture manager declarations, to separate them from the rest of the engine     */
/*****************************************************************************************************************************/

namespace display {
struct sdl_window : public window_impl {
    screen_coords get_drawable_resolution() { return resolution; }
    bool poll_events();
protected:
    sdl_window(screen_coords resolution, int flags);
    ~sdl_window();
    SDL_Window* window;
};

#ifdef OPENGL

////////////////////////////////////////
//*     OpenGL type declarations     *//
////////////////////////////////////////

struct gl_backend : public sdl_window {
    gl_backend(screen_coords);
    ~gl_backend();
    void swap_buffers(renderer& r);
    void set_resolution(screen_coords coords);
    void set_vsync(bool);
private:
    SDL_GLContext gl_context;
};

class renderer_gl : public renderer {
public:
    renderer_gl();
    void clear_screen();
    void set_viewport(screen_coords);
    void set_camera(vec2d<f32>);

    void set_vsync(bool);
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
    renderer_gl::shader text_shader = gen_shader("shaders/ui.vert", "shaders/shader.frag");
    renderer_gl::shader ui_shader = gen_shader("shaders/ui.vert", "shaders/shader.frag");
    renderer_gl::shader map_shader = gen_shader("shaders/world.vert", "shaders/shader.frag");

    size<f32> viewport;
    std::vector<f32> camera = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
};

class texture_manager_gl : public texture_manager {
public:
    void update(texture*);
private:
    u32 get_new_id();
};

#endif //OPENGL


//////////////////////////////////////////
//*     Software type declarations     *//
//////////////////////////////////////////

struct software_backend : public sdl_window {
    software_backend(screen_coords _resolution);
    ~software_backend();
    void attach_shm(framebuffer& fb);
    void swap_buffers(renderer& r);
    void update_resolution();
    void set_vsync(bool);

private:
    SDL_Texture* fb_texture;
    SDL_Renderer* renderer_handle;
    u32* buffer;
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

class texture_manager_software : public texture_manager {
public:
    void update(texture*);
    std::array<image, 2048> textures_2x_scaled;
    std::array<image, 2048> textures_4x_scaled;
private:
    u32 get_new_id();
    size_t texture_counter = 0;
};
}
#endif //DISPLAY_IMPL_H
