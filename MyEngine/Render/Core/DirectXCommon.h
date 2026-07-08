#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>
#include <wrl.h>

// ブレンド設定
enum class BlendMode {
	None,     // ブレンド無し
	Normal,   // 通常aブレンド（デフォルト）: Src * SrcA + Dest * (1 - SrcA) 
	Add,      // 加算: Src * SrcA + Dest * 1
	Subtract, // 減算: Dest * 1 - Src * SrcA
	Multily,  // 乗算: Src * 0 + Dest * Src
	Screen,   // スクリーン: Src * (1 - Dest) + Dest * 1

	Count,    // 参照用。使用禁止
};

// PSO作成に必要なパラメータをまとめた構造体
struct PSODesc {
	IDxcBlob* vs = nullptr;                                                          // 頂点シェーダー
	IDxcBlob* ps = nullptr;                                                          // ピクセルシェーダー
	D3D12_INPUT_LAYOUT_DESC inputLayout{};                                           // 入力レイアウト
	ID3D12RootSignature* rootSignature = nullptr;                                    // ルートシグネチャ
	D3D12_DEPTH_STENCIL_DESC depthStencil{};                                         // 深度ステンシル設定
	BlendMode blend = BlendMode::Normal;                                             // ブレンドモード
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // トポロジ
};

/// <summary>
/// DirectX12の基盤クラス
/// <para>PSO・RootSignatureの管理はPSOManagerが担当する</para>
/// </summary>
class DirectXCommon {
public:
	static constexpr UINT kSwapChainBufferCount = 2; // スワップチェーンのバッファ数
	static constexpr UINT kSRVDescriptorHeap = 128;  // SRVDescriptorHeapのスロット数

	DirectXCommon() = default;
	~DirectXCommon() = default;
	DirectXCommon(const DirectXCommon&) = delete;
	DirectXCommon& operator=(const DirectXCommon&) = delete;

	/// <summary>
	/// DX12基盤の初期化。Engine::Initialize()内で1度だけ呼ぶ。
	/// </summary>
	static void Initialize();

	/// <summary>
	/// DX12基盤の解放。Engine::Finalize()内で最後に呼ぶ。
	/// <para>TextureManager・RenderContext等の自身を使っているものの解放が全て終わった後に呼ぶこと。</para>
	/// </summary>
	static void Release();

	/// <summary>
	/// デバイスが削除された場合、Logに出力する
	/// </summary>
	static void ReportDeviceRemoved(HRESULT hr);

	/// <summary>
	/// デバイスが削除されたかどうかをチェックする
	/// </summary>
	static bool CheckDeviceRemoved();

	/// <summary>
	/// GPUの処理完了を待つ
	/// <para>コマンドキューに積んだ全コマンドの実行が完了するまでCPUをブロックする</para>
	/// </summary>
	static void WaitForGPU();

	/// <summary>
	/// SRVスロット番号を1つ発行する。
	/// <para>TextureManager・RenderTextureなどSRVを必要とする全クラスがこれを使う。</para>
	/// <para>スロット0はImGui予約済みのため1から開始する。</para>
	/// </summary>
	/// <returns>確保したスロット番号</returns>
	static uint32_t AllocateSRVSlot();

