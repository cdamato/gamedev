
#include <unicode/unistr.h>
#include <unicode/stringpiece.h>
#include <unicode/brkiter.h>
#include <iostream>
#include <common/parser.h>
#include <numeric>
#include <atomic>
#include <common/png.h>
#include <cmath>
#include <ft2build.h>
#include "ecs.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace ecs {

/////////////////////////////
//     ECS ENGINE CODE     //
/////////////////////////////

std::vector<entity> entity_manager::remove_marked() {
	std::vector<entity> destroyed_list;
    for(auto& bucket: buckets) {
    	const std::unique_lock lock(bucket.mutex);
        for(auto id: bucket.ids) {
        	entity_freelist.push_back(id);
        	destroyed_list.push_back(id);
        }
        bucket.ids.clear();
    }
    return destroyed_list;
}

void entity_manager::mark_entity(entity id) {
    const auto bucket_id = get_thread_id() % bucket_count;
    auto& bucket = buckets[bucket_id];
    const std::unique_lock lock(bucket.mutex);
    bucket.ids.push_back(id);
}

std::size_t entity_manager::get_thread_id() noexcept {
	static std::atomic<std::size_t> generator = 0;
    const thread_local auto id = generator.fetch_add(1, std::memory_order_relaxed);
    return id;
}

entity_manager::entity_manager() {
	entity_freelist = std::vector<u32>(max_entities);
	std::iota (std::begin(entity_freelist), std::end(entity_freelist), 0); // Fill with 0, 1, ..., 99.
}

void ecs_engine::run_ecs(int framerate_multiplier) {
    c_player& player = pool<c_player>().get(_player_id);
	system_velocity_run(pool<c_velocity>(), pool<c_display>(), framerate_multiplier);
	system_collison_run(pool<c_collision>(), pool<c_display>(), *pool<c_mapdata>().begin());
	systems.shooting.run(pool<c_display>(), pool<c_weapon_pool>(), player);
	systems.health.run(pool<c_health>(), pool<c_damage>(), entities);
	systems.health.update_healthbars(pool<c_health>(), pool<c_display>());
    systems.proxinteract.run(pool<c_proximity>(), pool<c_widget>(), pool<c_display>().get(_player_id));
	systems.text.run(pool<c_text>(), pool<c_display>());
	std::vector<entity> destroyed = entities.remove_marked();
	for (auto entity : destroyed) {
		components.remove_all(entity);
	}
}

entity entity_manager::add_entity() {
	if (entity_freelist.empty()) throw "error af";
	entity e = entity_freelist[0];

	std::swap(entity_freelist[0], entity_freelist.back());
	entity_freelist.pop_back();
	return e;
}

////////////////////////////////
//     ECS COMPONENT CODE     //
////////////////////////////////

u8 c_collision:: get_team_detectors() { return collision_flags.to_ulong() & 0b111; }
u8 c_collision::get_team_signals() { return (collision_flags.to_ulong() & 0b111000) >> 3; }
u8 c_collision:: get_tilemap_collision() { return (collision_flags.to_ulong() & 0b11000000) >> 6; }
void c_collision::set_team_detector(flags to_set) { collision_flags |= std::bitset<8>(to_set); }
void c_collision::set_team_signal(flags to_set) { collision_flags |= std::bitset<8>(to_set << 3); }
void c_collision:: set_tilemap_collision(flags to_set) { collision_flags |= std::bitset<8>(to_set); }

bool c_mapdata::tile_data::is_occupied(u8 quad_index, u8 type) {
    if (type == 0) return false;
    u8 bitmask = type << (quad_index * 2);
    return (collision & std::bitset<8>(bitmask)) == bitmask;
}

void c_mapdata::tile_data::set_data(u8 quad_index, u8 type) {
    std::bitset<8> aa(type << (quad_index * 2));
    collision |= aa;
}

u32 c_display::add_sprite(u16 num_quads, render_layers layer) {
    _sprites.push_back(sprite_data(num_quads, layer));
    return _sprites.size() - 1;
}

