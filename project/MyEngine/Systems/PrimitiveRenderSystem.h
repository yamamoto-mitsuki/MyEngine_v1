#pragma once
#include "MyEngine/Components/PrimitiveRendererComponent.h"

// 前方宣言
class Camera;


/// <summary>
/// PrimitiveRendererComponent を持つ Entity を、その形（球・箱）で描く
/// <para>形は Renderer の DrawSphere・DrawOBB を使う（頂点はエンジンがコードで作る。モデルのファイルは要らない）</para>
/// </summary>
class PrimitiveRenderSystem {
public:
	// PrimitiveRendererComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release() {}

	// 描画リクエストを積む（WindowManager::DrawAll から。停止中も描く）
	static void Draw(Camera* camera);
};