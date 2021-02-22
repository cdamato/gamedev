#ifndef RENDERER_H
#define RENDERER_H

#include <set>
#include <unordered_map>
#include <string>
#include <common/graphical_types.h>


class renderer_base  : no_copy, no_move {
public:
    virtual void set_viewport(size<u16>) = 0;
    virtual void set_camera(point<f32>) = 0;
    virtual void clear_screen() = 0;
    virtual texture add_texture(std::string name) = 0;

    void clear_sprites();
    void add_sprite(subsprite&);
    void render_layer(std::multiset<subsprite>);

    texture get_texture(std::string name);
    void set_texture_data(texture, texture_data);

    static constexpr unsigned quads_in_buffer = 1024;
    static constexpr unsigned buffer_size = quads_in_buffer * vertices_per_quad * sizeof(vertex);
protected:
    virtual void process_texture_data(texture) = 0;
    virtual void render_batch(texture, render_layers) = 0;
    void load_textures();

    std::vector<texture_data> texture_datas;
    std::unordered_map<std::string, texture> texture_map;
    
    vertex* mapped_vertex_buffer = nullptr;
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

    void render_batch(texture, render_layers);
};

// This is quite buggy and also has performance issues - even on a modern Ryzen CPU, it's hard to get 60fps.
class renderer_software : public renderer_base {
public:
    void clear_screen();
    texture add_texture(std::string name) ;
    renderer_software();
    void set_viewport(size<u16>);
    void set_camera(point<f32>);
    image& get_framebuffer() { return framebuffer;}
private:
    void process_texture_data(texture) {}

    image framebuffer;

    size<u16> resolution;
    point<f32> camera;

    size_t texture_counter = 0;
    void render_batch(texture, render_layers);
};

#endif //RENDERER_H