rect<f32> c_display::get_dimensions() {
    sprite_coords min(65535, 65535);
    sprite_coords max(0, 0);

    for (auto& sprite : _sprites) {
        rect<f32> dim = sprite.get_dimensions();
        min.x = std::min(min.x, dim.origin.x);
        min.y = std::min(min.y, dim.origin.y);

        max.x = std::max(max.x, (dim.size + dim.origin).x);
        max.y = std::max(max.y, (dim.size + dim.origin).y);
    }

    return rect<f32>(min, max - min);
}

////////////////////////////////////////
//          ECS SYSTEMS CODE          //
////////////////////////////////////////

/////////////////////////
//     S_COLLISION     //
/////////////////////////

bool sprite_has_collision(size_t i, c_collision& col) {
    for (auto it : col.disabled_sprites)
        if (it == i) return false;
    return true;
}

// add the normals of all valid sprites in dpy to the vector normals
void add_normals(c_display& dpy, c_collision& col,std::vector<world_coords>& normals) {
    size_t index = 0;
    for (auto& sprite : dpy) {
        index++;
        if (!sprite_has_collision(index - 1, col)) continue;

        // Get the edge vector between two consecutive vertices, then compute and add the normal
        for (size_t i = 0; i < sprite.vertices().size(); i++) {
            size_t vert_index = i + 1 == sprite.vertices().size() ? 0 : i + 1;
            world_coords edge = sprite.vertices()[vert_index].pos - sprite.vertices()[i].pos;
            normals.push_back(world_coords(-edge.y, edge.x));
        }
    }
}

// Project the vertices of the shape onto each normal, to determine overlaps and gaps
void project_shape(c_display& spr, c_collision& col, world_coords normal, f32& min, f32& max) {
    size_t index = 0;
    for (auto& sprite : spr) {
        index++;
        if (!sprite_has_collision(index - 1, col)) continue;
        // Projection onto an axis is the dot product of each vertex with the axis normal.
        for (auto vertex : sprite.vertices()) {
            f32 projection = (vertex.pos.x * normal.x) + (vertex.pos.y * normal.y);
            min = std::min(min, projection);
            max = std::max(max, projection);
        }
    }
}

// SAT collision detection - if there's a gap between two shapes, they don't collide.
// To find if there's a gap, first get the normals of each edge of each sprite.
// Then, project each shape on the axis, and test the min/max of the projections for overlap.
bool test_collision(c_display& a_dpy, c_collision& a_col, c_display& b_dpy, c_collision& b_col) {
    std::vector<world_coords> normals;
    add_normals(a_dpy, a_col, normals);
    add_normals(b_dpy, b_col, normals);

    for (auto normal : normals) {
        f32 a_min = 65535, a_max = 0, b_min = 65535, b_max = 0;

        // Project both sprites onto the axis
        project_shape(a_dpy, a_col, normal, a_min, a_max);
        project_shape(b_dpy, b_col, normal, b_min, b_max);
        // Test for a gap between the min and maxes of both projections
        bool a = (a_min < b_max) && (a_min > b_min);
        bool b = (b_min < a_max) && (b_min > a_min);
        if ((a || b) == false) {
            return false;
        }
    }
    return true;
}




c_mapdata::tile_data get_tiledata(c_mapdata& data, world_coords pos) {
    world_coords trunc_pos(trunc(pos.x), trunc(pos.y));

    int tile_index =  trunc_pos.x + ( trunc_pos.y * data.size.x);
    return data.tiledata_lookup[data.map[tile_index]];
}


bool map_collision (c_display& spr, c_mapdata& data, u8 type) {
    /*for (auto vertex : spr.vertices()) {
        // get the decimal part, then divide it by 0.50 to determine which half it's in, then floor to get an int

        world_coords pos = vertex.pos;
        if ( pos.x < 0 ||  pos.y < 0) return true;


        auto collision_data = get_tiledata(data, pos);
        world_coords trunc_pos(trunc(pos.x), trunc(pos.y));
        int quadrant_x = (pos.x - trunc_pos.x) > 0.50f ? 1 : 0 ;
        int quadrant_y = (pos.y - trunc_pos.y) > 0.50f ? 1 : 0;
        int subquad_index = quadrant_x + (quadrant_y * 2);

        if( collision_data.is_occupied(subquad_index, type)) return true;
cant wait to be 6.9
    }*/
    return false;
}

