#include "ui_helper_funcs.h"


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



bool selection_keypress(entity e, const event& ev, engine& g) {
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


rect<f32> calc_size_percentages(rect<f32> parent, rect<f32> sizes ) {
    sprite_coords p_origin = parent.origin;
    sprite_coords p_size = parent.size;

    sprite_coords new_origin(((sizes.origin.x / 100.0f) * p_size.x) + p_origin.x,
                             ((sizes.origin.y / 100.0f) * p_size.y) + p_origin.y);
    sprite_coords new_size((p_size.x / 100) * sizes.size.x, ( p_size.y / 100) * sizes.size.y);

    return rect<f32>(new_origin, new_size);
}


bool follower_moveevent(entity e, const event& ev_in, ecs::c_display& spr) {
    auto& ev = dynamic_cast<const event_cursor&>(ev_in);
    sprite_coords dim = spr.get_dimensions().size;
    sprite_coords p (ev.pos().x - (dim.x / 2), ev.pos().y - (dim.y / 2));
    for (auto& sprite : spr) {
        sprite.move_to(p);
    }
    return true;
}