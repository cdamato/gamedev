
#include <engine/engine.h>
#include <common/noise.h>
#include <random>

std::random_device rd;
std::mt19937 gen = std::mt19937(rd());
int mt_random(int a, int b)
{
	std::uniform_int_distribution<> dis(a, b);
	return dis(gen);
}

void set_tilecollison_lookup(c_mapdata& data) {

	data.tiledata_lookup.resize(15);

	data.tiledata_lookup[2].set_data(3, 0b11);


	data.tiledata_lookup[3].set_data(2, 0b11);
	data.tiledata_lookup[3].set_data(3, 0b11);


	data.tiledata_lookup[4].set_data(2, 0b11);


	data.tiledata_lookup[5].set_data(1, 0b11);
	data.tiledata_lookup[5].set_data(2, 0b11);
	data.tiledata_lookup[5].set_data(3, 0b11);

	data.tiledata_lookup[6].set_data(0, 0b11);
	data.tiledata_lookup[6].set_data(2, 0b11);
	data.tiledata_lookup[6].set_data(3, 0b11);

	data.tiledata_lookup[7].set_data(1, 0b11);
	data.tiledata_lookup[7].set_data(3, 0b11);

	data.tiledata_lookup[8].set_data(0, 0b11);
	data.tiledata_lookup[8].set_data(1, 0b11);
	data.tiledata_lookup[8].set_data(2, 0b11);
	data.tiledata_lookup[8].set_data(3, 0b11);


	data.tiledata_lookup[9].set_data(0, 0b11);
	data.tiledata_lookup[9].set_data(2, 0b11);


	data.tiledata_lookup[10].set_data(0, 0b11);
	data.tiledata_lookup[10].set_data(1, 0b11);
	data.tiledata_lookup[10].set_data(3, 0b11);


	data.tiledata_lookup[11].set_data(0, 0b11);
	data.tiledata_lookup[11].set_data(1, 0b11);
	data.tiledata_lookup[11].set_data(2, 0b11);

	data.tiledata_lookup[12].set_data(1, 0b11);

	data.tiledata_lookup[13].set_data(0, 0b11);
	data.tiledata_lookup[13].set_data(1, 0b11);

	data.tiledata_lookup[14].set_data(0, 0b11);
}

enum directions {
	NW,
	N,
	NE,
	W,
	E,
	SW,
	S,
	SE,


	cardinals = 1 << N | 1 << E | 1 << S | 1 << W,
	ordinals = 1 << NE | 1 << NW | 1 <<SE | 1 <<SW,
};

void read_neighbors(std::vector<u8>& tiles, int neighbors[8], int x_in, int y_in, size<u16> map_size) {
	//neighbors = {0};

	int start_x = std::max(0, x_in - 1);
	int start_y = std::max(0, y_in - 1);

	for (int y = start_y; y <= std::min(y_in + 1, int(map_size.h - 1)); y++) {
		for (int x = start_x; x <= std::min(x_in + 1, int(map_size.w - 1)); x++) {
			if (x == x_in && y == y_in) continue;
			int neighbor_index = (x - (x_in - 1)) + ((y - (y_in - 1)) * 3);
			if (neighbor_index >= 4) neighbor_index -= 1;
			neighbors[neighbor_index] = tiles[x + (y * map_size.w)];
		}
	}
}


