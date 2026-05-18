#include "MyEngine/Engine.h"

Engine* Engine::instance_ = nullptr;

void Engine::Initialize(const WindowConfig& config, std::unique_ptr<IScene> initialScene) {
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr) && "COMの初期化に失敗しました");

	// クラッシュハンドラの登録
	SetUnhandledExceptionFilter(ExportDump);

	// インスタンス生成
	instance_ = new Engine();

	// ログ初期化
	LogManager::Init();

	// DirectX初期化
	instance_->dxCommon_ = std::make_unique<DirectXCommon>();
	instance_->dxCommon_->Init();

	// 各マネージャーの初期化
	DebugRender::GetInstance();
	TextureManager::Init(instance_->dxCommon_.get());
	ModelManager::Init(instance_->dxCommon_.get());
	SoundManager::Init();

	// GlobalVariablesファイル読み込み
	GlobalVariables::GetInstance()->LoadFiles();

	// ウィンドウ生成
	instance_->windowManager_.AddWindow(config, instance_->dxCommon_.get(), std::move(initialScene));

#ifdef USE_IMGUI
	if (config.isImGui) {
		instance_->windowManager_.SetImGuiTargetWindow(config.title);
	}
#endif

	// InputManager初期化
	InputManager::Init(GetModuleHandle(nullptr), instance_->windowManager_.GetMainHWND());
	
	// lastTime_ を現在時刻で初期化
	instance_->lastTime_ = std::chrono::high_resolution_clock::now();
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

	InputManager::Update(); 
}

void Engine::EndFrame() {
	instance_->windowManager_.PreRenderAll();
	instance_->windowManager_.PostRenderAll();
}

void Engine::Finalize() {
	InputManager::Finalize();
	instance_->windowManager_.Finalize();

	delete instance_;
	instance_ = nullptr;

	CoUninitialize();
}