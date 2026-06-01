#define NOMINMAX
#include "MyEngine/Window/WindowManager.h"
#include "MyEngine/Audio/SoundManager.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Render/DebugRender.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Render/ModelManager.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Render/RenderTexture.h"
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Utils/ConvertString.h"
#include "externals/imgui/imgui.h"
#include <cassert>
#include <format>

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

#ifdef USE_IMGUI
	// ImGuiのウィンドウなら初期化
	if (config.isImGui) {
		ImGuiManager::Initialize(window.get());
		window->SetImGuiTarget(true);
	}
#endif

	// ウィンドウをまとめた構造体に入れる
	windows_.push_back({std::move(window), std::move(render), std::move(sceneManager)});
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
		if (w.sceneManager) {
			w.sceneManager->Update();
		}
	}
	InputManager::SetActiveWindow(nullptr);
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
			ImGuiManager::Begin();
			DrawGameToRenderTexture(w);
		}
#endif

		// ===== 描画開始処理(RenderTargetの切り替えやバリアの設定など) =====
		w.renderer->PreDraw();

		if (!w.window->GetWindowConfig().isImGui) {
			// Release版
			RECT clientRect{};
			GetClientRect(w.window->GetHWND(), &clientRect);
			float totalWidth = static_cast<float>(clientRect.right - clientRect.left);
			float totalHeight = static_cast<float>(clientRect.bottom - clientRect.top);
			gameViewWidth_ = totalWidth;
			gameViewHeight_ = totalHeight;
			// ビューポートをウィンドウ全体にセット
			RenderContext::SetViewportAndScissor(totalWidth, totalHeight);
			RenderContext::StartDrawModel();
			DebugRender::Flush3d(w.window->GetTitle());
			ModelManager::Flush3d(w.window->GetTitle());
			RenderContext::StartDrawSprite();
			DebugRender::Flush2d(w.window->GetTitle(), w.renderer.get());
			for (const auto& entry : preRenderCallbacks_) {
				if (entry.targetWindowTitle.empty() || entry.targetWindowTitle == w.window->GetTitle()) {
					entry.func();
				}
			}
		}
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
	// 全ウィンドウのバッファを入れ替える
	for (WindowSet& w : windows_) {
		w.renderer->GetSwapChain()->Present(1, 0);
	}

#ifdef USE_IMGUI
	for (WindowSet& w : windows_) {
		if (w.window->GetWindowConfig().isImGui) {
			// ImGuiリクエストをクリア
			ImGuiManager::ClearRequests();
			break;
		}
	}
