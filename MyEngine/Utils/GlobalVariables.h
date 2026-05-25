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
        int currentIndex = 0;             // 現在の選択インデックス
    };

    // ImGui::ColorEdit4用
    struct ColorItem {
        uint32_t rgba = 0xFFFFFFFF;
    };

    using Item     = std::variant<bool, int32_t, float, Vector2, Vector3, Vector4, ComboItem, ColorItem>;
    using Category = std::map<std::string, Item>;
    using Group    = std::map<std::string, Category>;
    using Scene    = std::map<std::string, Group>;
    using json     = nlohmann::json;

    // シングルトン
    static GlobalVariables* GetInstance();

    // 更新（ImGui描画）
    void Update();

    //===========================
    // 登録
    //===========================
    void AddScene(const std::string& sceneName);
    void AddGroup(const std::string& sceneName, const std::string& groupName);
    void AddCategory(const std::string& sceneName, const std::string& groupName,
        const std::string& categoryName);

    // 項目が未登録のときのみ登録
    template<typename T>
    void AddItem(const std::string& sceneName, const std::string& groupName,
        const std::string& categoryName, const std::string& itemName, const T& item) {
        if (datas_[sceneName][groupName][categoryName].find(itemName) == datas_[sceneName][groupName][categoryName].end()) {
            SetValue(sceneName, groupName, categoryName, itemName, item);
            LogManager::Log(sceneName + "の" + groupName + "の" + categoryName + "の" + itemName + "をImGuiに登録しました。");
        }
    }

    // 変数をセット（上書き）
    template<typename T>
    void SetValue(const std::string& sceneName, const std::string& groupName,
        const std::string& categoryName, const std::string& itemName, const T& item) {
        datas_[sceneName][groupName][categoryName][itemName] = Item(item);
    }

    // 変数を取得
    template<typename T>
    T GetValue(const std::string& sceneName, const std::string& groupName,
        const std::string& categoryName, const std::string& itemName) const {
        // シーンを検索
        auto itScene = datas_.find(sceneName);
        if (itScene == datas_.end()) {
            LogManager::Log(sceneName + "シーンは登録されていませんでした。");
            assert(false && "登録されていないシーンを取得しようとしました。");
        }
        // グループを検索
        auto itGroup = itScene->second.find(groupName);
        if (itGroup == itScene->second.end()) {
            LogManager::Log(groupName + "グループは登録されていませんでした。");
            assert(false && "登録されていないグループを取得しようとしました。");
        }
        // カテゴリを検索
        auto itCategory = itGroup->second.find(categoryName);
        if (itCategory == itGroup->second.end()) {
            LogManager::Log(categoryName + "カテゴリは登録されていませんでした。");
            assert(false && "登録されていないカテゴリを取得しようとしました。");
        }
        // アイテムを検索
        if (itCategory->second.find(itemName) == itCategory->second.end()) {
            LogManager::Log(itemName + "は登録されていませんでした。");
            assert(false && "登録されていないアイテムを取得しようとしました。");
        }
        return std::get<T>(itCategory->second.at(itemName));
    }

    // ImGuiで描画
    void DrawValue(const std::string& key, Item& item, const std::string& uniquePath) {
        std::visit([&](auto& v) {
            using T = std::decay_t<decltype(v)>;
            // ##の後ろにパスをつける
            std::string uid = "##" + uniquePath + "/" + key;

            if constexpr (std::is_same_v<T, bool>) {
                ImGui::Text(key.c_str());
                ImGui::Checkbox(uid.c_str(), &v);

            } else if constexpr (std::is_same_v<T, int32_t>) {
                ImGui::Text(key.c_str());
                ImGui::DragInt(uid.c_str(), &v);

            } else if constexpr (std::is_same_v<T, float>) {
                ImGui::Text(key.c_str());
                ImGui::DragFloat(uid.c_str(), &v, 0.01f);

            } else if constexpr (std::is_same_v<T, Vector2>) {
                ImGui::Text(key.c_str());
                ImGui::DragFloat2(uid.c_str(), &v.x, 0.01f);

            } else if constexpr (std::is_same_v<T, Vector3>) {
                ImGui::Text(key.c_str());
                ImGui::DragFloat3(uid.c_str(), &v.x, 0.01f);

            } else if constexpr (std::is_same_v<T, Vector4>) {
                ImGui::Text(key.c_str());
                ImGui::DragFloat4(uid.c_str(), &v.x, 0.01f);

            } else if constexpr (std::is_same_v<T, ComboItem>) {
                // ComboBox
                std::vector<const char*> cstrs;
                for (const auto& s : v.options) cstrs.push_back(s.c_str());
                const char* const* items = cstrs.data();
                ImGui::Text(key.c_str());
                ImGui::Combo(uid.c_str(), &v.currentIndex, items, static_cast<int>(cstrs.size()));

            } else if constexpr (std::is_same_v<T, ColorItem>) {
                // uint32_t(0xRRGGBBAA) → float[4] に変換してColorEdit4で表示
                float col[4];
                col[0] = ((v.rgba >> 24) & 0xFF) / 255.0f; // R
                col[1] = ((v.rgba >> 16) & 0xFF) / 255.0f; // G
                col[2] = ((v.rgba >>  8) & 0xFF) / 255.0f; // B
                col[3] = ( v.rgba        & 0xFF) / 255.0f; // A
                ImGui::Text(key.c_str());
                if (ImGui::ColorEdit4(uid.c_str(), col)) {
                    // float[4] → uint32_t(0xRRGGBBAA) に戻す
                    uint32_t r = static_cast<uint32_t>(col[0] * 255.0f);
                    uint32_t g = static_cast<uint32_t>(col[1] * 255.0f);
                    uint32_t b = static_cast<uint32_t>(col[2] * 255.0f);
                    uint32_t a = static_cast<uint32_t>(col[3] * 255.0f);
                    v.rgba = (r << 24) | (g << 16) | (b << 8) | a;
                }
            }
            }, item);
    }

    // ファイルに書き出し
    void SaveFile(const std::string& sceneName);
    // ファイル読み込み
    void LoadFile(const std::string& sceneName);
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
    std::map<std::string, Scene> datas_;
};