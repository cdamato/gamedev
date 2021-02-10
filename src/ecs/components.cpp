#include "components.h"

#include <utility>
#include <vector>
#include <set>



u8 c_collision:: get_team_detectors() {
	return flags.to_ulong() & 0b111;
}
u8 c_collision::get_team_signals() {
	return (flags.to_ulong() & 0b111000) >> 3;
}
u8 c_collision:: get_tilemap_collision() {
	return (flags.to_ulong() & 0b11000000) >> 6;
}


void c_collision::set_team_detector(collision_flags to_set) {
	flags |= std::bitset<8>(to_set);
}
void c_collision::set_team_signal(collision_flags to_set) {
	flags |= std::bitset<8>(to_set << 3);
}
void c_collision:: set_tilemap_collision(collision_flags to_set) {
	flags |= std::bitset<8>(to_set);
}

bool tile_data::is_occupied(u8 quad_index, u8 type) {
	if (type == 0) return false;
	u8 bitmask = type << (quad_index * 2);
	return (collision & std::bitset<8>(bitmask)) == bitmask;
}

void tile_data::set_data(u8 quad_index, u8 type) {
	std::bitset<8> aa(type << (quad_index * 2));
	collision |= aa;
}


