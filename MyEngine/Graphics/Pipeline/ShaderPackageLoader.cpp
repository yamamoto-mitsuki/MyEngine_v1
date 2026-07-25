#include "ShaderPackageLoader.h"
#include <format>
#include <fstream>
#include <unordered_map>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Diagnostics/MyAssert.h"

// 静的メンバ変数
ShaderPackageLoader* ShaderPackageLoader::instance_ = nullptr;

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
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void ShaderPackageLoader::Initilaize() { 
	instance_ = new ShaderPackageLoader(); 
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void ShaderPackageLoader::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// 描画設定からシェーダーキャッシュを取り出す
//=============================================================================
size_t ShaderPackageLoader::GetProgramIndex(DrawCategory c, ShadingType s) {
	auto it = instance_->programIndexByCatShading_.find(DrawKey(c, s));
	MY_ASSERT_MSG(it != instance_->programIndexByCatShading_.end(), std::format("program ({}/{}) が未登録", magic_enum::enum_name(c), magic_enum::enum_name(s)));
	return it->second;
}


//=============================================================================
// .shader → .hlsl作成
//=============================================================================
// ===== 単体作成 =====
// 単発：1ファイルを parse→generate→登録（追加）。エンジンもプロジェクトもこれ経由
void ShaderPackageLoader::Load(const std::filesystem::path& file, const std::filesystem::path& outRoot) {
	ShaderDefinition def = ParseFile(file);
	Generate(def, outRoot); // hlsl作成
	Register(def);          // メンバ変数に登録
}


void ShaderPackageLoader::LoadAll(const std::filesystem::path& dataDir, const std::filesystem::path& outRoot) {
	std::vector<ShaderDefinition> defs;
	// 再帰的にフォルダを見て .shader だけ拾う
	// std::filesystem::recursive_directory_iterator は、指定したディレクトリ以下を再帰的にすべて探索する
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dataDir)) {
		if (entry.is_regular_file() && entry.path().extension() == L".shader") {
			defs.push_back(ParseFile(entry.path()));
		}
	}
	for (auto& d : defs) {
		Generate(d, outRoot); // hlsl作成
	}
	// VS, Includeを先に登録
	for (auto& d : defs) {
		if (d.meta.stage != ShaderMeta::Stage::PS) {
			Register(d); 
		}	
	}
	// PS
	for (auto& d : defs) {
		if (d.meta.stage != ShaderMeta::Stage::PS) {
			Register(d);
		}
	}
}


//=============================================================================
// outRoot/path に書き出す。中身が前回と同じならスキップ
//=============================================================================
bool ShaderPackageLoader::Generate(const ShaderDefinition& def, const std::filesystem::path& outRoot) {
	std::filesystem::path outPath = outRoot / def.meta.path; // 例: MyEngine/shader/Model/Object3dLambert.PS.hlsl
	std::filesystem::path hashPath = outPath;
	hashPath += L".hash";
	// 今の中身のハッシュ（16進文字列）
	const std::string hash = std::format("{:016x}", Fnv1a(def.hlslBody));
	// .hlsl と .hash が両方あって、ハッシュ一致ならスキップ
	if (std::filesystem::exists(outPath) && std::filesystem::exists(hashPath)) {
		std::ifstream hin(hashPath, std::ios::binary);
		std::string prev((std::istreambuf_iterator<char>(hin)), std::istreambuf_iterator<char>());
		if (prev == hash)
			return false; // 最新
	}

	// 生成：親ディレクトリを作ってから書き出し
	if (outPath.has_parent_path())
		std::filesystem::create_directories(outPath.parent_path());
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
// 追加登録
//=============================================================================
void ShaderPackageLoader::Register(const ShaderDefinition& def) {
	const ShaderMeta& m = def.meta;
	switch (m.stage) {
	case ShaderMeta::Stage::Include: // hlsli
		return; // 生成だけ。登録不要
	case ShaderMeta::Stage::VS: // VS
		instance_->vsByName_[m.pairName] = {m.path, m.profile, ConvertString(m.entry)};
		return;
	case ShaderMeta::Stage::PS: { // PS
		auto it = instance_->vsByName_.find(m.vsName);
		MY_ASSERT_MSG(it != instance_->vsByName_.end(), std::format("{}: vs '{}' 未登録（VSを先にLoadしてください）", def.source, m.vsName));
		// 情報を入れる
		ShaderProgramInfo p;
		p.name = m.pairName.empty() ? std::format("{}_{}", magic_enum::enum_name(m.drawCategory), magic_enum::enum_name(m.shading)) : m.pairName;
		p.drawCategory = m.drawCategory;
		p.shadingType = m.shading;
		p.vsInfo.path = it->second.path;
		p.vsInfo.profile = it->second.profile;
		p.vsInfo.entry = it->second.entry;
		p.psInfo.path = m.path;
		p.psInfo.profile = m.profile;
		p.psInfo.entry = ConvertString(m.entry);
		// 描画情報とキーを結びつける
		const uint32_t key = DrawKey(m.drawCategory, m.shading);
		if (auto e = instance_->programIndexByCatShading_.find(key); e != instance_->programIndexByCatShading_.end()) {
			instance_->programs_[e->second] = std::move(p); // 同じ(cat,shading)は上書き（indexは維持＝ホットリロード対応）
		} else {
			instance_->programIndexByCatShading_[key] = instance_->programs_.size();
			instance_->programs_.push_back(std::move(p)); // 新規は末尾に追加＝既存indexは動かない
		}
		return;
	}
	}
}


//=============================================================================
// .shaderの内容を見る
//=============================================================================
ShaderDefinition ShaderPackageLoader::ParseFile(const std::filesystem::path& path) {
	// バイナリモードで開く
	std::ifstream ifs(path, std::ios::binary);
	MY_ASSERT_MSG(ifs.is_open(), std::format(".shaderを開けません: {}", path.string()));
	// ファイルを先頭から終端まで見る
	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	return ParseText(content, path.filename().string());
}


//=============================================================================
// .shaderの内容を解析
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

	def.meta = ParseMeta(metaLines, sourceName); // #METAの解析
	def.hlslBody = std::move(hlslBody);          // hlslの内容を入れる
	return def;
}


//=============================================================================
// .shaderの #META を解析
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
		m.pairName = Require(kv, "name", src); // ペア参照キー
		break;

	case ShaderMeta::Stage::PS: // PS
		m.drawCategory = ParseEnum<DrawCategory>(Require(kv, "drawCategory", src), "drawCategory", src);
		m.vsName = Require(kv, "vs", src); // ペアVS
		if (auto s = kv.find("shading"); s != kv.end()) {
			m.shading = ParseEnum<ShadingType>(s->second, "shading", src); // 省略時 Unlit
		}
		if (auto n = kv.find("name"); n != kv.end()) {
			m.pairName = n->second; // 任意（名前引き用）
		}
		break;

	case ShaderMeta::Stage::Include: // hlsli
		break; // path のみでOK
	}
	return m;
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