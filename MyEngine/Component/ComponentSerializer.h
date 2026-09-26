#pragma once
#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#include <externals/nlohmann/json_fwd.hpp>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Entity/EntityManager.h"

// シーンファイルに使うJSONの型
// ・キーは書いた順に並ぶ（ordered_map。ui.Field の順番のままファイルに出る）
// ・小数は float のまま書く（普通の nlohmann::json は double なので、0.1f が 0.10000000149011612 と書かれてしまう）
using SceneJson = nlohmann::basic_json<nlohmann::ordered_map, std::vector, std::string, bool, std::int64_t, std::uint64_t, float>;


/// <summary>
/// Componentの型ごとの「保存する項目」の表。型の名前（ファイルに書く名前）と、見せ方の関数（COMPONENTの中身と同じ形）を持つ
/// <para>見せ方の関数に「書く係」「読む係」の ComponentUI を渡すと、ui.Field の並びがそのままJSONの項目になる</para>
/// <para>Editorの登録表（ComponentEditorRegistry）とは別物。Releaseでもシーンを読むのに要る</para>
/// </summary>
class ComponentSerializer {
public:
	/// <summary>
	/// 型を登録する。name はファイルに書く名前（変えると、前に保存したファイルのその型が読めなくなる）
	/// </summary>
	template<class T> static void Register(const char* name, void (*describe)(ComponentUI&, T&)) {
		RegisterErased(
		    name,
		    // --- 書く：持っていれば、写しに対して見せ方の関数を呼ぶ（関数は T& を受け取るので、本物を書き換えないように写す）---
		    [describe](Handle<Entity> entity, ComponentUI& writer) {
			    const T* component = EntityManager::Get<T>(entity);
			    if (component == nullptr) {
				    return false; // 持っていない
			    }
			    T copy = *component;
			    describe(writer, copy);
			    return true;
		    },
		    // --- 読む：初期値から始めて、ファイルにある項目だけ上書きし、予約せずその場で付ける（Transformのように持っていれば上書き）---
		    [describe](Handle<Entity> entity, ComponentUI& reader) {
			    T value{};
			    describe(reader, value);
			    EntityManager::RestoreComponents(entity, {ComponentSnapshot::Make(value)});
		    });
	}

	// Entityが持っているComponentを全部 out に書く（{"型の名前": {"項目": 値, ...}, ...}）。警告は warnings に集める（同じ文を何度も出さないため）
	static void Save(Handle<Entity> entity, SceneJson& out, std::set<std::string>& warnings);
	// in に書いてあるComponentを全部付ける。知らない型の名前は警告して飛ばす。フレームの境目でだけ呼ぶ
	static void Load(Handle<Entity> entity, const SceneJson& in, std::set<std::string>& warnings);

	// エンジンのComponent（Transform・ModelRenderer・ライト）を登録する。定義は EngineComponents.cpp
	static void RegisterEngineComponents();

private:
	using SaveFunction = std::function<bool(Handle<Entity>, ComponentUI&)>; // 持っていなければfalse
	using LoadFunction = std::function<void(Handle<Entity>, ComponentUI&)>;

	// 型を消した形で表に入れる（名前順に並べる）
	static void RegisterErased(const char* name, SaveFunction save, LoadFunction load);
};