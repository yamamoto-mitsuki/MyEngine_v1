#include "MyEngine/Utils/GlobalVariables.h"
#include "MyEngine/Log/LogManager.h"
#include <fstream>
#include <format>
#include <Windows.h>

GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables globalVariables;
	return &globalVariables;
}

void GlobalVariables::AddGroup(const std::string& groupName) {
	datas_[groupName];
}

void GlobalVariables::AddCategory(const std::string& groupName_, const std::string& categoryName_) {
	auto it = datas_.find(groupName_); // 追加する対象のグループ名
	// 既にあるグループかチェック
	if (it != datas_.end()) {
		Group& group = it->second;
		group[categoryName_];
		return;
	}
	LogManager::Log(groupName_ + "がImGuiのグループで見つかりませんでした");
}

void GlobalVariables::Update() {
#ifdef USE_IMGUI
	// ImGuiウィンドウ
	ImGui::Begin("Global Variables", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	// --- 各グループ ---
	for (auto itData = datas_.begin(); itData != datas_.end(); ++itData) {
		const std::string& groupName = itData->first; // 現在のグループ名
		Group& group = itData->second; // 現在のグループ
		// グループ名を表示
		if (!ImGui::CollapsingHeader(groupName.c_str())) continue;

		// --- 各カテゴリ ---
		for (auto itCategory = group.begin(); itCategory != group.end(); ++itCategory) {
			const std::string& categoryName = itCategory->first; // 現在のカテゴリ名
			Category& category = itCategory->second; // 現在のカテゴリ
			// カテゴリを表示
			if (!ImGui::TreeNode(categoryName.c_str())) continue;

			// --- 各項目　---
			for (auto itItem = category.begin(); itItem != category.end(); ++itItem) {
				DrawValue(itItem->first, itItem->second);
			}

			ImGui::TreePop();
		}

		ImGui::Text("\n"); // 改行

		if (ImGui::Button("Save")) {
			SaveFile(groupName);
			std::string message = std::format("{}.json Saved", groupName);
			ImGui::OpenPopup("SavedPopup");
		}
		// ポップアップを画面中央に固定
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
		ImGui::SetNextWindowSize(ImVec2(120, 80), ImGuiCond_Always);

		if (ImGui::BeginPopupModal("SavedPopup", nullptr, 
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
			float windowWidth = ImGui::GetWindowWidth();
			float windowHeight = ImGui::GetWindowHeight();
			float textWidth = ImGui::CalcTextSize("Saved!").x;     // テキストの横幅を取得
			ImGui::SetCursorPosX((windowWidth - textWidth) / 2.0f);
			ImGui::TextColored(ImVec4(1, 1, 1, 1), "Saved!");
			ImGui::Spacing();
			// ボタン
			float buttonWidth = 50.0f;
			float buttonHeight = 25.0f;
			ImGui::SetCursorPosX((windowWidth - buttonWidth) / 2.0f);
			ImGui::SetCursorPosY(windowHeight - buttonHeight - 10.0f);
			if (ImGui::Button("OK", ImVec2(buttonWidth, buttonHeight))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	ImGui::End();
#endif
}

void GlobalVariables::SaveFile(const std::string& groupName) {
	auto itGroup = datas_.find(groupName);
	// 指定したグループがあるかチェック
	if (itGroup == datas_.end()) {
		LogManager::Log(groupName + "は登録されていないグループ名です");
		assert(false && "登録されていないグループ名を Save しようとしました。");
		return;
	}

	json root;
	root = json::object();
	// jsonオブジェクトを登録
	root[groupName] = json::object();

	// --- 各カテゴリ ---
	for (auto itCategory = itGroup->second.begin(); itCategory != itGroup->second.end(); ++itCategory) {
		const std::string& categoryName = itCategory->first;
		// jsonオブジェクトに登録
		root[groupName][categoryName] = json::object();

		// --- 各項目　---
		for (auto itItem = itCategory->second.begin(); itItem != itCategory->second.end(); ++itItem) {
			const std::string& itemName = itItem->first;
			Item& item = itItem->second;

			std::visit([&](const auto& v) {
				using T = std::decay_t<decltype(v)>;

				if constexpr (std::is_same_v<T, int32_t>) {
					root[groupName][categoryName][itemName] = v;
				} else if constexpr (std::is_same_v<T, float>) {
					root[groupName][categoryName][itemName] = v;
				} else if constexpr (std::is_same_v<T, Vector2>) {
					root[groupName][categoryName][itemName] = json::array({v.x, v.y});
				} else if constexpr (std::is_same_v<T, Vector3>) {
					root[groupName][categoryName][itemName] = json::array({v.x, v.y, v.z});
				} else if constexpr (std::is_same_v<T, Vector4>) {
					root[groupName][categoryName][itemName] = json::array({v.x, v.y, v.z, v.w});
				}
			}, item);
		}
	}

	// --- ファイルに書き出し ---
	std::filesystem::path dir(kDirecoryPath);
	// ディレクトリがないければ生成
	if (!std::filesystem::exists(dir)) {
		std::filesystem::create_directory(dir);
	}
	// jsonファイルパス
	std::string filePath = kDirecoryPath + groupName + ".json";

	std::ofstream ofs; 
	ofs.open(filePath); // ファイルを開く(なければ新規作成)
	// ファイルが開けないとき
	if (ofs.fail()) {
		LogManager::Log(groupName + "グループのファイルを開けませんでした");
		assert(false && "Save で保存ができませんでした。");
		return;
	}
	// ファイルにjson文字列を書き込む(インデント幅4)
	ofs << std::setw(4) << root << std::endl;
	ofs.close(); // ファイルを閉じる
}

void GlobalVariables::LoadFile(const std::string& groupName) {
	// 読み込むファイル
	std::string filePath = kDirecoryPath + groupName + ".json";
	// 読み込み用ファイルストリーム
	std::ifstream ifs;
	ifs.open(filePath);
	// 読み込み失敗
	if (ifs.fail()) {
		LogManager::Log(groupName + ".jsonを読み込めませんでした。");
		assert(false && ".jsonを読み込めませんでした。");
		return;
	}

	json root;
	// json文字列からjsonのデータ構造に展開
	ifs >> root;
	ifs.close();

	// グループ検索
	json::iterator itGroup = root.find(groupName);
	// 未登録ではないか
	if (itGroup == root.end()) {
		LogManager::Log(groupName + "グループがjsonの中に見つかりません。");
		assert(false && "jsonに未登録のグループが参照されました。");
	}

	// --- 各カテゴリ ---
	for (json::iterator itCategory = itGroup->begin(); itCategory != itGroup->end(); ++itCategory) {
		const std::string& categoryName = itCategory.key(); // アイテム名を取得

		for (json::iterator itItem = itCategory->begin(); itItem != itCategory->end(); ++itItem) {
			const std::string& itemName = itItem.key(); // アイテム名を取得
			// jsonの型を判定して対応するItemをセット
			if (itItem->is_number_integer()) {
				datas_[groupName][categoryName][itemName] = itItem->get<int32_t>();

			} else if (itItem->is_array()) {
				if (itItem->size() == 2) {
					datas_[groupName][categoryName][itemName] = 
						Vector2{ itItem->at(0).get<float>(),itItem->at(1).get<float>() };

				} else if (itItem->size() == 3) {
					datas_[groupName][categoryName][itemName] = 
						Vector3{ itItem->at(0).get<float>(), itItem->at(1).get<float>(), 
						itItem->at(2).get<float>() };

				} else if (itItem->size() == 4) {
					datas_[groupName][categoryName][itemName] = 
						Vector4{ itItem->at(0).get<float>(), itItem->at(1).get<float>(), 
						itItem->at(2).get<float>(), itItem->at(3).get<float>() };
				}
			}
		}
	}
}

void GlobalVariables::LoadFiles() {
	std::filesystem::path dir(kDirecoryPath);
	// ディレクトリがないければスキップ
	if (!std::filesystem::exists(dir)) {
		return;
	}

	std::filesystem::directory_iterator dir_it(dir);
	for (const std::filesystem::directory_entry& entry : dir_it) {
		const std::filesystem::path& filePath = entry.path();  // ファイルパス取得
		std::string extension = filePath.extension().string(); // ファイル拡張子取得
		// .json以外スキップ
		if (extension.compare(".json") != 0) {
			continue;
		}
		// ファイル読み込み
		LoadFile(filePath.stem().string());
	}
}