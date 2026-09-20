#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Entity/EntityManager.h"

// Add Componentのメニューの分け方。"/" で区切ると、その下にさらにカテゴリを作れる（"Gameplay/Movement" など）
//
//   Core        Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
//   Rendering3D 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
//   Rendering2D 2D（将来のSpriteRenderer）
//   Lighting    ライト
//   Effects     パーティクルなど
//   Physics     当たり判定
//   Audio       音
//   Gameplay    ゲーム固有のデータ（COMPONENT(...) で書いた物は全部ここに入る）
//
// 上の段の並び順は ComponentEditor.cpp の kTopLevelOrder が決める。そこに無い名前は後ろに回る


/// <summary>
/// Componentの種類ごとに1つ作る「エディタでの扱い方」（UnityのCustomEditorと同じ考え方）
/// <para>新しいComponentを作ったら、EntityManager::RegisterComponent&lt;T&gt;() で置き場所を作り、
/// TypedComponentEditorを継承したクラスを ComponentEditorRegistry::Register する</para>
/// </summary>
class ComponentEditor {
public:
	virtual ~ComponentEditor() = default;

	// ===== 種類の情報 =====
	virtual const char* GetName() const = 0;         // Inspectorの見出し・Add Componentの表示名
	virtual const char* GetCategory() const = 0;     // Add Componentでどのカテゴリに出すか（"Gameplay/Movement" のように "/" で入れ子）
	virtual bool IsOptional() const { return true; } // Add・Removeの対象か（Transformのように必ず持つものはfalse）
	virtual size_t GetSize() const = 0;              // Componentの大きさ（Undo・コピーで中身を丸ごと写すのに使う）

	// ===== 実体の出し入れ（実体を持っているManagerへの窓口）=====
	virtual void* Get(Handle<Entity> handle) const = 0;                       // 持っていなければnullptr。ポインタは使い捨て
	virtual bool IsAddPending(Handle<Entity>) const { return false; }         // 追加を予約済みか
	virtual void RequestAdd(Handle<Entity>, const void* /*initial*/) const {} // 追加を予約（initialがnullptrなら初期値）
	virtual void RequestRemove(Handle<Entity>) const {}                       // 取り外しを予約

	// ===== Inspectorの中身 =====
	virtual void Draw(void* component) const = 0;
};


/// <summary>
/// ComponentEditorの登録表。カテゴリ順（同じカテゴリの中は名前順）に並べて持つ
/// <para>ゲームのシーンはエンジンのEditorより先に登録することがある（ImGuiManagerの初期化より前にシーンが作られる）。
/// それでもTransformが一番上に来るように、登録した順ではなくカテゴリで並べる</para>
/// </summary>
template<class T> 
class TypedComponentEditor : public ComponentEditor {
	// これはバイトコピーできることの検査。ポインタを含まないことまでは検査できない。
	static_assert(std::is_trivially_copyable_v<T>);

public:
	size_t GetSize() const final { return sizeof(T); }
	bool IsOptional() const final { return EntityManager::IsRemovable<T>(); }
	void* Get(Handle<Entity> handle) const final { return EntityManager::Get<T>(handle); }
	bool IsAddPending(Handle<Entity> handle) const final { return EntityManager::IsAddPending<T>(handle); }
	void RequestAdd(Handle<Entity> handle, const void* initial) const final {
		T value{};
		if (initial) {
			std::memcpy(&value, initial, sizeof(T));
		}
		EntityManager::RequestAdd<T>(handle, value);
	}
	void RequestRemove(Handle<Entity> handle) const final { EntityManager::RequestRemove<T>(handle); }
	void Draw(void* component) const final { DrawComponent(*static_cast<T*>(component)); }

protected:
	virtual void DrawComponent(T& component) const = 0;
};


/// <summary>
/// ComponentEditorの登録表。Inspectorの区画も Add Component のメニューも、この並びをそのまま使う
/// </summary>
class ComponentEditorRegistry {
public:
	// 登録する。同じ名前のものが登録済みなら何もしない（シーンを作り直すたびに増えないように）
	static void Register(std::unique_ptr<ComponentEditor> editor);
	static const std::vector<std::unique_ptr<ComponentEditor>>& GetAll();
};