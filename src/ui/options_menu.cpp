#include <engine/engine.h>
#include <algorithm>
#include "ui.h"

void options_menu_init(entity e, engine& g, entity root, screen_coords pos_in) {
    make_widget(e, g, g.ui.root);
    auto& display = g.ecs.add<ecs::c_display>(e);
    display.add_sprite(1, render_layers::ui);

    sprite_coords pos = pos_in.to<f32>();
    sprite_coords label_fullscreen_size = g.get_text_size("GRAPHICSMENU_FULLSCREEN_CONTROL");
    sprite_coords label_vsync_size = g.get_text_size("GRAPHICSMENU_VSYNC_CONTROL");
    sprite_coords label_windowres_size = g.get_text_size("GRAPHICSMENU_WINDOWRES_CONTROL");
    sprite_coords label_renderer_size = g.get_text_size("GRAPHICSMENU_RENDERER_CONTROL");
    sprite_coord_t largest_x = 1.3 * std::max({label_fullscreen_size.x, label_vsync_size.x, label_windowres_size.x, label_renderer_size.x});

    std::vector<std::string> windowres_options {"1920x1080", "1600x1900", "1024x768"};
    std::vector<std::string> renderer_options {"OpenGL", "Software"};

    sprite_coords max_option_size(0, 0);
    for (auto& option : windowres_options) {
        max_option_size = std::max(max_option_size, g.get_text_size(option));
    }
    for (auto& option : renderer_options) {
        max_option_size = std::max(max_option_size, g.get_text_size(option));
    }
    max_option_size.x *= 1.3;
    max_option_size.y *= 1.2;

    sprite_coords widget_pos = sprite_coords(pos.x + largest_x, pos.y);
    entity checkboxes = g.create_entity();
    initialize_checkbox_group(checkboxes, e, 5, g, 2);
    add_checkbox(checkboxes, g, pos, sprite_coords(largest_x, max_option_size.y), widget_pos, 0, true, "GRAPHICSMENU_FULLSCREEN_CONTROL");
    pos.y += max_option_size.y * 1.2;
    widget_pos.y += max_option_size.y * 1.2;
    add_checkbox(checkboxes, g, pos, sprite_coords(largest_x, max_option_size.y), widget_pos, 1, true, "GRAPHICSMENU_VSYNC_CONTROL");
    pos.y += max_option_size.y * 1.2;
    widget_pos.y += max_option_size.y* 1.2;


    entity dropdowns = g.create_entity();
    initialize_dropdown_group(dropdowns, e, 5, g, 2);
    add_dropdown(dropdowns, g, pos, sprite_coords(largest_x, max_option_size.y), widget_pos, max_option_size, 0, windowres_options, "GRAPHICSMENU_WINDOWRES_CONTROL", 0);
    pos.y += max_option_size.y* 1.2;
    widget_pos.y += max_option_size.y* 1.2;
    add_dropdown(dropdowns, g, pos, sprite_coords(largest_x, max_option_size.y), widget_pos, max_option_size, 1, renderer_options, "GRAPHICSMENU_RENDERER_CONTROL", 0);

    display.sprites(0).set_pos(pos_in.to<f32>(), widget_pos + max_option_size, 0);
    display.sprites(0).tex = g.textures().get("menu_background");
}
/*
void options_menu_init(entity e, engine& g, entity root, screen_coords pos) {
    g.ui.focus = e;
    add_textinput(e, g.ui.root, 5, g, point<f32>(100, 100), sprite_coords(200, 32));
}*/