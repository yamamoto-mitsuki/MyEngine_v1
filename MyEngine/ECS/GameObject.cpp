#include "GameObject.h"

#include "MyEngine/Diagnostics/MyAssert.h"

//=============================================================================
// 作る・探す
//=============================================================================
GameObject GameObject::Create(const std::string& name, GameObject parent) { return {EntityManager::Create(name, parent.handle)}; }

GameObject GameObject::Find(const std::string& name) { return {EntityManager::FindByName(name)}; }

//=============================================================================
// 有効・無効
//=============================================================================
void GameObject::SetActive(bool isActive) const {
	if (Entity* entity = EntityManager::Get(handle)) {
		entity->isActive = isActive;
	}
}

bool GameObject::IsActiveSelf() const {
	const Entity* entity = EntityManager::Get(handle);
	return entity != nullptr && entity->isActive;
}

bool GameObject::IsActiveInHierarchy() const { return EntityManager::IsActiveInHierarchy(handle); }

//=============================================================================
// 位置・名前
//=============================================================================
TransformComponent& GameObject::GetTransform() const {
	// Transform は全部の Entity が必ず持っている（外せない）ので、無いのは「もう消えた GameObject」のときだけ
	TransformComponent* transform = EntityManager::Get<TransformComponent>(handle);
	MY_ASSERT_MSG(transform != nullptr, "消えた GameObject の transform を読みました（if (gameObject) で確かめてから読んでください）");
	return *transform;
}

std::string GameObject::GetName() const {
	const Entity* entity = EntityManager::Get(handle);
	return entity ? entity->name : std::string();
}

void GameObject::SetName(const std::string& newName) const {
	if (Entity* entity = EntityManager::Get(handle)) {
		entity->name = newName;
	}
}