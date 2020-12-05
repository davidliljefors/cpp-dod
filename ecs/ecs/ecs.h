#pragma once
#include <cstdint>
#include <bitset>


namespace ecs {
	using entity_id = uint64_t;
	using entity_index = uint32_t;
	using entity_version = uint32_t;
	constexpr uint32_t MAX_COMPONENTS{ 32u };
	constexpr uint32_t MAX_ENTITIES{ 100000u };
	using component_mask = std::bitset<MAX_COMPONENTS>;

#ifdef __cpp_lib_concepts
#include <concepts>
	template<typename T>
	concept ComponentType = requires (T t)
	{
		typename T::storage_type;
	};
#define ECS_COMPONENT ComponentType
#else
#define ECS_COMPONENT typename
#endif

	struct default_storage_t {
		constexpr static size_t size = MAX_ENTITIES;
	};
	struct small_storage_t {
		constexpr static size_t size = 8;
	};


	constexpr entity_id create_entity_id(entity_index index, entity_version version)
	{
		return static_cast<entity_id>(index) << 32 | version;
	}

	constexpr entity_index get_entity_index(entity_id id)
	{
		return static_cast<entity_index>(id >> 32);
	}

	constexpr entity_version get_entity_version(entity_id id)
	{
		return static_cast<entity_version>(id);
	}

	constexpr bool is_entity_valid(entity_id id)
	{
		return (id >> 32) != std::numeric_limits<entity_index>::max();
	}

	constexpr auto INVALID_ENTITY_INDEX = std::numeric_limits<entity_index>::max();
	constexpr auto INVALID_ENTITY = create_entity_id(std::numeric_limits<entity_index>::max(), 0);
}
