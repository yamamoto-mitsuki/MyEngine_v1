#include "MyEngine/Component/ComponentUI.h"

#include <filesystem>
#include <format>
#include <string>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

//=============================================================================
// アセットの番号 ↔ パス（Inspector・保存・読み込みで共通）
//=============================================================================
const std::string& GetAssetPath(AssetType type, uint32_t handle) {
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	if (handle == 0) {
		return kEmpty;
	}
	switch (type) {
	case AssetType::Model:
		return ModelManager::GetModelPath(handle);
	case AssetType::Texture:
		return TextureManager::GetTexturePath(handle);
	}
	return kEmpty;
}

uint32_t LoadAsset(AssetType type, const std::string& path) {
	if (path.empty()) {
		return 0; // 未選択
	}
	// 無いファイルを読ませるとManagerの中で止まるので、先に確かめる
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		LogManager::Warning(std::format("アセットが見つかりません（未選択として扱います）: {}", path));
		return 0;
	}
	switch (type) {
	case AssetType::Model:
		return ModelManager::Load(path);
	case AssetType::Texture:
		return TextureManager::Load(path);
	}
	return 0;
}

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
// 範囲が決まっているか
bool HasRange(const FieldStyle& style) { return style.min < style.max; }

// つまんで動かす速さ（指定が無ければ型ごとの既定）
float DragSpeed(const FieldStyle& style, float defaultSpeed) { return style.dragSpeed > 0.0f ? style.dragSpeed : defaultSpeed; }

// 直前の項目にマウスを乗せたら説明を出す
void DrawTooltip(const FieldStyle& style) {
	if (style.tooltip != nullptr && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", style.tooltip);
	}
}

// ===== アセットの一覧（フォルダを掘るのは、最初に開いたときと Refresh のときだけ）=====
constexpr const char* kAssetSearchRoots[] = {"resources", "MyEngine/Resources"}; // 探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr std::string_view kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};
constexpr std::string_view kTextureExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".hdr"};

// その種類の拡張子か（大文字でも通るように小文字へ直して比べる）
bool IsAssetFile(const std::filesystem::path& path, AssetType type) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const std::span<const std::string_view> candidates = (type == AssetType::Model) ? std::span<const std::string_view>(kModelExtensions) : std::span<const std::string_view>(kTextureExtensions);
	return std::find(candidates.begin(), candidates.end(), extension) != candidates.end();
}

// その種類のファイルのパス一覧（区切りは / にそろえる。ModelManagerのキャッシュのキーと同じ形にするため）
const std::vector<std::string>& AssetFiles(AssetType type, bool refresh) {
	static std::vector<std::string> files[2]; // [AssetType] ごとの一覧
	static bool scanned[2] = {false, false};
	const size_t slot = static_cast<size_t>(type);
	if (scanned[slot] && !refresh) {
		return files[slot];
	}
	files[slot].clear();
	for (const char* root : kAssetSearchRoots) {
		std::error_code error; // 例外ではなくエラーコードで受ける（フォルダが無くても止まらない）
		if (!std::filesystem::exists(root, error)) {
			continue;
		}
		const auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsAssetFile(entry.path(), type)) {
				files[slot].push_back(entry.path().generic_string());
			}
		}
	}
	std::sort(files[slot].begin(), files[slot].end());
	scanned[slot] = true;
	return files[slot];
}

