
#include <engine/engine.h>
#include "basic_entity_funcs.h"


void transfer_item(entity , engine& g, c_inventory&);
// Given a point, this function returns the grid square it resides in
point<u16> get_gridsquare(sprite& spr, point<f32> in) {
	point<f32> tl = spr.get_dimensions(0).origin;
	return point<u16>((in.x - tl.x ) / 64.0f, (in.y- tl.y) / 64.0f);
}

// Given a gridsquare, this function returns the top left coordinate of the grid square it resides in
point<f32> snap_to_grid(sprite& spr, point<u16> in) {
	point<f32> tl = spr.get_dimensions(0).origin;
	return point<f32>(in.x * 64 + tl.x, in.y * 64 + tl.y);
}


// Set the inventory to be the active cursor widget, and put the selection box in place of the clicked-on element.
void pickup_item(entity e, point<f32> box_snap_point, engine& g) {
    c_selection &select = g.ecs.get_component<c_selection>(e);
    sprite &spr = g.ecs.get_component<sprite>(e);
    g.ui.cursor = e;

    size <f32> item_size = spr.get_dimensions(select.box_index).size;
    spr.set_pos(box_snap_point, item_size, select.box_index, 0);
    spr.get_subsprite(select.box_index).layer = render_layers::ui;
}

void write_item_to_slot(size_t index, point<f32> pos, entity e, item item_in, engine& g) {
    sprite& spr = g.ecs.get_component<sprite>(e);
    c_selection &select = g.ecs.get_component<c_selection>(e);

    size<f32> item_size = spr.get_dimensions(select.box_index).size;
    spr.set_pos(pos, item_size, 1, index);

    size<f32> subtexture(1.0f / 2, 1);
    spr.set_uv( point<f32>(subtexture.w * item_in.ID, 0), subtexture, 1, index);
}

void empty_slot(size_t index, point<f32> pos, entity e, engine& g) {
    c_selection &select = g.ecs.get_component<c_selection>(e);
    sprite& spr = g.ecs.get_component<sprite>(e);

    spr.get_subsprite(select.box_index).layer = render_layers::null;
    spr.set_pos(pos, size(0.0f, 0.0f), 1, index);
}

void place_item(entity e, engine& g, c_selection& select,    c_inventory& inv,    sprite& spr) {

    g.ui.cursor = 65535;
    // box_index represents the position the currently held item was taken from
    point<u16> box_point = get_gridsquare(spr, spr.get_dimensions(select.box_index).origin);
    size_t box_index = box_point.x + (box_point.y * select.grid_size.w);

    item held = inv.data.get(box_index);
    empty_slot(box_index, snap_to_grid(spr, box_point), e, g);
    inv.data.remove(box_index);


    if (inv.data.exists(select.active_index())) {
        item highlight = inv.data.get(select.active_index());
        inv.data.remove(select.active_index());
        inv.data.add(box_index, highlight);
        write_item_to_slot(box_index, snap_to_grid(spr, select.active), e, highlight, g);
        pickup_item(e, snap_to_grid(spr, box_point), g);

    }

    inv.data.add(select.active_index(), held);
    write_item_to_slot(select.active_index(), snap_to_grid(spr, select.active), e, held, g);
}

bool inventory_mouseevents(entity e, const event& ev,  engine& g, c_inventory& inv) {

    if (ev.active_state()) return true;
 	c_selection& select = g.ecs.get_component<c_selection>(e);
	sprite& spr = g.ecs.get_component<sprite>(e);

	// There's no item "in hand", try and pick one up
	if (g.ui.cursor == 65535) {
		// There's nothing in the slot the mouse is hovering on, nothing can be done
		if (!inv.data.exists(select.active_index())) return true;
        pickup_item(e, snap_to_grid(spr, select.active), g);
	} else if (g.ui.cursor != e) {
	    entity parent = g.ecs.get_component<c_widget>(e).parent;
        transfer_item(parent, g, inv);
	} else {  // You already have an item, try to place or swap it.
        place_item(e, g, select, inv, spr);
	}
	return true;
}


bool inventory_motion(entity e, const event& ev, engine& game) {
    sprite& spr = game.ecs.get_component<sprite>(e);
    c_selection& select = game.ecs.get_component<c_selection>(e);

    if (game.ui.cursor == e) {
        size<f32> dim = spr.get_dimensions(select.box_index).size;
        point<f32> p (ev.pos.x - (dim.w / 2), ev.pos.y - (dim.h / 2));

        auto dimensions = spr.get_dimensions(select.box_index).origin;
        point<u16> box_point = get_gridsquare(spr, dimensions);
        size_t box_index = box_point.x + (box_point.y * select.grid_size.w);
        spr.set_pos(p, dim, 1, box_index);
    }


	//if (ev.active_state() == false)	w->flags.unset(widget::flags::pressed);
	point<u16> active = select.active;
	size_t index = active.x + (active.y * select.grid_size.w);
	spr.set_uv(point<f32>(0, 0), size<f32>(0.5, 1), 0, index);

	if (ev.active_state() == false) return true;

	active = get_gridsquare(spr, ev.pos);
	if (active.x >= select.grid_size.w || active.y >= select.grid_size.h) return true;

	index = active.x + (active.y * select.grid_size.w);
	select.active = active;
	spr.set_uv(point<f32>(0.5, 0), size<f32>(0.5, 1), 0, index);

	return true;
}

