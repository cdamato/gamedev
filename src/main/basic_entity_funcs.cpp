#include "basic_entity_funcs.h"

#include <engine/engine.h>
#include <random>
#include <math.h>




void egen_bullet(entity e, engine& game, std::string texname,
				 world_coords dimensions, world_coords speed, world_coords source, world_coords dest, ecs::c_collision::flags team)
{

	ecs::c_display& spr = game.ecs.add<ecs::c_display>(e);
	spr.add_sprite(1, render_layers::sprites);
	spr.sprites(0).tex = game.textures().get(texname.c_str());
	//spr._dimensions = rect<f32>(source, dimensions);
	spr.sprites(0).set_pos(source, dimensions, 0);
	spr.sprites(0).set_tex_region(0, 0);
	spr.sprites(0).rotate(atan2(dest.x - source.x, (source.y - dest.y)));



	ecs::c_damage& o = game.ecs.add<ecs::c_damage>(e);
	o.damage = 25;

	ecs::c_collision& c = game.ecs.add<ecs::c_collision>(e);
	c.set_team_signal(team);
	c.set_tilemap_collision(ecs::c_collision::flags::ground);
	c.set_tilemap_collision(ecs::c_collision::flags::air);
	//c.set_tilemap_collision(collision_types::ground);
	//c.p_id = e.ID();
	c.on_collide = [=, &game, &o](u32 ID)
	{
		o.enemy_ID = ID;
		game.destroy_entity(e);
	};


	world_coords delta(dest.x - source.x, dest.y - source.y);
	float hypotLen = sqrt(pow(delta.x, 2) + pow(delta.y, 2));


	ecs::c_velocity& v = game.ecs.add<ecs::c_velocity>(e);
	v.delta = world_coords(speed.x * (delta.x / hypotLen), speed.y * (delta.y / hypotLen));
}



void egen_enemy(entity e, engine& game, world_coords pos)
{
    game.ecs.add<ecs::c_enemy>(e);

	ecs::c_display& spr = game.ecs.add<ecs::c_display>(e);
	spr.add_sprite(1, render_layers::sprites);
	spr.sprites(0).tex = game.textures().get("player");
	spr.sprites(0).set_pos(pos, sprite_coords(1, 1), 0);
 	spr.sprites(0).set_tex_region(0, 0);


	ecs::c_collision& c = game.ecs.add<ecs::c_collision>(e);
	c.disabled_sprites.push_back(1);
	ecs::c_health& h = game.ecs.add<ecs::c_health>(e);
	h.has_healthbar = true;
	ecs::c_damage& d = game.ecs.add<ecs::c_damage>(e);

	c.set_team_signal(ecs::c_collision::flags::enemy);
	c.set_team_detector(ecs::c_collision::flags::ally);
	c.on_collide = [=, &d](u32 ID)
	{
		d.enemy_ID = ID;
	};

	h.health = 100;
	h.max_health = 100;
}


void setup_player(engine& game)
{
	entity e = game.player_id();
	ecs::c_display& spr = game.ecs.add<ecs::c_display>(e);
	spr.add_sprite(1, render_layers::sprites);

	spr.sprites(0).tex = game.textures().get("player");

	spr.sprites(0).set_pos(sprite_coords(9, 9), sprite_coords(1, 1), 0);
	spr.sprites(0).set_tex_region(0, 0);

	game.ecs.add<ecs::c_velocity>(e);
	game.ecs.add<ecs::c_weapon_pool>(e);
	game.ecs.add<ecs::c_player>(e);
	game.ecs.get<ecs::c_player>(e);

    game.ecs.get<ecs::c_inventory>(e);
	//pool.weapons[0] = w.stats;
	//pool.weapons[1] = c_weapon::data {bullet_types::standard, 75, 100} ;
/*
	c_health* h = game.ecs.add<c_health>(e);
	h->health = 100;
	h->die = [&]() {
		e.mark_deletion();
	};
*/
	ecs::c_collision& c = game.ecs.add<ecs::c_collision>(e);
	c.set_team_detector(ecs::c_collision::flags::enemy);
	c.set_tilemap_collision(ecs::c_collision::flags::ground);
	c.on_collide = [e, &game](u32 ID)
	{
		if(ID == game.map_id()) {
			auto& velocity = game.ecs.get<ecs::c_velocity>(e);
			ecs::c_display& spr2 = game.ecs.get<ecs::c_display>(e);
			for (auto& sprite : spr2) {
			    sprite.move_by(sprite_coords(0, 0) - velocity.delta);
			}
			printf("player collides with wall \n");
		}
	};
}


