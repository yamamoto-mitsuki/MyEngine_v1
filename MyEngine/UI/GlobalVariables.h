#pragma once
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <iomanip>
#include <filesystem>
#include <type_traits>

#include <externals/imgui/imgui.h>
#include <externals/nlohmann/json.hpp>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Diagnostics//MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"


/// <summary>
/// 調整項目の管理
/// <para>表示の階層は Parameters(ウィンドウ) → グループ(タブ) → カテゴリ(入れ子可) → アイテム。</para>
/// <para>保存はカテゴリ単位。Resources/Parameters/&lt;グループ名&gt;/&lt;カテゴリ名&gt;.json に書き出す。</para>
/// </summary>
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

    // ===== カテゴリの追加 =====
    class CategoryBuilder {
	public:
		CategoryBuilder(GVNode* node, const std::string& groupName) : node_(node), groupName_(groupName) {}

        /// <summary>
		/// 子カテゴリに移動する。なければ作成する。
		/// <para>例: .Category("A").Category("B").Add(...)</para>
		/// </summary>
		CategoryBuilder Category(const std::string& categoryName) { return CategoryBuilder(node_->GetOrCreateChild(categoryName), groupName_); }

		/// <summary>旧名。Category() と同じ。</summary>
		CategoryBuilder Group(const std::string& categoryName) { return Category(categoryName); }

        /// <summary>
		/// 項目を追加する。すでに登録済みの場合はスキップする（値を上書きしない）。
		/// <para>例: .Add("X", x).Add("Y", y)</para>
		/// </summary>
		template<typename T> CategoryBuilder& Add(const std::string& itemName, const T& value) {
			if (!node_->FindItem(itemName)) {
				node_->entries_.push_back({itemName, Item(value)});
				LogManager::Log(groupName_ + " / " + itemName);
			}
			return *this;
		}

    private:
		GlobalVariables::GVNode* node_;
		std::string groupName_;
    };

    // ===== グループのルート =====
	class GroupRoot {
	public:
		GroupRoot(GVNode* root, const std::string& groupName) : root_(root), groupName_(groupName) {}

		/// <summary>
		/// カテゴリを取得または作成して CategoryBuilder を返す。
		/// <para>ここで作られたカテゴリが &lt;カテゴリ名&gt;.json の単位になる。</para>
		/// </summary>
		CategoryBuilder Category(const std::string& categoryName) { return CategoryBuilder(root_->GetOrCreateChild(categoryName), groupName_); }

		/// <summary>旧名。Category() と同じ。</summary>
		CategoryBuilder Group(const std::string& categoryName) { return Category(categoryName); }

	private:
		GVNode* root_;
		std::string groupName_;
	};

	// 旧名エイリアス（既存コードの互換用）
	using GroupBuilder = CategoryBuilder;
	using SceneBuilder = GroupRoot;

	// シングルトン
	static GlobalVariables* GetInstance();

	// コピー禁止
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	/// <summary>
	/// グループを宣言する。（Parametersウィンドウのタブ1つ分）
	/// <para>既に存在する場合はそのグループのルートノードを返す。</para>
	/// </summary>
	/// <param name="groupName">グループ名。保存先フォルダ名にもなる</param>
	GroupRoot Group(const std::string& groupName);

	/// <summary>旧名。Group() と同じ。</summary>
	GroupRoot Scene(const std::string& groupName) { return Group(groupName); }

	/// <summary>
	/// 値を取得する。カテゴリのパスは "/" 区切りで指定する。
	/// 例: Get&lt;Vector3&gt;("GameScene", "Enemy/Transform", "Position") → GameScene → Enemy → Transform → Position の値を返す
	/// </summary>
	/// <param name="groupName">グループ名</param>
	/// <param name="categoryPath">"/" 区切りのカテゴリパス（例: "Enemy/Transform"）</param>
	/// <param name="itemName">項目名</param>
	template<typename T>
	T Get(const std::string& groupName, const std::string& categoryPath, const std::string& itemName) const {
		const GVNode* node = FindNode(groupName, categoryPath);
		if (!node) {
			LogManager::Error("ノードが見つかりません: " + groupName + " / " + categoryPath);
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
	void Set(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const T& value) {
		GVNode* node = FindNode(groupName, categoryPath);
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

	// ===== 保存 =====

	/// <summary>
	/// カテゴリ1つを Resources/Parameters/&lt;グループ名&gt;/&lt;カテゴリ名&gt;.json に書き出す。
	/// </summary>
	void SaveCategory(const std::string& groupName, const std::string& categoryName);

	/// <summary>
	/// グループ内の全カテゴリを一括保存する。（カテゴリごとに1ファイル）
	/// </summary>
	void SaveGroup(const std::string& groupName);

	/// <summary>
	/// 登録済み全グループを一括保存する。
	/// </summary>
	void SaveAll();

	/// <summary>旧名。SaveGroup() と同じ。</summary>
	void SaveFile(const std::string& groupName) { SaveGroup(groupName); }

	// ===== 読み込み =====

	/// <summary>
	/// カテゴリ1つ分のjsonを読み込む。ファイルがなければスキップ。
	/// </summary>
	void LoadCategory(const std::string& groupName, const std::string& categoryName);

	/// <summary>
	/// グループ1つ分（フォルダ内の全カテゴリ）を読み込む。
	/// <para>旧形式の &lt;グループ名&gt;.json が残っていればそれも読み込む。</para>
	/// </summary>
	void LoadGroup(const std::string& groupName);

	/// <summary>
	/// Resources/Parameters/ 以下の全jsonを読み込む。
	/// </summary>
	void LoadFiles();

	/// <summary>旧名。LoadGroup() と同じ。</summary>
	void LoadFile(const std::string& groupName) { LoadGroup(groupName); }

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;

	// ===== 内部ヘルパー =====
	const GVNode* FindNode(const std::string& groupName, const std::string& categoryPath) const;
	GVNode* FindNode(const std::string& groupName, const std::string& categoryPath);
	GVNode* FindGroupRoot(const std::string& groupName);
	void NodeToJson(const GVNode& node, json& out) const;
	void JsonToNode(const json& json, GVNode& node);
	// jsonファイルを out に読み込む。ファイルなし・パース失敗なら false
	bool ReadJsonFile(const std::filesystem::path& filePath, json& out) const;
	// ノードの中身をそのままjsonルートに書き出す
	bool WriteNodeToFile(const std::filesystem::path& filePath, const GVNode& node) const;
	// フォルダ内の <カテゴリ名>.json を全部読む
	void LoadGroupCategories(const std::string& groupName);
	// 旧形式（Resources/Parameters/<グループ名>.json にカテゴリがまとまっている）を読む
	void LoadLegacyGroupFile(const std::string& groupName);
	// 保存先パス
	std::filesystem::path GroupDirectory(const std::string& groupName) const { return std::filesystem::path(kDirectoryPath) / groupName; }
	std::filesystem::path CategoryFilePath(const std::string& groupName, const std::string& categoryName) const { return GroupDirectory(groupName) / (categoryName + ".json"); }
#ifdef USE_IMGUI
	void DrawGroup(const std::string& groupName, GVNode& root);
	void DrawNode(GVNode& node, const std::string& uniquePath);
	void DrawItem(const std::string& label, Item& item, const std::string& uid);
	void DrawSavedPopup();

	std::string savedMessage_; // 保存完了ポップアップに出す文言
#endif

	std::vector<std::pair<std::string, GVNode>> groups_;
	const std::string kDirectoryPath = "Resources/Parameters/";
};
