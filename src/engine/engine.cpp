#include "engine.h"
#include <common/parser.h>
#include "../main/basic_entity_funcs.h"



void engine::run_tick() {
	point<f32> start_pos = ecs.get_component<sprite>(player_id()).get_dimensions().origin;

	ecs.run_ecs();
	ui.active_interact = ecs.systems.proxinteract.active_interact;
    //renderer.set_texture_data()
    for (auto logic_func : logic.logic) {
        logic_func(*this);
    }

    int h = ui.hover_timer.elapsed<timer::ms>().count();
    if (h >= 1500 && ui.hover_active == false) {
        ui.hover_active = true;
        event a;
        ui.handle_hover(a, *this);
    }

    renderer.set_texture_data(ecs.systems.health.get_texture(), ecs.systems.health.get_data());

    renderer.set_camera(offset);
    renderer.clear_sprites();
	for (auto& sprite : ecs.components.get_pool(type_tag<sprite>())) {
        for (int i = 0; i < sprite.num_subsprites(); i++) {
            renderer.add_sprite(sprite.get_subsprite(i));
        }
	}

    point<f32> end_pos = ecs.get_component<sprite>(player_id()).get_dimensions().origin;
	offset += (end_pos - start_pos);
}


engine::engine()  : settings(), window(settings){

	ecs.systems.shooting.bullet_types.push_back(s_shooting::bullet{size<f32>(0.3, 0.6), point<f32>(8, 8), "bullet"});
	ecs.systems.shooting.shoot = [this] (std::string s, size<f32> d, point<f32> v, point<f32> o, point<f32> t, collision_flags a) {
		create_entity(egen_bullet, s, d, v, o, t, a);
	};

    ecs._map_id = create_entity([&](entity, engine&){});
	ecs._player_id = create_entity([&](entity, engine&){});

    ecs.add_component<c_mapdata>(map_id());


    ecs.systems.health.set_texture(renderer.add_texture("healthbar_atlas"));

    auto& inv = ecs.add_component<c_inventory>(player_id());
    ecs.add_component<sprite>(player_id());
    ecs.add_component<c_weapon_pool>(player_id());
    for(int i = 0; i < 36; i++) {
        u32 h = rand() % 3;
        if (h == 2) continue;
        inv.data.add(i, item{h, 0});
    }

	renderer.set_viewport(settings.resolution);
	create_entity([&](entity e, engine& g){
        g.ecs.add_component<c_widget>(e);
        g.ui.root = e;
    });
}

void engine::destroy_entity(entity e) {
    ecs.entities.mark_entity(e);
    // Recursively delete child widget entities
    if (ecs.component_exists<c_widget>(e)) {
        c_widget& w = ecs.get_component<c_widget>(e);
        for (auto child : w.children) destroy_entity(child);
    }
}



void engine::render() {
	renderer.render_layer(render_layers::sprites);
	renderer.render_layer(render_layers::ui);
	renderer.render_layer(render_layers::text);

	window.swap_buffers();
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
    resolution = size<u16>(list->get<int>(0), list->get<int>(1));

    int fullscreen =  d->get<int>("fullscreen");
    if (fullscreen != 0) flags.set(window_flags::fullscreen);
    if (fullscreen == 1) flags.set(window_flags::bordered_window);

    if (d->get<bool>("vsync")) flags.set(window_flags::vsync);

}

template <typename T>
bool test_range(T a, T b, T c) {
    return ((b > a) && (b < c));
}

bool test_collision(rect<f32> r, point<f32> p) {
    bool x = test_range(r.origin.x, p.x, r.bottom_right().x);
    bool y = test_range(r.origin.y, p.y, r.bottom_right().y);

    return ((x == true) && (y == true));
}

template <int type>
entity at_cursor(entity e, engine& g, point<f32> cursor) {
    entity next = 65535;
    c_widget& w = g.ecs.get_component<c_widget>(e);
    for (auto it = w.children.begin(); it != w.children.end(); it++) {
        if (!g.ecs.component_exists<event_wrapper<type>>(*it)) continue;
        if (test_collision(g.ecs.get_component<sprite>(*it).get_dimensions(0), cursor)) next = *it;
    }

    if (next == 65535) return e;
    return at_cursor<type>(next, g, cursor);
}

