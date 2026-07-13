#pragma once
#include <cstdint>

#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "MyEngine/Input/MouseInput.h"
#include "MyEngine/Input/KeyboardInput.h"
#include "MyEngine/Input/XInputGamepad.h"



/// <summary>
/// すべての入力処理を管理するクラス
/// </summary>
class InputManager {
public:
	// コピー・ムーブ禁止
	InputManager() = default;
	~InputManager() = default;
	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	/// <summary>
	/// 初期化。ウィンドウ生成後に1度だけ呼ぶ。
	/// </summary>
	static void Initialize(HINSTANCE hInstance, HWND hwnd);

	/// <summary>
	/// 終了処理。プログラム終了時に1度だけ呼ぶ。
	/// </summary>
	static void Release();

	/// <summary>
	/// 毎フレームメインループの先頭で呼ぶ。
	/// </summary>
	static void Update();

	//============================================================
	// キーボード
	//============================================================

	/// <summary>指定したキーが押されているか。</summary>
	static bool IsKeyPressed(uint8_t key);

	/// <summary>指定したキーを押した瞬間か。</summary>
	static bool IsKeyTriggered(uint8_t key);

	/// <summary>指定したキーを離した瞬間か。</summary>
	static bool IsKeyReleased(uint8_t key);

	//============================================================
	// マウス ボタン番号: 0=左, 1=右, 2=中
	//============================================================

	/// <summary>指定ボタンが押されているか。</summary>
	static bool IsMouseButtonPressed(int button);

	/// <summary>指定ボタンを押した瞬間か。</summary>
	static bool IsMouseButtonTriggered(int button);

	/// <summary>指定ボタンを離した瞬間か。</summary>
	static bool IsMouseButtonReleased(int button);

	/// <summary>マウスX座標 (クライアント座標、ウィンドウ左端が 0)。</summary>
	static long GetMousePositionX();

	/// <summary>マウスY座標 (クライアント座標、ウィンドウ上端が 0)。</summary>
	static long GetMousePositionY();

	/// <summary>前フレームからのX移動量 (右が正)。</summary>
	static long GetMouseDeltaX();

	/// <summary>前フレームからのY移動量 (下が正)。</summary>
	static long GetMouseDeltaY();

	/// <summary>ホイール移動量 (上スクロールが正)。</summary>
	static long GetMouseDeltaWheel();

	//============================================================
	// Xbox コントローラー
	//============================================================

	/// <summary>指定番号のコントローラーが接続されているか。</summary>
	static bool IsGamepadConnected(int index = 0);

	/// <summary>指定ボタンが押されているか。</summary>
	static bool IsButtonPressed(uint16_t button, int index = 0);

	/// <summary>指定ボタンを押した瞬間か。</summary>
	static bool IsButtonTriggered(uint16_t button, int index = 0);

	/// <summary>指定ボタンを離した瞬間か。</summary>
	static bool IsButtonReleased(uint16_t button, int index = 0);

	/// <summary>左スティックX軸 (-1.0f=左, 1.0f=右, デッドゾーン処理済み)。</summary>
	static float GetLeftStickX(int index = 0);

	/// <summary>左スティックY軸 (-1.0f=下, 1.0f=上, デッドゾーン処理済み)。</summary>
	static float GetLeftStickY(int index = 0);

	/// <summary>右スティックX軸 (-1.0f=左, 1.0f=右, デッドゾーン処理済み)。</summary>
	static float GetRightStickX(int index = 0);

	/// <summary>右スティックY軸 (-1.0f=下, 1.0f=上, デッドゾーン処理済み)。</summary>
	static float GetRightStickY(int index = 0);

	/// <summary>左トリガー (0.0f=未押し, 1.0f=最大)。</summary>
	static float GetLeftTrigger(int index = 0);

	/// <summary>右トリガー (0.0f=未押し, 1.0f=最大)。</summary>
	static float GetRightTrigger(int index = 0);

	/// <summary>
	/// 振動をセットする。
	/// <para>leftMotor  : 低周波モーター(ドスン系) 0.0f〜1.0f</para>
	/// <para>rightMotor : 高周波モーター(ブルブル系) 0.0f〜1.0f</para>
	/// </summary>
	static void SetVibration(float leftMotor, float rightMotor, int index = 0);

	/// <summary>
	/// 振動を止める。
	/// </summary>
	static void StopVibration(int index = 0);

	// ---- デッドゾーン設定 (0.0f〜1.0f, デフォルトは Microsoft 推奨値) ----
	static void SetLeftStickDeadzone(float deadzone);
	static void SetRightStickDeadzone(float deadzone);
	static void SetTriggerDeadzone(float deadzone);
	static float GetLeftStickDeadzone();
	static float GetRightStickDeadzone();
	static float GetTriggerDeadzone();

	// 入力を検知するウィンドウをセット
	static void SetActiveWindow(HWND hwnd) { GetInstance()->activeHwnd_ = hwnd; }

#ifdef USE_IMGUI
	// マウスがViewportにあるか
	static bool IsMouseInViewport() { return GetInstance()->mouseInViewport_; }
	static void SetMouseInViewport(bool inViewport) { GetInstance()->mouseInViewport_ = inViewport; }
	// Viewportがフォーカスされてるか
	static void SetViewportFocused(bool focused) { GetInstance()->viewportFocused_ = focused; }
	static bool IsViewportFocused() { return GetInstance()->viewportFocused_; }
#endif

private:
	static InputManager* GetInstance();

	// 内部ヘルパー
	void initInternal(HINSTANCE hInstance, HWND hwnd);
	void finalizeInternal();
	void updateInternal();
	static bool IsMouseCapturedByImGui();
	static bool IsKeyboardCapturedByImGui();
	static bool IsActive() { return GetInstance()->activeHwnd_ != nullptr; }

	HWND activeHwnd_ = nullptr;

#ifdef USE_IMGUI
	bool mouseInViewport_ = false; // マウスがViewportにあるか
	bool viewportFocused_ = false; // Viewportがフォーカスされてるか
#endif
	// DirectInput8 インターフェース。キーボードとマウスで共有する。
	Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;

	KeyboardInput keyboard_; // DirectInput キーボード
	MouseInput mouse_;       // DirectInput マウス
	XInputGamepad gamepad_;  // XInput Xbox コントローラー
};