#include "SceneSerializer.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Component/ComponentSerializer.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
// 集めた警告を1回ずつ出す（同じComponentを100個持っていても、同じ文は1回だけ）
void FlushWarnings(const std::set<std::string>& warnings) {
	for (const std::string& warning : warnings) {
		LogManager::Warning(warning);
	}
}

//=============================================================================
// 全Entity → JSON
//=============================================================================
SceneJson SaveAll() {
	std::set<std::string> warnings;
	SceneJson entities = SceneJson::array();
	// 並んでいる順（Hierarchyに出る順）のまま書く。読むときも同じ順に作るので、並びが変わらない
	for (const Entity& entity : EntityManager::GetAll()) {
		const Entity* parent = EntityManager::Get(entity.parent);
		SceneJson components = SceneJson::object();
		ComponentSerializer::Save(entity.self, components, warnings);

		// 1つのEntityの中身は手元で組み立ててから入れる（ordered_map は途中で参照を持つと壊れるため）
		SceneJson item = SceneJson::object();
		item["id"] = entity.id;
		item["name"] = entity.name;
		item["parent"] = parent ? parent->id : EntityId{0}; // 親は EntityId で書く（0は親なし）
		item["active"] = entity.isActive;
		item["components"] = std::move(components);
		entities.push_back(std::move(item));
	}
	FlushWarnings(warnings);

	SceneJson root = SceneJson::object();
	root["version"] = SceneSerializer::kVersion;
	root["entities"] = std::move(entities);
	return root;
}

//=============================================================================
// JSON → Entity（型が違う値を value() で読むと例外が出るので、呼ぶ側で受ける）
//=============================================================================
bool LoadAll(const SceneJson& root) {
	// --- 版を確かめる（新しい版は、どこが変わったか分からないので読まない）---
	const int version = root.value("version", 0);
	if (version < 1 || version > SceneSerializer::kVersion) {
		LogManager::Error(std::format("このエンジンでは読めない版です（ファイル: {} / エンジン: {}）", version, SceneSerializer::kVersion));
		return false;
	}
	// 版を上げたら、ここで古い版のJSONを今の形に直してから読む（例: if (version < 2) { 回転をクオータニオンに直す }）
	const auto entitiesIt = root.find("entities");
	if (entitiesIt == root.end() || !entitiesIt->is_array()) {
		LogManager::Error("\"entities\" がありません");
		return false;
	}
	const SceneJson& entities = *entitiesIt;
	std::set<std::string> warnings;

	// ===== 1周目：Entityを作る（親はまだ付けない。ファイルの並びが崩れていても困らないように、全部そろってから付ける）=====
	std::vector<Handle<Entity>> created(entities.size()); // 作れなかった所は無効なHandleのまま
	for (size_t i = 0; i < entities.size(); ++i) {
		const SceneJson& item = entities[i];
		const EntityId id = item.value("id", EntityId{0});
		const std::string name = item.value("name", std::string("Entity"));
		if (id == 0 || EntityManager::FindById(id).IsValid()) {
			warnings.insert(std::format("EntityIdが0か、2つ目の同じ番号なので飛ばしました: {}", name));
			continue;
		}
		created[i] = EntityManager::CreateWithId(id, name, {});
		EntityManager::Get(created[i])->isActive = item.value("active", true);
	}

	// ===== 2周目：親子をつなぐ =====
	for (size_t i = 0; i < entities.size(); ++i) {
		const EntityId parentId = entities[i].value("parent", EntityId{0});
		if (!created[i].IsValid() || parentId == 0) {
			continue;
		}
		const Handle<Entity> parent = EntityManager::FindById(parentId);
		if (!parent.IsValid()) {
			warnings.insert(std::format("親の番号 {} が見つからないので、一番上に置きました", parentId));
			continue;
		}
		EntityManager::SetParent(created[i], parent); // 輪になる親子は SetParent が断る
	}

	// ===== 3周目：Componentを付ける（全Entityがそろった後なので、Componentの中のEntityへの参照も引ける）=====
	for (size_t i = 0; i < entities.size(); ++i) {
		const auto componentsIt = entities[i].find("components");
		if (!created[i].IsValid() || componentsIt == entities[i].end() || !componentsIt->is_object()) {
			continue;
		}
		ComponentSerializer::Load(created[i], *componentsIt, warnings);
	}
	FlushWarnings(warnings);
	return true;
}

// 読み込みの入口。失敗したら作りかけのEntityを消して、何も無かったことにする
bool LoadSafely(const SceneJson& root) {
	try {
		if (LoadAll(root)) {
			return true;
		}
	} catch (const SceneJson::exception& exception) {
		// "id": "abc" のように、形の違う値があったとき
		LogManager::Error(std::format("シーンの読み込みに失敗しました: {}", exception.what()));
	}
	SceneSerializer::DestroyAllNow();
	return false;
}

