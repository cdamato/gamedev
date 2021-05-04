#include <numeric>
#include <atomic>
#include <common/png.h>
#include <cmath>
#include <ft2build.h>
#include "ecs.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#define VERTICES_PER_QUAD 4

/***************************/
/*     ECS ENGINE CODE     */
/***************************/

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
	systems.health.run(pool<c_health>(), pool<c_damage>(), pool<c_healthbar>(), entities);
	systems.health.update_healthbars(pool<c_healthbar>(), pool<c_health>(), pool<c_display>());
    systems.proxinteract.run(pool<c_proximity>(), pool<c_keyevent>(), pool<c_display>().get(_player_id));
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

/******************************/
/*     ECS COMPONENT CODE     */
/******************************/

u8 c_collision:: get_team_detectors() { return flags.to_ulong() & 0b111; }
u8 c_collision::get_team_signals() { return (flags.to_ulong() & 0b111000) >> 3; }
u8 c_collision:: get_tilemap_collision() { return (flags.to_ulong() & 0b11000000) >> 6; }
void c_collision::set_team_detector(collision_flags to_set) { flags |= std::bitset<8>(to_set); }
void c_collision::set_team_signal(collision_flags to_set) { flags |= std::bitset<8>(to_set << 3); }
void c_collision:: set_tilemap_collision(collision_flags to_set) { flags |= std::bitset<8>(to_set); }

bool tile_data::is_occupied(u8 quad_index, u8 type) {
    if (type == 0) return false;
    u8 bitmask = type << (quad_index * 2);
    return (collision & std::bitset<8>(bitmask)) == bitmask;
}

void tile_data::set_data(u8 quad_index, u8 type) {
    std::bitset<8> aa(type << (quad_index * 2));
    collision |= aa;
}

u32 c_display::add_sprite(u16 num_quads, render_layers layer) {
    _sprites.push_back(sprite_data{nullptr, num_quads, _vertices.size() / VERTICES_PER_QUAD, nullptr, layer});
    _vertices.resize(_vertices.size() + (VERTICES_PER_QUAD * num_quads));
    vertex* ptr = _vertices.data();
    for (auto& sprite : _sprites) {
        sprite.vertices = ptr;
        ptr += sprite.size * VERTICES_PER_QUAD;
    }
    return _sprites.size() - 1;
}

// Rotate sprite by an angle in radians. For proper rotation, origin needs to be in the center of the object.
void c_display::rotate(f32 theta) {
    f32 s = sin(theta);
    f32 c = cos(theta);
    rect<f32> dimensions = get_dimensions();
    sprite_coords center = dimensions.center();
    for (auto& vert : _vertices) {
        f32 i = vert.pos.x - center.x;
        f32 j = vert.pos.y - center.y;
        vert.pos.x = ((i * c) - (j * s)) + center.x;
        vert.pos.y = ((i * s) + (j * c)) + center.y;
    }
}

void sprite_data::set_pos(sprite_coords pos, sprite_coords size, size_t quad) {
    // Vertices are in order top left, top right, bottom right, and bottom left
    size_t index = (quad) * VERTICES_PER_QUAD;
    vertices[index].pos = pos;
    vertices[index + 1].pos = sprite_coords(pos.x + size.x, pos.y);
    vertices[index + 2].pos = pos + sprite_coords(size.x, size.y);
    vertices[index + 3].pos = sprite_coords(pos.x, pos.y + size.y);
}

void sprite_data::set_tex_region(size_t tex_index, size_t quad) {
    ::size<f32> region_size(1.0f / tex->regions.x, 1.0f / tex->regions.y);
    ::size<f32> pos = region_size * ::size<f32>(tex_index % tex->regions.x, tex_index / tex->regions.x);
    // Vertices are in order top left, top right, bottom right, and bottom left
    size_t index = (quad) * VERTICES_PER_QUAD;
    vertices[index].uv = pos;
    vertices[index + 1].uv = point<f32>(pos.x + region_size.x, pos.y);
    vertices[index + 2].uv = pos + point<f32>(region_size.x, region_size.y);
    vertices[index + 3].uv = point<f32>(pos.x, pos.y + region_size.y);
}

void c_display::move_to(sprite_coords pos) {
    sprite_coords change = pos - _vertices[0].pos;
    move_by(change);
}

void c_display::move_by(sprite_coords change) {
    for (auto& vert : _vertices) {
        vert.pos += change;
    }
}

