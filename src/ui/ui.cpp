#include <engine/engine.h>
#include "ui.h"

rect<f32> resize_quad(rect<f32> r, f32 scale_factor, rect<f32> old_parent_rect, rect<f32> new_parent_rect) {
    sprite_coords new_size = r.size * scale_factor;
    sprite_coords position_ratio = (r.center() - old_parent_rect.origin) / old_parent_rect.size;
    sprite_coords new_origin = (new_parent_rect.size * position_ratio) - (new_size / 2) + new_parent_rect.origin;
    return rect<f32>(new_origin, new_size);
}
void resize_widget(entity e, engine& g, f32 scale_factor, rect<f32> old_parent_rect, rect<f32> new_parent_rect) {
    auto& display = g.ecs.get<ecs::c_display>(e);
    auto& widget = g.ecs.get<ecs::c_widget>(e);

    // calculate a rectangle grid to fit the new bounds of this widget
    rect<f32> old_rect = display.sprites(0).get_dimensions();
    sprite_coords new_size = old_rect.size * scale_factor;
    sprite_coords pos_ratio = old_rect.origin / (old_parent_rect.size - old_rect.size);
    sprite_coords new_origin = (new_parent_rect.size - new_size) * pos_ratio;
    rect<f32> new_rect(new_origin, new_size);
    // iterate through every quad, resizing and repositioning it using ratios relative to new widget rectangle
    for (auto& sprite : display) {
        for (int i = 0; i < sprite.num_quads(); i++) {
            rect<f32> r = resize_quad(sprite.get_dimensions(i), scale_factor, old_rect, new_rect);
            sprite.set_pos(r.origin, r.size, i);
        }
    }
    for (auto& child : widget.children) {
        resize_widget(child, g, scale_factor, old_rect, new_rect);
    }
}

void resize_ui(engine& g, f32 scale_factor) {
    auto& widget = g.ecs.get<ecs::c_widget>(g.ui.root);
    rect<f32> thing(sprite_coords(0,0), sprite_coords(1920, 1080));
    for (auto& child : widget.children) {
        resize_widget(child, g, scale_factor, thing, thing);
    }
}

int cursorpos_to_navindex(entity e, engine& g, sprite_coords ev_pos) {
    auto& select = g.ecs.get<ecs::c_selection>(e);
    rect<f32> grid_entry = g.ecs.get<ecs::c_display>(e).sprites(0).get_dimensions(0);
    point<u16> cursor_focus = [&] () {;
        if (!test_collision(g.ecs.get<ecs::c_display>(e).sprites(0).get_dimensions(), ev_pos)) {
            return point<u16>(65535, 65535);
        } else {
            return ((ev_pos - grid_entry.origin) / grid_entry.size).to<u16>();
        }
    }();
    int focus_index = project_to_1D(cursor_focus, select.grid_size.x);

    if (g.ecs.exists<ecs::c_checkbox>(e)) {
        return focus_index;
    } else if (g.ecs.exists<ecs::c_slider>(e)) {
        if (g.ui.cursor == e) {
            return ev_pos.x;
        } else {
            return focus_index;
        }
    } else if (g.ecs.exists<ecs::c_button>(e)) {
        auto& button = g.ecs.get<ecs::c_button>(e);
        auto& select = g.ecs.get<ecs::c_selection>(e);
        if (focus_index < select.num_elements()) {
            if (test_collision(g.ecs.get<ecs::c_display>(e).sprites(button.sprite_index).get_dimensions(focus_index), ev_pos)) {
                return focus_index;
            } else {
                return 65535;
            }
        }
    } else if (g.ecs.exists<ecs::c_dropdown>(e)) {
        auto& dropdown = g.ecs.get<ecs::c_dropdown>(e);
        auto& select = g.ecs.get<ecs::c_selection>(e);
        if (focus_index < select.num_elements()) {
            if (test_collision(g.ecs.get<ecs::c_display>(e).sprites(dropdown.sprite_index).get_dimensions(focus_index), ev_pos)) {
                return focus_index;
            } else {
                return 65535;
            }
        }
    } else {
        return focus_index;
    }
    return 65535;
}
bool send_navevent(entity e, engine& g, int new_index) {
    g.ui.hover_timer.start();
    auto& w = g.ecs.get<ecs::c_widget>(e);
    auto& select = g.ecs.get<ecs::c_selection>(e);

    if (g.ecs.exists<ecs::c_checkbox>(e)) {
        checkbox_navigation(e, g, new_index);
        selectiongrid_navigation(e, g, new_index);
    } else if (g.ecs.exists<ecs::c_slider>(e)) {
        if (g.ui.cursor == e) {
            slider_navigation(e, g, new_index);
        } else {
            selectiongrid_navigation(e, g, new_index);
        }
    } else if (g.ecs.exists<ecs::c_button>(e)) {
        button_navigation(e, g, new_index);
    } else if (g.ecs.exists<ecs::c_dropdown>(e)) {
        dropdown_navigation(e, g, new_index);
    } else {
        w.on_navigate(e, g, new_index);
    }
    return true;
}