void system_collison_run(pool<c_collision>& collisions, pool<c_display>& sprites, c_mapdata& data) {
    for (auto col_a : collisions) {
        c_display& spr_a = sprites.get(col_a.parent);
        //if (map_collision(spr_a, data, col_a.get_tilemap_collision())) { col_a.on_collide(data.parent); }
        u8 a_sig = col_a.get_team_signals();
        u8 a_dec = col_a.get_team_detectors();

        auto col_it = pool<c_collision>::iterator(col_a.parent + 1, &collisions);
        for (; col_it != collisions.end(); ++col_it) {
            auto col_b = *col_it;
            u8 b_sig = col_b.get_team_signals();
            u8 b_dec = col_b.get_team_detectors();
            if (((a_sig & b_dec) == 0) && ((b_sig & a_dec) == 0)) continue;

            c_display& spr_b = sprites.get(col_b.parent);
            if (test_collision(spr_a, col_a, spr_b, col_b)) {
                col_b.on_collide(col_a.parent);
                col_a.on_collide(col_b.parent);
            }
        }
    }
}

//////////////////////
//     S_HEALTH     //
//////////////////////

void s_health::run(pool<c_health>& healths, pool<c_damage>& damages, entity_manager& entities) {
    for(auto& damage : damages) {
        if (damage.enemy_ID == 65535) continue;
        entity enemy = damage.enemy_ID;
        damage.enemy_ID = 65535;
        if (!healths.exists(enemy)) continue;

        auto& health = healths.get(enemy);
        health.health -= damage.damage;

        if (health.has_healthbar)
            set_entry(healthbar_atlas_indices[health.parent], health.health);

        if (health.health <= 0) {
            regenerate = true;
            entities.mark_entity(health.parent);
        }
    }
}


static color to_rgb(f32 h, f32 s, f32 v, f32 max = 255) {
    s /= 100.0f;
    v /= 100.0f;

    const f32 z = v * s;
    const f32 x = z * (1 - fabs(fmod(h / 60.0, 2) - 1));
    const f32 m = v - z;

    f32 rp = 0, gp = 0, bp = 0;

    gp = h >= 60 ? z : x;
    bp = h < 120 ? 0 : x;

    if ((h >= 0) & (h < 60))
        rp = z;
    else if (h >= 60 && h < 120)
        rp = x;
    else if (h >= 120 && h < 180)
        rp = 0;

    return color((rp + m) * max, (gp + m) * max, (bp + m) * max, 255);
}

void s_health::set_entry(u32 index, f32 val) {
    color c = to_rgb(val, 100, 50);
    size_t start = index * 32;

    for (size_t i = start; i < start + 32 ; i ++) {
        tex->image_data.write(i, c);
    }
}

void s_health::update_healthbars(pool<c_health>& healths, pool<c_display>& sprites) {
    size_t new_atlassize = 0;

    for (auto& health : healths) {
        if (health.has_healthbar) new_atlassize++;
    }

    if (new_atlassize != last_atlassize) regenerate = true;
    last_atlassize = new_atlassize;

    if (!regenerate || new_atlassize == 0) return;

    size_t index = 0;
    size<u16> tex_size = size<u16>(8, 4 * new_atlassize);
    tex->image_data = image(std::vector<u8>(tex_size.x * tex_size.y * 4), tex_size);
    tex->regions = size<u16>(1, new_atlassize);

    size<f32> slice_size(1, 1.0f / new_atlassize);

    for (auto& health : healths) {
        if (health.has_healthbar == false) continue;
        healthbar_atlas_indices[health.parent] = index;

        c_display& spr = sprites.get(health.parent);
        u32& sprite_index = healthbar_sprite_indices[health.parent];

        // This is a new healthbar, not yet hooked up to a sprite
        if (healthbar_sprite_indices[health.parent] == 0) {
            sprite_coords tl = spr.get_dimensions().bottom_left() + sprite_coords(0, 0.1);
            sprite_index = spr.add_sprite(1, render_layers::sprites);
            spr.sprites(sprite_index).tex = tex;
            size<f32> size(1, 0.25);

            spr.sprites(sprite_index).set_pos(tl, size, 0);
        }

        spr.sprites(sprite_index).set_tex_region(index, 0);
        set_entry(index, (health.health / health.max_health) * 100.0f);

        index++;
    }

    regenerate = false;
}

