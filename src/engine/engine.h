#ifndef ENGINE_H
#define ENGINE_H

#include <ecs/ecs.h>
#include <display/display_impl.h>
#include <unordered_map>
#include <memory>

class engine;

class ui_manager {
public:
	entity root = 65535;
	entity focus = 65535;
	entity cursor = 65535;
	entity active_interact = 65535;

	screen_coords window_size;
	screen_coords last_position;

	timer hover_timer;
	bool hover_active = false;
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
	screen_coords resolution;
	std::bitset<8> flags;
	std::unordered_map<u8, command> bindings;
	int framerate_multiplier = 2;
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
	ecs::ecs_engine ecs;
	ui_manager ui;
    logic_manager logic;
    display::texture_manager& textures() { return display.textures(); }
    display::window_impl& window() { return display.get_window(); }

	engine();
	bool process_events();
	void render();
	void run_tick();

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
	world_coords offset = world_coords(0, 0);
    std::bitset<8> command_states;
private:
    display::renderer& renderer() { return display.get_renderer(); }
    display::display_manager display;
};

void remove_child(ecs::c_widget& w, entity b);
bool process_event(event& ev, engine&);


#endif //ENGINE_H
