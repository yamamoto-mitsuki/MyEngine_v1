#pragma once
#include <cstdint>
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/ModelRendererComponent.h"
#include "MyEngine/Entity/TransformComponent.h"

// Entityに1つずつ配る、消して作り直しても変わらない番号（0は「無し」）
// Handleは消すと無効になり、作り直すと別の値になる。Undoや将来のシーン保存では、相手をこの番号で覚える
using EntityId = uint64_t;


/// <summary>
/// シーンに置くもの1個
/// <para>Componentそのものは持たず、Handleで指す（実体はそれぞれのManagerが型別に持つ）</para>
/// <para>ARCHITECTURE.md 段階3でEntityが「ただのID」になったら、name はEditor用の別の表に移す</para>
/// </summary>
struct Entity {
	std::string name = "Entity";           // ヒエラルキーに出す名前
	EntityId id = 0;                       // 消して作り直しても変わらない番号（Undo・保存用）
	Handle<Entity> self;                   // 自分を指すHandle（一覧から選ぶときに使う）
	Handle<Entity> parent;                 // 親。無効なら一番上（root）
	Handle<TransformComponent> transform;  // 全Entityが必ず1つ持つ
	Handle<ModelRendererComponent> render; // 未追加なら無効なHandle
	bool isActive = true;                  // falseで更新・描画の対象から外す（使うのは後のStep）
};