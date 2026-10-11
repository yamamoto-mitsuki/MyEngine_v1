#pragma once
#include <array>
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"


/// <summary>
/// ブルーム。明るい部分を抜き出して段階的にぼかし、元の絵に加算する
/// </summary>
class BloomPass {
public:
	static constexpr uint32_t kLevelCount = 4;       // 縮小の段数
	static constexpr uint32_t kMaxPassPerFrame = 32; // 1フレームに積むパス数の上限

	// --- 調整項目 ---
	float kThreshold = 0.75f; // これより明るい所だけ光る
	float kKnee = 0.25f;      // しきい値付近の柔らかさ
	float kIntensity = 0.9f;  // 合成の強さ

	float addIntensity = 0.0f;
	float addThreshold = 0.0f; // 負の値でしきい値が下がる＝もっと光る

	void Initialize(uint32_t width, uint32_t height);

	/// <summary>
	/// シーンの絵から、ぼかした光の層を作る
	/// </summary>
	void Record(RenderTexture& scene);

	/// <summary>
	/// 今バインドされている描画先へ、シーンと光を合成して描く
	/// </summary>
	void Composite(RenderTexture& scene, float width, float height);

	/// <summary>
	/// フレーム末に定数バッファの参照位置を戻す
	/// </summary>
	void ResetSlot() { slot_ = 0; }

private:
	// シェーダーの BloomParam と同じ並びにすること
	struct Param {
		float texelSize[2] = {0.0f, 0.0f};
		float threshold = 0.0f;
		float knee = 1.0f;
		float intensity = 1.0f;
		uint32_t srcIndex = 0;
		uint32_t addIndex = 0;
		float padding = 0.0f;
	};
	static constexpr uint32_t kSlotSize = 256;

	void CreateRootSignature();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const char* psName);
	void BindAndDraw(ID3D12PipelineState* pso, const Param& param);

	RootSignatureInfo rsInfo_{};
	Microsoft::WRL::ComPtr<ID3D12PipelineState> brightPSO_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> downPSO_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> upPSO_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePSO_;
	std::array<std::unique_ptr<RenderTexture>, kLevelCount> down_;
	std::array<std::unique_ptr<RenderTexture>, kLevelCount> up_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_;
	uint8_t* cbMapped_ = nullptr;
	uint32_t slot_ = 0;
};
