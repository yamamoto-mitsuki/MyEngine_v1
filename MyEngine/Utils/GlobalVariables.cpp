#include <Windows.h>
#include <fstream>
#include <sstream>
#include <format>
#include "MyEngine/Utils/GlobalVariables.h"
#include "MyEngine/Log/LogManager.h"

//=============================================================================
// シングルトン
//=============================================================================
GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables instance;
	return &instance;
}

//=============================================================================
// シーンを宣言する
// 既に存在する場合はそのルートノードをそのまま返す（データをリセットしない）。
//=============================================================================
GlobalVariables::SceneBuilder GlobalVariables::Scene(const std::string& sceneName) {
	// 既存シーンを検索
	for (auto& [name, node] : scenes_) {
		if (name == sceneName) return SceneBuilder(&node, sceneName);
	}
	// 新規作成
	scenes_.emplace_back(sceneName, GVNode{});
	LogManager::Log("RegisterScene: " + sceneName);
	return SceneBuilder(&scenes_.back().second, sceneName);
}

//=============================================================================
// sceneName と "/" 区切りの groupPath からノードを検索する
//=============================================================================
const GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& sceneName, const std::string& groupPath) const {
	// シーンを検索
	const GVNode* node = nullptr;
	for (const auto& [name, n] : scenes_) {
		if (name == sceneName) {
			node = &n;
			break;
		}
	}
	if (!node) {
		return nullptr;
	}
	// "/" 区切りでパスを分割して辿る
	std::istringstream ss(groupPath);
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
GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& sceneName, const std::string& groupPath) {
	// const版に委譲してconst_castで返す
	return const_cast<GVNode*>(static_cast<const GlobalVariables*>(this)->FindNode(sceneName, groupPath));
}

//=============================================================================
// ImGui描画更新
//=============================================================================
void GlobalVariables::Update() {
#ifdef USE_IMGUI
	ImGui::Begin("Parameters");

	if (scenes_.empty()) {
		ImGui::TextDisabled("登録されたシーンがありません");
		ImGui::End();
		return;
	}

	// シーンタブ
	if (ImGui::BeginTabBar("SceneTabs")) {
		for (auto& [sceneName, rootNode] : scenes_) {
			if (!ImGui::BeginTabItem(sceneName.c_str())) {
				continue;
			}
			// ルートノードの子グループを描画
			DrawNode(rootNode, sceneName);

			ImGui::Spacing();

			// Saveボタン
			if (ImGui::Button("Save")) {
				SaveFile(sceneName);
				ImGui::OpenPopup("SavedPopup");
			}
			// 保存完了ポップアップ
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(120, 80), ImGuiCond_Always);
			if (ImGui::BeginPopupModal("SavedPopup", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
				float ww = ImGui::GetWindowWidth();
				float wh = ImGui::GetWindowHeight();
				float tw = ImGui::CalcTextSize("Saved!").x;
				ImGui::SetCursorPosX((ww - tw) / 2.0f);
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "Saved!");
				ImGui::Spacing();
				float bw = 50.0f, bh = 25.0f;
				ImGui::SetCursorPosX((ww - bw) / 2.0f);
				ImGui::SetCursorPosY(wh - bh - 10.0f);
				if (ImGui::Button("OK", ImVec2(bw, bh))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
#endif
}

#ifdef USE_IMGUI
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
			// ② child groupはCollapsingHeaderを開いて中身を再帰描画
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
#endif

//=============================================================================
// ファイルに書き出し
//=============================================================================
void GlobalVariables::SaveFile(const std::string& sceneName) {
	// シーンを検索
	GVNode* root = nullptr;
	for (auto& [name, node] : scenes_) {
		if (name == sceneName) {
			root = &node;
			break;
		}
	}
	if (!root) {
		LogManager::Error("[GlobalVariables::SaveFile] シーンが見つかりません: " + sceneName);
		assert(false && "SaveFile: 登録されていないシーン名");
		return;
	}

	json j = json::object();
	NodeToJson(*root, j);

	// ディレクトリ作成
	std::filesystem::create_directories(kDirectoryPath);
	std::string filePath = kDirectoryPath + sceneName + ".json";
	std::ofstream ofs(filePath);
	if (!ofs) {
		LogManager::Error("[GlobalVariables::SaveFile] ファイルを開けません: " + filePath);
		assert(false && "SaveFile: ファイルオープン失敗");
		return;
	}
	ofs << std::setw(4) << j << std::endl;
	LogManager::Log("[GlobalVariables::SaveFile] 保存完了: " + filePath);
}

//=============================================================================
// ノードをjsonに再帰的に書き出す
// アイテムはフラットに、子ノードは子オブジェクトとして書き出す
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
// ファイル読み込み
// 既に登録済みのアイテムにのみ値を適用する（新規アイテムは作らない）
//=============================================================================
void GlobalVariables::LoadFile(const std::string& sceneName) {
	std::string filePath = kDirectoryPath + sceneName + ".json";
	std::ifstream ifs(filePath);
	if (!ifs) {
		LogManager::Warning("[GlobalVariables::LoadFile] ファイルなし（スキップ）: " + filePath);
		return;
	}
	json j;
	ifs >> j;

	// シーンのルートノードを検索
	GVNode* root = nullptr;
	for (auto& [name, node] : scenes_) {
		if (name == sceneName) {
			root = &node;
			break;
		}
	}
	if (!root) {
		// まだシーンが登録されていない場合はスキップ
		// （LoadFiles()はInitialize()より前に呼ばれることがある）
		return;
	}
	JsonToNode(j, *root);
	LogManager::Log("[GlobalVariables::LoadFile] 読み込み完了: " + filePath);
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

		// 子ノードとして登録済みか確認
		GVNode* childNode = node.FindChild(key);
		if (childNode && it->is_object()) {
			JsonToNode(*it, *childNode);
		}
	}
}

//=============================================================================
// 全ファイル読み込み
//=============================================================================
void GlobalVariables::LoadFiles() {
	std::filesystem::path dir(kDirectoryPath);
	if (!std::filesystem::exists(dir)) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (entry.path().extension().string() != ".json") {
			continue;
		}
		LoadFile(entry.path().stem().string());
	}
}

//=============================================================================
// 全シーンを一括保存
//=============================================================================
void GlobalVariables::SaveAll() {
	for (const auto& [sceneName, _] : scenes_) {
		SaveFile(sceneName);
	}
	LogManager::Log("[GlobalVariables::SaveAll] 全シーン保存完了");
}