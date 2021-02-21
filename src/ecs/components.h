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


struct component_base {
	entity ref = 65535;
};

/*******************************/
/*     Non-Component Types     */
/*******************************/


struct tile_data {
	std::bitset<8> collision;

	bool is_occupied(u8, u8);
	void set_data(u8, u8);
};
enum collision_flags : u8 {
	// damage teams
	ally = 1,
	enemy = 2,
	environment = 4,

	// tilemap collision types
	ground = 64,
	air = 128,
};

// An item, as represented in the inventory.
struct item {
	u32 ID = 0;
	u32 quantity = 0;
};

// A basic callback wrapper for UI event components
template <int h>
class event_wrapper : public component_base {
	std::function<bool(event&)> handler;
	bool active = false;
public:
	template <typename func_type, typename... Args>
	void add_event(func_type func, Args&&... args){
		handler = [&args..., *this, func = std::move(func)] (const event& obj) {
			return func(ref, obj, args...);
		};
	}
	bool run_event(event& e) {	return handler(e); }
};



enum bullet_types {
    standard = 0,
};

struct weapon {
    struct data {
        bullet_types bullet;
        f32 damage = 25;
        u16 cooldown = 750;
    };
    entity ID;
    data stats;
    timer t;
};
/*****************************/
/*     Visual Components     */
/*****************************/

class sprite : public component_base {
public:
	rect<f32> get_dimensions(u8 subsprite = 255);
	u32 gen_subsprite(u16 num_quads, render_layers layer);

	subsprite& get_subsprite(size_t index) { return _subsprites[index]; }
	u32 num_subsprites() {return _subsprites.size(); }
	const std::vector<vertex>& vertices() { return _vertices; };

	void rotate(f32 theta);
	void move_by(point<f32>);
	void move_to(point<f32>);

	void set_uv(point<f32> pos, size<f32> size, size_t, size_t start);
	void set_pos(point<f32>, size<f32>, size_t, size_t);

	void update();

private:
	std::vector<vertex> _vertices {};
	std::vector<subsprite> _subsprites;
};

struct c_velocity : public component_base {
	point<f32> delta;
};

struct c_collision: public component_base  {
	std::function<void(u32)> on_collide;
	std::vector<u8> disabled_subsprites; // Some sprites shouldn't have hitboxes e.g. healthbars

	void set_team_detector(collision_flags);
	u8 get_team_detectors();
	void set_team_signal(collision_flags);
	u8 get_team_signals();

	void set_tilemap_collision(collision_flags);
	u8 get_tilemap_collision();
private:
	std::bitset<8> flags;
};

struct c_proximity : public component_base {
    enum shape {
        rectangle,
        elipse
    } shape;

    point<f32> origin;
    size<f32> radii;
};

/*****************************/
/*     Unique Components     */
/*****************************/

struct c_mapdata : public component_base {
	std::vector<u8> map;
	::size<u16> size;
	std::vector<tile_data> tiledata_lookup;
	tile_data get_tiledata(point<f32>);
};

struct c_player : public component_base {
	bool shoot = false;
	point<f32> target;
};

struct c_inventory : public component_base {
	marked_storage<item, 36> data;
	size<u16> display_size;
};

/*****************************/
/*     Combat Components     */
/*****************************/

struct c_weapon_pool : public component_base {
	marked_storage<weapon, 4> weapons;
	u8 current = 0;
};

struct c_health : public component_base {
	f32 health = 50;
	f32 max_health = 50;
};

struct c_healthbar : public component_base {
	u8 subsprite_index;
	size_t tex_index;
	bool update;
};

struct c_damage : public component_base {
	entity enemy_ID = 65535;
	int damage;
};

/*******************************/
/*     AI/Enemy Components     */
/*******************************/

struct c_enemy : public component_base {
};

/*******************************/
/*     UI/Event Components     */
/*******************************/

struct c_widget : public component_base {
	entity parent = 65535;
	std::vector<entity> children;
};

struct c_text : public component_base {
	u8 subsprite_index = 0;
	std::string text;
};

struct c_selection : public component_base {
	point<u16> active {};
	size<u16> grid_size {};
	int box_index = 65535; // Can be used to help display selection status
	entity active_index() { return active.x + (active.y * grid_size.w); }
};

using c_mouseevent = event_wrapper<0>;
using c_cursorevent = event_wrapper<1>;
using c_keyevent = event_wrapper<2>;
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
	m(sprite) m(c_collision) m(c_velocity) m(c_proximity)\
	m(c_health) m(c_damage) m(c_healthbar) m(c_weapon_pool) \
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

#endif //COMPONENTS_H
