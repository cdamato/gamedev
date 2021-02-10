#ifndef RENDERER_H
#define RENDERER_H

#include <set>
#include <unordered_map>
#include <ecs/ecs.h>
struct subsprite {
    const vertex* data;
    u32 size;
    texture* tex;

    inline bool operator < (const subsprite& rhs ) const { return tex < rhs.tex; }
};


class renderer_base  : no_copy, no_move {
public:

    virtual void set_viewport(size<u16>) = 0;
    virtual void set_camera(point<f32>) = 0;
    virtual void clear_screen() = 0;
    virtual texture* add_texture(std::string name) = 0;
    virtual void set_texture_data(texture*, std::vector<u8>&, size<u16>, bool = false) = 0;

    void clear_sprites();
    void add_sprite(sprite&);
    void render_layer(render_layers layer);

    texture* get_texture(std::string name);
    void set_texture_data(texture*, std::string);
protected:
    void load_textures();
    virtual void render_batch(render_layers) = 0;

    std::multiset<subsprite>& get_set(render_layers layer);

    std::multiset<subsprite> sprites;
    std::multiset<subsprite> ui;
    std::multiset<subsprite> text;

    std::unordered_map<std::string, texture> texture_map;
    vertex* mapped_vertex_buffer = nullptr;
    texture* current_tex = nullptr;
    size_t quads_batched = 0;
};

class renderer_gl : public renderer_base {
public:
    void clear_screen();
    texture* add_texture(std::string name) ;
    renderer_gl();
    void set_viewport(size<u16>);
    void set_camera(point<f32>);
    void set_texture_data(texture*, std::vector<u8>&, size<u16>, bool = false);
private:
    struct impl;
    impl* data = nullptr;

    void render_batch(render_layers);
};


struct color {
    u8 r, g, b, a;
};
struct image {
    ::size <u16> size;
    std::vector <u8> data;

    color get(size_t i) { return color{data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]}; }
    void write(size_t i, color c) {
        data[i * 4] = c.r;
        data[i * 4 + 1] = c.g;
        data[i * 4 + 2] = c.b;
        data[i * 4 + 3] = c.a;
    }
};

class renderer_software : public renderer_base {
public:
    void clear_screen();
    texture* add_texture(std::string name) ;
    renderer_software();
    void set_viewport(size<u16>);
    void set_camera(point<f32>);
    void set_texture_data(texture*, std::vector<u8>&, size<u16>, bool = false);
private:
    std::vector<image> textures;
    image framebuffer;

    size<u16> resolution;
    point<f32> camera;

    size_t texture_counter = 0;
    void render_batch(render_layers);
};

#endif //RENDERER_H