void inventory_init(entity e, engine& g, entity parent, c_inventory& inv, point<u16> origin) {
	g.ui.focus = e;
    g.ecs.add_component<c_widget>(e);
	make_parent(parent, e, g);
	c_selection& select = g.ecs.add_component<c_selection>(e);

	g.ecs.add_component<c_mouseevent>(e).add_event(inventory_mouseevents, g, inv);
	g.ecs.add_component<c_cursorevent>(e).add_event(inventory_motion, g);
	g.ecs.add_component<c_keyevent>(e).add_event(selection_keypress, g);

	select.grid_size = size<u16>(9, 4);
	size<f32> element_size(64, 64);
	rect<f32> dim(origin.to<f32>(),
			size<f32>(select.grid_size.w * element_size.w, select.grid_size.h * element_size.h));

	int num_grid_elements = select.grid_size.w * select.grid_size.h;
	sprite& spr = g.ecs.add_component<sprite>(e);
	spr.gen_subsprite(num_grid_elements, render_layers::ui  );
	spr.get_subsprite(0).tex = g.renderer().get_texture("highlight");

	int item_spr = spr.gen_subsprite(num_grid_elements, render_layers::ui);
	spr.get_subsprite(item_spr).tex = g.renderer().get_texture("items");


    select.box_index = spr.gen_subsprite(1, render_layers::null);
    spr.set_pos(point<f32>(0, 0), element_size, select.box_index, 0);
    spr.set_uv(point<f32>(0.5, 0), size<f32>(0.5, 1), select.box_index, 0);
    spr.get_subsprite(select.box_index).tex = g.renderer().get_texture("highlight");


	int index = 0;
	point<f32> pos = origin.to<f32>();
	for (u16 y = 0; y < select.grid_size.h; y++) {
		for (u16 x = 0; x < select.grid_size.w; x++) {
			spr.set_pos(pos, element_size, 0, index);
			spr.set_uv(point<f32>(0, 0), size<f32>(0.5, 1), 0, index);

			if (inv.data.exists(index)) {
                write_item_to_slot(index, pos, e, inv.data.get(index), g);
			}

			pos.x += element_size.w;
			index++;
		}
		pos.y += element_size.h;
		pos.x = 100;
	}
}

// transfer from A to B
void transfer_item(entity e, engine& g, c_inventory& b_inv) {
    c_widget& widget = g.ecs.get_component<c_widget>(e);
    entity a = g.ui.cursor;


    c_inventory& player_inv = g.ecs.get_component<c_inventory>(g.player_id());
    c_inventory& storage = g.ecs.get_component<c_inventory>(g.ui.active_interact);
    c_inventory& a_inv = &b_inv == &player_inv ? storage : player_inv;
    c_selection& a_select = g.ecs.get_component<c_selection>(a);
    sprite& a_spr = g.ecs.get_component<sprite>(a);
    point<u16> held_point = get_gridsquare(a_spr, a_spr.get_dimensions(a_select.box_index).origin);
    size_t held_index = held_point.x + (held_point.y * a_select.grid_size.w);

    item held = a_inv.data.get(held_index);
    empty_slot(held_index, snap_to_grid(a_spr, held_point), a, g);
    a_inv.data.remove(held_index);




    entity b = 65535;
    for (auto& child : widget.children)
        if (child != a) {
            b = child;
            break;
        }
    c_selection& b_select = g.ecs.get_component<c_selection>(b);
    sprite& b_spr = g.ecs.get_component<sprite>(b);



    g.ui.cursor = 65535;

    if (b_inv.data.exists(b_select.active_index())) {
        item highlight = b_inv.data.get(b_select.active_index());
        b_inv.data.remove(b_select.active_index());
        a_inv.data.add(held_index, highlight);
        write_item_to_slot(held_index, snap_to_grid(a_spr, a_select.active), a, highlight, g);
        pickup_item(a, snap_to_grid(a_spr, held_point), g);

    }


    b_inv.data.add(b_select.active_index(), held);
    write_item_to_slot(b_select.active_index(), snap_to_grid(b_spr, b_select.active), b, held, g);
}

void inv_transfer_init(entity e, engine& g, c_inventory& storage_inv) {
    g.ecs.add_component<c_widget>(e);
    g.command_states.set(command::toggle_inventory, true);
    g.create_entity(inventory_init, e, g.ecs.get_component<c_inventory>(g.player_id()), point<u16>(100, 100));
    g.create_entity(inventory_init, e, storage_inv, point<u16>(100, 400));
    g.ui.focus = e;
}