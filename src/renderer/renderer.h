#ifndef RENDERER_H
#define RENDERER_H

#include <set>
#include <unordered_map>
#include <common/graphical_types.h>
#include <string>

class vertex;
class sprite;
struct subsprite {
    const vertex* data;
    u32 size;
    texture tex;

    inline bool operator < (const subsprite& rhs ) const { return tex < rhs.tex; }
};


class renderer_base  : no_copy, no_move {
public:

    virtual void set_viewport(size<u16>) = 0;
    virtual void set_camera(point<f32>) = 0;
    virtual void clear_screen() = 0;
    virtual texture add_texture(std::string name) = 0;

    void clear_sprites();
    void add_sprite(sprite&);
    void render_layer(render_layers layer);

    texture get_texture(std::string name);
    void set_texture_data(texture, texture_data);
protected:
    virtual void process_texture_data(texture) = 0;
    virtual void render_batch(render_layers) = 0;
    void load_textures();

    std::multiset<subsprite>& get_set(render_layers layer);

    std::multiset<subsprite> sprites;
    std::multiset<subsprite> ui;
    std::multiset<subsprite> text;

    std::vector<texture_data> texture_datas;
    std::unordered_map<std::string, texture> texture_map;
    vertex* mapped_vertex_buffer = nullptr;
    texture current_tex = null_texture;
    size_t quads_batched = 0;
};

class renderer_gl : public renderer_base {
public:
    void clear_screen();
    texture add_texture(std::string name) ;
    renderer_gl();
    void set_viewport(size<u16>);
    void set_camera(point<f32>);
private:
    void process_texture_data(texture);
    struct impl;
    impl* data = nullptr;

    void render_batch(render_layers);
};

    void set_texture_data(texture, std::vector<u8>&, size<u16>, bool = false);

class renderer_software : public renderer_base {
public:
    void clear_screen();
    texture add_texture(std::string name) ;
    renderer_software();
    void set_viewport(size<u16>);
    void set_camera(point<f32>);

private:
    void process_image_data(texture) {}
    image framebuffer;

    size<u16> resolution;
    point<f32> camera;

    size_t texture_counter = 0;
    void render_batch(render_layers);
};

#endif //RENDERER_H