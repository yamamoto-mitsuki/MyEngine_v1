#include "PrefabManager.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Components/Core/TransformComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/String/ConvertString.h"


namespace {
constexpr const char* kTransformName = "Transform"; // Transform の保存の名前（作るときに、これだけ先に付ける）

/// <summary>
/// 読み込んだ Prefab 1つ分
/// </summary>
struct PrefabAsset {
	std::string path;                          // パス（UTF-8。ToPathKey の形。読み込み済みかを調べるキーにもなる）
	std::shared_ptr<const SceneJson> entities; // Entity の並び（ファイルの "entities"）。保存し直すと、新しい物に差し替わる
};

/// <summary>
/// 作ったけれど、まだ Transform 以外の Component を付けていない物（次のフレームの頭の Flush で付ける）
/// </summary>
struct PendingLoad {
	std::shared_ptr<const SceneJson> entities; // 作ったときの中身（途中で Prefab を保存し直しても、作ったときの中身で付ける）
	std::vector<Handle<Entity>> created;       // entities の i 番目から作った Entity
	EntityIdMap idMap;                         // ファイルの番号 → 作った Entity の番号
};

/// <summary>
/// PrefabManager が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct PrefabState {
	std::vector<PrefabAsset> assets;       // 読み込んだ Prefab（番号 - 1 が添字。番号は1から、0は未選択）
	std::vector<PendingLoad> pendingLoads; // Component を付ける予約
	std::vector<uint32_t> placeRequests;   // エディタから置く予約
};
std::unique_ptr<PrefabState> state;

//=============================================================================
// 小さな道具
//=============================================================================
// 番号から Prefab（知らない番号なら nullptr）
PrefabAsset* Find(uint32_t handle) {
	if (handle == 0 || handle > state->assets.size()) {
		return nullptr;
	}
	return &state->assets[handle - 1];
}

// 集めた警告を1回ずつ出す（同じ Prefab を何個作っても、同じ文は1回だけ）
void FlushWarnings(const std::set<std::string>& warnings) {
	for (const std::string& warning : warnings) {
		LogManager::Warning(warning);
	}
}

// Entity 1つ分の "components"（無ければ nullptr）
const SceneJson* FindComponents(const SceneJson& item) {
	const auto it = item.find("components");
	return (it != item.end() && it->is_object()) ? &*it : nullptr;
}

// Entity 1つ分の形が正しいか（読むときに1回だけ確かめる。作るたびに value() で形の違う値を読むと例外が出るので）
bool IsValidItem(const SceneJson& item) {
	if (!item.is_object()) {
		return false;
	}
	// --- 番号は必ず要る（0は「無し」なので使えない）---
	const auto id = item.find("id");
	if (id == item.end() || !id->is_number_unsigned() || id->get<EntityId>() == 0) {
		return false;
	}
	// --- ほかは、あれば形を確かめる（無ければ作るときに初期値）---
	const auto isOptional = [&item](const char* key, bool (SceneJson::*isExpected)() const noexcept) {
		const auto it = item.find(key);
		return it == item.end() || ((*it).*isExpected)();
	};
	return isOptional("name", &SceneJson::is_string) && isOptional("parent", &SceneJson::is_number_unsigned) && isOptional("active", &SceneJson::is_boolean) &&
	       isOptional("components", &SceneJson::is_object);
}

