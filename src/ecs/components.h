#ifndef COMPONENTS_H
#define COMPONENTS_H

/**************************************************************************************************/
/*     components.h - component types, pools, and the component manager for the ECS framework     */
/**************************************************************************************************/

#include <common/graphical_types.h>
#include <common/bit_types.h>
#include <common/event.h>
#include <functional>
#include <vector>

namespace ecs {
struct component {
	entity parent = null_entity;
};

/*****************************/
/*     Visual Components     */
/*****************************/

using sprite_id = u32;
class c_display : public component {
public:
	rect<f32> get_dimensions();
	sprite_id add_sprite(u16 num_quads, render_layers layer);
    sprite_data& sprites(size_t index) { return _sprites[index]; }

	std::vector<sprite_data>::iterator begin() { return _sprites.begin(); }
	std::vector<sprite_data>::iterator end() { return _sprites.end(); }
private:
	std::vector<sprite_data> _sprites;
};

struct c_velocity : public component {
	sprite_coords delta;
};

struct c_collision: public component {
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
	std::vector<sprite_id> disabled_sprites; // Some sprites shouldn't have hitboxes e.g. healthbars

	void set_team_detector(flags);
	u8 get_team_detectors();
	void set_team_signal(flags);
	u8 get_team_signals();

	void set_tilemap_collision(flags);
	u8 get_tilemap_collision();
private:
	std::bitset<8> collision_flags;
};

struct c_proximity : public component {
    enum shape {
        rectangle,
        elipse
    } shape;

	world_coords origin;
    world_coords radii;
    bool activated = false;
};

/*****************************/
/*     Unique Components     */
/*****************************/

struct c_mapdata : public component {
	struct tile_data {
		std::bitset<8> collision;

		bool is_occupied(u8, u8);
		void set_data(u8, u8);
	};

	std::vector<u8> map;
	world_coords size;
	std::vector<tile_data> tiledata_lookup;
	tile_data get_tiledata(world_coords);
};

struct c_player : public component {
	bool shoot = false;
	world_coords target;
};

struct c_inventory : public component {
	struct item {
		u32 ID = 0;
		u32 quantity = 0;
	};
	marked_storage<item, 36> data;
	size<u16> display_size;
};

/*****************************/
/*     Combat Components     */
/*****************************/

struct c_weapon_pool : public component {
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

struct c_health : public component {
	f32 health = 50;
	f32 max_health = 50;
	bool has_healthbar = false;
};

struct c_damage : public component {
	entity enemy_ID = 65535;
	int damage;
};

/*******************************/
/*     AI/Enemy Components     */
/*******************************/

struct c_enemy : public component {
};

/*******************************/
/*     UI/Event Components     */
/*******************************/

struct c_widget : public component {
	entity parent = 65535;
	std::vector<entity> children;
};

struct c_text : public component {
	u8 sprite_index = 0;
	std::string text;
};

struct c_selection : public component {
	point<u16> active {};
	size<u16> grid_size {};
	int box_index = 65535; // Can be used to help display selection status
	entity active_index() { return active.x + (active.y * grid_size.x); }
};

// A basic callback wrapper for UI event components
template <int h>
class event_wrapper : public component {
    std::function<bool(event&)> handler;
    bool active = false;
public:
    template <typename func_type, typename... Args>
    void add_event(func_type func, Args&&... args){
        handler = [&args..., *this, func = std::move(func)] (const event& obj) {
            return func(parent, obj, args...);
        };
    }
    bool run_event(event& e) {	return handler(e); }
};

using c_keyevent = event_wrapper<event_keypress::id>;
using c_mouseevent = event_wrapper<event_mousebutton::id>;
using c_cursorevent = event_wrapper<event_cursor::id>;
using c_hoverevent = event_wrapper<3>;

/*****************************/
/*     Component Manager     */
/*****************************/

static constexpr u32 max_entities = 128;

template <typename T>
using pool = marked_storage<T, max_entities>;

template <typename T>
struct type_tag {};

// I REALLY don't like using C macros here, but C++ lacks decent macros or metaprogramming.
// If there's a better solution, let me know.

#define ALL_COMPONENTS(m)\
	m(c_display) m(c_collision) m(c_velocity) m(c_proximity)\
	m(c_health) m(c_damage) m(c_weapon_pool) \
    m(c_enemy)\
	m(c_player) m(c_inventory) m(c_mapdata)\
	m(c_widget) m(c_selection) m(c_text)\
	m(c_mouseevent) m(c_cursorevent) m(c_keyevent) m(c_hoverevent)

#define POOL_NAME(T) T ## _pool
#define GENERATE_ACCESS_FUNCTIONS(T) constexpr pool<T>& get_pool(type_tag<T>) { return POOL_NAME(T); }
#define GENERATE_REMOVE_CALLS(T) POOL_NAME(T).remove(e);
#define GENERATE_POOLS(T) pool<T> POOL_NAME(T);

class component_manager {
public:
	ALL_COMPONENTS(GENERATE_ACCESS_FUNCTIONS)

	void remove_all(entity e) {
		ALL_COMPONENTS(GENERATE_REMOVE_CALLS)
	}

private:
	ALL_COMPONENTS(GENERATE_POOLS)
};
}
#endif //COMPONENTS_H
