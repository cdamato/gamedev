#ifndef ECS_H
#define ECS_H

///////////////////////////////////////////////////////////////////////////////////////
//     ECS.H - ENTITIES, COMPONENTS, SYSTEMS, THEIR MANAGERS, AND THE ECS ENGINE     //
///////////////////////////////////////////////////////////////////////////////////////

#include <common/graphical_types.h>
#include <common/marked_storage.h>
#include <functional>
#include <vector>
#include <mutex>

class engine;
namespace ecs {

class entity_manager {
public:
	constexpr static auto max_entites = 128;
	entity_manager();
	entity add_entity();
	void mark_entity(entity id);
	std::vector<entity> remove_marked();
private:
	constexpr static u8 bucket_count = 4;
	struct bucket {
		std::mutex mutex;
		std::vector <entity> ids;
	};
	std::array<bucket, 4> buckets;
	std::vector <u32> entity_freelist;
	std::size_t get_thread_id() noexcept;
};

///////////////////////////////////////////////////////////////
//     COMPONENT TYPES, POOLS, AND THE COMPONENT MANAGER     //
///////////////////////////////////////////////////////////////

struct component {
	entity parent = null_entity;
};

///////////////////////////////
//     VISUAL COMPONENTS     //
///////////////////////////////

using sprite_id = u32;

class display : public component {
public:
	rect<f32> get_dimensions();
	sprite_id add_sprite(u16 num_quads, texture* tex, u16 z_index, render_layers layer);
	sprite_data& sprites(size_t index) { return _sprites[index]; }
	std::vector<sprite_data>::iterator begin() { return _sprites.begin(); }
	std::vector<sprite_data>::iterator end() { return _sprites.end(); }
private:
	std::vector<sprite_data> _sprites;
};

struct velocity : public component {
	sprite_coords delta;
};

struct collision : public component {
	enum flags : u8 {
		// damage teams
		ally = 1,
		enemy = 2,
		environment = 4,

		// tilemap collision types
		ground = 64,
		air = 128,
	};
	std::function<void(u32)> on_collide;
	std::vector <sprite_id> disabled_sprites; // Some sprites shouldn't have hitboxes e.g. healthbars

	void set_team_detector(flags);
	u8 get_team_detectors();
	void set_team_signal(flags);
	u8 get_team_signals();
	void set_tilemap_collision(flags);
	u8 get_tilemap_collision();
private:
	std::bitset<8> collision_flags;
};

struct proximity : public component {
	enum shape {
		rectangle,
		elipse
	} shape;

	world_coords origin;
	world_coords radii;
	bool activated = false;
};

///////////////////////////////
//     UNIQUE COMPONENTS     //
///////////////////////////////

struct mapdata : public component {
	struct tile_data {
		std::bitset<8> collision;

		bool is_occupied(u8, u8);
		void set_data(u8, u8);
	};
	tile_data get_tiledata(world_coords);

	std::vector <u8> map;
	world_coords size;
	std::vector <tile_data> tiledata_lookup;
};

struct player : public component {
	bool shoot = false;
	world_coords target;
};

struct inventory : public component {
	struct item {
		u32 ID = 0;
		u32 quantity = 0;
	};
	marked_storage<item, 36> data;
	size <u16> display_size;
};

///////////////////////////////
//     COMBAT COMPONENTS     //
///////////////////////////////

struct weapon_pool : public component {
	struct weapon {
		enum bullet_types {
			standard = 0,
		};
		struct data {
			bullet_types bullet;
			f32 damage = 25;
			u16 cooldown = 750;
		};
		entity ID;
		data stats;
		timer t;
	};
	marked_storage<weapon, 4> weapons;
	u8 current = 0;
};

struct health : public component {
	f32 health = 50;
	f32 max_health = 50;
	bool has_healthbar = false;
};

struct damage : public component {
	entity enemy_ID = 65535;
	int damage;
};

/////////////////////////////////
//     AI/ENEMY Components     //
/////////////////////////////////

struct enemy : public component {	};

/////////////////////////////////
//     UI/EVENT COMPONENTS     //
/////////////////////////////////

struct widget : public component {
	entity parent = 65535;
	bool accepts_textinput = false;
	std::vector <entity> children;

	using activation_action = void (*)(entity, engine&, bool);
	activation_action on_activate = nullptr;
	using navigation_action = void (*)(entity, engine&, u32);
	navigation_action on_navigate = nullptr;
    using hover_action = void (*)(entity, engine&);
    hover_action on_hover = nullptr;
};

struct text : public component {
	struct text_entry {
		u8 quad_index = 0;
		std::string text = "";
		color text_color = color(0, 0, 0, 0);
		bool regen = false;
	};
	u8 sprite_index = 0;
	std::vector <text_entry> text_entries;
};

// Represents a navigable grid of smaller components of the same type. For widgets using this, the first sprite must be the container.
struct selection : public component {
	point<u16> active {65535, 65535};
	point<u16> highlight {65535, 65535};
	size<u16> grid_size {65535, 65535};
    u32 sprite_index;

