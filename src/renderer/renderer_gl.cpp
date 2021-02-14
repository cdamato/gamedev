#include "renderer.h"
#include <GL/glew.h>


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


// forward-declare functions
shader gen_text_shader();
shader gen_ui_shader();
shader gen_map_shader();


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

void renderer_gl::render_batch(render_layers layer) {
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


u32 gen_shader(u32 type, const char* code)
{
	u32 ID = glCreateShader(type);
	glShaderSource(ID, 1, &code, 0);
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

shader gen_map_shader()
{
	const char* frag_code =
		"#version 120\n"

		"uniform sampler2D tex;"
		"varying vec2 uv_out;"

		"void main()"
		"{"
			"gl_FragColor = vec4(texture2D(tex, uv_out).rgba);"
		"}";
	const char* vert_code =
		"#version 120\n"

		"uniform vec2 viewport;"
		"uniform mat4 viewMatrix;"
		"uniform float z_index;"

		"attribute vec3 pos;"
		"attribute vec2 uv;"


		"varying vec2 uv_out;"

		"void main()"
		"{"
			"gl_Position =  viewMatrix * vec4(pos.x * viewport.x - 1.0, pos.y * -viewport.y + 1.0, "
			"1.0 - (z_index / 10), 1);"
			"uv_out = uv;"
		"}";

	u32 frag = gen_shader(GL_FRAGMENT_SHADER, frag_code);
	u32 vert= gen_shader (GL_VERTEX_SHADER, vert_code);

	return shader(frag, vert);

}


shader gen_ui_shader() {
	const char* frag_code =
		"#version 120\n"

		"uniform sampler2D tex;"
		"varying vec2 uv_out;"

		"void main()"
		"{"
			"gl_FragColor = vec4(texture2D(tex, uv_out).rgba);"
		"}";

	const char* vert_code =
		"#version 120\n"

		"uniform vec2 viewport;"
		"uniform float z_index;"

		"attribute vec3 pos;"
		"attribute vec2 uv;"

		"varying vec2 uv_out;"

		"void main()"
		"{"
			"gl_Position =  vec4(pos.x * viewport.x - 1.0, pos.y  * -viewport.y + 1.0, 0.5 - (z_index / 10), 1);"
			"uv_out = uv;"
		"}";


	u32 frag = gen_shader(GL_FRAGMENT_SHADER, frag_code);
	u32 vert = gen_shader(GL_VERTEX_SHADER, vert_code);

	return shader(frag, vert);
}


shader gen_text_shader()
{
	const char* frag_code =
		"#version 120\n"

		"uniform sampler2D tex;"
		"uniform vec3 color;"

		"varying vec2 uv_out;"

		"void main() {"
			"gl_FragColor = vec4(color.rgb, texture2D(tex, uv_out).r);"
		"}";
	const char* vert_code =
		"#version 120\n"

		"uniform vec2 viewport;"
		"attribute vec3 pos;"
		"attribute vec2 uv;"

		"varying vec2 uv_out;"

		"void main()"
		"{"
			"gl_Position =  vec4(pos.x * viewport.x - 1.0, pos.y  * -viewport.y + 1.0, 0.5, 1);"
			"uv_out = uv;"
		"}";

	u32 frag = gen_shader(GL_FRAGMENT_SHADER, frag_code);
	u32 vert = gen_shader(GL_VERTEX_SHADER, vert_code);

	return shader(frag, vert);
}