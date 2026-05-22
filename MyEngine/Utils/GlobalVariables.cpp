#include "MyEngine/Utils/GlobalVariables.h"
#include "MyEngine/Log/LogManager.h"
#include <fstream>
#include <format>
#include <Windows.h>

GlobalVariables* GlobalVariables::GetInstance() {
    static GlobalVariables globalVariables;
    return &globalVariables;
}

//==========================================
// シーンを追加
//==========================================
void GlobalVariables::AddScene(const std::string& sceneName) {
    datas_[sceneName];
}

//==========================================
// グループを追加
//==========================================
void GlobalVariables::AddGroup(const std::string& sceneName, const std::string& groupName) {
    auto it = datas_.find(sceneName);
    if (it != datas_.end()) {
        it->second[groupName];
        return;
    }
    LogManager::Log(sceneName + "シーンが見つかりませんでした");
}

//==========================================
// カテゴリを追加
//==========================================
void GlobalVariables::AddCategory(const std::string& sceneName, const std::string& groupName,
    const std::string& categoryName) {
    // シーンを検索
    auto itScene = datas_.find(sceneName);
    if (itScene == datas_.end()) {
        LogManager::Log(sceneName + "シーンが見つかりませんでした");
        return;
    }
    // グループを検索
    auto itGroup = itScene->second.find(groupName);
    if (itGroup != itScene->second.end()) {
        itGroup->second[categoryName];
        return;
    }
    LogManager::Log(groupName + "グループが見つかりませんでした");
}

