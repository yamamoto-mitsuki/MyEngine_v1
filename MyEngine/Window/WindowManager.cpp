#include "MyEngine/Window/WindowManager.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Render/DebugRender.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Utils/ConvertString.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Render/ModelManager.h"
#include "MyEngine/Audio/SoundManager.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Scene/IScene.h"
#include <cassert>
#include <format>

void WindowManager::AddWindow(const WindowConfig& config, DirectXCommon* dxCommon, std::unique_ptr<IScene> initialScene) {
	// dxCommon_を初期化
	dxCommon_ = dxCommon;
	// 最初のウィンドウのみRenderContextを初期化する
	if (windows_.empty()) {
		RenderContext::Init(dxCommon_);
		windows_.reserve(30);
	}

	// ウィンドウの生成
	std::unique_ptr<Win32Window> window = std::make_unique<Win32Window>();
	window->Init(config);
	// ウィンドウに描画するための機能の生成
	std::unique_ptr<RenderWindow> render = std::make_unique<RenderWindow>();
	render->Init(window.get(), dxCommon);
	// ウィンドウサイズの変更を動的に変更できるようにする
	RenderWindow* renderPtr = render.get();
	window->SetOnResize([renderPtr](int w, int h) { renderPtr->Resize(w, h); });
	// SceneManagerの生成と初期シーンのセット
	std::unique_ptr<SceneManager> sceneManager = std::make_unique<SceneManager>();
	// ウィンドウをまとめた構造体に入れる
	windows_.push_back({std::move(window), std::move(render), std::move(sceneManager)});
	LogManager::Log(std::format("AddWindow: title={} hwnd={}", ConvertString(config.title), (void*)windows_.back().window->GetHWND()));
#ifdef USE_IMGUI
	if (config.isImGui) {
		SetImGuiTargetWindow(config.title);
	}
#endif

	if (initialScene) {
		initialScene->SetWindowTitle(config.title);
		initialScene->Initialize();
		windows_.back().sceneManager->SetScene(std::move(initialScene));
	} else {
		LogManager::Log("window.get()->GetTitle()ウィンドウでシーンの指定なし");
	}
}

bool WindowManager::ProcessMessage() {
	for (std::vector<WindowSet>::iterator it = windows_.begin(); it != windows_.end();) {
		if (!it->window->ProcessMessage()) {
			// ウィンドウが閉じられたら削除
			it = windows_.erase(it);
			continue;
		}
		++it;
	}

	// 全ウィンドウのが閉じたら終了
	return !windows_.empty();
}

void WindowManager::UpdateAll() {
	HWND focused = GetForegroundWindow();
	for (WindowSet& w : windows_) {
		bool isFocused = (focused == w.window->GetHWND());
		InputManager::SetActiveWindow(isFocused ? w.window->GetHWND() : nullptr);
		if (w.sceneManager) {
			w.sceneManager->Update();
		}
	}
	InputManager::SetActiveWindow(nullptr);
}

void WindowManager::DrawAll() {
	for (WindowSet& w : windows_) {
		// シーン描画リクエスト
		if (w.sceneManager) {
			w.sceneManager->Draw();
		}
	}
}

void WindowManager::PreRenderAll() {
	// ImGuiのフレーム開始
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Begin();
#endif

	// ウィンドウ分描画する
	for (WindowSet& w : windows_) {
		// ===== 描画開始処理(RenderTargetの切り替えやバリアの設定など) =====
		w.renderer->PreDraw();

		// --- ウィンドウ全体のサイズを取得 ---
		RECT clientRect{};
		GetClientRect(w.window->GetHWND(), &clientRect);
		float totalWidth  = static_cast<float>(clientRect.right  - clientRect.left);
		float totalHeight = static_cast<float>(clientRect.bottom - clientRect.top);

		// --- ゲーム画面のビューポート・シザーを計算 ---
		const WindowConfig& cfg = w.window->GetWindowConfig();
		float startX, startY, gameWidth, gameHeight;

		if (cfg.isImGui) {
			// ゲーム画面領域を計算
			std::tie(startX, startY, gameWidth, gameHeight) =
				CalcGameViewRect(cfg, totalWidth, totalHeight);
		} else {
			// ImGuiなし: ウィンドウ全体をゲーム画面として使う
			startX     = 0.0f;
			startY     = 0.0f;
			gameWidth  = totalWidth;
			gameHeight = totalHeight;
		}

		// --- ゲーム画面のビューポート・シザーをセット ---
		RenderContext::SetViewportAndScissor(gameWidth, gameHeight, startX, startY);

		// --- プロジェクト側で登録したものを実行 ---
		for (const auto& cb : preRenderCallbacks_) {
			cb(w.window->GetTitle(), dxCommon_);
		}

		// --- 3Dをまとめて描画 ---
		RenderContext::StartDrawModel();
		DebugRender::Flush3d(w.window->GetTitle(), &RenderContext::GetInstance());
		ModelManager::Flush3d(w.window->GetTitle(), &RenderContext::GetInstance());

		// --- 2Dをまとめて描画 ---
		RenderContext::StartDrawSprite();
		DebugRender::Flush2d(w.window->GetTitle(), &RenderContext::GetInstance(), w.renderer.get());
	}

#ifdef USE_IMGUI
	// ImGuiリクエストを積む
	ImGuiManager::GetInstance()->ProcessRequests();
#endif
}

