#pragma once

#include "olcPixelGameEngine.h"
#include "ecs.h"

using olc::vf2d;

constexpr uint32_t SCREEN_WIDTH = 600;
constexpr uint32_t SCREEN_HEIGHT = 400;
constexpr vf2d TILE_SIZE{ 16.0f, 16.0f };

struct Transform
{
	using storage_type = ecs::default_storage;

	Transform(float x, float y) :
		position({ x, y }) {}

	Transform(vf2d position) :
		position(position) {}

	vf2d position{ 0.0f, 0.0f };
};

struct Name
{
	using storage_type = ecs::default_storage;

	Name(std::string name)
		:value(std::move(name)) {}
	std::string value;
};

struct Graphic
{
	using storage_type = ecs::default_storage;

	Graphic(olc::Pixel color, vf2d size)
		:color(color), size(size) {}

	olc::Pixel color = olc::WHITE;
	vf2d size = TILE_SIZE;
};

struct CircleCollider
{
	using storage_type = ecs::default_storage;

	CircleCollider() = default;
	CircleCollider(uint32_t radius)
		: radius(radius) {}
	uint32_t radius = 8;
};


struct Player
{
	using storage_type = ecs::small_storage;

	float movement_speed = 250.f;
};

struct Enemy
{
	using storage_type = ecs::default_storage;

	float movement_speed = 25.f;
	float stopping_distance = 10.f;
};

struct Health
{
	using storage_type = ecs::default_storage;

	Health(uint32_t value) :
		value(value) {}
	uint32_t value{ 100 };
};