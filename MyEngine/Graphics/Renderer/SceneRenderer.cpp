#define NOMINMAX
#include "SceneRenderer.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Scene/Skybox.h"

SceneRenderer* SceneRenderer::instance_ = nullptr;
namespace {
static constexpr uint32_t kMaxViewsPerFrame = 4;
static constexpr uint32_t kCameraSlots = 8;
static constexpr uint32_t kCameraSlotSize = 256;
}


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void SceneRenderer::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new SceneRenderer();
	// GPUに送るカメラ構造体をMapして作成
	instance_->cameraCB_ = DirectXCommon::CreateMappedUploadBuffer(kCameraSlotSize * kMaxViewsPerFrame, 
		reinterpret_cast<void**>(&instance_->cameraCBMapped_));
	// 天球生成。初期化はまだ
	instance_->skybox_ = std::make_unique<Skybox>();
	LogManager::Log("Initialized");
}

// ===== 天球 =====
void SceneRenderer::InitializeSkybox() { instance_->skybox_->Initialize(); }

// ===== 解放 =====
void SceneRenderer::Release() {
	delete instance_;
	instance_ = nullptr;
}


//=============================================================================
// Viewport と Scissorの設定
//=============================================================================
void SceneRenderer::SetViewportAndScissor(float width, float height) {
	D3D12_VIEWPORT vp{0.0f, 0.0f, width, height, 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);
}


//=============================================================================
// シーン一括描画
//=============================================================================
void SceneRenderer::Render(const Camera* camera, RenderTexture* target, RenderWindow* rw, const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// 描画先をこのカメラ用RenderTextureに切替＋クリア
	target->PreDraw();
	SetViewportAndScissor(static_cast<float>(target->GetWidth()), static_cast<float>(target->GetHeight()));

	// TODO(view-CBV): ここで camera の viewProj を「パス共通CBV」にバインドする。
	//   現状メッシュはenqueue時にWVPを焼き込み済みなので、この camera では変わらない。
	//   移行後、下の Flush 前に SetGraphicsRootConstantBufferView(cameraCBV) を差すだけ。
	D3D12_GPU_VIRTUAL_ADDRESS camCB = camera ? UploadCameraCB(camera) : 0;

	{
		GPU_SCOPE(cmdList, "Opaque");
		RenderQueue::FlushOpaqueMesh(windowTitle, camCB);
	}
	if (instance_->skybox_) {
		GPU_SCOPE(cmdList, "Skybox");
		instance_->skybox_->Draw(camera);
	}
	{
		GPU_SCOPE(cmdList, "Transparent");
		RenderQueue::FlushTransparentMesh(windowTitle, camCB);
	}
	{
		GPU_SCOPE(cmdList, "Line");
		RenderQueue::FlushLine(windowTitle);
	}
	{
		GPU_SCOPE(cmdList, "Particle");
		RenderQueue::FlushParticle(windowTitle);
	}
	{
		GPU_SCOPE(cmdList, "2D");
		RenderQueue::Flush2d(windowTitle, rw);
	}

	target->PostDraw();
}


//=============================================================================
// GPU転送用のカメラを更新
//=============================================================================
// UploadCameraCB：% をやめて Assert
D3D12_GPU_VIRTUAL_ADDRESS SceneRenderer::UploadCameraCB(const Camera* camera) {
	MY_ASSERT_MSG(instance_->cameraSlot_ < kMaxViewsPerFrame, "1フレームのビュー数が kMaxViewsPerFrame を超えました");
	uint32_t slot = instance_->cameraSlot_++;
	auto* dst = reinterpret_cast<CameraData*>(instance_->cameraCBMapped_ + slot * kCameraSlotSize);
	// 転送情報
	const Matrix4x4& v = camera->GetViewMatrix();
	dst->viewProj = v * camera->GetProjectionMatrix();
	dst->worldPosition = camera->GetTranslation();
	dst->right = {v.m[0][0], v.m[1][0], v.m[2][0]};
	dst->up = {v.m[0][1], v.m[1][1], v.m[2][1]};
	return instance_->cameraCB_->GetGPUVirtualAddress() + slot * kCameraSlotSize;
}