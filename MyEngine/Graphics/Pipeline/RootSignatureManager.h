#pragma once
#include <span>
#include <array>
#include <optional>

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
	SamplingType type;              // Sampler設定
	UINT shaderRegister;            // レジスタ番号
};
// --- Sampler情報をまとめたもの ---
inline constexpr std::array kStaticSamplerDefaultLayout = {
    StaticSampler{"gLinearWrap", SamplingType::LinearWrap, 0}, // s0
    // StaticSampler{"gLinearClamp", SamplingType::LinearClamp, 1}, // s1
    // StaticSampler{"gPointClamp",  SamplingType::PointClamp,  2}, // s2
    // StaticSampler{"gShadowMap",   SamplingType::ShadowMap,   3}, // s3
};

//==========================================
// RootParameters全要素の情報
//==========================================
// register のBind情報である RootParameter を全種類分格納したもの
enum class RootSignatureID {
	Sprite,
	ModelLit,
	ModelUnlit,
	Line,
};
// レジスタに送るリソースの役割
enum class RootBind {
	TransformationMatrix, // TransformationMatrixData (VS)
	Material,             // Material3dData / Material2dData / MaterialLineData (PS)
	DirectionalLight,     // DirectionalLightData (PS)
	Camera,               // CameraData (PS)
	WindowSize,           // ウィンドウサイズ (VS, Spriteのみ)
	BindlessTexture,      // バインドレステクスチャ（対応する構造体なし）
};
// スロットがどのShaderとバインドするか
enum class BindType {
	CBV_VS,
	CBV_PS,
	BindlessTexture,
};
// 1個分の定義
struct RootParameter {
	RootBind bind;       // 役割（スロット検索キー）
	BindType type;       // リソース型とどのShaderで使うか（例: BindType::CBV_VS）
	UINT shaderRegister; // レジスタ番号
};
// --- 各RootParameters情報をまとめたもの ---
// Sprite
inline constexpr std::array kRootParametersSpriteLayout = {
    RootParameter{RootBind::WindowSize,      BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{RootBind::Material,        BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{RootBind::BindlessTexture, BindType::BindlessTexture, 0}, // [2] t0 PS
};
// ModelLit
inline constexpr std::array kRootParametersModelLitLayout = {
    RootParameter{RootBind::TransformationMatrix, BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{RootBind::Material,             BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{RootBind::DirectionalLight,     BindType::CBV_PS,          1}, // [2] b1 PS
    RootParameter{RootBind::Camera,               BindType::CBV_PS,          2}, // [3] b2 PS
    RootParameter{RootBind::BindlessTexture,      BindType::BindlessTexture, 0}, // [4] t0 PS
};
// ModelLit
inline constexpr std::array kRootParametersModelUnlitLayout = {
    RootParameter{RootBind::TransformationMatrix, BindType::CBV_VS,          0}, // [0] b0 VS
    RootParameter{RootBind::Material,             BindType::CBV_PS,          0}, // [1] b0 PS
    RootParameter{RootBind::BindlessTexture,      BindType::BindlessTexture, 0}, // [2] t0 PS
};
// Line
inline constexpr std::array kRootParametersLineLayout = {
    RootParameter{RootBind::TransformationMatrix, BindType::CBV_VS, 0}, // [0] b0 VS
    RootParameter{RootBind::Material,             BindType::CBV_PS, 0}, // [1] b0 PS
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
	/// ShadingType から RootSignatureID を入手
	/// </summary>
	/// <param name="drawCategory">描画したい形状（Sprite, Model, Line）</param>
	/// <param name="shadingType">HalfLambert, Unlitなどの表現したいShading</param>
	/// <returns>GetRootSignature で使うRootSignatureID</returns>
	static RootSignatureID GetRootSignatureID(DrawCategory drawCategory, ShadingType shadingType);

	//==========================================
	// ID → 取得
	//==========================================

	/// <summary>
	/// RootSignature内での役割から、レジスタ番号を取得。そのRootSignatureにないならnullptr
	/// </summary>
	/// <param name="id">GetRootSignatureIDから取得できるID</param>
	/// <param name="bind">探したいレジスタ番号のリソース型。RootBind::~ で探す</param>
	/// <returns></returns>
	static std::optional<UINT> GetBindSlot(RootSignatureID id, RootBind bind);

	/// <summary>
	/// RootSignatureID から 格納された RootSignature を返す
	/// <para>
	/// </summary>
	/// <param name="id">GetRootSignatureID　から入手できる ID</param>
	/// <param name="sampler">サンプリングタイプ</param>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature(RootSignatureID id);

private:
	//==========================================
	// 1. ID → Layout（設計図）
	//==========================================

	/// <summary>
	/// SamplerLayout を入手
	/// </summary>
	static std::span<const StaticSampler> GetStaticSamplerLayout(RootSignatureID id);

	/// <summary>
	/// RootSignatureID から  RootParametersLayout を格納した配列を入手
	/// <para>RootSignature 作成時に使用
	/// </summary>
	static std::span<const RootParameter> GetRootParametersLayout(RootSignatureID id);

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
	/// <para>return CreateRootSignature; で終わるので、ID3D12RootSignature を返す</para>
	/// </summary>
	/// <returns></returns>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> MakeRootSignature(RootSignatureID id);

	/// <summary>
	/// 引数の設定を元に RootSignature を作成する。
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc);

	// インスタンス
	static RootSignatureManager* instance_;
	// 作成した RootSignature を格納する
	std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, magic_enum::enum_count<RootSignatureID>()> rootSignatures_;
};