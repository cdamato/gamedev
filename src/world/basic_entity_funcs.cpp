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







void basic_sprite_setup(entity e, engine& g, render_layers layer, sprite_coords origin, sprite_coords pos_size, size_t tex_index, std::string texname) {
	ecs::c_display& spr = g.ecs.add<ecs::c_display>(e);
    spr.add_sprite(1, layer);
	spr.sprites(0).tex = g.textures().get(texname.c_str());
    spr.sprites(0).set_pos(origin, pos_size, 0);
    spr.sprites(0).set_tex_region(tex_index, 0);
}