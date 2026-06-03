#include "MyEngine/Engine.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Render/RenderTexture.h"
#include <psapi.h>
#include <shobjidl.h>
#pragma comment(lib, "psapi.lib")

Engine* Engine::instance_ = nullptr;

void Engine::Initialize(const WindowConfig& config, std::unique_ptr<IScene> initialScene) {
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr) && "COMの初期化に失敗しました");

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
#endif
	RenderContext::Initialize();
	DebugRender::Initialize();
	TextureManager::Init(instance_->dxCommon_.get());
	ModelManager::Initialize();
	SoundManager::Initialize();
	PSOManager::Initialize();
	// ウィンドウ生成
	instance_->windowManager_.AddWindow(config, std::move(initialScene));
	// InputManager初期化
	InputManager::Initialize(GetModuleHandle(nullptr), instance_->windowManager_.GetMainHWND());
	// lastTime_ を現在時刻で初期化
	instance_->lastTime_ = std::chrono::high_resolution_clock::now();

	std::string logPath = LogManager::GetLogFilePath();
	std::wstring absoluteLogPath = std::filesystem::absolute(logPath).wstring();
	std::wstring command = L"/c code\"" + absoluteLogPath + L"\"";
}

bool Engine::ProcessMessage() { return instance_->windowManager_.ProcessMessage(); }

void Engine::BeginFrame() {
	// deltaTime の計算
	auto now = std::chrono::high_resolution_clock::now();
	instance_->deltaTime_ = std::chrono::duration<float>(now - instance_->lastTime_).count();
	// 重い処理やブレークポイントで止めて再開したときに巨大なdeltaTimeになるのを防ぐためにクランプ
	if (instance_->deltaTime_ > 0.1f) {
		instance_->deltaTime_ = 0.1f;
	}
	// 今の時刻を保存しておく（次のフレームで使う）
	instance_->lastTime_ = now;
	// 操作入力の更新
	InputManager::Update();
	// Updateの処理時間を計測
	instance_->updateStart_ = std::chrono::high_resolution_clock::now();
}

void Engine::EndFrame() {
#ifdef USE_IMGUI
	// Updateの処理時間を確定
	auto updateEnd = std::chrono::high_resolution_clock::now();
	float updateMs = std::chrono::duration<float, std::milli>(updateEnd - instance_->updateStart_).count();
	// Renderの処理時間を計測
	auto renderStart = std::chrono::high_resolution_clock::now();
#endif

	// Render
	instance_->windowManager_.PreRenderAll();
	instance_->windowManager_.PostRenderAll();

#ifdef USE_IMGUI
	// Renderの処理時間を確定
	auto renderEnd = std::chrono::high_resolution_clock::now();
	float renderMs = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
	// デバッグ表示に反映
	float fps = ImGui::GetIO().Framerate;
	float frameMs = instance_->deltaTime_ * 1000.0f;
	ImGuiManager::SetDebugValue(DebugCategory::Performance, "FPS", fps, "");
	ImGuiManager::SetDebugValue(DebugCategory::Performance, "Frame Time", frameMs, "ms");
	ImGuiManager::SetDebugValue(DebugCategory::Performance, "Update", updateMs, "ms");
	ImGuiManager::SetDebugValue(DebugCategory::Performance, "Render", renderMs, "ms");

	// VRAM（IDXGIAdapter3経由で取得）
	IDXGIAdapter4* adapter = DirectXCommon::GetAdapter4();
	if (adapter) {
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
		float vramGB = info.CurrentUsage / (1024.0f * 1024.0f * 1024.0f);
		ImGuiManager::SetDebugValue(DebugCategory::Memory, "VRAM", vramGB, "GB");
	}

	// メモリ使用量（Windows API）
	PROCESS_MEMORY_COUNTERS pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		float memMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);
		ImGuiManager::SetDebugValue(DebugCategory::Memory, "Memory", memMB, "MB");
	}

	// DrawCall数の確定とリセット
	ImGuiManager::SetDebugValue(DebugCategory::Rendering, "Draw Calls", DirectXCommon::GetDrawCallCount());
	DirectXCommon::ResetDrawCallCount();
#endif
}

void Engine::Finalize() {
	InputManager::Release();
	instance_->windowManager_.Finalize();
	delete instance_;
	instance_ = nullptr;
	// ===== 終了通知 =====
	LogManager::Flush();
	std::string logPath = LogManager::GetLogFilePath();
	std::wstring absoluteLogPath = std::filesystem::absolute(logPath).wstring();
	std::wstring command = L"/c code\"" + absoluteLogPath + L"\"";

	GameNotification::Send("エンジン終了", "",
	{
	   {"logをVSCodeで開く", [absoluteLogPath]() { 
		ShellExecuteW(nullptr, 
					  L"open", 
					  L"C:\\Users\\K025G\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe", 
					  absoluteLogPath.c_str(), 
					  nullptr, 
					  SW_SHOW); 
	   }}
    });

	Sleep(500);
	LogManager::Shutdown();
	CoUninitialize();
}

//==========================================
// ゲッター
//==========================================
float Engine::GetGameViewWidth() { return instance_->windowManager_.GetGameViewWidth(); }
float Engine::GetGameViewHeight() { return instance_->windowManager_.GetGameViewHeight(); }