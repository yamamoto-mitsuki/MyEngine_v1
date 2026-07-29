#include <Windows.h>
#include <fstream>
#include <sstream>
#include <format>
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Diagnostics/LogManager.h"

//=============================================================================
// シングルトン
//=============================================================================
GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables instance;
	return &instance;
}

//=============================================================================
// グループを宣言する
// 既に存在する場合はそのルートノードをそのまま返す（データをリセットしない）。
//=============================================================================
GlobalVariables::GroupRoot GlobalVariables::Group(const std::string& groupName) {
	// 既存グループを検索
	for (auto& [name, node] : groups_) {
		if (name == groupName) return GroupRoot(&node, groupName);
	}
	// 新規作成
	groups_.emplace_back(groupName, GVNode{});
	LogManager::Log("RegisterGroup: " + groupName);
	return GroupRoot(&groups_.back().second, groupName);
}

//=============================================================================
// groupName と "/" 区切りの categoryPath からノードを検索する
//=============================================================================
const GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& groupName, const std::string& categoryPath) const {
	// グループを検索
	const GVNode* node = nullptr;
	for (const auto& [name, n] : groups_) {
		if (name == groupName) {
			node = &n;
			break;
		}
	}
	if (!node) {
		return nullptr;
	}
	// "/" 区切りでパスを分割して辿る
	std::istringstream ss(categoryPath);
	std::string token;
	while (std::getline(ss, token, '/')) {
		if (token.empty()) {
			continue;
		}
		bool found = false;
		for (const auto& e : node->entries_) {
			if (e.name == token && std::holds_alternative<std::shared_ptr<GVNode>>(e.value)) {
				node = std::get<std::shared_ptr<GVNode>>(e.value).get();
				found = true;
				break;
			}
		}
		if (!found) {
			return nullptr;
		}
	}
	return node;
}

// ===== 非const版 =====
GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& groupName, const std::string& categoryPath) {
	// const版に委譲してconst_castで返す
	return const_cast<GVNode*>(static_cast<const GlobalVariables*>(this)->FindNode(groupName, categoryPath));
}

//=============================================================================
// グループのルートノードを検索する
//=============================================================================
GlobalVariables::GVNode* GlobalVariables::FindGroupRoot(const std::string& groupName) {
	for (auto& [name, node] : groups_) {
		if (name == groupName) {
			return &node;
		}
	}
	return nullptr;
}

