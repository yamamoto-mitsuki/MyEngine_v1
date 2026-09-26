#pragma once
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"

/// <summary>
/// 項目の見せ方の指定。省略できる（Range(...) や Tip(...) で作る）
/// </summary>
struct FieldStyle {
	float min = 0.0f;              // 下限（min < max のときだけ効く）
	float max = 0.0f;              // 上限
	float dragSpeed = 0.0f;        // つまんで動かす速さ（0なら型ごとの既定）
	bool slider = false;           // true: スライダー、false: つまんで動かす
	const char* tooltip = nullptr; // マウスを乗せたときに出す説明

	// つなげて書ける（Range(0.0f, 10.0f).Tip("説明") のように）
	FieldStyle Tip(const char* text) const {
		FieldStyle copy = *this;
		copy.tooltip = text;
		return copy;
	}
	FieldStyle Drag(float speed) const {
		FieldStyle copy = *this;
		copy.dragSpeed = speed;
		copy.slider = false;
		return copy;
	}
};

// 範囲を決める（スライダーになる）
inline FieldStyle Range(float min, float max) {
	FieldStyle style;
	style.min = min;
	style.max = max;
	style.slider = true;
	return style;
}

// 下限だけ決める（つまんで動かす。負の値にしたくない項目に）
inline FieldStyle AtLeast(float min) {
	FieldStyle style;
	style.min = min;
	style.max = FLT_MAX; // windows.h の max マクロに邪魔されないよう、numeric_limits ではなく cfloat のFLT_MAXを使う
	return style;
}

// つまんで動かす速さを決める
inline FieldStyle Drag(float speed) {
	FieldStyle style;
	style.dragSpeed = speed;
	return style;
}

// 説明だけ付ける
inline FieldStyle Tip(const char* text) {
	FieldStyle style;
	style.tooltip = text;
	return style;
}

// ファイルから読み込む物（アセット）の種類。AssetField で使う
enum class AssetType {
	Model,   // ModelManager::Load の番号
	Texture, // TextureManager::Load の番号
};

// アセットの番号からパスを引く（未選択・知らない番号なら空）。保存とInspectorの表示で使う
const std::string& GetAssetPath(AssetType type, uint32_t handle);
// パスのファイルを読み込んで番号をもらう（空・ファイルが無いなら0＝未選択）
uint32_t LoadAsset(AssetType type, const std::string& path);

/// <summary>
/// Componentの中身の見せ方を書くための道具。ゲーム側にImGuiが出てこないようにするための薄い包み
/// <para>実体は差し替えられる。Inspectorに描く係・ファイルへ書く係・ファイルから読む係の3つがあり、
/// COMPONENT(...) に書いた並びが、そのまま「Inspectorの見た目」と「セーブデータの項目」になる（ComponentSerializer.cpp）</para>
/// <para>保存するときの項目の名前は label。label を変えると、前に保存した値は読めなくなる（読み込み時に警告が出る）</para>
/// </summary>
class ComponentUI {
public:
	virtual ~ComponentUI() = default;

	// ===== 値を1つ見せる（型で見た目が決まる）=====
	virtual void Field(const char* label, float& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, int& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, bool& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector2& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector3& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector4& value, FieldStyle style = {}) = 0;

	// ===== 別のEntityへの参照（InspectorではHierarchyからドラッグ＆ドロップで入れる。保存はEntityIdで）=====
	virtual void Field(const char* label, EntityRef& value, FieldStyle style = {}) = 0;

	// ===== enum（選択肢の名前は magic_enum が型から作る。保存も名前で行うので、並びを変えても読める）=====
	template<class E>
	    requires std::is_enum_v<E>
	void Field(const char* label, E& value, FieldStyle style = {}) {
		constexpr auto names = magic_enum::enum_names<E>(); // 選択肢の名前（宣言の順）
		const auto current = magic_enum::enum_index(value); // 今の値が何番目か（magic_enumが知らない値なら無し）
		size_t index = current.value_or(0);
		const size_t before = index;
		EnumField(label, index, names, style);
		// 変わったときだけ書き戻す（知らない値を、触っていないのに先頭の選択肢へ変えてしまわないように）
		if (index != before && index < names.size()) {
			value = magic_enum::enum_value<E>(index);
		}
	}

	// ===== アセット（モデル・テクスチャの番号。Inspectorではファイルを選ぶ。保存はパスで）=====
	virtual void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle style = {}) = 0;

	// ===== 色（0〜1。色見本を押すと色を選べる）=====
	virtual void ColorField(const char* label, Vector3& rgb) = 0;
	virtual void ColorField(const char* label, Vector4& rgba) = 0;

	// ===== 飾り（保存には関係しない）=====
	virtual void Label(const char* text) = 0;               // 灰色の説明文
	virtual void Separator(const char* text = nullptr) = 0; // 区切り線（文字を入れると見出しになる）
	virtual void Space() = 0;                               // 1行あける

protected:
	// enumの中身。何番目の選択肢かで受け渡す（names は選択肢の名前）
	virtual void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle style) = 0;
};

// Inspectorに描く係（エンジンが1つだけ持っている。ゲームからは触らない）
ComponentUI& GetInspectorUI();