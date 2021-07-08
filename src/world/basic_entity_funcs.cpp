#include "basic_entity_funcs.h"

#include <engine/engine.h>
#include <random>
#include <math.h>




void egen_bullet(entity e, engine& game, std::string texname,
				 world_coords dimensions, world_coords speed, world_coords source, world_coords dest,  ecs::collision::flags team)
{

	 ecs::display& spr = game.ecs.add<ecs::display>(e);
	spr.add_sprite(1, game.textures().get(texname.c_str()), 3, render_layers::sprites);
	spr.sprites(0).set_pos(source, dimensions, 0);
	spr.sprites(0).set_tex_region(0, 0);
	spr.sprites(0).rotate(atan2(dest.x - source.x, (source.y - dest.y)));



	 ecs::damage& o = game.ecs.add<ecs::damage>(e);
	o.damage = 25;

	 ecs::collision& c = game.ecs.add<ecs::collision>(e);
	c.set_team_signal(team);
	c.set_tilemap_collision( ecs::collision::flags::ground);
	c.set_tilemap_collision( ecs::collision::flags::air);
	//c.set_tilemap_collision(collision_types::ground);
	//c.p_id = e.ID();
	c.on_collide = [=, &game, &o](u32 ID)
	{
		o.enemy_ID = ID;
		game.destroy_entity(e);
	};


	world_coords delta(dest.x - source.x, dest.y - source.y);
	float hypotLen = sqrt(pow(delta.x, 2) + pow(delta.y, 2));


	 ecs::velocity& v = game.ecs.add<ecs::velocity>(e);
	v.delta = world_coords(speed.x * (delta.x / hypotLen), speed.y * (delta.y / hypotLen));
}



void egen_enemy(entity e, engine& game, world_coords pos)
{
	game.ecs.add<ecs::enemy>(e);
	ecs::display& spr = game.ecs.add<ecs::display>(e);
	spr.add_sprite(1, game.textures().get("player"), 2, render_layers::sprites);
	spr.sprites(0).set_pos(pos, sprite_coords(1, 1), 0);
	spr.sprites(0).set_tex_region(0, 0);


	ecs::collision& c = game.ecs.add<ecs::collision>(e);
	c.disabled_sprites.push_back(1);
	ecs::health& h = game.ecs.add<ecs::health>(e);
	h.has_healthbar = true;
	ecs::damage& d = game.ecs.add<ecs::damage>(e);

	c.set_team_signal( ecs::collision::flags::enemy);
	c.set_team_detector( ecs::collision::flags::ally);
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
	ecs::display& spr = game.ecs.add<ecs::display>(e);
	spr.add_sprite(1, game.textures().get("player"), 2, render_layers::sprites);

	spr.sprites(0).set_pos(sprite_coords(9, 9), sprite_coords(1, 1), 0);
	spr.sprites(0).set_tex_region(0, 0);

	game.ecs.add<ecs::velocity>(e);
	game.ecs.add<ecs::weapon_pool>(e);
	game.ecs.add<ecs::player>(e);
	game.ecs.get<ecs::player>(e);

    game.ecs.get<ecs::inventory>(e);
	//pool.weapons[0] = w.stats;
	//pool.weapons[1] = c_weapon::data {bullet_types::standard, 75, 100} ;
/*
	c_health* h = game.ecs.add<c_health>(e);
	h->health = 100;
	h->die = [&]() {
		e.mark_deletion();
	};
*/
	 ecs::collision& c = game.ecs.add<ecs::collision>(e);
	c.set_team_detector( ecs::collision::flags::enemy);
	c.set_tilemap_collision( ecs::collision::flags::ground);
	c.on_collide = [e, &game](u32 ID)
	{
		if(ID == game.map_id()) {
			auto& velocity = game.ecs.get<ecs::velocity>(e);
			 ecs::display& spr2 = game.ecs.get<ecs::display>(e);
			for (auto& sprite : spr2) {
			    sprite.move_by(sprite_coords(0, 0) - velocity.delta);
			}
			printf("player collides with wall \n");
		}
	};
}

void basic_sprite_setup(entity e, engine& g, render_layers layer, sprite_coords origin, sprite_coords pos_size, size_t tex_index, std::string texname) {
	ecs::display& spr = g.ecs.add<ecs::display>(e);
    spr.add_sprite(1, g.textures().get(texname.c_str()), 3, layer);
    spr.sprites(0).set_pos(origin, pos_size, 0);
    spr.sprites(0).set_tex_region(tex_index, 0);
}
