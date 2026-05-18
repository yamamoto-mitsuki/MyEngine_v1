#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>
#include <wrl.h>

class DirectXCommon {
public:
	static constexpr UINT kSwapChainBufferCount = 2;
	static constexpr UINT kSRVDescriptorHeap = 128;
	uint32_t descriptorSizeSRV = 0;
	uint32_t descriptorSizeRTV = 0;
	uint32_t descriptorSizeDSV = 0;

public:
	~DirectXCommon();

	/// <summary>
	/// 初期化
	/// </summary>
	void Init();

	/// <summary>
	/// GPUの完了待ち
	/// </summary>
	void WaitForGPU();

	/// <summary>
	/// シェーダーをコンパイルしてBlobで返す
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob>
	    CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, 
		IDxcIncludeHandler* includeHandler, const std::vector<std::wstring>& defines = {}); 
	/// <summary>
	/// GraphicsPipelineStateObjectを生成
	/// </summary>
	/// <param name="rootSignature">使用するRootSignature</param>
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(IDxcBlob* vs, IDxcBlob* ps, D3D12_INPUT_LAYOUT_DESC inputLayout, 
		ID3D12RootSignature* rootSignature, D3D12_DEPTH_STENCIL_DESC depthStencilDesc, 
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE); // ここは基本触らない。(線の描画のみ変更)

	/// <summary>
	/// BufferResourceの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// <summary>
	/// DepthStencilTextureResourceの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	/// <summary>
	/// ディスクリプタヒープを生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// 指定インデックスのCPUDescriptorHandleを取得する
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	/// <summary>
	/// 指定インデックスのGPUDescriptorHandleを取得する
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	// ゲッター
	IDXGIFactory7* GetFactory() const { return dxgiFactory_.Get(); }
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

	ID3D12PipelineState* GetPSO3dLitTex() const { return pso3dLitTex_.Get(); }
	ID3D12PipelineState* GetPSO3dLitNoTex() const { return pso3dLitNoTex_.Get(); }
	ID3D12PipelineState* GetPSO3dHalfLitTex() const { return pso3dHalfLitTex_.Get(); }
	ID3D12PipelineState* GetPSO3dHalfLitNoTex() const { return pso3dHalfLitNoTex_.Get(); }
	ID3D12PipelineState* GetPSO3dNoLitTex() const { return pso3dNoLitTex_.Get(); }
	ID3D12PipelineState* GetPSO3dNoLitNoTex() const { return pso3dNoLitNoTex_.Get(); }
	ID3D12PipelineState* GetPSO3dLine() const { return pso3dLine_.Get(); }
	ID3D12PipelineState* GetPSO2dTex() const { return pso2dTex_.Get(); }
	ID3D12PipelineState* GetPSO2dNoTex() const { return pso2dNoTex_.Get(); }

	ID3D12RootSignature* GetRootSignature3dLit() const { return rootSignature3dLit_.Get(); }
	ID3D12RootSignature* GetRootSignature3dNoLit() const { return rootSignature3dNoLit_.Get(); }
	ID3D12RootSignature* GetRootSignature3dLine() const { return rootSignature3dLine_.Get(); }
	ID3D12RootSignature* GetRootSignature2d() const { return rootSignature2d_.Get(); }

	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_.Get(); }

private:
	/// <summary>
	/// Lightingあり3D用RootSignatureを生成する
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dLit();

	/// <summary>
	/// Lightingなし3D用RootSignatureを生成する
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature3dNoLit();

	/// <summary>
	/// 2D用RootSignatureを生成する
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature2d();

	// Line3D専用のRootSignatureを生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureLine3d();

	/// <summary>
	/// 3D用DepthStencilStateを生成する
	/// </summary>
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilDesc3d();

	/// <summary>
	/// 2D用DepthStencilStateを生成する
	/// </summary>
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilDesc2d();

	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature3dLit_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature3dNoLit_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature2d_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature3dLine_ = nullptr;
	// PSO
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dLitTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dLitNoTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dHalfLitTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dHalfLitNoTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dNoLitTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dNoLitNoTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso2dTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso2dNoTex_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3dLine_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
};