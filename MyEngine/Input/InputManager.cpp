#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Log/LogManager.h"
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#include <cassert>
#include <format>
#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

// ============================================================
// シングルトン
// ============================================================
InputManager* InputManager::GetInstance() {
	// static ローカル変数は最初の呼び出し時に 1 度だけ初期化される
	static InputManager instance;
	return &instance;
}

// ============================================================
// static 窓口 (外部から呼ぶインターフェース)
// 実処理はすべて GetInstance() 経由でインスタンスメソッドに委譲する
// ============================================================
void InputManager::Init(HINSTANCE hInstance, HWND hwnd) { GetInstance()->initInternal(hInstance, hwnd); }
void InputManager::Finalize() { GetInstance()->finalizeInternal(); }
void InputManager::Update() { GetInstance()->updateInternal(); }

// ---- キーボード ----
bool InputManager::IsKeyPressed(uint8_t key) {
	if (!IsActive()) return false;
	if (IsKeyboardCapturedByImGui()) return false;
	return GetInstance()->keyboard_.IsPressed(key);
}
bool InputManager::IsKeyTriggered(uint8_t key) {
	if (!IsActive()) return false;
	if (IsKeyboardCapturedByImGui()) return false;
	return GetInstance()->keyboard_.IsTriggered(key);
}
bool InputManager::IsKeyReleased(uint8_t key) {
	if (!IsActive()) return false;
	return GetInstance()->keyboard_.IsReleased(key);
}

// ---- マウス ----
bool InputManager::IsMouseButtonPressed(int b) {
	if (!IsActive()) return false;
	if (IsMouseCapturedByImGui()) return false;
	return GetInstance()->mouse_.IsButtonPressed(b);
}
bool InputManager::IsMouseButtonTriggered(int b) {
	if (!IsActive()) return false;
	if (IsMouseCapturedByImGui()) return false;
	return GetInstance()->mouse_.IsButtonTriggered(b);
}
bool InputManager::IsMouseButtonReleased(int b) {
	if (!IsActive()) return false;
	return GetInstance()->mouse_.IsButtonReleased(b);
}
long InputManager::GetMouseDeltaX() {
	if (!IsActive()) return 0;
	if (IsMouseCapturedByImGui()) return 0;
	return GetInstance()->mouse_.GetDeltaX();
}
long InputManager::GetMouseDeltaY() {
	if (!IsActive()) return 0;
	if (IsMouseCapturedByImGui()) return 0;
	return GetInstance()->mouse_.GetDeltaY();
}
long InputManager::GetMouseDeltaWheel() {
	if (!IsActive()) return 0;
	if (IsMouseCapturedByImGui()) return 0;
	return GetInstance()->mouse_.GetDeltaWheel();
}

// ---- Xbox コントローラー ----
bool InputManager::IsGamepadConnected(int i) { return GetInstance()->gamepad_.IsConnected(i); }
bool InputManager::IsButtonPressed(uint16_t btn, int i) { return GetInstance()->gamepad_.IsButtonPressed(btn, i); }
bool InputManager::IsButtonTriggered(uint16_t btn, int i) { return GetInstance()->gamepad_.IsButtonTriggered(btn, i); }
bool InputManager::IsButtonReleased(uint16_t btn, int i) { return GetInstance()->gamepad_.IsButtonReleased(btn, i); }
float InputManager::GetLeftStickX(int i) { return GetInstance()->gamepad_.GetLeftStickX(i); }
float InputManager::GetLeftStickY(int i) { return GetInstance()->gamepad_.GetLeftStickY(i); }
float InputManager::GetRightStickX(int i) { return GetInstance()->gamepad_.GetRightStickX(i); }
float InputManager::GetRightStickY(int i) { return GetInstance()->gamepad_.GetRightStickY(i); }
float InputManager::GetLeftTrigger(int i) { return GetInstance()->gamepad_.GetLeftTrigger(i); }
float InputManager::GetRightTrigger(int i) { return GetInstance()->gamepad_.GetRightTrigger(i); }
void InputManager::SetVibration(float l, float r, int i) { GetInstance()->gamepad_.SetVibration(l, r, i); }
void InputManager::StopVibration(int i) { GetInstance()->gamepad_.StopVibration(i); }

// ---- デッドゾーン ----
void InputManager::SetLeftStickDeadzone(float d) { GetInstance()->gamepad_.SetLeftStickDeadzone(d); }
void InputManager::SetRightStickDeadzone(float d) { GetInstance()->gamepad_.SetRightStickDeadzone(d); }
void InputManager::SetTriggerDeadzone(float d) { GetInstance()->gamepad_.SetTriggerDeadzone(d); }
float InputManager::GetLeftStickDeadzone() { return GetInstance()->gamepad_.GetLeftStickDeadzone(); }
float InputManager::GetRightStickDeadzone() { return GetInstance()->gamepad_.GetRightStickDeadzone(); }
float InputManager::GetTriggerDeadzone() { return GetInstance()->gamepad_.GetTriggerDeadzone(); }

// ============================================================
// 内部ヘルパー
// ============================================================
void InputManager::initInternal(HINSTANCE hInstance, HWND hwnd) {
	LogManager::Log("[InputManager] 初期化開始");

	// --- DirectInput8 インターフェースの生成 ---
	HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput_, nullptr);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[InputManager] DirectInput8Create失敗 HRESULT=0x{:08X}", (uint32_t)hr));
		assert(false && "DirectInput8の初期化に失敗しました");
	}
	LogManager::Log("[InputManager] DirectInput8Create成功");

	// --- 各デバイスの初期化 ---
	keyboard_.Init(directInput_.Get(), hwnd);
	mouse_.Init(directInput_.Get(), hwnd);
	gamepad_.Init();
	LogManager::Log("[InputManager] 初期化完了");
}

void InputManager::finalizeInternal() {
	LogManager::Log("[InputManager] 終了処理開始");
	gamepad_.Finalize();
	mouse_.Finalize();
	keyboard_.Finalize();
	directInput_.Reset();

	LogManager::Log("[InputManager] 終了処理完了");
}

void InputManager::updateInternal() {
	keyboard_.Update();
	mouse_.Update();
	gamepad_.Update();
}

bool InputManager::IsMouseCapturedByImGui() {
#ifdef USE_IMGUI
	return ImGui::GetIO().WantCaptureMouse;
#else
	return false;
#endif
}

bool InputManager::IsKeyboardCapturedByImGui() {
#ifdef USE_IMGUI
	return ImGui::GetIO().WantCaptureKeyboard;
#else
	return false;
#endif
}