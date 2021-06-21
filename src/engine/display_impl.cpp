#include "display.h"
#include <cstring>
#include <fstream>
#include <cmath>
#include <assert.h>
#include <common/parser.h>
#include <common/png.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>

namespace display {

// Some renderer constants
constexpr unsigned quads_in_buffer = 1024;
constexpr unsigned vertex_buffer_size = quads_in_buffer * vertices_per_quad * sizeof(vertex);
constexpr unsigned zindex_buffer_size = quads_in_buffer * vertices_per_quad * sizeof(u8);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//     WINDOW, RENDERER AND TEXTURE MANAGER CLASS DECLARATIONS, FOR BOTH OPENGL AND SOFTWARE RENDERERS     //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct sdl_window : public window_impl {
    void set_resolution(screen_coords coords) { if (!_fullscreen) { SDL_SetWindowSize(window, coords.x, coords.y); } }
    void set_fullscreen(bool fullscreen_state) { SDL_SetWindowFullscreen(window, fullscreen_state ? SDL_WINDOW_FULLSCREEN_DESKTOP  : 0); }
    bool poll_events();
protected:
    sdl_window(screen_coords resolution, int flags);
    ~sdl_window();
    SDL_Window* window;
};

///////////////////////////////////////
//     OPENGL CLASS DECLARATIONS     //
///////////////////////////////////////

#ifdef OPENGL

struct gl_backend : public sdl_window {
    gl_backend(screen_coords);
    ~gl_backend();
    void swap_buffers(renderer& r);
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

    class vbo {
    public:
        vbo();
        ~vbo();
        void bind();
    private:
        u32 _id;
    };

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
    renderer_gl::vbo vertex_vbo;
    renderer_gl::vbo zindex_vbo;

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

/////////////////////////////////////////
//     SOFTWARE CLASS DECLARATIONS     //
/////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////
//     IMPLEMENTATION CODE FOR OPENGL AND SOFTWARE RENDERERS     //
///////////////////////////////////////////////////////////////////

//////////////////////////////////
//     DISPLAY MANAGER CODE     //
//////////////////////////////////

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
    get_window().set_vsync(false);
}

//////////////////////////////////////////////////////////////
//     COMMON WINDOW, RENDERER AND TEXTURE MANAGER CODE     //
//////////////////////////////////////////////////////////////

////////////////////////////////
//     COMMON WINDOW CODE     //
////////////////////////////////

sdl_window::~sdl_window() { SDL_DestroyWindow(window); }
sdl_window::sdl_window(screen_coords resolution_in, int flags) {
    u32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | flags;
    window = SDL_CreateWindow("SDL2 OpenGL Example", 0, 0, resolution_in.x, resolution_in.y, window_flags);

    point<int> window_size;
    SDL_GL_GetDrawableSize(window, &window_size.x, &window_size.y);
    _resolution = window_size.to<u16>();
}

bool sdl_window::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event) > 0) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
            case SDL_KEYUP:
            case SDL_KEYDOWN: {
                if (event.key.repeat) break;
                event_keypress e(event.key.keysym.sym, event.type == SDL_KEYUP);
                event_callback(e);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                event_mousebutton ev(screen_coords(event.button.x, event.button.y), event.type == SDL_MOUSEBUTTONUP);
                event_callback(ev);
                break;
            }
            case SDL_MOUSEMOTION: {
                event_cursor e(screen_coords(event.motion.x, event.motion.y));
                event_callback(e);
                break;
            }
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        resize_callback(screen_coords(event.window.data1, event.window.data2));
                        break;
                }
                break;
            }
        }
    }
    return true;
}

//////////////////////////////////
//     COMMON RENDERER CODE     //
//////////////////////////////////

void renderer::add_sprite(sprite_data& sprite) { batching_pool.emplace_back(sprite); }
void renderer::clear_sprites() { batching_pool.clear(); }
void renderer::mark_sprites_dirty() { sprites_dirty = true;}

