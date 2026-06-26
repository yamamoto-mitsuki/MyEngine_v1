#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

/// <summary>
/// DefaultHeapへの転送をまとめて行う
/// <para>専用のCommandList,Fenceを持ち、フレーム描画の同期とは独立して動く</para>
/// </summary>
class UploadContext {
public:
	/// <summary>初期化。DirectXCommon::Initialize()の後に呼ぶ</summary>
	/// <param name="stagingBytes">使い回すステージングバッファのサイズ（デフォルト: 8MB）</param>
	static void Initialize(size_t stagingBytes = 8 * 1024 * 1024);

	/// <summary>解放。DirectXCommon::Release()の前に呼ぶ</summary>
	static void Release();

	/// <summary>
	/// dest(DEFAULTヒープ)へdataを転送する予約。stagingへ書き、GPUコピーを記録する。
	/// </summary>
	/// <param name="finalState">転送後の状態（頂点バッファなら VERTEX_AND_CONSTANT_BUFFER）</param>
	static void QueueUpload(ID3D12Resource* dest, const void* data, size_t size, D3D12_RESOURCE_STATES finalState);

	/// <summary>予約した全コピーをGPUに流して完了を待ち、stagingを再利用可能に戻す</summary>
	static void Flush();

private:
	static UploadContext* instance_;
	void Init(size_t stagingBytes);

	// 転送用
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
	// 永続MapのUploadBuffer
	Microsoft::WRL::ComPtr<ID3D12Resource> staging_;
	uint8_t* stagingMapped_ = nullptr;
	size_t stagingCapacity_ = 0;
	size_t stagingOffset_ = 0;
	bool hasPending_ = false; // 未Flushの記録があるか
};