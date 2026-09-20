#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <numbers>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Entity/TransformComponent.h"

// ゲーム固有Componentを書くための道具。ゲーム側はこのヘッダ1つだけをインクルードする
//
//   1ファイル＝1Component。書くのは「① データ ② 見せ方 ③ 処理」の3つだけ
//
//   struct Spin { Vector3 speed = {0.0f, 90.0f, 0.0f}; };            // ① データ
//   COMPONENT(Spin, "Movement") { ui.Field("Speed", value.speed); }  // ② 見せ方
//   void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) { ... }  // ③ 処理
//   SYSTEM(SpinUpdate);

// 度 ↔ ラジアン（TransformComponent::rotation はラジアン）
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

//=============================================================================
// COMPONENT(...) の中身を Inspector の区画につなぐ
//=============================================================================

/// <summary>
/// COMPONENT(...) に書いた「見せ方」を使うComponentEditor。ゲームは直接触らない
/// </summary>
template<class T> class DescribedComponentEditor final : public TypedComponentEditor<T> {
public:
	using DescribeFunction = void (*)(ComponentUI& ui, T& value);

	DescribedComponentEditor(const char* name, std::string category, DescribeFunction describe) : name_(name), category_(std::move(category)), describe_(describe) {}

	const char* GetName() const override { return name_; }
	const char* GetCategory() const override { return category_.c_str(); }

protected:
	void DrawComponent(T& value) const override { describe_(GetInspectorUI(), value); }

private:
	const char* name_;          // COMPONENT(型名) の型名がそのまま表示名になる
	std::string category_;      // "Gameplay" か "Gameplay/サブカテゴリ"
	DescribeFunction describe_; // ②に書いた見せ方
};

// "Movement" から "Gameplay/Movement" を作る（空なら "Gameplay"）
inline std::string MakeGameplayCategory(const char* subCategory) {
	std::string path = "Gameplay";
	if (subCategory != nullptr && subCategory[0] != 0) {
		path += "/";
		path += subCategory;
	}
	return path;
}

//=============================================================================
// 登録待ちの表（static変数が作られるのはmainより前なので、その場では登録できない）
//=============================================================================
namespace ComponentRegistryDetail {
// Componentの登録を1つ予約する
void AddPending(std::function<void()> apply);
// Systemを1つ登録する
void AddSystem(const char* name, std::function<void(float)> run);
} // namespace ComponentRegistryDetail

/// <summary>
/// COMPONENT(...) が1つ作る物。作られた時点では「登録待ちの表」に並ぶだけ
/// </summary>
template<class T> class ComponentRegistrar {
public:
	ComponentRegistrar(const char* name, const char* subCategory, void (*describe)(ComponentUI&, T&)) {
		ComponentRegistryDetail::AddPending([name, subCategory, describe]() {
			EntityManager::RegisterComponent<T>();                                                                                               // 置き場所
			ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<T>>(name, MakeGameplayCategory(subCategory), describe)); // Inspector
		});
	}
};