// ファイルを読んで、Entity の並びを取り出す（読めなければ nullptr）
std::shared_ptr<const SceneJson> ReadEntities(const std::filesystem::path& filePath) {
	std::ifstream stream(filePath);
	if (!stream) {
		LogManager::Warning(std::format("Prefab のファイルを開けませんでした: {}", ToUtf8(filePath)));
		return nullptr;
	}
	const SceneJson root = SceneJson::parse(stream, nullptr, false); // false: 壊れていても例外を投げず、discarded を返す
	if (root.is_discarded() || !root.is_object()) {
		LogManager::Error(std::format("Prefab のファイルが壊れています: {}", ToUtf8(filePath)));
		return nullptr;
	}
	const SceneJson* entities = SceneSerializer::FindEntities(root);
	if (entities == nullptr || entities->empty()) {
		LogManager::Error(std::format("Prefab に Entity がありません: {}", ToUtf8(filePath)));
		return nullptr;
	}
	if (!std::all_of(entities->begin(), entities->end(), &IsValidItem)) {
		LogManager::Error(std::format("Prefab の Entity の形が違います（\"id\" が無い・数でない など）: {}", ToUtf8(filePath)));
		return nullptr;
	}
	return std::make_shared<const SceneJson>(*entities);
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void PrefabManager::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<PrefabState>();
}

void PrefabManager::Release() { state.reset(); }

//=============================================================================
// 読み込み
//=============================================================================
uint32_t PrefabManager::Load(const std::filesystem::path& filePath) {
	MY_ASSERT_MSG(state != nullptr, "PrefabManager::Initialize()を先に呼んでください");
	// --- 読み込み済みなら、同じ番号を返す ---
	const std::string key = ToPathKey(filePath);
	const auto cached = std::ranges::find(state->assets, key, &PrefabAsset::path);
	if (cached != state->assets.end()) {
		return static_cast<uint32_t>(cached - state->assets.begin()) + 1;
	}
	// --- ファイルから読む ---
	std::shared_ptr<const SceneJson> entities = ReadEntities(filePath);
	if (entities == nullptr) {
		return 0; // 止めずに未選択の番号を返す
	}
	const size_t count = entities->size();
	state->assets.push_back({key, std::move(entities)});
	const uint32_t handle = static_cast<uint32_t>(state->assets.size());
	LogManager::Log(std::format("読み込み完了 \"{}\" -> handle={} Entity数={}", key, handle, count));
	return handle;
}

const std::string& PrefabManager::GetPath(uint32_t handle) {
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	const PrefabAsset* asset = (state != nullptr) ? Find(handle) : nullptr;
	return (asset != nullptr) ? asset->path : kEmpty;
}

//=============================================================================
// 保存
//=============================================================================
bool PrefabManager::Save(Handle<Entity> root, const std::string& path) {
	MY_ASSERT_MSG(state != nullptr, "PrefabManager::Initialize()を先に呼んでください");
	if (!EntityManager::IsAlive(root)) {
		return false;
	}
	const SceneJson json = SceneSerializer::SaveTree(root);
	if (!SceneSerializer::WriteFile(path, json)) {
		return false;
	}
	// --- もう読み込んでいる Prefab なら、中身を差し替える ---
	// このあと作る物から新しい中身になる。作りかけの物（PendingLoad）は前の中身を shared_ptr で持っているので、そのまま付け終われる
	const std::string key = ToPathKey(ToPath(path));
	const auto cached = std::ranges::find(state->assets, key, &PrefabAsset::path);
	if (cached != state->assets.end()) {
		cached->entities = std::make_shared<const SceneJson>(json.at("entities"));
	}
	LogManager::Log(std::format("Prefab を保存しました: {}（Entity {}個）", path, json.at("entities").size()));
	return true;
}

