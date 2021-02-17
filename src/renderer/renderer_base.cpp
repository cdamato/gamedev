#include <cstring>
#include <common/png.h>
#include <common/parser.h>
#include "renderer.h"


static constexpr unsigned VERTICES_PER_QUAD = 4;
static constexpr unsigned NUM_QUADS = 1024;
static constexpr unsigned buffer_size = NUM_QUADS * VERTICES_PER_QUAD * sizeof(vertex);

void renderer_base::clear_sprites() {
   // //sprites.clear();
   // text.clear();
  //  ui.clear();
}

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


texture renderer_base::get_texture(std::string name) {
    return texture_map[name];
}



void load_image(texture_data& data, std::string filename) {
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
        load_image(data, "textures/" + list->get<std::string>(0));
        data.subsprites = size<u16>(list->get<int>(1), list->get<int>(2));
        data.z_index = list->get<int>(3);
        set_texture_data(tex, data);
    }
}