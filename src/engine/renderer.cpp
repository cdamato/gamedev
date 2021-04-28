#include <common/png.h>
#include <common/parser.h>
#include <GL/glew.h>
#include <assert.h>
#include "renderer.h"

static constexpr unsigned VERTICES_PER_QUAD = 4;
static constexpr unsigned NUM_QUADS = 1024;
static constexpr unsigned buffer_size = NUM_QUADS * VERTICES_PER_QUAD * sizeof(vertex);

void renderer_base::clear_sprites() {}

void renderer_base::render_layer(std::multiset<sprite_data> sprites) {
    if (sprites.size() == 0 ) return;
    texture current_tex = (*sprites.begin()).tex;
    render_layers layer = (*sprites.begin()).layer;

    for(auto sprite : sprites) {
        if (sprite.tex != current_tex) {
            render_batch(current_tex, layer);
            current_tex = sprite.tex;
            layer = sprite.layer;
        }

        int quads_remaining = sprite.size;
        while (quads_remaining > 0) {
            const vertex* src_ptr = sprite.vertices +  (sprite.size - quads_remaining)  * VERTICES_PER_QUAD;
            vertex* dest_ptr = mapped_vertex_buffer + quads_batched * VERTICES_PER_QUAD;

            int quads_to_batch = std::min(NUM_QUADS - quads_batched, size_t(quads_remaining));
            int bytes_to_batch = quads_to_batch * VERTICES_PER_QUAD * sizeof(vertex);

            quads_remaining -= quads_to_batch;
            quads_batched += quads_to_batch;

            memcpy(dest_ptr, src_ptr, bytes_to_batch);

            if (quads_batched == NUM_QUADS) {
                current_tex = sprite.tex;
                layer = sprite.layer;
                render_batch(current_tex,layer);
            }
        }
    }
    render_batch(current_tex, layer);
}

void renderer_base::set_texture_data(texture tex, texture_data data) {
    if (textures.size() <= tex)
        textures.resize(tex + 1);
    textures[tex] = data;
    process_texture_data(tex);
}

texture renderer_base::get_texture(std::string name) { return texture_map[name]; }



void load_pixel_data(texture_data& data, std::string filename) {
    std::vector<unsigned char> pixels;
    unsigned int width;
    unsigned int height;
    auto error = lodepng::decode(pixels, width, height, filename);

    if (error)
        printf("Error Loading texture: %s\n", lodepng_error_text(error));
    // With the file's data in hand, do the actual setting
    data.image_data = image(pixels, size<u16>(width, height));
}


void renderer_base::load_textures() {
    config_parser p("config/textures.txt");
    auto d = p.parse();
    for (auto it = d->begin(); it != d->end(); it++) {
        const config_list* list = dynamic_cast<const config_list*>((&it->second)->get());
        texture tex = add_texture(it->first);
        texture_data data;
        load_pixel_data(data, "textures/" + list->get<std::string>(0));
        data.regions = size<u16>(list->get<int>(1), list->get<int>(2));
        data.z_index = list->get<int>(3);
        data.scale_factor = list->get<int>(4);
        set_texture_data(tex, data);
    }
}









#ifdef OPENGL



void bind_texture(texture tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
}

class shader : no_copy, no_move
{
    u32 _ID;
    std::unordered_map<std::string, int> attributes = std::unordered_map<std::string, int>();
public:
    shader(u32 vert, u32 frag);
    ~shader();
    void register_uniform(std::string name);
    void bind();
    template <typename... Args>
    void update_uniform(std::string name, Args... args);
};



shader::shader(u32 vert, u32 frag) {
    _ID = glCreateProgram();
    glAttachShader(_ID, vert);
    glAttachShader(_ID, frag);
    glLinkProgram(_ID);
    glDetachShader(_ID, vert);
    glDetachShader(_ID, frag);
}

shader::~shader() {
    glDeleteProgram(_ID);
};

void shader::register_uniform(std::string name){
    bind();
    attributes[name] = glGetUniformLocation(_ID, name.c_str());
};

