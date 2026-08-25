#pragma once
#include <memory>
#include "MyEngine/Graphics/PostEffect/LensPass.h"
#include "MyEngine/Graphics/PostEffect/BloomPass.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"


/// <summary>
/// ポストエフェクトをまとめて順に掛ける。効果ごとの有無と数値は GlobalVariables で持つ
/// </summary>
class PostProcess {
public:
	void Initialize(uint32_t width, uint32_t height);
	void Render(RenderTexture& scene, float width, float height);
	void ResetSlot();
	BloomPass& GetBloomPass() { return bloom_; }
	LensPass& GetLensPass() { return lens_; }

private:
	// --- 調整項目 ---
	int kBloomEnabled = 1; // GlobalVariables は bool を持たないので 0/1 で扱う
	int kLensEnabled = 1;

	void RegisterGV();
	void ApplyGV();

	BloomPass bloom_;
	LensPass lens_;
	std::unique_ptr<RenderTexture> work_; // ブルーム合成の置き場
};
