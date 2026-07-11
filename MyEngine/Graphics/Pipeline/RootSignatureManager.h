#pragma once
#include <span>
#include <array>
#include <string_view>

#include <d3d12.h>
#include <wrl.h>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"

//==========================================
// Sampler全要素の情報
//==========================================
// StaticSampler1個分の定義
struct StaticSampler {
	std::string_view variablesName; // hlslで宣言した変数名（例: gSampler）
	SamplingType type;               // Sampler設定
	UINT shaderRegister;            // レジスタ番号
};
// --- Sampler情報をまとめたもの ---
// 今の状態。将来的にふやそうぜ
inline constexpr std::array kStaticSamplerLayout = {
    StaticSampler{"gLinearWrap",  SamplingType::LinearWrap,  0}, // s0
    //StaticSampler{"gLinearClamp", SamplingType::LinearClamp, 1}, // s1
    //StaticSampler{"gPointClamp",  SamplingType::PointClamp,  2}, // s2
    //StaticSampler{"gShadowMap",   SamplingType::ShadowMap,   3}, // s3
};

//==========================================
// RootParameters全要素の情報
//==========================================
// register のBind情報である RootParameter を全種類分格納したもの
enum class RootParameterID {
	Sprite,
	ModelLit,
	ModelUnlit,
	Line,
};
// スロットがどのShaderとバインドするか
enum class BindType {
	CBV_VS,
	CBV_PS,
	BindlessTexture,
};
// 1個分の定義
struct RootParameter {
	std::string_view variableName; // hlslで宣言した変数名（例: gMaterial）
	BindType type;                 // リソース型とどのShaderで使うか（例: BindType::CBV_VS）
	UINT shaderRegister;           // レジスタ番号
};
// --- 各RootParameters情報をまとめたもの ---
// Sprite
inline constexpr std::array kRootParametersSpriteLayout = {
    RootParameter{"gWindowSize", BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{"gMaterial",   BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{"gTextures",   BindType::BindlessTexture, 0}, // [2] t0 PS
};
// ModelLit
inline constexpr std::array kRootParametersModelLitLayout = {
    RootParameter{"gTransformationMatrix", BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{"gMaterial",             BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{"gDirectionalLight",     BindType::CBV_PS,          1}, // [2] b1 PS
    RootParameter{"gCamera",               BindType::CBV_PS,          2}, // [3] b2 PS
    RootParameter{"gTextures",             BindType::BindlessTexture, 0}, // [4] t0 PS
};
// ModelLit
inline constexpr std::array kRootParametersModelUnlitLayout = {
    RootParameter{"gTransformationMatrix", BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{"gMaterial",             BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{"gTextures",             BindType::BindlessTexture, 0}, // [4] t0 PS
};
// Line
inline constexpr std::array kRootParametersLineLayout = {
    RootParameter{"gTransformationMatrix", BindType::CBV_VS, 0}, // [0] b0 VS
    RootParameter{"gMaterial",             BindType::CBV_PS, 0}, // [1] b0 PS
};


/// <summary>
/// RootSignatureの生成、登録、管理をするクラス
/// </summary>
class RootSignatureManager {
public:
	static void Initialize();
	static void Release();

	//==========================================
	// 描画情報 → IDを取得
	//==========================================

	/// <summary>
	/// ShadingType から RootParameterID を入手
	/// </summary>
	/// <param name="drawCategory">描画したい形状（Sprite, Model, Line）</param>
	/// <param name="shadingType">HalfLambert, Unlitなどの表現したいShading</param>
	/// <returns>GetRootSignature で使うRootParameterID</returns>
	static RootParameterID GetRootParameterID(DrawCategory drawCategory, ShadingType shadingType);


	//==========================================
	// ID → 取得
	//==========================================

	/// <summary>
	/// RootParameterID から 格納された RootSignature を返す
	/// <para>
	/// </summary>
	/// <param name="id">GetRootParameterID　から入手できる ID</param>
	/// <param name="sampler">サンプリングタイプ</param>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature(RootParameterID id);


private:
	//==========================================
	// 1. ID → Layout（設計図）
	//==========================================

	/// <summary>
	/// RootParameterID から  RootParametersLayout を格納した配列を入手
	/// <para>RootSignature 作成時に使用
	/// </summary>
	static std::span<const RootParameter> GetRootParametersLayout(RootParameterID id);

	//==========================================
	// 2. Layout（設計図） → D3D12型
	//==========================================

	/// <summary>
	/// Samplerの設定
	/// </summary>
	static D3D12_STATIC_SAMPLER_DESC MakeStaticSampler(const StaticSampler& sampler);

	/// <summary>
	/// layout（設計図）から RootParameters を返す
	/// </summary>
	/// <returns>RootSignature 作成に使う D3D12_ROOT_PARAMETER1</returns>
	static std::vector<D3D12_ROOT_PARAMETER1> MakeRootParameters(std::span<const RootParameter> layout, D3D12_DESCRIPTOR_RANGE1& outRange);

	//==========================================
	// 3. D3D12 RootParameter 部品生成
	//==========================================

	/// <summary>
	/// バインドレスSRVのDescriptorTableパラメータを作成する。必ずparams[0]に配置すること
	/// </summary>
	static D3D12_ROOT_PARAMETER1 CreateRootParameterBindlessTable(D3D12_DESCRIPTOR_RANGE1& outRange);

	/// <summary>
	/// SRVのRootParameterを作成する
	/// </summary>
	/// <param name="visibility">使うシェーダーの種類</param>
	/// <param name="shaderRegister">レジスタ番号</param>
	/// <returns></returns>
	static D3D12_ROOT_PARAMETER1 CreateRootParameterSRV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister);

	/// <summary>
	/// 定数バッファ(CBV)のRootParameterを作成する
	/// </summary>
	/// <param name="visibility">使うシェーダーの種類</param>
	/// <param name="shaderRegister">レジスタ番号</param>
	static D3D12_ROOT_PARAMETER1 CreateRootParameterCBV(D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister);

	//==========================================
	// 4. RootSignature作成
	//==========================================

	/// <summary>
	/// ID から CreateRootSignatureに必要な設定を入れる
	/// </summary>
	/// <param name="id"></param>
	/// <returns></returns>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> MakeRootSignature(RootParameterID id);

	/// <summary>
	/// 引数の設定を元に RootSignature を作成する。
	/// <para>まだ登録されていない場合はここで登録を行う</para>
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_ROOT_PARAMETER1* params, 
		UINT paramCount, const D3D12_STATIC_SAMPLER_DESC* samplers, UINT samplerCount);


	// インスタンス
	static RootSignatureManager* instance_;
	// 作成した RootSignature を格納する
	std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, magic_enum::enum_count<RootParameterID>()> rootSignatures_;
};