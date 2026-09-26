#pragma once
#include <cstdint>
#include <string>

#include "MyEngine/Core/Handle.h"

using EntityId = uint64_t; // Undoで復元する番号。0は無し。実行中Handleとは別物。


/// <summary>
/// 別のEntityへの参照（Componentに持たせる用）
/// <para>Handleではなく EntityId で覚えるので、保存・Play/Stop・削除のUndo・コピーをまたいでも同じ相手を指す
/// （Handleは作り直すたびに変わる）。使うときは EntityManager::Get&lt;T&gt;(ref) / EntityManager::Find(ref)</para>
/// </summary>
struct EntityRef {
	EntityId id = 0; // 0は「無し」

	bool IsSet() const { return id != 0; }
	bool operator==(const EntityRef&) const = default;
};


/// <summary>
/// Entity
/// </summary>
struct Entity {
	std::string name = "Entity";
	EntityId id = 0;
	Handle<Entity> self;
	Handle<Entity> parent;
	bool isActive = true;
};