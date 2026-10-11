#pragma once
#include <string>
#include <type_traits>
#include <typeinfo>

#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Systems/ScriptSystem.h"

// 前方宣言
class Script;


/// <summary>
/// Entity を Unity の GameObject のように扱う窓口。中身は Handle だけなので、コピーして持ち歩いてよい
/// <para>名前・Component・Script の実体はエンジンの中にあり、これは「どの Entity か」を覚えているだけ</para>
/// <para>相手が消えると Handle が合わなくなり、`if (target)` が false になる。GetComponent も nullptr を返す</para>
/// </summary>
struct GameObject {
	Handle<Entity> handle; // どの Entity か

	// ===== 変数のように書ける値（Unity と同じ名前）=====
	__declspec(property(get = GetTransform)) TransformComponent& transform; // 位置・回転・大きさ（消えた GameObject で読むと止まる）
	__declspec(property(get = GetName, put = SetName)) std::string name;    // 名前
	__declspec(property(get = IsActiveSelf)) bool activeSelf;               // 自分の有効・無効
	__declspec(property(get = IsActiveInHierarchy)) bool activeInHierarchy; // 親まで全部有効か（無効の親の下なら false）

	// ===== 作る・探す =====
	// 新しい Entity を作る（Unity の new GameObject("名前")）。Transform はすぐ付いている
	static GameObject Create(const std::string& name = "GameObject", GameObject parent = {});
	// 名前で探す（同じ名前が何個もあれば最初の1つ。無ければ消えた GameObject と同じ扱い）
	static GameObject Find(const std::string& name);

	// ===== Component・Script =====
	/// <summary>
	/// Component か Script を取り出す。持っていなければ nullptr。ポインタは使い捨て（フレームをまたいで持たない）
	/// <para>Script は親クラスの型でも見つかる（Unity と同じ）</para>
	/// </summary>
	template<class T> T* GetComponent() const {
		if constexpr (std::is_base_of_v<Script, T>) {
			// 「T か、T を継承した Script か」を調べる関数を渡して探してもらう（ScriptSystem は T を知らないので）
			Script* found = ScriptSystem::Find(handle, [](const Script& script) { return dynamic_cast<const T*>(&script) != nullptr; });
			return dynamic_cast<T*>(found);
		} else {
			return EntityManager::Get<T>(handle);
		}
	}

	/// <summary>
	/// Script を付ける。付けた Script を返す（もう付いていればそれ）。Awake・Start は次のフレームの Update の前
	/// </summary>
	template<class T>
	    requires std::is_base_of_v<Script, T>
	T* AddComponent() const {
		return dynamic_cast<T*>(ScriptSystem::Add(handle, typeid(T)));
	}

	/// <summary>
	/// データの Component を付ける（予約。付くのは次のフレームの頭なので、何も返さない）
	/// </summary>
	template<class T>
	    requires(!std::is_base_of_v<Script, T>)
	void AddComponent(const T& initial = {}) const {
		EntityManager::RequestAdd<T>(handle, initial);
	}

	// ===== 有効・無効 =====
	// その場で変わる。Script の OnEnable・OnDisable は、次の Update の前に呼ばれる
	void SetActive(bool isActive) const;

	// ===== 比べる・確かめる =====
	explicit operator bool() const { return EntityManager::IsAlive(handle); } // まだ居るか
	bool operator==(const GameObject& other) const { return handle == other.handle; }

	// ===== 上の「変数のように書ける値」の中身（直接呼んでもよい）=====
	TransformComponent& GetTransform() const;
	std::string GetName() const;
	void SetName(const std::string& newName) const;
	bool IsActiveSelf() const;
	bool IsActiveInHierarchy() const;
};