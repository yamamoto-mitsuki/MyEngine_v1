#pragma once
#define NOMINMAX
#include "MyEngine/Sound/SoundManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Camera/DebugCamera.h"
#include "MyEngine/Debug/CrashHandler.h"
#include "MyEngine/Debug/D3D12ResourceLeakChecker.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Collision/CollisionProfiler.h"
#include "MyEngine/Render/PrimitiveRenderer.h"
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Render/ModelManager.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Render/Core/PSOManager.h"
#include "MyEngine/Window/WindowManager.h"
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Utils/GlobalVariables.h"
#include "MyEngine/Debug/GameNotification.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Render/EditorOverlay.h"
#include "MyEngine/Render/ViewportRenderer.h"

#include <chrono>
#include <memory>
#include <string>

class Engine {
public:

	/// <summary>
	/// エンジンの初期化とウィンドウ生成。プログラム起動時に1度だけ呼ぶ。
	/// </summary>
	/// <param name="config">ウィンドウ設定(サイズ・タイトル・ImGui設定・アスペクト比など)</param>
	/// <param name="initialScene">シーンの設定</param>
	static void Initialize(const WindowConfig& config, std::unique_ptr<IScene> initialScene);

	/// <summary>
	/// メッセージ処理。falseが返ったらループを抜ける。
	/// </summary>
	static bool ProcessMessage();

	/// <summary>
	/// フレームの開始処理。毎フレーム最初に呼ぶ。
	/// </summary>
	static void BeginFrame();

	/// <summary>
	/// フレームの終了処理。毎フレーム最後に呼ぶ。
	/// </summary>
	static void EndFrame();

	/// <summary>
	/// エンジンの終了処理。プログラム終了時に1度だけ呼ぶ。
	/// </summary>
	static void Finalize();

	// ===== ゲッター =====
	static DirectXCommon* GetDxCommon() { return instance_->dxCommon_.get(); }
	static WindowManager* GetWindowManager() { return &instance_->windowManager_; }
	static float GetDeltaTime() { return instance_->deltaTime_ * instance_->timeScale_; }
	static float GetGameViewWidth();   // ゲーム画面の幅
	static float GetGameViewHeight();  // ゲーム画面の高さ
	static float GetImGuiOffsetX();    // ImGui描画開始X座標
	static float GetImGuiWidth();      // ImGui幅
	static float GetWindowWidth();     // ウィンドウ全体の幅
	static float GetWindowHeight();    // ウィンドウ全体の高さ

	// ===== セッター =====
	static float GetTimeScale() { return instance_->timeScale_; }

private:
	Engine() = default;
	~Engine() = default;
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	// インスタンス
	static Engine* instance_;

	std::unique_ptr<DirectXCommon> dxCommon_;
	WindowManager windowManager_;
	// 時間管理
	float deltaTime_ = 0.0f; // 前フレームからの経過時間
	float timeScale_ = 1.0f; // deltaTimeの経過倍率。通常1.0f
	std::chrono::high_resolution_clock::time_point lastTime_;    // 前フレームの時刻
	std::chrono::high_resolution_clock::time_point updateStart_; // 更新処理の計測用
};