#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "MyEngine/Scene/Skybox.h"
#include "MyEngine/Graphics/PostEffect/PostProcess.h"

// 前方宣言
class Skybox;
class Camera;
class RenderWindow;
class RenderTexture;


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
	static void InitializePost(uint32_t width, uint32_t height);

	// --- camera視点で targetにシーン1回分を描く ---
	// Textureに描く
	static void RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, bool usePost = true);
	// Windowに描く
	static void RenderToWindow(const Camera* camera, RenderWindow* window, const std::wstring& windowTitle, float width, float height);

	// カメラのリングバッファ参照位置リセット
	static void ResetViewIndex() { 
		instance_->cameraSlot_ = 0;
		instance_->skybox_->ResetSlot();
		if (instance_->postProcess_) {
			instance_->postProcess_->ResetSlot();
		}
	}

	static Skybox* GetSkybox() { return instance_->skybox_.get(); }
	static PostProcess* GetPost() { return instance_->postProcess_.get(); }

private:
	SceneRenderer() = default;
	~SceneRenderer() = default;
	static void SetViewportAndScissor(float width, float height);
	static void RenderInternal(const Camera* camera, const std::wstring& windowTitle, float width, float height);
	static D3D12_GPU_VIRTUAL_ADDRESS UploadCameraCB(const Camera* camera);

	static SceneRenderer* instance_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<RenderTexture> postRT_;
	std::unique_ptr<PostProcess> postProcess_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraCB_;
	uint8_t* cameraCBMapped_ = nullptr;
	uint32_t cameraSlot_ = 0;
};