rect<f32> calc_dimensions(vertex* vert_array, size_t begin, size_t end) {
    sprite_coords min(65535, 65535);
    sprite_coords max(0, 0);

    for (size_t i = begin; i < end ; i++) {
        auto vertex = vert_array[i];
        min.x = std::min(min.x, vertex.pos.x);
        min.y = std::min(min.y, vertex.pos.y);

        max.x = std::max(max.x, vertex.pos.x);
        max.y = std::max(max.y, vertex.pos.y);
    }

    sprite_coords s = max - min;
    return rect<f32>(min, sprite_coords(s.x, s.y));
}

rect<f32> c_display::get_dimensions() {
    return calc_dimensions(_vertices.data(), 0,  _vertices.size());
}

rect<f32> sprite_data::get_dimensions() {
    return calc_dimensions(vertices, 0, size);
}
/***************************/
/*     ECS SYSTEM CODE     */
/***************************/

/***********************************/
/*     SAT Collision Detection     */
/***********************************/

bool is_sprite_disabled(size_t i, c_collision& col) {
    for (auto it : col.disabled_sprites)
        if (it == i) return true;
    return false;
}

std::vector<world_coords> get_normals(c_display& spr, c_collision& col) {
    std::vector<world_coords> axes;
    size_t index = 0;
    for (size_t i = 0; i < spr.num_sprites(); i++) {
        if (is_sprite_disabled(i, col)) continue;

        index = spr.sprites(i).start;
        size_t max_size = index + (spr.sprites(i).size * 4);

        for (size_t j = index; j != max_size; j ++) {
            size_t vert_index = j + 1 == max_size ? 0 : j + 1;
            world_coords edge = spr.vertices()[vert_index].pos - spr.vertices()[j].pos;
            axes.push_back(world_coords(-edge.y, edge.x));
        }
    }
    return axes;
}

void project_shape(c_display& spr, c_collision& col, world_coords normal, f32& min, f32& max) {
    size_t index = 0;
    for (size_t i = 0; i < spr.num_sprites(); i++) {
        if (is_sprite_disabled(i, col)) continue;

        index = spr.sprites(i).start;

        for (size_t j = index; j != index + (spr.sprites(i).size * 4); j ++) {
            world_coords pos = spr.vertices()[j].pos;
            f32 projection = (pos.x * normal.x) + (pos.y * normal.y);
            min = std::min(min, projection);
            max = std::max(max, projection);
        }
    }
}

bool test_normalset(c_display& a_spr, c_collision& a_col, c_display& b_spr, c_collision& b_col, std::vector<world_coords>& normals) {
    for (auto normal : normals) {
        f32 a_min = 65535, a_max = 0, b_min = 65535, b_max = 0;

        project_shape(a_spr, a_col, normal, a_min, a_max);
        project_shape(b_spr, b_col, normal, b_min, b_max);

        bool a = (a_min < b_max) && (a_min > b_min);
        bool b = (b_min < a_max) && (b_min > a_min);
        if ((a || b) == false) {
            return false;
        }
    }
    return true;
}

bool test_collision(c_display& a, c_collision& a_col, c_display& b, c_collision& b_col) {
    std::vector<world_coords> a_axes = get_normals(a, a_col);
    std::vector<world_coords> b_axes = get_normals(b, b_col);

    return test_normalset(a, a_col, b, b_col, a_axes) || test_normalset(a, a_col, b, b_col, b_axes);
}

tile_data get_tiledata(c_mapdata& data, world_coords pos) {
    world_coords trunc_pos(trunc(pos.x), trunc(pos.y));

    int tile_index =  trunc_pos.x + ( trunc_pos.y * data.size.x);
    return data.tiledata_lookup[data.map[tile_index]];
}


bool map_collision (c_display& spr, c_mapdata& data, u8 type) {
    for (auto vertex : spr.vertices()) {
        // get the decimal part, then divide it by 0.50 to determine which half it's in, then floor to get an int

        world_coords pos = vertex.pos;
        if ( pos.x < 0 ||  pos.y < 0) return true;


        auto collision_data = get_tiledata(data, pos);
        world_coords trunc_pos(trunc(pos.x), trunc(pos.y));
        int quadrant_x = (pos.x - trunc_pos.x) > 0.50f ? 1 : 0 ;
        int quadrant_y = (pos.y - trunc_pos.y) > 0.50f ? 1 : 0;
        int subquad_index = quadrant_x + (quadrant_y * 2);

        if( collision_data.is_occupied(subquad_index, type)) return true;

    }
    return false;
}

