#include "AttackPart.h"

#include <format>
#include <map>

#include "MyEngine/Diagnostics/LogManager.h"

namespace {
// 名前 → 作り方 の表
// ATTACK_PART は main より前に登録するので、ふつうの static 変数だと表の方がまだ作られていないことがある。関数の中の static にする（REGISTER_SCENE と同じ）
std::map<std::string, AttackPartRegistry::CreateFunction, std::less<>>& AttackTable() {
	static std::map<std::string, AttackPartRegistry::CreateFunction, std::less<>> table;
	return table;
}
} // namespace


//=============================================================================
// 登録・作る
//=============================================================================
void AttackPartRegistry::Register(const char* name, CreateFunction create) { AttackTable()[name] = create; }

std::unique_ptr<AttackPart> AttackPartRegistry::Create(std::string_view name) {
	const auto found = AttackTable().find(name);
	return (found != AttackTable().end()) ? found->second() : nullptr;
}

std::vector<std::string> AttackPartRegistry::GetNames() {
	std::vector<std::string> names;
	names.reserve(AttackTable().size());
	for (const auto& [name, create] : AttackTable()) {
		names.push_back(name);
	}
	return names;
}