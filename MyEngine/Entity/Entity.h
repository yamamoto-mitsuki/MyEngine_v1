#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/TransformComponent.h"
#include "MyEngine/Entity/RenderComponent.h"


/// <summary>
/// シーンに置くもの1個
/// <para>Componentそのものは持たず、Handleで指す（実体はそれぞれのManagerが型別に持つ）</para>
/// <para>ARCHITECTURE.md 段階3でEntityが「ただのID」になったら、name はEditor用の別の表に移す</para>
/// </summary>
struct Entity {
	std::string name = "Entity";          // ヒエラルキーに出す名前
	Handle<Entity> self;                  // 自分を指すHandle（一覧から選ぶときに使う）
	Handle<Entity> parent;                // 親。無効なら一番上（root）
	Handle<TransformComponent> transform; // 全Entityが必ず1つ持つ
	Handle<RenderComponent> render;       // 未追加なら無効なHandle
	bool isActive = true;                 // falseで更新・描画の対象から外す（使うのは後のStep）
};