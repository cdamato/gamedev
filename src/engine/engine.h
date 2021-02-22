#ifndef ENGINE_H
#define ENGINE_H

#include <ecs/ecs.h>
#include "renderer.h"
#include <unordered_map>
#include <memory>

class engine;

class ui_manager {
public:
	entity root = 65535;
	entity focus = 65535;
	entity cursor = 65535;
	entity active_interact = 65535;

	size<u16> window_size;
	point<f32> last_position;

	timer hover_timer;
	bool hover_active = false;

	bool handle_button(event& event, engine&);
    bool handle_keypress(event& event, engine&);
    bool handle_cursor(event& event, engine&);
    bool handle_hover(event& event, engine&);
};


enum window_flags {
	fullscreen,
	use_software_render,
	bordered_window,
	vsync,
};

class settings_manager {
public:
	settings_manager();
	size<u16> resolution;
	std::bitset<8> flags;
	std::unordered_map<u8, command> bindings;

	friend class engine;
};


class window_impl {
public:
    virtual bool poll_events(event&) = 0;
    virtual void swap_buffers(renderer_base&) = 0;
    virtual size<u16> get_drawable_resolution() = 0;
};
class window_manager  : no_copy, no_move {
public:
	window_manager(settings_manager& settings);
	~window_manager();

	void swap_buffers(renderer_base& r) { context->swap_buffers(r); };
	void set_fullscreen(bool);
	void set_vsync(bool);
	void set_resolution(size<u16>);

	bool poll_events(event& e) { return context->poll_events( e); };
private:
	window_impl* context;
	bool fullscreen = false;
};

typedef void (*logic_func)(engine&);
class logic_manager {
public:
    void add(logic_func);
    void remove(logic_func);
private:
    friend class engine;
    std::vector<logic_func> logic;
};

class engine {
public:
    settings_manager settings;
	window_manager window;
	ecs_engine ecs;
	ui_manager ui;
    logic_manager logic;
	//scene_manager?
	std::vector<entity> scene;

	renderer_base& renderer() { return *_renderer.get(); }

	engine();
	void set_resolution(size<u16>);
	bool process_events();
	void render();
	void run_tick();
	bool accept_event(event&);

    std::multiset<subsprite> sprites;

	template <typename f, typename... Args>
	entity create_entity(f func, Args&&... args) {
		entity e = ecs.entities.add_entity();
		std::apply(func, std::tuple_cat(std::forward_as_tuple(e, *this), std::forward_as_tuple(args...)));
		return e;
	};

	void destroy_entity(entity e);
	entity player_id() { return ecs._player_id; }
	entity map_id() { return ecs._map_id; }

	bool in_dungeon = false;
    point<f32> offset = point<f32>(0, 0);
    std::bitset<8> command_states;
private:
    std::unique_ptr<renderer_base> _renderer;
};



void inventory_init(entity, engine&, entity, c_inventory& inv, point<u16>);

#endif //ENGINE_H
