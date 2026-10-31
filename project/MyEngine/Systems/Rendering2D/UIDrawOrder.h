#pragma once
#include <tuple>
#include <vector>
#include <algorithm>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/ECS/EntityManager.h"

/// <summary>
/// 画面に重ねる順に並べるための1件（Image・Vector Shape の Draw の中だけで使う）
/// </summary>
struct UIDrawItem {
	int order = 0;         // Component の Order（小さい方が奥）
	EntityId id = 0;       // 同じ Order なら、先に作った物（番号が小さい物）が奥
	Handle<Entity> entity; // 持ち主
};


/// <summary>
/// 描く物（有効で、親まで有効な物）を Order の順に集める。T は enabled・order を持つ Component
/// <para>RenderQueue は同じ Order の物を積んだ順に描くので、先に作った物が奥になるように並べてから積む</para>
/// </summary>
template<class T> 
void CollectUIDrawItems(std::vector<UIDrawItem>& items) {
	items.clear();
	EntityManager::ForEach<T>([&items](Handle<Entity> entity, const T& component) {
		if (!component.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		const Entity* owner = EntityManager::Get(entity);
		items.push_back({component.order, owner ? owner->id : 0, entity});
	});
	// Order の小さい順。同じなら番号の小さい順
	std::ranges::sort(items, [](const UIDrawItem& a, const UIDrawItem& b) { return std::tie(a.order, a.id) < std::tie(b.order, b.id); });
}