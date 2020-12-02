#pragma once
#include <cstdint>
#include <vector>
#include <bitset>
#include <memory>
#include <functional>
#include <iterator>
#include <array>
#include <algorithm>
#include <cassert>
#include <variant>

#include "type_traits.h"

namespace ecs {
	using entity_id = uint64_t;
	using entity_index = uint32_t;
	using entity_version = uint32_t;
	constexpr uint32_t MAX_COMPONENTS{ 32u };
	constexpr uint32_t MAX_ENTITIES{ 100000u };
	using component_mask = std::bitset<MAX_COMPONENTS>;

	struct default_storage {
		constexpr static size_t size = MAX_ENTITIES;
	};
	struct small_storage {
		constexpr static size_t size = 8;
	};

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

	namespace detail {
		using id_type = uint8_t;
		inline id_type component_counter{ 0 };

		template <ECS_COMPONENT T>
		id_type component_id() noexcept
		{
			static const uint8_t component_id = component_counter++;
			return component_id;
		}

		template<typename tag>
		struct component_pool
		{
			static_assert("Must specialize on tag!");
		};

		template<>
		struct component_pool<default_storage>
		{
			component_pool() = default;
			component_pool(size_t elementsize)
				:elementSize(elementsize)
			{
				storage = std::make_unique<uint8_t[]>(elementSize * default_storage::size);
			}

			inline void* get(size_t index)
			{
				return &storage[index * elementSize];
			}

			std::unique_ptr<uint8_t[]> storage{ nullptr };
			const size_t elementSize{ 0 };
		};

