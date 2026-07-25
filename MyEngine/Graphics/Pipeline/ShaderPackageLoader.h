#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"

// --- .shaderに記述されているメタ情報 ---
struct ShaderMeta {
	// 使用するシェーダーステージ
	enum class Stage {
		VS, PS, Include
	};
	Stage stage{};        // どこで使うか
	std::string pairName; // VS必須（ペア参照キー）／PS任意
	std::wstring path;    // 生成先（ルート相対）
	std::wstring profile; // 空ならstageから既定
	std::string entry = "main";
	// PSのみ
	DrawCategory drawCategory{};
	ShadingType shading = ShadingType::Unlit;
	std::string vsName; // ペアVSの name
};

// --- .shaderの全内容 ---
struct ShaderDefinition {
	ShaderMeta meta;      // #META～#META_END の情報
	std::string hlslBody; // #HLSL〜#HLSL_END の原文
	std::string source;   // エラー表示用（ファイル名）
};

// --- Shaderファイル情報 ---
struct VSInfo {
	std::wstring path, profile, entry;
};
struct PSInfo {
	std::wstring path, profile, entry;
};

// --- 描画する時に使うシェーダー情報 ---
struct ShaderProgramInfo {
	std::string name; // 表示名(PSのname)
	DrawCategory drawCategory{};
	ShadingType shadingType = ShadingType::Unlit;
	VSInfo vsInfo;
	PSInfo psInfo;
};


/// <summary>
/// .shaderを読む、見るクラス
/// </summary>
class ShaderPackageLoader {
public:

	static void Initilaize();
	static void Release();

	/// <summary>
	/// 1つの.shaderファイルを読んで.hlslを作成
	/// </summary>
	/// <param name="file">.shaderのパス</param>
	/// <param name="outRoot">.hlslを置きたいパス</param>
	static void Load(const std::filesystem::path& file, const std::filesystem::path& outRoot);

	/// <summary>
	/// data/ 以下の .shader を全部 読み込み→作成する
	/// </summary>
	/// <param name="dataDir">shader/dataなど.shaderが含まれているフォルダパス
	/// <param name="outRoot">.hlslの生成フォルダ
	static void LoadAll(const std::filesystem::path& dataDir, const std::filesystem::path& outRoot);

	/// <summary>
	/// 描画設定からシェーダーキャッシュを取り出す
	/// </summary>
	static size_t GetProgramIndex(DrawCategory category, ShadingType type);

	/// <summary>
	/// シェーダーキャッシュからシェーダー情報を取り出す
	/// </summary>
	/// <param name="index">GetProgramIndexの戻り値</param>
	static const ShaderProgramInfo& GetProgramAt(size_t index) { return instance_->programs_[index]; }

	// シェーダーのペア（vs, psなど）の総数
	static size_t GetProgramCount() { return instance_->programs_.size(); }


private:
	static bool Generate(const ShaderDefinition& def, const std::filesystem::path& outRoot); // outRoot/path に書き出す。中身が同じならスキップ
	static ShaderDefinition ParseFile(const std::filesystem::path& path); // ファイルを開いて全部読んでParseText関数に送る
	static ShaderDefinition ParseText(std::string_view text, std::string_view sourceName); // 中身を1行づつ読んでShaderDefinitionに格納
	static uint32_t DrawKey(DrawCategory c, ShadingType s) { return (uint32_t(c) << 8) | uint32_t(s); } // 描画設定をキーにする
	static ShaderMeta ParseMeta(const std::vector<std::string>& metaLines, std::string_view src); // .shaderの#METAを解析
	static void Register(const ShaderDefinition& def);
	static std::string_view Trim(std::string_view s);  // 空白を削除
	static std::string Require(const std::unordered_map<std::string, std::string>& kv, std::string_view key, std::string_view src); // 一致しているか
	static std::wstring DefaultProfile(ShaderMeta::Stage s);  // ShaderのPrifileのデフォルト設定
	template<class E>  static E ParseEnum(std::string_view s, std::string_view key, std::string_view src); // 文字列→enum を magic_enum で一括変換

	static ShaderPackageLoader* instance_;
	std::unordered_map<std::string, VSInfo> vsByName_; // 名前→VS（値で保持）
	std::vector<ShaderDefinition> defs_; // .shaderに内容を保存する箱
	std::vector<ShaderProgramInfo> programs_; // 描画の際に使うShaderファイルの組み合わせを保存する箱
	std::unordered_map<uint32_t, size_t> programIndexByCatShading_;
};