void WindowManager::PostRenderAll() {
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Render();
	RenderImGui();
#endif

	// バリアの入れ替えと共有バッファのオフセット値をリセット
	for (WindowSet& w : windows_) {
		w.renderer->PostDraw();
	}
	// コマンドリストの実行
	ExecuteOnly();
	// 全ウィンドウのバッファを入れ替える
	for (WindowSet& w : windows_) {
		w.renderer->GetSwapChain()->Present(1, 0);
	}
	
#ifdef USE_IMGUI
	// ImGuiリクエストをクリア
	ImGuiManager::ClearRequests();
#endif

	// DebugRenderリクエストをクリア
	DebugRender::ClearRequests(); 
	// ModelManagerリクエストをクリア
	ModelManager::ClearRequests();
	// 描画コールインデックスをリセット
	RenderContext::ResetDrawCallIndex();
	
	// プロジェクト側で追加した後処理を実行
	for (const auto& cb : postRenderCallbacks_) {
		cb();
	}

	// 全ウィンドウのコマンドリストがコマンドキューに投げられた後、ここで待つ
	dxCommon_->WaitForGPU();
	// コマンドアロケータ、コマンドリストをリセット
	dxCommon_->GetCommandAllocator()->Reset();
	dxCommon_->GetCommandList()->Reset(dxCommon_->GetCommandAllocator(), nullptr);

	// ウィンドウのサイズ処理
	for (WindowSet& w : windows_) {
		if (w.window->GetPendingResize()) {
			int width = w.window->GetPendingWidth();
			int height = w.window->GetPendingHeight();
			w.window->ClearPendingResize();
			w.renderer->Resize(width, height, false);
		}
	}
}

void WindowManager::ExecuteOnly() {
	// コマンドリストを閉じる
	HRESULT hr = dxCommon_->GetCommandList()->Close();
	if (FAILED(hr)) {
		LogManager::Log(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		assert(false && "コマンドリストの内容を確定できませんでした");
	}
	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = {dxCommon_->GetCommandList()};
	dxCommon_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);
}

void WindowManager::Finalize() {
	dxCommon_->WaitForGPU();
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Finalize();
#endif
	RenderContext::Release();
	TextureManager::Release();
	ModelManager::Release();
	SoundManager::Finalize();
	InputManager::Finalize();
	windows_.clear();
}

#ifdef USE_IMGUI
void WindowManager::RenderImGui() {
	const std::wstring& target = imguiTargetWindow_.empty() ? windows_[0].window->GetTitle() : imguiTargetWindow_;
	for (WindowSet& w : windows_) {
		if (w.window->GetTitle() == target) {
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = w.renderer->GetCurrentRTVHandle();
			dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtv, false, nullptr);
			ImGuiManager::GetInstance()->RenderDrawData();
			return;
		}
	}
}
#endif

#ifdef USE_IMGUI
std::wstring WindowManager::GetImGuiTargetWindow() const {
	if (imguiTargetWindow_.empty()) {
		return windows_[0].window->GetTitle();
	}
	return imguiTargetWindow_;
}
#endif

#ifdef USE_IMGUI
void WindowManager::SetImGuiTargetWindow(const std::wstring& windowTitle) {
	imguiTargetWindow_ = windowTitle;
	for (WindowSet& w : windows_) {
		if (w.window->GetTitle() == windowTitle) {
			w.window->SetImGuiTarget(true);
			ImGuiManager::GetInstance()->Init(w.window.get(), dxCommon_, w.window->GetWindowConfig());
			return;
		}
	}
}
#endif

std::tuple<float, float, float, float> WindowManager::CalcGameViewRect(const WindowConfig& cfg, float totalWidth, float totalHeight) {
	float startX = totalWidth  * cfg.gameViewStart.x;
	float startY = totalHeight * cfg.gameViewStart.y;
	float gameWidth, gameHeight;

	if (cfg.gameViewEnd.y < 0.0f) {
		// gameViewEndXを指定 → gameViewEndYをaspectRatioから自動計算
		float endX  = totalWidth * cfg.gameViewEnd.x;
		gameWidth   = endX - startX;
		gameHeight  = gameWidth / cfg.gameAspectRatio;
	} else {
		// gameViewEndYを指定 → gameViewEndXをaspectRatioから自動計算
		float endY  = totalHeight * cfg.gameViewEnd.y;
		gameHeight  = endY - startY;
		gameWidth   = gameHeight * cfg.gameAspectRatio;
	}

	return {startX, startY, gameWidth, gameHeight};
}

//==========================================
// ゲッター
//==========================================
Win32Window* WindowManager::GetWindowByTitle(const std::wstring& title) {
	for (WindowSet& w : windows_) {
		if (w.window->GetTitle() == title) {
			return w.window.get();
		}
	}
	return nullptr;
}

IScene* WindowManager::GetSceneByTitle(const std::wstring& title) {
	for (WindowSet& w : windows_) {
		if (w.window->GetTitle() == title) {
			return w.sceneManager->GetCurrentScene();
		}
	}
	return nullptr;
}

void WindowManager::AddPreEnderCallback(std::function<void(const std::wstring&, DirectXCommon*)> callback) { preRenderCallbacks_.push_back(std::move(callback)); }
void WindowManager::AddPostRenderCallback(std::function<void()> callback) { postRenderCallbacks_.push_back(std::move(callback)); }