#include "AttackPart.h"

#include <format>
#include <map>

#include "MyEngine/Diagnostics/LogManager.h"

namespace {
// 表の1行
struct PartEntry {
	AttackPartRegistry::CreateFunction create; // 作り方
	std::string description;                   // Part の欄に出す説明
};

// 名前 → 作り方と説明 の表
// ATTACK_PART は main より前に登録するので、ふつうの static 変数だと表の方がまだ作られていないことがある。関数の中の static にする（REGISTER_SCENE と同じ）
std::map<std::string, PartEntry, std::less<>>& PartTable() {
	static std::map<std::string, PartEntry, std::less<>> table;
	return table;
}
} // namespace


//=============================================================================
// 登録・作る
//=============================================================================
void AttackPartRegistry::Register(const char* name, CreateFunction create, const char* description) { PartTable()[name] = {create, description}; }

std::unique_ptr<AttackPart> AttackPartRegistry::Create(std::string_view name) {
	const auto found = PartTable().find(name);
	return (found != PartTable().end()) ? found->second.create() : nullptr;
}

std::vector<std::string> AttackPartRegistry::GetNames() {
	std::vector<std::string> names;
	names.reserve(PartTable().size());
	for (const auto& [name, entry] : PartTable()) {
		names.push_back(name);
	}
	return names;
}

const std::string& AttackPartRegistry::GetDescription(std::string_view name) {
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	const auto found = PartTable().find(name);
	return (found != PartTable().end()) ? found->second.description : kEmpty;
}