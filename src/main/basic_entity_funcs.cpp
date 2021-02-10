#include "basic_entity_funcs.h"

#include <engine/engine.h>
#include <random>
#include <math.h>




void egen_bullet(entity e, engine& game, std::string texname,
		size<f32> dimensions, point<f32> speed, point<f32> source, point<f32> dest,	collision_flags team)
{

	sprite& spr = game.ecs.add_component<sprite>(e);
	spr.gen_subsprite(1, render_layers::sprites);
	spr.get_subsprite(0).tex = game.renderer.get_texture(texname.c_str());
	//spr._dimensions = rect<f32>(source, dimensions);
	spr.set_pos(source, dimensions, 0, 0);



	c_damage& o = game.ecs.add_component<c_damage>(e);
	o.damage = 25;

	c_collision& c = game.ecs.add_component<c_collision>(e);
	c.set_team_signal(team);
	c.set_tilemap_collision(collision_flags::ground);
	c.set_tilemap_collision(collision_flags::air);
	//c.set_tilemap_collision(collision_types::ground);
	//c.p_id = e.ID();
	c.on_collide = [=, &game, &o](u32 ID)
	{
		o.enemy_ID = ID;
		game.destroy_entity(e);
	};


	point<f32> delta(dest.x - source.x, dest.y - source.y);
	float hypotLen = sqrt(pow(delta.x, 2) + pow(delta.y, 2));


	c_velocity& v = game.ecs.add_component<c_velocity>(e);
	v.delta = point<f32>((delta.x / hypotLen) / 8, (delta.y / hypotLen) / 8);

	spr.set_uv(point<f32>(0, 0), size<f32>(1, 1), 0, 0);
	spr.rotate(atan2(dest.x - source.x, (source.y - dest.y)));
}



/*
void egen_crate(entity e, sprite& spr, c_collision& c, c_health& h, point<f32> pos)
{
	spr.gen_subsprite(1, render_layers::sprites);
	spr.set_pos(pos, size<f32>(1, 1), 0, 0);
	spr.set_uv(point<f32>(0, 0), size<f32>(1.0f, 1.0f), 0, 0);

	//c.p_id = e.ID();
	c.set_team_detector(collision_flags::ally);
	c.set_team_detector(collision_flags::enemy);


	h.health = 100;
}*/


void egen_enemy(entity e, engine& game, point<f32> pos)
{
    game.ecs.add_component<c_enemy>(e);

	sprite& spr = game.ecs.add_component<sprite>(e);
	spr.gen_subsprite(1, render_layers::sprites);
	spr.get_subsprite(0).tex = game.renderer.get_texture("player");
	spr.set_pos(pos, size<f32>(1, 1), 0, 0);
 	spr.set_uv( point<f32>(0, 0), size<f32>(1, 1), 0, 0);


	c_collision& c = game.ecs.add_component<c_collision>(e);
	c.disabled_subsprites.push_back(1);
	c_health& h = game.ecs.add_component<c_health>(e);
	game.ecs.add_component<c_healthbar>(e);
	c_damage& d = game.ecs.add_component<c_damage>(e);

	c.set_team_signal(collision_flags::enemy);
	c.set_team_detector(collision_flags::ally);
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
	sprite& spr = game.ecs.add_component<sprite>(e);
	spr.gen_subsprite(1, render_layers::sprites);

	spr.get_subsprite(0).tex = game.renderer.get_texture("player");

	spr.set_pos(point<f32>(9, 9), size<f32>(0.75, 0.75), 0, 0);

	game.ecs.add_component<c_velocity>(e);
	game.ecs.add_component<c_weapon_pool>(e);
	game.ecs.add_component<c_player>(e);
	game.ecs.get_component<c_player>(e);

    game.ecs.get_component<c_inventory>(e);
	//pool.weapons[0] = w.stats;
	//pool.weapons[1] = c_weapon::data {bullet_types::standard, 75, 100} ;
/*
	c_health* h = game.ecs.add_component<c_health>(e);
	h->health = 100;
	h->die = [&]() {
		e.mark_deletion();
	};
*/
	c_collision& c = game.ecs.add_component<c_collision>(e);
	c.set_team_detector(collision_flags::enemy);
	c.set_tilemap_collision(collision_flags::ground);
	c.on_collide = [e, &game](u32 ID)
	{
		if(ID == game.map_id()) {
			auto& velocity = game.ecs.get_component<c_velocity>(e);
			sprite& spr2 = game.ecs.get_component<sprite>(e);
			spr2.move_by(point<f32>(0,0 ) - velocity.delta);
			printf("player collides with wall \n");
		}
	};

	spr.set_uv( point<f32>(0, 0), size<f32>(1, 1), 0, 0);
}


// remove B from A's children list
void remove_child(entity a, entity b, engine& game) {

	c_widget& w = game.ecs.get_component<c_widget>(a);
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
	c_widget& w_a = game.ecs.get_component<c_widget>(a);
	c_widget& w_b = game.ecs.get_component<c_widget>(b);

	if (w_b.parent != 65535)  remove_child(w_b.parent, b, game);

	w_a.children.emplace_back(b);
	w_b.parent = a;
}





void basic_sprite_setup(entity e, engine& g, render_layers layer, point<f32> origin, size<f32> pos_size, point<f32> uv_origin, size<f32> uv_size, std::string texname) {
    sprite& spr = g.ecs.add_component<sprite>(e);
    spr.gen_subsprite(1, layer);
    spr.set_pos(origin, pos_size, 0, 0);
    spr.set_uv(uv_origin, uv_size, 0, 0);
    spr.get_subsprite(0).tex = g.renderer.get_texture(texname.c_str());
}

void make_widget(entity e, engine& g, entity parent) {
    g.ecs.add_component<c_widget>(e);
    make_parent(parent, e, g);
}



bool selection_keypress(entity e, const event& ev, engine& g) {
	c_selection& select = g.ecs.get_component<c_selection>(e);
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
		new_active.y = std::min(select.active.y + 1, select.grid_size.h - 1);
		movement = true;
		break;

	case SDLK_RETURN: {
		c_mouseevent& mve = g.ecs.get_component<c_mouseevent>(e);
		event ev;
		ev.set_type(event_flags::button_press);
		ev.set_active(true);
		mve.run_event(ev);
		ev.set_active(false);
		mve.run_event(ev);
	}*/
	}

	if (movement) {
		sprite& spr = g.ecs.get_component<sprite>(e);
		c_cursorevent& cve = g.ecs.get_component<c_cursorevent>(e);
		u16 x_pos = (new_active.x * 64) + spr.get_dimensions().origin.x;
		u16 y_pos = (new_active.y * 64) + spr.get_dimensions().origin.y;

		event e;
		e.set_active(true);
		e.set_type(event_flags::cursor_moved);
		e.pos = point<f32>(x_pos, y_pos);
		cve.run_event(e);

	}
	//set_box(select);
	return true;
}


rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes ) {
	point<f32> p_origin = parent.origin;
	size<f32> p_size = parent.size;

	point<f32> new_origin(((sizes.origin.x / 100.0f) * p_size.w) + p_origin.x,
						((sizes.origin.y / 100.0f) * p_size.h) + p_origin.y);
	size<f32> new_size((p_size.w / 100) * sizes.size.w, ( p_size.h / 100) * sizes.size.h);

	return rect<f32>(new_origin, new_size);
}


bool follower_moveevent(entity e, const event& ev, sprite& spr) {
	size<f32> dim = spr.get_dimensions().size;
	point<f32> p (ev.pos.x - (dim.w / 2), ev.pos.y - (dim.h / 2));
	spr.move_to(p);
	return true;
}