#endif

	// DebugRenderリクエストをクリア
	DebugRender::ClearRequests();
	// ModelManagerリクエストをクリア
	ModelManager::ClearRequests();
	// 描画コールインデックスをリセット
	RenderContext::ResetDrawCallIndex();

	// プロジェクト側で追加した後処理を実行
	for (const auto& cb : postRenderCallbacks_) {
		cb.func();
	}

	// 全ウィンドウのコマンドリストがコマンドキューに投げられた後、ここで待つ
	DirectXCommon::WaitForGPU();
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
// ゲーム描画をRenderTextureに向ける
//=============================================================================
#ifdef USE_IMGUI
void WindowManager::DrawGameToRenderTexture(WindowSet& w) {
	// ===== 描画コマンドを積む =====
	RenderTexture::PreDraw();
	// 指定したサイズで描画
	RenderContext::SetViewportAndScissor(static_cast<float>(RenderTexture::GetWidth()), static_cast<float>(RenderTexture::GetHeight()));
	RenderContext::StartDrawModel();
	DebugRender::Flush3d(w.window->GetTitle());
	ModelManager::Flush3d(w.window->GetTitle());
	RenderContext::StartDrawSprite();
	DebugRender::Flush2d(w.window->GetTitle(), w.renderer.get());
	// プロジェクト側で登録したもの
	for (const auto& entry : preRenderCallbacks_) {
		if (entry.targetWindowTitle.empty() || entry.targetWindowTitle == w.window->GetTitle()) {
			entry.func();
		}
	}
	RenderTexture::PostDraw();
	

	// ===== ViewportウィンドウにRenderTextureを表示する =====
	// ViewportウィンドウだけWindowBgの色をクリアカラーにそろえる
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(RenderWindow::kClearColor[0], RenderWindow::kClearColor[1], 
						  RenderWindow::kClearColor[2], RenderWindow::kClearColor[3]));
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleColor();
	// Viewportウィンドウの使える領域を取得する
	ImVec2 available = ImGui::GetContentRegionAvail();
	// 0以下になるとアスペクト計算で除算エラーになるのでガードする
	available.x = std::max(available.x, 1.0f);
	available.y = std::max(available.y, 1.0f);
	// ウィンドウの左上を取得
	ImVec2 contentMin = ImGui::GetCursorScreenPos();
	ImVec2 contentMax = ImVec2(contentMin.x + available.x, contentMin.y + available.y);

	// ===== アスペクト比を維持したImGui::Image用サイズを計算する =====
	float aspect = static_cast<float>(RenderTexture::GetWidth()) / static_cast<float>(RenderTexture::GetHeight());
	ImVec2 imageSize;
	if (available.x / available.y > aspect) {
		// 横が余っている → 高さ基準でサイズを決める
		imageSize.y = available.y;
		imageSize.x = available.y * aspect;
	} else {
		// 縦が余っている → 幅基準でサイズを決める
		imageSize.x = available.x;
		imageSize.y = available.x / aspect;
	}
	// ゲームがRelease版でどのウィンドウサイズでも
	gameViewWidth_ = imageSize.x;
	gameViewHeight_ = imageSize.y;

	// ===== 画像を中央揃えで表示する =====
	// ImGui::Imageはデフォルトで左上揃えなので
	// 余白の半分をオフセットとして加算して中央に配置する
	float offsetX = (available.x - imageSize.x) * 0.5f;
	float offsetY = (available.y - imageSize.y) * 0.5f;
	ImVec2 imageMin = ImVec2(contentMin.x + offsetX, contentMin.y + offsetY);
	ImVec2 imageMax = ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

	// ===== 余白を4枚の矩形で塗りつぶす =====
	// Imageの上下左右の余白をそれぞれ独立した矩形で塗る
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImU32 marginColor = IM_COL32(5, 5, 5, 255);
	// 上
	drawList->AddRectFilled({contentMin.x - 15.0f, contentMin.y - 15.0f}, ImVec2(contentMax.x + 15.0f, imageMin.y), marginColor);
	// 下
	drawList->AddRectFilled(ImVec2(contentMin.x - 15.0f, imageMax.y), {contentMax.x + 15.0f, contentMax.y + 15.0f}, marginColor);
	// 左
	drawList->AddRectFilled(ImVec2(contentMin.x - 15.0f, imageMin.y), ImVec2(imageMin.x, imageMax.y), marginColor);
	// 右
	drawList->AddRectFilled(ImVec2(imageMax.x, imageMin.y), ImVec2(contentMax.x + 15.0f, imageMax.y), marginColor);


	ImGui::SetCursorScreenPos(imageMin);

	// ===== 表示 =====
	ImGui::Image((ImTextureID)RenderTexture::GetSRVGPUHandle().ptr, imageSize);

	// ===== Viewportのみ操作を反映させる =====
	bool inViewport = ImGui::IsItemHovered();
	InputManager::SetMouseInViewport(inViewport);

	// Viewportをクリックしたらフォーカスをセット
	if (inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(true);
	}
	// Viewport外をクリックしたらフォーカス解除
	if (!inViewport && ImGui::IsMouseClicked(0)) {
		InputManager::SetViewportFocused(false);
	}

	ImGui::End();
}
#endif

//=============================================================================
// 積まれたコマンドを実行
//=============================================================================
void WindowManager::ExecuteOnly() {
	// コマンドリストを閉じる
	HRESULT hr = DirectXCommon::GetCommandList()->Close();
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
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
	RenderTexture::Release();
#endif
	RenderContext::Release();
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
// プロジェクト固有描画開始、終了処理の登録
//==========================================
int WindowManager::AddPreRenderCallback(std::function<void()> callback, const std::wstring& targetTitle) {
	int id = nextCallbackId_;
	nextCallbackId_++;
	preRenderCallbacks_.push_back({id, targetTitle, std::move(callback)});
	LogManager::Log(std::format("[WindowManager::AddPreRenderCallback] PreRenderCallback登録 id={} target={}", id, targetTitle.empty() ? "全ウィンドウ" : ConvertString(targetTitle)));
	return id;
}

int WindowManager::AddPostRenderCallback(std::function<void()> callback, const std::wstring& targetTitle) {
	int id = nextCallbackId_;
	nextCallbackId_++;
	postRenderCallbacks_.push_back({id, targetTitle, std::move(callback)});
	LogManager::Log(std::format("[WindowManager::AddPreRenderCallback] PostRenderCallback登録 id={} target={}", id, targetTitle.empty() ? "全ウィンドウ" : ConvertString(targetTitle)));
	return id;
}
//==========================================
// プロジェクト固有描画開始、終了処理の登録解除
//==========================================
void WindowManager::RemovePreRenderCallback(int id) {
	std::erase_if(preRenderCallbacks_, [id](const CallbackEntry& entry) { return entry.id == id; });
	LogManager::Log(std::format("[WindowManager::RemovePreRenderCallback] PreRenderCallback解除 id={}", id));
}

void WindowManager::RemovePostRenderCallback(int id) {
	std::erase_if(postRenderCallbacks_, [id](const CallbackEntry& entry) { return entry.id == id; });
	LogManager::Log(std::format("[WindowManager::RemovePreRenderCallback] PostRenderCallback解除 id={}", id));
}