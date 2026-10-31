#pragma once
#include "MyEngine/Graphics/PostEffect/BloomPass.h"
#include "MyEngine/Graphics/PostEffect/DepthOfFieldPass.h"
#include "MyEngine/Graphics/PostEffect/LensPass.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include <memory>

// 前方宣言
class Camera;

// ポストエフェクトをどこまで掛けるか（SceneRenderer::RenderToTexture に渡す）
enum class PostEffectMode {
	None,   // 掛けない（被写界深度・ブルーム・レンズを外す。露出・トーンマップだけは掛ける）
	All,    // 全部（Game ビュー・製品版）
	NoLens, // レンズ（歪み・色収差・周辺を暗く）だけ外す。Scene ビュー用（絵を歪ませると、ギズモやクリックで選ぶ位置が物とずれる）
};


/// <summary>
/// ポストエフェクトをまとめて順に掛ける（被写界深度 → ブルーム → レンズ・露出・トーンマップ）。ブルーム・レンズ・トーンマップの有無と数値は GlobalVariables で持つ
/// <para>被写界深度の値はシーンの Depth Of Field Component から（ApplyDepthOfField に渡す）</para>
/// <para>途中の絵は全部 HDR。画面（SDR）の 0〜1 に収めるのは、最後のレンズのパス（LensPass）</para>
/// </summary>
class PostProcess {
public:
	void Initialize(uint32_t width, uint32_t height);

	/// <summary>
	/// 被写界深度を掛ける（3D の途中。不透明な物と空を描いた後、半透明を描く前に呼ぶ）
	/// </summary>
	/// <param name="scene">不透明な物と空を描いた絵（深度を読める RenderTexture。PostDraw 済み）</param>
	/// <returns>ぼかした絵。描画先として張ったまま（深度は scene の物）なので、この後に半透明を描いて PostDraw する</returns>
	RenderTexture* ApplyDepthOfField(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings);

	void Prepare(RenderTexture& scene, bool useBloom = true);       // useBloom が false ならブルームを掛けない（ポストエフェクトを切ったビュー）
	void Composite(const ViewRect& rect, bool useLens = true); // useLens が false ならレンズを掛けずに、露出・トーンマップだけ掛けて通す
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
	DepthOfFieldPass depthOfField_;
	BloomPass bloom_;
	LensPass lens_;
	std::unique_ptr<RenderTexture> depthOfFieldRT_; // 被写界深度の合成の置き場（HDR）
	std::unique_ptr<RenderTexture> work_;           // ブルーム合成の置き場（HDR）
};