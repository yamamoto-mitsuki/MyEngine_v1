#include "MyEngine/Engine.h"

#include <psapi.h>
#include <shobjidl.h>

// Editor
#include "MyEngine/Editor/Profiler.h"
#include "MyEngine/Editor/EditorOverlay.h"
// Diagnostics
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/CrashHandler.h"
// Sound
#include "MyEngine/Sound/SoundManager.h"
// Input
#include "MyEngine/Input/InputManager.h"
// Collision
#include "MyEngine/Collision/Profiling/CollisionProfiler.h"
// Graphics
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/Renderer/PrimitiveRenderer.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/ViewportRenderer.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"

#pragma comment(lib, "psapi.lib")

// 静的メンバ変数
Engine* Engine::instance_ = nullptr;


//=============================================================================
// 初期化
//=============================================================================
void Engine::Initialize(const WindowConfig& config, std::unique_ptr<IScene> initialScene) {
	// COM（Component Object Model）の初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(SUCCEEDED(hr), "COMの初期化に失敗しました");
	}
	// 通知を出すためのID登録
	SetCurrentProcessExplicitAppUserModelID(L"MyEngine");
	GameNotification::Initialize();
	// クラッシュハンドラの登録
	SetUnhandledExceptionFilter(ExportDump);

	// インスタンス生成
	instance_ = new Engine();

	// ログ初期化
	LogManager::Initialize();
	// DirectX初期化
	instance_->dxCommon_ = std::make_unique<DirectXCommon>();
	instance_->dxCommon_->Initialize();
	// 各初期化
#ifdef USE_IMGUI
	RenderTexture::Initialize(1280, 720);
	EditorOverlay::Initialize();
#endif
	ViewportRenderer::Initialize();
	Time::Initialize();
	GPUProfiler::Initialize();
	CollisionProfiler::Initialize();
	RenderContext::Initialize();
	UploadContext::Initialize();
	PrimitiveRenderer::Initialize();
	TextureManager::Initialize();
	ModelManager::Initialize();
	SoundManager::Initialize();
	PSOManager::Initialize();
	// ウィンドウ生成
	instance_->windowManager_.AddWindow(config, std::move(initialScene));
	// InputManager初期化
	InputManager::Initialize(GetModuleHandle(nullptr), instance_->windowManager_.GetMainHWND());
	// lastTime_ を現在時刻で初期化
	instance_->lastTime_ = std::chrono::high_resolution_clock::now();
}

//=============================================================================
// ウィンドウが受け取るメッセージ
//=============================================================================
bool Engine::ProcessMessage() { return instance_->windowManager_.ProcessMessage(); }

//=============================================================================
// フレームの最初に行う処理
//=============================================================================
void Engine::BeginFrame() {
	// 操作入力の更新
	InputManager::Update();
	// deltaTime の計算
	auto now = std::chrono::high_resolution_clock::now();
	float dt = std::chrono::duration<float>(now - instance_->lastTime_).count();
	// 重い処理やブレークポイントで止めて再開したときに巨大なdeltaTimeになるのを防ぐためにクランプ
	if (dt > 0.1f) {
		dt = 0.1f;
	}
	// 今の時刻を保存しておく（次のフレームで使う）
	Time::SetDeltaTime(dt);
	instance_->lastTime_ = now;
	// Updateの処理時間を計測
	instance_->updateStart_ = std::chrono::high_resolution_clock::now();
}

void Engine::EndFrame() {
#ifdef USE_IMGUI
	// Updateの処理時間を確定
	auto updateEnd = std::chrono::high_resolution_clock::now();
	float updateMs = std::chrono::duration<float, std::milli>(updateEnd - instance_->updateStart_).count();
	Profiler::Category(ProfCategory::CPU).Group("Times").Value("Update", updateMs, "ms");
#endif

#ifdef _DEBUG
	// Renderの処理時間を計測（CPU時間）
	auto renderStart = std::chrono::high_resolution_clock::now();
	// GPU時間での測定を開始
	GPUProfiler::BeginFrame();
#endif

	// Render
	instance_->windowManager_.PreRenderAll();
	instance_->windowManager_.PostRenderAll();

#ifdef USE_IMGUI
	// Renderの処理時間を確定（CPU）
	auto renderEnd = std::chrono::high_resolution_clock::now();
	float renderMs = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
	Profiler::Category(ProfCategory::CPU).Group("Times").Value("Render", renderMs, "ms");
	// 処理時間の結果を保存（GPU）
	GPUProfiler::Readback();
	// Frameの結果
	Profiler::Category(ProfCategory::Frame).Value("FPS", ImGui::GetIO().Framerate);
	Profiler::Category(ProfCategory::Frame).Value("Frame Time", Time::GetUnscaleDeltaTime() * 1000.0f, "ms");
	// 当たり判定
	CollisionProfiler::Report();
	CollisionProfiler::Reset();

	// VRAM（IDXGIAdapter3経由で取得しているのでゲーム実行中だけではなく、システム全体の量）
	IDXGIAdapter4* adapter = DirectXCommon::GetAdapter4();
	if (adapter) {
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
		float vramGB = info.CurrentUsage / (1024.0f * 1024.0f * 1024.0f);
		Profiler::Category(ProfCategory::Memory).Value("System VRAM", vramGB, "GB");
	}

	// メモリ使用量（Windows API経由で取得しているのでゲーム実行中だけではなく、システム全体の量）
	PROCESS_MEMORY_COUNTERS pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		float memMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);
		Profiler::Category(ProfCategory::Memory).Value("System WorkingSet", memMB, "MB");
	}
	// DrawCall数の確定とリセット
	Profiler::Category(ProfCategory::GPU).Group("Stats").Value("Draw Calls", DirectXCommon::GetDrawCallCount());
	DirectXCommon::ResetDrawCallCount();

#endif
}

void Engine::Finalize() {
	InputManager::Release();
	CollisionProfiler::Release();
	GPUProfiler::Release();
#ifdef USE_IMGUI
	EditorOverlay::Release();
	ViewportRenderer::Release();
#endif
	instance_->windowManager_.Finalize();
	delete instance_;
	instance_ = nullptr;
	LogManager::Flush();
	LogManager::Shutdown();
	CoUninitialize();
}

//==========================================
// ゲッター
//==========================================
float Engine::GetGameViewWidth() { return ViewportRenderer::GetGameViewWidth(); }
float Engine::GetGameViewHeight() { return ViewportRenderer::GetGameViewHeight(); }