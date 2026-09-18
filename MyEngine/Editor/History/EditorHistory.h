#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"

// 前方宣言
class ComponentEditor;


/// <summary>
/// エディタの操作の履歴（Undo・Redo）と、コピー・貼り付け
/// <para>操作はRequestで予約だけして、次のフレームの最初（Flush）に順番どおり実行する（UIを描いている途中でEntityを増減させないため）</para>
/// <para>相手はHandleではなくEntityIdで覚える（消して戻すとHandleは変わるが、EntityIdは変わらない）</para>
/// <para>仕組みの解説は Docs/Tasks/Editor.md の「Undoの仕組み」</para>
/// </summary>
class EditorHistory {
public:
	// ===== 毎フレーム =====
	// フレームの最初に1回呼ぶ。予約された操作を、頼まれた順に実行する
	static void Flush();
	// シーンの切り替え・終了で呼ぶ。履歴を捨てる（コピーした物は残すので、Stopの後にも貼れる）
	static void Clear();

	// ===== Undo / Redo =====
	static bool CanUndo();
	static bool CanRedo();
	static void RequestUndo();
	static void RequestRedo();

	// ===== Entityの操作 =====
	static void RequestCreate(const std::string& name, Handle<Entity> parent); // parentが無効ならroot
	static void RequestDestroy(Handle<Entity> handle);                         // 子も一緒に消える
	static void RequestRename(Handle<Entity> handle, const std::string& name); // 空の名前にはしない
	static void RequestSetActive(Handle<Entity> handle, bool isActive);
	static void RequestReparent(Handle<Entity> child, Handle<Entity> parent); // parentが無効ならrootへ

	// ===== コピー・貼り付け =====
	static bool CanPaste();
	static void Copy(Handle<Entity> handle);             // その場で写す（何も変えないので予約しない）
	static void RequestPaste(Handle<Entity> parent);     // parentの子として貼る（無効ならroot）
	static void RequestDuplicate(Handle<Entity> handle); // 同じ親の下に複製する

	// ===== Component =====
	static void RequestAddComponent(Handle<Entity> handle, const ComponentEditor& editor);
	static void RequestRemoveComponent(Handle<Entity> handle, const ComponentEditor& editor);
	// Inspectorが描く前と描いた後の中身を渡す。変わったバイトだけを覚えておく
	static void RecordComponentChange(Handle<Entity> handle, const ComponentEditor& editor, const void* before, const void* after);
	// 操作の区切り（マウスを離した・入力を確定した）で呼ぶ。ここまでの変更を1件の履歴にまとめる
	static void CommitComponentChanges();
};