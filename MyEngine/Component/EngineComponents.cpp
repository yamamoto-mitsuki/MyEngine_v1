#include "MyEngine/Component/ComponentSerializer.h"

#include <algorithm>
#include <cstdint>

#include "MyEngine/Entity/ModelRendererComponent.h"
#include "MyEngine/Entity/TransformComponent.h"
#include "MyEngine/Light/LightComponent.h"

// エンジンのComponentの「保存する項目」。書き方は COMPONENT(...) の中身と同じ
// Inspectorは Editor/Inspector の手書きのEditorのまま（度で見せる・Resetボタンなど、細かく作り込んでいるため）
// ここは保存の名前を決めるだけなので、ラベルはメンバ変数の名前にそろえる
// Componentに項目を足したら、ここにも1行足す（足し忘れると、その項目は保存されず、Stopで初期値に戻る）

namespace {
//=============================================================================
// Transform
//=============================================================================
void DescribeTransform(ComponentUI& ui, TransformComponent& value) {
	ui.Field("translation", value.translation);
	ui.Field("rotation", value.rotation); // ラジアン（X→Y→Zの順）。度に直して書くと、保存するたびに誤差が乗る
	ui.Field("scale", value.scale);
	// worldMatrix は毎フレーム計算する結果なので書かない
}

//=============================================================================
// ModelRenderer
//=============================================================================
// 0xRRGGBBAA → 0〜1の4つ（色の Field は Vector4 なので、保存するときだけ写す）
Vector4 UnpackColor(uint32_t rgba) {
	return {
	    static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(rgba & 0xFF) / 255.0f,
	};
}

// 0〜1の4つ → 0xRRGGBBAA（8bitなので、写して戻しても値は変わらない）
uint32_t PackColor(const Vector4& color) {
	// ファイルを手で書き換えて1を超えても、隣の色にはみ出さないように0〜1に収める
	const auto toByte = [](float value) { return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return (toByte(color.x) << 24) | (toByte(color.y) << 16) | (toByte(color.z) << 8) | toByte(color.w);
}

void DescribeModelRenderer(ComponentUI& ui, ModelRendererComponent& value) {
	ui.Field("enabled", value.enabled);
	// --- 使うファイル（パスで書く。番号は起動するたびに変わる）---
	ui.AssetField("model", value.modelHandle, AssetType::Model);
	ui.AssetField("texture", value.textureHandle, AssetType::Texture); // 未選択＝モデルのマテリアルのテクスチャ
	// --- 描画の設定（enumは名前で書く）---
	ui.Field("shadingType", value.shadingType);
	ui.Field("blendMode", value.blendMode);
	ui.Field("rasterizerType", value.rasterizerType);
	ui.Field("depthMode", value.depthMode);
	ui.Field("billboard", value.billboard);
	// --- 色 ---
	Vector4 color = UnpackColor(value.color);
	ui.ColorField("color", color);
	value.color = PackColor(color);
	// --- マテリアル ---
	ui.ColorField("ambient", value.material.ambient);
	ui.ColorField("diffuse", value.material.diffuse);
	ui.ColorField("specular", value.material.specular);
	ui.ColorField("emissive", value.material.emissive);
	ui.Field("shininess", value.material.shininess);
	ui.Field("metallic", value.material.metallic);
	ui.Field("roughness", value.material.roughness);
	ui.Field("alphaCutoff", value.material.alphaCutoff);
	// --- UV ---
	ui.Field("uvTranslation", value.uvTransform.translation);
	ui.Field("uvRotation", value.uvTransform.rotation);
	ui.Field("uvScale", value.uvTransform.scale);
}

//=============================================================================
// ライト
//=============================================================================
// ギズモの表示（3種類で共通）
void DescribeGizmo(ComponentUI& ui, LightGizmoFlags& gizmo) {
	ui.Field("showIcon", gizmo.showIcon);
	ui.Field("showRange", gizmo.showRange);
}

void DescribeDirectionalLight(ComponentUI& ui, DirectionalLightComponent& value) {
	ui.ColorField("color", value.color); // 見た目の色（sRGB）。リニアへの変換は LightSystem が集めるときに行う
	ui.Field("intensity", value.intensity);
	DescribeGizmo(ui, value.gizmo);
}

void DescribePointLight(ComponentUI& ui, PointLightComponent& value) {
	ui.ColorField("color", value.color);
	ui.Field("intensity", value.intensity);
	ui.Field("radius", value.radius);
	ui.Field("decay", value.decay);
	DescribeGizmo(ui, value.gizmo);
}

void DescribeSpotLight(ComponentUI& ui, SpotLightComponent& value) {
	ui.ColorField("color", value.color);
	ui.Field("intensity", value.intensity);
	ui.Field("range", value.range);
	ui.Field("decay", value.decay);
	ui.Field("outerAngle", value.outerAngle); // 度
	ui.Field("innerAngle", value.innerAngle); // 度
	DescribeGizmo(ui, value.gizmo);
}
} // namespace

//=============================================================================
// エンジンのComponentを登録する（Engine::Initialize から1回。Releaseでも呼ぶ）
// 名前はInspectorの見出しと同じにしておく（ファイルを開いたときに、どのComponentか分かるように）
//=============================================================================
void ComponentSerializer::RegisterEngineComponents() {
	Register<TransformComponent>("Transform", &DescribeTransform);
	Register<ModelRendererComponent>("Model Renderer", &DescribeModelRenderer);
	Register<DirectionalLightComponent>("Directional Light", &DescribeDirectionalLight);
	Register<PointLightComponent>("Point Light", &DescribePointLight);
	Register<SpotLightComponent>("Spot Light", &DescribeSpotLight);
}