#pragma once
#include <array>
#include <string>
#include <wrl.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <externals/magic_enum/magic_enum.hpp>


// 1シェーダーから取り出した1つのBind情報
struct ReflectedBind {
	std::string name;           // "gCamera"
	D3D_SHADER_INPUT_TYPE type; // CBUFFER / TEXTURE / SAMPLER / STRUCTURED ...
	UINT reg;                   // register番号 (b2 の 2)
	UINT space;                 // register space (space1 など)
	UINT count;                 // 配列要素数。0 = 無制限(bindless)
	D3D12_SHADER_VISIBILITY visibility;
};

// コンパイルする個々のシェーダーファイル
enum class ShaderFile {
	// VS
	Object3dVS,
	ParticleVS,
	Sprite2dVS,
	Line3dVS,
	EquirectToCubeVS,
	BrdfLutVS,
	// PS
	LambertPS,
	HalfLambertPS,
	PhongPS,
	BlinnPhongPS,
	PBRPS,
	EquirectToCubePS,
	IrradiancePS,
	PrefilterPS,
	BrdfLutPS,
	UnlitPS,
	ParticlePS,
	Sprite2dPS,
	Line3dPS,
};


/// <summary>
/// シェーダーのコンパイルと保管を管理するクラス
/// </summary>
class ShaderCompiler {
public:
	static void Initialize();
	static void Release();

	/// <summary>
	/// ShaderFile を1つ取得。未コンパイルなら生成してキャッシュ。
	/// </summary>
	/// <param name="file"></param>
	/// <returns></returns>
	static IDxcBlob* GetShaderFile(ShaderFile file);

	/// <summary>
	/// 全てのシェーダーをコンパイルする
	/// </summary>
	static void CompileAll();

	/// <summary>
	/// ShaderがどんなResourcesを使っているかを取得
	/// </summary>
	std::vector<ReflectedBind> Reflect(IDxcResult* result, D3D12_SHADER_VISIBILITY stage);

	/// <summary>
	/// VS, PSなどどこで使うか
	/// </summary>
	std::vector<ReflectedBind> MergeStages(std::vector<ReflectedBind> vs, const std::vector<ReflectedBind>& ps);


private:
	/// <summary>
	/// シェーダーのコンパイルを行う
	/// </summary>
	/// <param name="path">読みたいシェーダー</param>
	/// <param name="profile">シェーダーのバージョン</param>
	/// <returns></returns>
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& path, const wchar_t* profile);


	ShaderCompiler() = default;
	~ShaderCompiler() = default;

	// インスタンス
	static ShaderCompiler* instance_;

	// ShaderFileごとのコンパイル結果
	std::array<Microsoft::WRL::ComPtr<IDxcBlob>, magic_enum::enum_count<ShaderFile>()> cache_;
};