void renderer::render_layer(texture_manager& tm) {
    if (batching_pool.size() == 0 ) return;

    if (sprites_dirty) {
        std::sort(batching_pool.begin(), batching_pool.end());
        sprites_dirty = false;
    }
    texture* current_tex = (*batching_pool.begin()).tex;
    render_layers layer = (*batching_pool.begin()).layer;

    for(auto& sprite : batching_pool) {
        if (sprite.tex != current_tex || sprite.layer != layer) {
            render_batch(current_tex, layer, tm);
            current_tex = sprite.tex;
            layer = sprite.layer;
        }

        int quads_remaining = sprite.vertices().size() / vertices_per_quad;
        while (quads_remaining > 0) {
            const vertex* src_ptr = sprite.vertices().data() +  ((sprite.vertices().size() / vertices_per_quad) - quads_remaining)  * vertices_per_quad;
            vertex* vertex_buffer_ptr = vertex_buffer + quads_batched * vertices_per_quad;
            u8* zindex_buffer_ptr = zindex_buffer + quads_batched * vertices_per_quad;

            int quads_to_batch = std::min(quads_in_buffer - quads_batched, size_t(quads_remaining));
            int vertex_bytes_to_batch = quads_to_batch * vertices_per_quad * sizeof(vertex);
            int zindex_bytes_to_batch = quads_to_batch * vertices_per_quad ;

            quads_remaining -= quads_to_batch;
            quads_batched += quads_to_batch;

            memcpy(vertex_buffer_ptr, src_ptr, vertex_bytes_to_batch);
            memset(zindex_buffer_ptr, sprite.z_index, zindex_bytes_to_batch);

            if (quads_batched == quads_in_buffer) {
                current_tex = sprite.tex;
                layer = sprite.layer;
                render_batch(current_tex,layer, tm);
            }
        }
    }
    render_batch(current_tex, layer, tm);
}

/////////////////////////////////////////
//     COMMON TEXTURE MANAGER CODE     //
/////////////////////////////////////////

texture* texture_manager::get(std::string name) { return &textures[texture_map[name]]; }
texture* texture_manager::add(std::string name) {
    u32 id = get_new_id();
    texture* tex = &textures[id];
    tex->id = id;
    texture_map[name] = id;
    return tex;
}

void load_pixel_data(texture* data, std::string filename) {
    std::vector<unsigned char> pixels;
    unsigned int width, height;
    auto error = lodepng::decode(pixels, width, height, filename);
    if (error)
        printf("Error Loading texture: %s\n", lodepng_error_text(error));
    data->image_data = image(pixels, size<u16>(width, height));
}

void texture_manager::load_textures() {
    config_parser p("config/textures.txt");
    auto d = p.parse();
    for (auto it = d->begin(); it != d->end(); it++) {
        const config_list* list = dynamic_cast<const config_list*>((&it->second)->get());
        texture* tex = add(it->first);
        load_pixel_data(tex, "textures/" + list->get<std::string>(0));
        tex->regions = size<u16>(list->get<int>(1), list->get<int>(2));
        update(tex);
    }
}

#ifdef OPENGL

////////////////////////////////////////////////////////////////
//*     OpenGL window, renderer, and texture manager code    *//
////////////////////////////////////////////////////////////////

//////////////////////////////////
//*     OpenGL window  code    *//
//////////////////////////////////

gl_backend::gl_backend(screen_coords resolution_in) : sdl_window(resolution_in, SDL_WINDOW_OPENGL) { gl_context = SDL_GL_CreateContext(window); }
gl_backend::~gl_backend() { SDL_GL_DeleteContext(gl_context); }
void gl_backend::swap_buffers(renderer& r) { SDL_GL_SwapWindow(window); }
void gl_backend::set_vsync(bool state) { SDL_GL_SetSwapInterval(state ? 1 : 0); }

///////////////////////////////////
//*     OpenGL Renderer code    *//
///////////////////////////////////

renderer_gl::vbo::vbo() {
    glGenBuffers(1, &_id);
    bind();
}

renderer_gl::vbo::~vbo() {
    glDeleteBuffers(1, &_id);
}

void renderer_gl::vbo::bind() {
    glBindBuffer(GL_ARRAY_BUFFER, _id);
}

renderer_gl::shader::shader(u32 vert, u32 frag) {
    _ID = glCreateProgram();
    glAttachShader(_ID, vert);
    glAttachShader(_ID, frag);
    glLinkProgram(_ID);
    glDetachShader(_ID, vert);
    glDetachShader(_ID, frag);
}

renderer_gl::shader::~shader() {
    glDeleteProgram(_ID);
};

void renderer_gl::shader::register_uniform(std::string name){
    bind();
    attributes[name] = glGetUniformLocation(_ID, name.c_str());
};

template <typename... Args>
void renderer_gl::shader::update_uniform(std::string name, Args... args) {
    bind();
    if constexpr (sizeof...(args) == 3)
        glUniform3f(attributes[name], args...);
    if constexpr (sizeof...(args) == 2)
        glUniform2f(attributes[name], args...);
    if constexpr (sizeof...(args) == 1) {
        if constexpr(std::is_same<Args... ,float>::value) {
            glUniform1f(attributes[name], args...);
        } else {
            glUniformMatrix4fv(attributes[name], 1, GL_FALSE,args...);
        }
    }
}
void renderer_gl::shader::bind() {
    glUseProgram(_ID);
};

