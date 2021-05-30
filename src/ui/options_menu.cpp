#include <engine/engine.h>
#include <world/basic_entity_funcs.h>
#include "ui_helper_funcs.h"




void optionmenu_activation(entity e, engine& g) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    auto& checkbox = g.ecs.get<ecs::c_checkbox>(e);
    auto& dpy = g.ecs.get<ecs::c_display>(e);
    int active_index = project_to_1D(select.active, select.grid_size.x);
    checkbox.checked.flip(active_index);
    int new_tex_region = checkbox.checked.test(active_index) ? 3 : 2;
    dpy.sprites(checkbox.sprite_index).set_tex_region(new_tex_region, active_index);
}

void optionmenu_set_highlight(entity e, engine& g, int index, bool state) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    auto& checkbox = g.ecs.get<ecs::c_checkbox>(e);
    auto& dpy = g.ecs.get<ecs::c_display>(e);

    int activated = (checkbox.checked.test(index) ? 1 : 0) + (state ? 2 : 0);
    dpy.sprites(0).set_tex_region(state ? 1 : 0, index);
    dpy.sprites(checkbox.sprite_index).set_tex_region(activated, index);

}

void optionmenu_navigation(entity e, engine& g, int old_index, int new_index) {

    auto& select = g.ecs.get<ecs::c_selection>(e);
    optionmenu_set_highlight(e, g, old_index, false);

    if (abs(new_index) >= (select.grid_size.x * select.grid_size.y)) return;
    optionmenu_set_highlight(e, g, new_index, true);
    select.active = project_to_2D<u16>(new_index, select.grid_size.x);
}


void add_checkbox(entity e, sprite_data& checkbox_sprite, sprite_data& text_sprite, ecs::c_text& text, point<f32> pos, bool state, std::string label, int index) {
    checkbox_sprite.set_pos(pos, sprite_coords(16, 16), index);
    checkbox_sprite.set_tex_region(state == true ? 1 : 0, index);
    text_sprite.set_pos(pos + sprite_coords(24, - 9), sprite_coords(300, 32), index);
    text.text_entries.push_back(ecs::c_text::text_entry{index, label});
}

void options_menu_init(entity e, engine& g, entity root, screen_coords pos) {
    g.ui.focus = e;

    make_widget(e, g, g.ui.root);
    auto& widget = g.ecs.get<ecs::c_widget>(e);
    widget.on_activate = optionmenu_activation;
    widget.on_navigate = optionmenu_navigation;

    auto& select = g.ecs.add<ecs::c_selection>(e);
    select.grid_size = size<u16>(1, 2);

    auto& display = g.ecs.add<ecs::c_display>(e);
    display.add_sprite(3, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");

    int text_sprite_index = display.add_sprite(2, render_layers::text);
    auto& text = g.ecs.add<ecs::c_text>(e);
    text.sprite_index = text_sprite_index;

    int checkbox_sprite_index = display.add_sprite(2, render_layers::ui);
    auto& checkbox = g.ecs.add<ecs::c_checkbox>(e);
    checkbox.sprite_index = checkbox_sprite_index;

    sprite_data& text_sprite = display.sprites(text_sprite_index);
    sprite_data& checkbox_sprite = display.sprites(checkbox_sprite_index);
    checkbox_sprite.tex = g.textures().get("checkbox");


    point<f32> write_pos = pos.to<f32>();
    add_checkbox(e, checkbox_sprite, text_sprite, text, write_pos+ point<f32>(16, 16), false, "Make settings menu mockup", 0);

    display.sprites(0).set_pos(write_pos, sprite_coords(400, 48), 0);
    display.sprites(0).set_tex_region(0, 0);

    write_pos += point<f32>(0, 48);
    add_checkbox(e, checkbox_sprite, text_sprite, text, write_pos + point<f32>(16, 16), true, "make checkboxes highlight/click", 1);
    checkbox.checked[1] = true;
    display.sprites(0).set_pos(write_pos, sprite_coords(400, 48), 1);
    display.sprites(0).set_tex_region(0, 1);
}