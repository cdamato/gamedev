#ifndef SYSTEM_H
#define SYSTEM_H

#include "components.h"

class sprite;
class entity_manager;


class texture_generator {
public:
	void set_texture(texture* in) {
		tex = in;
		regenerate = true;
	}
	texture* get_texture() { return tex; }
protected:
	size_t last_atlassize = 0;
	texture* tex;
	bool regenerate = true;
};

struct s_shooting {
	using bullet_func = std::function<void(std::string, world_coords, world_coords, world_coords, world_coords, c_collision::flags)>;
	struct bullet {
		world_coords dimensions;
		world_coords speed;
		std::string tex;
	};

	std::vector<bullet> bullet_types;
	bullet_func shoot;
	void run(pool<c_display>&, pool<c_weapon_pool>, c_player&);
};


struct s_health : public texture_generator {
public:
	void run(pool<c_health>&, pool<c_damage>&, entity_manager&);
	void update_healthbars(pool<c_health>&, pool<c_display>&);
private:
	void set_entry(u32, f32);

	std::array<u32, max_entities> healthbar_sprite_indices = std::array<u32, max_entities>();
	std::array<u32, max_entities> healthbar_atlas_indices = std::array<u32, max_entities>();
};

struct s_text {
public:
	texture atlas;
	s_text();

	void run(pool<c_text>&, pool<c_display>&);
	std::vector<u8> get_texdata() { return texture_data; }
private:
    struct impl;
    std::vector<u8> texture_data;
	impl* data;
	size_t last_atlassize = 0;
	bool reconstruct_atlas = true;
};

void system_collison_run(pool<c_collision>&, pool<c_display>&, c_mapdata&);
void system_velocity_run(pool<c_velocity>&, pool<c_display>&, int);

// Combine proximity detectors and keypresses to allow us to "interact" with world entities
class s_proxinteract {
public:
    entity active_interact = 65535;
    void run(pool<c_proximity>&, pool <c_keyevent>&, c_display&);
};

struct system_manager {
	s_health health;
	s_shooting shooting;
	s_text text;
    s_proxinteract proxinteract;
};

#endif //SYSTEM_H
