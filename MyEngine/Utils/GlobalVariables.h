#pragma once
#include <vector>
#include <string>
#include <variant>
#include <map>
#include <memory>
#include <filesystem>
#include <iomanip>
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Log/LogManager.h"

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
		std::vector<std::pair<std::string, Item>> items_;
		std::vector<std::pair<std::string, std::shared_ptr<GVNode>>> children_;
        // 子ノードを名前で検索
        GVNode* FindChild(const std::string& name) {
			for (const auto& [n, node] : children_) {
				if (n == name) return node.get();
                
            }
			return nullptr;
        }
        // 子ノードを名前で取得
		GVNode* GetOrCreateChild(const std::string& name) {
			for (const auto& [n, node] : children_) {
				if (n == name) return node.get();
            }
			children_.emplace_back(name, std::make_shared<GVNode>());
			return children_.back().second.get();
        }
        // アイテムを名前で検索
		Item* FindItem(const std::string& name) {
			for (auto& [n, item] : items_) {
				if (n == name) return &item;
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
        template<typename T> 
        GroupBuilder& Add(const std::string& itemName, const T& value) {
            // 未登録のとき追加
			if (!node_->FindItem(itemName)) {
				node_->items_.emplace_back(itemName, Item(value));
				LogManager::Log("[GlobalVariables::Add]" + sceneName_ + " / " + itemName);
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
			LogManager::Error("[GlobalVariables::Get] ノードが見つかりません: " + sceneName + " / " + groupPath);
			assert(false && "GlobalVariables::Get ノードが見つかりません");
		}
		for (const auto& [name, item] : node->items_) {
			if (name == itemName) {
				return std::get<T>(item);
			}
		}
		LogManager::Error("[GlobalVariables::Get] アイテムが見つかりません: " + itemName);
		assert(false && "GlobalVariables::Get アイテムが見つかりません");
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