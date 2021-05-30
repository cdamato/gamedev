#include <common/png.h>
#include <common/parser.h>
#include <GL/glew.h>
#include <assert.h>
#include <cmath>
#include "display_impl.h"
#include "display.h"

namespace display {
constexpr unsigned quads_in_buffer = 1024;
constexpr unsigned buffer_size = quads_in_buffer * vertices_per_quad * sizeof(vertex);


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
        tex->z_index = list->get<int>(3);
        tex->scale_factor = list->get<int>(4);
        update(tex);
    }
}


void renderer::add_sprite(sprite_data& sprite) { batching_pool.emplace(sprite); }
void renderer::clear_sprites() { batching_pool.clear(); }
void renderer::render_layer(texture_manager& tm) {
    if (batching_pool.size() == 0 ) return;
    texture* current_tex = (*batching_pool.begin()).tex;
    render_layers layer = (*batching_pool.begin()).layer;

    for(auto sprite : batching_pool) {
        if (sprite.tex != current_tex || sprite.layer != layer) {
            render_batch(current_tex, layer, tm);
            current_tex = sprite.tex;
            layer = sprite.layer;
        }

        int quads_remaining = sprite.vertices().size() / vertices_per_quad;
        while (quads_remaining > 0) {
            const vertex* src_ptr = sprite.vertices().data() +  ((sprite.vertices().size() / vertices_per_quad) - quads_remaining)  * vertices_per_quad;
            vertex* dest_ptr = batching_buffer + quads_batched * vertices_per_quad;

            int quads_to_batch = std::min(quads_in_buffer - quads_batched, size_t(quads_remaining));
            int bytes_to_batch = quads_to_batch * vertices_per_quad * sizeof(vertex);

            quads_remaining -= quads_to_batch;
            quads_batched += quads_to_batch;

            memcpy(dest_ptr, src_ptr, bytes_to_batch);

            if (quads_batched == quads_in_buffer) {
                current_tex = sprite.tex;
                layer = sprite.layer;
                render_batch(current_tex,layer, tm);
            }
        }
    }
    render_batch(current_tex, layer, tm);
}




#ifdef OPENGL

void texture_manager_gl::update(texture* tex) {
    unsigned data_type = tex->is_greyscale ? 0x1903 : GL_RGBA;
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, data_type, tex->image_data.size().x, tex->image_data.size().y, 0, data_type, GL_UNSIGNED_BYTE, tex->image_data.data().data());
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



vertex* map_buffer() noexcept {
    glBufferData(GL_ARRAY_BUFFER, buffer_size, nullptr, GL_STREAM_DRAW);
    return static_cast<vertex*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
}

vertex* unmap_buffer() noexcept {
    glUnmapBuffer(GL_ARRAY_BUFFER);
    return nullptr;
}

void renderer_gl::render_batch(texture* current_tex, render_layers layer, texture_manager&) {
    if(quads_batched == 0) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, current_tex->id);
    renderer_gl::shader& shader = get_shader(layer);
    shader.update_uniform("z_index", current_tex->z_index);

    batching_buffer = unmap_buffer();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quads_batched * 6),
                   GL_UNSIGNED_SHORT, (void*) 0);
    batching_buffer = map_buffer();
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
    get_shader(render_layers::text).register_uniform("z_index");

    get_shader(render_layers::ui).register_uniform("viewport");
    get_shader(render_layers::ui).register_uniform("z_index");

    get_shader(render_layers::sprites).register_uniform("viewport");
    get_shader(render_layers::sprites).register_uniform("z_index");
    get_shader(render_layers::sprites).register_uniform("viewMatrix");

    unsigned int ID;
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) (sizeof(sprite_coords)));

    batching_buffer = map_buffer();
}

void renderer_gl::set_viewport(screen_coords screen_size) {
    glViewport(0, 0, screen_size.x, screen_size.y);
    viewport = size<f32>(128.0f / screen_size.x, 128.0f / screen_size.y);
    get_shader(render_layers::text).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    get_shader(render_layers::ui).update_uniform("viewport", 2.0f / screen_size.x, 2.0f / screen_size.y);
    get_shader(render_layers::sprites).update_uniform("viewport", viewport.x, viewport.y);
    get_shader(render_layers::sprites).update_uniform("z_index", 0.0f);
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


#endif //OPENGL


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

void renderer_software::clear_screen() {
    memset(fb.data(), 0, fb.size().x * fb.size().y * 4);
}


renderer_software::renderer_software() {
    batching_buffer = new vertex[quads_in_buffer * vertices_per_quad];
}



void renderer_software::set_viewport(screen_coords screen_size) {
    //resolution = screen_size;
}


void renderer_software::set_camera(vec2d<f32> camera_in) {
    camera = camera_in;
}


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
        const auto tl_vert = transpose_vertex(matrix, batching_buffer[i]);
        const auto br_vert = transpose_vertex(matrix, batching_buffer[i + 2]);

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
}