//=============================================================================
// ImGui描画更新
//=============================================================================
void GlobalVariables::Update() {
#ifdef USE_IMGUI
	ImGui::Begin("Parameters");

	if (groups_.empty()) {
		ImGui::TextDisabled("登録されたグループがありません");
		ImGui::End();
		return;
	}

	// グループタブ
	if (ImGui::BeginTabBar("GroupTabs")) {
		for (auto& [groupName, rootNode] : groups_) {
			if (!ImGui::BeginTabItem(groupName.c_str())) {
				continue;
			}
			DrawGroup(groupName, rootNode);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
#endif
}

#ifdef USE_IMGUI
//=============================================================================
// グループ1つ分（タブの中身）を描画する
// 直下のカテゴリは <カテゴリ名>.json の単位なので、ヘッダ右端に個別Saveボタンを出す
//=============================================================================
void GlobalVariables::DrawGroup(const std::string& groupName, GVNode& root) {
	constexpr float kSaveButtonWidth = 46.0f;

	for (auto& entry : root.entries_) {
		// カテゴリに属さないアイテムはその場で描画（通常は無い）
		if (std::holds_alternative<Item>(entry.value)) {
			DrawItem(entry.name, std::get<Item>(entry.value), "##" + groupName + "/" + entry.name);
			continue;
		}

		auto& categoryNode = std::get<std::shared_ptr<GVNode>>(entry.value);
		std::string headerPath = groupName + "/" + entry.name;

		// ヘッダを出した後にボタンを重ねるので、先に行の左端と幅を控えておく
		float startX = ImGui::GetCursorPosX();
		float availWidth = ImGui::GetContentRegionAvail().x;

		bool opened = ImGui::CollapsingHeader((entry.name + "##" + headerPath).c_str(), ImGuiTreeNodeFlags_AllowOverlap);

		// カテゴリ単体の保存
		ImGui::SameLine(startX + availWidth - kSaveButtonWidth);
		if (ImGui::SmallButton(("Save##" + headerPath).c_str())) {
			SaveCategory(groupName, entry.name);
			savedMessage_ = "Saved: " + groupName + "/" + entry.name + ".json";
			ImGui::OpenPopup("SavedPopup");
		}

		if (opened) {
			ImGui::Indent();
			DrawNode(*categoryNode, headerPath);
			ImGui::Unindent();
		}
	}

	ImGui::Spacing();

	// グループ内の全カテゴリを一括保存
	if (ImGui::Button("Save All Categories")) {
		SaveGroup(groupName);
		savedMessage_ = "Saved: " + groupName + "/*.json";
		ImGui::OpenPopup("SavedPopup");
	}

	DrawSavedPopup();
}

//=============================================================================
// ノードを再帰的に描画する
//=============================================================================
void GlobalVariables::DrawNode(GVNode& node, const std::string& uniquePath) {
	for (auto& entry : node.entries_) {
		if (std::holds_alternative<Item>(entry.value)) {
			// ① itemはその場で直接描画
			std::string uid = "##" + uniquePath + "/" + entry.name;
			DrawItem(entry.name, std::get<Item>(entry.value), uid);

		} else {
			// ② 子カテゴリはCollapsingHeaderを開いて中身を再帰描画
			auto& childNode = std::get<std::shared_ptr<GVNode>>(entry.value);
			std::string headerPath = uniquePath + "/" + entry.name;
			std::string headerLabel = entry.name + "##" + headerPath;

			if (!ImGui::CollapsingHeader(headerLabel.c_str())) {
				continue;
			}
			ImGui::Indent();
			DrawNode(*childNode, headerPath);
			ImGui::Unindent();
		}
	}
}
//=============================================================================
// 1つのアイテムをImGuiで描画する
// ラベル（左）＋ ウィジェット（右）の2列レイアウト
//=============================================================================
void GlobalVariables::DrawItem(const std::string& label, Item& item, const std::string& uid) {
	std::visit( [&](auto& v) {
		using T = std::decay_t<decltype(v)>;

		    if constexpr (std::is_same_v<T, bool>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::Checkbox(uid.c_str(), &v);

		    } else if constexpr (std::is_same_v<T, int32_t>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragInt(uid.c_str(), &v);

		    } else if constexpr (std::is_same_v<T, float>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat(uid.c_str(), &v, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector2>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat2(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector3>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat3(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector4>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat4(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, ComboItem>) {
			    std::vector<const char*> cstrs;
			    for (const auto& s : v.options) {
				    cstrs.push_back(s.c_str());
			    }
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::Combo(uid.c_str(), &v.currentIndex, cstrs.data(), static_cast<int>(cstrs.size()));

		    } else if constexpr (std::is_same_v<T, ColorItem>) {
			    float col[4];
			    col[0] = ((v.rgba >> 24) & 0xFF) / 255.0f;
			    col[1] = ((v.rgba >> 16) & 0xFF) / 255.0f;
			    col[2] = ((v.rgba >> 8) & 0xFF) / 255.0f;
			    col[3] = (v.rgba & 0xFF) / 255.0f;
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    if (ImGui::ColorEdit4(uid.c_str(), col)) {
				    uint32_t r = static_cast<uint32_t>(col[0] * 255.0f);
				    uint32_t g = static_cast<uint32_t>(col[1] * 255.0f);
				    uint32_t b = static_cast<uint32_t>(col[2] * 255.0f);
				    uint32_t a = static_cast<uint32_t>(col[3] * 255.0f);
				    v.rgba = (r << 24) | (g << 16) | (b << 8) | a;
			    }
		    }
	    },
	    item);
}

//=============================================================================
// 保存完了ポップアップ
//=============================================================================
void GlobalVariables::DrawSavedPopup() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(320, 90), ImGuiCond_Always);
	if (!ImGui::BeginPopupModal("SavedPopup", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
		return;
	}
	float ww = ImGui::GetWindowWidth();
	float wh = ImGui::GetWindowHeight();
	float tw = ImGui::CalcTextSize(savedMessage_.c_str()).x;
	ImGui::SetCursorPosX((ww - tw) / 2.0f);
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", savedMessage_.c_str());
	ImGui::Spacing();
	float bw = 50.0f, bh = 25.0f;
	ImGui::SetCursorPosX((ww - bw) / 2.0f);
	ImGui::SetCursorPosY(wh - bh - 10.0f);
	if (ImGui::Button("OK", ImVec2(bw, bh))) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
#endif

//=============================================================================
// カテゴリ1つを Resources/Parameters/<グループ名>/<カテゴリ名>.json に書き出す
//=============================================================================
void GlobalVariables::SaveCategory(const std::string& groupName, const std::string& categoryName) {
	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		LogManager::Error("[GlobalVariables::SaveCategory] グループが見つかりません: " + groupName);
		return;
	}
	GVNode* category = root->FindChild(categoryName);
	if (!category) {
		LogManager::Error("[GlobalVariables::SaveCategory] カテゴリが見つかりません: " + groupName + " / " + categoryName);
		return;
	}
	WriteNodeToFile(CategoryFilePath(groupName, categoryName), *category);
}

//=============================================================================
// グループ内の全カテゴリを一括保存する
//=============================================================================
void GlobalVariables::SaveGroup(const std::string& groupName) {
	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		LogManager::Error("[GlobalVariables::SaveGroup] グループが見つかりません: " + groupName);
		return;
	}

	int savedCount = 0;
	for (const auto& entry : root->entries_) {
		// カテゴリ（子ノード）だけがファイルの単位
		if (!std::holds_alternative<std::shared_ptr<GVNode>>(entry.value)) {
			continue;
		}
		if (WriteNodeToFile(CategoryFilePath(groupName, entry.name), *std::get<std::shared_ptr<GVNode>>(entry.value))) {
			++savedCount;
		}
	}
	LogManager::Log(std::format("[GlobalVariables::SaveGroup] {} のカテゴリを{}件保存しました", groupName, savedCount));
}

//=============================================================================
// 全グループを一括保存
//=============================================================================
void GlobalVariables::SaveAll() {
	for (const auto& [groupName, _] : groups_) {
		SaveGroup(groupName);
	}
	LogManager::Log("[GlobalVariables::SaveAll] 全グループ保存完了");
}

//=============================================================================
// ノードの中身をjsonのルートに書き出してファイルへ保存する
//=============================================================================
bool GlobalVariables::WriteNodeToFile(const std::filesystem::path& filePath, const GVNode& node) const {
	json j = json::object();
	NodeToJson(node, j);

	// ディレクトリ作成
	std::error_code ec;
	std::filesystem::create_directories(filePath.parent_path(), ec);

	std::ofstream ofs(filePath);
	if (!ofs) {
		LogManager::Error("[GlobalVariables] ファイルを開けません: " + filePath.string());
		assert(false && "GlobalVariables: ファイルオープン失敗");
		return false;
	}
	ofs << std::setw(4) << j << std::endl;
	LogManager::Log("[GlobalVariables] 保存完了: " + filePath.string());
	return true;
}

//=============================================================================
// ノードをjsonに再帰的に書き出す
// アイテムはフラットに、子カテゴリは子オブジェクトとして書き出す
//=============================================================================
void GlobalVariables::NodeToJson(const GVNode& node, json& out) const {
	for (const auto& entry : node.entries_) {
		if (std::holds_alternative<Item>(entry.value)) {
			std::visit(
			    [&](const auto& v) {
				    using T = std::decay_t<decltype(v)>;
				    if constexpr (std::is_same_v<T, bool>) {
					    out[entry.name] = v;
				    } else if constexpr (std::is_same_v<T, int32_t>) {
					    out[entry.name] = v;
				    } else if constexpr (std::is_same_v<T, float>) {
					    out[entry.name] = v;
				    } else if constexpr (std::is_same_v<T, Vector2>) {
					    out[entry.name] = json::array({v.x, v.y});
				    } else if constexpr (std::is_same_v<T, Vector3>) {
					    out[entry.name] = json::array({v.x, v.y, v.z});
				    } else if constexpr (std::is_same_v<T, Vector4>) {
					    out[entry.name] = json::array({v.x, v.y, v.z, v.w});
				    } else if constexpr (std::is_same_v<T, ColorItem>) {
					    out[entry.name] = static_cast<int32_t>(v.rgba);
				    } else if constexpr (std::is_same_v<T, ComboItem>) {
					    out[entry.name] = v.currentIndex;
				    }
			    },
			    std::get<Item>(entry.value));
		} else {
			json childJson = json::object();
			NodeToJson(*std::get<std::shared_ptr<GVNode>>(entry.value), childJson);
			out[entry.name] = childJson;
		}
	}
}

//=============================================================================
// jsonファイルの読み込み
// ファイルなし・壊れたjsonは false を返すだけで落とさない
//=============================================================================
bool GlobalVariables::ReadJsonFile(const std::filesystem::path& filePath, json& out) const {
	std::ifstream ifs(filePath);
	if (!ifs) {
		LogManager::Warning("[GlobalVariables] ファイルなし（スキップ）: " + filePath.string());
		return false;
	}
	// 例外を投げない版のパース
	out = json::parse(ifs, nullptr, false);
	if (out.is_discarded()) {
		LogManager::Error("[GlobalVariables] jsonの解析に失敗しました: " + filePath.string());
		return false;
	}
	return true;
}

//=============================================================================
// カテゴリ1つ分のjsonを読み込む
// 既に登録済みのアイテムにのみ値を適用する（新規アイテムは作らない）
//=============================================================================
void GlobalVariables::LoadCategory(const std::string& groupName, const std::string& categoryName) {
	std::filesystem::path filePath = CategoryFilePath(groupName, categoryName);
	json j;
	if (!ReadJsonFile(filePath, j)) {
		return;
	}

	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		// まだグループが登録されていない場合はスキップ
		// （LoadFiles()は登録より前に呼ばれることがある）
		return;
	}
	GVNode* category = root->FindChild(categoryName);
	if (!category) {
		// まだカテゴリが登録されていない場合もスキップ
		return;
	}
	JsonToNode(j, *category);
	LogManager::Log("[GlobalVariables::LoadCategory] 読み込み完了: " + filePath.string());
}

//=============================================================================
// グループ1つ分を読み込む
//=============================================================================
void GlobalVariables::LoadGroup(const std::string& groupName) {
	// 旧形式を先に読み、新形式で上書きする
	LoadLegacyGroupFile(groupName);
	LoadGroupCategories(groupName);
}

//=============================================================================
// Resources/Parameters/<グループ名>/ 以下の全カテゴリを読み込む
//=============================================================================
void GlobalVariables::LoadGroupCategories(const std::string& groupName) {
	std::filesystem::path dir = GroupDirectory(groupName);
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file() || entry.path().extension().string() != ".json") {
			continue;
		}
		LoadCategory(groupName, entry.path().stem().string());
	}
}

//=============================================================================
// 旧形式（Resources/Parameters/<グループ名>.json にカテゴリがまとまっている）を読み込む
// 一度Saveすれば新形式のファイルが出来るので、その後はこちらを消してよい
//=============================================================================
void GlobalVariables::LoadLegacyGroupFile(const std::string& groupName) {
	std::filesystem::path filePath = std::filesystem::path(kDirectoryPath) / (groupName + ".json");
	std::error_code ec;
	if (!std::filesystem::exists(filePath, ec)) {
		return;
	}
	json j;
	if (!ReadJsonFile(filePath, j)) {
		return;
	}
	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		return;
	}
	JsonToNode(j, *root);
	LogManager::Warning("[GlobalVariables] 旧形式のjsonを読み込みました: " + filePath.string() + " （Saveするとカテゴリ単位のファイルに移行します）");
}