// 1つの値を1行の文字列にする（名前に壊れた文字が混じっていても、例外で落とさず置き換える）
std::string ToLine(const SceneJson& json) { return json.dump(-1, ' ', false, SceneJson::error_handler_t::replace); }

// ファイル用に、人が読める形で書く。タブで字下げし、数だけの配列（Vector3 など）は1行にまとめる
// （nlohmann の dump で字下げすると1要素1行になり、Transform1つで15行になってしまう）
void WriteReadable(const SceneJson& json, int depth, std::string& out) {
	const std::string indent(static_cast<size_t>(depth) + 1, '\t');  // 中身の字下げ
	const std::string closeIndent(static_cast<size_t>(depth), '\t'); // 閉じかっこの字下げ
	const bool isNumberArray = json.is_array() && std::all_of(json.begin(), json.end(), [](const SceneJson& element) { return element.is_number(); });

	if (json.is_object() && !json.empty()) {
		// --- { "キー": 値, ... } は1項目1行 ---
		out += "{\n";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += indent + ToLine(SceneJson(it.key())) + ": ";
			WriteReadable(*it, depth + 1, out);
			out += (std::next(it) == json.end()) ? "\n" : ",\n";
		}
		out += closeIndent + "}";
	} else if (isNumberArray) {
		// --- [1.0, 2.0, 3.0] は1行 ---
		out += "[";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += ToLine(*it);
			out += (std::next(it) == json.end()) ? "" : ", ";
		}
		out += "]";
	} else if (json.is_array() && !json.empty()) {
		// --- それ以外の配列（Entityの並びなど）は1要素ずつ ---
		out += "[\n";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += indent;
			WriteReadable(*it, depth + 1, out);
			out += (std::next(it) == json.end()) ? "\n" : ",\n";
		}
		out += closeIndent + "]";
	} else {
		out += ToLine(json); // 数・文字・true/false・空の {} []
	}
}
} // namespace

//=============================================================================
// ファイル
//=============================================================================
bool SceneSerializer::SaveFile(const std::string& path) {
	const std::filesystem::path filePath(path);
	std::error_code error; // 例外ではなくエラーコードで受ける
	std::filesystem::create_directories(filePath.parent_path(), error);
	// 前のファイルを1つだけ残す（書いている途中で落ちても、1つ前には戻れるように）
	if (std::filesystem::exists(filePath, error)) {
		std::filesystem::copy_file(filePath, filePath.string() + ".bak", std::filesystem::copy_options::overwrite_existing, error);
	}
	std::ofstream stream(filePath);
	if (!stream) {
		LogManager::Error(std::format("シーンファイルを開けませんでした: {}", path));
		return false;
	}
	std::string text;
	WriteReadable(SaveAll(), 0, text); // gitの差分で見やすいように、タブで字下げして書く
	stream << text << '\n';
	stream.close();
	if (stream.fail()) {
		LogManager::Error(std::format("シーンファイルを書き切れませんでした（1つ前は .bak にあります）: {}", path));
		return false;
	}
	LogManager::Log(std::format("シーンを保存しました: {}", path));
	return true;
}

SceneLoadResult SceneSerializer::LoadFile(const std::string& path) {
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		return SceneLoadResult::NotFound;
	}
	std::ifstream stream(path);
	const SceneJson root = SceneJson::parse(stream, nullptr, false); // false: 壊れていても例外を投げず、discarded を返す
	stream.close();
	const bool isLoaded = !root.is_discarded() && root.is_object() && LoadSafely(root);
	if (!isLoaded) {
		// 読めなかったファイルを別の名前で残す（このまま Save を2回押すと、本体も .bak も空のシーンで上書きされてしまうので）
		std::filesystem::copy_file(path, path + ".broken", std::filesystem::copy_options::overwrite_existing, error);
		LogManager::Error(std::format("シーンファイルを読めませんでした（壊れている・新しい版で保存された）。{}.broken に写しました", path));
		return SceneLoadResult::Failed;
	}
	LogManager::Log(std::format("シーンを読み込みました: {}（{}個）", path, EntityManager::GetCount()));
	return SceneLoadResult::Loaded;
}

//=============================================================================
// 文字列
//=============================================================================
std::string SceneSerializer::SaveToText() { return ToLine(SaveAll()); } // 1行に詰める（人が読まないので）

bool SceneSerializer::LoadFromText(const std::string& text) {
	const SceneJson root = SceneJson::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		LogManager::Error("退避したシーンが読めませんでした");
		return false;
	}
	return LoadSafely(root);
}

//=============================================================================
// 全Entityを今すぐ消す
//=============================================================================
void SceneSerializer::DestroyAllNow() {
	std::vector<Handle<Entity>> roots;
	EntityManager::GetRoots(roots);
	for (Handle<Entity> root : roots) {
		EntityManager::Destroy(root); // 子も一緒に消える
	}
	EntityManager::FlushDestroy(); // 予約をその場で反映する（フレームの境目なので安全。EditorHistoryのUndoと同じ使い方）
}