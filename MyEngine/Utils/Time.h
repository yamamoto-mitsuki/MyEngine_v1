#pragma once

// 時間計測クラス
class Time {
public:
	static void Initialize() { instance_ = new Time(); }
	static void Release() { 
		delete instance_;
		instance_ = nullptr;
	};
	// ゲッター
	static float GetDeltaTime() { return instance_->deltaTime_ * instance_->timeScale_; }
	static float GetUnscaleDeltaTime() { return instance_->deltaTime_; }
	static float GetTimeScale() { return instance_->timeScale_; }
	// セッター
	static void SetDeltaTime(float time) { instance_->deltaTime_ = time; }
	static void SetTimeScale(float scale) { instance_->timeScale_ = scale; }

private:
	static inline Time* instance_ = nullptr;
	float deltaTime_ = 0.0f;
	float timeScale_ = 1.0f;
};