////////////////////////////
//     S_PROXINTERACT     //
////////////////////////////

bool test_proximity_collision(c_proximity& c, c_display& spr) {
    if(c.shape == c_proximity::shape::rectangle)
        return AABB_collision(rect<f32>(c.origin, c.radii), spr.sprites(0).get_dimensions());
    if(c.shape == c_proximity::shape::elipse) {
        world_coords p = spr.sprites(0).get_dimensions().center();
        f32 left_side = pow(p.x - c.origin.x, 2) / pow(c.radii.x, 2);
        f32 right_side = pow(p.y - c.origin.y, 2) / pow(c.radii.y, 2);
        return left_side + right_side <= 1;
    }
    return false;
}

template <typename T>
f32 distance(T a, T b) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
}

void s_proxinteract::run(pool<c_proximity>& proximities, pool <c_widget>& widgets, c_display& player_spr) {
    f32 min = 1000;
    entity current_min = 65535;
    for(auto& proximity : proximities) {
        if (!widgets.exists(proximity.parent)) continue;
        if (widgets.get(proximity.parent).on_activate == nullptr) continue;

        if (test_proximity_collision(proximity, player_spr)) {
            f32 score = distance(proximity.origin, player_spr.sprites(0).get_dimensions().origin);
            if (score < min) {
                min = score;
                current_min = proximity.parent;
            }
        }
    }
    active_interact = current_min;
}

///////////////////////////
//     MISC. SYSTEMS     //
///////////////////////////

void system_velocity_run(pool<c_velocity>& velocities, pool<c_display>& sprites, int framerate_multiplier) {
    for (auto velocity : velocities) {
        c_display& spr = sprites.get(velocity.parent);
        sprite_coords new_velocity (velocity.delta.x / framerate_multiplier, velocity.delta.y / framerate_multiplier);
        for (auto& sprite : spr) {
            sprite.move_by(new_velocity);
        }
    }
}//     SOFTWARE TEXTURE MANAGAER CODE     //


void s_shooting::run(pool<c_display>& sprites, pool<c_weapon_pool> weapons, c_player& p) {
    auto& pool =  weapons.get(p.parent);
    c_weapon_pool::weapon active = pool.weapons.get(pool.current);
    if (p.shoot == true && active.t.elapsed<timer::ms>() > timer::ms(active.stats.cooldown)) {
        bullet& b = bullet_types[active.stats.bullet];
        shoot(b.tex, b.dimensions, b.speed, sprites.get(p.parent).get_dimensions().center(),
                     p.target, c_collision::flags::ally);
        active.t.start();
    }
    p.shoot = false;
}

////////////////////
//     S_TEXT     //
////////////////////

void import_locale(std::unordered_map<std::string, std::string>& locale) {
    config_parser p("config/locale_english.txt");
    auto d = p.parse();
    for (auto it = d->begin(); it != d->end(); it++) {
        const config_string* list = dynamic_cast<const config_string*>((&it->second)->get());
        std::string key = it->first;
        std::string value = dynamic_cast<config_string*>((&it->second)->get())->get();
        locale[key] = value;
    }
}

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

