#pragma once
#include "ecs.h"
#include <memory>
#include <cassert>
#include <vector>

namespace ecs::detail {

	

	template<typename tag_t>
	struct component_pool
	{
		static_assert("Must specialize on tag!");
	};

	template<>
	struct component_pool<default_storage_t>
	{
		component_pool() = default;
		component_pool(size_t elementsize)
			:elementSize(elementsize)
		{
			storage = std::make_unique<uint8_t[]>(elementSize * default_storage_t::size);
		}

		inline void* get(size_t index)
		{
			return &storage[index * elementSize];
		}

		std::unique_ptr<uint8_t[]> storage{ nullptr };
		const size_t elementSize{ 0 };
	};

	template<>
	struct component_pool<small_storage_t>
	{
		component_pool() = default;
		component_pool(size_t elementsize)
			:elementSize(elementsize)
		{
			index_mapping.reserve(small_storage_t::size);
			storage = std::make_unique<uint8_t[]>(elementSize * small_storage_t::size);
		}

		inline void* get(size_t index)
		{

			auto mapped_index = std::find(index_mapping.begin(), index_mapping.end(), index);
			if (mapped_index != index_mapping.end())
			{
				return storage.get() + *mapped_index * elementSize;
			}
			else
			{
				auto new_index = index_mapping.size();
				index_mapping.push_back(index);
				assert(index_mapping.size() < small_storage_t::size && "Growing small storage over maximum size not allowed");
				return storage.get() + new_index * elementSize;
			}
		}

		const auto& active_entities()
		{
			return index_mapping;
		}

		std::vector<size_t> index_mapping;
		std::unique_ptr<uint8_t[]> storage{ nullptr };
		const size_t elementSize{ 0 };
	};
}