	/// <summary>
	/// HLSLシェーダーをコンパイルしてBlobで返す
	/// </summary>
	/// <param name="filePath">hlslファイルのパス（例: L"Shader/Sprite.VS.hlsl"）</param>
	/// <param name="profile">シェーダープロファイル（例: L"vs_6_0", L"ps_6_0"）</param>
	/// <param name="dxcUtils">DXCユーティリティ</param>
	/// <param name="dxcCompiler">DXCコンパイラ</param>
	/// <param name="includeHandler">インクルードハンドラ</param>
	/// <param name="defines">プリプロセッサ定義 {L"USE_TEXTURE"}、{}等</param>
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	    const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, 
		IDxcIncludeHandler* includeHandler, const std::vector<std::wstring>& defines = {});

	/// <summary>
	/// GraphicsPipelineStateObjectを生成する
	/// <para>プロジェクト側でカスタムシェーダーを作る場合は直接呼んでもよい</para>
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const PSODesc& desc);

	/// <summary>
	/// Uploadヒープ（CPU → GPU）にバッファを生成する
	/// <para>頂点バッファ・定数バッファ・インデックスバッファ等に使う</para>
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(size_t sizeInBytes);

	/// <summary>
	/// Defaultヒープ（GPU専用・高速）にバッファを生成する
	/// <para>CPUから直接書けないので、UploadToDefaultBuffer で転送して使う。</para>
	/// <para>一度書いたら変わらない頂点バッファ等に使う。</para>
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(size_t sizeInBytes);

	/// <summary>
	/// Readbackヒープ（GPU → CPU）にバッファを生成する
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBuffer(size_t sizeInBytes);

	/// <summary>
	/// Uploadバッファを生成し、そのまま永続Mapして先頭ポインタを返す
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateMappedUploadBuffer(size_t sizeInBytes, void** outMappedPtr);

	/// <summary>
	/// 2DTextureResource（Defaultヒープ）を生成する
	/// <para>RenderTarget・DepthStencil・通常テクスチャなど、2DテクスチャのCreateCommittedResourceを共通化したもの</para>
	/// </summary>
	/// <param name="width">幅（ピクセル）</param>
	/// <param name="height">高さ（ピクセル）</param>
	/// <param name="format">ピクセルフォーマット</param>
	/// <param name="flags">用途フラグ（RTVなら ALLOW_RENDER_TARGET、DSVなら ALLOW_DEPTH_STENCIL、通常テクスチャなら NONE）</param>
	/// <param name="initialState">生成時のResourceState</param>
	/// <param name="clearValue">最適化クリア値。RenderTarget/DepthStencilは渡す、通常テクスチャはnullptr</param>
	/// <param name="arraySize">配列枚数（キューブマップ・テクスチャ配列用。通常1）</param>
	/// <param name="mipLevels">ミップマップ段数（通常1）</param>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, 
		D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE*clearValue = nullptr, 
		uint16_t arraySize = 1, uint16_t mipLevels = 1);

	/// <summary>
	/// 深度StencilTextureResourceを生成する
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

	/// <summary>
	/// RenderTargetTextureResourceを生成する
	/// </summary>
	/// <param name="width">幅（ピクセル）</param>
	/// <param name="height">高さ（ピクセル）</param>
	/// <param name="format">ピクセルフォーマット（通常は_SRGB系）</param>
	/// <param name="clearColor">クリア色。RGBAの順で4要素</param>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTargetTextureResource(uint32_t width, uint32_t height,
		DXGI_FORMAT format, const float clearColor[4]);

	/// <summary>
	/// DescriptorHeapを生成する
	/// </summary>
	static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// リソースのトランジションバリアを1つ発行する
	/// </summary>
	/// <param name="resource">状態を変えるリソース</param>
	/// <param name="before">変更前の状態</param>
	/// <param name="after">変更後の状態</param>
	static void TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	/// <summary>
	/// リソースの状態遷移バリアを複数まとめて発行する
	/// </summary>
	/// <param name="resources">
	/// {resource, before, after} のリスト。
	/// 例: { {gameRT, RT, SRV}, {bloomRT, SRV, RT} }
	/// </param>
	static void TransitionBarriers(std::initializer_list<std::tuple<ID3D12Resource*, D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES>> resources);

	/// <summary>
	/// 指定インデックスのCPUDescriptorHandleを取得する
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	/// <summary>
	/// 指定インデックスのGPUDescriptorHandleを取得する
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t index);

	// DrawCallをリセット、増加 
	static void ResetDrawCallCount() { instance_->drawCallCount_ = 0; }
	static void IncrementDrawCallCount() { instance_->drawCallCount_++; }


	//==========================================
	// ゲッター
	//==========================================
	static IDXGIFactory7* GetFactory();
	static IDXGIAdapter4* GetAdapter4();
	static ID3D12Device* GetDevice();
	static ID3D12CommandQueue* GetCommandQueue();
	static ID3D12CommandAllocator* GetCommandAllocator();
	static ID3D12GraphicsCommandList* GetCommandList();
	static ID3D12DescriptorHeap* GetSRVDescriptorHeap();
	static IDxcUtils* GetDxcUtils();
	static IDxcCompiler3* GetDxcCompiler();
	static IDxcIncludeHandler* GetIncludeHandler();
	static uint32_t GetDescriptorSizeSRV();
	static uint32_t GetDescriptorSizeRTV();
	static uint32_t GetDescriptorSizeDSV();
	static uint32_t GetDrawCallCount();

private:
	// staticポインタ。Init()で生成、Release()で解放する。
	// 解放順序をEngine::Finalize()で明示的に制御するためMeyersシングルトンを使わない。
	static DirectXCommon* instance_;

	// ===== 内部ヘルパー =====
	// 内部初期化
	void InitInternal();
	// DREDのAutoBreadcrumbsのopを文字列に変換する
	std::string BreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP op);
	// D3D12のデバックメッセージをLogに出力する
	static void CALLBACK D3D12DebugMessageCallback(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void*);
	// BlendModeに応じて D3D12_BLEND_DESC を作成
	static D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode);

	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
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

	// DrawCallカウンター
	uint32_t drawCallCount_ = 0;
	// SRVスロットの発行カウンタ
	// スロット0はImGui予約済みのため1から開始する
	uint32_t nextSrvSlot_ = 1;

	// DescriptorSizeのキャッシュ
	uint32_t descriptorSizeSRV_ = 0;
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;
};