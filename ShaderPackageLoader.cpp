#include "ShaderPackageLoader.h"
#include <format>
#include <fstream>
#include <unordered_map>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Diagnostics/MyAssert.h"


namespace {
// ハッシュ。FNV-1a 64bit
uint64_t Fnv1a(std::string_view s) {
	uint64_t h = 1469598103934665603ull;
	for (unsigned char c : s) {
		h ^= c;
		h *= 1099511628211ull;
	}
	return h;
}
} // namespace


//=============================================================================
// ファイルを開いて全部読む（解析なし）
//=============================================================================
ShaderDefinition ShaderPackageLoader::ParseFile(const std::filesystem::path& path) {
	// バイナリモードで開く
	std::ifstream ifs(path, std::ios::binary);
	MY_ASSERT_MSG(ifs.is_open(), std::format(".shaderを開けません: {}", path.string()));
	// ファイルを先頭から終端まで読む
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	return ParseText(content, path.filename().string());
}


//=============================================================================
// 中身から（本体パーサ）
//=============================================================================
ShaderDefinition ShaderPackageLoader::ParseText(std::string_view text, std::string_view sourceName) {
	ShaderDefinition def;
	def.source = std::string(sourceName); // ファイル名

	std::vector<std::string> metaLines;
	std::string hlslBody;
	enum class Block { None, Meta, Hlsl } block = Block::None;

	// ===== 全文読む =====
	size_t pos = 0;
	while (pos < text.size()) {
		// --- 1行取り出し ---
		size_t eol = text.find('\n', pos);
		std::string_view line = text.substr(pos, (eol == std::string_view::npos ? text.size() : eol) - pos);
		pos = (eol == std::string_view::npos) ? text.size() : eol + 1;
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1); // CRLF対応
		}

		std::string_view t = Trim(line);
		// --- ブロック境界（行まるごと一致で判定）---
		if (t == "#META") {
			block = Block::Meta;
			continue;
		}
		if (t == "#META_END") {
			block = Block::None;
			continue;
		}
		if (t == "#HLSL") {
			block = Block::Hlsl;
			continue;
		}
		if (t == "#HLSL_END") {
			block = Block::None;
			continue;
		}

		// --- 中身 ---
		if (block == Block::Meta) {
			metaLines.emplace_back(line); // metaは後でkey:value解析
		} else if (block == Block::Hlsl) {
			hlslBody += line; // HLSLはインデントごと原文保持
			hlslBody += '\n';
		}
	}

	def.meta = ParseMeta(metaLines, sourceName);
	def.hlslBody = std::move(hlslBody);
	return def;
}

//=============================================================================
// #META の key:value 解析
//=============================================================================
ShaderMeta ShaderPackageLoader::ParseMeta(const std::vector<std::string>& lines, std::string_view src) {
	// --- 1. key -> value を集める ---
	std::unordered_map<std::string, std::string> kv;
	for (const auto& raw : lines) {
		std::string_view line = Trim(raw);
		if (line.empty() || line.starts_with("//")) {
			continue; // 空行/コメント
		}
		size_t colon = line.find(':');
		MY_ASSERT_MSG(colon != std::string_view::npos, std::format("{}: ':' がありません -> '{}'", src, std::string(line)));
		kv[std::string(Trim(line.substr(0, colon)))] = std::string(Trim(line.substr(colon + 1)));
	}

	// --- 2. 共通フィールド ---
	ShaderMeta m;
	auto stageIt = kv.find("stage");
	MY_ASSERT_MSG(stageIt != kv.end(), std::format("{}: 必須 'stage' がありません", src));
	m.stage = ParseEnum<ShaderMeta::Stage>(stageIt->second, "stage", src);

	m.path = ConvertString(Require(kv, "path", src));
	if (auto e = kv.find("entry"); e != kv.end()) {
		m.entry = e->second;
	}
	if (auto p = kv.find("profile"); p != kv.end()) {
		m.profile = ConvertString(p->second);
	} else {
		m.profile = DefaultProfile(m.stage);
	}

	// --- 3. stage別フィールド ---
	switch (m.stage) {
	case ShaderMeta::Stage::VS: // VS
		m.name = Require(kv, "name", src); // ペア参照キー
		break;

	case ShaderMeta::Stage::PS: // PS
		m.drawCategory = ParseEnum<DrawCategory>(Require(kv, "drawCategory", src), "drawCategory", src);
		m.vsName = Require(kv, "vs", src); // ペアVS
		if (auto s = kv.find("shading"); s != kv.end()) {
			m.shading = ParseEnum<ShadingType>(s->second, "shading", src); // 省略時 Unlit
		}
		if (auto n = kv.find("name"); n != kv.end()) {
			m.name = n->second; // 任意（名前引き用）
		}
		break;

	case ShaderMeta::Stage::Include: // hlsli
		break; // path のみでOK
	}
	return m;
}