//=============================================================================
// SYSTEM(...) の中身（関数の引数の型から、回すComponentを決める）
//=============================================================================
namespace ComponentSystemDetail {

// 型の並びを持ち運ぶための箱
template<class... Ts> struct TypeList {};

// 最後の引数の型（引数の無い関数は登録できない）
template<class... Args> struct LastArg;
template<class T> struct LastArg<T> {
	using type = T;
};
template<class T, class... Rest> struct LastArg<T, Rest...> : LastArg<Rest...> {};

// tupleの Offset 番目から Index... 個を取り出して TypeList にする
template<class Tuple, size_t Offset, class Sequence> struct MakeTypeList;
template<class Tuple, size_t Offset, size_t... Index> struct MakeTypeList<Tuple, Offset, std::index_sequence<Index...>> {
	using type = TypeList<std::tuple_element_t<Offset + Index, Tuple>...>;
};

/// <summary>
/// 関数の引数を「Entity」「必要なComponent」「deltaTime」に分ける
/// <para>先頭が Handle&lt;Entity&gt; ならEntityを渡す。最後が float なら deltaTime を渡す。間は全部Component</para>
/// </summary>
template<class... Args> struct SystemArgs {
	static constexpr size_t count = sizeof...(Args);
	static_assert(count >= 1, "SYSTEMの関数には、Componentの参照を1つ以上書いてください");

	// 番兵の int を足して、引数が少なくても添字がはみ出さないようにする
	using AllWithGuard = std::tuple<Args..., int>;
	static constexpr bool hasEntity = std::is_same_v<std::tuple_element_t<0, AllWithGuard>, Handle<Entity>>;
	static constexpr bool hasDeltaTime = std::is_same_v<typename LastArg<Args...>::type, float>;
	static constexpr size_t componentCount = count - (hasEntity ? 1 : 0) - (hasDeltaTime ? 1 : 0);
	static_assert(componentCount >= 1, "SYSTEMの関数には、Componentの参照を1つ以上書いてください（Handleとfloatだけでは回せません）");

	using Components = typename MakeTypeList<std::tuple<Args...>, hasEntity ? 1 : 0, std::make_index_sequence<componentCount>>::type;
};

/// <summary>
/// First と Rest... を全部持っていて、親までたどって有効なEntityだけを回す
/// </summary>
template<bool HasEntity, bool HasDeltaTime, class Function, class First, class... Rest> void RunSystem(Function function, float deltaTime) {
	using FirstComponent = std::remove_cvref_t<First>;
	EntityManager::ForEach<FirstComponent>([&](Handle<Entity> entity, FirstComponent& first) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		// 残りのComponentを集める。1つでも持っていなければ呼ばない
		const std::tuple<std::remove_cvref_t<Rest>*...> rest(EntityManager::Get<std::remove_cvref_t<Rest>>(entity)...);
		const bool hasAll = std::apply([](auto*... component) { return ((component != nullptr) && ...); }, rest);
		if (!hasAll) {
			return;
		}
		std::apply(
		    [&](auto*... component) {
			    if constexpr (HasEntity && HasDeltaTime) {
				    function(entity, first, *component..., deltaTime);
			    } else if constexpr (HasEntity) {
				    function(entity, first, *component...);
			    } else if constexpr (HasDeltaTime) {
				    function(first, *component..., deltaTime);
			    } else {
				    function(first, *component...);
			    }
		    },
		    rest);
	});
}

// TypeList を開いて RunSystem に渡す
template<bool HasEntity, bool HasDeltaTime, class Function, class... Components> void RunSystemFromList(Function function, float deltaTime, TypeList<Components...>) {
	RunSystem<HasEntity, HasDeltaTime, Function, Components...>(function, deltaTime);
}

} // namespace ComponentSystemDetail

/// <summary>
/// SYSTEM(...) が1つ作る物。作られた時点では毎フレームの呼び出し表に並ぶだけ
/// </summary>
class SystemRegistrar {
public:
	template<class... Args> SystemRegistrar(const char* name, void (*function)(Args...)) {
		using Info = ComponentSystemDetail::SystemArgs<Args...>;
		ComponentRegistryDetail::AddSystem(
		    name, [function](float deltaTime) { ComponentSystemDetail::RunSystemFromList<Info::hasEntity, Info::hasDeltaTime>(function, deltaTime, typename Info::Components{}); });
	}
};

/// <summary>
/// COMPONENT / SYSTEM で書いた物をまとめて扱う。呼ぶのはエンジンだけ
/// </summary>
class GameComponentRegistry {
public:
	// 登録待ちを全部登録する。EntityManager::Initialize の後に1回呼ぶ
	static void ApplyAll();
	// SYSTEM(...) で登録した処理を全部呼ぶ。シーンのUpdateの後に呼ぶ
	static void UpdateAll(float deltaTime);
	// 登録された数（確認用）
	static size_t GetComponentCount();
	static size_t GetSystemCount();
};

//=============================================================================
// ゲーム側が書くのはこの2つだけ
//=============================================================================

/// <summary>
/// Componentの見せ方を書く。COMPONENT(型名) または COMPONENT(型名, "サブカテゴリ")
/// <para>中では ui（見せ方の道具）と value（Componentの中身）が使える。カテゴリは Gameplay で固定</para>
/// </summary>
// [[maybe_unused]] は「使わなくても警告しないで」の印。中身の無いComponent（印だけのタグ）でも警告が出ないようにしている
#define COMPONENT(Type, ...)                                                                                                                                                                           \
	inline void Describe_##Type([[maybe_unused]] ComponentUI& ui, [[maybe_unused]] Type& value);                                                                                                       \
	inline const ComponentRegistrar<Type> registrarOf_##Type(#Type, "" __VA_ARGS__, &Describe_##Type);                                                                                                 \
	inline void Describe_##Type([[maybe_unused]] ComponentUI& ui, [[maybe_unused]] Type& value)

/// <summary>
/// 毎フレームの処理を登録する。関数の引数に書いたComponentを全部持つEntityだけが呼ばれる
/// <para>先頭の引数を Handle&lt;Entity&gt; にするとそのEntityが入る（消したいとき用）。最後の引数を float にすると deltaTime が入る</para>
/// </summary>
#define SYSTEM(function) inline const SystemRegistrar registrarOfSystem_##function(#function, &function)