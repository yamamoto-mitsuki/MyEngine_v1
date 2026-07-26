#pragma once
#include <span>
#include <array>
#include <optional>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"


// レジスタに送るリソースの役割
enum class RootBind {
	TransformationMatrix, // TransformationMatrixData (VS)
	Material,             // Material3dData / Material2dData / MaterialLineData (PS)
	DirectionalLight,     // DirectionalLightData (PS)
	PointLights,          // PointLight（PS）
	Camera,               // CameraData (PS)
	Particles,            // Particle
	WindowSize,           // ウィンドウサイズ (VS, Spriteのみ)
	Skybox,               // 天球
	IBL,                  // Image Base Light（PS）
	BindlessTexture,      // バインドレステクスチャ
	BindlessTextureCube,  // バインドレスキューブテクスチャ
};

// マージ後の1リソース（reflection情報 + どのステージから見えるか）
struct MergedBind {
	ShaderResourceBinding bind;         // シェーダーの1つのResource情報
	D3D12_SHADER_VISIBILITY visibility; // VS, PSなどどこで使うか
};

// ルートシグニチャとルートパラメーターの番号をまとめたもの
struct RootSignatureInfo {
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	std::unordered_map<RootBind, UINT> slotOf; // ルートパラメータ番号
};


/// <summary>
/// RootSignatureの生成、登録、管理をするクラス
/// </summary>
class RootSignatureManager {
public:
	static void Initialize();
	static void Release();

	/// <summary>
	/// vs, psなどペアのシェーダーを結びつける
	/// </summary>
	static std::vector<MergedBind> MergeStages(const std::vector<ShaderResourceBinding>& vs, const std::vector<ShaderResourceBinding>& ps);

	/// <summary>
	/// 自動でRootSignature作成
	/// </summary>
	static RootSignatureInfo MakeRootSignatureInfo(const std::vector<MergedBind>& binds);


private:
	/// <summary>
	/// シェーダーのResource名とRootBindを結びつける
	/// </summary>
	/// <param name="name">シェーダーのResource名</param>
	static std::optional<RootBind> NameToRole(std::string_view name);

   
    static D3D12_ROOT_PARAMETER1 MakeRootParameter(const MergedBind& m, std::vector<D3D12_DESCRIPTOR_RANGE1>& ranges);

	static D3D12_STATIC_SAMPLER_DESC MakeStaticSampler(const MergedBind& m);

	/// <summary>
	/// 引数の設定を元に RootSignature を作成する。
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc);

	// インスタンス
	static RootSignatureManager* instance_;
};