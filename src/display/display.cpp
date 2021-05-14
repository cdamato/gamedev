#include "display_impl.h"

namespace display {

void display_manager::render() {
    get_renderer().clear_screen();
    get_renderer().render_layer(textures());
    get_window().swap_buffers(get_renderer());
}

void display_manager::initialize(display_types mode, screen_coords resolution) {
#ifdef OPENGL
    if (mode == display_types::software) {
        _window = std::unique_ptr<window_impl>(new software_backend(resolution));
        _textures = std::unique_ptr<texture_manager>(new texture_manager_software);
        _renderer = std::unique_ptr<renderer>(new renderer_software);
    } else {
        _window = std::unique_ptr<window_impl>(new gl_backend(resolution));
        renderer_gl::initialize_opengl();
        _textures = std::unique_ptr<texture_manager>(new texture_manager_gl);
        _renderer = std::unique_ptr<renderer>(new renderer_gl);
    }
#else
    _window = std::unique_ptr<window_impl>(new software_backend(resolution));
    _textures = std::unique_ptr<texture_manager>(new texture_manager_software);
    _renderer = std::unique_ptr<renderer>(new renderer_software);
#endif //OPENGL
    textures().load_textures();
}
}
