#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// インスペクターウィンドウ（HierarchyWindowで選んでいるEntityの中身を編集する）
/// <para>Componentごとに1つの区画（CollapsingHeader）にする。Componentが増えたらDrawXxxを足してDrawから呼ぶ</para>
/// </summary>
class InspectorWindow {
public:
	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ（HierarchyWindow::Drawより後）
	/// </summary>
	static void Draw();


private:
	// Entityそのものの情報（名前・有効無効・Handleの番号）
	static void DrawHeader(Handle<Entity> handle);
	// TransformComponentの区画
	static void DrawTransform(Handle<Entity> handle);
	// ModelRendererComponentの区画
	static void DrawModelRenderer(Handle<Entity> handle);
	// モデルを選ぶコンボ（resources以下を走査した一覧から選ぶ）
	static void DrawModelPicker(ModelRendererComponent& render);
	// Componentを足すボタン（カテゴリごとに縦に並べたポップアップ）
	static void DrawAddComponent(Handle<Entity> handle);
};