#include "ui.h"

///////////////////////////
//     CHECKBOX CODE     //
///////////////////////////

void optionmenu_activation(entity e, engine& g, bool release) {
    if (release) return;
    auto& select = g.ecs.get<ecs::selection>(e);
    auto& checkbox = g.ecs.get<ecs::checkbox>(e);
    auto& dpy = g.ecs.get<ecs::display>(e);
    checkbox.checked.flip(select.highlight_index());
    int new_tex_region = checkbox.checked.test(select.highlight_index()) ? 3 : 2;
    dpy.sprites(checkbox.sprite_index).set_tex_region(new_tex_region, select.highlight_index());
}

void checkbox_set_highlight(entity e, engine& g, int index, bool enter) {
    auto& checkbox = g.ecs.get<ecs::checkbox>(e);
    int activated = (checkbox.checked.test(index) ? 1 : 0) + (enter ? 2 : 0);
    g.ecs.get<ecs::display>(e).sprites(checkbox.sprite_index).set_tex_region(activated, index);
}

void checkbox_navigation(entity e, engine& g, u32 new_index) {
    auto& select = g.ecs.get<ecs::selection>(e);
    if (select.highlight_index() < select.num_elements()) {
        checkbox_set_highlight(e, g, select.highlight_index(), false);
    }
    if (new_index < select.num_elements()) {
        checkbox_set_highlight(e, g, new_index, true);
    }
}

void add_checkbox(entity e, engine& g, point<f32> text_pos, size<f32> grid_size, point<f32> checkbox_pos, u32 index, bool state, std::string label) {
    auto& display = g.ecs.get<ecs::display>(e);
    display.sprites(0).set_pos(text_pos, sprite_coords(checkbox_pos.x + grid_size.y, grid_size.y) - sprite_coords(text_pos.x, 0), index);
    display.sprites(0).set_tex_region(0, index);

    text_pos.x *= 1.05;
    auto& checkbox = g.ecs.get<ecs::checkbox>(e);
    checkbox.checked[index] = state;
    display.sprites(checkbox.sprite_index).set_pos(checkbox_pos, sprite_coords(grid_size.y, grid_size.y), index);
    display.sprites(checkbox.sprite_index).set_tex_region(state == true ? 1 : 0, index);

    auto& text = g.ecs.get<ecs::text>(e);

    sprite_coords label_size = g.get_text_size(label);
    display.sprites(text.sprite_index).set_pos(text_pos + sprite_coords(0, grid_size.y - label_size.y) / 2, grid_size, index);
    text.text_entries.push_back( ecs::text::text_entry{index, label});
}

