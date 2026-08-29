#include "AccelerationField.h"

#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"


//=============================================================================
// 初期化
//=============================================================================
void AccelerationField::Initialize() {
	// デフォルトの宣言（minを入れ忘れると判定も描画も壊れるので必ず両方入れる）
	acceleration = {15.0f, 0.0f, 0.0f};
	aabb.min = {-2.0f, -2.0f, -2.0f};
	aabb.max = {2.0f, 2.0f, 2.0f};
	isActive = true;
	// 描画用宣言
	drawConfig_.shadingType = ShadingType::Unlit;
	drawConfig_.rasterizerType = RasterizerType::Wireframe;
}


//=============================================================================
// 効果の適用
//=============================================================================
void AccelerationField::Apply(Particle& particle, float deltaTime) {
	if (!isActive) {
		return;
	}
	// 範囲の中にいる粒だけ加速させる
	if (Collision::IsPointInAABB(particle.transform.translation, aabb)) {
		particle.velocity += acceleration * deltaTime;
	}
}


//=============================================================================
// 描画
//=============================================================================
void AccelerationField::Draw() {
	drawConfig_.aabb = aabb;
	Renderer::DrawAABB(drawConfig_);
}
