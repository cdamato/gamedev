#ifndef ENTITY_FUNCS_H
#define ENTITY_FUNCS_H

#include <common/basic_types.h>
#include <string>
#include <vector>

#include <ecs/ecs.h>

struct engine;

void init_npc_hub(engine& e);
void basic_sprite_setup(entity e, engine& g, render_layers layer, sprite_coords origin, sprite_coords pos_size, size_t tex_index, std::string texname);
void egen_bullet(entity, engine&, std::string, world_coords, world_coords, world_coords, world_coords, ecs::c_collision::flags);
void egen_enemy(entity, engine&, world_coords);

void setup_player(engine&);
void init_map(engine&);
void set_map(engine&, world_coords, std::vector<u8>);
std::vector<u8> generate_map(world_coords);

#endif //ENTITY_FUNCS_H
