#include <engine/engine.h>
#include <world/basic_entity_funcs.h>
#include "ui.h"

void transfer_item(entity , engine& g,  ecs::inventory&);



void inventory_hover(entity e, engine& g) {

}


void inventory_navigation(entity e, engine& g, u32 new_target) {
     ecs::selection& select = g.ecs.get<ecs::selection>(e);
    if (select.active.x != 65535 && select.active.y != 65535) {
        sprite_coords new_position;
        auto& spr = g.ecs.get<ecs::display>(e);
        size<f32> item_size = spr.sprites(0).get_dimensions(0).size;
        if (g.ui.last_position != screen_coords(65535, 65535)) {
            new_position = sprite_coords(g.ui.last_position.to<f32>() - (item_size / 6));
        } else  {
            new_position = spr.sprites(0).get_dimensions(new_target).origin + (item_size / 6);
        }
        spr.sprites(1).set_pos(new_position, item_size, select.active_index());
        spr.sprites(2).set_pos(new_position, item_size, select.active_index());
    }
    selectiongrid_navigation(e, g, new_target);
}

// Given a gridsquare, this function returns the top left sprite coordinate corresponding to it
sprite_coords snap_to_grid( ecs::display& spr, point<u16> in) {
    sprite_coords tl = spr.sprites(0).get_dimensions().origin;
	return sprite_coords(in.x * 64 + tl.x, in.y * 64 + tl.y);
}

void set_square_highlight(entity e, engine& g, int square, bool highlight) {
    g.ecs.get<ecs::display>(e).sprites(0).set_tex_region(highlight ? 1 : 0, square);
}

// Set the inventory to be the active cursor widget, and put the selection box in place of the clicked-on element.
void pickup_item(entity e, sprite_coords box_snap_point, engine& g) {
     ecs::selection& select = g.ecs.get<ecs::selection>(e);
     ecs::display& spr = g.ecs.get<ecs::display>(e);
}

void write_item_to_slot(size_t index, sprite_coords pos, entity e,  ecs::inventory::item item_in, engine& g) {
     ecs::display& spr = g.ecs.get<ecs::display>(e);
     ecs::selection &select = g.ecs.get<ecs::selection>(e);

    size<f32> item_size = spr.sprites(select.sprite_index).get_dimensions(select.sprite_index).size;
    spr.sprites(1).set_pos(pos, item_size, index);
    spr.sprites(1).set_tex_region(item_in.ID, index);

    auto& text = g.ecs.get<ecs::text>(e);
    spr.sprites(text.sprite_index).set_pos(pos, item_size, index);
    text.text_entries[index].quad_index = index;
    text.text_entries[index].text = std::to_string(item_in.quantity);
}

void empty_slot(size_t index, sprite_coords pos, entity e, engine& g) {
     ecs::display& spr = g.ecs.get<ecs::display>(e);
    spr.sprites(1).set_pos(pos, size(0.0f, 0.0f), index);

    auto& text = g.ecs.get<ecs::text>(e);
    spr.sprites(text.sprite_index).set_pos(point<f32>(0, 0), size<f32>(0, 0), index);
    text.text_entries[index].text = "";
}

void place_item(entity e, engine& g,  ecs::selection& select,  ecs::inventory& inv,  ecs::display& spr) {
     ecs::inventory::item held = inv.data.get(select.active_index());
    empty_slot(select.active_index(), snap_to_grid(spr, select.active), e, g);
    inv.data.remove(select.active_index());


    if (inv.data.exists(select.highlight_index())) {
         ecs::inventory::item highlight = inv.data.get(select.highlight_index());
        inv.data.remove(select.highlight_index());
        inv.data.add(select.active_index(), highlight);
        write_item_to_slot(select.active_index(), snap_to_grid(spr, select.active), e, highlight, g);
        pickup_item(e, snap_to_grid(spr, select.active), g);
    } else {
        set_square_highlight(e, g, project_to_1D(select.active, select.grid_size.x), false);
        select.active = point<u16>(65535, 65535);
    }

    inv.data.add(select.highlight_index(), held);
    write_item_to_slot(select.highlight_index(), snap_to_grid(spr, select.highlight), e, held, g);
}

void inventory_activation(entity e, engine& g, bool release) {
     ecs::inventory& inv = g.ecs.get<ecs::inventory>(g.player_id());
     ecs::selection& select = g.ecs.get<ecs::selection>(e);
     ecs::display& spr = g.ecs.get<ecs::display>(e);

    // You already have an item, try to place or swap it.
    if (select.active_index() < select.num_elements() && release == false
        && select.highlight_index() < select.num_elements() ) {
        place_item(e, g, select, inv, spr);
    } else if (select.highlight_index() < select.num_elements() && release == false)  {
        if (inv.data.exists(select.highlight_index())) {
            pickup_item(e, snap_to_grid(spr, select.highlight), g);
            select.active = select.highlight;
        }
	} else if (g.ui.focus != e) { // There's another inventory widget active
        entity parent = g.ecs.get<ecs::widget>(e).parent;
        transfer_item(parent, g, inv);
	}
    inventory_navigation(e, g, select.active_index());
}


