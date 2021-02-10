#include "ecs.h"

#include <thread>
#include <numeric>
#include <atomic>

std::vector<entity> entity_manager::remove_marked()
{
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

void entity_manager::mark_entity(entity id)
{
    const auto bucket_id = get_thread_id() % bucket_count;
    auto& bucket = buckets[bucket_id];
    const std::unique_lock lock(bucket.mutex);
    bucket.ids.push_back(id);
}

std::size_t entity_manager::get_thread_id() noexcept
{
	static std::atomic<std::size_t> generator = 0;
    const thread_local auto id = generator.fetch_add(1, std::memory_order_relaxed);
    return id;
}

entity_manager::entity_manager() {
	entity_freelist = std::vector<u32>(max_entities);
	std::iota (std::begin(entity_freelist), std::end(entity_freelist), 0); // Fill with 0, 1, ..., 99.
}




void ecs_engine::run_ecs() {


	auto& sprites = components.get_pool(type_tag<sprite>());
	auto& velocities = components.get_pool(type_tag<c_velocity>());
	auto& collisions = components.get_pool(type_tag<c_collision>());

	auto& mapdata = components.get_pool(type_tag<c_mapdata>());

	system_velocity_run(velocities, sprites);
	system_collison_run(collisions, sprites, *mapdata.begin());

    c_player& player = components.get_pool(type_tag<c_player>()).get(_player_id);
	sprite& player_sprite = get_component<sprite>(_player_id);
	c_weapon_pool& weapons = get_component<c_weapon_pool>(_player_id);
	shooting_player(systems.shooting, player_sprite, player, weapons);

	// Health!
	auto& healths = components.get_pool(type_tag<c_health>());
	auto& damages = components.get_pool(type_tag<c_damage>());
	auto& healthbars = components.get_pool(type_tag<c_healthbar>());
	systems.health.run(healths, damages, healthbars, entities);
	systems.health.update_healthbars(healthbars, healths, sprites);

    auto& proximities = components.get_pool(type_tag<c_proximity>());
    auto& keyevents = components.get_pool(type_tag<c_keyevent>());
    systems.proxinteract.run(proximities, keyevents, sprites.get(_player_id));

	auto& texts = components.get_pool(type_tag<c_text>());
	systems.text.run(texts, sprites);

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




