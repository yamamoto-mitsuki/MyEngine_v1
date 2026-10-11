#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl.h>
#include <externals/magic_enum/magic_enum.hpp>

// 生成 .hlsl の置き場（Loader と Compiler で共有）
inline constexpr const wchar_t* kShaderGeneratedDir = L"MyEngine/Shader/Generated/";

// シェーダーから取り出した1つのResource情報
struct ShaderResourceBinding {
	std::string name;                // 変数名 "gCamera"
	D3D_SHADER_INPUT_TYPE type;      // 型 CBUFFER / TEXTURE / SAMPLER / STRUCTURED ...
	UINT registerNumber;             // register番号 (b2 の 2)
	UINT count;                      // 配列要素数。0 = 無制限(bindless)
	UINT space;                      // register space (space1 など)
	D3D_SRV_DIMENSION texSDimension; // テクスチャの形状（2D, Cubeなど）
};

// シェーダーから取り出した1つの入力パラメータ
struct ShaderInputParameter {
	std::string semanticName;                  // セマンティクス名 "POSITION"
	D3D_NAME systemValueType;                  // SV_VertexIDなどシステム値
	UINT semanticIndex;                        // セマンティクスのインデックス "TEXCOORD0"の0
	UINT registerIndex;                        // 入力スロット内でのパラメータ順序（レジスタ位置）
	D3D_REGISTER_COMPONENT_TYPE componentType; // float, int などの型
	BYTE mask;                                 // 0x0F(xyzw),0x07(xyz)など成分数を示すビットマスク
	BYTE readWriteMask;                        // float3でもシェーダーがxyしか使ってないなど判断してくれる
};

// 1つのシェーダに入ってる情報をまとめたもの
struct ShaderReflection {
	std::vector<ShaderResourceBinding> resources; // 全Reosurce情報
	std::vector<ShaderInputParameter> inputs;     // 全Input情報
	Microsoft::WRL::ComPtr<IDxcBlob> blob;        // コンパイル結果
};


/// <summary>
/// シェーダーのコンパイルと保管を管理するクラス
/// </summary>
class ShaderCompiler {
public:
	static void Initialize();
	static void Release();

	/// <summary>
	/// Shader/Generatedルートの相対パスでコンパイル&reflection取得（キャッシュ付き）
	/// </summary>
	static const ShaderReflection& CompileShaderReflection(const std::wstring& path, const std::wstring& profile, const std::wstring& entry = L"main");

private:
	/// <summary>
	/// シェーダーのコンパイルを行う
	/// </summary>
	static Microsoft::WRL::ComPtr<IDxcResult> CompileShader(const std::wstring& fullPath, const std::wstring& profile, const std::wstring& entry);

	/// <summary>
	/// シェーダーコンパイル結果からShaderBlobを取得する
	/// </summary>
	/// <param name="result">CompileShader関数の戻り値</param>
	static Microsoft::WRL::ComPtr<IDxcBlob> GetShaderBlob(IDxcResult* result);

	/// <summary>
	/// シェーダーコンパイルの結果からレジスタなど情報を取得する
	/// </summary>
	/// <param name="result">CompileShader関数の戻り値</param>
	static std::vector<ShaderResourceBinding> MakeShaderResourceBinding(IDxcResult* result);

	/// <summary>
	/// シェーダーコンパイルの結果から入力パラメータを取得する
	/// </summary>
	/// <param name="result">CompileShader関数の戻り値</param>
	static std::vector<ShaderInputParameter> MakeShaderInputParameter(IDxcResult* result);


	ShaderCompiler() = default;
	~ShaderCompiler() = default;

	static ShaderCompiler* instance_;
	std::unordered_map<std::wstring, ShaderReflection> cache_;
};