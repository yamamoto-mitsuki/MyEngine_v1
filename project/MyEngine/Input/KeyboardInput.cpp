#include "MyEngine/Input/KeyboardInput.h"

#include <format>
#include <cstring>
#define DIRECTINPUT_VERSION 0x0800

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"


void KeyboardInput::Init(IDirectInput8* directInput, HWND hwnd) {
	MY_ASSERT_MSG(directInput, "DirectInput8インターフェースがnullです");

	HRESULT hr;

	// --- キーボードデバイスの生成 ---
	hr = directInput->CreateDevice(GUID_SysKeyboard, &device_, nullptr);
	if (FAILED(hr)) {
		LogManager::Error(std::format("HRESULT=0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "キーボードデバイスの生成に失敗しました");
	}
	LogManager::Log("CreateDevice Success");

	// --- 入力データ形式のセット ---
	hr = device_->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr)) {
		LogManager::Error(std::format("HRESULT=0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false,"キーボードのデータフォーマット設定に失敗しました");
	}

	// --- 協調レベルのセット ---
	// DISCL_FOREGROUND   : アプリが前面にあるときだけ入力を受け取る
	// DISCL_NONEXCLUSIVE : 他のアプリと入力を共有する
	// DISCL_NOWINKEY     : Windowsキーを無効化する(ゲーム中の誤爆防止)
	hr = device_->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		LogManager::Error(std::format("HRESULT=0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "キーボードの協調レベル設定に失敗しました");
	}

	LogManager::Log("Initialized");
}

void KeyboardInput::Finalize() {
	if (device_) {
		device_->Unacquire();
		device_.Reset();
		LogManager::Log("Finalized");
	}
}

void KeyboardInput::Update() {
	memcpy(statePrev_, state_, sizeof(state_));

	HRESULT hr = device_->Acquire();
	if (SUCCEEDED(hr)) {
		hr = device_->GetDeviceState(sizeof(state_), state_);
		if (FAILED(hr)) {
			memset(state_, 0, sizeof(state_));
		}
	} else {
		memset(state_, 0, sizeof(state_));
	}
}

bool KeyboardInput::IsPressed(uint8_t key) const {
	return (state_[key] & 0x80) != 0;
}

bool KeyboardInput::IsTriggered(uint8_t key) const {
	// 押した瞬間
	return (state_[key] & 0x80) != 0 && (statePrev_[key] & 0x80) == 0;
}

bool KeyboardInput::IsReleased(uint8_t key) const {
	// 離した瞬間
	return (state_[key] & 0x80) == 0 && (statePrev_[key] & 0x80) != 0;
}