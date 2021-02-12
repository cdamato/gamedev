#include "systems.h"
#include "ecs.h"
#include <cmath>

/***********************************/
/*     SAT Collision Detection     */
/***********************************/

bool is_subsprite_disabled( size_t i, c_collision& col) {
    for (auto it : col.disabled_subsprites)
        if (it == i) return true;
    return false;
}

std::vector<point<f32>> get_normals(sprite& spr, c_collision& col){
	std::vector<point<f32>> axes;
	size_t index = 0;
	for (size_t i = 0; i < spr.num_subsprites(); i++) {
		if (is_subsprite_disabled(i, col)) continue;

		index = spr.get_subsprite(i).start;
		size_t max_size = index + (spr.get_subsprite(i).size * 4);

		for (size_t j = index; j != max_size; j ++) {
			size_t vert_index = j + 1 == max_size ? 0 : j + 1;
			point<f32> edge = spr.vertices()[vert_index].pos - spr.vertices()[j].pos;
			axes.push_back(point<f32>(-edge.y, edge.x));
		}
	}
	return axes;
}

void project_shape(sprite& spr, c_collision& col,point<f32> normal, f32& min, f32& max) {
	size_t index = 0;
	for (size_t i = 0; i < spr.num_subsprites(); i++) {
		if (is_subsprite_disabled(i, col)) continue;

		index = spr.get_subsprite(i).start;

		for (size_t j = index; j != index + (spr.get_subsprite(i).size * 4); j ++) {
			point<f32> pos = spr.vertices()[j].pos;
			f32 projection = (pos.x * normal.x) + (pos.y * normal.y);
			min = std::min(min, projection);
			max = std::max(max, projection);
		}
	}
}

