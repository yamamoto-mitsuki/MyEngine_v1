#pragma once
#define NOMINMAX

#include <chrono>
#include <memory>
#include <string>

#include "MyEngine/Time/time.h"
#include "MyEngine/Window/WindowManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"


/// <summary>
/// エンジンの基盤。ここでゲームループ、初期化を管理
/// </summary>
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
	static float GetDeltaTime() { return Time::GetDeltaTime(); }
	static float GetTimeScale() { return Time::GetTimeScale(); }
	static float GetGameViewWidth();   // ゲーム画面の幅
	static float GetGameViewHeight();  // ゲーム画面の高さ
	static float GetImGuiOffsetX();    // ImGui描画開始X座標
	static float GetImGuiWidth();      // ImGui幅
	static float GetWindowWidth();     // ウィンドウ全体の幅
	static float GetWindowHeight();    // ウィンドウ全体の高さ

private:
	Engine() = default;
	~Engine() = default;
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	// インスタンス
	static Engine* instance_;

	std::unique_ptr<DirectXCommon> dxCommon_;
	WindowManager windowManager_;
	std::chrono::high_resolution_clock::time_point lastTime_;    // 前フレームの時刻
	std::chrono::high_resolution_clock::time_point updateStart_; // 更新処理の計測用
};