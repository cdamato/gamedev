#include <common/png.h>
#include <common/parser.h>
#include <GL/glew.h>
#include "renderer.h"

static constexpr unsigned VERTICES_PER_QUAD = 4;
static constexpr unsigned NUM_QUADS = 1024;
static constexpr unsigned buffer_size = NUM_QUADS * VERTICES_PER_QUAD * sizeof(vertex);

void renderer_base::clear_sprites() {}

void renderer_base::render_layer(std::multiset<subsprite> sprites) {
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
            const vertex* src_ptr = sprite.data +  (sprite.size - quads_remaining)  * VERTICES_PER_QUAD;
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
    if (texture_datas.size() <= tex)
        texture_datas.resize(tex + 1);
    texture_datas[tex] = data;
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
        data.subsprites = size<u16>(list->get<int>(1), list->get<int>(2));
        data.z_index = list->get<int>(3);
        set_texture_data(tex, data);
    }
}













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
    shader.update_uniform("z_index", texture_datas[current_tex].z_index);

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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*) (sizeof(point<f32>)));

    mapped_vertex_buffer = map_buffer();
}

void renderer_gl::set_viewport(size<u16> screen_size) {
    glViewport(0, 0, screen_size.w, screen_size.h);
    data->viewport = size<f32>(128.0f / screen_size.w, 128.0f / screen_size.h);
    data->get_shader(render_layers::text).update_uniform("viewport", 2.0f / screen_size.w, 2.0f / screen_size.h);
    data->get_shader(render_layers::ui).update_uniform("viewport", 2.0f / screen_size.w, 2.0f / screen_size.h);
    data->get_shader(render_layers::sprites).update_uniform("viewport", data->viewport.w, data->viewport.h);
    data->get_shader(render_layers::sprites).update_uniform("z_index", 0.0f);
    data->get_shader(render_layers::sprites).update_uniform("viewMatrix", data->camera.data());
}

void renderer_gl::set_camera(point<f32> delta) {
    data->camera[12] = -((data->viewport.w) * delta.x);
    data->camera[13] = (data->viewport.h) * delta.y;
    data->get_shader(render_layers::sprites).update_uniform("viewMatrix", data->camera.data());
}

void renderer_gl::clear_screen()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(2);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void renderer_gl::process_texture_data(texture tex) {
    texture_data data = texture_datas[tex];
    unsigned data_type = data.is_greyscale ? 0x1903 : GL_RGBA;
    bind_texture(tex);
    glTexImage2D(GL_TEXTURE_2D, 0, data_type, data.image_data.size().w, data.image_data.size().h, 0, data_type, GL_UNSIGNED_BYTE, data.image_data.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
}















void renderer_software::clear_screen() {
    framebuffer = image(std::vector<u8>(resolution.w * resolution.h * 4, 255), resolution);
}

texture renderer_software::add_texture(std::string name) {
    texture_map[name] = texture_counter;
    texture tex = texture_counter;
    texture_counter++;
    return tex;
}

renderer_software::renderer_software() {
    clear_screen();
    load_textures();
    mapped_vertex_buffer = new vertex[NUM_QUADS * VERTICES_PER_QUAD];
}

void renderer_software::set_viewport(size<u16> screen_size) {
    resolution = screen_size;
}


void renderer_software::set_camera(point<f32> camera_in) {
    camera = camera_in;
}

point<f32> transpose_vertex(std::vector<f32> matrix, point<f32> vertex) {
    point<f32> new_vertex;
    new_vertex.x = (matrix[0] * vertex.x)  - 1.0;
    new_vertex.y = (-matrix[3] * vertex.y) + 1.0;
    return new_vertex;
}

void renderer_software::render_batch(texture current_tex, render_layers layer) {
    std::vector<f32> matrix;
    switch (layer) {
        case render_layers::text:
        case render_layers::ui:
            matrix = std::vector<f32> { 2.0f / resolution.w, 0, 0, 2.0f / resolution.h };
            break;
        case render_layers::sprites:
            break;
    }
    if (current_tex >= texture_datas.size()) throw "bruh";
    image tex = texture_datas[current_tex].image_data;

    for (size_t i = 0; i < quads_batched; i+= 4) {
        auto tl_vert = mapped_vertex_buffer[i];
        auto br_vert = mapped_vertex_buffer[i + 3];

        size<f32> pos_bounds (br_vert.pos.x - tl_vert.pos.x, br_vert.pos.y - tl_vert.pos.y );
        size<f32> uv_bounds (br_vert.uv.x - tl_vert.uv.x, br_vert.uv.y - tl_vert.uv.y );

        point<u16> texel_origin (tex.size().w * tl_vert.uv.x, tex.size().h * tl_vert.uv.y);
        size<u16> texel_bounds (tex.size().w * uv_bounds.w, tex.size().h * uv_bounds.h);

        size<f32> texel_ratio (f32(texel_bounds.w) / pos_bounds.w , f32(texel_bounds.h) / pos_bounds.h);
        point<f32> texel_pos = texel_origin.to<f32>();
        size_t read_index = 0;
        size_t write_index = tl_vert.pos.x + (tl_vert.pos.y * resolution.w);

        for (size_t y = 0; y < pos_bounds.h; y++) {
            for (size_t x = 0; x < pos_bounds.w; x++) {
                size_t texel_index = int(texel_pos.x) + int(texel_pos.y) * tex.size().w ;
                color c = tex.get(texel_index);

                // TODO - implement alpha blending
                if (c.a == 255) {
                    framebuffer.write(write_index, c);
                }

                write_index++;
                texel_pos.x += texel_ratio.w;
            }
            write_index += resolution.w - (pos_bounds.w); // jump to the beginning of the next row
            texel_pos.y += texel_ratio.h;
            texel_pos.x = texel_origin.x;
        }
    }

    quads_batched = 0;

    //Encode the image
    //unsigned error = lodepng::encode("render.png", framebuffer.data, resolution.w, resolution.h);

    //if there's an error, display it
    //if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
}
