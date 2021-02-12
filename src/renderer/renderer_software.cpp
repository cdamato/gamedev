#include "renderer.h"
#include <common/png.h>


//
// I haven't yet figured out a good way to render framebuffers in x11.
// Until I do so, this backend is "almost working, but useless"
//


/*
static constexpr unsigned VERTICES_PER_QUAD = 4;
static constexpr unsigned NUM_QUADS = 1024;

void renderer_software::clear_screen() {
    framebuffer.data = std::vector<u8>(resolution.w * resolution.h * 4, 255);
}

texture renderer_software::add_texture(std::string name) {
    texture tex = texture_map[name];
    tex = texture_counter;
    texture_counter++;
    textures.emplace_back(image{});
    return tex;
}

renderer_software::renderer_software() {
    framebuffer.data = std::vector<u8>(resolution.w * resolution.h * 4, 255);
    load_textures();
    mapped_vertex_buffer = new vertex[NUM_QUADS * VERTICES_PER_QUAD];
}

void renderer_software::set_viewport(size<u16> screen_size) {
    resolution = screen_size;
}


void renderer_software::set_camera(point<f32> camera_in) {
    camera = camera_in;
}

void renderer_software::set_texture_data(texture tex, std::vector<u8>& data, size<u16> size, bool ) {
    textures[tex] = image{size, std::vector<u8>(data)};
}


point<f32> transpose_vertex(std::vector<f32> matrix, point<f32> vertex) {
    point<f32> new_vertex;
    new_vertex.x = (matrix[0] * vertex.x)  - 1.0;
    new_vertex.y = (-matrix[3] * vertex.y) + 1.0;
    return new_vertex;
}

#include <iostream>
void renderer_software::render_batch(render_layers layer) {
    std::vector<f32> matrix;
    switch (layer) {
        case render_layers::text:
        case render_layers::ui:
            matrix = std::vector<f32> { 2.0f / resolution.w, 0, 0, 2.0f / resolution.h };
            break;
        case render_layers::sprites:
            break;
    }
    size_t tex_id = current_tex;
    if (tex_id >= textures.size()) throw "bruh";
    image tex = textures[tex_id];

    for (size_t i = 0; i < quads_batched; i+= 4) {
        auto tl_vert = mapped_vertex_buffer[i];
        auto br_vert = mapped_vertex_buffer[i + 3];

        size<f32> pos_bounds (br_vert.pos.x - tl_vert.pos.x, br_vert.pos.y - tl_vert.pos.y );
        size<f32> uv_bounds (br_vert.uv.x - tl_vert.uv.x, br_vert.uv.y - tl_vert.uv.y );

        point<u16> texel_origin (tex.size.w * tl_vert.uv.x, tex.size.h * tl_vert.uv.y);
        size<u16> texel_bounds (tex.size.w * uv_bounds.w, tex.size.h * uv_bounds.h);

        size<f32> texel_ratio (f32(texel_bounds.w) / pos_bounds.w , f32(texel_bounds.h) / pos_bounds.h);
        point<f32> texel_pos = texel_origin.to<f32>();
        size_t read_index = 0;
        size_t write_index = tl_vert.pos.x + (tl_vert.pos.y * resolution.w);

        for (size_t y = 0; y < pos_bounds.h; y++) {
            for (size_t x = 0; x < pos_bounds.w; x++) {
                size_t texel_index = int(texel_pos.x) + int(texel_pos.y) * tex.size.w ;
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
   // if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
}
*/