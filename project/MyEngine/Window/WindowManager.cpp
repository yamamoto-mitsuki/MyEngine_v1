#define NOMINMAX
#include "WindowManager.h"
#include <format>
#include <externals/imgui/imgui.h>
#include <externals/imgui/ImGuizmo.h>
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Sound/SoundManager.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Editor/Viewport/EditorOverlay.h"
#include "MyEngine/Editor/Viewport/ViewportWindow.h"
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Systems/AnimationSystem.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/LightSystem.h"
#include "MyEngine/Systems/PhysicsSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/SpriteRenderSystem.h"
#include "MyEngine/Systems/TrailSystem.h"
#include "MyEngine/Systems/TextRenderSystem.h"
#include "MyEngine/Systems/ElasticSurfaceSystem.h"
#include "MyEngine/Systems/PrimitiveRenderSystem.h"
#include "MyEngine/Systems/PolygonShapeSystem.h"
#include "MyEngine/Systems/ArrowStripSystem.h"
#include "MyEngine/Systems/WindZoneSystem.h"
#include "MyEngine/Scene/PrefabManager.h"
#include "MyEngine/Systems/ScriptSystem.h"
#include "MyEngine/Time/EngineTime.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/Renderer/SkinCache.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Text/FontManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Scene/SceneManager.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Windows/ControlWindow.h"
#endif


//=============================================================================
// ウィンドウの追加
//=============================================================================
void WindowManager::AddWindow(const WindowConfig& config, SceneFactory sceneFactory) {
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
	sceneManager->SetWindowTitle(config.title);
	sceneManager->Initialize(std::move(sceneFactory));
	// Editor
	auto editor = std::make_unique<EditorViewport>();
#ifdef USE_IMGUI
	editor->Initialize();
#endif

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
	// シーンは上の sceneManager->Initialize() が作っている（ここでもう1つ作らない）
}