bool test_normalset(sprite& a_spr, c_collision& a_col, sprite& b_spr, c_collision& b_col,
		std::vector<point<f32>>& normals) {
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

bool test_collision(sprite& a, c_collision& a_col, sprite& b, c_collision& b_col){
	std::vector<point<f32>> a_axes = get_normals(a, a_col);
	std::vector<point<f32>> b_axes = get_normals(b, b_col);

	return test_normalset(a, a_col, b, b_col, a_axes) || test_normalset(a, a_col, b, b_col, b_axes);
}

tile_data get_tiledata(c_mapdata& data, point<f32> pos) {
	point<f32> trunc_pos(trunc(pos.x), trunc(pos.y));

	int tile_index =  trunc_pos.x + ( trunc_pos.y * data.size.w);
	return data.tiledata_lookup[data.map[tile_index]];
}


bool map_collision (sprite& spr, c_mapdata& data, u8 type) {
	for (auto vertex : spr.vertices()) {
		// get the decimal part, then divide it by 0.50 to determine which half it's in, then floor to get an int

		point<f32> pos = vertex.pos;
		if ( pos.x < 0 ||  pos.y < 0) return true;


		auto collision_data = get_tiledata(data, pos);
		point<f32> trunc_pos(trunc(pos.x), trunc(pos.y));
		int quadrant_x = (pos.x - trunc_pos.x) > 0.50f ? 1 : 0 ;
		int quadrant_y = (pos.y - trunc_pos.y) > 0.50f ? 1 : 0;
		int subquad_index = quadrant_x + (quadrant_y * 2);

		if( collision_data.is_occupied(subquad_index, type)) return true;

	}
	return false;
}

void system_collison_run(pool<c_collision>& collisions, pool<sprite>& sprites, c_mapdata& data)
{
	for (auto col_a : collisions) {
		sprite spr_a = sprites.get(col_a.ref);

		//if (map_collision(spr_a, data, col_a.get_tilemap_collision())) {
		//	col_a.on_collide(data.ref);
		//}

        u8 a_sig = col_a.get_team_signals();
        u8 a_dec = col_a.get_team_detectors();

		auto col_it = pool<c_collision>::iterator(col_a.ref + 1, &collisions);
		for (; col_it != collisions.end(); ++col_it) {
			auto col_b = *col_it;

			u8 b_sig = col_b.get_team_signals();
			u8 b_dec = col_b.get_team_detectors();


			if (((a_sig & b_dec) == 0) && ((b_sig & a_dec) == 0)) continue;
            sprite spr_b = sprites.get(col_b.ref);


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


void s_health::run(pool<c_health>& healths, pool<c_damage>& damages,
		pool<c_healthbar>& healthbars, entity_manager& entities)
{
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


static color to_rgb(f32 h, f32 s, f32 v, f32 max = 255)
{
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
        data.image_data.write(i, c);
	}
}

void s_health::update_healthbars(pool<c_healthbar>& healthbars, pool<c_health>& healths, pool<sprite>& sprites)
{
	if (healthbars.size() != last_atlassize) regenerate = true;
	last_atlassize = healthbars.size();

//	healthbar_atlas.set_data(texture_data, 8, 4 * healthbars.size(), 0x1908 );
	if (!regenerate || healthbars.size() == 0) {
		return;
	}

	size_t index = 0;
    size<u16> tex_size = size<u16>(8, 4 * healthbars.size());
	data.image_data = image(std::vector<u8>(tex_size.w * tex_size.h * 4), tex_size);
    data.z_index = 2;

	size<f32> slice_size(1, 1.0f / healthbars.size());
	point<f32> pos(1, 0);

	for (auto& healthbar : healthbars) {
		c_health& health = healths.get(healthbar.ref);
		healthbar.tex_index = index;
		// This is a new healthbar, not yet hooked up to sprites
		sprite& spr = sprites.get(healthbar.ref);

		if (healthbar.subsprite_index == 0) {
			point<f32> tl = spr.get_dimensions().bottom_left() + point<f32>(0, 0.1);
			u32 index = spr.gen_subsprite(1, render_layers::sprites);
			healthbar.subsprite_index = index;
			spr.get_subsprite(index).tex = tex;
			size<f32> size(1, 0.25);

			spr.set_pos(tl, size, 0, spr.get_subsprite(index).start );
		}

		spr.set_uv(pos, slice_size, 0, healthbar.subsprite_index);
		set_entry(index, (health.health / health.max_health) * 100.0f);

		pos.y += slice_size.h;
		index++;
	}

//	healthbar_atlas.set_data(texture_data, width, height, 0x1908 );
	regenerate = false;
}

/**************************/
/*     s_proxinteract     */
/**************************/

bool AABB_collision(rect<f32> a, rect<f32> b) {
    return (a.origin.x <= b.top_right().x && a.top_right().x >= b.origin.x) &&
            (a.origin.y <= b.bottom_left().y && a.bottom_left().y >= b.origin.y);

}

bool test_proximity_collision(c_proximity& c, sprite& spr) {
    if(c.shape == c_proximity::shape::rectangle)
        return AABB_collision(rect<f32>(c.origin, c.radii), spr.get_dimensions(0));
    if(c.shape == c_proximity::shape::elipse) {
        point<f32> p = spr.get_dimensions(0).center();
        f32 left_side = pow(p.x - c.origin.x, 2) / pow(c.radii.w, 2);
        f32 right_side = pow(p.y - c.origin.y, 2) / pow(c.radii.h, 2);

        return left_side + right_side <= 1;
    }

    return false;
}
template <typename T>
f32 distance(T a, T b) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
}

void s_proxinteract::run(pool<c_proximity>& proximities, pool <c_keyevent>& keyevents, sprite& player_spr) {
    f32 min = 1000;
    entity current_min = 65535;
    for(auto& proximity : proximities) {
        if (!keyevents.exists(proximity.ref)) continue;
        if (test_proximity_collision(proximity, player_spr)) {
          f32 score = distance(proximity.origin, player_spr.get_dimensions(0).origin);
          if (score < min) {
              min = score;
              current_min = proximity.ref;
          }
        }
    }

    active_interact = current_min;
}

/*********************************/
/*     Free Function Systems     */
/*********************************/

void system_velocity_run(pool<c_velocity>& velocities, pool<sprite>& sprites) {
	for (auto velocity : velocities) {
		sprite& spr = sprites.get(velocity.ref);
		spr.move_by(velocity.delta);
	}
}


void shooting_player(s_shooting& system, sprite& spr, c_player& p, c_weapon_pool& weapons) {
	weapon active = weapons.weapons.get(weapons.current);
	if (p.shoot == true && active.t.elapsed<timer::ms>() > timer::ms(active.stats.cooldown)) {
		s_shooting::bullet& b = system.bullet_types[active.stats.bullet];
		system.shoot(b.tex, b.dimensions, b.speed, spr.get_dimensions().center(),
				p.target, collision_flags::ally);
		active.t.start();
	}

	p.shoot = false;
}


/******************************/
/*     Text Renderer Code     */
/******************************/

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "systems.h"


struct s_text::impl {
	FT_Library library;
	FT_Face face;
};

f32 calc_fontsize(size<f32> box) {
	u8 dpi = 96;
	f32 lineheight_inches = ((box.h) / static_cast<f32>(dpi));
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


void render_line(FT_Face face, std::string text, std::vector<u8>& bmp,
		point<u16> pen, int width, f32 lineheight) {
	pen.y += lineheight * 0.755;
	for ( size_t n = 0; n < text.length(); n++ )	{
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



s_text::s_text() {
	data = new impl;
	FT_Init_FreeType( &data->library );
	FT_New_Face( data->library, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &data->face);
}
#include <common/png.h>

void s_text::run(pool<c_text>& texts, pool<sprite>& sprites) {

	if (texts.size() != last_atlassize) reconstruct_atlas = true;
	if (reconstruct_atlas == false) return;

	size<f32> atlas_size(0, 0);

	for(auto sprite : sprites) {
	    if (!texts.exists(sprite.ref)) continue;
		auto dim =  sprite.get_dimensions(texts.get(sprite.ref).subsprite_index);
		atlas_size.h += dim.size.h;
		atlas_size.w = std::max(dim.size.w, atlas_size.w);
	}

	texture_data = std::vector<u8>(atlas_size.w * atlas_size.h);

	point<u16> pen(0, 0);

	for(auto text : texts) {

		auto dim =  sprites.get(text.ref).get_dimensions(text.subsprite_index);
		u8 num_lines = num_newlines(text.text);
		f32 fontsize = (calc_fontsize(dim.size) / num_lines) / 2;

		FT_Set_Char_Size( data->face, fontsize * 64, 0, 100, 0 );

		render_line( data->face, text.text, texture_data, pen,
				atlas_size.w, dim.size.h / num_lines);

		point<f32> pos(0, pen.y / static_cast<f32>(atlas_size.h));
		size<f32> new_dim(dim.size.w / static_cast<f32>(atlas_size.w), dim.size.h / static_cast<f32>(atlas_size.h));

		sprites.get(text.ref).set_uv(pos, new_dim, 0, text.subsprite_index);
		sprites.get(text.ref).get_subsprite(text.subsprite_index).tex = atlas;
		pen.y += dim.size.h ;
	}

//	atlas.set_data(atlas_data, atlas_size.w, atlas_size.h, 0x1903);
}

