#pragma once
#include <cstdint>

#include <Xinput.h>

// ============================================================
// ボタン定数
// ============================================================
namespace GamepadButton {
constexpr uint16_t DpadUp = XINPUT_GAMEPAD_DPAD_UP;
constexpr uint16_t DpadDown = XINPUT_GAMEPAD_DPAD_DOWN;
constexpr uint16_t DpadLeft = XINPUT_GAMEPAD_DPAD_LEFT;
constexpr uint16_t DpadRight = XINPUT_GAMEPAD_DPAD_RIGHT;
constexpr uint16_t Start = XINPUT_GAMEPAD_START;
constexpr uint16_t Back = XINPUT_GAMEPAD_BACK;
constexpr uint16_t LeftThumb = XINPUT_GAMEPAD_LEFT_THUMB;   // 左スティック押し込み
constexpr uint16_t RightThumb = XINPUT_GAMEPAD_RIGHT_THUMB; // 右スティック押し込み
constexpr uint16_t LeftShoulder = XINPUT_GAMEPAD_LEFT_SHOULDER;
constexpr uint16_t RightShoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER;
constexpr uint16_t A = XINPUT_GAMEPAD_A;
constexpr uint16_t B = XINPUT_GAMEPAD_B;
constexpr uint16_t X = XINPUT_GAMEPAD_X;
constexpr uint16_t Y = XINPUT_GAMEPAD_Y;
} 

class XInputGamepad {
public:
	// コントローラーの最大台数
	static constexpr int kMaxControllers = 4;

	/// <summary>初期化。接続済みコントローラーをログに出す。</summary>
	void Init();

	/// <summary>終了処理。全コントローラーの振動を止める。</summary>
	void Finalize();

	/// <summary>毎フレーム呼ぶ。接続状態と入力状態を更新する。</summary>
	void Update();

	// ---- 接続確認 ----

	/// <summary>指定番号のコントローラーが接続されているか。</summary>
	bool IsConnected(int index = 0) const;

	// ---- ボタン (GamepadButton::*** を使う) ----

	/// <summary>指定ボタンが押されているか。</summary>
	bool IsButtonPressed(uint16_t button, int index = 0) const;

	/// <summary>指定ボタンを押した瞬間か。</summary>
	bool IsButtonTriggered(uint16_t button, int index = 0) const;

	/// <summary>指定ボタンを離した瞬間か。</summary>
	bool IsButtonReleased(uint16_t button, int index = 0) const;

	// ---- スティック (-1.0f〜1.0f) ----

	/// <summary>左スティックX軸 (-1.0f=左, 1.0f=右)。</summary>
	float GetLeftStickX(int index = 0) const;

	/// <summary>左スティックY軸 (-1.0f=下, 1.0f=上)。</summary>
	float GetLeftStickY(int index = 0) const;

	/// <summary>右スティックX軸 (-1.0f=左, 1.0f=右)。</summary>
	float GetRightStickX(int index = 0) const;

	/// <summary>右スティックY軸 (-1.0f=下, 1.0f=上)。</summary>
	float GetRightStickY(int index = 0) const;

	// ---- トリガー (0.0f〜1.0f) ----

	/// <summary>左トリガー (0.0f=未押し, 1.0f=最大)。</summary>
	float GetLeftTrigger(int index = 0) const;

	/// <summary>右トリガー (0.0f=未押し, 1.0f=最大)。</summary>
	float GetRightTrigger(int index = 0) const;

	// ---- デッドゾーン設定 (0.0f〜1.0f) ----
	/// <summary>左スティックのデッドゾーンを設定 (0.0f〜1.0f)。</summary>
	void SetLeftStickDeadzone(float deadzone);

	/// <summary>右スティックのデッドゾーンを設定 (0.0f〜1.0f)。</summary>
	void SetRightStickDeadzone(float deadzone);

	/// <summary>トリガーのデッドゾーンを設定 (0.0f〜1.0f)。</summary>
	void SetTriggerDeadzone(float deadzone);

	float GetLeftStickDeadzone() const { return leftStickDeadzone_; }
	float GetRightStickDeadzone() const { return rightStickDeadzone_; }
	float GetTriggerDeadzone() const { return triggerDeadzone_; }

	// ---- 振動 ----

	/// <summary>
	/// 振動をセットする。
	/// leftMotor  : 低周波モーター(ドスン系) 0.0f〜1.0f
	/// rightMotor : 高周波モーター(ブルブル系) 0.0f〜1.0f
	/// </summary>
	void SetVibration(float leftMotor, float rightMotor, int index = 0);

	/// <summary>振動を止める。</summary>
	void StopVibration(int index = 0);

private:
	/// <summary>
	/// int16_t の生値をデッドゾーン処理して -1.0f〜1.0f に正規化する。
	/// デッドゾーン内はゼロを返す。
	/// </summary>
	float ApplyDeadzone(int16_t value, float deadzone) const;

	// デッドゾーン (0.0f〜1.0f)。Microsoft 推奨値をデフォルトに使用。
	float leftStickDeadzone_ = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.0f;   // ≒0.239f
	float rightStickDeadzone_ = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.0f; // ≒0.265f
	float triggerDeadzone_ = XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 255.0f;         // ≒0.118f

	// コントローラー1台分の状態をまとめた構造体
	struct ControllerState {
		XINPUT_STATE state = {};     // 今フレームの入力状態
		XINPUT_STATE statePrev = {}; // 前フレームの入力状態(Triggered/Released判定に使う)
		bool connected = false;
	};
	ControllerState controllers_[kMaxControllers] = {};
};