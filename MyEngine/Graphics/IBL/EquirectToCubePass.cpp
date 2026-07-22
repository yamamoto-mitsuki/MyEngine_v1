#include "EquirectToCubePass.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Math/MathIncludes.h"

using namespace Microsoft::WRL;

namespace {
// シェーダーファイルのパス
std::string vsPath;
std::string psPath;

constexpr uint32_t kEnvSize = 512; // 1面の画質
// CBufferに送るカメラデータ
struct EquirectCameraData {
	Matrix4x4 viewProj;
};
// 6面の視線（左手系）
const Vector3 kDirs[6] = {
    {1,  0,  0 },
    {-1, 0,  0 },
    {0,  1,  0 },
    {0,  -1, 0 },
    {0,  0,  1 },
    {0,  0,  -1}
};
const Vector3 kUps[6] = {
    {0, 1, 0 },
    {0, 1, 0 },
    {0, 0, -1},
    {0, 0, 1 },
    {0, 1, 0 },
    {0, 1, 0 }
};
// 原点視点のビュー行列（左手: right = cross(up, forward)）
Matrix4x4 MakeCubeView(const Vector3& f, const Vector3& up) {
	Vector3 r = Normalize(Cross(up, f));
	Vector3 u = Cross(f, r);
	Matrix4x4 v = {};
	v.m[0][0] = r.x;
	v.m[0][1] = u.x;
	v.m[0][2] = f.x;
	v.m[1][0] = r.y;
	v.m[1][1] = u.y;
	v.m[1][2] = f.y;
	v.m[2][0] = r.z;
	v.m[2][1] = u.z;
	v.m[2][2] = f.z;
	v.m[3][3] = 1.0f;
	return v;
}
// 256アライメントを守る
size_t AlignTo256(size_t s) { return (s + 255) & ~size_t(255); }
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void EquirectToCubePass::Initilaize() {
	// CBuffer
	cbSlotSize_ = AlignTo256(sizeof(EquirectCameraData));
	cbBuffer_ = DirectXCommon::CreateUploadBuffer(cbSlotSize_ * 6);
	cbBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cbMapped_));
	// RootSigature, PSO
	CreateRootSignature();
	CreatePSO();
}


//=============================================================================
// RootSignature作成
//=============================================================================
void EquirectToCubePass::CreateRootSignature() { 
	// ===== Sampler =====
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ShaderRegister = 0; // s0
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	
	// ===== RootParameter =====
	D3D12_ROOT_PARAMETER params[2]{};
	// b0 CBV（VS） viewProj
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[0].Descriptor.ShaderRegister = 0;
	// t0 SRV table (PS) equirect
	D3D12_DESCRIPTOR_RANGE srvRange{};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0; // t0
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &srvRange;

	// ===== RootSignature =====
	// desc
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 2;
	desc.pParameters = params;
	desc.NumStaticSamplers = 1;
	desc.pStaticSamplers = &sampler;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	// 作成
	ComPtr<ID3DBlob> blob, error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	MY_ASSERT_MSG(SUCCEEDED(hr), "IBL RootSignature Serialize失敗");
	hr = DirectXCommon::GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	MY_ASSERT_MSG(SUCCEEDED(hr), "IBL RootSignature作成失敗");
}


//=============================================================================
// Env（環境マップ）に6面を記録する
//=============================================================================
void Record(RenderTextureCube& env, D3D12_GPU_DESCRIPTOR_HANDLE equirectSrv) { 
	auto* cmdList = DirectXCommon::GetCommandList(); 
	// ResourceBarrier
	DirectXCommon::TransitionBarrier(env.GetResource(), 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // Befor
		D3D12_RESOURCE_STATE_RENDER_TARGET);        // Afetr

	DirectXCommon::TransitionBarrier(env.GetResource(), 
		D3D12_RESOURCE_STATE_RENDER_TARGET, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}


//=============================================================================
// PSO作成
//=============================================================================
void EquirectToCubePass::CreatePSO() {}