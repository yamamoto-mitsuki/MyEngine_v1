#ifdef USE_IMGUI
#include "EditorViewport.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Editor/EditorOverlay.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Input/InputManager.h"

namespace {
constexpr uint32_t kViewWidth = 1280;
constexpr uint32_t kViewHeight = 720;
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
	debugCamera_->Initialize(0.45f, aspectRatio, 0.1f, 100.0f);
	debugCamera_->SetTranslation({0.0f, 0.0f, -30.0f});
}

//=============================================================================
// 更新
//=============================================================================
void EditorViewport::Update() {
	// Sceneビューにあるときだけ操作
	if (sceneView_.IsHovered()) {
		debugCamera_->Update();
	}
}

//=============================================================================
// 描画（1ウィンドウ分の Game / Scene 2画面）
//=============================================================================
void EditorViewport::Render(const Camera* gameCamera, RenderWindow* rw, const std::wstring& windowTitle) {
	RenderTexture* gameRT = RenderTextureManager::Get(gameRT_);
	RenderTexture* sceneRT = RenderTextureManager::Get(sceneRT_);

	// --- 同じキューを2カメラで2回描く ---
	SceneRenderer::RenderToTexture(gameCamera, gameRT, rw, windowTitle); // "Game"
	SceneRenderer::RenderToTexture(debugCamera_.get(), sceneRT, rw, windowTitle); // "Scene"

	gameView_.Draw(gameRT); // Gameはギズモ無し
	// ギズモあり
	sceneView_.Draw(sceneRT, [this] {
		EditorOverlay::SetActiveCamera(debugCamera_.get());
		EditorOverlay::Draw(sceneView_.GetImageMin(), sceneView_.GetImageMax(), sceneView_.GetImageSize());
	});

	// どちらかのビュー上なら入力をImGuiに食わせない
	InputManager::SetMouseInViewport(gameView_.IsHovered() || sceneView_.IsHovered());
}
#endif