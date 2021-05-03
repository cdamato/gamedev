#include "engine.h"
#include <common/parser.h>
#include "../main/basic_entity_funcs.h"


void engine::run_tick() {
    world_coords start_pos = ecs.get<c_display>(player_id()).get_dimensions().origin;

    ecs.run_ecs(settings.framerate_multiplier);
    ui.active_interact = ecs.systems.proxinteract.active_interact;
    //renderer.set_texture_data()
    for (auto logic_func : logic.logic) {
        logic_func(*this);
    }

    int h = ui.hover_timer.elapsed<timer::ms>().count();
    if (h >= 1500 && ui.hover_active == false) {
        ui.hover_active = true;
        event a;
        //handle_hover(a, *this);
    }

    renderer().update_texture_data(ecs.systems.health.get_texture());

    renderer().set_camera(offset);
    renderer().clear_sprites();
    sprites.clear();
    for (auto& sprite : ecs.components.get_pool(type_tag<c_display>())) {
        for (int i = 0; i < sprite.num_sprites(); i++) {
            if (sprite.sprites(i).layer == render_layers::null) continue;
            sprites.emplace(sprite.sprites(i));
        }
    }

    world_coords end_pos = ecs.get<c_display>(player_id()).get_dimensions().origin;
    offset += (end_pos - start_pos);
}


void engine::destroy_entity(entity e) {
    ecs.entities.mark_entity(e);
    // Recursively delete child widget entities
    if (ecs.exists<c_widget>(e)) {
        c_widget& w = ecs.get<c_widget>(e);
        for (auto child : w.children) destroy_entity(child);
    }
}

void engine::render() {
    renderer().clear_screen();
    renderer().render_layer(sprites);
    window.swap_buffers(renderer());
}

void logic_manager::add(logic_func func) {
    logic.push_back(func);
}

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
    bindings['w'] = command::move_up;
    bindings['a'] = command::move_left;
    bindings['s'] = command::move_down;
    bindings['d'] = command::move_right;
    bindings['e'] = command::interact;
    bindings['\t'] = command::toggle_inventory;

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


template <int type>
entity at_cursor(entity e, engine& g, screen_coords cursor) {
    entity next = 65535;
    c_widget& w = g.ecs.get<c_widget>(e);
    for (auto it = w.children.begin(); it != w.children.end(); it++) {
        if (!g.ecs.exists<event_wrapper<type>>(*it)) continue;
        if (test_collision(g.ecs.get<c_display>(*it).get_dimensions(), cursor.to<f32>())) next = *it;
    }

    if (next == 65535) return e;
    return at_cursor<type>(next, g, cursor);
}

bool handle_button(event& e, engine& g) {
    g.ui.hover_timer.start();
    entity dest = at_cursor<0>(g.ui.root, g, e.pos);
    bool success = false;
    if (dest != 65535 && dest != g.ui.root)
        success = g.ecs.get<c_mouseevent>(dest).run_event(e);
    if (success) return true;

    world_coords h (e.pos.x / 64.0f, e.pos.y / 64.0f);
    if (e.active_state() && g.in_dungeon) {
        c_player& p = g.ecs.get<c_player>(g.player_id());
        p.shoot = true;
        p.target = h + g.offset;
        return true;
    }
    return false;
}

bool handle_keypress(event& e, engine& g) {
    auto it = g.settings.bindings.find(e.ID);
    if (it == g.settings.bindings.end()) {
        if (g.ui.focus == 65535) return false;
        return g.ecs.get<c_keyevent>(g.ui.focus).run_event(e);
    }

    command command = it->second;
    switch (command) {
        case command::interact: {
            if (!e.active_state())  return false;
            entity active = g.ui.active_interact;
            if (active == 65535)  return false;
            return g.ecs.get<c_keyevent>(active).run_event(e);
        }
        case command::toggle_inventory: {
            if (!e.active_state()) return false;
            bool toggle_state = g.command_states.test(command::toggle_inventory) == true;
            if (toggle_state) {
                g.destroy_entity(g.ui.focus);
                g.ui.focus = 65535;
                g.ui.cursor = 65535;
            } else {
                g.create_entity(inventory_init, g.ui.root, g.ecs.get<c_inventory>(g.player_id()), point<u16>(100, 100));
            }
            g.command_states.set(command::toggle_inventory, !toggle_state);
            return false;
        }
        default: {  // Semantically, using the default case for movement is wrong, but I don't want to chain all four movement cases together
            g.command_states.set(command, e.active_state());
            world_coords& velocity = g.ecs.get<c_velocity>(g.player_id()).delta;
            velocity.x = 0.10 * (g.command_states.test(command::move_right) - g.command_states.test(command::move_left));
            velocity.y = 0.10 * (g.command_states.test(command::move_down) - g.command_states.test(command::move_up));
            return true;
        }
    }
}

