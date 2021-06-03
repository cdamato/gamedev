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
    add_sliders(e, g, point<f32>(100, 100), sprite_coords(256, 16),
            0, 25, 100, 10, "Master Volume",
            0, 25, 100, 10, "Novice Volume");
}