void read_shader_source(std::string& shader_code, std::string filename) {
    std::ifstream shader_stream(filename, std::ios::in);
    if (!shader_stream.is_open()) {
        throw std::runtime_error("Failed to load shaders.");
    }
    shader_code = std::string (std::istreambuf_iterator<char>{shader_stream}, {});
    shader_stream.close();
}

u32 gen_shader_stage(u32 type, std::string filename) {
    std::string shader_code;
    read_shader_source(shader_code, filename);

    const char* shader_ptr =shader_code.c_str();
    u32 ID = glCreateShader(type);

    glShaderSource(ID, 1, &shader_ptr, 0);
    glCompileShader(ID);
    i32 Result = GL_FALSE;
    int InfoLogLength;

    glGetShaderiv(ID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0)
    {
        std::vector<char>shaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(ID, InfoLogLength, NULL, &shaderErrorMessage[0]);
        printf("%s\n", &shaderErrorMessage[0]);
    }
    return ID;
}

renderer_gl::shader renderer_gl::gen_shader(std::string vert_filename, std::string frag_filename) {
    u32 frag = gen_shader_stage(GL_FRAGMENT_SHADER, frag_filename);
    u32 vert= gen_shader_stage (GL_VERTEX_SHADER, vert_filename);
    return renderer_gl::shader(frag, vert);
}


template <typename T>
T* map_buffer(size_t buffer_size) noexcept {
    glBufferData(GL_ARRAY_BUFFER, buffer_size, nullptr, GL_STREAM_DRAW);
    return static_cast<T*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
}

template <typename T>
T* unmap_buffer() noexcept {
    glUnmapBuffer(GL_ARRAY_BUFFER);
    return nullptr;
}

void renderer_gl::render_batch(texture* current_tex, render_layers layer, texture_manager&) {
    if(quads_batched == 0) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, current_tex->id);
    renderer_gl::shader& shader = get_shader(layer);
    shader.bind();
    zindex_vbo.bind();
    zindex_buffer = unmap_buffer<u8>();
    vertex_vbo.bind();
    vertex_buffer = unmap_buffer<vertex>();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads_batched * 6),
                   GL_UNSIGNED_SHORT, (void*) 0);
    vertex_vbo.bind();
    vertex_buffer = map_buffer<vertex>(vertex_buffer_size);
    zindex_vbo.bind();
    zindex_buffer = map_buffer<u8>(zindex_buffer_size);
    quads_batched = 0;
}

renderer_gl::shader& renderer_gl::get_shader(render_layers layer) {
    if (layer == render_layers::sprites)
        return map_shader;
    if (layer == render_layers::ui)
        return ui_shader;
    if (layer == render_layers::text)
        return text_shader;
    throw "oops";
}



void GLAPIENTRY MessageCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ), type, severity, message );
}

void renderer_gl::initialize_opengl() {
    glewInit();
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

renderer_gl::renderer_gl() {
    std::vector<u16> index_buffer = std::vector<u16>(quads_in_buffer * 6);

    size_t vert_index = 0;
    size_t array_index = 0;
    while (array_index < index_buffer.size()) {
        // top left corner
        index_buffer[array_index] = vert_index;
        index_buffer[array_index + 5] = vert_index ;
        // top right
        index_buffer[array_index + 1] = vert_index + 1;
        // bottom left
        index_buffer[array_index + 4] = vert_index + 3;
        // bottom right
        index_buffer[array_index + 2] = vert_index + 2;
        index_buffer[array_index + 3] = vert_index + 2;

        array_index += 6;
        vert_index += 4;
    }

    GLuint elementbuffer;
    glGenBuffers(1, &elementbuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buffer.size() * sizeof(u16), &index_buffer[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

    get_shader(render_layers::text).register_uniform("color");
    get_shader(render_layers::text).register_uniform("viewport");

    get_shader(render_layers::ui).register_uniform("viewport");

    get_shader(render_layers::sprites).register_uniform("viewport");
    get_shader(render_layers::sprites).register_uniform("viewMatrix");


    zindex_vbo.bind();
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, (void*) 0);

    vertex_vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) (sizeof(sprite_coords)));


    vertex_buffer = map_buffer<vertex>(vertex_buffer_size);
    zindex_buffer = map_buffer<u8>(zindex_buffer_size);
}

void renderer_gl::set_viewport(screen_coords screen_size) {
    glViewport(0, 0, screen_size.x, screen_size.y);
    viewport = size<f32>(128.0f / screen_size.x, 128.0f / screen_size.y);
    get_shader(render_layers::text).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    get_shader(render_layers::ui).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    get_shader(render_layers::sprites).update_uniform("viewport", viewport.x, viewport.y);
    get_shader(render_layers::sprites).update_uniform("viewMatrix", camera.data());
}

