#ifndef UI_HELPER_FUNCS_H
#define UI_HELPER_FUNCS_H

#include <engine/engine.h>


point<u16> get_gridindex(rect<f32> grid, size<u16> num_elements, point<f32> pos);

void inv_transfer_init(entity e, engine& g, ecs::c_inventory&);
rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes );
bool follower_moveevent(u32 e, const event& ev, ecs::c_display&);
bool selection_keypress(u32 e, const event& ev, engine& c);
void make_widget(entity e, engine& g, entity parent);

#endif //UI_HELPER_FUNCS_H
