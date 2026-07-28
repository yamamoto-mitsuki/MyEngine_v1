#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

// 前方宣言
class Camera;
class RenderWindow;
class RenderTexture;
class Skybox;


/// <summary> 
/// 1カメラ分のシーンを RenderTextureに描くクラス
/// </summary>
class SceneRenderer {
public:
	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	static void Initialize();
	static void Release();
	static void InitializeSkybox(); 

	// --- camera視点で targetにシーン1回分を描く ---
	// Textureに描く
	static void RenderToTexture(const Camera* camera, RenderTexture* target, RenderWindow* rw, const std::wstring& windowTitle);
	// Windowに描く
	static void RenderToWindow(const Camera* camera, RenderWindow* rw, const std::wstring& windowTitle, float width, float height);

	// カメラのリングバッファ参照位置リセット
	static void ResetViewIndex() { instance_->cameraSlot_ = 0; }

	static Skybox* GetSkybox() { return instance_->skybox_.get(); }

private:
	SceneRenderer() = default;
	~SceneRenderer() = default;
	static void SetViewportAndScissor(float width, float height);
	static void RenderInternal(const Camera* camera, RenderWindow* rw, const std::wstring& windowTitle, float width, float height);
	static D3D12_GPU_VIRTUAL_ADDRESS UploadCameraCB(const Camera* camera);

	static SceneRenderer* instance_;
	std::unique_ptr<Skybox> skybox_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraCB_;
	uint8_t* cameraCBMapped_ = nullptr;
	uint32_t cameraSlot_ = 0;
};