//=============================================================================
// jsonをノードに再帰的に読み込む
// 登録済みアイテムの型に合わせて値を復元する
//=============================================================================
void GlobalVariables::JsonToNode(const json& j, GVNode& node) {
	for (auto it = j.begin(); it != j.end(); ++it) {
		const std::string& key = it.key();

		// アイテムとして登録済みか確認
		Item* itemPtr = node.FindItem(key);
		if (itemPtr) {
			// 型に合わせて復元
			std::visit(
			    [&](auto& v) {
				    using T = std::decay_t<decltype(v)>;
				    if constexpr (std::is_same_v<T, bool>) {
					    if (it->is_boolean()) {
						    v = it->get<bool>();
					    }
				    } else if constexpr (std::is_same_v<T, int32_t>) {
					    if (it->is_number_integer()) {
						    v = it->get<int32_t>();
					    }
				    } else if constexpr (std::is_same_v<T, float>) {
					    if (it->is_number()) {
						    v = it->get<float>();
					    }
				    } else if constexpr (std::is_same_v<T, Vector2>) {
					    if (it->is_array() && it->size() == 2) {
						    v = Vector2{it->at(0).get<float>(), it->at(1).get<float>()};
					    }
				    } else if constexpr (std::is_same_v<T, Vector3>) {
					    if (it->is_array() && it->size() == 3) {
						    v = Vector3{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>()};
					    }
				    } else if constexpr (std::is_same_v<T, Vector4>) {
					    if (it->is_array() && it->size() == 4) {
						    v = Vector4{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>(), it->at(3).get<float>()};
					    }
				    } else if constexpr (std::is_same_v<T, ColorItem>) {
					    if (it->is_number_integer()) {
						    v.rgba = static_cast<uint32_t>(it->get<int32_t>());
					    }
				    } else if constexpr (std::is_same_v<T, ComboItem>) {
					    if (it->is_number_integer()) {
						    v.currentIndex = it->get<int>();
					    }
				    }
			    },
			    *itemPtr);
			continue;
		}

		// 子カテゴリとして登録済みか確認
		GVNode* childNode = node.FindChild(key);
		if (childNode && it->is_object()) {
			JsonToNode(*it, *childNode);
		}
	}
}

//=============================================================================
// 全ファイル読み込み
// ① 旧形式: Resources/Parameters/<グループ名>.json
// ② 新形式: Resources/Parameters/<グループ名>/<カテゴリ名>.json
//=============================================================================
void GlobalVariables::LoadFiles() {
	std::filesystem::path dir(kDirectoryPath);
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		return;
	}

	// ① 旧形式（新形式に上書きされるように先に読む）
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file() || entry.path().extension().string() != ".json") {
			continue;
		}
		LoadLegacyGroupFile(entry.path().stem().string());
	}

	// ② 新形式（フォルダ1つがグループ1つ）
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_directory()) {
			continue;
		}
		LoadGroupCategories(entry.path().filename().string());
	}
}
