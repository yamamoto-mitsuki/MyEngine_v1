#pragma once
#include <memory>
#include <string>

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

	// camera視点で targetにシーン1回分を描く
	static void Render(const Camera* camera, RenderTexture* target, RenderWindow* rw, const std::wstring& windowTitle);

	static Skybox* GetSkybox() { return instance_->skybox_.get(); }

private:
	SceneRenderer() = default;
	~SceneRenderer() = default;
	static void SetViewportAndScissor(float width, float height);

	static SceneRenderer* instance_;
	std::unique_ptr<Skybox> skybox_;
};