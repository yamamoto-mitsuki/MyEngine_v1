#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"

/// <summary>
/// Add Componentのメニューの分け方（並びはこの順）
/// </summary>
enum class ComponentCategory {
	Core,        // Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
	Rendering3D, // 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
	Rendering2D, // 2D（将来のSpriteRenderer）
	Lighting,    // ライト（Light.md Step 6）
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
};


/// <summary>
/// Componentの種類ごとに1つ作る「エディタでの扱い方」（UnityのCustomEditorと同じ考え方）
/// <para>Inspectorの表示・Add Component・Undo・コピーは、全部この窓口を通してComponentを触る</para>
/// <para>新しいComponentを作ったら、TypedComponentEditorを継承したクラスを作って ComponentEditorRegistry::Register するだけでよい</para>
/// </summary>
class ComponentEditor {
public:
	virtual ~ComponentEditor() = default;

	// ===== 種類の情報 =====
	virtual const char* GetName() const = 0;           // Inspectorの見出し・Add Componentの表示名
	virtual ComponentCategory GetCategory() const = 0; // Add Componentでどのカテゴリに出すか
	virtual bool IsOptional() const { return true; }   // Add・Removeの対象か（Transformのように必ず持つものはfalse）
	virtual size_t GetSize() const = 0;                // Componentの大きさ（Undo・コピーで中身を丸ごと写すのに使う）

	// ===== 実体の出し入れ（実体を持っているManagerへの窓口）=====
	virtual void* Get(Handle<Entity> handle) const = 0;                       // 持っていなければnullptr。ポインタは使い捨て
	virtual bool IsAddPending(Handle<Entity>) const { return false; }         // 追加を予約済みか
	virtual void RequestAdd(Handle<Entity>, const void* /*initial*/) const {} // 追加を予約（initialがnullptrなら初期値）
	virtual void RequestRemove(Handle<Entity>) const {}                       // 取り外しを予約

	// ===== Inspectorの中身 =====
	virtual void Draw(void* component) const = 0;
};


/// <summary>
/// ComponentEditorの void* を、決まった型 T に直してくれる土台
/// <para>継承したクラスは T* / T& で書けるので、キャストの書き間違いが起きない</para>
/// </summary>
template<class T> 
class TypedComponentEditor : public ComponentEditor {
	// Undo・コピーは中身をバイト列として写す。ポインタや std::string を持つと写した先で壊れるので、持てないようにする
	static_assert(std::is_trivially_copyable_v<T>, "Componentはmemcpyで写せる型にしてください（ARCHITECTURE.md 原則2）");

public:
	size_t GetSize() const final { return sizeof(T); }
	void* Get(Handle<Entity> handle) const final { return GetComponent(handle); }
	void RequestAdd(Handle<Entity> handle, const void* initial) const final {
		T value{};
		if (initial) {
			std::memcpy(&value, initial, sizeof(T)); // バイト列からTへ戻す
		}
		RequestAddComponent(handle, value);
	}
	void Draw(void* component) const final { DrawComponent(*static_cast<T*>(component)); }

protected:
	virtual T* GetComponent(Handle<Entity> handle) const = 0;
	virtual void RequestAddComponent(Handle<Entity>, const T&) const {} // 外せないComponentは書かなくてよい
	virtual void DrawComponent(T& component) const = 0;
};


/// <summary>
/// ComponentEditorの登録表。Inspectorは登録した順に区画を並べる
/// </summary>
class ComponentEditorRegistry {
public:
	// 登録する。同じ名前のものが登録済みなら何もしない（シーンを作り直すたびに増えないように）
	static void Register(std::unique_ptr<ComponentEditor> editor);
	static const std::vector<std::unique_ptr<ComponentEditor>>& GetAll();
};