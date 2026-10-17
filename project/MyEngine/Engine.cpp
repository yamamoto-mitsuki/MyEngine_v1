#include "MyEngine/Engine.h"
#include <psapi.h>
#include <shobjidl.h>
// Editor
#include "MyEngine/Editor/Gizmos/LightGizmo.h"
#include "MyEngine/Editor/Viewport/EditorOverlay.h"
#include "MyEngine/Editor/Profiler.h"
// Diagnostics
#include "MyEngine/Diagnostics/CrashHandler.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
// UI
#include "MyEngine/UI/GlobalVariables.h"
// Sound
#include "MyEngine/Sound/SoundManager.h"
// ECS（Entity と Component の仕組み）
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
// Systems（Component を読んで処理する係）
#include "MyEngine/Systems/AnimationSystem.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/LightSystem.h"
#include "MyEngine/Systems/PhysicsSystem.h"
#include "MyEngine/Systems/SpriteRenderSystem.h"
#include "MyEngine/Systems/TrailSystem.h"
#include "MyEngine/Systems/ScriptSystem.h"
// Input
#include "MyEngine/Input/InputManager.h"
// Math
#include "MyEngine/Math/MathIncludes.h"
// Collision
#include "MyEngine/Collision/Profiling/CollisionProfiler.h"
// Particle
#include "MyEngine/Particle/ParticleManager.h"
// Graphics
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/IBL/IBLBaker.h"
#include  "MyEngine/Graphics/Animation/AnimationManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureManager.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

#pragma comment(lib, "psapi.lib")

// 静的メンバ変数
std::unique_ptr<Engine> Engine::instance_ = nullptr;


//=============================================================================
// 初期化
//=============================================================================
void Engine::Initialize(const WindowConfig& config, SceneFactory initialScene) {
	MY_ASSERT_MSG(initialScene != nullptr, "シーンの作り方(initialScene)が渡されていません");
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
	instance_ = std::make_unique<Engine>(ConstructorKey());

	// ログ初期化
	LogManager::Initialize();
	// DirectX初期化
	DirectXCommon::Initialize();
	UploadContext::Initialize();
	RenderTextureManager::Initialize();
	TextureManager::Initialize();
	ModelManager::Initialize();
	AnimationManager::Initialize();
	// 各初期化
#ifdef USE_IMGUI
	EditorOverlay::Initialize();
#endif
	Time::Initialize();
	GPUProfiler::Initialize();
	CollisionProfiler::Initialize();
	RenderContext::Initialize();
	RenderQueue::Initialize();
	Renderer::Initialize();
	SceneRenderer::Initialize();
	SoundManager::Initialize();
	PSOManager::Initialize();
	RootSignatureManager::Initialize();
	ShaderPackageLoader::Initialize();
	ShaderCompiler::Initialize();
	RandomEngine::Initialize();
	ParticleManager::Initialize();
	LightGizmo::Initialize();
	EntityManager::Initialize();
	LightSystem::Initialize();     // ライトのComponentをEntityManagerに登録するので、EntityManagerの後
	CollisionSystem::Initialize(); // ColliderをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	PhysicsSystem::Initialize();   // RigidBodyをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	AudioSystem::Initialize();     // AudioSourceをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	CameraSystem::Initialize();    // CameraをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	TrailSystem::Initialize();     // TrailをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	AnimationSystem::Initialize(); // AnimatorをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	SpriteRenderSystem::Initialize(); // Sprite Renderer・ImageをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	ScriptSystem::Initialize();    // Entityが消える直前の知らせをEntityManagerに頼むので、EntityManagerの後（Scriptの種類の登録は下のApplyAll）
	ComponentSerializer::RegisterEngineComponents(); // Transform・ModelRenderer・ライトの保存項目（Releaseでもシーンを読むのに要る）
	GameComponentRegistry::ApplyAll(); // COMPONENT(...) で書いたゲーム固有Componentを登録（登録待ちの表を空にする）

	ShaderPackageLoader::LoadAll(ShaderPackageLoader::FindDataDir()); // .hlsl生成
	IBLBaker::Initialize(); // IBL
	SceneRenderer::InitializeSkybox(); // 天球初期化
	SceneRenderer::InitializePost(1920, 1080); // ブルーム初期化
	instance_->windowManager_.AddWindow(config, std::move(initialScene)); // ウィンドウ生成                                 
	InputManager::Initialize(GetModuleHandle(nullptr), instance_->windowManager_.GetMainHWND()); // InputManager初期化
	GlobalVariables::GetInstance()->LoadFiles(); // 調整項目適用
	instance_->lastTime_ = std::chrono::high_resolution_clock::now(); // lastTime_ を現在時刻で初期化
}


