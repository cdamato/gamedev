#ifndef UI_HELPER_FUNCS_H
#define UI_HELPER_FUNCS_H

#include <engine/engine.h>




void checkbox_set_highlight(entity e, engine& g, int index, bool enter);
void checkbox_navigation(entity e, engine& g, int old_index, int new_index);
void add_checkbox(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, bool state, std::string label);
void initialize_checkbox_group(entity e, engine& g, int num_checkboxes);


template<typename T, typename U>
void add_checkboxes(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, T state, U label) {
    add_checkbox(e, g, pos, grid_size, index, state, label);
}

template<typename T, typename U, typename... Args>
void add_checkboxes(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, T state, U label, Args... args) {
    add_checkbox(e, g, pos, grid_size, index, state, label);
    pos += point<f32>(0, 48);
    add_checkboxes(e, g, pos, grid_size, index + 1, args...);
}

template<typename... Args>
void add_checkboxes(entity e, engine& g, point<f32> pos, size<f32> grid_size, Args... args) {
    initialize_checkbox_group(e, g, sizeof...(args)  / 2);
    add_checkboxes(e, g, pos, grid_size, 0, args...);
}


void slider_set_value(entity e, engine& g, int value, int index);
void slider_navigation(entity e, engine& g, int old_index, int new_index);
void add_slider(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, int min, int current, int max, int increment, std::string label);
void initialize_slider_group(entity e, engine& g, int num_sliders);


template<typename T, typename U, typename V, typename Y, typename X>
void add_sliders_impl(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, T min, U current, V max, Y increment, X label) {
    add_slider(e, g, pos, grid_size, index, min, current, max, increment, label);
}

template<typename T, typename U, typename V, typename Y, typename X, typename... Args>
void add_sliders_impl(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, T min, U current, V max, Y increment, X label, Args... args) {
    add_slider(e, g, pos, grid_size, index, min, current, max, increment, label);
    pos += point<f32>(0, 64);
    add_sliders_impl(e, g, pos, grid_size, index + 1, args...);
}

template<typename... Args>
void add_sliders(entity e, engine& g, point<f32> pos, size<f32> grid_size, Args... args) {
    initialize_slider_group(e, g, sizeof...(args)  / 5);
    add_sliders_impl(e, g, pos, grid_size, 0, args...);
}


void selectiongrid_set_highlight(entity e, engine& g, int index, bool enter);
void selectiongrid_navigation(entity e, engine& g, int old_index, int new_index);

point<u16> get_gridindex(rect<f32> grid, size<u16> num_elements, point<f32> pos);

void inv_transfer_init(entity e, engine& g, ecs::c_inventory&);
rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes );
void make_widget(entity e, engine& g, entity parent);


void inventory_init(entity, engine&, entity, ecs::c_inventory& inv, screen_coords);
void options_menu_init(entity, engine&, entity, screen_coords);

#endif //UI_HELPER_FUNCS_H
