#include "InspectorWindow.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <numbers>
#include <string>
#include <type_traits>
#include <vector>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/HierarchyWindow.h"
#include "MyEngine/Editor/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"

namespace {
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
constexpr const char* kModelSearchRoots[] = {"resources", "MyEngine/Resources"}; // モデルを探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr const char* kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"}; // モデルとして扱う拡張子
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）

std::vector<std::string> modelFiles; // 見つかったモデルのパス
bool modelFilesScanned = false;      // 1回でも走査したか

// 拡張子がモデルのものか（大文字でも通るように小文字へ直して比べる）
bool IsModelFile(const std::filesystem::path& path) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	for (const char* candidate : kModelExtensions) {
		if (extension == candidate) {
			return true;
		}
	}
	return false;
}

// フォルダを掘って、モデルファイルのパスを集める。毎フレームやるとディスクを叩き続けるので、1回だけ
void ScanModelFiles() {
	modelFiles.clear();
	for (const char* root : kModelSearchRoots) {
		std::error_code error; // 例外ではなくエラーコードで受ける（フォルダが無くても止まらない）
		if (!std::filesystem::exists(root, error)) {
			continue;
		}
		auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsModelFile(entry.path())) {
				modelFiles.push_back(entry.path().generic_string()); // 区切りを / に統一する
			}
		}
	}
	std::sort(modelFiles.begin(), modelFiles.end());
	modelFilesScanned = true;
}

// ===== Add Component のメニュー =====
// Componentの分類。Add Componentではカテゴリごとにサブメニューになる（並びはこの順）
enum class ComponentCategory {
	Rendering3D, // 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
	Rendering2D, // 2D（将来のSpriteRenderer）
	Lighting,    // ライト（Light.md Step 6）
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
};

// Add Componentに出す1項目
struct ComponentMenuItem {
	ComponentCategory category;         // どのサブメニューに出すか
	const char* name;                   // 表示名
	bool (*canAdd)(Handle<Entity>);     // 足せるか（まだ持っていない・予約もしていない）
	void (*requestAdd)(Handle<Entity>); // 追加を予約する（実体ができるのは次のフレームの頭）
};

// 足せるComponentの一覧。Componentを増やしたら、ここに1行足す
// （キャプチャしないラムダは関数ポインタに変換できる）
constexpr ComponentMenuItem kComponentMenu[] = {
    {ComponentCategory::Rendering3D, "Model Renderer", [](Handle<Entity> handle) { return !EntityManager::GetModelRenderer(handle) && !EntityManager::IsModelRendererAddPending(handle); },
     [](Handle<Entity> handle) { EntityManager::RequestAddModelRenderer(handle); }},
};
} // namespace


