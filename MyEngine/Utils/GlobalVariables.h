#pragma once
#include <vector>
#include <string>
#include <variant>
#include <unordered_map>
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Log/LogManager.h"

class GlobalVariables {
public:
	// ImGui::Combo用
	struct ComboItem {
		std::vector<std::string> options; // 選択肢リスト
		int currentIndex = 0;        // 現在の選択インデックス
	};

	using Item = std::variant<bool, int32_t, float, Vector2, Vector3, Vector4, ComboItem>;
	using Category = std::unordered_map<std::string, Item>;
	using Group = std::unordered_map<std::string, Category>;
	using json = nlohmann::json;

	// シングルトン
	static GlobalVariables* GetInstance();
	// 更新
	void Update();
	// グループ・カテゴリ・アイテム設定
	void AddGroup(const std::string& groupName);
	void AddCategory(const std::string& groupName, const std::string& categoryName);
	template<typename T>
	void AddItem(const std::string& groupName, const std::string& categoryName, const std::string& itemName, const T& item) {
		// 項目が未登録のとき
		if (datas_[groupName][categoryName].find(itemName) == datas_[groupName][categoryName].end()) {
			SetValue(groupName, categoryName, itemName, item);
		}
	}
	// 変数をセット
	template<typename T>
	void SetValue(const std::string& groupName, const std::string& categoryName, const std::string& itemName, const T& item) {
		datas_[groupName][categoryName][itemName] = Item(item);
	}
	// 変数を取得
	template<typename T>
	T GetValue(const std::string& groupName, const std::string& categoryName, const std::string& itemName) const {
		// グループを検索
		std::unordered_map<std::string, Group>::const_iterator itGroup = datas_.find(groupName);
		// 未登録か
		if (itGroup == datas_.end()) {
			LogManager::Log(groupName + "グループは登録されていませんでした。");
			assert(false && "GlobalVariablesファイルからの変数取得時、登録されていないグループを取得しようとしました。");
		}
		// グループ取得
		const Group& group = datas_.at(groupName);

		// カテゴリを検索
		std::unordered_map<std::string, Category>::const_iterator itCategory = group.find(categoryName);
		// 未登録か
		if (itCategory == group.end()) {
			LogManager::Log(groupName + "グループの" + categoryName + "カテゴリは登録されていませんでした。");
			assert(false && "GlobalVariablesファイルからの変数取得時、登録されていないカテゴリを取得しようとしました。");
		}
		// カテゴリ取得
		const Category& category = group.at(categoryName);
		// 指定のキーがあるか
		if (category.find(itemName) == category.end()) {
			LogManager::Log(groupName + "グループの" + categoryName + "カテゴリ、" + itemName + "は登録されていませんでした。");
			assert(false && "GlobalVariablesファイルからの変数取得時、登録されていないアイテムを取得しようとしました。");
		}

		return std::get<T>(category.at(itemName));
	}
	// 実際に描画
	void DrawValue(const std::string& key, Item& item) {
		std::visit([&](auto& v) {
			using T = std::decay_t<decltype(v)>;

			if constexpr (std::is_same_v<T, bool>) {
				ImGui::Checkbox(key.c_str(), &v);

			} else if constexpr (std::is_same_v<T, int32_t>) {
				ImGui::DragInt(key.c_str(), &v);

			} else if constexpr (std::is_same_v<T, float>) {
				ImGui::DragFloat(key.c_str(), &v, 0.01f);

			} else if constexpr (std::is_same_v<T, Vector2>) {
				ImGui::DragFloat2(key.c_str(), &v.x,0.01f);

			} else if constexpr (std::is_same_v<T, Vector3>) {
				ImGui::DragFloat3(key.c_str(), &v.x, 0.01f);

			} else if constexpr (std::is_same_v<T, Vector4>) {
				ImGui::DragFloat4(key.c_str(), &v.x, 0.01f);

			} else if constexpr (std::is_same_v<T, ComboItem>) {
				std::vector<const char*> cstrs;
				for (const auto& s : v.options) cstrs.push_back(s.c_str());
				ImGui::Combo(key.c_str(), &v.currentIndex, cstrs.data(), static_cast<int>(cstrs.size()));
			}
		}, item);
	}

	// ファイルに書き出し
	void SaveFile(const std::string& groupName);
	// ファイル読み込み
	void LoadFile(const std::string& groupName);
	// 全ファイル読み込み
	void LoadFiles();

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	// グローバル変数の保存先のファイルパス
	const std::string kDirecoryPath = "Resources/GlobalVariables/";
	// 全登録データ
	std::unordered_map<std::string, Group> datas_;
};