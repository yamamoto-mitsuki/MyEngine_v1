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
	static constexpr int kNameBufferSize = 64; // 名前の編集用バッファの長さ

	// Entityそのものの情報（名前・有効無効・Handleの番号）
	static void DrawHeader(Handle<Entity> handle);
	// TransformComponentの区画
	static void DrawTransform(Handle<Entity> handle);
	// Componentを足すボタン（中身は次のStepから増やす）
	static void DrawAddComponent(Handle<Entity> handle);

	static char nameBuffer_[kNameBufferSize]; // 名前の編集用
	static Handle<Entity> nameBufferOwner_;   // nameBuffer_ が今どのEntityのものか
};