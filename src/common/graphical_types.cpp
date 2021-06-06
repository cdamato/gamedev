#include "graphical_types.h"
#include <cmath>
#define VERTICES_PER_QUAD 4

// Rotate sprite by an angle in radians. For proper rotation, origin needs to be in the center of the object.
void sprite_data::rotate(f32 theta) {
    f32 s = sin(theta);
    f32 c = cos(theta);
    rect<f32> dimensions = get_dimensions();
    sprite_coords center = dimensions.center();
    for (auto& vert : _vertices) {
        f32 i = vert.pos.x - center.x;
        f32 j = vert.pos.y - center.y;
        vert.pos.x = ((i * c) - (j * s)) + center.x;
        vert.pos.y = ((i * s) + (j * c)) + center.y;
    }
}

void sprite_data::set_pos(sprite_coords pos, sprite_coords size, size_t quad) {
    // Vertices are in order top left, top right, bottom right, and bottom left
    size_t index = (quad) * VERTICES_PER_QUAD;
    _vertices[index].pos = pos;
    _vertices[index + 1].pos = sprite_coords(pos.x + size.x, pos.y);
    _vertices[index + 2].pos = pos + sprite_coords(size.x, size.y);
    _vertices[index + 3].pos = sprite_coords(pos.x, pos.y + size.y);
}

void sprite_data::set_uv(point<f32> pos, size<f32> size, size_t quad) {
    // Vertices are in order top left, top right, bottom right, and bottom left
    size_t index = (quad) * VERTICES_PER_QUAD;
    _vertices[index].uv = pos;
    _vertices[index + 1].uv = point<f32>(pos.x + size.x, pos.y);
    _vertices[index + 2].uv = pos + point<f32>(size.x, size.y);
    _vertices[index + 3].uv = point<f32>(pos.x, pos.y + size.y);
}

void sprite_data::set_tex_region(size_t tex_index, size_t quad) {
    ::size<f32> region_size(1.0f / tex->regions.x, 1.0f / tex->regions.y);
    ::size<f32> pos = region_size * ::size<f32>(tex_index % tex->regions.x, tex_index / tex->regions.x);
    // Vertices are in order top left, top right, bottom right, and bottom left
    size_t index = (quad) * VERTICES_PER_QUAD;
    _vertices[index].uv = pos;
    _vertices[index + 1].uv = point<f32>(pos.x + region_size.x, pos.y);
    _vertices[index + 2].uv = pos + point<f32>(region_size.x, region_size.y);
    _vertices[index + 3].uv = point<f32>(pos.x, pos.y + region_size.y);
}

void sprite_data::move_to(sprite_coords pos) {
    sprite_coords change = pos - _vertices[0].pos;
    move_by(change);
}

void sprite_data::move_by(sprite_coords change) {
    for (auto& vert : _vertices) {
        vert.pos += change;
    }
}


rect<f32> sprite_data::get_dimensions(u8 quad_index) {
    sprite_coords min(65535, 65535);
    sprite_coords max(0, 0);

    int start = 0;
    int finish = _vertices.size();
    if (quad_index != 255) {
        start = quad_index * 4;
        finish = start + 4;
    }

    for (int i = start; i < finish; i++) {
        auto& vertex = _vertices[i];
        min.x = std::min(min.x, vertex.pos.x);
        min.y = std::min(min.y, vertex.pos.y);

        max.x = std::max(max.x, vertex.pos.x);
        max.y = std::max(max.y, vertex.pos.y);
    }

    return rect<f32>(min, max - min);
}