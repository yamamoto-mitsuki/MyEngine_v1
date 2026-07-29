#ifdef USE_IMGUI
#include "EditorViewport.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Editor/EditorOverlay.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/UI/GlobalVariables.h"

namespace {
constexpr uint32_t kViewWidth = 1280; // アスペクト比: 横
constexpr uint32_t kViewHeight = 720; // アスペクト比: 縦
constexpr const char* kGVGroup = "Debug"; // ImGui登録グループ名
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void EditorViewport::Initialize(float aspectRatio) {
	// Game / Scene それぞれの描画先（SDR・深度あり）
	gameRT_ = RenderTextureManager::Create(kViewWidth, kViewHeight, RenderTextureFormat::SDR);
	sceneRT_ = RenderTextureManager::Create(kViewWidth, kViewHeight, RenderTextureFormat::SDR);
	// デバッグカメラ（自由視点）
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(0.45f, aspectRatio, 0.1f, 500.0f);
	debugCamera_->SetTranslation({0.0f, 0.0f, -30.0f});

	// 現在値を初期値として登録 → 保存済みjsonがあれば読み込んでカメラへ反映
	RegisterGV();
	GlobalVariables::GetInstance()->LoadGroup(kGVGroup);
	ApplyGV();
}


//=============================================================================
// 更新
//=============================================================================
void EditorViewport::Update() {
	// Sceneビューにあるときだけ操作
	if (sceneView_.IsHovered()) {
		debugCamera_->Update();
	}
	// ImGuiの調整項目と同期
	SyncGV();
}

//=============================================================================
// グリッドなどEditorのモデルなどの描画リクエストを送る
//=============================================================================
void EditorViewport::Draw() {
	if (isShowGrid_) {
		grid_->Draw();
	}
}

//=============================================================================
// 描画（1ウィンドウ分の Game / Scene 2画面）
//=============================================================================
void EditorViewport::Render(const Camera* gameCamera, RenderWindow* rw, const std::wstring& windowTitle) {
	RenderTexture* gameRT = RenderTextureManager::Get(gameRT_);
	RenderTexture* sceneRT = RenderTextureManager::Get(sceneRT_);

	// --- 同じキューを2カメラで2回描く ---
	SceneRenderer::RenderToTexture(gameCamera, gameRT, rw, windowTitle);          // "Game"
	SceneRenderer::RenderToTexture(debugCamera_.get(), sceneRT, rw, windowTitle); // "Scene"

	// --- Game ---
	gameView_.Draw(gameRT);

	// --- Scene ---
	sceneView_.Draw(sceneRT, [this] {
		// Editor
		EditorOverlay::SetActiveCamera(debugCamera_.get());
		EditorOverlay::Draw(sceneView_.GetImageMin(), sceneView_.GetImageMax(), sceneView_.GetImageSize());
	});

	// どちらかのビュー上なら入力をImGuiに食わせない
	InputManager::SetMouseInViewport(gameView_.IsHovered() || sceneView_.IsHovered());
}
#endif


//=============================================================================
// 調整項目
//=============================================================================
void EditorViewport::RegisterGV() {
	GlobalVariables::GetInstance()
	    ->(kGVGroup)
	    .Category(kGVCategory)
	    .Add<Vector3>("Translation", debugCamera_->GetTranslation())
	    .Add<Vector3>("Rotation", debugCamera_->GetRotation())
	    .Add<float>("FovY", debugCamera_->GetFovY())
	    .Add<float>("NearZ", debugCamera_->GetNearZ())
	    .Add<float>("FarZ", debugCamera_->GetFarZ())
	    .Add<float>("OrbitSpeed", debugCamera_->orbitSpeed)
	    .Add<float>("PanSpeed", debugCamera_->panSpeed)
	    .Add<float>("ZoomSpeed", debugCamera_->zoomSpeed)
	    .Add<float>("MouseDeltaThreshold", debugCamera_->mouseDeltaThreshold);
}