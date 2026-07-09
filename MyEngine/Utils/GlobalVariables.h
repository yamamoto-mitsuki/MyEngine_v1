#pragma once
#include <vector>
#include <string>
#include <variant>
#include <map>
#include <memory>
#include <filesystem>
#include <iomanip>
#include <type_traits>
#include <externals/imgui/imgui.h>
#include <externals/nlohmann/json.hpp>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Debug/MyAssert.h"

// ===== 調整項目の管理 =====
class GlobalVariables {
public:
    // ImGui::Combo用
    struct ComboItem {
        std::vector<std::string> options; // 選択肢リスト
        int currentIndex = 0;             // 現在の選択インデックス
    };

    // ImGui::ColorEdit4用
    struct ColorItem {
        uint32_t rgba = 0xFFFFFFFF;
    };

    using Item     = std::variant<bool, int32_t, float, Vector2, Vector3, Vector4, ComboItem, ColorItem>;
    using json     = nlohmann::json;
    // ツリーノード
    struct GVNode {
		struct Entry {
			std::string name;
			std::variant<Item, std::shared_ptr<GVNode>> value;
		};
		std::vector<Entry> entries_;
        // 子ノードを名前で検索
		GVNode* FindChild(const std::string& name) {
			for (auto& e : entries_) {
				if (e.name == name && std::holds_alternative<std::shared_ptr<GVNode>>(e.value)) {
					return std::get<std::shared_ptr<GVNode>>(e.value).get();
				}
			}
			return nullptr;
		}
        // 子ノードを名前で取得
		GVNode* GetOrCreateChild(const std::string& name) {
			if (GVNode* found = FindChild(name)) {
				return found;
			}
			entries_.push_back({name, std::make_shared<GVNode>()});
			return std::get<std::shared_ptr<GVNode>>(entries_.back().value).get();
		}
        // アイテムを名前で検索
		Item* FindItem(const std::string& name) {
			for (auto& e : entries_) {
				if (e.name == name && std::holds_alternative<Item>(e.value)) {
					return &std::get<Item>(e.value);
				}
			}
			return nullptr;
		}
    };

    // ===== グループの追加 ===== 
    class GroupBuilder {
	public:
		GroupBuilder(GVNode* node, const std::string& sceneName) : node_(node), sceneName_(sceneName) {}

        /// <summary>
		/// 子グループに移動する。なければ作成する。
		/// <para>例: .Group("A").Group("B").Add(...)</para>
		/// </summary>
		GroupBuilder Group(const std::string& groupName) { return GroupBuilder(node_->GetOrCreateChild(groupName), sceneName_); }

        /// <summary>
		/// 項目を追加する。すでに登録済みの場合はスキップする（値を上書きしない）。
		/// <para>例: .Add("X", x).Add("Y", y)</para>
		/// </summary>
		template<typename T> GroupBuilder& Add(const std::string& itemName, const T& value) {
			if (!node_->FindItem(itemName)) {
				node_->entries_.push_back({itemName, Item(value)}); // items_ → entries_
				LogManager::Log(sceneName_ + " / " + itemName);
			}
			return *this;
		}

    private:
		GlobalVariables::GVNode* node_;
		std::string sceneName_;
    };

    // ===== シーンの追加 =====
	class SceneBuilder {
	public:
		SceneBuilder(GVNode* root, const std::string& sceneName) : root_(root), sceneName_(sceneName) {}

		/// <summary>
		/// グループを取得または作成してGroupBuilderを返す。
		/// </summary>
		GroupBuilder Group(const std::string& groupName) { return GroupBuilder(root_->GetOrCreateChild(groupName), sceneName_); }

	private:
		GVNode* root_;
		std::string sceneName_;
	};

	// シングルトン
	static GlobalVariables* GetInstance();

	// コピー禁止
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	/// <summary>
	/// シーンを宣言する。
	/// <para>既に存在する場合はそのシーンのルートノードを返す。</para>
	/// </summary>
	/// <param name="sceneName">シーン名</param>
	SceneBuilder Scene(const std::string& sceneName);

