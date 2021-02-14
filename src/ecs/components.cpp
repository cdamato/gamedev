#include "components.h"

#include <utility>
#include <vector>
#include <set>



u8 c_collision:: get_team_detectors() {
	return flags.to_ulong() & 0b111;
}
u8 c_collision::get_team_signals() {
	return (flags.to_ulong() & 0b111000) >> 3;
}
u8 c_collision:: get_tilemap_collision() {
	return (flags.to_ulong() & 0b11000000) >> 6;
}


void c_collision::set_team_detector(collision_flags to_set) {
	flags |= std::bitset<8>(to_set);
}
void c_collision::set_team_signal(collision_flags to_set) {
	flags |= std::bitset<8>(to_set << 3);
}
void c_collision:: set_tilemap_collision(collision_flags to_set) {
	flags |= std::bitset<8>(to_set);
}

bool tile_data::is_occupied(u8 quad_index, u8 type) {
	if (type == 0) return false;
	u8 bitmask = type << (quad_index * 2);
	return (collision & std::bitset<8>(bitmask)) == bitmask;
}

void tile_data::set_data(u8 quad_index, u8 type) {
	std::bitset<8> aa(type << (quad_index * 2));
	collision |= aa;
}



#include <cmath>
#define VERTICES_PER_QUAD 4


u32 sprite::gen_subsprite(u16 num_quads, render_layers layer) {
    size_t old_size = _vertices.size();
    _vertices.resize(_vertices.size() + (4 * num_quads));
    _subsprites.push_back(subsprite{null_texture, num_quads, old_size, _vertices.data() + old_size, layer});
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
    // bottom right
    _vertices[index + 2].pos = pos + point<f32>(size.w, size.h);
    //bottom left
    _vertices[index + 3].pos = point<f32>(pos.x, pos.y + size.h);
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
    // bottom right
    _vertices[index + 2].uv = pos + point<f32>(size.w, size.h);;
    //bottom left
    _vertices[index + 3].uv = point<f32>(pos.x, pos.y + size.h);
}