void keyevent_to_navevent(entity e, engine& g, int x_delta, int y_delta, bool release) {
    g.ui.last_position = screen_coords(65535, 65535);
    if (release || e == g.ui.root || e == 65535) return;
    auto& select = g.ecs.get<ecs::c_selection>(e);
    int new_index = project_to_1D(select.highlight + point<u16>(x_delta, y_delta), select.grid_size.x);
    send_navevent(e, g, new_index);
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

bool send_actionevent(entity e, engine& g, bool release) {
    if (e != 65535 && e != g.ui.root) {
        g.ui.hover_timer.start();
        g.ecs.get<ecs::c_widget>(e).on_activate(e, g, release);
        return true;
    }
    return false;
}

bool handle_button(input_event& e_in, engine& g) {
    auto& ev = dynamic_cast<event_mousebutton&>(e_in);
    g.ui.hover_timer.start();

    entity dest = at_cursor<event_mousebutton::id>(g.ui.root, g, ev.pos());
    entity held = g.ui.cursor;
    if (g.ecs.get<ecs::c_widget>(dest).accepts_textinput) {
        screen_coords position = g.ui.last_position - g.ecs.get<ecs::c_display>(dest).sprites(0).get_dimensions().origin.to<u16>();
        textinput_navigation(dest, g, g.ecs.systems.text.character_at_position(g.ecs.get<ecs::c_text>(dest).text_entries.begin()->text, position));
        return true;
    }
    while (g.ecs.get<ecs::c_widget>(dest).on_activate == nullptr) {
        dest = g.ecs.get<ecs::c_widget>(dest).parent;
        if (dest == g.ui.root) break;
    }
    if (ev.release()) {
        g.ui.cursor = 65535;
    } else {
        g.ui.cursor = dest;
    }
    bool status;

        status = send_actionevent(dest, g, ev.release());

    // send a nav event when releasing the cursor away from the held entity
    if (ev.release() && held != dest && held != 65535) {
        ecs::c_widget::activation_action function = g.ecs.get<ecs::c_widget>(held).on_activate;
        if (function != nullptr) {
            function(held, g, true);
        }
        send_navevent(held, g, cursorpos_to_navindex(held, g, g.ui.last_position.to<f32>()));
    }

    if (status) return true;
    world_coords h (ev.pos().x / 64.0f, ev.pos().y / 64.0f);
    if (ev.release() && g.in_dungeon) {
        ecs::c_player& p = g.ecs.get<ecs::c_player>(g.player_id());
        p.shoot = true;
        p.target = h + g.offset;
        return true;
    }
    return false;
}

bool handle_keypress(input_event& e_in, engine& g) {
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
            g.ecs.get<ecs::c_widget>(active).on_activate(active, g, e.release());
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
        case command::nav_left:
            keyevent_to_navevent(g.ui.focus, g, -1, 0, e.release());
            return true;
        case command::nav_right:
            keyevent_to_navevent(g.ui.focus, g, 1, 0, e.release());
            return true;
        case command::nav_up:
            keyevent_to_navevent(g.ui.focus, g, 0, -1, e.release());
            return true;
        case command::nav_down:
            keyevent_to_navevent(g.ui.focus, g, 0, 1, e.release());
            return true;
        case command::nav_activate:
            g.ui.last_position = screen_coords(65535, 65535);
            send_actionevent(g.ui.focus, g, e.release());
        case command::text_backspace:
            if (!e.release()) return false;
            if (g.ecs.get<ecs::c_widget>(g.ui.focus).accepts_textinput == true) {
                std::string thing = " ";
                thing[0] = 0x08;
                textinput_event(g.ui.focus, g, thing);
            }
    }
    return true;
}