struct s_text::impl {
    FT_Library library;
    FT_Face face;
    hb_blob_t* blob = hb_blob_create_from_file("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

    hb_font_t* font;

    std::unordered_map<std::string, std::string> locale_lookup;
};

s_text::s_text() {
    data = new impl;
    FT_Init_FreeType(&data->library );
    FT_New_Face(data->library, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &data->face);
    FT_Set_Char_Size( data->face, 16 * 64, 0, 100, 0 );
    import_locale(data->locale_lookup);
    data->font = hb_ft_font_create_referenced(data->face);
    hb_ft_font_set_funcs(data->font);
    /*
    hb_buffer_destroy(buf);
    hb_font_destroy(font);
    hb_face_destroy(face);
    hb_blob_destroy(blob);
    */
}




std::vector<int> get_newlines(std::string& text) {
    icu::UnicodeString s(text.c_str());
    UErrorCode status = U_ZERO_ERROR;
    icu::BreakIterator* char_locations = icu::BreakIterator::createCharacterInstance(icu::Locale::getUS(), status);
    char_locations->setText(s);
    i32 num_chars = char_locations->preceding(text.length());
    if (num_chars < 50) {
        return std::vector<int>();
    }

    std::vector<int> newlines;
    icu::BreakIterator* linebreak_locations = icu::BreakIterator::createLineInstance(icu::Locale::getUS(), status);
    linebreak_locations->setText(s);
    int break_index = linebreak_locations->first();
    while (break_index != icu::BreakIterator::DONE) {
        break_index = linebreak_locations->next();
        int preceeding_char = char_locations->preceding(break_index);
        if (preceeding_char >= ((newlines.size() + 1) * 50) && preceeding_char != icu::BreakIterator::DONE) {

            int new_newline = linebreak_locations->preceding(break_index);
            if (new_newline < text.size() && new_newline != 0) {
                newlines.emplace_back(new_newline);
                printf("registered newline at %i\n", new_newline);
            }
            break_index = linebreak_locations->next();
        }
    }
    return newlines;
}



    void copy_bitmap(std::vector<u8>& h, FT_Bitmap* bmp,point<u16> pen, int width, color text_color) {

        for(size_t y = 0; y < bmp->rows; y++ ){
            for (size_t x = 0; x < bmp->width; x++) {
                size_t src_index = (x + (y * (bmp->width)));
                size_t dst_index = ((x + pen.x) + ((y + pen.y) * width)) * 4;

                if (dst_index >= h.size() || src_index >= bmp->rows * bmp->width) continue;
                h[dst_index] = text_color.r;
                h[dst_index + 1] = text_color.g;
                h[dst_index + 2] = text_color.b;
                h[dst_index + 3] = bmp->buffer[src_index];
            }
        }
    }

#include <png.h>
void render_line(FT_Face face, hb_font_t* font, std::string text, std::vector<u8>& bmp,   point<u16> pen, int width, f32 lineheight, color text_color) {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);
    hb_shape(hb_ft_font_create_referenced(face), buf, NULL, 0);
    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    auto newlines = get_newlines(text);
    int newline_index = 0;
    int next_newline = [&]() {
        if (newlines.empty()) return 65535;
        else return int(newlines[newline_index]);
    }();

    FT_Load_Char( face, '|', FT_LOAD_RENDER);
     lineheight = (face)->glyph->bitmap.rows * 1.15;

    for (unsigned int i = 0; i < glyph_count; i++) {
        f32 x_offset  = glyph_pos[i].x_offset / 64.0f;
        f32 y_offset  = glyph_pos[i].y_offset / 64.0f;
        f32 x_advance = glyph_pos[i].x_advance / 64.0f;
        f32 y_advance = glyph_pos[i].y_advance / 64.0f;

        auto error = FT_Load_Char( face, text[i], FT_LOAD_RENDER);
        if ( error ) continue;

        if (i == next_newline) {
            pen.x = 0;
            pen.y += lineheight;
            newline_index++;
            next_newline = newline_index == newlines.size() ? 65535 : newlines[newline_index];
        }

        FT_Bitmap ftBitmap = (face)->glyph->bitmap;

        int top =  face->glyph->bitmap_top;
        float x0 = pen.x + x_offset + face->glyph->bitmap_left;
        float y0 = floor(pen.y + y_offset + (lineheight - 5 - face->glyph->bitmap_top));


        copy_bitmap(bmp, &ftBitmap, point<u16>(x0, y0), width, text_color);

        pen.x += x_advance;
        pen.y += y_advance;
    }

}
std::string s_text::replace_locale_macro(std::string& text) {
    auto it = data->locale_lookup.find(text);
    if (it == data->locale_lookup.end()) {
        return text;
    } else {
        return it->second;
    }
}

