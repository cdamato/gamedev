#include <engine/engine.h>
#include "ui.h"

/*
void options_menu_init(entity e, engine& g, entity root, screen_coords pos) {
    g.ui.focus = e;
    add_checkboxes(e, g, pos.to<f32>() + point<f32>(16, 16), sprite_coords(400, 48),
                   false, "Make settings menu mockup",
                   true, "make checkboxes highlight/click",
                   true, "add more options",
                   false, "hope this thing works");
}*/

void options_menu_init(entity e, engine& g, entity root, screen_coords pos) {
    g.ui.focus = e;
    initialize_dropdown_group(e, g.ui.root, 5, g, 1);
    add_dropdown(e, g, point<f32>(100, 100), sprite_coords(200, 48), 0, std::vector<std::string>{"hi", "bye", "uwu"}, 0);
}