// remove B from A's children list
void remove_child(entity a, entity b, engine& game) {

	ecs::c_widget& w = game.ecs.get<ecs::c_widget>(a);
	for (auto it =  w.children.begin(); it !=  w.children.end(); it++) {
		if (*it == b) {
			// Constant time delete optimization since order is meaningless,
			std::swap(* w.children.rbegin(), *it);
			w.children.pop_back();
			return;
		}
	}
}

void make_parent(entity a, entity b, engine& game) {
	ecs::c_widget& w_a = game.ecs.get<ecs::c_widget>(a);
	ecs::c_widget& w_b = game.ecs.get<ecs::c_widget>(b);

	if (w_b.parent != 65535)  remove_child(w_b.parent, b, game);

	w_a.children.emplace_back(b);
	w_b.parent = a;
}





void basic_sprite_setup(entity e, engine& g, render_layers layer, sprite_coords origin, sprite_coords pos_size, point<f32> uv_origin, size<f32> uv_size, std::string texname) {
	ecs::c_display& spr = g.ecs.add<ecs::c_display>(e);
    spr.add_sprite(1, layer);
	spr.sprites(0).tex = g.textures().get(texname.c_str());
    spr.sprites(0).set_pos(origin, pos_size, 0);
    spr.sprites(0).set_tex_region(0, 0);
}

void make_widget(entity e, engine& g, entity parent) {
    g.ecs.add<ecs::c_widget>(e);
    make_parent(parent, e, g);
}



bool selection_keypress(entity e, const event& ev, engine& g) {
	ecs::c_selection& select = g.ecs.get<ecs::c_selection>(e);
	if (ev.active_state() == false) return false;
	// std::min is a fuckwad!
	//int  real_zero = 0;

	bool movement = false;

	point<u16> new_active = select.active;
	switch(ev.ID) {
	/*case SDLK_LEFT:
		new_active.x = std::max(static_cast<int>(select.active.x - 1), real_zero);
		movement = true;
		break;
	case SDLK_RIGHT:
		new_active.x = std::min(select.active.x + 1, select.grid_size.w - 1);
		movement = true;
		break;
	case SDLK_UP:
		new_active.y = std::max(static_cast<int>(select.active.y - 1), real_zero);
		movement = true;
		break;
	case SDLK_DOWN:
		new_active.y = std::min(select.active.y + 1, select.grid_size.y - 1);
		movement = true;
		break;

	case SDLK_RETURN: {
		c_mouseevent& mve = g.ecs.get<c_mouseevent>(e);
		event ev;
		ev.set_type(event_flags::button_press);
		ev.set_active(true);
		mve.run_event(ev);
		ev.set_active(false);
		mve.run_event(ev);
	}*/
	}

	if (movement) {
		ecs::c_display& spr = g.ecs.get<ecs::c_display>(e);
		ecs::c_cursorevent& cve = g.ecs.get<ecs::c_cursorevent>(e);
		u16 x_pos = (new_active.x * 64) + spr.get_dimensions().origin.x;
		u16 y_pos = (new_active.y * 64) + spr.get_dimensions().origin.y;

		event e;
		e.set_active(true);
		e.set_type(event_flags::cursor_moved);
		e.pos = screen_coords(x_pos, y_pos);
		cve.run_event(e);

	}
	//set_box(select);
	return true;
}


rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes ) {
	sprite_coords p_origin = parent.origin;
	sprite_coords p_size = parent.size;

	sprite_coords new_origin(((sizes.origin.x / 100.0f) * p_size.x) + p_origin.x,
						((sizes.origin.y / 100.0f) * p_size.y) + p_origin.y);
	sprite_coords new_size((p_size.x / 100) * sizes.size.x, ( p_size.y / 100) * sizes.size.y);

	return rect<f32>(new_origin, new_size);
}


bool follower_moveevent(entity e, const event& ev, ecs::c_display& spr) {
	sprite_coords dim = spr.get_dimensions().size;
	sprite_coords p (ev.pos.x - (dim.x / 2), ev.pos.y - (dim.y / 2));
	for (auto& sprite : spr) {
		sprite.move_to(p);
	}
	return true;
}
