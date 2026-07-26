#define NOMINMAX
#include "MyEngine/Window/WindowManager.h"
#include <format>
#include <externals/imgui/imgui.h>
#include <externals/imgui/ImGuizmo.h>
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Sound/SoundManager.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Editor/EditorOverlay.h"
#include "MyEngine/Editor/ViewportWindow.h"
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"


//=============================================================================
// ウィンドウの追加
//=============================================================================
void WindowManager::AddWindow(const WindowConfig& config, std::unique_ptr<IScene> initialScene) {
	// 最初のウィンドウのみRenderContextを初期化する
	if (windows_.empty()) {
		windows_.reserve(30);
	}

	// ウィンドウの生成
	std::unique_ptr<Win32Window> window = std::make_unique<Win32Window>();
	window->Init(config);
	// ウィンドウに描画するための機能の生成
	std::unique_ptr<RenderWindow> render = std::make_unique<RenderWindow>();
	render->Initialize(window.get());
	// ウィンドウサイズの変更を動的に変更できるようにする
	RenderWindow* renderPtr = render.get();
	window->SetOnResize([renderPtr](int w, int h) { renderPtr->Resize(w, h); });
	// SceneManagerの生成と初期シーンのセット
	std::unique_ptr<SceneManager> sceneManager = std::make_unique<SceneManager>();
	auto editor = std::make_unique<EditorViewport>();
	editor->Initialize();

#ifdef USE_IMGUI
	// ImGuiのウィンドウなら初期化
	if (config.isImGui) {
		ImGuiManager::Initialize(window.get());
		window->SetImGuiTarget(true);
	}
#endif

	// ウィンドウをまとめた構造体に入れる
	windows_.push_back({std::move(window), std::move(render), std::move(sceneManager), std::move(editor)});
	LogManager::Log(std::format("AddWindow: title={} hwnd={}", ConvertString(config.title), (void*)windows_.back().window->GetHWND()));


	if (initialScene) {
		initialScene->SetWindowTitle(config.title);
		initialScene->Initialize();
		windows_.back().sceneManager->SetScene(std::move(initialScene));
	} else {
		LogManager::Log("window.get()->GetTitle()ウィンドウでシーンの指定なし");
	}
}

//=============================================================================
// 全ウィンドウのメッセージ処理
//=============================================================================
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

//=============================================================================
// 全ウィンドウ更新
//=============================================================================
void WindowManager::UpdateAll() {
	HWND focused = GetForegroundWindow();
	for (WindowSet& w : windows_) {
		bool isFocused = (focused == w.window->GetHWND());
		InputManager::SetActiveWindow(isFocused ? w.window->GetHWND() : nullptr);
		// シーンマネージャの基底クラス
		if (w.sceneManager) {
			w.sceneManager->Update();
		}
	}
	InputManager::SetActiveWindow(nullptr); // 入力をリセット
	ParticleManager::Update(); // パーティクルの更新
}


//=============================================================================
//全ウィンドウの描画をコマンド積む
//=============================================================================
void WindowManager::DrawAll() {
	for (WindowSet& w : windows_) {
		// シーン描画リクエスト
		if (w.sceneManager) {
			w.sceneManager->Draw();
		}
	}
	ParticleManager::Draw(); // パーティクルの描画
}


//=============================================================================
// ウィンドウ描画開始
//=============================================================================
void WindowManager::PreRenderAll() {
	// ウィンドウ分描画する
	for (WindowSet& w : windows_) {
		// ImGuiのフレーム開始
#ifdef USE_IMGUI
		if (w.window->GetWindowConfig().isImGui) {
			// 調整項目のImGui
			ImGuiManager::Begin();
		}
#endif

		// ===== 描画開始処理(RenderTargetの切り替えやバリアの設定など) =====
		w.renderer->PreDraw(); // バリア変更 + クリア
		
		// シーンのカメラ取得
		IScene* scene = w.sceneManager->GetCurrentScene();
		Camera* gameCamera = scene ? scene->GetCamera() : nullptr;
		// シーンとエディタの表示
		w.editor->Render(gameCamera, w.renderer.get(), w.window->GetTitle());


#ifdef USE_IMGUI
		if (w.window->GetWindowConfig().isImGui) {
			EditorOverlay::Draw();
		}
#endif
	};

#ifdef USE_IMGUI
	for (WindowSet& w : windows_) {
		if (w.window->GetWindowConfig().isImGui) {
			// ImGuiリクエストを積む
			ImGuiManager::ProcessRequests();
			break;
		}
	}
#endif
}

