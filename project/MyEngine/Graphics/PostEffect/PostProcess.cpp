#include "PostProcess.h"
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Diagnostics/LogManager.h"

//=============================================================================
// 初期化
//=============================================================================
void PostProcess::Initialize(uint32_t width, uint32_t height) {
	// どちらも HDR（シーンの絵のまま、1 を超える明るさを残す。画面の 0〜1 に収めるのは最後のレンズのパス）
	depthOfFieldRT_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::HDR, false);
	work_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::HDR, false);
	depthOfField_.Initialize(width, height);
	bloom_.Initialize(width, height);
	lens_.Initialize();
	RegisterGV();
	GlobalVariables::GetInstance()->LoadGroup("PostEffect"); // 保存済みがあれば読む
	ApplyGV();
	LogManager::Log(std::format("PostEffect Bloom={} Lens={} Threshold={:.2f} Intensity={:.2f} Vignette={:.2f} ToneMap={} Exposure={:.2f}", kBloomEnabled, kLensEnabled, bloom_.kThreshold, bloom_.kIntensity,
	        lens_.kVignette, static_cast<int>(lens_.toneMap), lens_.kExposure));
}

//=============================================================================
// 準備（描画先を張り替えるので、この後に描画先を設定すること）
//=============================================================================
void PostProcess::Prepare(RenderTexture& scene, bool useBloom) {
	ApplyGV(); // ImGuiでいじった値がその場で効くように毎フレーム反映する
	source_ = &scene;

	// --- ブルーム：明るい所を滲ませて work_ へ合成する ---
	if (useBloom && kBloomEnabled != 0) {
		bloom_.Record(scene);
		work_->PreDraw();
		bloom_.Composite(scene, static_cast<float>(work_->GetWidth()), static_cast<float>(work_->GetHeight()));
		work_->PostDraw();
		source_ = work_.get();
	}
}

//=============================================================================
// 被写界深度（3D の途中で掛ける）
//=============================================================================
// ブルームより先（ぼけた明るい所が、ぼけた形のまま光るように）
RenderTexture* PostProcess::ApplyDepthOfField(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings) {
	depthOfField_.Record(scene, camera, settings);
	depthOfFieldRT_->PreDraw();
	depthOfField_.Composite(static_cast<float>(depthOfFieldRT_->GetWidth()), static_cast<float>(depthOfFieldRT_->GetHeight()));
	// この後に描く半透明が、不透明な物の後ろに隠れるように、scene の深度を張る（Composite の中で、深度は書く状態に戻っている）
	depthOfFieldRT_->BindWithDepthOf(scene);
	return depthOfFieldRT_.get();
}

//=============================================================================
// 最終出力（描画先は呼び出し側が設定済み）
//=============================================================================
void PostProcess::Composite(float width, float height, bool useLens) {
	// 切っていても最終出力として通す（HDR の絵を、露出・トーンマップで画面の 0〜1 に収めるのもここ）
	lens_.Render(*source_, width, height, useLens && kLensEnabled != 0);
}

//=============================================================================
// フレーム末のリセット
//=============================================================================
void PostProcess::ResetSlot() {
	depthOfField_.ResetSlot();
	bloom_.ResetSlot();
	lens_.ResetSlot();
}

//=============================================================================
// 調整項目
//=============================================================================
void PostProcess::RegisterGV() {
	auto post = GlobalVariables::GetInstance()->Group("PostEffect");

	post.Category("Bloom")
	    .Add("Enabled", kBloomEnabled)
	    .Add("Threshold", bloom_.kThreshold)
	    .Add("Knee", bloom_.kKnee)
	    .Add("Intensity", bloom_.kIntensity)
	    // ぼかし方（Dual：今までの／Kawase：川瀬式）と、川瀬式の段ごとの重み（Weight1 が 1/4 の段 … Weight4 が 1/32 の段）
	    .Add("Mode", GlobalVariables::MakeEnumCombo(bloom_.mode))
	    .Add("Weight1", bloom_.kKawaseWeights[0])
	    .Add("Weight2", bloom_.kKawaseWeights[1])
	    .Add("Weight3", bloom_.kKawaseWeights[2])
	    .Add("Weight4", bloom_.kKawaseWeights[3]);
	post.Category("Lens")
		.Add("Enabled", kLensEnabled)
		.Add("Distortion", lens_.kDistortion)
		.Add("Aberration", lens_.kAberration)
		.Add("RadialBlur", lens_.kRadialBlur)
		.Add("Vignette", lens_.kVignette);
	// 露出・トーンマップ（HDR の絵 → 画面）。Enabled は無い（3D の絵は HDR なので、いつも掛ける。None なら 1 で切るだけ＝今までの見た目）
	post.Category("ToneMap")
		.Add("Mode", GlobalVariables::MakeEnumCombo(lens_.toneMap))
		.Add("Exposure", lens_.kExposure);
}

void PostProcess::ApplyGV() {
	auto gv = GlobalVariables::GetInstance();
	auto s = "PostEffect";
	kBloomEnabled = gv->Get<int32_t>(s, "Bloom", "Enabled");
	bloom_.kThreshold = gv->Get<float>(s, "Bloom", "Threshold");
	bloom_.kKnee = gv->Get<float>(s, "Bloom", "Knee");
	bloom_.kIntensity = gv->Get<float>(s, "Bloom", "Intensity");
	bloom_.mode = GlobalVariables::GetEnumCombo<BloomMode>(gv->Get<GlobalVariables::ComboItem>(s, "Bloom", "Mode"));
	bloom_.kKawaseWeights[0] = gv->Get<float>(s, "Bloom", "Weight1");
	bloom_.kKawaseWeights[1] = gv->Get<float>(s, "Bloom", "Weight2");
	bloom_.kKawaseWeights[2] = gv->Get<float>(s, "Bloom", "Weight3");
	bloom_.kKawaseWeights[3] = gv->Get<float>(s, "Bloom", "Weight4");

	kLensEnabled = gv->Get<int32_t>(s, "Lens", "Enabled");
	lens_.kDistortion = gv->Get<float>(s, "Lens", "Distortion");
	lens_.kAberration = gv->Get<float>(s, "Lens", "Aberration");
	lens_.kRadialBlur = gv->Get<float>(s, "Lens", "RadialBlur");
	lens_.kVignette = gv->Get<float>(s, "Lens", "Vignette");

	lens_.toneMap = GlobalVariables::GetEnumCombo<ToneMapMode>(gv->Get<GlobalVariables::ComboItem>(s, "ToneMap", "Mode"));
	lens_.kExposure = gv->Get<float>(s, "ToneMap", "Exposure");
}