		template<>
		struct component_pool<small_storage>
		{
			component_pool() = default;
			component_pool(size_t elementsize)
				:elementSize(elementsize)
			{
				index_mapping.reserve(small_storage::size);
				storage = std::make_unique<uint8_t[]>(elementSize * small_storage::size);
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
					assert(index_mapping.size() < small_storage::size && "Growing small storage over maximum size not allowed");
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

	struct world;
	struct entity_builder
	{
		entity_builder(entity_id id, ecs::world* world)
			:id(id), world(world)
		{}

		template<ECS_COMPONENT T, typename... Args>
		entity_builder& with(Args&&... args);

		entity_id id{};
		ecs::world* world{};
	};

	struct world
	{
		world()
		{
			entities.reserve(MAX_ENTITIES);
		};

		struct entity_desc
		{
			entity_id id{};
			component_mask mask{};
		};

		entity_builder create_entity()
		{
			if (!free_entities.empty())
			{
				const auto new_index = free_entities.back();
				const auto new_version = get_entity_version(entities[new_index].id);
				free_entities.pop_back();

				const auto new_id = create_entity_id(new_index, new_version);
				entities[new_index].id = new_id;
				return entity_builder(new_id, this);
			}

			if (entities.size() >= MAX_ENTITIES)
			{
				//std::cerr << "Reached max entities!\n";
				return entity_builder(0, nullptr);
			}

			entity_desc new_entity;
			new_entity.id = create_entity_id(static_cast<entity_index> (entities.size()), 0);
			entities.emplace_back(new_entity);
			return entity_builder(new_entity.id, this);
		}


		template<ECS_COMPONENT T>
		T& add_component(entity_id entity)
		{
			const auto component_id = detail::component_id<T>();

			if (component_pools.size() <= component_id)
			{
				component_pools.reserve(component_id + 1);
				component_pools.push_back(std::make_unique<pools>(pool_t<typename T::storage_type>(sizeof(T))));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(get_componenent_internal<T>(entity_index, component_id)) T{};

			entities[entity_index].mask.set(component_id);
			return *component;
		}

		template<ECS_COMPONENT T, typename... Args>
		T& add_component(entity_id entity, Args&&... args)
		{
			const auto component_id = detail::component_id<T>();

			if (component_pools.size() <= component_id)
			{
				component_pools.reserve(component_id + 1);
				component_pools.push_back(std::make_unique<pools>(pool_t<typename T::storage_type>(sizeof(T))));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(get_componenent_internal<T>(entity_index, component_id)) T(std::forward<Args>(args)...);
			entities[entity_index].mask.set(component_id);
			return *component;
		}

		template<ECS_COMPONENT T>
		T& get_component(entity_id entity)
		{
			const auto component_id = detail::component_id<T>();
			const auto entity_index = get_entity_index(entity);

			if (!entities[entity_index].mask.test(component_id))
			{
				assert("get component on entity without component!");
			}

			return *get_componenent_internal<T>(entity_index, component_id);
		}

		template<ECS_COMPONENT... Ts>
		auto get_components(entity_id entity)
		{
			return std::make_tuple(get_component<Ts>(entity) ...);
		}

		template<ECS_COMPONENT T>
		void remove_component(entity_id entity)
		{
			const auto entity_index = get_entity_index(entity);

			if (entities[entity_index].id != entity)
			{
				std::cerr << "Remove component failed!\n";
				return;
			}

			const auto component_id = detail::component_id<T>();
			entities[entity_index].mask.reset(component_id);
		}

		void destroy_entity(entity_id entity)
		{
			const auto new_id = create_entity_id(INVALID_ENTITY_INDEX, get_entity_version(entity) + 1);
			const auto entity_index = get_entity_index(entity);

			entities[entity_index].id = new_id;
			entities[entity_index].mask.reset();
			free_entities.push_back(entity_index);
		}

		template<ECS_COMPONENT... Ts>
		auto view()
		{
			return view<Ts...>(this);
		}

		std::vector<entity_desc> entities;
		std::vector<entity_index> free_entities;
		template <typename pool_tag>
		using pool_t = detail::component_pool<pool_tag>;

		using pools = std::variant<pool_t<default_storage>, pool_t<small_storage>>;
		std::vector<std::unique_ptr<pools>> component_pools;

	private:
		template<ECS_COMPONENT T>
		inline T* get_componenent_internal(entity_index entity_index, detail::id_type component_id)
		{
			if constexpr (std::is_same_v<typename T::storage_type, default_storage>)
			{
				return static_cast<T*>(std::get<pool_t<default_storage>>(*component_pools[component_id]).get(entity_index));
			}
			else if constexpr (std::is_same_v<T::storage_type, small_storage>)
			{
				return static_cast<T*>(std::get<pool_t<small_storage>>(*component_pools[component_id]).get(entity_index));
			}
			else
			{
				static_assert("Unknown storage tag");
			}
		}
	};

	template<ECS_COMPONENT... Ts>
	struct view
	{
		view(ecs::world& world) : world(&world)
		{
			if constexpr (sizeof...(Ts) > 0)
			{
				const detail::id_type component_ids[] = { detail::component_id<Ts>() ... };
				for (const auto& id : component_ids)
				{
					mask.set(id);
				}
			}
		}

		template<typename Func>
		void for_each(Func&& func)
		{
			for (const auto entity : *this)
			{
				func(world->get_component<Ts>(entity)...);
			}
		};

		template<typename Func>
		void for_each_entity(Func&& func)
		{
			for (const auto entity : *this)
			{
				func(entity, world->get_component<Ts>(entity)...);
			}
		};

		ecs::world* world{ nullptr };
		component_mask mask;

		// If 0 template arguments we want all entities
		constexpr static bool all = (sizeof...(Ts) == 0);

		struct iterator
		{
			iterator(ecs::world* world, entity_index index, component_mask mask) noexcept
				: world(world), index(index), mask(mask) {}

			auto operator*() const
			{
				return world->entities[index].id;
			}

			bool operator==(const iterator& other) const
			{
				return index == other.index || index == world->entities.size();
			}

			bool operator!=(const iterator& other) const
			{
				return (index != other.index && index != world->entities.size());
			}

			iterator& operator++()
			{
				do
				{
					index++;
				} while (index < world->entities.size() && !valid_index());
				return *this;
			}

			bool valid_index()
			{
				if constexpr (all)
				{
					return is_entity_valid(world->entities[index].id);
				}
				else
				{
					return is_entity_valid(world->entities[index].id) &&
						(mask == (mask & world->entities[index].mask));
				}

			}

		private:
			ecs::world* world{ };
			entity_index index{ };
			component_mask mask{ };
		};

		const iterator begin() const
		{
			entity_index first_index = 0;
			while (first_index < world->entities.size() &&
				(mask != (mask & world->entities[first_index].mask)
					|| !is_entity_valid(world->entities[first_index].id)))
			{
				first_index++;
			}
			return iterator(world, first_index, mask);
		}

		const iterator end() const
		{
			return iterator(world, entity_index(world->entities.size()), mask);
		}
	};


	template<ECS_COMPONENT T, typename... Args>
	entity_builder& entity_builder::with(Args&&... args)
	{
		if (world)
		{
			world->add_component<T>(id, std::forward<Args>(args)...);
		}
		return *this;
	}
}