//==========================================
// ImGui描画
//==========================================
void GlobalVariables::Update() {
#ifdef USE_IMGUI
    ImGui::Begin("Global Variables", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // --- シーンごとにタブを表示 ---
    if (ImGui::BeginTabBar("SceneTabs")) {
        for (auto& [sceneName, scene] : datas_) {
            if (ImGui::BeginTabItem(sceneName.c_str())) {

                // --- 各グループ ---
                for (auto& [groupName, group] : scene) {
                    if (!ImGui::CollapsingHeader(groupName.c_str())) continue;

                    // --- 各カテゴリ ---
                    for (auto& [categoryName, category] : group) {
                        if (!ImGui::TreeNode(categoryName.c_str())) continue;

                        // --- 各アイテム ---
                        for (auto& [itemName, item] : category) {
                            DrawValue(itemName, item);
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::Text("\n");

                // Saveボタン
                if (ImGui::Button("Save")) {
                    SaveFile(sceneName);
                    ImGui::OpenPopup("SavedPopup");
                }

                // Saveポップアップ
                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
                ImGui::SetNextWindowSize(ImVec2(120, 80), ImGuiCond_Always);
                if (ImGui::BeginPopupModal("SavedPopup", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
                    float ww = ImGui::GetWindowWidth();
                    float wh = ImGui::GetWindowHeight();
                    float tw = ImGui::CalcTextSize("Saved!").x;
                    ImGui::SetCursorPosX((ww - tw) / 2.0f);
                    ImGui::TextColored(ImVec4(1,1,1,1), "Saved!");
                    ImGui::Spacing();
                    float bw = 50.0f, bh = 25.0f;
                    ImGui::SetCursorPosX((ww - bw) / 2.0f);
                    ImGui::SetCursorPosY(wh - bh - 10.0f);
                    if (ImGui::Button("OK", ImVec2(bw, bh))) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}

//==========================================
// ファイルに書き出し
//==========================================
void GlobalVariables::SaveFile(const std::string& sceneName) {
    auto itScene = datas_.find(sceneName);
    // 指定したシーンがあるかチェック
    if (itScene == datas_.end()) {
        LogManager::Log(sceneName + "は登録されていないシーン名です");
        assert(false && "登録されていないシーン名をSaveしようとしました。");
        return;
    }

    json root;
    root = json::object();
    root[sceneName] = json::object();

    // --- 各グループ ---
    for (auto& [groupName, group] : itScene->second) {
        root[sceneName][groupName] = json::object();

        // --- 各カテゴリ ---
        for (auto& [categoryName, category] : group) {
            root[sceneName][groupName][categoryName] = json::object();

            // --- 各アイテム ---
            for (auto& [itemName, item] : category) {
                std::visit([&](const auto& v) {
                    using T = std::decay_t<decltype(v)>;

                    if constexpr (std::is_same_v<T, bool>) {
                        root[sceneName][groupName][categoryName][itemName] = v;
                    } else if constexpr (std::is_same_v<T, int32_t>) {
                        root[sceneName][groupName][categoryName][itemName] = v;
                    } else if constexpr (std::is_same_v<T, float>) {
                        root[sceneName][groupName][categoryName][itemName] = v;
                    } else if constexpr (std::is_same_v<T, Vector2>) {
                        root[sceneName][groupName][categoryName][itemName] = json::array({v.x, v.y});
                    } else if constexpr (std::is_same_v<T, Vector3>) {
                        root[sceneName][groupName][categoryName][itemName] = json::array({v.x, v.y, v.z});
                    } else if constexpr (std::is_same_v<T, Vector4>) {
                        root[sceneName][groupName][categoryName][itemName] = json::array({v.x, v.y, v.z, v.w});
                    } else if constexpr (std::is_same_v<T, ColorItem>) {
                        // uint32_tをそのままintとして保存
                        root[sceneName][groupName][categoryName][itemName] = static_cast<int32_t>(v.rgba);
                    } else if constexpr (std::is_same_v<T, ComboItem>) {
                        // ComboItemは保存しない
                    }
                    }, item);
            }
        }
    }

    // --- ファイルに書き出し ---
    std::filesystem::path dir(kDirecoryPath);
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directory(dir);
    }
    std::string filePath = kDirecoryPath + sceneName + ".json";
    std::ofstream ofs;
    ofs.open(filePath);
    if (ofs.fail()) {
        LogManager::Log(sceneName + "のファイルを開けませんでした");
        assert(false && "Saveで保存ができませんでした。");
        return;
    }
    ofs << std::setw(4) << root << std::endl;
    ofs.close();
}

//==========================================
// ファイル読み込み
//==========================================
void GlobalVariables::LoadFile(const std::string& sceneName) {
    std::string filePath = kDirecoryPath + sceneName + ".json";
    std::ifstream ifs;
    ifs.open(filePath);
    // 読み込み失敗はスキップ
    if (ifs.fail()) {
        LogManager::Log(sceneName + ".jsonを読み込めませんでした。スキップします。");
        return;
    }

    json root;
    ifs >> root;
    ifs.close();

    // シーンを検索
    json::iterator itScene = root.find(sceneName);
    if (itScene == root.end()) {
        LogManager::Log(sceneName + "シーンがjsonの中に見つかりません。スキップします。");
        return;
    }

    // --- 各グループ ---
    for (json::iterator itGroup = itScene->begin(); itGroup != itScene->end(); ++itGroup) {
        const std::string& groupName = itGroup.key();

        // --- 各カテゴリ ---
        for (json::iterator itCategory = itGroup->begin(); itCategory != itGroup->end(); ++itCategory) {
            const std::string& categoryName = itCategory.key();

            // --- 各アイテム ---
            for (json::iterator itItem = itCategory->begin(); itItem != itCategory->end(); ++itItem) {
                const std::string& itemName = itItem.key();

                auto& existing = datas_[sceneName][groupName][categoryName];

                // ComboItemとして登録済みならindexだけ復元
                if (existing.find(itemName) != existing.end()) {
                    if (std::holds_alternative<ComboItem>(existing[itemName])) {
                        if (itItem->is_number_integer()) {
                            std::get<ComboItem>(existing[itemName]).currentIndex = itItem->get<int>();
                        }
                        continue;
                    }
                    // ColorItemとして登録済みならrgbaを復元
                    if (std::holds_alternative<ColorItem>(existing[itemName])) {
                        if (itItem->is_number_integer()) {
                            std::get<ColorItem>(existing[itemName]).rgba =
                                static_cast<uint32_t>(itItem->get<int32_t>());
                        }
                        continue;
                    }
                }

                if (itItem->is_boolean()) {
                    datas_[sceneName][groupName][categoryName][itemName] = itItem->get<bool>();
                } else if (itItem->is_number_integer()) {
                    datas_[sceneName][groupName][categoryName][itemName] = itItem->get<int32_t>();
                } else if (itItem->is_number_float()) {
                    datas_[sceneName][groupName][categoryName][itemName] = itItem->get<float>();
                } else if (itItem->is_array()) {
                    if (itItem->size() == 2) {
                        datas_[sceneName][groupName][categoryName][itemName] =
                            Vector2{ itItem->at(0).get<float>(), itItem->at(1).get<float>() };
                    } else if (itItem->size() == 3) {
                        datas_[sceneName][groupName][categoryName][itemName] =
                            Vector3{ itItem->at(0).get<float>(), itItem->at(1).get<float>(),
                            itItem->at(2).get<float>() };
                    } else if (itItem->size() == 4) {
                        datas_[sceneName][groupName][categoryName][itemName] =
                            Vector4{ itItem->at(0).get<float>(), itItem->at(1).get<float>(),
                            itItem->at(2).get<float>(), itItem->at(3).get<float>() };
                    }
                }
            }
        }
    }
}

//==========================================
// 全ファイル読み込み
//==========================================
void GlobalVariables::LoadFiles() {
    std::filesystem::path dir(kDirecoryPath);
    // ディレクトリがなければスキップ
    if (!std::filesystem::exists(dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension().string() != ".json") continue;
        LoadFile(entry.path().stem().string());
    }
}