void system_collison_run(pool<c_collision>& collisions, pool<c_display>& sprites, c_mapdata& data) {
    for (auto col_a : collisions) {
        c_display& spr_a = sprites.get(col_a.ref);
        //if (map_collision(spr_a, data, col_a.get_tilemap_collision())) { col_a.on_collide(data.ref); }
        u8 a_sig = col_a.get_team_signals();
        u8 a_dec = col_a.get_team_detectors();

        auto col_it = pool<c_collision>::iterator(col_a.ref + 1, &collisions);
        for (; col_it != collisions.end(); ++col_it) {
            auto col_b = *col_it;
            u8 b_sig = col_b.get_team_signals();
            u8 b_dec = col_b.get_team_detectors();
            if (((a_sig & b_dec) == 0) && ((b_sig & a_dec) == 0)) continue;

            c_display& spr_b = sprites.get(col_b.ref);
            if (test_collision(spr_a, col_a, spr_b, col_b)) {
                col_b.on_collide(col_a.ref);
                col_a.on_collide(col_b.ref);
            }
        }
    }
}

/*********************************/
/*     Health/Healthbar Code     */
/*********************************/

void s_health::run(pool<c_health>& healths, pool<c_damage>& damages, pool<c_healthbar>& healthbars, entity_manager& entities) {
    for(auto& damage : damages) {
        if (damage.enemy_ID == 65535) continue;
        entity enemy = damage.enemy_ID;
        damage.enemy_ID = 65535;
        if (!healths.exists(enemy)) continue;

        auto& health = healths.get(enemy);
        health.health -= damage.damage;

        if (healthbars.exists(health.ref))
            set_entry(healthbars.get(health.ref).tex_index, health.health);

        if (health.health <= 0) {
            regenerate = true;
            entities.mark_entity(health.ref);
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

void s_health::update_healthbars(pool<c_healthbar>& healthbars, pool<c_health>& healths, pool<c_display>& sprites) {
    if (healthbars.size() != last_atlassize) regenerate = true;
    last_atlassize = healthbars.size();

    if (!regenerate || healthbars.size() == 0) return;

    size_t index = 0;
    size<u16> tex_size = size<u16>(8, 4 * healthbars.size());
    tex->image_data = image(std::vector<u8>(tex_size.x * tex_size.y * 4), tex_size);
    tex->z_index = 2;
    tex->regions = size<u16>(1, healthbars.size());

    size<f32> slice_size(1, 1.0f / healthbars.size());

    for (auto& healthbar : healthbars) {
        c_health& health = healths.get(healthbar.ref);
        healthbar.tex_index = index;
        // This is a new healthbar, not yet hooked up to sprites
        c_display& spr = sprites.get(healthbar.ref);

        if (healthbar.sprite_index == 0) {
            sprite_coords tl = spr.get_dimensions().bottom_left() + sprite_coords(0, 0.1);
            u32 index = spr.add_sprite(1, render_layers::sprites);
            healthbar.sprite_index = index;
            spr.sprites(index).tex = tex;
            size<f32> size(1, 0.25);

            spr.sprites(healthbar.sprite_index).set_pos(tl, size, 0);
        }

        spr.sprites(healthbar.sprite_index).set_tex_region(index, 0);
        set_entry(index, (health.health / health.max_health) * 100.0f);

        index++;
    }

//	healthbar_atlas.set_data(texture_data, width, height, 0x1908 );
    regenerate = false;
}

/**************************/
/*     s_proxinteract     */
/**************************/



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

void s_proxinteract::run(pool<c_proximity>& proximities, pool <c_keyevent>& keyevents, c_display& player_spr) {
    f32 min = 1000;
    entity current_min = 65535;
    for(auto& proximity : proximities) {
        if (!keyevents.exists(proximity.ref)) continue;
        if (test_proximity_collision(proximity, player_spr)) {
            f32 score = distance(proximity.origin, player_spr.sprites(0).get_dimensions().origin);
            if (score < min) {
                min = score;
                current_min = proximity.ref;
            }
        }
    }
    active_interact = current_min;
}

/**************************/
/*     Misc. Systems     */
/*************************/

void system_velocity_run(pool<c_velocity>& velocities, pool<c_display>& sprites, int framerate_multiplier) {
    for (auto velocity : velocities) {
        c_display& spr = sprites.get(velocity.ref);
        sprite_coords new_velocity (velocity.delta.x / framerate_multiplier, velocity.delta.y / framerate_multiplier);
        spr.move_by(new_velocity);
    }
}

void s_shooting::run(pool<c_display>& sprites, pool<c_weapon_pool> weapons, c_player& p) {
    auto& pool =  weapons.get(p.ref);
    weapon active = pool.weapons.get(pool.current);
    if (p.shoot == true && active.t.elapsed<timer::ms>() > timer::ms(active.stats.cooldown)) {
        bullet& b = bullet_types[active.stats.bullet];
        shoot(b.tex, b.dimensions, b.speed, sprites.get(p.ref).get_dimensions().center(),
                     p.target, collision_flags::ally);
        active.t.start();
    }
    p.shoot = false;
}

/******************************/
/*     Text Renderer Code     */
/******************************/

struct s_text::impl {
   // FT_Library library;
    //FT_Face face;
};

s_text::s_text() {
    data = new impl;
   // FT_Init_FreeType( &data->library );
  //  FT_New_Face( data->library, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &data->face);
}
/*
f32 calc_fontsize(size<f32> box) {
    u8 dpi = 96;
    f32 lineheight_inches = ((box.y) / static_cast<f32>(dpi));
    return (72.0f / (1.0f / lineheight_inches)) * 0.9;
}

u8 num_newlines(std::string text) {
    u32 num_lines = 0;
    size_t index = 0;
    for (;;) {
        index = text.find("\n", index + 1);
        if (index == std::string::npos) break;
        num_lines++;
    }
    return num_lines;
}

void copy_bitmap(std::vector<u8>& h, FT_Bitmap* bmp,point<u16> pen, int width) {
    for(size_t y = 0; y < bmp->rows; y++ ){
        for (size_t x = 0; x < bmp->width; x++) {
            size_t src_index = (x + (y * (bmp->width)));
            size_t dst_index = ((x + pen.x) + ((y + pen.y) * width));

            h[dst_index] = bmp->buffer[src_index];
        }
    }
}

void render_line(FT_Face face, std::string text, std::vector<u8>& bmp, point<u16> pen, int width, f32 lineheight) {
    pen.y += lineheight * 0.755;
    for ( size_t n = 0; n < text.length(); n++) {
        if(text[n] == '\n') {
            pen.y += lineheight;
            pen.x = 0;
            continue;
        }
        auto error = FT_Load_Char( face, text[n], FT_LOAD_RENDER );
        if ( error ) continue;

        int new_y = std::max(pen.y - (face->glyph->metrics.horiBearingY / 64), 0L);
        copy_bitmap(bmp, &face->glyph->bitmap, point<u16>(pen.x + face->glyph->bitmap_left, new_y), width );
        pen.x += face->glyph->advance.x >> 6;
    }
}
*/

void s_text::run(pool<c_text>& texts, pool<c_display>& sprites) {/*
    if (texts.size() != last_atlassize) reconstruct_atlas = true;
    if (reconstruct_atlas == false) return;

    size<f32> atlas_size(0, 0);

    for(auto& sprite : sprites) {
        if (!texts.exists(sprite.ref)) continue;
        auto dim =  sprite.get_dimensions(texts.get(sprite.ref).sprite_index);
        atlas_size.y += dim.size.y;
        atlas_size.w = std::max(dim.size.x, atlas_size.x);
    }

    texture_data = std::vector<u8>(atlas_size.w * atlas_size.y);

    point<u16> pen(0, 0);

    for(auto text : texts) {
        auto dim =  sprites.get(text.ref).get_dimensions(text.sprite_index);
        u8 num_lines = num_newlines(text.text);
        f32 fontsize = (calc_fontsize(dim.size) / num_lines) / 2;

        FT_Set_Char_Size( data->face, fontsize * 64, 0, 100, 0 );

        render_line( data->face, text.text, texture_data, pen,
                     atlas_size.x, dim.size.y / num_lines);

        point<f32> pos(0, pen.y / static_cast<f32>(atlas_size.y));
        size<f32> new_dim(dim.size.w / static_cast<f32>(atlas_size.x), dim.size.y / static_cast<f32>(atlas_size.y));

        sprites.get(text.ref).set_uv(pos, new_dim, 0, text.sprite_index);
        sprites.get(text.ref).sprites(text.sprite_index).tex = atlas;
        pen.y += dim.size.y ;
    }*/
}