	u32 num_elements() { return grid_size.x * grid_size.y; }
    u32 active_index() { return active.x + (active.y * grid_size.x); }
    u32 highlight_index() { return highlight.x + (highlight.y * grid_size.x); }
};

struct checkbox : public component {
	std::bitset<8> checked;
	int sprite_index;
};

// Stores data for 4 sliders
struct slider : public component {
	std::array<int, 4> min_values;
	std::array<int, 4> current_values;
	std::array<int, 4> max_values;
	std::array<int, 4> increments;
};

// Stores data for 4 button
struct button : public component {
    std::array<widget::activation_action, 4> callbacks;
    int sprite_index;
};

struct dropdown : public component {
	struct dropdown_data {
		std::vector<std::string> entries;
		int active;
	};
	std::vector<dropdown_data> dropdowns;
	int sprite_index;
};

///////////////////////////////
//     COMPONENT MANAGER     //
///////////////////////////////

static constexpr u32 max_entities = 128;
template<typename T>
using pool = marked_storage<T, max_entities>;
template<typename T>
struct type_tag {};

#define ALL_COMPONENTS(m)\
    m(display) m(collision) m(velocity) m(proximity)\
    m(health) m(damage) m(weapon_pool) \
    m(enemy)\
    m(player) m(inventory) m(mapdata)\
    m(widget) m(selection) m(text) m(checkbox) m(slider) m(button) m(dropdown)\

#define POOL_NAME(T) T ## _pool
#define GENERATE_ACCESS_FUNCTIONS(T) constexpr pool<T>& get_pool(type_tag<T>) { return POOL_NAME(T); }
#define GENERATE_REMOVE_CALLS(T) POOL_NAME(T).remove(e);
#define GENERATE_POOLS(T) pool<T> POOL_NAME(T);

class component_manager {
public:
	ALL_COMPONENTS(GENERATE_ACCESS_FUNCTIONS)
	void remove_all(entity e) { ALL_COMPONENTS(GENERATE_REMOVE_CALLS) }
private:
	ALL_COMPONENTS(GENERATE_POOLS)
};



////////////////////////////////////////////
//          SYSTEMS DECLARATIONS          //
////////////////////////////////////////////

////////////////////////////
//     COMBAT SYSTEMS     //
////////////////////////////

struct s_shooting {
	using bullet_func = std::function<void(std::string, world_coords, world_coords, world_coords, world_coords, collision::flags)>;
	struct bullet {
		world_coords dimensions;
		world_coords speed;
		std::string tex;
	};

	std::vector <bullet> bullet_types;
	bullet_func shoot;

	void run(pool<display>&, pool<weapon_pool>, player&);
};

struct s_health : public texture_generator {
public:
	void run(pool<health>&, pool<damage>&, entity_manager&);
	void update_healthbars(pool<health>&, pool<display>&);
private:
	void set_entry(u32, f32);

	std::array <u32, max_entities> healthbar_sprite_indices = std::array<u32, max_entities>();
	std::array <u32, max_entities> healthbar_atlas_indices = std::array<u32, max_entities>();
};

////////////////////////////
//     VISUAL SYSTEMS     //
////////////////////////////

struct s_text : public texture_generator {
public:
	s_text();
	void run(pool<text>&, pool<display>&);

    screen_coords get_text_size(std::string&);
    size_t character_at_position(std::string text, screen_coords pos);
    screen_coords position_of_character(std::string text, size_t pos);
    int num_characters(std::string text);
    int bytes_of_character(std::string text, int char_index);
    int character_byte_index(std::string text, int char_index);
	std::vector<int> get_numlines(std::string& text);
private:
	void render_line(std::string text, point<u16> pen, color text_color);
	std::string replace_locale_macro(std::string&);
	struct impl;
	impl* data;
};

void system_collison_run(pool<collision>&, pool<display>&, mapdata&);
void system_velocity_run(pool<velocity>&, pool<display>&, int);

// Combine proximity detectors and keypresses to allow us to "interact" with world entities
class s_proxinteract {
public:
	entity active_interact = 65535;
	void run(pool<proximity>&, pool<widget>&, display&);
};

////////////////////////////
//     SYSTEM MANAGER     //
////////////////////////////

struct system_manager {
	s_health health;
	s_shooting shooting;
	s_text text;
	s_proxinteract proxinteract;
};


//////////////////////////////////////////////
//          ECS ENGINE DECLARATION          //
//////////////////////////////////////////////

class ecs_engine {
public:
	template<typename T>
	void remove(entity e) { components.get_pool(type_tag<T>()).remove(e); }

	template<typename T>
	constexpr T& get(entity e) {
		auto& pool = components.get_pool(type_tag<T>());
		if (!pool.exists(e)) throw "bruh";
		return pool.get(e);
	}

	template<typename T>
	constexpr bool exists(entity e) { return components.get_pool(type_tag<T>()).exists(e); }

	template<typename T>
	constexpr T& add(entity e) {
		auto& c = components.get_pool(type_tag<T>()).add(e, T());
		c.parent = e;
		return c;
	}

	template<typename T>
	constexpr pool<T>& pool() { return components.get_pool(type_tag<T>()); }

    system_manager systems;
private:
	entity_manager entities;
	component_manager components;
	entity _player_id = 65535;
	entity _map_id = 65535;
	void run_ecs(int framerate_multiplier);

	friend class ::engine;
};
}
#endif //ECS_H
