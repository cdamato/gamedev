#ifndef ENGINE_H
#define ENGINE_H

#include <ecs/ecs.h>
#include <renderer/renderer.h>
#include <unordered_map>

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


class window_impl;
class window_manager  : no_copy, no_move {
public:
	window_manager(settings_manager& settings);
	~window_manager();

	void swap_buffers();
	void set_fullscreen(bool);
	void set_vsync(bool);
	void set_resolution(size<u16>);

	bool poll_events(event&);
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
    renderer_software renderer;
	ecs_engine ecs;
	ui_manager ui;
    logic_manager logic;
	//scene_manager?
	std::vector<entity> scene;

	engine();
	void set_resolution(size<u16>);
	bool process_events();
	void render();
	void run_tick();
	bool accept_event(event&);

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
};



void inventory_init(entity, engine&, entity, c_inventory& inv, point<u16>);

#endif //ENGINE_H