void initialize_checkbox_group(entity e, entity parent, u8 z_index, engine& g, u32 num_checkboxes) {
    make_widget(e, g, parent);
    auto& widget = g.ecs.get<ecs::widget>(e);
    widget.on_activate = optionmenu_activation;
    widget.on_navigate = checkbox_navigation;

    g.ecs.add<ecs::selection>(e).grid_size = size<u16>(1, num_checkboxes);

    auto& display = g.ecs.add<ecs::display>(e);
    display.add_sprite(num_checkboxes, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");

    auto& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = display.add_sprite(num_checkboxes, render_layers::ui);;

    int checkbox_sprite_index = display.add_sprite(num_checkboxes, render_layers::ui);
    auto& checkbox = g.ecs.add<ecs::checkbox>(e);
    checkbox.sprite_index = checkbox_sprite_index;
    display.sprites(checkbox_sprite_index).tex = g.textures().get("checkbox");
    display.sprites(checkbox.sprite_index).z_index = z_index;
}

/////////////////////////
//     SLIDER CODE     //
/////////////////////////

void slider_set_value(entity e, engine& g, int value, u32 index) {
    auto& slider = g.ecs.get<ecs::slider>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    value = std::clamp(value, slider.min_values[index], slider.max_values[index]);
    slider.current_values[index] = value;
    rect<f32> slider_rect = display.sprites(1).get_dimensions(index + g.ecs.get<ecs::selection>(e).grid_size.y);
    f32 value_percentage = (f32(value) / f32(slider.max_values[index]));
    f32 new_x = slider_rect.origin.x + (value_percentage * slider_rect.size.x) - 8;
    new_x = std::clamp(new_x, slider_rect.origin.x, slider_rect.origin.x + slider_rect.size.x);
    display.sprites(1).set_pos(point<f32>(new_x, slider_rect.origin.y), sprite_coords(16, 32), index);
}

int value_at_position(entity e, engine& g, int position, u32 index) {
    auto& slider = g.ecs.get<ecs::slider>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    rect<f32> slider_rect = display.sprites(1).get_dimensions(index + g.ecs.get<ecs::selection>(e).grid_size.y);
    position = std::clamp(f32(position), slider_rect.origin.x, slider_rect.origin.x + slider_rect.size.x);
    f32 position_percentage = (f32(position - slider_rect.origin.x) / f32(slider_rect.size.x));
    f32 new_value = position_percentage * (slider.max_values[index] - slider.min_values[index]);
    return new_value;
}

void slider_navigation(entity e, engine& g, u32 new_position) {
    auto& select = g.ecs.get<ecs::selection>(e);
    if (select.active_index() < select.num_elements()) {
        slider_set_value(e, g, value_at_position(e, g, new_position, 0), select.highlight_index());
    }
}

void add_slider(entity e, engine& g, point<f32> pos, size<f32> grid_size, u32 index, int min, int current, int max, int increment, std::string label) {
    auto& slider = g.ecs.get<ecs::slider>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    slider.min_values[index] = min;
    slider.current_values[current] = current;
    slider.max_values[index] = max;
    slider.increments[index] = increment;

    display.sprites(0).set_pos(pos, sprite_coords(512, 64), index);
    display.sprites(0).set_tex_region(0, index);

    int num_sliders = g.ecs.get<ecs::selection>(e).grid_size.y;
    display.sprites(1).set_pos(pos + point<f32>(240, 16), sprite_coords(256, 32), index + num_sliders );
    display.sprites(1).set_tex_region(index, 1);
    display.sprites(1).set_tex_region((index + num_sliders), 0);
    slider_set_value(e, g, 25, index);

    auto& text = g.ecs.get<ecs::text>(e);
    display.sprites(text.sprite_index).set_pos(pos + sprite_coords(24, 16), sprite_coords(384, 32), index);
    text.text_entries.push_back( ecs::text::text_entry{index, label});
}

void initialize_slider_group(entity e, entity parent, u8 z_index, engine& g, int num_sliders) {
    make_widget(e, g, parent);
    auto& widget = g.ecs.get<ecs::widget>(e);
    widget.on_navigate = slider_navigation;
    g.ecs.add<ecs::selection>(e).grid_size = size<u16>(1, num_sliders);

    auto& display = g.ecs.add<ecs::display>(e);
    display.add_sprite(num_sliders, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");

    display.add_sprite(num_sliders * 2, render_layers::ui);
    display.sprites(1).tex = g.textures().get("slider");
    g.ecs.add<ecs::slider>(e);

    auto& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = display.add_sprite(num_sliders, render_layers::ui);
}

/////////////////////////
//     BUTTON CODE     //
/////////////////////////

void button_activation(entity e, engine& g, bool release) {
    auto& buttons = g.ecs.get<ecs::button>(e);
    auto& select = g.ecs.get<ecs::selection>(e);
    int active_index = select.active_index();
    if (release && select.active_index() < select.num_elements()) {
        if (select.active_index() == select.highlight_index()) {
            buttons.callbacks[active_index](e, g, release);
        }
        g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(0, select.active_index());
        select.active = point<u16>(65535, 65535);
    } else if (release == false && select.highlight_index() < select.num_elements()) {
        g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(2,  select.highlight_index());
        select.active = select.highlight;
    }
}

void button_navigation(entity e, engine& g,u32 new_index) {
    auto& select = g.ecs.get<ecs::selection>(e);
    auto& buttons = g.ecs.get<ecs::button>(e);


    if (select.highlight_index() < select.num_elements()) {
        if (select.highlight_index() != select.active_index()) {
            g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(0, select.highlight_index());
        }
    }
    if (new_index < select.num_elements()) {
        if (new_index != select.active_index()) {
            g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(1, new_index);
        } else {
            g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(2, new_index);
        }
    } else if (select.active_index() < select.num_elements()) {
        g.ecs.get<ecs::display>(e).sprites(buttons.sprite_index).set_tex_region(1, select.active_index());
    }
    select.highlight = project_to_2D<u16>(new_index, select.grid_size.x);
}

void add_button(entity e, engine& g, point<f32> pos, size<f32> grid_size, u32 index,  ecs::widget::activation_action func, std::string label) {
    auto& button = g.ecs.get<ecs::button>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    button.callbacks[index] = func;

    display.sprites(0).set_pos(pos, grid_size, index);
    display.sprites(0).set_tex_region(0, index);

    display.sprites(1).set_pos(pos, grid_size , index);
    display.sprites(1).set_tex_region(0, index);

    auto& text = g.ecs.get<ecs::text>(e);
    display.sprites(text.sprite_index).set_pos(pos , grid_size, index);
    text.text_entries.push_back( ecs::text::text_entry{index, label});
}

void initialize_button_group(entity e, entity parent, u8 z_index, engine& g, int num_buttons) {
    make_widget(e, g, parent);
    auto& widget = g.ecs.get<ecs::widget>(e);
    widget.on_navigate = button_navigation;
    widget.on_activate = button_activation;
    g.ecs.add<ecs::selection>(e).grid_size = size<u16>(1, num_buttons);
    g.ecs.get<ecs::selection>(e).active = point<u16>(65535, 65535);

    auto& display = g.ecs.add<ecs::display>(e);
    display.add_sprite(num_buttons, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");
    display.sprites(0).z_index = z_index;

    auto& button = g.ecs.add<ecs::button>(e);
    button.sprite_index = display.add_sprite(num_buttons, render_layers::ui);
    display.sprites(button.sprite_index).tex = g.textures().get("button");
    display.sprites(button.sprite_index).z_index = z_index + 2;

    auto& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = display.add_sprite(num_buttons, render_layers::ui);
}

///////////////////////////
//     DROPDOWN CODE     //
///////////////////////////

void dropdown_set_option(entity e, engine& g, int index) {
    auto& dropdown = g.ecs.get<ecs::dropdown>(e);
    auto& select = g.ecs.get<ecs::selection>(e);
    auto& text = g.ecs.get<ecs::text>(e);
    int text_index = (select.active_index() * 2) + 1;
    if (select.active_index() < select.num_elements()) {
        text.text_entries[text_index].text = dropdown.dropdowns[select.active_index()].entries[index];
        text.text_entries[text_index].regen = true;
    }

}

void destroy_menu_expansion(entity e, engine& g) {
    entity parent = g.ecs.get<ecs::widget>(e).parent;
    g.destroy_entity(e);

    auto& parent_select = g.ecs.get<ecs::selection>(parent);
    auto& parent_display = g.ecs.get<ecs::display>(parent);
    auto& dropdown = g.ecs.get<ecs::dropdown>(parent);
    rect<f32> new_rect = parent_display.sprites(dropdown.sprite_index).get_dimensions(parent_select.active_index());
    g.ecs.get<ecs::display>(parent).sprites(0).set_pos(new_rect.origin, new_rect.size,parent_select.active_index());
    parent_select.active = point<u16>(65535, 65535);
}

void dropdown_menu_activation(entity e, engine& g, bool release) {
    entity parent = g.ecs.get<ecs::widget>(e).parent;
    auto& selection = g.ecs.get<ecs::selection>(e);
    dropdown_set_option(parent, g, selection.active_index());
    g.ui.focus = parent;
    destroy_menu_expansion(e, g);
}

void open_dropdown(entity e, engine& g, entity parent, rect<f32> box,  ecs::dropdown::dropdown_data dropdown_data) {
    g.ui.focus = e;
    initialize_button_group(e, parent, g.ecs.get<ecs::display>(parent).sprites(0).z_index + 1, g, dropdown_data.entries.size());
    for (int i = 0; i < dropdown_data.entries.size(); i++) {
        box.origin.y += box.size.y;
        add_button(e, g, box.origin, box.size, i, dropdown_menu_activation, dropdown_data.entries[i]);
    }
}

void dropdown_activation(entity e, engine& g, bool release) {
    auto& dropdown = g.ecs.get<ecs::dropdown>(e);
    auto& select = g.ecs.get<ecs::selection>(e);
    int active_index = select.active_index();
    if (release && select.active_index() < select.num_elements()) {
        sprite_data& dropdown_sprite = g.ecs.get<ecs::display>(e).sprites(dropdown.sprite_index);
        dropdown_sprite.set_tex_region(0, select.active_index());
        auto& widget = g.ecs.get<ecs::widget>(e);
        if (select.active_index() == select.highlight_index() && widget.children.empty()) {
            entity child = g.create_entity(open_dropdown, e, dropdown_sprite.get_dimensions(select.active_index()), dropdown.dropdowns[select.active_index()]);
            rect<f32> dropdown_rect = dropdown_sprite.get_dimensions(select.active_index());
            g.ecs.get<ecs::display>(e).sprites(0).set_pos
                (dropdown_rect.origin, dropdown_rect.size + g.ecs.get<ecs::display>(child).sprites(0).get_dimensions().size, select.active_index());
        } else {
            destroy_menu_expansion(*widget.children.begin(), g);
            select.active = point<u16>(65535, 65535);
        }
    } else if (release == false && select.highlight_index() < select.num_elements()) {
        g.ecs.get<ecs::display>(e).sprites(dropdown.sprite_index).set_tex_region(2,  select.highlight_index());
        select.active = select.highlight;
    }
}

void dropdown_navigation(entity e, engine& g, u32 new_index) {
    auto& select = g.ecs.get<ecs::selection>(e);
    auto& dropdown = g.ecs.get<ecs::dropdown>(e);
    if (select.highlight_index() < select.num_elements()) {
        if (select.highlight_index() != select.active_index()) {
            g.ecs.get<ecs::display>(e).sprites(dropdown.sprite_index).set_tex_region(0, select.highlight_index());
        }
    }
    if (new_index < select.num_elements()) {
        if (new_index != select.active_index()) {
            g.ecs.get<ecs::display>(e).sprites(dropdown.sprite_index).set_tex_region(1, new_index);
        }
    }
    select.highlight = project_to_2D<u16>(new_index, select.grid_size.x);
}

void add_dropdown(entity e, engine& g, point<f32> pos, size<f32> grid_size, point<f32> box_pos, size<f32> box_size,
                  u32 index, std::vector<std::string> options, std::string label, int active) {
    auto& dropdown = g.ecs.get<ecs::dropdown>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    dropdown.dropdowns.emplace_back( ecs::dropdown::dropdown_data{ options, active });

    display.sprites(0).set_pos(pos, sprite_coords(box_pos.x + box_size.x, box_size.y) - sprite_coords(pos.x, 0), index);
    display.sprites(0).set_tex_region(0, index);
    pos.x *= 1.05;

    auto& text = g.ecs.get<ecs::text>(e);
    // set label for dropdown entry
    sprite_coords label_size = g.get_text_size(label);
    display.sprites(text.sprite_index).set_pos(pos + sprite_coords(0, grid_size.y - label_size.y) / 2, grid_size, (index * 2));
    text.text_entries.push_back( ecs::text::text_entry{(index * 2), label});

    display.sprites(1).set_pos(box_pos, box_size, index);
    display.sprites(1).set_tex_region(0, index);
    sprite_coords text_size = g.get_text_size(options[active]);
    display.sprites(text.sprite_index).set_pos(box_pos + (box_size - text_size) / 2, grid_size, (index * 2) + 1);
    text.text_entries.push_back( ecs::text::text_entry{(index * 2) + 1, options[active]});
}

void initialize_dropdown_group(entity e, entity parent, u8 z_index, engine& g, int num_dropdowns) {
    make_widget(e, g, parent);
    auto& widget = g.ecs.get<ecs::widget>(e);
    widget.on_navigate = dropdown_navigation;
    widget.on_activate = dropdown_activation;
    g.ecs.add<ecs::selection>(e).grid_size = size<u16>(1, num_dropdowns);
    g.ecs.get<ecs::selection>(e).active = point<u16>(65535, 65535);

    auto& display = g.ecs.add<ecs::display>(e);
    display.add_sprite(num_dropdowns, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");
    display.sprites(0).z_index = z_index;

    auto& dropdown = g.ecs.add<ecs::dropdown>(e);
    dropdown.sprite_index = display.add_sprite(num_dropdowns, render_layers::ui);
    display.sprites(dropdown.sprite_index).tex = g.textures().get("button");
    display.sprites(dropdown.sprite_index).z_index = z_index + 1;

    auto& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = display.add_sprite(num_dropdowns * 2, render_layers::ui);
}

////////////////////////////
//     SELECTION CODE     //
////////////////////////////

void selectiongrid_set_highlight(entity e, engine& g, u32 index, bool enter) {
    g.ecs.get<ecs::display>(e).sprites(0).set_tex_region(enter ? 1 : 0, index);
}

void selectiongrid_navigation(entity e, engine& g, u32 new_index) {
    auto& select = g.ecs.get<ecs::selection>(e);
    /*if (select.highlight_index() < select.num_elements()) {
         selectiongrid_set_highlight(e, g, select.highlight_index(), false);
    }
    if (u32(new_index) < select.num_elements()) {
        selectiongrid_set_highlight(e, g, new_index, true);
    };*/
    select.highlight = project_to_2D<u16>(new_index, select.grid_size.x);
}

////////////////////////////
//     TEXTINPUT CODE     //
////////////////////////////

// todo - add newline support
void textinput_event(entity e, engine& g, std::string new_text) {
    auto& text = g.ecs.get<ecs::text>(e);
    auto& select = g.ecs.get<ecs::selection>(e);
    auto& text_str = text.text_entries.begin()->text;
    if (new_text[0] == 0x08 && new_text.size() == 1) { // backspace
        if (select.highlight.x < 1) {
            text_str = "  ";
            return;
        } else {
            int byte_index_to_delete = g.ecs.systems.text.character_byte_index(text_str, select.highlight.x - 1);
            int bytes_to_delete = g.ecs.systems.text.bytes_of_character(text_str, select.highlight.x - 1);
            int remaining_bytes = int(text_str.size()) - (byte_index_to_delete + bytes_to_delete);
            printf("susbttring half i is %s, half two is %s\n", text_str.substr(0, byte_index_to_delete).c_str(),
                   text_str.substr(byte_index_to_delete + bytes_to_delete, remaining_bytes).c_str());
            text_str = text_str.substr(0, byte_index_to_delete).c_str() + text_str.substr(byte_index_to_delete + bytes_to_delete, remaining_bytes);
        }
    } else {
        if (select.highlight.x == select.grid_size.x) {
            text_str += new_text;
        } else {
            int bytes_before_cursor = g.ecs.systems.text.character_byte_index(text_str, select.highlight.x);
            int bytes_at_cursor = g.ecs.systems.text.bytes_of_character(text_str, select.highlight.x);
            int remaining_bytes = int(text_str.size()) - (bytes_before_cursor);

            printf("susbttring half i is %s, half two is %s\n", text_str.substr(0, bytes_before_cursor).c_str(),
                   text_str.substr(bytes_before_cursor, remaining_bytes).c_str());
            text_str = text_str.substr(0, bytes_before_cursor).c_str() + new_text + text_str.substr(bytes_before_cursor, remaining_bytes);
        }
    }
    text.text_entries.begin()->regen = true;
    int new_numchars = g.ecs.systems.text.num_characters(text_str) - 1;
    int characters_added = new_numchars - (select.grid_size.x) ;
    select.highlight.x += characters_added;
    select.grid_size.x += characters_added;

    auto& display = g.ecs.get<ecs::display>(e);
    auto new_cursor_pos = g.ecs.systems.text.position_of_character(text_str, select.highlight.x).to<f32>() + display.sprites(0).get_dimensions(0).origin;
    display.sprites(1).set_pos(new_cursor_pos, display.sprites(1).get_dimensions(0).size, 0);
}

void textinput_navigation(entity e, engine& g, u32 new_index) {
    auto& text = g.ecs.get<ecs::text>(e);
    auto& display = g.ecs.get<ecs::display>(e);
    auto& select = g.ecs.get<ecs::selection>(e);
    select.highlight.x = std::min(select.grid_size.x, u16(new_index));
    select.highlight.x = select.highlight.x == 65535 ? 0 : select.highlight.x;
    auto new_cursor_pos = g.ecs.systems.text.position_of_character(text.text_entries.begin()->text, select.highlight.x ).to<f32>() + display.sprites(0).get_dimensions(0).origin;
    display.sprites(1).set_pos(new_cursor_pos, display.sprites(1).get_dimensions(0).size, 0);

}

// some oddities - to avoid creating a new component, this utilizes existing components in weird ways.
//     - c_selection's grid size uses its x component to store a character index, and y to store the number of lines
//     - a single bool in the widget component stores whether a component supports text input
//     - additionally, because of how text input works, clicking with the mouse redirects to a navigation action
void add_textinput(entity e, entity parent, u8 z_index, engine& g, point<f32> pos, size<f32> grid_size) {
    make_widget(e, g, parent);
    g.ui.focus = e;
    auto& widget = g.ecs.get<ecs::widget>(e);
    widget.accepts_textinput = true;
    widget.on_navigate = textinput_navigation;
    auto& select = g.ecs.add<ecs::selection>(e);
    select.grid_size = size<u16>(0, 1);
    select.highlight.x = 0;

    auto& display = g.ecs.add<ecs::display>(e);



    display.add_sprite(1, render_layers::ui);
    display.sprites(0).tex = g.textures().get("menu_background");
    display.sprites(0).z_index = z_index;
    display.sprites(0).set_pos(pos, grid_size, 0);

    display.add_sprite(1, render_layers::ui);
    display.sprites(1).tex = g.textures().get("cursor_and_highlight");
    display.sprites(1).z_index = z_index;
    display.sprites(1).set_pos(pos, sprite_coords(6, grid_size.y), 0);
    display.sprites(1).set_tex_region(0, 0);



    auto& text = g.ecs.add<ecs::text>(e);
    text.sprite_index = display.add_sprite(1, render_layers::ui);
    text.text_entries.emplace_back( ecs::text::text_entry{});
    display.sprites(text.sprite_index ).set_pos(pos, grid_size, 0);
}