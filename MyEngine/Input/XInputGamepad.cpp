#define NOMINMAX
#include "MyEngine/Input/XInputGamepad.h"

#include <format>
#include <algorithm>

#include "MyEngine/Diagnostics/LogManager.h"

#pragma comment(lib, "xinput.lib")


void XInputGamepad::Init() {
	for (int i = 0; i < kMaxControllers; ++i) {
		memset(&controllers_[i].state, 0, sizeof(XINPUT_STATE));
		memset(&controllers_[i].statePrev, 0, sizeof(XINPUT_STATE));
		controllers_[i].connected = false;
	}

	for (int i = 0; i < kMaxControllers; ++i) {
		XINPUT_STATE tmp{};
		if (XInputGetState(i, &tmp) == ERROR_SUCCESS) {
			LogManager::Log(std::format("コントローラー{}番 起動時に接続確認", i));
		}
	}
	LogManager::Log("Initialized");
}

void XInputGamepad::Finalize() {
	// 終了時に振動が残らないよう全コントローラーを停止する
	for (int i = 0; i < kMaxControllers; ++i) {
		if (controllers_[i].connected) {
			StopVibration(i);
		}
	}
	LogManager::Log("Finalized");
}

void XInputGamepad::Update() {
	for (int i = 0; i < kMaxControllers; ++i) {
		// 前フレームの状態を保存してから今フレームを取得する
		controllers_[i].statePrev = controllers_[i].state;

		DWORD result = XInputGetState(i, &controllers_[i].state);
		bool nowConnected = (result == ERROR_SUCCESS);

		if (nowConnected != controllers_[i].connected) {
			if (nowConnected) {
				LogManager::Log(std::format("[XInputGamepad] コントローラー{}番 接続", i));
			} else {
				LogManager::Log(std::format("[XInputGamepad] コントローラー{}番 切断", i));
				memset(&controllers_[i].state, 0, sizeof(XINPUT_STATE));
			}
		}
		controllers_[i].connected = nowConnected;
	}
}

bool XInputGamepad::IsConnected(int index) const {
	if (index < 0 || index >= kMaxControllers) {
		return false;
	}
	return controllers_[index].connected;
}

bool XInputGamepad::IsButtonPressed(uint16_t button, int index) const {
	if (!IsConnected(index)) {
		return false;
	}
	return (controllers_[index].state.Gamepad.wButtons & button) != 0;
}

bool XInputGamepad::IsButtonTriggered(uint16_t button, int index) const {
	if (!IsConnected(index)) {
		return false;
	}
	bool now = (controllers_[index].state.Gamepad.wButtons & button) != 0;
	bool prev = (controllers_[index].statePrev.Gamepad.wButtons & button) != 0;
	return now && !prev;
}

bool XInputGamepad::IsButtonReleased(uint16_t button, int index) const {
	if (!IsConnected(index)) {
		return false;
	}
	bool now = (controllers_[index].state.Gamepad.wButtons & button) != 0;
	bool prev = (controllers_[index].statePrev.Gamepad.wButtons & button) != 0;
	return !now && prev;
}

float XInputGamepad::GetLeftStickX(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	return ApplyDeadzone(controllers_[index].state.Gamepad.sThumbLX, leftStickDeadzone_);
}

float XInputGamepad::GetLeftStickY(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	return ApplyDeadzone(controllers_[index].state.Gamepad.sThumbLY, leftStickDeadzone_);
}

float XInputGamepad::GetRightStickX(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	return ApplyDeadzone(controllers_[index].state.Gamepad.sThumbRX, rightStickDeadzone_);
}

float XInputGamepad::GetRightStickY(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	return ApplyDeadzone(controllers_[index].state.Gamepad.sThumbRY, rightStickDeadzone_);
}

float XInputGamepad::GetLeftTrigger(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	// トリガーは 0〜255 の BYTE 値。255.0f で割って 0.0f〜1.0f に正規化する。
	float val = controllers_[index].state.Gamepad.bLeftTrigger / 255.0f;
	// デッドゾーン以下はゼロ扱い
	return (val > triggerDeadzone_) ? val : 0.0f;
}

float XInputGamepad::GetRightTrigger(int index) const {
	if (!IsConnected(index)) {
		return 0.0f;
	}
	float val = controllers_[index].state.Gamepad.bRightTrigger / 255.0f;
	return (val > triggerDeadzone_) ? val : 0.0f;
}

void XInputGamepad::SetLeftStickDeadzone(float deadzone) {
	// 0.0f〜1.0f にクランプしてから保存する
	leftStickDeadzone_ = std::max(0.0f, std::min(1.0f, deadzone));
}

void XInputGamepad::SetRightStickDeadzone(float deadzone) {
	rightStickDeadzone_ = std::max(0.0f, std::min(1.0f, deadzone));
}

void XInputGamepad::SetTriggerDeadzone(float deadzone) {
	triggerDeadzone_ = std::max(0.0f, std::min(1.0f, deadzone));
}

void XInputGamepad::SetVibration(float leftMotor, float rightMotor, int index) {
	if (!IsConnected(index)) {
		return;
	}
	// 0.0f〜1.0f を 0〜65535に変換して XInputSetState に渡す
	auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
	XINPUT_VIBRATION vib = {};
	vib.wLeftMotorSpeed = static_cast<WORD>(clamp01(leftMotor) * 65535.0f);
	vib.wRightMotorSpeed = static_cast<WORD>(clamp01(rightMotor) * 65535.0f);
	DWORD result = XInputSetState(index, &vib);
}

void XInputGamepad::StopVibration(int index) {
	SetVibration(0.0f, 0.0f, index);
}

float XInputGamepad::ApplyDeadzone(int16_t value, float deadzone) const {
	// int16_t(-32768〜32767) を -1.0f〜1.0f に正規化してからデッドゾーン判定する。
	// int16_t のまま比較すると float のデッドゾーン値と単位が合わないため先に正規化する。
	float normalized = std::max(-1.0f, std::min(1.0f, static_cast<float>(value) / 32767.0f));
	// デッドゾーン内はゼロ扱い
	if (normalized > -deadzone && normalized < deadzone) {
		return 0.0f;
	}
	return normalized;
}