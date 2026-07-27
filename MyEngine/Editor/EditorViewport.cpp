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
// 描画（1ウィンドウ分の Game / Scene 2画面）
//=============================================================================
void EditorViewport::Render(const Camera* gameCamera, RenderWindow* rw, const std::wstring& windowTitle) {
	// Sceneビューにフォーカス中だけデバッグカメラを操作
	if (sceneView_.IsHovered()) {
		debugCamera_->Update();
	}

	RenderTexture* gameRT = RenderTextureManager::Get(gameRT_);
	RenderTexture* sceneRT = RenderTextureManager::Get(sceneRT_);

	// 同じキューを2カメラで2回描く（RenderQueue::Clear はフレーム末なのでOK）
	SceneRenderer::Render(gameCamera, gameRT, rw, windowTitle);
	SceneRenderer::Render(debugCamera_.get(), sceneRT, rw, windowTitle);

	// ImGuiパネルに表示
	gameView_.Draw(gameRT);
	sceneView_.Draw(sceneRT);

	// ギズモは Scene ビュー（デバッグカメラ）に出す
	EditorOverlay::SetActiveCamera(debugCamera_.get());
	gameView_.Draw(gameRT); // Gameはギズモ無し
	sceneView_.Draw(sceneRT, [this] {
		EditorOverlay::SetActiveCamera(debugCamera_.get());
		EditorOverlay::Draw(sceneView_.GetImageMin(), sceneView_.GetImageMax(), sceneView_.GetImageSize());
	});

	// 入力は Game ビュー基準
	InputManager::SetMouseInViewport(gameView_.IsHovered());
}