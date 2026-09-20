#pragma once
#include <cstdint>
#include <string>

#include "MyEngine/Core/Handle.h"

using EntityId = uint64_t; // Undoで復元する番号。0は無し。実行中Handleとは別物。

struct Entity {
	std::string name = "Entity";
	EntityId id = 0;
	Handle<Entity> self;
	Handle<Entity> parent;
	bool isActive = true;
};