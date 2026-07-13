#define NOMINMAX
#include "MyEngine/Graphics/RenderTarget/ViewportRenderer.h"

#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
ViewportRenderer* ViewportRenderer::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void ViewportRenderer::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new ViewportRenderer();
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void ViewportRenderer::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// ビューポートとシザーの設定
//=============================================================================
void ViewportRenderer::SetViewportAndScissor(float width, float height, float offsetX, float offsetY) {
	// Viewport
	D3D12_VIEWPORT viewport{};
	viewport.TopLeftX = offsetX;
	viewport.TopLeftY = offsetY;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	// Scissor
	D3D12_RECT scissor{};
	scissor.left = static_cast<LONG>(offsetX);
	scissor.top = static_cast<LONG>(offsetY);
	scissor.right = static_cast<LONG>(offsetX + width);
	scissor.bottom = static_cast<LONG>(offsetY + height);
	// Set
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);
}


//=============================================================================
// シーン描画（Debug / Release共通の描画列）
//=============================================================================
void ViewportRenderer::RenderScene(RenderWindow* renderer, const std::wstring& windowTitle, float width, float height) {
	auto* cmdList = DirectXCommon::GetCommandList();
	SetViewportAndScissor(width, height); // Viewport, Scissor の設定
	{
		// メッシュ＋Line（ソート済み）
		GPU_SCOPE(cmdList, "Flush3d");
		RenderQueue::Flush3d(windowTitle);
	}
	{
		// スプライト（呼び出し順）
		GPU_SCOPE(cmdList, "Flush2d");
		RenderQueue::Flush2d(windowTitle, renderer);
	}
	{
		// プロジェクト側から登録した処理
		GPU_SCOPE(cmdList, "PreRenderCallbacks");
		ExecutePreRenderCallbacks();
	}
}


//=============================================================================
// ImGui::Imageで描画
//=============================================================================
void ViewportRenderer::Draw(RenderWindow* renderer, const std::wstring& windowTitle, float windowWidth, float windowHeight) {
	// ===== Debug版 =====
#ifdef USE_IMGUI
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	// --- ゲーム画面を RenderTexture に焼く ---
	RenderTexture::PreDraw();
	RenderScene(renderer, windowTitle, static_cast<float>(RenderTexture::GetWidth()), static_cast<float>(RenderTexture::GetHeight()));
	RenderTexture::PostDraw();

	// --- アスペクト比を維持したサイズを計算する ---
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
	
	// 中央揃えの配置座標を計算する
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
	// 余白を4枚の矩形で塗りつぶす
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 marginCol = IM_COL32(5, 5, 5, 255);
	dl->AddRectFilled({contentMin.x - 15.0f, contentMin.y - 15.0f}, {contentMax.x + 15.0f, imageMin.y}, marginCol); // 上
	dl->AddRectFilled({contentMin.x - 15.0f, imageMax.y}, {contentMax.x + 15.0f, contentMax.y + 15.0f}, marginCol); // 下
	dl->AddRectFilled({contentMin.x - 15.0f, imageMin.y}, {imageMin.x, imageMax.y}, marginCol);                     // 左
	dl->AddRectFilled({imageMax.x, imageMin.y}, {contentMax.x + 15.0f, imageMax.y}, marginCol);                     // 右
	// ゲーム画面を Image として表示する
	ImGui::SetCursorScreenPos(imageMin);
	ImGui::Image((ImTextureID)RenderTexture::GetSRVGPUHandle().ptr, imageSize);
	// Viewport 内のみ入力を受け付ける
	const bool inViewport = ImGui::IsItemHovered();
	InputManager::SetMouseInViewport(inViewport);
	if (inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(true);
	}
	if (!inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(false);
	}

	// ===== Release版 =====
#else
	instance_->gameViewWidth_ = windowWidth;
	instance_->gameViewHeight_ = windowHeight;

	RenderScene(renderer, windowTitle, windowWidth, windowHeight);
#endif
}

//=============================================================================
// コールバック登録 / 解除
//=============================================================================
int ViewportRenderer::AddPreRenderCallback(std::function<void()> callback) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	const int id = instance_->nextCallbackId_++;
	instance_->preRenderCallbacks_.push_back({id, std::move(callback)});
	LogManager::Log(std::format("登録 id={}", id));
	return id;
}

void ViewportRenderer::RemovePreRenderCallback(int id) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	auto& callbacks = instance_->preRenderCallbacks_;
	std::erase_if(instance_->preRenderCallbacks_, [id](const CallbackEntry& e) { return e.id == id; });
	LogManager::Log(std::format("解除 id={}", id));
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