bool handle_cursor(event& e, engine& g) {
    g.ui.hover_timer.start();
    if (g.ui.cursor != 65535) g.ecs.get<c_cursorevent>(g.ui.cursor).run_event(e);

    entity dest = at_cursor<1>(g.ui.root, g, e.pos);
    entity start = at_cursor<1>(g.ui.root, g, g.ui.last_position);

    g.ui.last_position = e.pos;

    if (start == g.ui.root && dest == g.ui.root) return false;
    if (start == dest) return g.ecs.get<c_cursorevent>(start).run_event(e);

    if (dest != 65535 && dest != g.ui.root) return g.ecs.get<c_cursorevent>(dest).run_event(e);
    e.set_active(false);
    if (start != 65535 && start != g.ui.root) return g.ecs.get<c_cursorevent>(start).run_event(e);

    return false;
}

bool handle_hover(event& e, engine& g) {
    entity dest = at_cursor<3>(g.ui.root, g, g.ui.last_position);
    if (dest == 65535 || dest == g.ui.root) return false;
    return g.ecs.get<c_hoverevent>(dest).run_event(e);
}



bool process_event(event& e, engine& g) {
    if (e.type() == event_flags::null) return true;

    g.ui.hover_timer.start();
    g.ui.hover_active = false;

    if (e.type() == event_flags::button_press)
        handle_button(e, g);
    else if (e.type() == event_flags::key_press)
        handle_keypress(e, g);
    else if (e.type() == event_flags::cursor_moved)
        handle_cursor(e, g);
    return true;
}




bool engine::process_events() {
    return window.poll_events();
}

engine::engine()  : settings(), window(settings){

    printf("Window Initialized\n");
    window.set_event_callback([this](event& ev) { process_event(ev, *this); });
    window.set_resize_callback([this](screen_coords size) { renderer().set_viewport(size); });
#ifdef OPENGL
    if (settings.flags.test(window_flags::use_software_render)) {
        _renderer = std::unique_ptr<renderer_base>(new renderer_software(settings.resolution));
    } else {
        _renderer = std::unique_ptr<renderer_base>(new renderer_gl);
    }
#else
    _renderer = std::unique_ptr<renderer_base>(new renderer_software(settings.resolution));
#endif //OPENGL

    ecs.systems.shooting.bullet_types.push_back(s_shooting::bullet{world_coords(0.3, 0.6), world_coords(0.25, 0.25), "bullet"});
    ecs.systems.shooting.shoot = [this] (std::string s, world_coords d, world_coords v, world_coords o, world_coords t, collision_flags a) {
        create_entity(egen_bullet, s, d, v, o, t, a);
    };

    window.swap_buffers(renderer());
    ecs._map_id = create_entity([&](entity, engine&){});
    ecs._player_id = create_entity([&](entity, engine&){});
    ecs.add<c_player>(ecs._player_id);
    ecs.add<c_mapdata>(map_id());


    ecs.systems.health.set_texture(renderer().add_texture("healthbar_atlas"));

    auto& inv = ecs.add<c_inventory>(player_id());
    ecs.add<c_display>(player_id());
    ecs.add<c_weapon_pool>(player_id());
    for(int i = 0; i < 36; i++) {
        u32 h = rand() % 3;
        if (h == 2) continue;
        inv.data.add(i, item{h, 0});
    }

    renderer().set_viewport(settings.resolution);
    create_entity([&](entity e, engine& g){
        g.ecs.add<c_widget>(e);
        g.ui.root = e;
    });
}
