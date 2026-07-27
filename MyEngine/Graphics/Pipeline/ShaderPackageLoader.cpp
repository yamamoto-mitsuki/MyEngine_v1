#include "ShaderPackageLoader.h"
#include <format>
#include <fstream>
#include <unordered_map>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Diagnostics/MyAssert.h"

// 静的メンバ変数
ShaderPackageLoader* ShaderPackageLoader::instance_ = nullptr;


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
// ゲッター
//=============================================================================
size_t ShaderPackageLoader::GetProgramIndex(DrawCategory c, ShadingType s) {
	auto it = instance_->programIndexByCatShading_.find(DrawKey(c, s));
	MY_ASSERT_MSG(it != instance_->programIndexByCatShading_.end(), std::format("program ({}/{}) が未登録", magic_enum::enum_name(c), magic_enum::enum_name(s)));
	return it->second;
}

const ShaderReflection& ShaderPackageLoader::GetShaderReflection(const std::string& name) {
	auto it = instance_->shadersByName_.find(name);
	MY_ASSERT_MSG(it != instance_->shadersByName_.end(), std::format("shader '{}' が未登録です", name));
	const ShaderEntry& s = it->second;
	return ShaderCompiler::CompileShaderReflection(s.path, s.profile, s.entry);
}


//=============================================================================
// .shader → .hlsl作成
//=============================================================================
// ===== 単体作成 =====
void ShaderPackageLoader::Load(const std::filesystem::path& file, const std::filesystem::path& outRoot) {
	ShaderDefinition def = ParseFile(file);
	Generate(def, outRoot); // hlsl作成
	Register(def);          // メンバ変数に登録
}

// ===== 一括 =====
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
		if (d.meta.stage == ShaderMeta::Stage::PS) {
			Register(d);
		}
	}
	LogManager::Log(std::format("[shader] {} shaders / {} programs", instance_->shadersByName_.size(), instance_->programs_.size()));
}


