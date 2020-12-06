#pragma once

#include "ecs.h"

#include <variant>
#include <vector>

namespace ecs {
	
	namespace detail {
		inline int counter = 0;
		template <ECS_COMPONENT T>
		__forceinline static int type_id()
		{
			static int value = counter++;
			return value;
		}
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

			if (entities.size() >= MAX_ENTITIES) [[unlikely]]
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
			const auto component_id = detail::type_id<T>();

			if (component_pools.size() <= component_id) [[unlikely]]
			{
				component_pools.reserve(component_id + 1);
				component_pools.push_back(std::make_unique<pools>(pool_t<typename T::storage_type>(sizeof(T))));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(get_componenent_address<T> (entity_index, component_id)) T{};

			entities[entity_index].mask.set(component_id);
			return *component;
		}

		template<ECS_COMPONENT T, typename... Args>
		T& add_component(entity_id entity, Args&&... args)
		{
			const auto component_id = detail::type_id<T>();

			if (component_pools.size() <= component_id) [[unlikely]]
			{
				component_pools.reserve(component_id + 1);
				component_pools.push_back(std::make_unique<pools>(pool_t<typename T::storage_type>(sizeof(T))));
			}

			const auto entity_index = get_entity_index(entity);
			const auto component = ::new(get_componenent_address<T>(entity_index, component_id)) T(std::forward<Args>(args)...);
			entities[entity_index].mask.set(component_id);
			return *component;
		}

		template<ECS_COMPONENT T>
		T& get_component(entity_id entity)
		{
			const auto component_id = detail::type_id<T>();
			const auto entity_index = get_entity_index(entity);


			assert(entities[entity_index].mask.test(component_id) && "get component on entity without component!");

			return *get_componenent_address<T>(entity_index, component_id);
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

			if (entities[entity_index].id != entity) [[unlikely]]
			{
				std::cerr << "Remove component failed!\n";
				return;
			}

			const auto component_id = detail::type_id<T>();
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

		using pools = std::variant<pool_t<default_storage_t>, pool_t<small_storage_t>>;
		std::vector<std::unique_ptr<pools>> component_pools;

	private:
		template<ECS_COMPONENT T>
		inline T* get_componenent_address(entity_index entity_index, int component_id)
		{
			return static_cast<T*>(std::get<pool_t<typename T::storage_type>>(*component_pools[component_id]).get(entity_index));
		}
	};

	template<ECS_COMPONENT... Ts>
	struct view
	{
		view(ecs::world& world) : world(&world)
		{
			if constexpr (sizeof...(Ts) > 0)
			{
				const int component_ids[] = { detail::type_id<Ts>() ... };
				for (const auto& id : component_ids)
				{
					mask.set(id);
				}
			}
			// Todo logic on storage types. only check small storage list of entities if present!
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