void s_text::run(pool<c_text>& texts, pool<c_display>& displays) {
    size<f32> atlas_size(0, 0);
    int num_text_entries = 0;
    for(auto& text : texts) {
        for (auto entry : text.text_entries) {
            if (entry.text == "") continue;
            auto dim = displays.get(text.parent).sprites(text.sprite_index).get_dimensions(entry.quad_index);
            atlas_size.y += dim.size.y;
            atlas_size.x = std::max(dim.size.x, atlas_size.x);
            num_text_entries++;
            if (entry.regen) {
                regenerate = true;
                entry.regen = false;
            }
        }
    }

    if (num_text_entries != last_atlassize) regenerate = true;
    if (num_text_entries == 0) return;
    last_atlassize = num_text_entries;
    if (regenerate == false) return;

    tex->image_data = image(std::vector<u8>(atlas_size.x * atlas_size.y * 4, 0), atlas_size.to<u16>());
    point<u16> pen(0, 0);

    for(auto text : texts) {
        for (auto entry : text.text_entries) {
            if (entry.text == "") continue;

            auto dim = displays.get(text.parent).sprites(text.sprite_index).get_dimensions(entry.quad_index);

            render_line(data->face, data->font, replace_locale_macro(entry.text), tex->image_data.data(), pen, atlas_size.x, dim.size.y, entry.text_color);

            point<f32> pos(0, pen.y / static_cast<f32>(atlas_size.y));
            size<f32> new_dim(dim.size.x / static_cast<f32>(atlas_size.x), dim.size.y / static_cast<f32>(atlas_size.y));

            displays.get(text.parent).sprites(text.sprite_index).set_uv(pos, new_dim, entry.quad_index);
            pen.y += dim.size.y ;
        }
        displays.get(text.parent).sprites(text.sprite_index).tex = tex;
        displays.get(text.parent).sprites(text.sprite_index).z_index = 9;
    }
    regenerate = false;
/*
    unsigned error = lodepng::encode("test.png", tex->image_data.data(), atlas_size.x, atlas_size.y);

    //if there's an error, display it
    if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;*/
}



screen_coords s_text::get_text_size(std::string& text) {
    std::string to_display = replace_locale_macro(text);
    auto newlines = get_newlines(to_display);
    int newline_index = 0;
    int num_newlines = newlines.empty() ? 1 : newlines.size() + 1;
    int next_newline = [&]() {
       if (newlines.empty()) return 65535;
       else return int(newlines[newline_index]);
    }();

    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_buffer_add_utf8(buf, to_display.c_str(), -1, 0, -1);
    hb_shape(hb_ft_font_create_referenced(data->face), buf, NULL, 0);
    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);


    FT_Load_Char( data->face, '|', FT_LOAD_RENDER);
    int lineheight = (data->face)->glyph->bitmap.rows * 1.15;

    screen_coords box_size(0, lineheight * (num_newlines));
    u16 row_size = 0;
    for (int i = 0; i < glyph_count; i++) {
        row_size += glyph_pos[i].x_advance / 64.0f;
        if (i == next_newline) {
            newline_index++;
            next_newline = newline_index == newlines.size() ? 65535 : newlines[newline_index];
            FT_Load_Char( data->face, to_display[i], FT_LOAD_RENDER);
            row_size += (data->face)->glyph->bitmap.width;

            box_size.x = std::max(box_size.x, row_size);
            row_size = 0;
        }
    }
    box_size.x = std::max(box_size.x, row_size);
    return box_size;
}
}