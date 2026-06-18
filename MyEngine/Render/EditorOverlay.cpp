#define NOMINMAX
#ifdef USE_IMGUI
#include "MyEngine/Render/EditorOverlay.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Render/ViewportRenderer.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Log/LogManager.h"
#include <cstring>

EditorOverlay* EditorOverlay::instance_ = nullptr;

//=============================================================================
// 初期化
//=============================================================================
void EditorOverlay::Initialize() {
	MY_ASSERT(instance_ == nullptr && "[EditorOverlay::Initialize] Initialize()が2回以上呼ばれています");
	instance_ = new EditorOverlay();
	LogManager::Log("[EditorOverlay::Initialize] Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void EditorOverlay::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("[EditorOverlay::Release] Released");
}

//=============================================================================
// オーバーレイを描画
//=============================================================================
void EditorOverlay::Draw() {
	MY_ASSERT(instance_ && "[EditorOverlay::Draw] Initialize()を先に呼んでください");

	const Camera* cam = GetActiveCamera();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 imageMin = ViewportRenderer::GetImageMin();
	ImVec2 imageMax = ViewportRenderer::GetImageMax();
	ImVec2 imageSize = ViewportRenderer::GetImageSize();

	DrawAxisGizmo(dl, cam, imageMin, imageMax, imageSize);

	// 軸表示
	if (instance_->isShowAxis_ && cam) {
		DrawAxisGizmo(dl, cam, imageMin, imageMax, imageSize);
	}
}

//=============================================================================
// DrawSettings
// 任意の ImGui ウィンドウ内から呼ぶ ON/OFF コントロール
//=============================================================================
void EditorOverlay::DrawSettings() {
	MY_ASSERT(instance_ && "[EditorOverlay::DrawSettings] Initialize()を先に呼んでください");
	ImGui::SeparatorText("Editor Overlay");
	ImGui::Checkbox("Axis Gizmo", &instance_->isShowAxis_);
}

//=============================================================================
// 軸表示
//=============================================================================
void EditorOverlay::DrawAxisGizmo(ImDrawList* dl, const Camera* cam, ImVec2 imageMin, ImVec2 imageMax, ImVec2 imageSize) {
	MY_ASSERT(cam && "[EditorOverlay::DrawAxisGizmo] EditorOverlay::SetActiveCameraを呼び出してください。");
	
	ImGuizmo::BeginFrame();

	// 描画先を Viewport ウィンドウの DrawList に固定する
	ImGuizmo::SetDrawlist(dl);

	ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);
	// 列優先の行列に変換
	auto toColMajor = [](const Matrix4x4& m, float out[16]) {
		for (int r = 0; r < 4; ++r) {
			for (int c = 0; c < 4; ++c) {
				out[c * 4 + r] = m.m[r][c];
			}
		}
	};

	float viewF[16];
	float projF[16];
	toColMajor(cam->GetViewMatrix(), viewF);
	toColMajor(cam->GetProjectionMatrix(), projF);

	float viewCopy[16];
	std::memcpy(viewCopy, viewF, sizeof(viewF));

	// 大きさ、位置
	const float gizmoSizePx = std::max(60.0f, imageSize.x * 0.08f);
	const ImVec2 gizmoSize = {gizmoSizePx, gizmoSizePx};
	const ImVec2 gizmoPos = {imageMax.x - gizmoSizePx, imageMin.y};

	float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

	ImGuizmo::ViewManipulate(
	    viewCopy, 
		projF, 
		ImGuizmo::ROTATE,
		ImGuizmo::LOCAL,
	    identity, 
	    8.0f, gizmoPos, gizmoSize, 
		0x10101080);
}

#endif