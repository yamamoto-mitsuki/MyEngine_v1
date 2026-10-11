#pragma once
#include <memory>
#include "MyEngine/Graphics/PostEffect/LensPass.h"
#include "MyEngine/Graphics/PostEffect/BloomPass.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

// ポストエフェクトをどこまで掛けるか（SceneRenderer::RenderToTexture に渡す）
enum class PostEffectMode {
	None,   // 掛けない
	All,    // 全部（Game ビュー・製品版）
	NoLens, // レンズ（歪み・色収差・周辺を暗く）だけ外す。Scene ビュー用（絵を歪ませると、ギズモやクリックで選ぶ位置が物とずれる）
};

/// <summary>
/// ポストエフェクトをまとめて順に掛ける。効果ごとの有無と数値は GlobalVariables で持つ
/// </summary>
class PostProcess {
public:
	void Initialize(uint32_t width, uint32_t height);
	void Prepare(RenderTexture& scene);
	void Composite(float width, float height, bool useLens = true); // useLens が false ならレンズを掛けずに通す（Scene ビュー）
	void ResetSlot();
	BloomPass& GetBloomPass() { return bloom_; }
	LensPass& GetLensPass() { return lens_; }

private:
	// --- 調整項目 ---
	int kBloomEnabled = 1; // GlobalVariables は bool を持たないので 0/1 で扱う
	int kLensEnabled = 1;

	void RegisterGV();
	void ApplyGV();

	RenderTexture* source_ = nullptr; // Prepare が決めた、レンズへの入力
	BloomPass bloom_;
	LensPass lens_;
	std::unique_ptr<RenderTexture> work_; // ブルーム合成の置き場
};