//=============================================================================
// outRoot/path に書き出す。中身が同じならスキップ
//=============================================================================
bool ShaderPackageLoader::IsGenerate(const ShaderDefinition& def, const std::filesystem::path& outRoot) {
	std::filesystem::path outPath = outRoot / def.meta.path; // 例: MyEngine/shader/Model/Object3dLambert.PS.hlsl
	std::filesystem::path hashPath = outPath;
	hashPath += L".hash";
	// 今の中身のハッシュ（16進文字列）
	const std::string hash = std::format("{:016x}", Fnv1a(def.hlslBody));

	// --- .hlsl と .hash が両方あって、ハッシュ一致ならスキップ ---
	if (std::filesystem::exists(outPath) && std::filesystem::exists(hashPath)) {
		std::ifstream hin(hashPath, std::ios::binary);
		std::string prev((std::istreambuf_iterator<char>(hin)), std::istreambuf_iterator<char>());
		if (prev == hash) {
			return false; // 最新
		}
	}

	 // --- 生成：親ディレクトリを作ってから書き出し ---
	if (outPath.has_parent_path()) {
		std::filesystem::create_directories(outPath.parent_path());
	}
	// std::ofstream out は寿命が切れたらファイルを閉じるので、ブロックで囲んだままにすること
	{
		std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
		out << def.hlslBody;
	}
	{
		std::ofstream out(hashPath, std::ios::binary | std::ios::trunc);
		out << hash;
	}
	return true;
}


//=============================================================================
// 全定義を生成
//=============================================================================
void ShaderPackageLoader::GenerateAll(const std::vector<ShaderDefinition>& defs, const std::filesystem::path& outRoot) {
	for (const auto& d : defs) {
		bool wrote = IsGenerate(d, outRoot);
		LogManager::Log(std::format("[shader] {} {}", wrote ? "generated" : "skipped(up-to-date)", ConvertString(std::wstring(d.meta.path))));
	}
}


//=============================================================================
// data/ 以下の .shader を全部 読み込み→生成する
//=============================================================================
void ShaderPackageLoader::LoadAll(const std::filesystem::path& dataDir, const std::filesystem::path& outRoot) {
	std::vector<ShaderDefinition> defs;

	// 再帰的にフォルダを見て .shader だけ拾う
	// std::filesystem::recursive_directory_iterator は、指定したディレクトリ以下を再帰的にすべて探索する
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dataDir)) {
		if (entry.is_regular_file() && entry.path().extension() == L".shader") {
			defs.push_back(ParseFile(entry.path()));
		}
	}

	// 全部まとめてディスクへ（存在/ハッシュでスキップ）
	GenerateAll(defs, outRoot);
}


//=============================================================================
// ヘルパー
//=============================================================================
std::string_view ShaderPackageLoader::Trim(std::string_view s) {
	constexpr const char* ws = " \t\r\n";
	size_t b = s.find_first_not_of(ws);
	if (b == std::string_view::npos) {
		return {};
	}
	return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::string ShaderPackageLoader::Require(const std::unordered_map<std::string, std::string>& kv, std::string_view key, std::string_view src) {
	auto it = kv.find(std::string(key));
	MY_ASSERT_MSG(it != kv.end(), std::format("{}: 必須フィールド '{}' がありません", src, std::string(key)));
	return it->second;
}

std::wstring ShaderPackageLoader::DefaultProfile(ShaderMeta::Stage s) {
	switch (s) {
	case ShaderMeta::Stage::VS:
		return L"vs_6_0";
	case ShaderMeta::Stage::PS:
		return L"ps_6_0";
	default:
		return L""; // Include はコンパイルしない
	}
}

template<class E> E ShaderPackageLoader::ParseEnum(std::string_view s, std::string_view key, std::string_view src) {
	auto e = magic_enum::enum_cast<E>(s);
	MY_ASSERT_MSG(e.has_value(), std::format("{}: {}='{}' は無効な値です", src, std::string(key), std::string(s)));
	return *e;
}