bool ui_manager::handle_button(event& e, engine& g) {
    hover_timer.start();
    entity dest = at_cursor<0>(root, g, e.pos);
    bool success = false;
    if (dest != 65535 && dest != root)
        success = g.ecs.get_component<c_mouseevent>(dest).run_event(e);
    if (success) return true;

    point<f32> h (e.pos.x / 64.0f, e.pos.y / 64.0f);
    if (e.active_state() && g.in_dungeon) {
        c_player& p = g.ecs.get_component<c_player>(g.player_id());
        p.shoot = true;
        p.target = h + g.offset;
        return true;
    }
    return false;
}

bool ui_manager::handle_keypress(event& e, engine& g) {
    auto it = g.settings.bindings.find(e.ID);
    if (it == g.settings.bindings.end()) {
        if (focus == 65535) return false;
        return g.ecs.get_component<c_keyevent>(focus).run_event(e);
    }

    command command = it->second;
    switch (command) {
        case command::interact: {
            if (!e.active_state())  return false;
                entity active = g.ui.active_interact;
            if (active == 65535)  return false;
                return g.ecs.get_component<c_keyevent>(active).run_event(e);
        }
        case command::toggle_inventory: {
            if (!e.active_state()) return false;
            bool toggle_state = g.command_states.test(command::toggle_inventory) == true;
            if (toggle_state) {
                g.destroy_entity(g.ui.focus);
                g.ui.focus = 65535;
                g.ui.cursor = 65535;
            }
            else
                g.create_entity(inventory_init, g.ui.root, g.ecs.get_component<c_inventory>(g.player_id()), point<u16>(100, 100));
            g.command_states.set(command::toggle_inventory, !toggle_state);
            return false;
        }
        default: {  // Semantically, using the default case for movement is wrong, but I don't want to chain all four movement cases together
            g.command_states.set(command, e.active_state());
            point<f32>& velocity = g.ecs.get_component<c_velocity>(g.player_id()).delta;
            velocity.x = 0.05 * (g.command_states.test(command::move_right) - g.command_states.test(command::move_left));
            velocity.y = 0.05 * (g.command_states.test(command::move_down) - g.command_states.test(command::move_up));
            return true;
        }
    }
}

bool ui_manager::handle_cursor(event& e, engine& g) {
    hover_timer.start();
    if (cursor != 65535) g.ecs.get_component<c_cursorevent>(cursor).run_event(e);

    entity dest = at_cursor<1>(root, g, e.pos);
    entity start = at_cursor<1>(root, g, last_position);

    last_position = e.pos;

    if (start == root && dest == root) return false;
    if (start == dest) return g.ecs.get_component<c_cursorevent>(start).run_event(e);

    if (dest != 65535 && dest != root) return g.ecs.get_component<c_cursorevent>(dest).run_event(e);
    e.set_active(false);
    if (start != 65535 && start != root) return g.ecs.get_component<c_cursorevent>(start).run_event(e);

    return false;
}

bool ui_manager::handle_hover(event& e, engine& g) {

    entity dest = at_cursor<3>(root, g, last_position);
    if (dest == 65535 || dest == root) return false;
    return g.ecs.get_component<c_hoverevent>(dest).run_event(e);
}

bool engine::process_events() {
    event e;
    while (window.poll_events(e)) {
        if (e.type() == terminate) return false;
        if (e.type() == event_flags::null) continue;

        ui.hover_timer.start();
        ui.hover_active = false;

        if (e.type() == event_flags::button_press)
            ui.handle_button(e, *this);
        else if (e.type() == event_flags::key_press)
            ui.handle_keypress(e, *this);
        else if (e.type() == event_flags::cursor_moved)
            ui.handle_cursor(e, *this);

    }
    return true;
}