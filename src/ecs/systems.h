#ifndef SYSTEM_H
#define SYSTEM_H

#include "components.h"

class sprite;
class entity_manager;


class texture_generator {
public:
    void set_texture(texture in) { tex = in; }
    texture get_texture() { return tex; }
    texture_data& get_data() { return data;  }
protected:
    texture_data data;
    size_t last_atlassize = 0;
    texture tex;
    bool regenerate = true;
};

struct s_shooting {
	using bullet_func = std::function<void(std::string, size<f32>, point<f32>, point<f32>, point<f32>, collision_flags)>;
	struct bullet {
		size<f32> dimensions;
		point<f32> speed;
		std::string tex;
	};

	std::vector<bullet> bullet_types;
	bullet_func shoot;
};


struct s_health : public texture_generator {
public:
	void run(pool<c_health>&, pool<c_damage>&, pool<c_healthbar>&, entity_manager&);
	void update_healthbars(pool<c_healthbar>&, pool<c_health>&, pool<sprite>&);
private:
	void set_entry(u32, f32);
};

struct s_text {
public:
	texture atlas;
	s_text();

	void run(pool<c_text>&, pool<sprite>&);
	std::vector<u8> get_texdata() { return texture_data; }
private:
    struct impl;
    std::vector<u8> texture_data;
	impl* data;
	size_t last_atlassize = 0;
	bool reconstruct_atlas = true;
};


void shooting_player(s_shooting&, sprite&, c_player&, c_weapon_pool&);
void system_collison_run(pool<c_collision>&, pool<sprite>&, c_mapdata&);
void system_velocity_run(pool<c_velocity>&, pool<sprite>&);

// Combine proximity detectors and keypresses to allow us to "interact" with world entities
class s_proxinteract {
public:
    entity active_interact = 65535;
    void run(pool<c_proximity>&, pool <c_keyevent>&, sprite&);
};
//void run_proximity(s_proximity& sys, c_proximity* c);
//oid run_player_interact(s_proximity& sys, sprite*, c_player*);
//static void finish_proximity(s_proximity& sys) { sys.interactables.clear(); }



/*
void run_proximity(s_proximity& sys, c_proximity* c) {
    if (c->type == c_proximity::type::interactable)
        sys.interactables.emplace_back(std::reference_wrapper(c));
    else if (c->type == c_proximity::weapon)
        sys.weapons.emplace_back(std::reference_wrapper(c));
}

bool test_proximity_collision(c_proximity* c, sprite* spr) {
    if(c->shape == c_proximity::shape::rectangle)
        return test_collision(rect<f32>(c->origin, c->radii), spr->dimensions());
    if(c->shape == c_proximity::shape::elipse) {
        point<f32> p = spr->dimensions().center();
        f32 left_side = pow(p.x - c->origin.x, 2) / pow(c->radii.w, 2);
        f32 right_side = pow(p.y - c->origin.y, 2) / pow(c->radii.h, 2);

        return left_side + right_side <= 1;
    }

    return false;
}

void run_player_interact(s_proximity& sys, sprite* s, c_player* p) {
    std::vector<f32> scores(sys.interactables.size(), 65535);

    for(size_t i = 0; i < sys.interactables.size(); i++) {
        c_proximity* c = sys.interactables[i];

        //if (test_proximity_collision(c, s))
        //scores[i] = distance(c.origin, s.dimensions.origin);
    }

    f32 min = 100;
    size_t min_index = 65535;
    for(size_t i = 0; i < scores.size(); i++) {
        if(scores[i] < min) {
            min = scores[i];
            min_index = i;
        }
    }

    if(min_index != 65535)
        sys.active_interact = sys.interactables[min_index]->entity;
}

*/



struct system_manager {
	s_health health;
	s_shooting shooting;
	s_text text;
    s_proxinteract proxinteract;
};

#endif //SYSTEM_H
