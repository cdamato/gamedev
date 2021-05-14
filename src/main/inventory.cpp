
#include <engine/engine.h>
#include "basic_entity_funcs.h"


void transfer_item(entity , engine& g, c_inventory&);
// Given a point, this function returns the grid square it resides in
point<u16> get_gridsquare(c_display& spr, point<f32> in) {
    sprite_coords tl = spr.sprites(0).get_dimensions().origin;
	return point<u16>((in.x - tl.x ) / 64.0f, (in.y- tl.y) / 64.0f);
}

// Given a gridsquare, this function returns the top left coordinate of the grid square it resides in
sprite_coords snap_to_grid(c_display& spr, point<u16> in) {
    sprite_coords tl = spr.sprites(0).get_dimensions().origin;
	return sprite_coords(in.x * 64 + tl.x, in.y * 64 + tl.y);
}


// Set the inventory to be the active cursor widget, and put the selection box in place of the clicked-on element.
void pickup_item(entity e, sprite_coords box_snap_point, engine& g) {
    c_selection &select = g.ecs.get<c_selection>(e);
    c_display& spr = g.ecs.get<c_display>(e);
    g.ui.cursor = e;

    size <f32> item_size = spr.sprites(select.box_index).get_dimensions().size;
    spr.sprites(select.box_index).set_pos(box_snap_point, item_size, 0);
    spr.sprites(select.box_index).layer = render_layers::ui;
}

void write_item_to_slot(size_t index, sprite_coords pos, entity e, c_inventory::item item_in, engine& g) {
    c_display& spr = g.ecs.get<c_display>(e);
    c_selection &select = g.ecs.get<c_selection>(e);

    sprite_coords item_size = spr.sprites(select.box_index).get_dimensions().size;
    spr.sprites(1).set_pos(pos, item_size, index);

    spr.sprites(1).set_tex_region(item_in.ID, index);
}

void empty_slot(size_t index, sprite_coords pos, entity e, engine& g) {
    c_selection &select = g.ecs.get<c_selection>(e);
    c_display& spr = g.ecs.get<c_display>(e);

    spr.sprites(select.box_index).layer = render_layers::null;
    spr.sprites(1).set_pos(pos, size(0.0f, 0.0f), index);
}

