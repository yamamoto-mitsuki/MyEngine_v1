#pragma once
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/ModelRendererComponent.h"


/// <summary>
/// ModelRendererComponentのInspector（どのモデルを描くか・描画設定・マテリアル）
/// </summary>
class ModelRendererEditor : public TypedComponentEditor<ModelRendererComponent> {
public:
	const char* GetName() const override { return "Model Renderer"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Rendering3D; }
	bool IsAddPending(Handle<Entity> handle) const override;
	void RequestRemove(Handle<Entity> handle) const override;

protected:
	ModelRendererComponent* GetComponent(Handle<Entity> handle) const override;
	void RequestAddComponent(Handle<Entity> handle, const ModelRendererComponent& initial) const override;
	void DrawComponent(ModelRendererComponent& render) const override;

private:
	// モデルを選ぶコンボ（resources以下を走査した一覧から選ぶ）
	void DrawModelPicker(ModelRendererComponent& render) const;
};