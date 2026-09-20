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
	// エンジンのComponent。ライトは LightSystem::Initialize、ゲーム固有の型はゲームが登録する
	RegisterComponent<TransformComponent>();
	RegisterComponent<ModelRendererComponent>();
	LogManager::Log("Initialized");
}

void EntityManager::Release() {
	EnsureInitialized();
	delete instance_; // 全Storageと、その予約もunique_ptrが解放する
	instance_ = nullptr;
	LogManager::Log("Released");
}

//=============================================================================
// 生成・破棄
//=============================================================================
Handle<Entity> EntityManager::Create(const std::string& name, Handle<Entity> parent) {
	return CreateWithId(NewId(), name, parent); // 新しい番号を配って作る
}

Handle<Entity> EntityManager::CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent) {
	EnsureInitialized();
	MY_ASSERT_MSG(id != 0 && !FindById(id).IsValid(), "EntityIdが0か、もう使われています");
	const Handle<Entity> handle = instance_->entities_.Create();
	Entity* entity = instance_->entities_.Get(handle);
	entity->name = name;
	entity->id = id;
	entity->self = handle;
	instance_->idToHandle_[id] = handle;
	// 番号を指定して作ったときも、この先配る番号とぶつからないようにする
	instance_->nextId_ = (std::max)(instance_->nextId_, id + 1);
	// 全Entityの必須データ。作った直後から取れるように、予約せずその場で作る
	// （ForEach<TransformComponent> の途中ならここでアサートが出る）
	RequireStorage<TransformComponent>().SetNow(handle, TransformComponent{});
	// 親は SetParent を通す（輪になっていないかを見るため）
	if (parent.IsValid()) {
		SetParent(handle, parent);
	}
	return handle;
}

EntityId EntityManager::NewId() { return instance_->nextId_++; }

void EntityManager::Destroy(Handle<Entity> handle) { instance_->pendingDestroy_.push_back(handle); }

//=============================================================================
// 取得
//=============================================================================
Entity* EntityManager::Get(Handle<Entity> handle) { return instance_->entities_.Get(handle); }

bool EntityManager::IsAlive(Handle<Entity> handle) { return instance_ && instance_->entities_.IsAlive(handle); }

Handle<Entity> EntityManager::FindById(EntityId id) {
	auto it = instance_->idToHandle_.find(id);
	return (it != instance_->idToHandle_.end()) ? it->second : Handle<Entity>{};
}

size_t EntityManager::GetCount() { return instance_->entities_.Size(); }

bool EntityManager::IsActiveInHierarchy(Handle<Entity> handle) {
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		const Entity* entity = instance_->entities_.Get(current);
		if (!entity || !entity->isActive) {
			return false;
		}
		if (!entity->parent.IsValid()) {
			return true; // 一番上まで全部有効だった
		}
		current = entity->parent;
	}
	return false; // 深すぎる（輪になっている）
}

IComponentStorage* EntityManager::FindStorage(std::type_index type) {
	if (!instance_) {
		return nullptr;
	}
	const auto it = instance_->components_.find(type);
	return it == instance_->components_.end() ? nullptr : it->second.get();
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
// Componentの写し（Undo・コピー用）
//=============================================================================
void EntityManager::CaptureComponents(Handle<Entity> handle, std::vector<ComponentSnapshot>& out) {
	out.clear();
	if (!IsAlive(handle)) {
		return;
	}
	// Inspectorに出していない（Editorを登録していない）型も含めて、持っている物を全部写す
	for (const auto& [type, storage] : instance_->components_) {
		if (const void* component = storage->Find(handle)) {
			const auto* bytes = static_cast<const std::byte*>(component);
			out.push_back({type, std::vector<std::byte>(bytes, bytes + storage->GetSize())});
		}
	}
}

void EntityManager::RestoreComponents(Handle<Entity> handle, const std::vector<ComponentSnapshot>& components) {
	EnsureInitialized();
	if (!IsAlive(handle)) {
		return;
	}
	for (const ComponentSnapshot& component : components) {
		IComponentStorage* storage = FindStorage(component.type);
		MY_ASSERT_MSG(storage != nullptr && storage->GetSize() == component.bytes.size(), "写したComponentの型が登録されていません");
		storage->SetNowBytes(handle, component.bytes.data());
	}
}

//=============================================================================
// 予約の反映
//=============================================================================
void EntityManager::FlushComponentChanges() {
	EnsureInitialized();
	for (auto& [type, storage] : instance_->components_) {
		storage->Flush(&EntityManager::IsAlive);
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
		const Entity* entity = self.entities_.Get(handle);
		TransformComponent* transform = Get<TransformComponent>(handle);
		if (!entity || !transform) {
			continue;
		}
		// 自分の分（大きさ → 回転 → 位置）
		transform->worldMatrix = MakeAffineMatrix(transform->scale, transform->rotation, transform->translation);
		// 親がいれば、親のワールド行列を掛ける（親は先に確定している）
		if (const TransformComponent* parentTransform = Get<TransformComponent>(entity->parent)) {
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
	EnsureInitialized();
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
		// ゲームが登録した型も含め、実体と未反映の予約をすべて消す
		for (auto& [type, storage] : self.components_) {
			storage->DestroyEntity(handle);
		}
		self.idToHandle_.erase(entity->id);
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

void EntityManager::EnsureInitialized() { MY_ASSERT_MSG(instance_ != nullptr, "EntityManager::Initialize()を先に呼んでください"); }