template <typename... Args>
void shader::update_uniform(std::string name, Args... args) {
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
void shader::bind() {
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

shader gen_map_shader() {
    u32 frag = gen_shader_stage(GL_FRAGMENT_SHADER, "shaders/shader.frag");
    u32 vert= gen_shader_stage (GL_VERTEX_SHADER, "shaders/world.vert");
    return shader(frag, vert);
}

shader gen_ui_shader() {
    u32 frag = gen_shader_stage(GL_FRAGMENT_SHADER, "shaders/shader.frag");
    u32 vert= gen_shader_stage (GL_VERTEX_SHADER, "shaders/ui.vert");
    return shader(frag, vert);
}

shader gen_text_shader() {
    u32 frag = gen_shader_stage(GL_FRAGMENT_SHADER, "shaders/text.frag");
    u32 vert= gen_shader_stage (GL_VERTEX_SHADER, "shaders/ui.vert");
    return shader(frag, vert);
}



class renderer_gl::impl {
public:
    shader& get_shader(render_layers layer);

    shader text_shader = gen_text_shader();
    shader ui_shader = gen_ui_shader();
    shader map_shader = gen_map_shader();

    size<f32> viewport;
    std::vector<f32> camera = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
};


vertex* map_buffer() noexcept {
    glBufferData(GL_ARRAY_BUFFER, renderer_base::buffer_size, nullptr, GL_STREAM_DRAW);
    return static_cast<vertex*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
}

vertex* unmap_buffer() noexcept {
    glUnmapBuffer(GL_ARRAY_BUFFER);
    return nullptr;
}

void renderer_gl::render_batch(texture current_tex, render_layers layer) {
    if(quads_batched == 0) {
        return;
    }
    bind_texture(current_tex);
    shader& shader = data->get_shader(layer);
    shader.update_uniform("z_index", textures[current_tex].z_index);

    mapped_vertex_buffer = unmap_buffer();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads_batched * 6),
                   GL_UNSIGNED_SHORT, (void*) 0);
    mapped_vertex_buffer = map_buffer();
    quads_batched = 0;
}
shader& renderer_gl::impl::get_shader(render_layers layer) {
    if (layer == render_layers::sprites)
        return map_shader;
    if (layer == render_layers::ui)
        return ui_shader;
    if (layer == render_layers::text)
        return text_shader;
    throw "oops";
}



void GLAPIENTRY MessageCallback( GLenum source, GLenum type, GLuint id,
GLenum severity, GLsizei length, const GLchar* message, const void* userParam )
{
fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
type, severity, message );
}

texture renderer_gl::add_texture(std::string name) {
    u32 obj;
    glGenTextures(1, &obj);
    texture_map[name] = obj;
    return obj;
}

renderer_gl::renderer_gl() {
    glewInit();
    glEnable              ( GL_DEBUG_OUTPUT );
    glEnable( GL_BLEND );
    glEnable(GL_DEPTH_TEST);
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    data = new impl;

    load_textures();
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

    data->get_shader(render_layers::text).register_uniform("color");
    data->get_shader(render_layers::text).register_uniform("viewport");
    data->get_shader(render_layers::text).register_uniform("z_index");

    data->get_shader(render_layers::ui).register_uniform("viewport");
    data->get_shader(render_layers::ui).register_uniform("z_index");

    data->get_shader(render_layers::sprites).register_uniform("viewport");
    data->get_shader(render_layers::sprites).register_uniform("z_index");
    data->get_shader(render_layers::sprites).register_uniform("viewMatrix");

    unsigned int ID;
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) (sizeof(sprite_coords)));

    mapped_vertex_buffer = map_buffer();
}

void renderer_gl::set_viewport(screen_coords screen_size) {
    glViewport(0, 0, screen_size.x, screen_size.y);
    data->viewport = size<f32>(128.0f / screen_size.x, 128.0f / screen_size.y);
    data->get_shader(render_layers::text).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    data->get_shader(render_layers::ui).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    data->get_shader(render_layers::sprites).update_uniform("viewport", data->viewport.x, data->viewport.y);
    data->get_shader(render_layers::sprites).update_uniform("z_index", 0.0f);
    data->get_shader(render_layers::sprites).update_uniform("viewMatrix", data->camera.data());
}

void renderer_gl::set_camera(vec2d<f32> delta) {
    data->camera[12] = -((data->viewport.x) * delta.x);
    data->camera[13] = (data->viewport.y) * delta.y;
    data->get_shader(render_layers::sprites).update_uniform("viewMatrix", data->camera.data());
}

