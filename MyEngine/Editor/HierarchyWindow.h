#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// ヒエラルキーウィンドウ（Entityの一覧・作成・削除・選択・親子の付け替え・名前の変更）
/// <para>選択中のEntityはここが持つ。InspectorWindowはこれを見る</para>
/// </summary>
class HierarchyWindow {
public:
	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ
	/// </summary>
	static void Draw();

	// 選択中のEntity（何も選んでいなければ無効なHandle）
	static Handle<Entity> GetSelected() { return selected_; }
	static void SetSelected(Handle<Entity> handle) { selected_ = handle; }


private:
	// Entity1つ分を描く（子がいれば入れ子で描く）
	static void DrawEntityNode(Handle<Entity> handle);
	// 一覧を描いている間に受け付けた操作を、描き終わってから反映する
	static void ApplyRequests();
	// 名前の変更を始める（そのEntityの行を入力欄にする）
	static void StartRename(Handle<Entity> handle);
	// 名前の入力欄（DrawEntityNodeの中、ツリーの矢印の横に出す）
	static void DrawRenameField(Handle<Entity> handle);

	static Handle<Entity> selected_;       // 選択中
	static Handle<Entity> createChildOf_;  // このEntityの子を作る（有効なときだけ）
	static Handle<Entity> destroyRequest_; // このEntityを消す
	static Handle<Entity> reparentChild_;  // 親を付け替えるEntity
	static Handle<Entity> reparentParent_; // 新しい親（無効ならrootへ移す）
	static bool createRootRequest_;        // 一番上にEntityを作る
	static bool reparentRequest_;          // 親の付け替えを頼まれた
	static Handle<Entity> renaming_;       // 名前を変更中のEntity（無効なら変更中ではない）
	static std::string renameBuffer_;      // 入力中の名前（確定するまでEntityの名前は変えない）
	static bool renameFocusRequest_;       // 入力欄を出した最初のフレームだけtrue（フォーカスを移す）
};