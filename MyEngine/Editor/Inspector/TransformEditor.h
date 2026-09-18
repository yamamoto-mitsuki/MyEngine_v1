#pragma once
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// TransformComponentのInspector（全Entityが必ず持つので、外せない）
/// </summary>
class TransformEditor : public TypedComponentEditor<TransformComponent> {
public:
	const char* GetName() const override { return "Transform"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Core; }
	bool IsOptional() const override { return false; }


protected:
	TransformComponent* GetComponent(Handle<Entity> handle) const override;
	void DrawComponent(TransformComponent& transform) const override;
};