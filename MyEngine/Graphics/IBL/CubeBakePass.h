#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "MyEngine/Graphics/RenderTarget/RenderTextureCube.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"


/// <summary>
/// HDR画像（Equirectangular形式）の画像をCubeの形に変換する
/// </summary>
class CubeBakePass {
public:
	// PSO, RootSignature, CBVを作成
	void Initialize(ShaderFile ps);
	// Env（環境マップ）に6面を記録する
	void Record(RenderTextureCube& env, D3D12_GPU_DESCRIPTOR_HANDLE srv);


private:
	void CreateRootSignature();
	void CreatePSO(ShaderFile ps);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cbBuffer_;
	uint8_t* cbMapped_ = nullptr;
	size_t cbSlotSize_ = 0;
};
