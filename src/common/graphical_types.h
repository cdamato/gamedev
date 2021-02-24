#ifndef GRAPHICAL_TYPES_H
#define GRAPHICAL_TYPES_H

#include "basic_types.h"
#include <vector>

using entity = u32;
using texture = u32;
constexpr static entity null_entity = 65535;
constexpr static texture null_texture = 65535;


struct color {
    u8 r, g, b, a;
    color() = default;
    color(u8 r_in, u8 g_in, u8 b_in, u8 a_in) : r(r_in), g(g_in), b(b_in), a(a_in) {}
};

template <typename T>
struct image_wrapper {
public:
    image_wrapper() = default;
    image_wrapper(T data_in, size<u16> size_in) : _data(data_in), _size(size_in)
    {

    }
    color get(size_t i) {
        if (i * 4 >= _data.size()) throw "bruh";
        return color{_data[i * 4], _data[i * 4 + 1], _data[i * 4 + 2], _data[i * 4 + 3]};
    }
    void write(size_t i, color c) {
        if (i * 4 >= _data.size()) throw "bruh";
        _data[i * 4] = c.r;
        _data[i * 4 + 1] = c.g;
        _data[i * 4 + 2] = c.b;
        _data[i * 4 + 3] = c.a;
    }
    T& data() { return _data; }
    ::size<u16> size() { return _size; }
private:
    T _data;
    ::size <u16> _size;
};

using image = image_wrapper<std::vector<u8>>;
using framebuffer = image_wrapper<u8*>;

struct vertex {
    point<f32> pos;
    point<f32> uv;
};

struct texture_data {
    f32 z_index;
    image image_data;
    size<u16> subsprites;
    int scale_factor;
    bool is_greyscale = false;
};

// You can set render layer to "null" to prevent display of a sprite.
enum class render_layers : u8 {
    null,
    sprites,
    text,
    ui
};

struct subsprite {
    texture tex = null_texture;
    u16 size = 0;
    u16 start = 0;
    vertex* data;
    render_layers layer;

    inline bool operator < (const subsprite& rhs ) const { return tex < rhs.tex; }
};


static constexpr unsigned vertices_per_quad = 4;

#endif //GRAPHICAL_TYPES_H