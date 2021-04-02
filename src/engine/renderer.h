#ifndef RENDERER_H
#define RENDERER_H

#include <set>
#include <unordered_map>
#include <string>
#include <common/graphical_types.h>


class renderer_base : no_copy, no_move {
public:
    virtual ~renderer_base() = default;
    virtual void set_viewport(screen_coords) = 0;
    virtual void set_camera(vec2d<f32>) = 0;
    virtual void clear_screen() = 0;
    virtual texture add_texture(std::string name) = 0;

    void clear_sprites();
    void add_sprite(sprite_data&);
    void render_layer(std::multiset<sprite_data>);

    texture get_texture(std::string name);
    void set_texture_data(texture, texture_data);

    static constexpr unsigned quads_in_buffer = 1024;
    static constexpr unsigned buffer_size = quads_in_buffer * vertices_per_quad * sizeof(vertex);
protected:
    std::vector<texture_data> textures;
    std::unordered_map<std::string, texture> texture_map;
    vertex* mapped_vertex_buffer = nullptr;
    size_t quads_batched = 0;

    virtual void process_texture_data(texture) = 0;
    virtual void render_batch(texture, render_layers) = 0;
    void load_textures();
};

class renderer_gl : public renderer_base {
public:
    void clear_screen();
    texture add_texture(std::string name) ;
    renderer_gl();
    void set_viewport(screen_coords);
    void set_camera(vec2d<f32>);
private:
    // PIMPL to avoid bringing shader class into the header.
    struct impl;
    impl* data = nullptr;

    void render_batch(texture, render_layers);
    void process_texture_data(texture);
};

// This is quite buggy.
class renderer_software : public renderer_base {
public:
    void clear_screen();
    texture add_texture(std::string name) ;
    renderer_software(screen_coords);
    void set_viewport(screen_coords);
    void set_camera(vec2d<f32>);
    framebuffer& get_framebuffer() { return fb;}
private:
    framebuffer fb;
    vec2d<f32> camera;
    size_t texture_counter = 0;

    void render_batch(texture, render_layers);
    void process_texture_data(texture);
};

#endif //RENDERER_H