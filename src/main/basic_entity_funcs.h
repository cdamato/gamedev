#ifndef ENTITY_FUNCS_H
#define ENTITY_FUNCS_H

#include <common/basic_types.h>
#include <string>
#include <vector>

enum collision_flags : u8;
enum class render_layers : u8;
class c_display;
class event;
using entity = u32;
struct engine;

struct c_inventory;
void inv_transfer_init(entity e, engine& g, c_inventory&);

class engine;
void init_npc_hub(engine& e);

rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes );
bool follower_moveevent(u32 e, const event& ev, c_display&);

// make A a parent of B
void make_parent(entity a, entity b, engine&);
bool selection_keypress(u32 e, const event& ev, engine& c);

void basic_sprite_setup(entity e, engine& g, render_layers layer, sprite_coords origin, sprite_coords pos_size, point<f32> uv_origin, size<f32> uv_size, std::string texname);
void make_widget(entity e, engine& g, entity parent);

void egen_bullet(entity, engine&, std::string, world_coords, world_coords, world_coords, world_coords, collision_flags);
void egen_enemy(entity, engine&, world_coords);

void init_map(engine&);
void setup_player(engine&);
void set_map(engine&, world_coords, std::vector<u8>);
std::vector<u8> generate_map(world_coords);

#endif //ENTITY_FUNCS_H
