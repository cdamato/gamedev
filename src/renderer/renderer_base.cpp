#include "renderer.h"
#include <cstring>

#include <cmath>
#include <common/png.h>
#include <common/parser.h>

static constexpr unsigned VERTICES_PER_QUAD = 4;
static constexpr unsigned NUM_QUADS = 1024;
static constexpr unsigned buffer_size = NUM_QUADS * VERTICES_PER_QUAD * sizeof(vertex);


std::multiset<subsprite>& renderer_base::get_set(render_layers layer) {
    if (layer == render_layers::sprites)
        return sprites;
    if (layer == render_layers::ui)
        return ui;
    if (layer == render_layers::text)
        return text;
    throw "oops";
}


void renderer_base::add_sprite(sprite& spr) {
    const vertex* ptr = spr.vertices().data();
    for (u32 i = 0; i < spr.num_subsprites(); i++) {
        render_layers layer = spr.get_subsprite(i).layer;
        if (layer != render_layers::null) {
            get_set(layer).insert(subsprite{ptr, spr.get_subsprite(i).size, spr.get_subsprite(i).tex});
        }
        ptr += spr.get_subsprite(i).size * VERTICES_PER_QUAD;
    }
}

void renderer_base::clear_sprites() {
    sprites.clear();
    text.clear();
    ui.clear();
}

void renderer_base::render_layer(render_layers layer) {
    auto& spriteset = get_set(layer);
    if (spriteset.size() == 0 ) return;

    current_tex = (*spriteset.begin()).tex;
    u32 dest_index = 0;


    for(auto sprite : spriteset) {
        if (sprite.tex != current_tex) {
            dest_index = 0;
            render_batch(layer);
            current_tex = sprite.tex;
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
                dest_index = 0;
                current_tex = sprite.tex;
                render_batch(layer);
            }
        }
    }
    render_batch(layer);
}



texture* renderer_base::get_texture(std::string name) {
    return &texture_map[name];
}



void renderer_base::set_texture_data(texture* tex, std::string filename){
    std::vector<unsigned char> pixels;
    unsigned int width;
    unsigned int height;
    auto error = lodepng::decode(pixels, width, height, filename);

    if (error)
        printf("Error Loading texture: %s\n", lodepng_error_text(error));
    // With the file's data in hand, do the actual setting
    set_texture_data(tex, pixels, size<u16>(width, height), false);
}



void renderer_base::load_textures() {
    config_parser p("config/textures.txt");
    auto d = p.parse();
    for (auto it = d->begin(); it != d->end(); it++) {
        const config_list* list = dynamic_cast<const config_list*>((&it->second)->get());
        texture* tex = add_texture(it->first);
        set_texture_data(tex, "textures/" + list->get<std::string>(0));
        tex->z_index =  list->get<int>(3);
    }
}






u32 sprite::gen_subsprite(u16 num_quads, render_layers layer) {
    _subsprites.push_back(subsprite{nullptr, num_quads, _vertices.size() / 4, layer});
    _vertices.resize(_vertices.size() + (4 * num_quads));
    return _subsprites.size() - 1;
}

// Rotate sprite by a given degree in angles, measured in radians.
// For proper rotation, origin needs to be in the center of the object.
void sprite::rotate(f32 theta)
{
    f32 s = sin(theta);
    f32 c = cos(theta);

    rect<f32> dimensions = get_dimensions();
    point<f32> center = dimensions.center();

    for (auto& vert : _vertices) {

        f32 i = vert.pos.x - center.x;
        f32 j = vert.pos.y - center.y;

        vert.pos.x = ((i * c) - (j * s)) + center.x;
        vert.pos.y = ((i * s) + (j * c)) + center.y;
    }
}


void sprite::set_pos(point<f32> pos, size<f32> size, size_t subsprite, size_t quad)
{
    size_t index = (_subsprites[subsprite].start + quad) * VERTICES_PER_QUAD;
    // top left corner
    _vertices[index].pos = pos;
    // top right
    _vertices[index + 1].pos = point<f32>(pos.x + size.w, pos.y);
    //bottom left
    _vertices[index + 2].pos = point<f32>(pos.x, pos.y + size.h);
    // bottom right
    _vertices[index + 3].pos = pos + point<f32>(size.w, size.h);
}


void sprite::move_to(point<f32> pos) {
    point<f32> change = pos - _vertices[0].pos;
    for (auto& vert : _vertices) {
        vert.pos += change;
    }
}
void sprite::move_by(point<f32> change) {
    for (auto& vert : _vertices) {
        vert.pos += change;
    }
}
rect<f32> sprite::get_dimensions(u8 subsprite) {

    size_t vert_begin = 0;
    size_t vert_end = _vertices.size();

    if (subsprite != 255) {
        vert_begin = _subsprites[subsprite].start * 4;
        vert_end = vert_begin + (_subsprites[subsprite].size * 4);
    }

    point<f32> min(65535, 65535);
    point<f32> max(0, 0);

    for (size_t i = vert_begin; i < vert_end ; i++) {
        auto vertex = _vertices[i];
        min.x = std::min(min.x, vertex.pos.x);
        min.y = std::min(min.y, vertex.pos.y);

        max.x = std::max(max.x, vertex.pos.x);
        max.y = std::max(max.y, vertex.pos.y);
    }

    point<f32> s = max - min;
    return rect<f32>(min, size<f32>(s.x, s.y));
}

void sprite::set_uv(point<f32> pos, size<f32> size, size_t subsprite, size_t quad)
{
    size_t index = (_subsprites[subsprite].start + quad) * VERTICES_PER_QUAD;
    // top left corner
    _vertices[index].uv = pos;
    // top right
    _vertices[index + 1].uv = point<f32>(pos.x + size.w, pos.y);
    //bottom left
    _vertices[index + 2].uv = point<f32>(pos.x, pos.y + size.h);
    // bottom right
    _vertices[index + 3].uv = pos + point<f32>(size.w, size.h);;
}
