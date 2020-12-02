#pragma once
#include <cstdint>
#include <vector>
#include <bitset>
#include <memory>
#include <functional>
#include <iterator>
#include <cassert>

namespace ecs {
	using entity_id = uint64_t;
	using entity_index = uint32_t;
	using entity_version = uint32_t;
	constexpr uint32_t MAX_COMPONENTS{ 32u };
	constexpr uint32_t MAX_ENTITIES{ 100000u };
	using component_mask = std::bitset<MAX_COMPONENTS>;

	struct component {};

#ifdef __cpp_lib_concepts
#include <concepts>
	template<typename T>
	concept ComponentType = std::derived_from<T, component>;
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
		using id_type = uint32_t;
		inline id_type component_counter{ 0 };

		template <ECS_COMPONENT T>
		id_type component_id() noexcept
		{
			static const int component_id = component_counter++;
			return component_id;
		}

		struct component_pool
		{
			component_pool() = default;
			component_pool(size_t elementsize)
			{
				elementSize = elementsize;
				storage = std::make_unique<uint8_t[]>(elementSize * MAX_ENTITIES);
			}

			inline void* get(size_t index)
			{
				return storage.get() + index * elementSize;
			}

			std::unique_ptr<uint8_t[]> storage{ nullptr };
			size_t elementSize{ 0 };
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
				std::cerr << "Reached max entities!\n";
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
				component_pools.push_back(std::make_unique<detail::component_pool>(sizeof(T)));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(component_pools[component_id]->get(entity_index)) T{};

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
				component_pools.push_back(std::make_unique<detail::component_pool>(sizeof(T)));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(component_pools[component_id]->get(entity_index)) T(std::forward<Args>(args)...);
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

			auto component = static_cast<T*>(component_pools[component_id]->get(entity_index));
			return *component;
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
		std::vector<std::unique_ptr<detail::component_pool>> component_pools;
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
			iterator(ecs::world* world, entity_index index, component_mask mask)
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
