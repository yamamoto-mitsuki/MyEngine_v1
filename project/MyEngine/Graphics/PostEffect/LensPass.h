#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"


/// <summary>
/// レンズの効果。画面中心からの距離で、歪み・色収差・放射ブラー・ビネットを掛ける。
/// 全部0なら単なるコピーになるので、最終出力のブリットも兼ねる
/// </summary>
class LensPass {
public:
	// 1フレームに描くビュー数の上限（Game と Scene で2回通る）
	static constexpr uint32_t kMaxPassPerFrame = 8;

	// --- 調整項目（GlobalVariables から入る基準値）---
	float kDistortion = 0.0f; // 画面端の歪み。0.2で気づく程度
	float kAberration = 0.0f; // 色収差。0.02で十分
	float kRadialBlur = 0.0f; // 中心へ向かって流れるブラー。0.10で強め
	float kVignette = 0.35f;  // 端の暗さ。常時少し掛けておくと締まる

	// --- 演出用の上乗せ。毎フレーム書かないと0に戻る ---
	// 掛けっぱなしになる事故を防ぐため、Render() の最後に自分で消す
	float addDistortion = 0.0f;
	float addAberration = 0.0f;
	float addRadialBlur = 0.0f;
	float addVignette = 0.0f;

	void Initialize();

	/// <summary>
	/// source を読んで、今バインドされている描画先へ描く
	/// </summary>
	/// <param name="enabled">false なら効果を掛けずにコピーだけする</param>
	void Render(RenderTexture& source, float width, float height, bool enabled);

	/// <summary>
	/// フレーム末にスロットを戻す
	/// </summary>
	void ResetSlot() { slot_ = 0; }

private:
	// シェーダーの LensParam と同じ並びにすること
	struct Param {
		float distortion; // 画面端の歪み。正で外へ膨らむ
		float aberration; // 色収差。端ほどRGBがずれる
		float radialBlur; // 中心へ向かって流れるブラー
		float vignette;   // 端の暗さ
		uint32_t srcIndex = 0;
		float padding[3] = {};
	};
	static constexpr uint32_t kSlotSize = 256;

	void CreateRootSignature();
	void CreatePSO();

	RootSignatureInfo rsInfo_{};
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_;
	uint8_t* cbMapped_ = nullptr;
	uint32_t slot_ = 0;
};