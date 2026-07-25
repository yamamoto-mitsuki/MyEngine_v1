#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"

// .shaderに記述されているメタ情報
struct ShaderMeta {
	// 使用するシェーダーステージ
	enum class Stage {
		VS, PS, Include
	};
	Stage stage{};        // どこで使うか
	std::string name;     // VS必須（ペア参照キー）／PS任意
	std::wstring path;    // 生成先（ルート相対）
	std::wstring profile; // 空ならstageから既定
	std::string entry = "main";
	// PSのみ
	DrawCategory drawCategory{};
	ShadingType shading = ShadingType::Unlit;
	std::string vsName; // ペアVSの name
};

// .shaderの全内容
struct ShaderDefinition {
	ShaderMeta meta;      // #META～#META_END の情報
	std::string hlslBody; // #HLSL〜#HLSL_END の原文
	std::string source;   // エラー表示用（ファイル名）
};


/// <summary>
/// .saderを読む、見るクラス
/// </summary>
class ShaderPackageLoader {
public:

	/// <summary>
	/// ファイルを開いて全部読んでParseText関数に送る
	/// </summary>
	/// <param name="path">.shaderのファイルパス</param>
	static ShaderDefinition ParseFile(const std::filesystem::path& path);

	/// <summary>
	/// 中身を1行づつ読んでShaderDefinitionに格納
	/// </summary>
	/// <param name="text"></param>
	/// <param name="sourceName"></param>
	static ShaderDefinition ParseText(std::string_view text, std::string_view sourceName);

	/// <summary>
	/// outRoot/path に書き出す。中身が同じならスキップ
	/// </summary>
	static bool IsGenerate(const ShaderDefinition& def, const std::filesystem::path& outRoot);

	/// <summary>
	/// 全定義を生成
	/// </summary>
	static void GenerateAll(const std::vector<ShaderDefinition>& defs, const std::filesystem::path& outRoot);

	/// <summary>
	/// data/ 以下の .shader を全部 読み込み→生成する
	/// </summary>
	/// <param name="dataDir">shader/dataなど.shaderが含まれているフォルダパス
	/// <param name="outRoot">.hlslの生成フォルダ
	static void LoadAll(const std::filesystem::path& dataDir, const std::filesystem::path& outRoot);

private:
	static ShaderMeta ParseMeta(const std::vector<std::string>& metaLines, std::string_view src);
	static std::string_view Trim(std::string_view s);
	static std::string Require(const std::unordered_map<std::string, std::string>& kv, std::string_view key, std::string_view src);
	static std::wstring DefaultProfile(ShaderMeta::Stage s);

	// 文字列→enum を magic_enum で一括変換（enum追加時にパーサ改修不要）
	template<class E> 
	static E ParseEnum(std::string_view s, std::string_view key, std::string_view src);
};