void inventory_init(entity e, engine& g, entity parent,  ecs::inventory& inv, screen_coords origin) {
	g.ui.focus = e;
	make_widget(e, g, parent);
     ecs::selection& select = g.ecs.add<ecs::selection>(e);

    auto& w = g.ecs.get<ecs::widget>(e);
    w.on_activate = inventory_activation;
    w.on_navigate = inventory_navigation;
    w.on_hover = inventory_hover;

	select.grid_size = size<u16>(9, 4);
	select.active = point<u16>(65535, 65535);
	sprite_coords element_size(64, 64);
	rect<f32> dim(origin.to<f32>(), sprite_coords(select.grid_size.x * element_size.x, select.grid_size.y * element_size.y));

	int num_grid_elements = select.grid_size.x * select.grid_size.y;
    ecs::display& spr = g.ecs.add<ecs::display>(e);
	select.sprite_index = spr.add_sprite(num_grid_elements, g.textures().get("highlight"), 7, render_layers::ui);

	int item_spr = spr.add_sprite(num_grid_elements, g.textures().get("items"), 8, render_layers::ui);

    ecs::text& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = spr.add_sprite(num_grid_elements, nullptr, 0, render_layers::text);
    text.text_entries = std::vector<ecs::text::text_entry>(num_grid_elements);

	int index = 0;
	sprite_coords pos = origin.to<f32>();
	for (u16 y = 0; y < select.grid_size.y; y++) {
		for (u16 x = 0; x < select.grid_size.x; x++) {
			spr.sprites(select.sprite_index).set_pos(pos, element_size, index);
			spr.sprites(select.sprite_index).set_tex_region(0, index);

			if (inv.data.exists(index)) {
                write_item_to_slot(index, pos, e, inv.data.get(index), g);
			}

			pos.x += element_size.x;
			index++;
		}
		pos.y += element_size.y;
		pos.x = origin.x;
	}
}

// transfer from A to B - broken
void transfer_item(entity e, engine& g,  ecs::inventory& b_inv) {
     ecs::widget& widget = g.ecs.get<ecs::widget>(e);
    entity a = 65535;


     ecs::inventory& player_inv = g.ecs.get<ecs::inventory>(g.player_id());
     ecs::inventory& storage = g.ecs.get<ecs::inventory>(g.ui.active_interact);
     ecs::inventory& a_inv = &b_inv == &player_inv ? storage : player_inv;
     ecs::selection& a_select = g.ecs.get<ecs::selection>(a);
     ecs::display& a_spr = g.ecs.get<ecs::display>(a);
    size_t held_index = project_to_1D(a_select.active, a_select.grid_size.x);

     ecs::inventory::item held = a_inv.data.get(held_index);
    empty_slot(held_index, snap_to_grid(a_spr, a_select.active), a, g);
    a_inv.data.remove(held_index);




    entity b = 65535;
    for (auto& child : widget.children)
        if (child != a) {
            b = child;
            break;
        }
     ecs::selection& b_select = g.ecs.get<ecs::selection>(b);
     ecs::display& b_spr = g.ecs.get<ecs::display>(b);


    if (b_inv.data.exists(b_select.highlight_index())) {
         ecs::inventory::item highlight = b_inv.data.get(b_select.highlight_index());
        b_inv.data.remove(b_select.highlight_index());
        a_inv.data.add(held_index, highlight);
        write_item_to_slot(held_index, snap_to_grid(a_spr, a_select.highlight), a, highlight, g);
        pickup_item(a, snap_to_grid(a_spr, a_select.active), g);

    }

    b_inv.data.add(b_select.highlight_index(), held);
    write_item_to_slot(b_select.highlight_index(), snap_to_grid(b_spr, b_select.highlight), b, held, g);
}

void inv_transfer_init(entity e, engine& g,  ecs::inventory& storage_inv) {
    g.ecs.add<ecs::widget>(e);
    g.command_states.set(command::toggle_inventory, true);
    g.create_entity(inventory_init, e, g.ecs.get<ecs::inventory>(g.player_id()), screen_coords(100, 100));
    g.create_entity(inventory_init, e, storage_inv, screen_coords(100, 400));
    g.ui.focus = e;
}