void renderer_gl::set_camera(vec2d<f32> delta) {
    camera[12] = -((viewport.x) * delta.x);
    camera[13] = (viewport.y) * delta.y;
    get_shader(render_layers::sprites).update_uniform("viewMatrix", camera.data());
}

void renderer_gl::clear_screen()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(2);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

///////////////////////////////////////////
//*     OpenGL texture manager code     *//
///////////////////////////////////////////

void texture_manager_gl::update(texture* tex) {
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->image_data.size().x, tex->image_data.size().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex->image_data.data().data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
}

u32 texture_manager_gl::get_new_id() {
    u32 id;
    glGenTextures(1, &id);
    return id;
}

#endif //OPENGL

//////////////////////////////////////////////////////////////////
//     SOFTWARE WINDOW, RENDERER, AND TEXTURE MANAGER CODE     //
/////////////////////////////////////////////////////////////////

/////////////////////////////////
//     SOFTWARE WINDOW CODE    //
/////////////////////////////////

void software_backend::attach_shm(framebuffer& fb) { fb = framebuffer(reinterpret_cast<u8*>(buffer), resolution()); }
void software_backend::swap_buffers(renderer& r) {
    auto& frame = reinterpret_cast<renderer_software&>(r).get_framebuffer();
    if (reinterpret_cast<u32*>(frame.data()) != buffer) {
        attach_shm(frame);
        return;
    }
    SDL_UpdateTexture(fb_texture, NULL, buffer, resolution().x * sizeof (u32));
    SDL_RenderClear(renderer_handle);
    SDL_RenderCopy(renderer_handle, fb_texture , NULL, NULL);
    SDL_RenderPresent(renderer_handle);
}

void software_backend::set_vsync(bool state) {
    SDL_DestroyTexture(fb_texture);
    SDL_DestroyRenderer(renderer_handle);
    int flags = SDL_RENDERER_SOFTWARE | state ? SDL_RENDERER_PRESENTVSYNC : 0;
    renderer_handle =  SDL_CreateRenderer(window, -1, flags);
    fb_texture = SDL_CreateTexture(renderer_handle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, resolution().x, resolution().y);
}

software_backend::software_backend(screen_coords resolution_in) : sdl_window(resolution_in, 0) {
    renderer_handle =  SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    fb_texture = SDL_CreateTexture(renderer_handle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, resolution().x, resolution().y);
    buffer = new u32[resolution().x * resolution().y];
}

software_backend::~software_backend() {
    SDL_DestroyTexture(fb_texture);
    SDL_DestroyRenderer(renderer_handle);
    delete[] buffer;
}

////////////////////////////////////
//     SOFTWARE RENDERER CODE     //
////////////////////////////////////

image rescale_texture(image source, size<u16> new_size) {
    size<f32> upscale_ratio (new_size.x / source.size().x, new_size.y / source.size().y);
    size<u16> upscaled_size(source.size().x * upscale_ratio.x, source.size().y * upscale_ratio.y);
    image upscaled_tex(std::vector<u8>(upscaled_size.x * upscaled_size.y * 4), upscaled_size);

    size_t write_index = 0;
    size_t read_index = 0;
    point<f32> read_pos(0, 0);
    for (size_t y = 0; y < upscaled_size.y; y++) {
        for (size_t x = 0; x < upscaled_size.x; x++) {
            read_index = int(read_pos.x) + int(read_pos.y) * source.size().x ;
            color c = source.get(read_index);
            std::swap(c.r, c.b);
            upscaled_tex.write(write_index, c);


            write_index++;
            read_pos.x += 1.0f / upscale_ratio.x;
        }
        read_pos.y += 1.0f / upscale_ratio.y;
        read_pos.x = 0;
    }
    return upscaled_tex;
}

void renderer_software::clear_screen() { memset(fb.data(), 0, fb.size().x * fb.size().y * 4); }
renderer_software::renderer_software() { vertex_buffer = new vertex[quads_in_buffer * vertices_per_quad]; }
void renderer_software::set_viewport(screen_coords screen_size) { }
void renderer_software::set_camera(vec2d<f32> camera_in) { camera = camera_in; }

vertex transpose_vertex(std::array<f32, 6> matrix, vertex vert) {
    point<f32> new_vertex;
    new_vertex.x = (matrix[0] * vert.pos.x) + (matrix[1] * vert.pos.y) + matrix [2];
    new_vertex.y = (matrix[3] * vert.pos.x) + (matrix[4] * vert.pos.y) + matrix [5];
    return vertex {new_vertex, vert.uv};
}

