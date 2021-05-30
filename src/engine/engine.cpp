#include "engine.h"
#include <common/parser.h>
#include <world/basic_entity_funcs.h>


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
        //event a;
        //handle_hover(a, *this);
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

void engine::render() {
    display.render();
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
    bindings['o'] = command::toggle_options_menu;

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
    ecs::c_widget& w = g.ecs.get<ecs::c_widget>(e);
    for (auto it = w.children.begin(); it != w.children.end(); it++) {
        if (g.ecs.get<ecs::c_display>(*it).sprites(0).layer == render_layers::sprites)
            continue;
        if (test_collision(g.ecs.get<ecs::c_display>(*it).get_dimensions(), cursor.to<f32>()))
            next = *it;
    }

    if (next == 65535) return e;
    return at_cursor<type>(next, g, cursor);
}

bool handle_button(event& e_in, engine& g) {
    auto& ev = dynamic_cast<event_mousebutton&>(e_in);
    if (!ev.release()) return false;
    g.ui.hover_timer.start();

    entity dest = at_cursor<event_mousebutton::id>(g.ui.root, g, ev.pos());
    while (g.ecs.get<ecs::c_widget>(dest).on_activate == nullptr) {
        dest = g.ecs.get<ecs::c_widget>(dest).parent;
        if (dest == g.ui.root) break;
    }
    if (dest != 65535 && dest != g.ui.root) {

        g.ecs.get<ecs::c_widget>(dest).on_activate(dest, g);
        return true;
    }


    world_coords h (ev.pos().x / 64.0f, ev.pos().y / 64.0f);
    if (ev.release() && g.in_dungeon) {
        ecs::c_player& p = g.ecs.get<ecs::c_player>(g.player_id());
        p.shoot = true;
        p.target = h + g.offset;
        return true;
    }
    return false;
}

bool handle_keypress(event& e_in, engine& g) {
    auto& e = dynamic_cast<event_keypress&>(e_in);
    auto it = g.settings.bindings.find(e.key_id());
    if (it == g.settings.bindings.end()) {
        if (g.ui.focus == 65535) return false;
        return false;
        // return g.ecs.get<ecs::c_event_callbacks>(g.ui.focus).run_event(e);
    }

    command command = it->second;
    switch (command) {
        case command::interact: {
            if (e.release())  return false;
            entity active = g.ui.active_interact;
            if (active == 65535)  return false;
            g.ecs.get<ecs::c_widget>(active).on_activate(active, g);
            break;
        }
        case command::toggle_inventory: {
            if (e.release()) return false;
            bool toggle_state = g.command_states.test(command::toggle_inventory) == true;
            if (toggle_state) {
                g.destroy_entity(g.ui.focus);
                g.ui.focus = 65535;
                g.ui.cursor = 65535;
            } else {
                g.create_entity(inventory_init, g.ui.root, g.ecs.get<ecs::c_inventory>(g.player_id()), point<u16>(100, 100));
            }
            g.command_states.set(command::toggle_inventory, !toggle_state);
            return false;
        }
        case command::toggle_options_menu: {
            if (e.release()) return false;
            bool toggle_state = g.command_states.test(command::toggle_options_menu) == true;
            if (toggle_state) {
                g.destroy_entity(g.ui.focus);
                g.ui.focus = 65535;
                g.ui.cursor = 65535;
            } else {
                g.create_entity(options_menu_init, g.ui.root, point<u16>(100, 100));
            }
            g.command_states.set(command::toggle_options_menu, !toggle_state);
            return false;
        }
        case command::move_left:
            g.ecs.get<ecs::c_velocity>(g.player_id()).delta.x += e.release() ? 0.10 : -0.10;
            return true;
        case command::move_right:
            g.ecs.get<ecs::c_velocity>(g.player_id()).delta.x -= e.release() ? 0.10 : -0.10;
            return true;
        case command::move_up:
            g.ecs.get<ecs::c_velocity>(g.player_id()).delta.y += e.release() ? 0.10 : -0.10;
            return true;
        case command::move_down:
            g.ecs.get<ecs::c_velocity>(g.player_id()).delta.y -= e.release() ? 0.10 : -0.10;
            return true;
    }
    return true;
}


bool cursorevent_to_navevent(entity e, event_cursor& ev, engine& g) {
    auto& w = g.ecs.get<ecs::c_widget>(e);
    if (w.on_navigate == nullptr) return false;
    if (g.ecs.exists<ecs::c_selection>(e)) {
        auto& select = g.ecs.get<ecs::c_selection>(e);
        rect<f32> grid_dimensions = g.ecs.get<ecs::c_display>(e).sprites(0).get_dimensions();
        point<f32> item_size = grid_dimensions.size / select.grid_size.to<f32>();
        point<u16> new_active = ((ev.pos().to<f32>() - grid_dimensions.origin) / item_size).to<u16>();
        w.on_navigate(e, g, project_to_1D(select.active, select.grid_size.x), project_to_1D(new_active, select.grid_size.x));
    }

    return true;
}

bool handle_cursor(event& e_in, engine& g) {
    auto& e = dynamic_cast<event_cursor &>(e_in);
    g.ui.hover_timer.start();
    //if (g.ui.cursor != 65535) g.ecs.get<ecs::c_event_callbacks>(g.ui.cursor).run_event(e);

    entity dest = at_cursor<event_cursor::id>(g.ui.root, g, e.pos());
    entity start = at_cursor<event_cursor::id>(g.ui.root, g, g.ui.last_position);
    if (start == g.ui.root && dest == g.ui.root) return false;


    g.ui.last_position = e.pos();
    if (start == dest)  {
        cursorevent_to_navevent(start, e, g);
        return true;
    }
    // The cursor is switching between entities, give focus transition events
    if (dest != 65535 && dest != g.ui.root) {
        cursorevent_to_navevent(dest, e, g);
    }
    if (start != 65535 && start != g.ui.root) {
        cursorevent_to_navevent(start, e, g);
    }

    return true;
}

bool handle_hover(event& e, engine& g) {
    entity dest = at_cursor<3>(g.ui.root, g, g.ui.last_position);
    if (dest == 65535 || dest == g.ui.root) return false;
    //return g.ecs.get<ecs::c_event_callbacks>(dest).run_event(e);
}



bool process_event(event& e, engine& g) {
    g.ui.hover_timer.start();
    g.ui.hover_active = false;

    switch (e.event_id()) {
        case event_keypress::id:
            return handle_keypress(e, g);
        case event_mousebutton::id:
            return handle_button(e, g);
        case event_cursor::id:
            return handle_cursor(e, g);
        default:
            return true;
    }
}




bool engine::process_events() {
    return window().poll_events();
}

engine::engine() {

    display.initialize(display::display_manager::display_types(settings.flags.test(window_flags::use_software_render)), settings.resolution);
    printf("Window Initialized\n");
    window().event_callback = [this](event& ev) { process_event(ev, *this); };
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

    renderer().set_viewport(settings.resolution);
    create_entity([&](entity e, engine& g){
        g.ecs.add<ecs::c_widget>(e);
        g.ui.root = e;
    });
}