void place_item(entity e, engine& g, c_selection& select,    c_inventory& inv,    c_display& spr) {

    g.ui.cursor = 65535;
    // box_index represents the position the currently held item was taken from
    point<u16> box_point = get_gridsquare(spr, spr.sprites(select.box_index).get_dimensions().origin);
    size_t box_index = box_point.x + (box_point.y * select.grid_size.x);

    c_inventory::item held = inv.data.get(box_index);
    empty_slot(box_index, snap_to_grid(spr, box_point), e, g);
    inv.data.remove(box_index);


    if (inv.data.exists(select.active_index())) {
        c_inventory::item highlight = inv.data.get(select.active_index());
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
 	c_selection& select = g.ecs.get<c_selection>(e);
	c_display& spr = g.ecs.get<c_display>(e);

	// There's no item "in hand", try and pick one up
	if (g.ui.cursor == 65535) {
		// There's nothing in the slot the mouse is hovering on, nothing can be done
		if (!inv.data.exists(select.active_index())) return true;
        pickup_item(e, snap_to_grid(spr, select.active), g);
	} else if (g.ui.cursor != e) {
	    entity parent = g.ecs.get<c_widget>(e).parent;
        transfer_item(parent, g, inv);
	} else {  // You already have an item, try to place or swap it.
        place_item(e, g, select, inv, spr);
	}
	return true;
}


bool inventory_motion(entity e, const event& ev, engine& game) {
    c_display& spr = game.ecs.get<c_display>(e);
    c_selection& select = game.ecs.get<c_selection>(e);

    if (game.ui.cursor == e) {
        sprite_coords dim = spr.sprites(select.box_index).get_dimensions().size;
        sprite_coords p (ev.pos.x - (dim.x / 2), ev.pos.y - (dim.y / 2));

        auto dimensions = spr.sprites(select.box_index).get_dimensions().origin;
        point<u16> box_point = get_gridsquare(spr, dimensions);
        size_t box_index = box_point.x + (box_point.y * select.grid_size.x);
        spr.sprites(1).set_pos(p, dim, box_index);
    }


	//if (ev.active_state() == false)	w->flags.unset(widget::flags::pressed);
	point<u16> active = select.active;
	size_t index = active.x + (active.y * select.grid_size.x);
	spr.sprites(0).set_tex_region(0, index);

	if (ev.active_state() == false) return true;

	// no idea what's going on here.
	point<f32> bruh = ev.pos.to<f32>();
	active = get_gridsquare(spr, bruh);
	if (active.x >= select.grid_size.x || active.y >= select.grid_size.y) return true;

	index = active.x + (active.y * select.grid_size.x);
	select.active = active;
	spr.sprites(0).set_tex_region(1, index);

	return true;
}

void inventory_init(entity e, engine& g, entity parent, c_inventory& inv, screen_coords origin) {
	g.ui.focus = e;
    g.ecs.add<c_widget>(e);
	make_parent(parent, e, g);
	c_selection& select = g.ecs.add<c_selection>(e);

	g.ecs.add<c_mouseevent>(e).add_event(inventory_mouseevents, g, inv);
	g.ecs.add<c_cursorevent>(e).add_event(inventory_motion, g);
	g.ecs.add<c_keyevent>(e).add_event(selection_keypress, g);

	select.grid_size = size<u16>(9, 4);
	sprite_coords element_size(64, 64);
	rect<f32> dim(origin.to<f32>(),
               sprite_coords(select.grid_size.x * element_size.x, select.grid_size.y * element_size.y));

	int num_grid_elements = select.grid_size.x * select.grid_size.y;
	c_display& spr = g.ecs.add<c_display>(e);
	spr.add_sprite(num_grid_elements, render_layers::ui  );
	spr.sprites(0).tex = g.textures().get("highlight");

	int item_spr = spr.add_sprite(num_grid_elements, render_layers::ui);
	spr.sprites(item_spr).tex = g.textures().get("items");


    select.box_index = spr.add_sprite(1, render_layers::null);
    spr.sprites(select.box_index).set_pos(sprite_coords(0, 0), element_size, 0);
    spr.sprites(select.box_index).tex = g.textures().get("highlight");
    spr.sprites(select.box_index).set_tex_region(1, 0);


	int index = 0;
	sprite_coords pos = origin.to<f32>();
	for (u16 y = 0; y < select.grid_size.y; y++) {
		for (u16 x = 0; x < select.grid_size.x; x++) {
			spr.sprites(0).set_pos(pos, element_size, index);
			spr.sprites(0).set_tex_region(0, index);

			if (inv.data.exists(index)) {
                write_item_to_slot(index, pos, e, inv.data.get(index), g);
			}

			pos.x += element_size.x;
			index++;
		}
		pos.y += element_size.y;
		pos.x = 100;
	}
}

// transfer from A to B
void transfer_item(entity e, engine& g, c_inventory& b_inv) {
    c_widget& widget = g.ecs.get<c_widget>(e);
    entity a = g.ui.cursor;


    c_inventory& player_inv = g.ecs.get<c_inventory>(g.player_id());
    c_inventory& storage = g.ecs.get<c_inventory>(g.ui.active_interact);
    c_inventory& a_inv = &b_inv == &player_inv ? storage : player_inv;
    c_selection& a_select = g.ecs.get<c_selection>(a);
    c_display& a_spr = g.ecs.get<c_display>(a);
    point<u16> held_point = get_gridsquare(a_spr, a_spr.sprites(a_select.box_index).get_dimensions().origin);
    size_t held_index = held_point.x + (held_point.y * a_select.grid_size.x);

    c_inventory::item held = a_inv.data.get(held_index);
    empty_slot(held_index, snap_to_grid(a_spr, held_point), a, g);
    a_inv.data.remove(held_index);




    entity b = 65535;
    for (auto& child : widget.children)
        if (child != a) {
            b = child;
            break;
        }
    c_selection& b_select = g.ecs.get<c_selection>(b);
    c_display& b_spr = g.ecs.get<c_display>(b);



    g.ui.cursor = 65535;

    if (b_inv.data.exists(b_select.active_index())) {
        c_inventory::item highlight = b_inv.data.get(b_select.active_index());
        b_inv.data.remove(b_select.active_index());
        a_inv.data.add(held_index, highlight);
        write_item_to_slot(held_index, snap_to_grid(a_spr, a_select.active), a, highlight, g);
        pickup_item(a, snap_to_grid(a_spr, held_point), g);

    }


    b_inv.data.add(b_select.active_index(), held);
    write_item_to_slot(b_select.active_index(), snap_to_grid(b_spr, b_select.active), b, held, g);
}

void inv_transfer_init(entity e, engine& g, c_inventory& storage_inv) {
    g.ecs.add<c_widget>(e);
    g.command_states.set(command::toggle_inventory, true);
    g.create_entity(inventory_init, e, g.ecs.get<c_inventory>(g.player_id()), screen_coords(100, 100));
    g.create_entity(inventory_init, e, storage_inv, screen_coords(100, 400));
    g.ui.focus = e;
}