void render_quad(void* fb_data, screen_coords fb_size, image& texture_data, vertex tl_vert, vertex br_vert) {
    point<f32> tl_clipped(round(std::max(0.0f, tl_vert.pos.x)), round(std::max(0.0f, tl_vert.pos.y)));
    point<f32> br_clipped(round(std::min(f32(fb_size.x), br_vert.pos.x)), round(std::min(f32(fb_size.y), br_vert.pos.y)));
    point<f32> clipped_offset = tl_clipped - tl_vert.pos;

    size<u16> tl_texel(tl_vert.uv.x * texture_data.size().x + abs(clipped_offset.x), tl_vert.uv.y * texture_data.size().y + abs(clipped_offset.y));
    size<u16> clipped_size = (br_clipped - tl_clipped).to<u16>();

    size_t fb_write_start = (tl_clipped.x  + (tl_clipped.y * fb_size.x)) * 4;
    size_t fb_write_stride = (fb_size.x) * 4;
    size_t tex_read_start = (tl_texel.x  + (tl_texel.y * texture_data.size().x)) * 4;
    size_t tex_read_stride = (texture_data.size().x) * 4;
    size_t bytes_per_line = clipped_size.x * 4;

    for (size_t y = 0; y < clipped_size.y; y++) {
        auto dest_offset = fb_write_start + (y * fb_write_stride);
        auto src_offset = tex_read_start + (y * tex_read_stride);

        memcpy(reinterpret_cast<char*>(fb_data) + dest_offset, texture_data.data().data() + src_offset, bytes_per_line);
    }
}

void renderer_software::render_batch(texture* current_tex, render_layers layer, texture_manager& tm_base) {
    std::array<f32, 6> matrix;
    switch (layer) {
        case render_layers::text:
        case render_layers::ui:
            matrix = std::array<f32, 6> { 1, 0, 0,
                                          0, 1, 0 };
            break;
        case render_layers::sprites:
            matrix = std::array<f32, 6> { 64, 0, 64 * -camera.x,
                                          0, 64, 64 * -camera.y};
            break;
        case render_layers::null:
            return;
    }

    auto& textures = dynamic_cast<texture_manager_software&>(tm_base);
    rect<f32> frame(point<f32>(0, 0), fb.size().to<f32>());
    for (size_t i = 0; i < quads_batched * 4; i+= 4) {
        const auto tl_vert = transpose_vertex(matrix, vertex_buffer[i]);
        const auto br_vert = transpose_vertex(matrix, vertex_buffer[i + 2]);

        rect<f32> sprite_rect(tl_vert.pos, br_vert.pos - tl_vert.pos);
        if (!AABB_collision(frame, sprite_rect)) continue;

        size<f32> subregion_size = current_tex->image_data.size().to<f32>() * (br_vert.uv - tl_vert.uv);

        // test if the sprite's render size is 4x the texture, 2x, 1x, or something else.
        if ((subregion_size * 4) - sprite_rect.size < size<f32>(0.01, 0.01)) {
            render_quad(fb.data(), fb.size(), textures.textures_4x_scaled[current_tex->id], tl_vert, br_vert);
        } else if ((subregion_size * 2) - sprite_rect.size < size<f32>(0.01, 0.01)) {
            render_quad(fb.data(), fb.size(), textures.textures_2x_scaled[current_tex->id], tl_vert, br_vert);
        } else if (subregion_size - sprite_rect.size < size<f32>(0.01, 0.01)) {
            render_quad(fb.data(), fb.size(), current_tex->image_data, tl_vert, br_vert);
        } else {
            image a = rescale_texture(current_tex->image_data, sprite_rect.size.to<u16>());
            render_quad(fb.data(), fb.size(), a, tl_vert, br_vert);
        }
    }
    quads_batched = 0;
}

///////////////////////////////////////////
//     SOFTWARE TEXTURE MANAGER CODE     //
///////////////////////////////////////////

void texture_manager_software::update(texture* tex) {
    image& image_data = tex->image_data;
    if(image_data.size().x == 0 && image_data.size().y == 0) return;

    textures_2x_scaled[tex->id] = rescale_texture(image_data, size<u16>(image_data.size().x * 2, image_data.size().y * 2));
    textures_4x_scaled[tex->id] = rescale_texture(image_data, size<u16>(image_data.size().x * 4, image_data.size().y * 4));
}

u32 texture_manager_software::get_new_id() {
    u32 id = texture_counter;
    texture_counter++;
    return id;
}
}
