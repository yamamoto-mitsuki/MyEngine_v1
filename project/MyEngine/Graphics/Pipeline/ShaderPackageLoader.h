#pragma once
#include <memory>
#include <deque>
#include <string>
 #include <vector>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"

// #META（全 .shader 共通）
struct ShaderMeta {
	enum class Stage { VS, PS, CS, Include };
	Stage stage{};
	std::wstring path;
	std::wstring profile; // Include は空
	std::wstring entry = L"main";
};
// #PROGRAM（描画PSのみ。無ければ has=false）
struct ProgramDef {
	bool has = false;
	DrawCategory category{};
	ShadingType shading = ShadingType::Unlit;
	std::string vsName;
	std::string skinnedVsName; // skinnedVsがあれば、同じPSでSkinnedModelの組み合わせも作る（無ければ空）
};
// 1つの .shader を解析した結果（一時オブジェクト）
struct ShaderDefinition {
	ShaderMeta meta;
	ProgramDef program;
	std::string hlslBody;
	std::string source;
};
// レジストリ：1コンパイル単位（名前引き）
struct ShaderEntry {
	ShaderMeta::Stage stage{};
	std::wstring path, profile, entry;
};
// レジストリ：1描画プログラム（(cat,shading)引き）
struct ProgramEntry {
	std::string name;
	DrawCategory category{};
	ShadingType shading{};
	std::string vsName, psName;
};


/// <summary>
/// .shaderを読む、見るクラス
/// </summary>
class ShaderPackageLoader {
public:
	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class ShaderPackageLoader;
	};
	explicit ShaderPackageLoader(ConstructorKey) {} // 鍵を受け取るコンストラクタ


	static void Initialize();
	static void Release();

	/// <summary>
	/// 1つの.shaderファイルを読んで.hlslを作成。Shader/Generatedに作成される。
	/// </summary>
	/// <param name="file">.shaderのパス</param>
	static void Load(const std::filesystem::path& file);

	/// <summary>
	/// data/ 以下の .shader を全部 読み込み→作成する
	/// </summary>
	/// <param name="dataDir">shader/dataなど.shaderが含まれているフォルダパス
	static void LoadAll(const std::filesystem::path& dataDir);

	/// <summary>
	/// .shader を読むフォルダを決める
	/// <para>Release 以外：エンジンのフォルダ（MyEngine.props の MYENGINE_ROOT_DIR）を直接読む。.shader を直したら、次の起動で反映される</para>
	/// <para>Release・エンジンのフォルダが無い PC：exe の隣にコピーされた MyEngine/Shader/Data</para>
	/// </summary>
	static std::filesystem::path FindDataDir();

	/// <summary>
	/// 名前からShaderのResource, Input, コンパイル結果を取得
	/// </summary>
	static const ShaderReflection& GetShaderReflection(const std::string& name);

	/// <summary>
	/// 描画設定からシェーダーキャッシュを取り出す
	/// </summary>
	static size_t GetProgramIndex(DrawCategory category, ShadingType type);

	/// <summary>
	/// シェーダーキャッシュからシェーダー情報を取り出す
	/// </summary>
	/// <param name="index">GetProgramIndexの戻り値</param>
	static const ProgramEntry& GetProgramAt(size_t index) { return instance_->programs_[index]; }

	// シェーダーのペア（vs, psなど）の総数
	static size_t GetProgramCount() { return instance_->programs_.size(); }


private:
	static bool Generate(const ShaderDefinition& def); // outRoot/path に書き出す。中身が同じならスキップ
	static ShaderDefinition ParseFile(const std::filesystem::path& path); // ファイルを開いて全部読んでParseText関数に送る
	static ShaderDefinition ParseText(std::string_view text, std::string_view sourceName); // 中身を1行づつ読んでShaderDefinitionに格納
	static uint32_t DrawKey(DrawCategory c, ShadingType s) { return (uint32_t(c) << 8) | uint32_t(s); } // 描画設定をキーにする
	static ShaderMeta ParseMeta(const std::vector<std::string>& metaLines, std::string_view src); // .shaderの#METAを解析
	static ProgramDef ParseProgram(const std::vector<std::string>& lines, std::string_view src);  // .shaderの#PROGRAM解析
	static void Register(const ShaderDefinition& def);
	// 描画プログラムを1つ登録（同じ組み合わせは上書き）
	static void AddProgram(DrawCategory category, ShadingType shadingType, const std::string& vsName, const std::string& psName);
	static std::string_view Trim(std::string_view s);  // 空白を削除
	static std::string Require(const std::unordered_map<std::string, std::string>& kv, std::string_view key, std::string_view src); // 一致しているか
	static std::wstring DefaultProfile(ShaderMeta::Stage s); // ShaderのProfileのデフォルト設定
	static std::string DeriveName(const std::wstring& path); // ファイルパス（Model/Lambert.PS.hlsl）をLambertPSなどにと変換
	static std::unordered_map<std::string, std::string> ToKeyValues(const std::vector<std::string>&, std::string_view);
	template<class E> static E ParseEnum(std::string_view s, std::string_view key, std::string_view src);

	// シングルトン
	friend struct std::default_delete<ShaderPackageLoader>;
	~ShaderPackageLoader() = default;
	ShaderPackageLoader(const ShaderPackageLoader&) = delete;
	ShaderPackageLoader& operator=(const ShaderPackageLoader&) = delete;
	static std::unique_ptr<ShaderPackageLoader> instance_;
	std::unordered_map<std::string, ShaderEntry> shadersByName_;
	std::vector<ShaderDefinition> defs_; // .shaderに内容を保存する箱
	std::deque<ProgramEntry> programs_;  // 描画の際に使うShaderファイルの組み合わせを保存する箱
	std::unordered_map<uint32_t, size_t> programIndexByCatShading_;
};