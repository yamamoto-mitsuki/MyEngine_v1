#include "ModelRenderSystem.h"

#include <cstddef>

#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/IBL/IBLEnvironment.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

namespace {
// 親をたどり、(1) 自分と祖先すべてが有効か (2) rootの配下か を調べる。
// rootが無効なHandleのときは(2)を見ない（＝全部のEntityが対象。Hierarchyで作ったEntityがそのまま描かれる）
bool IsDrawableInRoot(Handle<Entity> handle, Handle<Entity> root) {
	bool belongsToRoot = !root.IsValid(); // rootを指定していなければ、最初からtrue
	// 生存Entity数以上に親をたどるなら循環している。
	const size_t count = EntityManager::GetCount();
	for (size_t i = 0; i < count; ++i) {
		const Entity* entity = EntityManager::Get(handle);
		if (!entity || !entity->isActive) {
			return false;
		}
		if (handle == root) {
			belongsToRoot = true;
		}
		if (!entity->parent.IsValid()) {
			return belongsToRoot;
		}
		handle = entity->parent;
	}
	return false;
}
} // namespace



//=============================================================================
// ModelRendererComponentの描画
//=============================================================================
void ModelRenderSystem::Draw(Handle<Entity> root, Camera* camera, IBLEnvironment* environment, const std::wstring& windowTitle) {
	if (!camera || windowTitle.empty()) {
		return;
	}

	for (const Entity& entity : EntityManager::GetAll()) {
		const ModelRendererComponent* render = EntityManager::GetModelRenderer(entity.self);
		if (!render || !render->enabled || render->modelHandle == 0) {
			continue;
		}
		if (!IsDrawableInRoot(entity.self, root)) {
			continue;
		}
		const TransformComponent* transform = EntityManager::GetTransform(entity.self);
		if (!transform || !ModelManager::GetModelAsset(render->modelHandle)) {
			continue;
		}
		if (render->shadingType == ShadingType::PBR && (!environment || environment->GetParametersAddress() == 0)) {
			continue;
		}

		Renderer::ModelConfig config{};
		config.modelHandle = render->modelHandle;
		config.textureHandle = render->textureHandle;
		config.color = render->color;
		config.uvTransform = render->uvTransform;
		config.material = render->material;
		config.shadingType = render->shadingType;
		config.blendMode = render->blendMode;
		config.rasterizerType = render->rasterizerType;
		config.depthMode = render->depthMode;
		config.billboard = render->billboard;
		config.worldMatrix = &transform->worldMatrix;
		config.camera = camera;
		config.env = environment;
		config.windowTitle = windowTitle;
		Renderer::DrawModel(config);
	}
}