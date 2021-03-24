#ifndef ECS_H
#define ECS_H

#include <vector>
#include <mutex>

#include "components.h"
#include "systems.h"


class entity_manager {
public:
    entity_manager();

	entity add_entity();
    void mark_entity(entity id);
    std::vector<entity> remove_marked();
private:
    constexpr static u8 bucket_count = 4;
    struct bucket {
        std::mutex mutex;
        std::vector<entity> ids;
    };

    std::array<bucket, 4> buckets;
	std::vector<u32> entity_freelist;

    std::size_t get_thread_id() noexcept;
};

class ecs_engine {
public:
	template <typename T>
	void remove(entity e) { components.get_pool(type_tag<T>()).remove(e); }

	template <typename T>
	constexpr T& get(entity e) {
	    auto& pool = components.get_pool(type_tag<T>());
	    if (!pool.exists(e)) throw "bruh";
	    return pool.get(e);
	}

	template <typename T>
	constexpr bool exists(entity e) {return components.get_pool(type_tag<T>()).exists(e); }

	template <typename T>
	constexpr T& add(entity e) {
		auto& c = components.get_pool(type_tag<T>()).add(e, T());
		c.ref = e;
		return c;
	}

	template <typename T>
    constexpr pool<T>& pool() { return components.get_pool(type_tag<T>()); }

private:
	entity_manager entities;
	component_manager components;
	system_manager systems;

    entity _player_id = 65535;
    entity _map_id = 65535;

	void run_ecs();

	friend class engine;
};

#endif //ECS_H
