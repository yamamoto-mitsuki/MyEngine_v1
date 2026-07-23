#pragma once
#include <array>
#include <string>

#include <wrl.h>
#include <dxcapi.h>

#include <externals/magic_enum/magic_enum.hpp>

// コンパイルする個々のシェーダーファイル
enum class ShaderFile {
	// VS
	Object3dVS,
	ParticleVS,
	Sprite2dVS,
	Line3dVS,
	EquirectToCubeVS,
	// PS
	LambertPS,
	HalfLambertPS,
	PhongPS,
	BlinnPhongPS,
	PBRPS,
	EquirectToCubePS,
	IrradiancePS,
	PrefilterPS,
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