//=============================================================================
// 全ウィンドウのメッセージ処理
//=============================================================================
bool WindowManager::ProcessMessage() {
	for (std::vector<WindowSet>::iterator it = windows_.begin(); it != windows_.end();) {
		if (!it->window->ProcessMessage()) {
			// ウィンドウが閉じられたら、シーンの後片付けをしてから削除（×で閉じるのが普通の終わり方なので、ここで呼ばないとFinalizeが一度も走らない）
			if (it->sceneManager) {
				it->sceneManager->Finalize();
			}
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
	EntityManager::FlushComponentChanges(); // 前フレームに予約したComponentを追加
	PrefabManager::Flush(); // 前フレームに Prefab から作った Entity に、Component・Script を付ける（エディタから置く予約もここで作る）
#ifdef USE_IMGUI
	EditorHistory::Flush();
#endif

	HWND focused = GetForegroundWindow();
	for (WindowSet& w : windows_) {
		bool isFocused = (focused == w.window->GetHWND());
		InputManager::SetActiveWindow(isFocused ? w.window->GetHWND() : nullptr);
		// シーンマネージャの基底クラス
		if (w.sceneManager) {
			w.sceneManager->Update();
		}

		#ifdef USE_IMGUI
		if (w.editor) {
			w.editor->Update(); // デバッグカメラ更新
		}
#endif
	}
	// パーティクルのビルボード用カメラは、現在のシーンから毎フレーム取る。
	// ゲーム側が SetCamera を呼ぶ必要がなくなり、破棄済みカメラを掴む事故も起きない
	std::shared_ptr<Camera> gameCamera;
	for (WindowSet& w : windows_) {
		if (!w.sceneManager) {
			continue;
		}
		IScene* scene = w.sceneManager->GetCurrentScene();
		if (scene && scene->GetCamera()) {
			gameCamera = scene->GetCamera();
			break;
		}
	}

	// 音（消したEntityの音を止めるので、FlushDestroy の後）。エディタの停止中はAudioSourceを鳴らさない
	bool isGameRunning = false;
	for (WindowSet& w : windows_) {
		if (w.sceneManager && w.sceneManager->GetPlayState() != PlayState::Editing) {
			isGameRunning = true; // 再生中・一時停止中
		}
	}
	ParticleManager::SetCamera(gameCamera); // 見つけたカメラを毎フレーム渡す（シーンが替わっても古いカメラを掴まない）
	AnimationSystem::Update(isGameRunning, Time::GetDeltaTime()); // アニメーション。停止中もInspectorのPreview Timeのポーズを見せる。
	EntityManager::UpdateTransforms();      // ワールド行列（親→子）
	// 風（Wind Zone）の範囲を集め直して、再生中なら Rigidbody を押す（押した速さで、すぐ後の物理が動かす）。画面の風の線の値も書く
	WindZoneSystem::Update(isGameRunning, Time::GetDeltaTime());
	// Rigid body を動かして、ぶつかった物から押し戻す
	// 動かした物があれば、ワールド行列を計算し直す（カメラ・ライト・当たり判定・描画が、押し戻した後の位置を使うように）
	if (isGameRunning && PhysicsSystem::Update(Time::GetDeltaTime())) {
		EntityManager::UpdateTransforms();
	}
	// カメラの追いかけ（Camera Follow）。物理で動いた後の位置を追う。カメラを動かしたら、カメラ（と子）のワールド行列を計算し直す
	if (isGameRunning && CameraSystem::UpdateFollow(Time::GetDeltaTime())) {
		EntityManager::UpdateTransforms();
	}
	CameraSystem::Update();                 // Camera Component の位置・向きをゲームのカメラへ写す（ワールド行列の後、使う人たちの前）
	ParticleManager::Update();              // パーティクル更新
	LightSystem::Update();                  // ライト更新
	ScriptSystem::FlushDestroy();           // Destroy(Script) の予約の反映（OnDestroy を呼ぶ。Entity の破棄より先）
	EntityManager::FlushDestroy();          // 破棄予約の反映
	CollisionSystem::Update();              // 当たり判定（消すと決めたEntityを消した後。結果は次のフレームのSYSTEMが読む）
	AudioSystem::Update(isGameRunning, gameCamera.get());
	TrailSystem::Update(isGameRunning, Time::GetDeltaTime());
	ElasticSurfaceSystem::Update(isGameRunning);

	InputManager::SetActiveWindow(nullptr); // 入力をリセット
}


//=============================================================================
//全ウィンドウの描画をコマンド積む
//=============================================================================
void WindowManager::DrawAll() {
	for (WindowSet& w : windows_) {
		// --- 描画リクエスト ---
		if (w.sceneManager) {
			w.sceneManager->Draw(); // シーン
			IScene* scene = w.sceneManager->GetCurrentScene();
#ifdef USE_IMGUI
			w.editor->Draw(scene ? scene->GetCamera().get() : nullptr); // Editor（グリッドなど）
#endif
		}
	}
	ParticleManager::Draw(); // パーティクルの描画

	// 最初に見つかったシーンのカメラ（軌跡の帯をカメラへ向ける・ギズモに使う）と、そのシーンの環境光（PBR の形に使う）
	Camera* gameCamera = nullptr;
	IBLEnvironment* gameEnvironment = nullptr;
	for (WindowSet& w : windows_) {
		IScene* scene = w.sceneManager ? w.sceneManager->GetCurrentScene() : nullptr;
		if (scene && scene->GetCamera()) {
			gameCamera = scene->GetCamera().get();     // 借りるだけ（持ち主はシーン・CameraSystem）
			gameEnvironment = scene->GetEnvironment(); // 借りるだけ（持ち主はシーン。持っていなければ nullptr）
			break;
		}
	}
	TrailSystem::Draw(gameCamera);                            // 軌跡の描画
	ElasticSurfaceSystem::Draw(gameCamera, gameEnvironment);  // 弾む面（厚みのある板と支柱）の描画
	PrimitiveRenderSystem::Draw(gameCamera, gameEnvironment); // 決まった形（球・箱）の描画
	PolygonShapeSystem::Draw(gameCamera, gameEnvironment);    // 多角形を押し出した形（足場）の描画
	ArrowStripSystem::Draw(gameCamera, gameEnvironment);      // 向きを示す矢印の列の描画
	SpriteRenderSystem::Draw(gameCamera); // 画像の板（Sprite Renderer）と、画面に重ねる絵（Image）の描画
	TextRenderSystem::Draw(); // 画面に重ねる文字（Text）の描画（Image とは Order の順に重なる）

#ifdef USE_IMGUI
	// ライト・Collider のギズモ（エディタ用）
	LightSystem::DrawGizmos(gameCamera);
	CollisionSystem::DrawGizmos(gameCamera);
	WindZoneSystem::DrawGizmos(gameCamera); // 風の範囲と向き（Collider のギズモと一緒に出す・消す）
	AnimationSystem::DrawGizmos(gameCamera);
#endif
}


//=============================================================================
// ウィンドウ描画開始
//=============================================================================
void WindowManager::PreRenderAll() {
	// ===== 文字のアトラスを GPU へ送る（DrawAll の Text で、初めて使った文字が足されている）=====
	// 描画とは別のコマンドリストで送って待つので、このフレームの描画からもう読める
	FontManager::UploadAtlases();

	// ===== スキンのあるメッシュを CS で曲げる（どのウィンドウも描き始める前に、1フレームに1回）=====
	// DrawAll の DrawModel が積んだ仕事を全部ここで Dispatch する。Scene ビュー・Game ビューは、同じ曲げた頂点を描く
	SkinCache::Dispatch(DirectXCommon::GetCommandList());

	// ウィンドウ分描画する
	for (WindowSet& w : windows_) {
		// ImGuiのフレーム開始
#ifdef USE_IMGUI
		if (w.window->GetWindowConfig().isImGui) {
			// 調整項目のImGui
			ImGuiManager::Begin();
			ControlWindow::Draw(w.sceneManager.get()); // 再生・ギズモの道具・保存
		}
#endif

		// ===== 描画開始処理(RenderTargetの切り替えやバリアの設定など) =====
		w.renderer->PreDraw(); // バリア変更 + クリア
		
		// シーンのカメラ取得
		IScene* scene = w.sceneManager->GetCurrentScene();
		Camera* gameCamera = scene ? scene->GetCamera().get() : nullptr;

#ifdef USE_IMGUI
		// Debug: シーンとエディタの表示
		w.editor->Render(gameCamera, w.window->GetTitle());
#else
		// Release: RenderTexture を経由せずスワップチェーンへ直接描く
		RECT rc{};
		GetClientRect(w.window->GetHWND(), &rc);
		SceneRenderer::RenderToWindow(gameCamera, w.renderer.get(), w.window->GetTitle(), static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top));
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

	RenderQueue::Clear(); // PrimitiveRendererリクエストをクリア
	RenderContext::ResetFrame();     // GPU用データの書き込み位置を先頭に戻す
	SceneRenderer::ResetViewIndex(); // カメラのリングバッファ参照位置リセット

	// プロジェクト側で追加した後処理を実行
	for (const auto& cb : framEndCallbacks_) {
		cb.func();
	}

	// 全ウィンドウのコマンドリストがコマンドキューに投げられた後、ここで待つ
	DirectXCommon::WaitForGPU();
	// GPUがもう読まないと確かになったので、このフレームに返してもらったSRVスロットを配り直せるようにする
	DirectXCommon::RecycleFreedSRVSlots();
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
	FontManager::Release();
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