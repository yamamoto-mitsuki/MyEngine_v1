#include "DefaultScene.h"

#include <numbers>

#include "MyEngine/Components/CameraComponent.h"
#include "MyEngine/Components/LightComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Scene/Skybox.h"
#include "MyEngine/Systems/ModelRenderSystem.h"

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace

//=============================================================================
// 初期化（Entity がそろった後に呼ばれる）
//=============================================================================
void DefaultScene::Initialize() {
	// 環境光（IBL）。天球と同じ既定の画像から作る（シーンごとに空を変えるのは SceneFlow.md の SF4）
	environment_.MakeFromHDR(TextureManager::Load(Skybox::kDefaultSkyPath));
}

//=============================================================================
// 描画
//=============================================================================
void DefaultScene::Draw() {
	// 全部の Entity のモデルを描く（カメラは Hierarchy の Camera Component のカメラ）
	ModelRenderSystem::Draw({}, GetCamera(), &environment_, GetWindowTitle());
}

//=============================================================================
// 新しいシーンの最初の配置（シーンファイルがまだ無いときだけ呼ばれる）
//=============================================================================
void DefaultScene::CreateDefaultEntities() {
	// カメラ（Unity の新しいシーンと同じ位置。見た目は Inspector で直して Save する）
	const Handle<Entity> camera = EntityManager::Create("Main Camera");
	EntityManager::Get<TransformComponent>(camera)->translation = {0.0f, 1.0f, -10.0f};
	EntityManager::RequestAdd<CameraComponent>(camera);

	// 平行光源（+Z の向きに照らす。向きは GameScene の最初の配置と同じ）
	const Handle<Entity> light = EntityManager::Create("Directional Light");
	TransformComponent* lightTransform = EntityManager::Get<TransformComponent>(light);
	lightTransform->translation = {0.0f, 3.0f, 0.0f};
	lightTransform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f};
	EntityManager::RequestAdd<DirectionalLightComponent>(light);
}