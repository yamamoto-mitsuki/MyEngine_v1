#include "EntityManager.h"

#include <algorithm>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"

// 静的メンバ変数
EntityManager* EntityManager::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
void EntityManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new EntityManager();
	LogManager::Log("Initialized");
}

void EntityManager::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// 生成・破棄
//=============================================================================
Handle<Entity> EntityManager::Create(const std::string& name, Handle<Entity> parent) {
	// Transformは全Entityが必ず持つので、一緒に作る
	Handle<TransformComponent> transform = instance_->transforms_.Create();
	Handle<Entity> handle = instance_->entities_.Create();

	Entity* entity = instance_->entities_.Get(handle);
	MY_ASSERT_MSG(entity, "作った直後のEntityが取れませんでした");
	entity->name = name;
	entity->self = handle; // 一覧から選ぶときに使う
	entity->transform = transform;
	// 親は SetParent を通す（輪になっていないかを見るため）
	if (parent.IsValid()) {
		SetParent(handle, parent);
	}
	return handle;
}

void EntityManager::Destroy(Handle<Entity> handle) { instance_->pendingDestroy_.push_back(handle); }


//=============================================================================
// 取得
//=============================================================================
Entity* EntityManager::Get(Handle<Entity> handle) { return instance_->entities_.Get(handle); }

TransformComponent* EntityManager::GetTransform(Handle<Entity> handle) {
	Entity* entity = instance_->entities_.Get(handle);
	if (!entity) {
		return nullptr;
	}
	return instance_->transforms_.Get(entity->transform);
}

bool EntityManager::IsAlive(Handle<Entity> handle) { return instance_->entities_.IsAlive(handle); }

size_t EntityManager::GetCount() { return instance_->entities_.Size(); }

// ===== Render =====
void EntityManager::RequestAddModelRenderer(Handle<Entity> handle, const ModelRendererComponent& initial) {
	if (!IsAlive(handle) || GetModelRenderer(handle) || IsModelRendererAddPending(handle)) {
		return;
	}
	instance_->pendingAddModelRenderer_.emplace_back(handle, initial); // 初期値も一緒に覚えておく
}

bool EntityManager::IsModelRendererAddPending(Handle<Entity> handle) {
	for (const auto& [pendingHandle, initial] : instance_->pendingAddModelRenderer_) {
		if (pendingHandle == handle) {
			return true;
		}
	}
	return false;
}

ModelRendererComponent* EntityManager::GetModelRenderer(Handle<Entity> handle) {
	Entity* entity = instance_->entities_.Get(handle);
	if (!entity) {
		return nullptr;
	}
	return instance_->modelRenderers_.Get(entity->render);
}

void EntityManager::FlushComponentChanges() {
	EntityManager& self = *instance_;
	for (const auto& [handle, initial] : self.pendingAddModelRenderer_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity || self.modelRenderers_.IsAlive(entity->render)) {
			continue;
		}
		entity->render = self.modelRenderers_.Create();
		*self.modelRenderers_.Get(entity->render) = initial; // 予約したときの値を入れる
	}
	self.pendingAddModelRenderer_.clear();
}


//=============================================================================
// 親子
//=============================================================================
void EntityManager::SetParent(Handle<Entity> child, Handle<Entity> parent) {
	Entity* entity = instance_->entities_.Get(child);
	if (!entity) {
		return;
	}
	// 自分自身、または自分の子孫を親にすると、親子が輪になって無限ループする
	if (child == parent || IsDescendantOf(parent, child)) {
		LogManager::Warning("親子が輪になる付け替えは無視しました");
		return;
	}
	entity->parent = parent;
}

bool EntityManager::IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor) {
	if (!ancestor.IsValid()) {
		return false;
	}
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = instance_->entities_.Get(current);
		if (!entity) {
			return false;
		}
		if (entity->parent == ancestor) {
			return true;
		}
		current = entity->parent;
	}
	return false;
}

void EntityManager::GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (entity.parent == handle) {
			out.push_back(entity.self);
		}
	}
}

void EntityManager::GetRoots(std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (!entity.parent.IsValid()) {
			out.push_back(entity.self);
		}
	}
}


//=============================================================================
// 更新順序2：ワールド行列（親 → 子）
//=============================================================================
void EntityManager::UpdateTransforms() {
	EntityManager& self = *instance_;

	// --- 深さ（親をたどる回数）を数えて、浅い順に並べる ---
	// 親の行列が先に確定していないと、子の行列が1フレーム遅れてしまう
	self.updateOrder_.clear();
	for (const Entity& entity : self.entities_) {
		self.updateOrder_.emplace_back(self.CalcDepth(entity.self), entity.self);
	}
	std::sort(self.updateOrder_.begin(), self.updateOrder_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

	// --- 浅い順に、自分の行列を作って親の行列を掛ける ---
	for (const auto& [depth, handle] : self.updateOrder_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity) {
			continue;
		}
		TransformComponent* transform = self.transforms_.Get(entity->transform);
		if (!transform) {
			continue;
		}
		// 自分の分（大きさ → 回転 → 位置）
		transform->worldMatrix = MakeAffineMatrix(transform->scale, transform->rotation, transform->translation);
		// 親がいれば、親のワールド行列を掛ける（親は先に確定している）
		if (const TransformComponent* parentTransform = GetTransform(entity->parent)) {
			transform->worldMatrix = transform->worldMatrix * parentTransform->worldMatrix;
		}
	}
}

uint32_t EntityManager::CalcDepth(Handle<Entity> handle) {
	uint32_t depth = 0;
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = entities_.Get(current);
		if (!entity || !entity->parent.IsValid()) {
			break;
		}
		++depth;
		current = entity->parent;
	}
	return depth;
}


//=============================================================================
// 更新順序6：破棄
//=============================================================================
void EntityManager::FlushDestroy() {
	EntityManager& self = *instance_;
	if (self.pendingDestroy_.empty()) {
		return;
	}

	// 親を消したら子も消す。消しながら探すと崩れるので、先に全部集める
	self.destroyWork_.clear();
	for (Handle<Entity> handle : self.pendingDestroy_) {
		self.CollectDescendants(handle, self.destroyWork_);
	}
	self.pendingDestroy_.clear();

	for (Handle<Entity> handle : self.destroyWork_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity) {
			continue; // 親と一緒に2回集まったときは、2回目は消えている
		}
		self.modelRenderers_.Destroy(entity->render);
		self.transforms_.Destroy(entity->transform); // Componentも一緒に消す
		self.entities_.Destroy(handle);
	}
}

void EntityManager::CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	if (!entities_.IsAlive(handle)) {
		return;
	}
	out.push_back(handle);
	// 子を探して、その子孫も集める
	for (const Entity& entity : entities_) {
		if (entity.parent == handle) {
			CollectDescendants(entity.self, out);
		}
	}
}