//=============================================================================
// outRoot/path に書き出す。中身が前回と同じならスキップ
//=============================================================================
bool ShaderPackageLoader::Generate(const ShaderDefinition& def, const std::filesystem::path& outRoot) {
	std::filesystem::path outPath = outRoot / def.meta.path;
	// --- .hlsl と中身が同じならスキップ ---
	if (std::filesystem::exists(outPath)) {
		std::ifstream in(outPath, std::ios::binary);
		std::string prev((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (prev == def.hlslBody) {
			return false;
		}
	}
	// 生成
	if (outPath.has_parent_path()) {
		std::filesystem::create_directories(outPath.parent_path());
	}
	std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
	out << def.hlslBody;
	return true;
}


//=============================================================================
// 追加登録
//=============================================================================
void ShaderPackageLoader::Register(const ShaderDefinition& def) {
	const std::string name = DeriveName(def.meta.path);
	// Includeはコンパイルしないので名前登録しない
	if (def.meta.stage != ShaderMeta::Stage::Include) {
		instance_->shadersByName_.insert_or_assign(name, ShaderEntry{def.meta.stage, def.meta.path, def.meta.profile, def.meta.entry});
		LogManager::Log(std::format("ShaderName: {}", name));
	} 
	// #PROGRAM があれば描画プログラム
	if (def.program.has) {
		ProgramEntry p;
		p.category = def.program.category;
		p.shading = def.program.shading;
		p.vsName = def.program.vsName;
		p.psName = name; // このファイル自身がPS
		p.name = std::format("{}{}", magic_enum::enum_name(p.category), magic_enum::enum_name(p.shading));

		const uint32_t key = DrawKey(p.category, p.shading);
		
		auto e = instance_->programIndexByCatShading_.find(key);
		// 同(cat,shading)は上書き（index維持）
		if (e != instance_->programIndexByCatShading_.end()) {
			instance_->programs_[e->second] = std::move(p); 
		} else {
			// 新規は末尾（既存indexは動かない）
			instance_->programIndexByCatShading_[key] = instance_->programs_.size();
			instance_->programs_.push_back(std::move(p));
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
	def.source = std::string(sourceName);
	std::vector<std::string> metaLines, programLines;
	std::string hlslBody;
	enum class Block { None, Meta, Program, Hlsl } block = Block::None;

	// --- 全文解析 ---
	size_t pos = 0;
	while (pos < text.size()) {
		// 空白無視
		size_t eol = text.find('\n', pos);
		std::string_view line = text.substr(pos, (eol == std::string_view::npos ? text.size() : eol) - pos);
		pos = (eol == std::string_view::npos) ? text.size() : eol + 1;
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		// #～を分類分け
		std::string_view t = Trim(line);
		if (t == "#META") {
			block = Block::Meta;
			continue;
		}
		if (t == "#META_END") {
			block = Block::None;
			continue;
		}
		if (t == "#PROGRAM") {
			block = Block::Program;
			continue;
		}
		if (t == "#PROGRAM_END") {
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
		// 分類分け結果を1行づつ見る
		switch (block) {
		case Block::Meta:
			metaLines.emplace_back(line);
			break;
		case Block::Program:
			programLines.emplace_back(line);
			break;
		case Block::Hlsl:
			hlslBody += line;
			hlslBody += '\n';
			break;
		default:
			break;
		}
	}
	def.meta = ParseMeta(metaLines, sourceName);
	def.program = ParseProgram(programLines, sourceName); // 空なら has=false
	def.hlslBody = std::move(hlslBody);
	return def;
}


//=============================================================================
// .shaderの #～ を解析
//=============================================================================
// ===== #META =====
ShaderMeta ShaderPackageLoader::ParseMeta(const std::vector<std::string>& lines, std::string_view src) {
	auto kv = ToKeyValues(lines, src);
	ShaderMeta m;
	m.stage = ParseEnum<ShaderMeta::Stage>(Require(kv, "stage", src), "stage", src);
	m.path = ConvertString(Require(kv, "path", src));

	auto e = kv.find("entry");
	if (e != kv.end()) {
		m.entry = ConvertString(e->second);
	}

	if (m.stage != ShaderMeta::Stage::Include) {
		m.profile = (kv.contains("profile")) ? ConvertString(kv.at("profile")) : DefaultProfile(m.stage);
	}
		
	return m;
}

// ===== #PROGRAM =====
ProgramDef ShaderPackageLoader::ParseProgram(const std::vector<std::string>& lines, std::string_view src) {
	ProgramDef p;
	// #PROGRAM 無し
	if (lines.empty()) {
		return p;
	}
		
	auto kv = ToKeyValues(lines, src);
	p.has = true;
	p.category = ParseEnum<DrawCategory>(Require(kv, "category", src), "category", src);
	p.vsName = Require(kv, "vs", src);
	if (auto s = kv.find("shading"); s != kv.end())
		p.shading = ParseEnum<ShadingType>(s->second, "shading", src);
	return p;
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

std::string ShaderPackageLoader::DeriveName(const std::wstring& path) {
	std::string stem = std::filesystem::path(path).stem().string();
	std::erase(stem, '.');
	return stem;
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

std::unordered_map<std::string, std::string> ShaderPackageLoader::ToKeyValues(const std::vector<std::string>& lines, std::string_view src) {
	std::unordered_map<std::string, std::string> kv;
	for (const auto& raw : lines) {
		std::string_view line = Trim(raw);
		if (line.empty() || line.starts_with("//")) {
			continue;
		}
		
		size_t colon = line.find(':');
		MY_ASSERT_MSG(colon != std::string_view::npos, std::format("{}: ':' がありません -> '{}'", src, std::string(line)));
		kv[std::string(Trim(line.substr(0, colon)))] = std::string(Trim(line.substr(colon + 1)));
	}
	return kv;
}