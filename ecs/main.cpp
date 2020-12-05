#include <iostream>
#include <cstdlib>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#include "components.h"
#include "ecs/include.h"

using namespace std::string_literals;
using olc::vf2d;



auto make_player(ecs::world& world, std::string name, olc::Pixel color)
{
	auto builder = world.create_entity();
	builder
		.with<Transform>(200.0F, 200.0F)
		.with<Name>(name)
		.with<CircleCollider>()
		.with<Player>()
		.with<Health>(100);

	return builder.id;
}

void make_enemy(ecs::world& world, float x, float y)
{
	const auto red = x / SCREEN_WIDTH;
	const auto green = y / SCREEN_HEIGHT;
	auto color = olc::Pixel(static_cast<uint8_t>(red * 255), static_cast<uint8_t>(green * 255), 50);

	world.create_entity()
		.with<Transform>(vf2d{ x, y })
		.with<Health>(100)
		.with<Graphic>(color, vf2d{ 4, 4 })
		.with<CircleCollider>(2)
		.with<Enemy>();
}

class Example : public olc::PixelGameEngine
{
public:
	Example()
	{
		sAppName = "ECS Example"s;
	}

public:
	bool OnUserCreate() override
	{
		player = make_player(world, "Frappe"s, olc::GREEN);

		for (int x = 0; x < 5; x++)
		{
			for (int y = 0; y < 5; y++)
			{
				make_enemy(world, static_cast<float>(x * 8), static_cast<float>(y * 8));
			}
		}
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		Clear(olc::BLACK);

		// Render System
		ecs::view<Transform, Graphic>(world).for_each(
			[&](const Transform& t, const Graphic& g)
			{
				FillRect(t.position - (0.5f * g.size), g.size, g.color);
			}
		);

		ecs::view<Transform, Player, CircleCollider>(world).for_each(
			[&](const Transform& t, const Player& p, const CircleCollider& cc)
			{
				FillCircle(t.position, static_cast<int>(cc.radius));
			}
		);

		// Enemy chase player system
		auto player_pos = world.get_component<Transform>(player);
		ecs::view<Transform, Enemy>(world).for_each(
			[&](Transform& t, const Enemy& e)
			{
				const auto path_to_player = (player_pos.position - t.position);
				const auto distance = path_to_player.mag();

				if (distance > e.stopping_distance)
				{
					t.position += path_to_player.norm() * fElapsedTime * e.movement_speed * 1.0f / (distance * 0.1f);
				}
			}
		);

		// Player Movement System
		ecs::view<Transform, Player>(world).for_each(
			[&](Transform& t, const Player& p)
			{
				const float vertical = static_cast<float> (GetKey(olc::DOWN).bHeld - GetKey(olc::UP).bHeld);
				const float horizontal = static_cast<float> (GetKey(olc::RIGHT).bHeld - GetKey(olc::LEFT).bHeld);
				vf2d movement = { horizontal , vertical };
				if (movement.mag2() > 1.0f)
				{
					movement = movement.norm();
				}
				t.position += movement * fElapsedTime * p.movement_speed;
			}
		);

		// Player collision system
		ecs::view<Player, Transform, CircleCollider>(world).for_each(
			[&](const auto& player, const auto& playerPos, const auto& playerCollider)
			{
				ecs::view<Enemy, Transform, CircleCollider>(world).for_each_entity(
					[=](const auto& enemy_id, const auto& enemy, const auto& enemyPos, const auto& enemyCollider)
					{
						const float distance = (enemyPos.position - playerPos.position).mag();
						if (distance < enemyCollider.radius + playerCollider.radius)
						{
							world.destroy_entity(enemy_id);
						}
					}
				);
			}
		);

		// Spawn bunch of stuff on space
		if (GetKey(olc::SPACE).bPressed)
		{
			for (float x = 0; x < SCREEN_WIDTH; x += SCREEN_WIDTH / 300.0F)
			{
				for (float y = 0; y < SCREEN_HEIGHT; y+= SCREEN_HEIGHT / 300.0F)
				{
					make_enemy(world, x, y);
				}
			}
		}

		//
		if (GetMouse(0).bHeld)
		{
			auto& player_transform = world.get_component<Transform>(player);
			player_transform.position = GetMousePos();
		}

		if (GetMouseWheel() != 0)
		{
			auto& player_collider = world.get_component<CircleCollider>(player);
			player_collider.radius += GetMouseWheel() / 60;
			player_collider.radius = std::max<uint32_t>(player_collider.radius, 2);
		}

		return true;
	}

private:
	ecs::world world{};
	ecs::entity_id player{};
};

int main()
{
	Example example;
	if (example.Construct(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2))
	{
		example.Start();
	}
};