#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"

// 前方宣言
class ComponentEditor;


/// <summary>
/// インスペクターウィンドウ（HierarchyWindowで選んでいるEntityの中身を編集する）
/// <para>Componentごとの中身は ComponentEditor が描く。ここは並べるだけなので、Componentが増えてもこのクラスは変えない</para>
/// </summary>
class InspectorWindow {
public:
	/// <summary>
	/// エンジンのComponentEditorを登録する（ImGuiManager::Initializeから1回だけ）
	/// </summary>
	static void Initialize();

	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ（HierarchyWindow::Drawより後）
	/// </summary>
	static void Draw();


private:
	// Entityそのものの情報（有効無効・名前・番号）
	static void DrawHeader(Handle<Entity> handle);
	// Component1つ分の区画（持っていなければ何も出さない）
	static void DrawComponent(Handle<Entity> handle, const ComponentEditor& editor);
	// Componentを足すボタン（カテゴリごとに縦に並べたポップアップ）
	static void DrawAddComponent(Handle<Entity> handle);
};