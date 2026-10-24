#include "SceneVisibility.h"

#include <unordered_set>

#include "MyEngine/ECS/EntityManager.h"

namespace {
// 目を閉じた物の EntityId（Handle は Play / Stop で作り直すと変わるので、変わらない EntityId で覚える）
// unordered_set：同じ値を2つ持たない入れ物。「入っているか」を、数がいくつあっても1回で調べられる（ハッシュ表）
std::unordered_set<EntityId> hiddenIds;

// Handle → EntityId（居なければ 0）
EntityId IdOf(Handle<Entity> entity) {
	const Entity* owner = EntityManager::Get(entity);
	return (owner != nullptr) ? owner->id : 0;
}
} // namespace

//=============================================================================
// 調べる
//=============================================================================
bool SceneVisibility::IsHiddenSelf(Handle<Entity> entity) { return hiddenIds.contains(IdOf(entity)); }

bool SceneVisibility::IsHidden(Handle<Entity> entity) {
	// 誰も隠していなければ、親をたどらない（製品版・ふだんの描画は、ここで終わる）
	if (hiddenIds.empty()) {
		return false;
	}
	// 自分 → 親 → 親の親 … と一番上までたどる（親子は輪にならない。SetParent が断る）
	for (Handle<Entity> current = entity; current.IsValid();) {
		const Entity* owner = EntityManager::Get(current);
		if (owner == nullptr) {
			return false;
		}
		if (hiddenIds.contains(owner->id)) {
			return true;
		}
		current = owner->parent;
	}
	return false;
}

//=============================================================================
// 変える
//=============================================================================
void SceneVisibility::Toggle(Handle<Entity> entity) {
	const EntityId id = IdOf(entity);
	if (id == 0) {
		return;
	}
	// erase は消した数を返す（0 なら入っていなかった＝見せていたので、隠す）
	if (hiddenIds.erase(id) == 0) {
		hiddenIds.insert(id);
	}
}

void SceneVisibility::Clear() { hiddenIds.clear(); }