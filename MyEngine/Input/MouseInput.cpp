#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Input/MouseInput.h"
#include <cassert>
#include <cstring>
#include <format>

void MouseInput::Init(IDirectInput8* directInput, HWND hwnd) {
	assert(directInput && "DirectInput8インターフェースがnullです");
	assert(hwnd && "HWNDがnullです");

	hwnd_ = hwnd;

	HRESULT hr;

	// --- マウスデバイスの生成 ---
	hr = directInput->CreateDevice(GUID_SysMouse, &device_, nullptr);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[MouseInput] CreateDevice失敗 HRESULT=0x{:08X}", (uint32_t)hr));
		assert(false && "マウスデバイスの生成に失敗しました");
	}
	LogManager::Log("[MouseInput] CreateDevice成功");

	// --- 入力データ形式のセット ---
	hr = device_->SetDataFormat(&c_dfDIMouse2);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[MouseInput] SetDataFormat失敗 HRESULT=0x{:08X}", (uint32_t)hr));
		assert(false && "マウスのデータフォーマット設定に失敗しました");
	}

	// --- 協調レベルのセット ---
	hr = device_->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[MouseInput] SetCooperativeLevel失敗 HRESULT=0x{:08X}", (uint32_t)hr));
		assert(false && "マウスの協調レベル設定に失敗しました");
	}

	LogManager::Log("[MouseInput] 初期化完了");
}

void MouseInput::Finalize() {
	if (device_) {
		device_->Unacquire();
		device_.Reset();
		LogManager::Log("[MouseInput] 終了処理完了");
	}
}

void MouseInput::Update() {
	statePrev_ = state_;

	HRESULT hr = device_->Acquire();
	if (SUCCEEDED(hr)) {
		hr = device_->GetDeviceState(sizeof(DIMOUSESTATE2), &state_);
		if (FAILED(hr)) {
			memset(&state_, 0, sizeof(state_));
		}
	} else {
		memset(&state_, 0, sizeof(state_));
	}

	// --- クライアント座標でのマウス現在位置を取得 ---
	// DirectInput は相対値(移動量)しか返さないため Win32 API を使う。
	// GetCursorPos  : スクリーン座標(モニター左上が原点)でカーソル位置を取得
	// ScreenToClient: スクリーン座標 → 指定ウィンドウのクライアント座標に変換
	POINT p{};
	if (GetCursorPos(&p) && ScreenToClient(hwnd_, &p)) {
		posX_ = p.x;
		posY_ = p.y;
	}
}

bool MouseInput::IsButtonPressed(int button) const {
	if (button < 0 || button >= 8) {
		return false;
	}

	return (state_.rgbButtons[button] & 0x80) != 0;
}

bool MouseInput::IsButtonTriggered(int button) const {
	if (button < 0 || button >= 8) {
		return false;
	}
	return (state_.rgbButtons[button] & 0x80) != 0 && (statePrev_.rgbButtons[button] & 0x80) == 0;
}

bool MouseInput::IsButtonReleased(int button) const {
	if (button < 0 || button >= 8) {
		return false;
	}
	return (state_.rgbButtons[button] & 0x80) == 0 && (statePrev_.rgbButtons[button] & 0x80) != 0;
}