//=============================================================================
// 作る
//=============================================================================
Handle<Entity> PrefabManager::Instantiate(uint32_t prefab, const Vector3& position, Handle<Entity> parent) {
	MY_ASSERT_MSG(state != nullptr, "PrefabManager::Initialize()を先に呼んでください");
	const PrefabAsset* asset = Find(prefab);
	if (asset == nullptr) {
		LogManager::Warning(std::format("読み込んでいない Prefab の番号 {} から作ろうとしました（Inspector で Prefab を選んでいるか確かめてください）", prefab));
		return {};
	}
	PendingLoad pending;
	pending.entities = asset->entities; // 中身を共有する（写さない）
	const SceneJson& entities = *pending.entities;
	pending.created.reserve(entities.size());

	// ===== 1周目：Entity を作る（番号は新しくもらう。ファイルの番号 → 新しい番号を表に入れる）=====
	for (const SceneJson& item : entities) {
		const Handle<Entity> created = EntityManager::Create(item.value("name", std::string("Entity")));
		Entity* entity = EntityManager::Get(created);
		entity->isActive = item.value("active", true);
		pending.idMap[item.value("id", EntityId{0})] = entity->id;
		pending.created.push_back(created);
	}

	// ===== 2周目：親子をつなぐ =====
	// 先頭（一番上）は parent の子に。それ以外は、ファイルに書いてある親（見つからなければ一番上）の子にする
	const Handle<Entity> root = pending.created.front();
	for (size_t i = 0; i < entities.size(); ++i) {
		Handle<Entity> newParent = parent;
		if (i > 0) {
			const auto found = pending.idMap.find(entities[i].value("parent", EntityId{0}));
			newParent = (found != pending.idMap.end()) ? EntityManager::FindById(found->second) : root;
		}
		if (newParent.IsValid()) {
			EntityManager::SetParent(pending.created[i], newParent);
		}
	}

	// ===== 3周目：Transform だけ今すぐ付ける =====
	// Transform は Entity を作ったときから持っているので、上書きするだけ（Component の配列は増えない＝フレームの途中でも安全）
	std::set<std::string> warnings;
	for (size_t i = 0; i < entities.size(); ++i) {
		const SceneJson* components = FindComponents(entities[i]);
		if (components == nullptr) {
			continue;
		}
		const auto transform = components->find(kTransformName);
		if (transform == components->end()) {
			continue;
		}
		SceneJson onlyTransform = SceneJson::object();
		onlyTransform[kTransformName] = *transform;
		ComponentSerializer::Load(pending.created[i], onlyTransform, warnings, &pending.idMap);
	}
	FlushWarnings(warnings);
	// 一番上の位置を、渡された位置にする（回転・大きさは Prefab のまま）
	if (TransformComponent* transform = EntityManager::Get<TransformComponent>(root)) {
		transform->translation = position;
	}

	// ===== ほかの Component・Script は、次のフレームの頭に付ける =====
	state->pendingLoads.push_back(std::move(pending));
	return root;
}

void PrefabManager::RequestPlace(uint32_t prefab) {
	MY_ASSERT_MSG(state != nullptr, "PrefabManager::Initialize()を先に呼んでください");
	state->placeRequests.push_back(prefab);
}

//=============================================================================
// フレームの頭
//=============================================================================
void PrefabManager::Flush() {
	MY_ASSERT_MSG(state != nullptr, "PrefabManager::Initialize()を先に呼んでください");
	// ===== エディタから置く予約（作るだけ。Component は下でまとめて付く）=====
	// 取り出してから回す（回している間に予約が足されても壊れないように）
	const std::vector<uint32_t> places = std::exchange(state->placeRequests, {});
	for (const uint32_t prefab : places) {
		Instantiate(prefab, {0.0f, 0.0f, 0.0f});
	}

	// ===== Component・Script を付ける（Transform は作ったときに付けたので飛ばす）=====
	const std::vector<PendingLoad> loads = std::exchange(state->pendingLoads, {});
	std::set<std::string> warnings;
	for (const PendingLoad& pending : loads) {
		for (size_t i = 0; i < pending.created.size(); ++i) {
			if (!EntityManager::IsAlive(pending.created[i])) {
				continue; // 作ってから今までに消された（Destroy・Stop）
			}
			const SceneJson* components = FindComponents((*pending.entities)[i]);
			if (components == nullptr) {
				continue;
			}
			SceneJson rest = *components;
			rest.erase(kTransformName);
			// フレームの境目なので、予約せずその場で付けてよい（シーンを読むときと同じ）
			ComponentSerializer::Load(pending.created[i], rest, warnings, &pending.idMap);
		}
	}
	FlushWarnings(warnings);
}