#ifdef USE_IMGUI
#include "EditorViewport.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Editor/EditorOverlay.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/UI/GlobalVariables.h"

namespace {
constexpr uint32_t kViewWidth = 1280;
constexpr uint32_t kViewHeight = 720;
// 調整項目の置き場所（Parameters → Debug → DebugCamera）
constexpr const char* kGVGroup = "Debug";
constexpr const char* kGVCategory = "DebugCamera";
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

//=============================================================================
// 調整項目の登録
// Resources/Parameters/Debug/DebugCamera.json に保存される
//=============================================================================
void EditorViewport::RegisterGV() {
	GlobalVariables::GetInstance()
	    ->Group(kGVGroup)
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

//=============================================================================
// 保存値をデバッグカメラへ一括反映（起動時に1回）
//=============================================================================
void EditorViewport::ApplyGV() {
	GlobalVariables* gv = GlobalVariables::GetInstance();

	// 投影（アスペクト比はウィンドウ側の値をそのまま使う）
	debugCamera_->Initialize(
	    gv->Get<float>(kGVGroup, kGVCategory, "FovY"), debugCamera_->GetAspectRatio(), gv->Get<float>(kGVGroup, kGVCategory, "NearZ"),
	    gv->Get<float>(kGVGroup, kGVCategory, "FarZ"));

	// 操作感
	debugCamera_->orbitSpeed = gv->Get<float>(kGVGroup, kGVCategory, "OrbitSpeed");
	debugCamera_->panSpeed = gv->Get<float>(kGVGroup, kGVCategory, "PanSpeed");
	debugCamera_->zoomSpeed = gv->Get<float>(kGVGroup, kGVCategory, "ZoomSpeed");
	debugCamera_->mouseDeltaThreshold = gv->Get<float>(kGVGroup, kGVCategory, "MouseDeltaThreshold");

	// 前回終了時の視点
	debugCamera_->SetTranslation(gv->Get<Vector3>(kGVGroup, kGVCategory, "Translation"));
	debugCamera_->SetRotation(gv->Get<Vector3>(kGVGroup, kGVCategory, "Rotation"));
	debugCamera_->ReCalcViewMatrix();

	lastTranslation_ = debugCamera_->GetTranslation();
	lastRotation_ = debugCamera_->GetRotation();
}

//=============================================================================
// 毎フレームの同期
// 位置・回転は「動いた側」を正とするので、ビューポート操作とImGui入力の両方で動かせる
//=============================================================================
void EditorViewport::SyncGV() {
	GlobalVariables* gv = GlobalVariables::GetInstance();

	// --- 操作感は常に GV → カメラ ---
	debugCamera_->orbitSpeed = gv->Get<float>(kGVGroup, kGVCategory, "OrbitSpeed");
	debugCamera_->panSpeed = gv->Get<float>(kGVGroup, kGVCategory, "PanSpeed");
	debugCamera_->zoomSpeed = gv->Get<float>(kGVGroup, kGVCategory, "ZoomSpeed");
	debugCamera_->mouseDeltaThreshold = gv->Get<float>(kGVGroup, kGVCategory, "MouseDeltaThreshold");

	// --- 投影は変化したときだけ作り直す ---
	float fovY = gv->Get<float>(kGVGroup, kGVCategory, "FovY");
	float nearZ = gv->Get<float>(kGVGroup, kGVCategory, "NearZ");
	float farZ = gv->Get<float>(kGVGroup, kGVCategory, "FarZ");
	if (fovY != debugCamera_->GetFovY() || nearZ != debugCamera_->GetNearZ() || farZ != debugCamera_->GetFarZ()) {
		debugCamera_->Initialize(fovY, debugCamera_->GetAspectRatio(), nearZ, farZ);
	}

	// --- 位置・回転 ---
	if (debugCamera_->GetTranslation() != lastTranslation_ || debugCamera_->GetRotation() != lastRotation_) {
		// ビューポート操作で動いた → ImGuiの表示を追従させる（この状態でSaveすれば今の視点が保存される）
		gv->Set<Vector3>(kGVGroup, kGVCategory, "Translation", debugCamera_->GetTranslation());
		gv->Set<Vector3>(kGVGroup, kGVCategory, "Rotation", debugCamera_->GetRotation());
	} else {
		// カメラは動いていない → ImGuiで編集された値をカメラへ反映する
		debugCamera_->SetTranslation(gv->Get<Vector3>(kGVGroup, kGVCategory, "Translation"));
		debugCamera_->SetRotation(gv->Get<Vector3>(kGVGroup, kGVCategory, "Rotation"));
		debugCamera_->ReCalcViewMatrix();
	}
	lastTranslation_ = debugCamera_->GetTranslation();
	lastRotation_ = debugCamera_->GetRotation();
}
#endif