bool handle_cursor(input_event& e_in, engine& g) {
    auto& e = dynamic_cast<event_cursor &>(e_in);
    //if (g.ui.cursor != 65535) g.ecs.get<ecs::c_event_callbacks>(g.ui.cursor).run_event(e);

    entity dest = at_cursor<event_cursor::id>(g.ui.root, g, e.pos());
    entity start = at_cursor<event_cursor::id>(g.ui.root, g, g.ui.last_position);
    if (start == g.ui.root && dest == g.ui.root) return false;


    g.ui.last_position = e.pos();
    if (start == dest)  {
        if (g.ecs.get<ecs::c_widget>(dest).accepts_textinput) return true;
        send_navevent(dest, g, cursorpos_to_navindex(dest, g, e.pos().to<f32>()));
        return true;
    }
    if (start != 65535 && start != g.ui.root) {
        send_navevent(start, g, cursorpos_to_navindex(start, g, e.pos().to<f32>()));
    }
    // The cursor is switching between entities, give focus transition events
    if (dest != 65535 && dest != g.ui.root) {
        if (g.ecs.get<ecs::c_widget>(dest).accepts_textinput) return true;
        send_navevent(dest, g, cursorpos_to_navindex(dest, g, e.pos().to<f32>()));
    }

    return true;
}

bool handle_hover(input_event& e, engine& g) {
    entity dest = at_cursor<3>(g.ui.root, g, g.ui.last_position);
    if (dest == 65535 || dest == g.ui.root) return false;
    //return g.ecs.get<ecs::c_event_callbacks>(dest).run_event(e);
}

bool handle_textinput(input_event& ev_in, engine& g) {
    auto& ev = dynamic_cast<event_textinput&>(ev_in);
    if (g.ecs.get<ecs::c_widget>(g.ui.focus).accepts_textinput) {
        textinput_event(g.ui.focus, g, ev.text());
    }
    return true;
}

bool process_event(input_event& e, engine& g) {
    g.ui.hover_timer.start();
    g.ui.hover_active = false;

    switch (e.event_id()) {
        case event_keypress::id:
            return handle_keypress(e, g);
        case event_mousebutton::id:
            return handle_button(e, g);
        case event_cursor::id:
            return handle_cursor(e, g);
        case event_textinput::id:
            return handle_textinput(e, g);
        default:
            return true;
    }
}















point<u16> get_gridindex(rect<f32> grid, size<u16> num_elements, point<f32> pos) {
    sprite_coords element_size = grid.size / num_elements.to<f32>();
    return ((pos - grid.origin) / element_size).to<u16>();
}

// remove B from A's children list
void remove_child(ecs::c_widget& w, entity b) {
    for (auto it =  w.children.begin(); it !=  w.children.end(); it++) {
        if (*it == b) {
            // Constant-time delete optimization since order is meaningless,
            std::swap(* w.children.rbegin(), *it);
            w.children.pop_back();
            return;
        }
    }
}

// Make A a child of B
void make_parent(entity a, entity b, engine& game) {
    ecs::c_widget& w_a = game.ecs.get<ecs::c_widget>(a);
    ecs::c_widget& w_b = game.ecs.get<ecs::c_widget>(b);

    if (w_b.parent != 65535)  remove_child(w_b, b);

    w_a.children.emplace_back(b);
    w_b.parent = a;
}
void make_widget(entity e, engine& g, entity parent) {
    g.ecs.add<ecs::c_widget>(e);
    make_parent(parent, e, g);
}



bool selection_keypress(entity e, const input_event& ev, engine& g) {
    /*ecs::c_selection& select = g.ecs.get<ecs::c_selection>(e);
    if (ev.active_state() == false) return false;
    // std::min is a fuckwad!
    //int  real_zero = 0;

    bool movement = false;

    point<u16> new_active = select.active;
    switch(ev.ID) {
    case SDLK_LEFT:
        new_active.x = std::max(static_cast<int>(select.active.x - 1), real_zero);
        movement = true;
        break;
    case SDLK_RIGHT:
        new_active.x = std::min(select.active.x + 1, select.grid_size.w - 1);
        movement = true;
        break;
    case SDLK_UP:
        new_active.y = std::max(static_cast<int>(select.active.y - 1), real_zero);
        movement = true;
        break;
    case SDLK_DOWN:
        new_active.y = std::min(select.active.y + 1, select.grid_size.y - 1);
        movement = true;
        break;

    case SDLK_RETURN: {
        c_mouseevent& mve = g.ecs.get<c_mouseevent>(e);
        event ev;
        ev.set_type(event_flags::button_press);
        ev.set_active(true);
        mve.run_event(ev);
        ev.set_ai ctive(false);
        mve.run_event(ev);
    }
    }

    if (movement) {
        ecs::c_display& spr = g.ecs.get<ecs::c_display>(e);
        ecs::c_cursorevent& cve = g.ecs.get<ecs::c_cursorevent>(e);
        u16 x_pos = (new_active.x * 64) + spr.get_dimensions().origin.x;
        u16 y_pos = (new_active.y * 64) + spr.get_dimensions().origin.y;

        event e;
        e.set_active(true);
        e.set_type(event_flags::cursor_moved);
        e.pos = screen_coords(x_pos, y_pos);
        cve.run_event(e);

    }
    //set_box(select);*/
    return true;
}