	/// <summary>
	/// 値を取得する。パスは "/" 区切りで指定する。
	/// 例: Get&lt;Vector3&gt;("NormalScene", "Enemy/Transform", "Position") → NormalScene → Enemy → Transform → Position の値を返す
	/// </summary>
	/// <param name="sceneName">シーン名</param>
	/// <param name="groupPath">"/" 区切りのグループパス（例: "Enemy/Transform"）</param>
	/// <param name="itemName">項目名</param>
	template<typename T> 
	T Get(const std::string& sceneName, const std::string& groupPath, const std::string& itemName) const {
		const GVNode* node = FindNode(sceneName, groupPath);
		if (!node) {
			LogManager::Error("ノードが見つかりません: " + sceneName + " / " + groupPath);
			MY_ASSERT_MSG(false, "ノードが見つかりません");
		}
		for (const auto& e : node->entries_) {
			if (e.name == itemName && std::holds_alternative<Item>(e.value)) {
				return std::get<T>(std::get<Item>(e.value));
			}
		}
		LogManager::Error("アイテムが見つかりません: " + itemName);
		MY_ASSERT_MSG(false, "アイテムが見つかりません");
		return T{};
	}

	/// <summary>
	/// 登録済みアイテムの値を強制上書きする。
	/// 未登録の場合は何もしない。
	/// ウィンドウ追加後のComboリセット等、毎フレームでなく
	/// 特定タイミングで値を書き換えたいときに使う。
	/// </summary>
	template<typename T> 
	void Set(const std::string& sceneName, const std::string& groupPath, const std::string& itemName, const T& value) {
		GVNode* node = FindNode(sceneName, groupPath);
		if (!node) {
			return;
		}

		Item* item = node->FindItem(itemName);
		if (!item) {
			return;
		}

		*item = Item(value);
	}

	/// <summary>enum型 E から ComboItem を自動生成する（optionsを手入力せずに済む）</summary>
	template<typename E> static ComboItem MakeEnumCombo(E current) {
		static_assert(std::is_enum_v<E>, "MakeEnumCombo は enum 専用です");
		ComboItem combo;
		for (std::string_view name : magic_enum::enum_names<E>()) {
			combo.options.emplace_back(name); // 識別子名をそのまま選択肢に
		}
		combo.currentIndex = static_cast<int>(magic_enum::enum_index(current).value_or(0));
		return combo;
	}

	/// <summary>ComboItem の選択インデックスを enum E に戻す</summary>
	template<typename E> static E GetEnumCombo(const ComboItem& combo) {
		static_assert(std::is_enum_v<E>, "GetEnumCombo は enum 専用です");
		size_t idx = static_cast<size_t>(combo.currentIndex);
		if (idx >= magic_enum::enum_count<E>()) { // JSONに古いindexが残っていた場合の保険
			idx = 0;
		}
		return magic_enum::enum_value<E>(idx);
	}

	/// <summary>
	/// ImGuiの描画更新。ImGuiManager経由で毎フレーム呼ばれる。
	/// </summary>
	void Update();

	/// <summary>
	/// 指定シーンのデータをjsonに書き出す。
	/// </summary>
	void SaveFile(const std::string& sceneName);

	/// <summary>
	/// 指定シーンのjsonを読み込む。ファイルがなければスキップ。
	/// </summary>
	void LoadFile(const std::string& sceneName);

	/// <summary>
	/// Resources/GlobalVariables/ 以下の全jsonを読み込む。
	/// </summary>
	void LoadFiles();

	/// <summary>
	/// 登録済み全シーンを一括保存する。
	/// </summary>
	void SaveAll();

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;

	// ===== 内部ヘルパー =====
	const GVNode* FindNode(const std::string& sceneName, const std::string& groupPath) const;
	GVNode* FindNode(const std::string& sceneName, const std::string& groupPath);
	void NodeToJson(const GVNode& node, json& out) const;
	void JsonToNode(const json& json, GVNode& node);
#ifdef USE_IMGUI
	void DrawNode(GVNode& node, const std::string& uniquePath);
	void DrawItem(const std::string& label, Item& item, const std::string& uid);
#endif

	std::vector<std::pair<std::string, GVNode>> scenes_;
	const std::string kDirectoryPath = "Resources/Parameters/";
};