//=============================================================================
// 初期化（最初のシーンを名前で）
//=============================================================================
void Engine::Initialize(const WindowConfig& config, const std::string& firstScene) {
#ifdef USE_IMGUI
	// エディタは、前回最後に開いていたシーンから始める（Unity と同じ。覚えていなければ firstScene）
	const std::string sceneName = SceneManager::GetLastOpenedScene(firstScene);
#else
	// 製品版は、いつも firstScene から
	const std::string sceneName = firstScene;
#endif
	// 名前からシーンを作る「作り方」を渡す（Stop・Restart でも、この作り方で開いているシーンを作り直す）
	Initialize(config, [sceneName] { return SceneManager::CreateScene(sceneName); });
}


//=============================================================================
// ウィンドウが受け取るメッセージ
//=============================================================================
bool Engine::ProcessMessage() { return instance_->windowManager_.ProcessMessage(); }


//=============================================================================
// フレームの最初に行う処理
//=============================================================================
void Engine::BeginFrame() {
	InputManager::Update(); // 操作入力
	SoundManager::CleanupSourceVoices(Time::GetUnscaleDeltaTime()); // サウンド
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


//=============================================================================
// フレームの最後に行う処理
//=============================================================================
void Engine::EndFrame() {
#ifdef USE_IMGUI
	// Updateの処理時間を確定
	auto updateEnd = std::chrono::high_resolution_clock::now();
	float updateMs = std::chrono::duration<float, std::milli>(updateEnd - instance_->updateStart_).count();
	Profiler::Category(ProfCategory::CPU).Group("Times").Value("Update", updateMs, "ms");

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
	for (const GPUProfiler::Result& r : GPUProfiler::GetResults()) {
		Profiler::Category(ProfCategory::GPU).Group("Times").Value(r.name, r.ms, "ms");
	}
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

	// 1フレームにGPU用に書いたデータの量（RenderContextのFrameUploadBuffer）
	constexpr float kBytesToMB = 1.0f / (1024.0f * 1024.0f);
	float uploadMB = static_cast<float>(RenderContext::GetLastFrameUploadBytes()) * kBytesToMB;
	float uploadCapacityMB = static_cast<float>(RenderContext::GetFrameUploadCapacity()) * kBytesToMB;
	Profiler::Category(ProfCategory::GPU).Group("Stats").Bar("Frame Upload", uploadMB, uploadCapacityMB, "MB");

	// SRVスロットの使っている数（Play / Stop を何回繰り返しても、ここが増え続けなければよい）
	float usedSrvSlots = static_cast<float>(DirectXCommon::GetUsedSRVSlotCount());
	float srvSlotCapacity = static_cast<float>(DirectXCommon::kSRVDescriptorHeap - 1); // 0番はImGui用
	Profiler::Category(ProfCategory::GPU).Group("Stats").Bar("SRV Slots", usedSrvSlots, srvSlotCapacity);
#endif
}


//=============================================================================
// 終了処理
//=============================================================================
void Engine::Finalize() {
	ScriptSystem::Release(); // 動いている Script の OnDestroy を、ほかの物を片付ける前に呼ぶ（中で音やパーティクルを使ってもよいように）
	CollisionProfiler::Release();
	GPUProfiler::Release();
	ShaderCompiler::Release();
	ShaderPackageLoader::Release();
	PSOManager::Release();
	RootSignatureManager::Release();
	ParticleManager::Release();
#ifdef USE_IMGUI
	EditorOverlay::Release();
#endif
	SceneRenderer::Release();
	instance_->windowManager_.Finalize();

	LightSystem::Release();     // シーン（ウィンドウ）の破棄より後にする
	CollisionSystem::Release(); // シーン（ウィンドウ）の破棄より後にする
	PhysicsSystem::Release();   // シーン（ウィンドウ）の破棄より後にする
	AudioSystem::Release();     // シーン（ウィンドウ）の破棄より後にする
	CameraSystem::Release();    // シーン（ウィンドウ）の破棄より後にする
	TrailSystem::Release();     // シーン（ウィンドウ）の破棄より後にする
	AnimationSystem::Release(); // シーン（ウィンドウ）の破棄より後・AnimationManager::Release より前にする（AnimationPlayer が中身を見ているので）
	SpriteRenderSystem::Release(); // シーン（ウィンドウ）の破棄より後にする
	EntityManager::Release();   // シーン（ウィンドウ）の破棄より後にする
	// --- 今まで Release を呼んでいなかった物（作りっぱなしで、終わるときに消していなかった）---
	AnimationManager::Release();
	IBLBaker::Finalize();
	RenderQueue::Release();
	Renderer::Release();
	Time::Release();
	DirectXCommon::Release(); // GPUの物（テクスチャ・RenderTextureなど）を全部消した後、一番最後に
	instance_.reset();
	LogManager::Flush();
	LogManager::Shutdown();
	CoUninitialize();
}