#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>
#include <wrl.h>

/// <summary>
/// DirectX12の基盤クラス
/// デバイス・コマンドキュー・SRVヒープ等のDX12コア機能を管理する
/// PSO・RootSignatureの管理はPSOManagerが担当する
/// </summary>
class DirectXCommon {
public:
	static constexpr UINT kSwapChainBufferCount = 2; // スワップチェーンのバッファ数
	static constexpr UINT kSRVDescriptorHeap = 128;  // SRVDescriptorHeapのスロット数
	uint32_t descriptorSizeSRV = 0;                  // SRV/CBV/UAVのDescriptorサイズ
	uint32_t descriptorSizeRTV = 0;                  // RTVのDescriptorサイズ
	uint32_t descriptorSizeDSV = 0;                  // DSVのDescriptorサイズ

public:
	~DirectXCommon();

	/// <summary>
	/// DX12基盤の初期化
	/// デバイス・コマンドキュー・フェンス・SRVヒープを生成する
	/// <para>PSO/RootSignatureはPSOManager::Init()で行う</para>
	/// </summary>
	void Init();

	/// <summary>
	/// GPUの処理完了を待つ
	/// コマンドキューに積んだ全コマンドの実行が完了するまでCPUをブロックする
	/// </summary>
	void WaitForGPU();

	/// <summary>
	/// HLSLシェーダーをコンパイルしてBlobで返す
	/// </summary>
	/// <param name="filePath">hlslファイルのパス</param>
	/// <param name="profile">シェーダープロファイル（例: L"vs_6_0", L"ps_6_0"）</param>
	/// <param name="dxcUtils">DXCユーティリティ</param>
	/// <param name="dxcCompiler">DXCコンパイラ</param>
	/// <param name="includeHandler">インクルードハンドラ</param>
	/// <param name="defines">プリプロセッサ定義 {L"USE_TEXTURE"}、{}等</param>
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	    const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, 
		IDxcIncludeHandler* includeHandler, const std::vector<std::wstring>& defines = {});

	/// <summary>
	/// GraphicsPipelineStateObjectを生成する
	/// PSO設定はPSOManagerが知っているので、基本的にPSOManager経由で呼ばれる
	/// プロジェクト側でカスタムシェーダーを作る場合は直接呼んでもよい
	/// </summary>
	/// <param name="vs">頂点シェーダーのBlob</param>
	/// <param name="ps">ピクセルシェーダーのBlob</param>
	/// <param name="inputLayout">頂点データの構造定義（レイマーチング等は空で渡す）</param>
	/// <param name="rootSignature">使用するRootSignature</param>
	/// <param name="depthStencilDesc">深度ステンシル設定（PSOManager::DepthStencilDescXxx()を使う）</param>
	/// <param name="topologyType">プリミティブトポロジー（線の描画時はD3D12_PRIMITIVE_TOPOLOGY_TYPE_LINEに変更）</param>
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(
	    IDxcBlob* vs, IDxcBlob* ps, D3D12_INPUT_LAYOUT_DESC inputLayout, ID3D12RootSignature* rootSignature, D3D12_DEPTH_STENCIL_DESC depthStencilDesc,
	    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	/// <summary>
	/// GPUバッファリソースを生成する
	/// <para>頂点バッファ・定数バッファ・インデックスバッファ等に使う</para>
	/// </summary>
	/// <param name="sizeInBytes">バッファサイズ（バイト）。内部で256バイトアライメント済み</param>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// <summary>
	/// 深度ステンシルテクスチャリソースを生成する
	/// <para>RenderWindowのDSV生成時に使う</para>
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	/// <summary>
	/// DescriptorHeapを生成する
	/// RTV/DSV/SRVヒープの生成に使う
	/// </summary>
	/// <param name="heapType">ヒープの種類（RTV/DSV/CBV_SRV_UAV）</param>
	/// <param name="numDescriptors">スロット数</param>
	/// <param name="shaderVisible">シェーダーから参照する場合true（SRVはtrue、RTVはfalse）</param>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// 指定インデックスのCPUDescriptorHandleを取得する
	/// SRV/RTV/DSVの登録時に使う
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	/// <summary>
	/// 指定インデックスのGPUDescriptorHandleを取得する
	/// シェーダーへのテクスチャバインド時に使う
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	//==========================================
	// ゲッター
	//==========================================
	IDXGIFactory7* GetFactory() const { return dxgiFactory_.Get(); }
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	IDxcUtils* GetDxcUtils() const { return dxcUtils_.Get(); }
	IDxcCompiler3* GetDxcCompiler() const { return dxcCompiler_.Get(); }
	IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler_.Get(); }

private:
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
};