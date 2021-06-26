#include "engine.h"
#include <common/parser.h>
#include <world/basic_entity_funcs.h>
#include <SDL2/SDL.h>

void engine::run_tick() {
    world_coords start_pos = ecs.get<ecs::c_display>(player_id()).get_dimensions().origin;

    ecs.run_ecs(settings.framerate_multiplier);
    ui.active_interact = ecs.systems.proxinteract.active_interact;
    //renderer.set_texture_data()
    for (auto logic_func : logic.logic) {
        logic_func(*this);
    }

    int h = ui.hover_timer.elapsed<timer::ms>().count();
    if (h >= 1500 && ui.hover_active == false) {
        ui.hover_active = true;
        entity focus = ui.focus;
        if (focus != 65535 && focus != ui.root) {
            auto& widget = ecs.get<ecs::c_widget>(focus);
            if (widget.on_hover != nullptr) {
                widget.on_hover(focus, *this);
            }
        }
    } else if (ui.hover_active == true && h < 1500) { //timer has been reset

    }

    textures().update(ecs.systems.health.get_texture());
    textures().update(ecs.systems.text.get_texture());

    renderer().set_camera(offset);
    renderer().clear_sprites();
    for (auto& display : ecs.components.get_pool(ecs::type_tag<ecs::c_display>())) {
        for (auto& sprite : display) {
            if (sprite.layer == render_layers::null) continue;
            renderer().add_sprite(sprite);
        }
    }
    renderer().mark_sprites_dirty();

    world_coords end_pos = ecs.get<ecs::c_display>(player_id()).get_dimensions().origin;
    offset += (end_pos - start_pos);
}


void engine::destroy_entity(entity e) {
    ecs.entities.mark_entity(e);
    // Recursively delete child widget entities, remove entity from parent
    if (ecs.exists<ecs::c_widget>(e)) {
        remove_child(ecs.get<ecs::c_widget>(ecs.get<ecs::c_widget>(e).parent), e);
        ecs::c_widget& w = ecs.get<ecs::c_widget>(e);
        for (auto child : w.children) destroy_entity(child);
    }
}

void engine::render() { display.render();}

void logic_manager::add(logic_func func) { logic.push_back(func); }

void logic_manager::remove(logic_func func) {
    for (auto it : logic) {
        if (it == func) {
            std::swap(it, logic.back());
            logic.pop_back();
            break;
        }
    }
}


settings_manager::settings_manager() {
    bindings[SDLK_w] = command::move_up;
    bindings[SDLK_a] = command::move_left;
    bindings[SDLK_s] = command::move_down;
    bindings[SDLK_d] = command::move_right;
    bindings[SDLK_e] = command::interact;
    bindings[SDLK_TAB] = command::toggle_inventory;
    bindings[SDLK_o] = command::toggle_options_menu;
    bindings[SDLK_UP] = command::nav_up;
    bindings[SDLK_LEFT] = command::nav_left;
    bindings[SDLK_DOWN] = command::nav_down;
    bindings[SDLK_RIGHT] = command::nav_right;
    bindings[SDLK_RETURN] = command::nav_activate;
    bindings[SDLK_BACKSPACE] = command::text_backspace;
    bindings[SDLK_DELETE] = command::text_delete;

    config_parser p("settings.txt");
    auto d = p.parse();

    const config_list* list = dynamic_cast<const config_list*>(d->get("window_size"));
    resolution = screen_coords(list->get<int>(0), list->get<int>(1));

    int fullscreen =  d->get<int>("fullscreen");
    if (fullscreen != 0) flags.set(window_flags::fullscreen);
    if (fullscreen == 1) flags.set(window_flags::bordered_window);

    if (d->get<bool>("vsync")) flags.set(window_flags::vsync);
    if(d->get<bool>("use_software_renderer")) flags.set(window_flags::use_software_render);


    int framerate_mult =  d->get<int>("framerate_multiplier");
    if (framerate_mult != 0) {
        framerate_multiplier = framerate_mult;
    }
}

bool engine::process_events() {
    return window().poll_events();
}

engine::engine() {

    display.initialize(display::display_manager::display_types(settings.flags.test(window_flags::use_software_render)), settings.resolution);
    printf("Window Initialized\n");
    window().event_callback = [this](input_event& ev) { process_event(ev, *this); };
    window().resize_callback = [this](screen_coords size) { renderer().set_viewport(size); };

    ecs.systems.shooting.bullet_types.push_back(ecs::s_shooting::bullet{world_coords(0.3, 0.6), world_coords(0.25, 0.25), "bullet"});
    ecs.systems.shooting.shoot = [this] (std::string s, world_coords d, world_coords v, world_coords o, world_coords t, ecs::c_collision::flags a) {
        create_entity(egen_bullet, s, d, v, o, t, a);
    };

    window().swap_buffers(renderer());
    ecs._map_id = create_entity([&](entity, engine&){});
    ecs._player_id = create_entity([&](entity, engine&){});
    ecs.add<ecs::c_player>(ecs._player_id);
    ecs.add<ecs::c_mapdata>(map_id());

    ecs.systems.health.set_texture(textures().add("healthbar_atlas"));
    ecs.systems.text.set_texture(textures().add("text_atlas"));

    auto& inv = ecs.add<ecs::c_inventory>(player_id());
    ecs.add<ecs::c_display>(player_id());
    ecs.add<ecs::c_weapon_pool>(player_id());
    for(int i = 0; i < 36; i++) {
        u32 h = rand() % 3;
        if (h == 2) continue;
        inv.data.add(i, ecs::c_inventory::item{h, 0});
    }

    renderer().set_viewport(window().resolution());
    create_entity([&](entity e, engine& g){
        g.ecs.add<ecs::c_widget>(e);
        g.ui.root = e;
    });
}
