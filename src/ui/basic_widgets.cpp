#include "ui.h"

///////////////////////////
//     CHECKBOX CODE     //
///////////////////////////

void optionmenu_activation(entity e, engine& g) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    auto& checkbox = g.ecs.get<ecs::c_checkbox>(e);
    auto& dpy = g.ecs.get<ecs::c_display>(e);
    int active_index = project_to_1D(select.active, select.grid_size.x);
    checkbox.checked.flip(active_index);
    int new_tex_region = checkbox.checked.test(active_index) ? 3 : 2;
    dpy.sprites(checkbox.sprite_index).set_tex_region(new_tex_region, active_index);
}

void checkbox_set_highlight(entity e, engine& g, int index, bool enter) {
    auto& checkbox = g.ecs.get<ecs::c_checkbox>(e);
    int activated = (checkbox.checked.test(index) ? 1 : 0) + (enter ? 2 : 0);
    g.ecs.get<ecs::c_display>(e).sprites(checkbox.sprite_index).set_tex_region(activated, index);
}

void checkbox_navigation(entity e, engine& g, int old_index, int new_index) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    checkbox_set_highlight(e, g, old_index, false);
    if (u32(new_index) >= (select.grid_size.x * select.grid_size.y)) return;
    checkbox_set_highlight(e, g, new_index, true);
}

void add_checkbox(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, bool state, std::string label) {
    auto& checkbox = g.ecs.get<ecs::c_checkbox>(e);
    auto& display = g.ecs.get<ecs::c_display>(e);
    auto& text = g.ecs.get<ecs::c_text>(e);
    checkbox.checked[index] = state;

    display.sprites(0).set_pos(pos, grid_size, index);
    display.sprites(0).set_tex_region(0, index);
    pos += sprite_coords(16, 16);
    display.sprites(checkbox.sprite_index).set_pos(pos, sprite_coords(16, 16), index);
    display.sprites(checkbox.sprite_index).set_tex_region(state == true ? 1 : 0, index);
    display.sprites(text.sprite_index).set_pos(pos + sprite_coords(24, - 9), sprite_coords(300, 32), index);
    text.text_entries.push_back(ecs::c_text::text_entry{index, label});
}

void initialize_checkbox_group(entity e, engine& g, int num_checkboxes) {
    make_widget(e, g, g.ui.root);
    auto& widget = g.ecs.get<ecs::c_widget>(e);
    widget.on_activate = optionmenu_activation;
    widget.on_navigate = checkbox_navigation;

    g.ecs.add<ecs::c_selection>(e).grid_size = size<u16>(1, num_checkboxes);

    auto& display = g.ecs.add<ecs::c_display>(e);
    display.add_sprite(num_checkboxes, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");

    auto& text = g.ecs.add<ecs::c_text>(e);
    text.sprite_index = display.add_sprite(num_checkboxes, render_layers::ui);;

    int checkbox_sprite_index = display.add_sprite(num_checkboxes, render_layers::ui);
    auto& checkbox = g.ecs.add<ecs::c_checkbox>(e);
    checkbox.sprite_index = checkbox_sprite_index;
    display.sprites(checkbox_sprite_index).tex = g.textures().get("checkbox");
}

/////////////////////////
//     SLIDER CODE     //
/////////////////////////

void slider_set_value(entity e, engine& g, int value, int index) {
    auto& slider = g.ecs.get<ecs::c_slider>(e);
    auto& display = g.ecs.get<ecs::c_display>(e);
    value = std::clamp(value, slider.min_values[index], slider.max_values[index]);
    slider.current_values[index] = value;
    rect<f32> slider_rect = display.sprites(1).get_dimensions(index + g.ecs.get<ecs::c_selection>(e).grid_size.y);
    f32 value_percentage = (f32(value) / f32(slider.max_values[index]));
    f32 new_x = slider_rect.origin.x + (value_percentage * slider_rect.size.x) - 8;
    new_x = std::clamp(new_x, slider_rect.origin.x, slider_rect.origin.x + slider_rect.size.x);
    display.sprites(1).set_pos(point<f32>(new_x, slider_rect.origin.y), sprite_coords(16, 32), index);
}

int value_at_position(entity e, engine& g, int position, int index) {
    auto& slider = g.ecs.get<ecs::c_slider>(e);
    auto& display = g.ecs.get<ecs::c_display>(e);
    rect<f32> slider_rect = display.sprites(1).get_dimensions(index + g.ecs.get<ecs::c_selection>(e).grid_size.y);
    position = std::clamp(f32(position), slider_rect.origin.x, slider_rect.origin.x + slider_rect.size.x);
    f32 position_percentage = (f32(position - slider_rect.origin.x) / f32(slider_rect.size.x));
    f32 new_value = position_percentage * (slider.max_values[index] - slider.min_values[index]);
    return new_value;
}

void slider_navigation(entity e, engine& g, int old_position, int new_position) {
    slider_set_value(e, g, value_at_position(e, g, new_position, 0), g.ecs.get<ecs::c_selection>(e).active_index());
}

void add_slider(entity e, engine& g, point<f32> pos, size<f32> grid_size, int index, int min, int current, int max, int increment, std::string label) {
    auto& slider = g.ecs.get<ecs::c_slider>(e);
    auto& display = g.ecs.get<ecs::c_display>(e);
    slider.min_values[index] = min;
    slider.current_values[current] = current;
    slider.max_values[index] = max;
    slider.increments[index] = increment;

    display.sprites(0).set_pos(pos, sprite_coords(512, 64), index);
    display.sprites(0).set_tex_region(0, index);

    int num_sliders = g.ecs.get<ecs::c_selection>(e).grid_size.y;
    display.sprites(1).set_pos(pos + point<f32>(240, 16), sprite_coords(256, 32), index + num_sliders );
    display.sprites(1).set_tex_region(index, 1);
    display.sprites(1).set_tex_region((index + num_sliders), 0);
    slider_set_value(e, g, 25, index);

    auto& text = g.ecs.get<ecs::c_text>(e);
    display.sprites(text.sprite_index).set_pos(pos + sprite_coords(24, 16), sprite_coords(384, 32), index);
    text.text_entries.push_back(ecs::c_text::text_entry{index, label});
}

void initialize_slider_group(entity e, engine& g, int num_sliders) {
    make_widget(e, g, g.ui.root);
    auto& widget = g.ecs.get<ecs::c_widget>(e);
    widget.on_navigate = slider_navigation;
    g.ecs.add<ecs::c_selection>(e).grid_size = size<u16>(1, num_sliders);

    auto& display = g.ecs.add<ecs::c_display>(e);
    display.add_sprite(num_sliders, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");

    display.add_sprite(num_sliders * 2, render_layers::ui);
    display.sprites(1).tex = g.textures().get("slider");
    g.ecs.add<ecs::c_slider>(e);

    auto& text = g.ecs.add<ecs::c_text>(e);
    text.sprite_index = display.add_sprite(num_sliders, render_layers::ui);
}

////////////////////////////
//     SELECTION CODE     //
////////////////////////////

void selectiongrid_set_highlight(entity e, engine& g, int index, bool enter) {
    g.ecs.get<ecs::c_display>(e).sprites(0).set_tex_region(enter ? 1 : 0, index);
}

void selectiongrid_navigation(entity e, engine& g, int old_index, int new_index) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    selectiongrid_set_highlight(e, g, old_index, false);
    if (u32(new_index) >= (select.grid_size.x * select.grid_size.y)) return;
    selectiongrid_set_highlight(e, g, new_index, true);
    select.active = project_to_2D<u16>(new_index, select.grid_size.x);
}

