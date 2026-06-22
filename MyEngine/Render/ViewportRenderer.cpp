#define NOMINMAX
#include "MyEngine/Render/ViewportRenderer.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Render/PrimitiveRenderer.h"
#include "MyEngine/Render/ModelManager.h"
#include "MyEngine/Render/Core/RenderContext.h"
#include "MyEngine/Render/Core/RenderTexture.h"
#include "MyEngine/Render/Core/RenderWindow.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Log/LogManager.h"
#include <algorithm>
#include <format>

ViewportRenderer* ViewportRenderer::instance_ = nullptr;

//=============================================================================
// 初期化
//=============================================================================
void ViewportRenderer::Initialize() {
	MY_ASSERT(instance_ == nullptr && "[ViewportRenderer::Initialize] Initialize()が2回以上呼ばれています");
	instance_ = new ViewportRenderer();
	LogManager::Log("[ViewportRenderer::Initialize] Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void ViewportRenderer::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("[ViewportRenderer::Release] Released");
}

//=============================================================================
// ImGui::Imageで描画
//=============================================================================
void ViewportRenderer::Draw(RenderWindow* renderer, const std::wstring& windowTitle, float windowWidth, float windowHeight) {
#ifdef USE_IMGUI
	MY_ASSERT(instance_ && "[ViewportRenderer::Draw] Initialize()を先に呼んでください");
#endif
	// Debug版
#ifdef USE_IMGUI
	// ===== ゲーム画面を RenderTexture に焼く =====
	RenderTexture::PreDraw();
	RenderContext::SetViewportAndScissor(static_cast<float>(RenderTexture::GetWidth()), static_cast<float>(RenderTexture::GetHeight()));
	RenderContext::StartDrawModel();
	DebugRender::Flush3d(windowTitle);
	ModelManager::Flush3d(windowTitle);
	RenderContext::StartDrawSprite();
	DebugRender::Flush2d(windowTitle, renderer);
	ExecutePreRenderCallbacks();
	RenderTexture::PostDraw();

	// ===== アスペクト比を維持したサイズを計算する =====
	ImVec2 available = ImGui::GetContentRegionAvail();
	available.x = std::max(available.x, 1.0f);
	available.y = std::max(available.y, 1.0f);

	const float aspect = static_cast<float>(RenderTexture::GetWidth()) / static_cast<float>(RenderTexture::GetHeight());

	ImVec2 imageSize;
	if (available.x / available.y > aspect) {
		imageSize.y = available.y;
		imageSize.x = available.y * aspect;
	} else {
		imageSize.x = available.x;
		imageSize.y = available.x / aspect;
	}
	
	// ===== 中央揃えの配置座標を計算する =====
	ImVec2 contentMin = ImGui::GetCursorScreenPos();
	ImVec2 contentMax = {contentMin.x + available.x, contentMin.y + available.y};

	const float offsetX = (available.x - imageSize.x) * 0.5f;
	const float offsetY = (available.y - imageSize.y) * 0.5f;
	ImVec2 imageMin = {contentMin.x + offsetX, contentMin.y + offsetY};
	ImVec2 imageMax = {imageMin.x + imageSize.x, imageMin.y + imageSize.y};

	// EditorOverlay・InputManagerが使えるようにキャッシュ
	instance_->gameViewWidth_ = imageSize.x;
	instance_->gameViewHeight_ = imageSize.y;
	instance_->imageMin_ = imageMin;
	instance_->imageMax_ = imageMax;
	instance_->imageSize_ = imageSize;

	// ===== 余白を4枚の矩形で塗りつぶす =====
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 marginCol = IM_COL32(5, 5, 5, 255);
	dl->AddRectFilled({contentMin.x - 15.0f, contentMin.y - 15.0f}, {contentMax.x + 15.0f, imageMin.y}, marginCol); // 上
	dl->AddRectFilled({contentMin.x - 15.0f, imageMax.y}, {contentMax.x + 15.0f, contentMax.y + 15.0f}, marginCol); // 下
	dl->AddRectFilled({contentMin.x - 15.0f, imageMin.y}, {imageMin.x, imageMax.y}, marginCol);                     // 左
	dl->AddRectFilled({imageMax.x, imageMin.y}, {contentMax.x + 15.0f, imageMax.y}, marginCol);                     // 右

	// ===== ゲーム画面を Image として表示する =====
	ImGui::SetCursorScreenPos(imageMin);
	ImGui::Image((ImTextureID)RenderTexture::GetSRVGPUHandle().ptr, imageSize);

	// ===== Viewport 内のみ入力を受け付ける =====
	const bool inViewport = ImGui::IsItemHovered();
	InputManager::SetMouseInViewport(inViewport);
	if (inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(true);
	}
	if (!inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(false);
	}

	// Release版
#else
	instance_->gameViewWidth_ = windowWidth;
	instance_->gameViewHeight_ = windowHeight;

	RenderContext::SetViewportAndScissor(windowWidth, windowHeight);
	RenderContext::StartDrawModel();
	DebugRender::Flush3d(windowTitle);
	ModelManager::Flush3d(windowTitle);
	RenderContext::StartDrawSprite();
	DebugRender::Flush2d(windowTitle, renderer);
	ExecutePreRenderCallbacks();
#endif
}

//=============================================================================
// コールバック登録 / 解除
//=============================================================================
int ViewportRenderer::AddPreRenderCallback(std::function<void()> callback) {
	MY_ASSERT(instance_ && "[ViewportRenderer::AddPreRenderCallback] Initialize()を先に呼んでください");
	const int id = instance_->nextCallbackId_++;
	instance_->preRenderCallbacks_.push_back({id, std::move(callback)});
	LogManager::Log(std::format("[ViewportRenderer::AddPreRenderCallback] 登録 id={}", id));
	return id;
}

void ViewportRenderer::RemovePreRenderCallback(int id) {
	MY_ASSERT(instance_ && "[ViewportRenderer::RemovePreRenderCallback] Initialize()を先に呼んでください");
	auto& callbacks = instance_->preRenderCallbacks_;
	std::erase_if(instance_->preRenderCallbacks_, [id](const CallbackEntry& e) { return e.id == id; });
	LogManager::Log(std::format("[ViewportRenderer::RemovePreRenderCallback] 解除 id={}", id));
}

//=============================================================================
// 内部ヘルパー
//=============================================================================
// ===== コールバック実行 =====
void ViewportRenderer::ExecutePreRenderCallbacks() {
	for (const auto& entry : instance_->preRenderCallbacks_) {
		entry.func();
	}
}

//=============================================================================
// セッター / ゲッター
//=============================================================================
float ViewportRenderer::GetGameViewWidth() { return instance_ ? instance_->gameViewWidth_ : 0.0f; }
float ViewportRenderer::GetGameViewHeight() { return instance_ ? instance_->gameViewHeight_ : 0.0f; }
#ifdef USE_IMGUI
const ImVec2& ViewportRenderer::GetImageMin() { return instance_ ? instance_->imageMin_ : ImVec2{}; }
const ImVec2& ViewportRenderer::GetImageMax() { return instance_ ? instance_->imageMax_ : ImVec2{}; }
const ImVec2& ViewportRenderer::GetImageSize() { return instance_ ? instance_->imageSize_ : ImVec2{}; }
#endif