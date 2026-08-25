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
// 準備（描画先を張り替えるので、この後に描画先を設定すること）
//=============================================================================
void PostProcess::Prepare(RenderTexture& scene) {
	ApplyGV(); // ImGuiでいじった値がその場で効くように毎フレーム反映する
	source_ = &scene;

	// --- ブルーム：明るい所を滲ませて work_ へ合成する ---
	if (kBloomEnabled != 0) {
		bloom_.Record(scene);
		work_->PreDraw();
		bloom_.Composite(scene, static_cast<float>(work_->GetWidth()), static_cast<float>(work_->GetHeight()));
		work_->PostDraw();
		source_ = work_.get();
	}
}

//=============================================================================
// 最終出力（描画先は呼び出し側が設定済み）
//=============================================================================
void PostProcess::Composite(float width, float height) {
	// 切っていても最終出力として通す。全部0ならただのコピーになる
	lens_.Render(*source_, width, height, kLensEnabled != 0);
}

//=============================================================================
// フレーム末のリセット
//=============================================================================
void PostProcess::ResetSlot() {
	bloom_.ResetSlot();
	lens_.ResetSlot();
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