//=============================================================================
// 描画
//=============================================================================
void InspectorWindow::Draw() {
	ImGui::Begin("Inspector");

	// 選択中のEntityはHierarchyWindowが持っている
	Handle<Entity> handle = HierarchyWindow::GetSelected();
	if (!EntityManager::IsAlive(handle)) {
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	// Componentを1つずつ区画にして描く。Componentが増えたらここに足していく
	DrawHeader(handle);
	ImGui::Separator();
	DrawTransform(handle);
	ImGui::Separator();
	DrawModelRenderer(handle);
	DrawAddComponent(handle);

	ImGui::End();
}


//=============================================================================
// Entityそのものの情報
//=============================================================================
void InspectorWindow::DrawHeader(Handle<Entity> handle) {
	// ポインタは使い捨てにする（ARCHITECTURE.md 原則1）。持ち越すのはHandleだけ
	Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// --- 有効・無効 ---
	ImGui::Checkbox("##isActive", &entity->isActive);
	ImGui::SameLine();

	// --- 名前（Entityのstd::stringを直接編集する。長さの上限が無いので日本語でも切れない）---


	// --- 参考情報（読み取り専用） ---
	ImGui::TextDisabled("Handle: index=%u generation=%u", handle.index, handle.generation);
	if (const Entity* parent = EntityManager::Get(entity->parent)) {
		ImGui::TextDisabled("Parent: %s", parent->name.c_str());
	} else {
		ImGui::TextDisabled("Parent: (root)");
	}
}


//=============================================================================
// TransformComponent
//=============================================================================
void InspectorWindow::DrawTransform(Handle<Entity> handle) {
	TransformComponent* transform = EntityManager::GetTransform(handle);
	if (!transform) {
		return;
	}

	// 全Entityが必ず持つ区画なので、開いた状態で始める
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（Componentが増えたときに名前がぶつからないように）
	ImGui::PushID("Transform");

	ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);

	// 回転は中ではラジアンで持っているので、見せるときだけ度に直す。
	// 変わったときだけ書き戻す（毎フレーム度↔ラジアンを往復させると、値がじわじわずれる）
	Vector3 degrees = {transform->rotation.x * kRadToDeg, transform->rotation.y * kRadToDeg, transform->rotation.z * kRadToDeg};
	if (ImGui::DragFloat3("Rotation", &degrees.x, 0.5f)) {
		transform->rotation = {degrees.x * kDegToRad, degrees.y * kDegToRad, degrees.z * kDegToRad};
	}

	ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform->translation = {0.0f, 0.0f, 0.0f};
		transform->rotation = {0.0f, 0.0f, 0.0f};
		transform->scale = {1.0f, 1.0f, 1.0f};
	}

	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform->worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}