//=============================================================================
// 全ウィンドウ描画終了
//=============================================================================
void WindowManager::PostRenderAll() {
#ifdef USE_IMGUI
	// isImGuiがtrueのウィンドウだけImGuiを描画
	for (WindowSet& w : windows_) {
		if (w.window->GetWindowConfig().isImGui) {
			GPU_SCOPE(DirectXCommon::GetCommandList(), "ImGui");
			ImGuiManager::Render();
			RenderImGui();
			break;
		}
	}
#endif

	// バリアの入れ替えと共有バッファのオフセット値をリセット
	for (WindowSet& w : windows_) {
		w.renderer->PostDraw();
	}
	
	// コマンドリストの実行
	ExecuteOnly();
	// デバイスが削除されていないかチェック
	DirectXCommon::CheckDeviceRemoved();
	// 全ウィンドウのバッファを入れ替える
	for (WindowSet& w : windows_) {
		w.renderer->GetSwapChain()->Present(1, 0);
	}
	// デバイスが削除されていないかチェック
	DirectXCommon::CheckDeviceRemoved();

#ifdef USE_IMGUI
	for (WindowSet& w : windows_) {
		if (w.window->GetWindowConfig().isImGui) {
			// ImGuiリクエストをクリア
			ImGuiManager::ClearRequests();
			break;
		}
	}
#endif

	// PrimitiveRendererリクエストをクリア
	RenderQueue::Clear();
	// 描画コールインデックスをリセット
	RenderContext::ResetDrawCallIndex();

	// プロジェクト側で追加した後処理を実行
	for (const auto& cb : framEndCallbacks_) {
		cb.func();
	}

	// 全ウィンドウのコマンドリストがコマンドキューに投げられた後、ここで待つ
	DirectXCommon::WaitForGPU();
	// デバイスが削除されていないかチェック
	DirectXCommon::CheckDeviceRemoved();

	// コマンドアロケータ、コマンドリストをリセット
	DirectXCommon::GetCommandAllocator()->Reset();
	DirectXCommon::GetCommandList()->Reset(DirectXCommon::GetCommandAllocator(), nullptr);

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

//=============================================================================
// 積まれたコマンドを実行
//=============================================================================
void WindowManager::ExecuteOnly() {
	// GPUで記録したtickをReadbackBufferへコピー
	GPUProfiler::Resolve(DirectXCommon::GetCommandList());
	// コマンドリストを閉じる
	HRESULT hr = DirectXCommon::GetCommandList()->Close();
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		LogManager::Flush();
		assert(false && "コマンドリストの内容を確定できませんでした");
	}
	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = {DirectXCommon::GetCommandList()};
	DirectXCommon::GetCommandQueue()->ExecuteCommandLists(1, commandLists);
}

//=============================================================================
// 終了処理
//=============================================================================
void WindowManager::Finalize() {
	DirectXCommon::WaitForGPU();
#ifdef USE_IMGUI
	for (WindowSet& w : windows_) {
		if (w.window->GetWindowConfig().isImGui) {
			ImGuiManager::Release();
			break;
		}
	}
#endif
	RenderTextureManager::Finalize();
	RenderContext::Release();
	UploadContext::Release();
	TextureManager::Release();
	ModelManager::Release();
	SoundManager::Release();
	InputManager::Release();
	windows_.clear();
}

//=============================================================================
// ImGui描画
//=============================================================================
#ifdef USE_IMGUI
void WindowManager::RenderImGui() {
	const std::wstring& target = imguiTargetWindow_.empty() ? windows_[0].window->GetTitle() : imguiTargetWindow_;
	for (WindowSet& w : windows_) {
		if (w.window->GetTitle() == target) {
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = w.renderer->GetCurrentRTVHandle();
			DirectXCommon::GetCommandList()->OMSetRenderTargets(1, &rtv, false, nullptr);
			ImGuiManager::RenderDrawData();
			return;
		}
	}
}
#endif

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

//==========================================
// プロジェクト固有終了処理の登録
//==========================================
int WindowManager::AddFrameEndCallback(std::function<void()> callback, const std::wstring& targetTitle) {
	int id = nextCallbackId_;
	nextCallbackId_++;
	framEndCallbacks_.push_back({id, targetTitle, std::move(callback)});
	LogManager::Log(std::format("[WindowManager::AddFrameEndCallback] FrameEndCallback登録 id={} target={}", id, targetTitle.empty() ? "全ウィンドウ" : ConvertString(targetTitle)));
	return id;
}
//==========================================
// プロジェクト固有終了処理の登録解除
//==========================================
void WindowManager::RemoveFrameEndCallback(int id) {
	std::erase_if(framEndCallbacks_, [id](const CallbackEntry& entry) { return entry.id == id; });
	LogManager::Log(std::format("[WindowManager::RemoveFrameEndCallback] FrameEndCallback解除 id={}", id));
}