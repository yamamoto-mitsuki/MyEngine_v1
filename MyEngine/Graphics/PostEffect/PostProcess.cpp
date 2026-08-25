#include "PostProcess.h"
#include "MyEngine/UI/GlobalVariables.h"

//=============================================================================
// 初期化
//=============================================================================
void PostProcess::Initialize(uint32_t width, uint32_t height) {
	work_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::SDR, false);
	bloom_.Initialize(width, height);
	lens_.Initialize();
	RegisterGV();
	GlobalVariables::GetInstance()->LoadGroup("PostEffect"); // 保存済みがあれば読む
	ApplyGV();
}


//=============================================================================
// scene を入力に、全効果を掛けた結果を今の描画先へ出す
//=============================================================================
void PostProcess::Render(RenderTexture& scene, float width, float height) {
	ApplyGV(); // 調整項目を反映
	RenderTexture* source = &scene;

	// --- ブルーム：中間テクスチャへ合成する ---
	if (kBloomEnabled != 0) {
		bloom_.Record(scene);
		work_->PreDraw();
		bloom_.Composite(scene, static_cast<float>(work_->GetWidth()), static_cast<float>(work_->GetHeight()));
		work_->PostDraw();
		source = work_.get();
	}
	// --- レンズ：描画先は呼び出し側が設定済み。切っていてもコピーとして通す ---
	lens_.Render(*source, width, height, kLensEnabled != 0);
}


//=============================================================================
// 調整項目
//=============================================================================
void PostProcess::RegisterGV() {
	auto post = GlobalVariables::GetInstance()->Group("PostEffect");

	post.Category("Bloom").Add("Enabled", kBloomEnabled).Add("Threshold", bloom_.kThreshold).Add("Knee", bloom_.kKnee).Add("Intensity", bloom_.kIntensity);

	post.Category("Lens").Add("Enabled", kLensEnabled).Add("Distortion", lens_.kDistortion).Add("Aberration", lens_.kAberration).Add("RadialBlur", lens_.kRadialBlur).Add("Vignette", lens_.kVignette);
}

void PostProcess::ApplyGV() {
	auto gv = GlobalVariables::GetInstance();
	auto s = "PostEffect";
	kBloomEnabled = gv->Get<int32_t>(s, "Bloom", "Enabled");
	bloom_.kThreshold = gv->Get<float>(s, "Bloom", "Threshold");
	bloom_.kKnee = gv->Get<float>(s, "Bloom", "Knee");
	bloom_.kIntensity = gv->Get<float>(s, "Bloom", "Intensity");

	kLensEnabled = gv->Get<int32_t>(s, "Lens", "Enabled");
	lens_.kDistortion = gv->Get<float>(s, "Lens", "Distortion");
	lens_.kAberration = gv->Get<float>(s, "Lens", "Aberration");
	lens_.kRadialBlur = gv->Get<float>(s, "Lens", "RadialBlur");
	lens_.kVignette = gv->Get<float>(s, "Lens", "Vignette");
}