//=============================================================================
// ModelRendererComponent
//=============================================================================
void InspectorWindow::DrawModelRenderer(Handle<Entity> handle) {
	ModelRendererComponent* render = EntityManager::GetModelRenderer(handle);
	if (!render) {
		return; // まだ足していないEntityには区画を出さない
	}

	if (!ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（Transformの区画と項目名がぶつからないように）
	ImGui::PushID("ModelRenderer");

	ImGui::Checkbox("Enabled", &render->enabled);

	// --- どのモデルを描くか ---
	DrawModelPicker(*render);

	// --- 描画設定（enumはmagic_enumが名前を作る）---
	EditorWidgets::EnumCombo("Shading", render->shadingType);
	EditorWidgets::EnumCombo("Blend", render->blendMode);
	EditorWidgets::EnumCombo("Rasterizer", render->rasterizerType);
	EditorWidgets::EnumCombo("Depth", render->depthMode);
	EditorWidgets::EnumCombo("Billboard", render->billboard);

	// --- 色（0xRRGGBBAAで持っているので、編集のときだけ0〜1のfloatに直す）---
	float rgba[4] = {
	    static_cast<float>((render->color >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((render->color >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((render->color >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(render->color & 0xFF) / 255.0f,
	};
	if (ImGui::ColorEdit4("Color", rgba)) {
		render->color = (static_cast<uint32_t>(rgba[0] * 255.0f + 0.5f) << 24) | (static_cast<uint32_t>(rgba[1] * 255.0f + 0.5f) << 16) | (static_cast<uint32_t>(rgba[2] * 255.0f + 0.5f) << 8) |
		                static_cast<uint32_t>(rgba[3] * 255.0f + 0.5f);
	}

	// --- マテリアルの調整（モデルのmtl / glTFの値を上書きする）---
	if (ImGui::TreeNode("Material")) {
		ImGui::ColorEdit3("Ambient", &render->material.ambient.x);
		ImGui::ColorEdit3("Diffuse", &render->material.diffuse.x);
		ImGui::ColorEdit3("Specular", &render->material.specular.x);
		ImGui::ColorEdit3("Emissive", &render->material.emissive.x);
		ImGui::DragFloat("Shininess", &render->material.shininess, 0.5f, 0.0f, 256.0f);
		ImGui::SliderFloat("Metallic", &render->material.metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &render->material.roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha Cutoff", &render->material.alphaCutoff, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	// --- テクスチャのUVをずらす・回す（Vector3の先頭アドレスを渡してx,yだけ触る）---
	if (ImGui::TreeNode("UV Transform")) {
		ImGui::DragFloat2("Offset", &render->uvTransform.translation.x, 0.01f);
		ImGui::DragFloat2("Tiling", &render->uvTransform.scale.x, 0.01f);
		ImGui::DragFloat("Rotate", &render->uvTransform.rotation.z, 0.01f);
		ImGui::TreePop();
	}

	ImGui::PopID();
}


//=============================================================================
// モデルを選ぶコンボ
//=============================================================================
void InspectorWindow::DrawModelPicker(ModelRendererComponent& render) {
	// 一覧は最初に開いたときだけ作る
	if (!modelFilesScanned) {
		ScanModelFiles();
	}

	// 今選ばれているモデルの名前（未選択なら (none)）
	const std::string& currentName = ModelManager::GetModelName(render.modelHandle);
	const char* label = currentName.empty() ? "(none)" : currentName.c_str();

	if (ImGui::BeginCombo("Model", label)) {
		for (const std::string& path : modelFiles) {
			if (ImGui::Selectable(path.c_str())) {
				// 同じパスならキャッシュが返るので、選び直しても読み込み直さない
				render.modelHandle = ModelManager::Load(path);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		ScanModelFiles(); // フォルダにモデルを増やしたとき用
	}
	ImGui::TextDisabled("%zu files / handle=%u", modelFiles.size(), render.modelHandle);
}


//=============================================================================
// Componentを足すボタン
//=============================================================================
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	if (!EntityManager::IsAlive(handle)) {
		return;
	}

	if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f))) {
		ImGui::OpenPopup("addComponent");
	}

	// --- ボタンの真下に、ボタンと同じ幅の縦長のポップアップを出す ---
	// （横に出るサブメニューではなく、カテゴリを押すと下に開く形。Unityの Add Component と同じ向き）
	ImVec2 buttonMin = ImGui::GetItemRectMin();
	ImVec2 buttonMax = ImGui::GetItemRectMax();
	float width = buttonMax.x - buttonMin.x;
	ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y));
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, kAddComponentMaxHeight));
	if (!ImGui::BeginPopup("addComponent")) {
		return;
	}

	// --- 検索欄（開いた瞬間に文字を打てるようにする）---
	static ImGuiTextFilter filter; // ポップアップは同時に1つしか開かないので、関数の中のstaticで持つ
	if (ImGui::IsWindowAppearing()) {
		filter.Clear();
		ImGui::SetKeyboardFocusHere();
	}
	filter.Draw("##search", -FLT_MIN);
	ImGui::Separator();

	// --- カテゴリ（押すと下に開く）→ その中のComponent ---
	for (ComponentCategory category : magic_enum::enum_values<ComponentCategory>()) {
		// このカテゴリに、検索に合う項目が1つでもあるか（無いカテゴリは出さない）
		bool hasItem = false;
		for (const ComponentMenuItem& item : kComponentMenu) {
			if (item.category == category && filter.PassFilter(item.name)) {
				hasItem = true;
				break;
			}
		}
		if (!hasItem) {
			continue;
		}

		// 検索中は、見つかったカテゴリを全部開いて見せる
		if (filter.IsActive()) {
			ImGui::SetNextItemOpen(true);
		}
		std::string categoryName(magic_enum::enum_name(category));
		if (!ImGui::TreeNodeEx(categoryName.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			continue;
		}
		for (const ComponentMenuItem& item : kComponentMenu) {
			if (item.category != category || !filter.PassFilter(item.name)) {
				continue;
			}
			// すでに持っている・追加を予約済みなら、灰色にして押せなくする
			ImGui::BeginDisabled(!item.canAdd(handle));
			if (ImGui::Selectable(item.name)) { // Selectableを押すとポップアップは自動で閉じる
				item.requestAdd(handle);
			}
			ImGui::EndDisabled();
		}
		ImGui::TreePop();
	}

	ImGui::EndPopup();
}