/// <summary>
/// Inspectorに描く係。ImGuiを知っているのはここだけ
/// </summary>
class InspectorComponentUI final : public ComponentUI {
public:
	void Field(const char* label, float& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat(label, &value, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, int& value, FieldStyle style) override {
		const int min = static_cast<int>(style.min);
		const int max = static_cast<int>(style.max);
		if (style.slider && HasRange(style)) {
			ImGui::SliderInt(label, &value, min, max);
		} else if (HasRange(style)) {
			ImGui::DragInt(label, &value, DragSpeed(style, 1.0f), min, max);
		} else {
			ImGui::DragInt(label, &value, DragSpeed(style, 1.0f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, bool& value, FieldStyle style) override {
		ImGui::Checkbox(label, &value);
		DrawTooltip(style);
	}

	void Field(const char* label, Vector2& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat2(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector3& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat3(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector4& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat4(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	// 相手の名前を出した台。HierarchyからEntityをドラッグして落とすと入る。右クリックで外す
	void Field(const char* label, EntityRef& value, FieldStyle style) override {
		const Entity* target = EntityManager::Get(EntityManager::Find(value));
		// 番号はあるのに居ない＝相手が消えている（消した操作をUndoすると、同じ番号で戻ってまた指す）
		const std::string preview = target ? target->name : (value.IsSet() ? "(missing)" : "(none)");
		ImGui::PushID(label);                                                 // 同じ区画に参照が2つあっても、台のIDがぶつからないように
		ImGui::Button(preview.c_str(), ImVec2(ImGui::CalcItemWidth(), 0.0f)); // 押しても何もしない
		DrawTooltip(style);
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HierarchyWindow::kDragDropType)) {
				// Hierarchyが運んでくるのはHandle。覚えるのは作り直しても変わらない番号
				value = EntityManager::RefOf(*static_cast<const Handle<Entity>*>(payload->Data));
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("menu")) {
			if (ImGui::MenuItem("Clear")) {
				value = {};
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		// 他の項目と同じく、ラベルは右に出す（"表示###キー" の ## から後ろは、ImGuiのほかの項目と同じく出さない）
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(label, std::strstr(label, "##")); // 見つからなければnullptr＝最後まで出す
	}

	// resources以下の、その種類のファイルから選ぶ
	void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle style) override {
		const std::string& path = GetAssetPath(type, handle);
		const char* preview = (handle == 0) ? "(none)" : (path.empty() ? "(unknown)" : path.c_str());
		if (ImGui::BeginCombo(label, preview)) {
			const bool refresh = ImGui::Selectable("(Refresh)"); // フォルダにファイルを増やしたとき用
			if (ImGui::Selectable("(none)", handle == 0)) {
				handle = 0;
			}
			for (const std::string& file : AssetFiles(type, refresh)) {
				if (ImGui::Selectable(file.c_str(), file == path)) {
					handle = LoadAsset(type, file); // 同じパスならキャッシュが返るので、選び直しても読み込み直さない
				}
			}
			ImGui::EndCombo();
		}
		DrawTooltip(style);
	}

	void ColorField(const char* label, Vector3& rgb) override { ImGui::ColorEdit3(label, &rgb.x); }
	void ColorField(const char* label, Vector4& rgba) override { ImGui::ColorEdit4(label, &rgba.x); }

	void Label(const char* text) override { ImGui::TextDisabled("%s", text); }
	void Separator(const char* text) override {
		if (text != nullptr) {
			ImGui::SeparatorText(text);
		} else {
			ImGui::Separator();
		}
	}
	void Space() override { ImGui::Spacing(); }

protected:
	void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle style) override {
		// string_view は末尾に0があるとは限らないので、ImGuiに渡す前に std::string にする
		const std::string current = index < names.size() ? std::string(names[index]) : std::string("?");
		if (ImGui::BeginCombo(label, current.c_str())) {
			for (size_t i = 0; i < names.size(); ++i) {
				const std::string name(names[i]);
				if (ImGui::Selectable(name.c_str(), i == index)) {
					index = i;
				}
				if (i == index) {
					ImGui::SetItemDefaultFocus(); // 開いたときに今の選択へスクロールする
				}
			}
			ImGui::EndCombo();
		}
		DrawTooltip(style);
	}
};
} // namespace

#else // USE_IMGUI

namespace {
/// <summary>
/// Editorの無いビルド（Release）用。Componentの見せ方は書かれているが、描く相手が居ないので何もしない
/// </summary>
class InspectorComponentUI final : public ComponentUI {
public:
	void Field(const char*, float&, FieldStyle) override {}
	void Field(const char*, int&, FieldStyle) override {}
	void Field(const char*, bool&, FieldStyle) override {}
	void Field(const char*, Vector2&, FieldStyle) override {}
	void Field(const char*, Vector3&, FieldStyle) override {}
	void Field(const char*, Vector4&, FieldStyle) override {}
	void Field(const char*, EntityRef&, FieldStyle) override {}
	void AssetField(const char*, uint32_t&, AssetType, FieldStyle) override {}
	void ColorField(const char*, Vector3&) override {}
	void ColorField(const char*, Vector4&) override {}
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}

protected:
	void EnumField(const char*, size_t&, std::span<const std::string_view>, FieldStyle) override {}
};
} // namespace

#endif // USE_IMGUI

//=============================================================================
// Inspectorに描く係を取り出す
//=============================================================================
ComponentUI& GetInspectorUI() {
	static InspectorComponentUI ui; // 最初に呼ばれたときに1つだけ作られる
	return ui;
}