void renderer_gl::clear_screen()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(2);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void renderer_gl::process_texture_data(texture tex) {
    texture_data data = textures[tex];
    unsigned data_type = data.is_greyscale ? 0x1903 : GL_RGBA;
    bind_texture(tex);
    glTexImage2D(GL_TEXTURE_2D, 0, data_type, data.image_data.size().x, data.image_data.size().y, 0, data_type, GL_UNSIGNED_BYTE, data.image_data.data().data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
}

#endif //OPENGL










void renderer_software::clear_screen() {
    //printf("frame is %p, \n", reinterpret_cast<renderer_software*>(_renderer.get())->get_framebuffer().data());
    //memset(fb.data(), 255, fb.size().w * fb.size().y * 4);
}

texture renderer_software::add_texture(std::string name) {
    texture_map[name] = texture_counter;
    texture tex = texture_counter;
    texture_counter++;
    return tex;
}

renderer_software::renderer_software(screen_coords resolution_in) {
    //resolution = resolution_in;
    load_textures();
    mapped_vertex_buffer = new vertex[NUM_QUADS  * VERTICES_PER_QUAD];
}
void renderer_software::process_texture_data(texture tex_id) {
    auto& tex_data = textures[tex_id];
    image& tex = tex_data.image_data;
    size<f32> upscale_ratio (1.0f / tex_data.scale_factor, 1.0f / tex_data.scale_factor);
    size<u16> upscaled_size(tex.size().x / upscale_ratio.x, tex.size().y / upscale_ratio.y);
    image upscaled_tex(std::vector<u8>(upscaled_size.x * upscaled_size.y * 4), upscaled_size);

    size_t write_index = 0;
    size_t read_index = 0;
    point<f32> read_pos(0, 0);
    for (size_t y = 0; y < upscaled_size.y; y++) {
        for (size_t x = 0; x < upscaled_size.x; x++) {
            read_index = int(read_pos.x) + int(read_pos.y) * tex.size().x ;
            color c = tex.get(read_index);
            std::swap(c.r, c.b);
            upscaled_tex.write(write_index, c);


            write_index++;
            read_pos.x += upscale_ratio.x;
        }
        read_pos.y += upscale_ratio.y;
        read_pos.x = 0;
    }
    std::swap(tex_data.image_data, upscaled_tex);
}

void renderer_software::set_viewport(screen_coords screen_size) {
    //resolution = screen_size;
}


void renderer_software::set_camera(vec2d<f32> camera_in) {
    camera = camera_in;
}

vertex transpose_vertex(std::array<f32, 4> matrix, vertex vert) {
    point<f32> new_vertex;
    new_vertex.x = (matrix[0] * vert.pos.x) + (matrix[1] * vert.pos.y);
    new_vertex.y = (matrix[2] * vert.pos.x) + (matrix[3] * vert.pos.y);
    return vertex {new_vertex, vert.uv};
}

#ifdef SSE
#include <immintrin.h>
// Utilizes SSE intrinsics to achieve non-temporal writes. to remove a bottleneck created by memcpy.
void alt_memcpy(void* data, void* src, std::size_t bytes)
{
    assert(bytes % 64 == 0);
    auto vec_pointer = static_cast<float*>(data);
    auto src_pointer = static_cast<__m128*>(src);
    const auto zero = _mm_setzero_ps();

    // One float is equivalent to 1 pixel byte - one SSE transfer carries 4 pixels.
    // One loop iteration will carry 16 pixels, which works well for most cases.
    for(const auto vec_end = vec_pointer + (bytes / sizeof(float)); vec_pointer != vec_end; vec_pointer += 16, src_pointer += 4) {
        _mm_stream_ps(&vec_pointer[0], src_pointer[0]);
        _mm_stream_ps(&vec_pointer[4], src_pointer[1]);
        _mm_stream_ps(&vec_pointer[8], src_pointer[2]);
        _mm_stream_ps(&vec_pointer[12], src_pointer[3]);
    }
}
#else

// A fallback passthrough function to regular memcpy
void alt_memcpy(void* data, void* src, std::size_t bytes)
{
    memcpy(data, src, bytes);
}
#endif //SSE

void renderer_software::render_batch(texture current_tex, render_layers layer) {
    std::array<f32, 4> matrix;
    switch (layer) {
        case render_layers::text:
        case render_layers::ui:
            matrix = std::array<f32, 4> { 1, 0, 0, 1 };
            break;
        case render_layers::sprites:
            matrix = std::array<f32, 4> { 64, 0, 0, 64 };
            break;
    }

    if (current_tex >= textures.size()) throw "bruh";
    image& tex = textures[current_tex].image_data;

    rect<f32> frame(point<f32>(0, 0), fb.size().to<f32>());
    for (size_t i = 0; i < quads_batched * 4; i+= 4) {
        auto tl_vert = transpose_vertex(matrix, mapped_vertex_buffer[i]);
        auto br_vert = transpose_vertex(matrix, mapped_vertex_buffer[i + 2]);

        rect<f32> sprite_rect(tl_vert.pos, tl_vert.pos - br_vert.pos);
        if (!AABB_collision(frame, sprite_rect)) continue;

        tl_vert.pos = point<f32>(std::max(0.0f, tl_vert.pos.x), std::max(0.0f, tl_vert.pos.y));
        br_vert.pos = point<f32>(std::min(f32(fb.size().x), br_vert.pos.x), std::min(f32(fb.size().y), br_vert.pos.y));

        size<u16> clipped_size(br_vert.pos.x - tl_vert.pos.x, br_vert.pos.y - tl_vert.pos.y);
        size_t write_index = (tl_vert.pos.x  + (tl_vert.pos.y * fb.size().x)) * 4;
        size_t bytes_per_line = clipped_size.x * 4;
        size_t write_stride = (fb.size().x) * 4;

        for (size_t y = 0; y < clipped_size.y; y++) {
            alt_memcpy(fb.data() + write_index + (y * write_stride) , tex.data().data() + (y * 4 *  tex.size().x), bytes_per_line);
        }
    }
    quads_batched = 0;
}