void set_map(engine& game, size<u16> dimensions, std::vector<u8> tiles) {

	f32 tex_w = 1.0f / 5.0f;
	f32 tex_h = 1.0f / 3.0f;

	entity e = game.map_id();

    game.ecs.remove_component<sprite>(e);
    sprite& spr = game.ecs.add_component<sprite>(e);

    c_mapdata& mapdata = game.ecs.get_component<c_mapdata>(e);

    set_tilecollison_lookup(mapdata);

	spr.gen_subsprite(tiles.size(), render_layers::sprites);
	spr.get_subsprite(0).tex = game.renderer().get_texture("tilemap");

	spr.set_pos(point<f32>(0,0), size<f32>(dimensions.w, dimensions.h), 0, 0);

	size_t index = 0;
	u8 tile_type = 0;

	std::vector<u8> modified_map(tiles.size());
	auto write_tile = [&] (f32 pos_x, f32 pos_y, f32 tex_x, f32 tex_y) {
		tex_x *= tex_w;
		tex_y *= tex_h;
		spr.set_pos(point<f32>(pos_x, pos_y), size<f32>(1, 1), 0, index);
		spr.set_uv(point<f32>(tex_x, tex_y), size<f32>(tex_w, tex_h), 0, index);
		index++;

	};

	int neighbors[8] = {tile_type};
	for (int y = 0; y < dimensions.h; y++) {
		for (int x = 0; x < dimensions.w; x++) {

			if (tiles[index] == 1) {
				int type =  mt_random(0, 100) < 25 ? 0 : 0;
				write_tile(x, y, type, 0);
				continue;
			}
			if (tiles[index]  != tile_type) {
				index++;
				continue;
			}

			// each bit marks a neighbor that is a different tile type
			std::bitset<8> flags;

			read_neighbors(tiles, neighbors, x, y, dimensions);

			for (int i = 0; i < 8; i++) {
				if (neighbors[i] != tile_type)
					flags.set(i);
				neighbors[i] = tile_type;
			}

			if (flags.none()) {
				write_tile(x, y, 3, 1);
				continue;
			}

			else if((flags.to_ulong() & directions::cardinals) == 0) {
				f32 tex_y = (flags.test(NW) || flags.test(NE))  ? 1 : 2;
				f32 tex_x = (flags.test(NW) || flags.test(SW))  ? 0 : 1;

				modified_map[index] = (tex_y * 5) + tex_x;
				write_tile(x, y, tex_x, tex_y);
				continue;
			} else {
				f32 tex_x = (3 - flags.test(W) + flags.test(E));
				f32 tex_y = (1 - flags.test(N) + flags.test(S));

				modified_map[index] = (tex_y * 5) + tex_x;
				write_tile(x, y, tex_x, tex_y);
				continue;
			}


			index++;

			}
		}

	tiles = modified_map;


	mapdata.map = tiles;
	mapdata.size = dimensions;
}

void gen_obstaclething(entity e, engine& game, point<f32> pos) {

	sprite& spr = game.ecs.add_component<sprite>(e);
	//spr->init(1);
	spr.gen_subsprite(1, render_layers::sprites);
	spr.get_subsprite(0).tex = game.renderer().get_texture("crate");

	spr.set_pos(pos, size<f32>(1, 1), 0, 0);
	spr.set_uv(point<f32>(0, 0), size<f32>(1.0f, 1.0f), 0, 0);
}

std::vector<u8> generate_map(size<u16> map_size) {

	PerlinNoise pnr(mt_random(0, 65535));

	map_size = size<u16>(32, 32);

	f32 min = 1;
	f32 max = 0;

	std::vector<f32> tile_raw (map_size.w * map_size.h);

	size_t index = 0;
	for (int i = 1; i < map_size.h + 1; i++) {
		for (int j = 1; j < map_size.w + 1; j++) {

			double x = (double)j / ((double) map_size.w + 1);
			double y = (double)i / ((double) map_size.h + 1);

			f32 val = fabs(pnr.noise(x, y));
			if (val < min) min = val;
			if (val > max) max = val;

			tile_raw[index] = val;
			index += 1;
		}
	}

	std::vector<u8> map(tile_raw.size());

	int type_count = 3;
	index = 0;
	f32 denom = max - min;
	for (int y = 0; y < map_size.h; y++) {
		for (int x = 0; x < map_size.w; x++) {

			f32 val = (tile_raw[index] - min) / denom;
			f32 range = 1.0f / type_count;
			for(int i = 0; i < type_count; i++) {
				if (val > i * range && val < (i + 1) * range) {
					map[index] = i;
					printf("%i", i);
					break;
				}
			}
			index += 1;
		}
		printf("\n");
	}

	printf("\n\n");
	return map;
}




