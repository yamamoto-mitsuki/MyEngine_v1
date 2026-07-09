#include "UploadContext.h"
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Debug/MyAssert.h"
#include <cassert>
#include <cstring>

UploadContext* UploadContext::instance_ = nullptr;

//=============================================================================
// 初期化 / 解放
//=============================================================================
void UploadContext::Initialize(size_t stagingBytes) { 
	MY_ASSERT_MSG(instance_ == nullptr, "UploadContext::Initializeが2回呼ばれています");
	instance_ = new UploadContext();
	instance_->Init(stagingBytes);
}

void UploadContext::Init(size_t stagingBytes) { 
	auto* device = DirectXCommon::GetDevice(); 
	// 転送専用のCommandAllocator, List, Fenceを作成
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// Bufferを使いまわす（永続Map）
	staging_ = DirectXCommon::CreateUploadBuffer(stagingBytes);
	staging_->Map(0, nullptr, reinterpret_cast<void**>(&stagingMapped_));
	stagingCapacity_ = stagingBytes;
	stagingOffset_ = 0;

	LogManager::Log("Initialized");
}

void UploadContext::Release() {
	if (!instance_) {
		return;
	}

	if (instance_->stagingMapped_) {
		instance_->staging_->Unmap(0, nullptr);
	}

	if (instance_->fenceEvent_) {
		CloseHandle(instance_->fenceEvent_);
	}
	delete instance_;
	instance_ = nullptr;
}

//=============================================================================
// 転送の予約：stagingへ書き、staging→dest のコピーを記録する
//=============================================================================
void UploadContext::QueueUpload(ID3D12Resource* dest, const void* data, size_t size, D3D12_RESOURCE_STATES finalState) { 
	MY_ASSERT_MSG(instance_, "UploadContext::Initialize()を先に呼んでください");
	MY_ASSERT_MSG(size <= instance_->stagingCapacity_, "1回の転送サイズがstaging容量を超えています。容量を増やしてください。");

	// stagingに入りきらない場合、いったんFlush()して空に
	if (instance_->stagingOffset_ + size > instance_->stagingCapacity_) {
		Flush();
	}

	// staging（永続Map）へCPUコピー
	std::memcpy(instance_->stagingMapped_ + instance_->stagingOffset_, data, size);
	// stagingの該当するOffsetを dest へコピー
	instance_->commandList_->CopyBufferRegion(dest, 0, instance_->staging_.Get(), instance_->stagingOffset_, size);
	
	// Resourceの状態を変更
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = dest;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = finalState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	instance_->commandList_->ResourceBarrier(1, &barrier);

	// Offsetを進める
	instance_->stagingOffset_ = (instance_->stagingOffset_ + size + 255) & ~size_t(255);
	instance_->hasPending_ = true;
}

//=============================================================================
// 予約した全コピーを実行してGPU完了を待つ
//=============================================================================
void UploadContext::Flush() { 
	// 予約しているものがあるか確認
	if (!instance_->hasPending_) {
		return;
	}
	// 記録を確定して実行
	instance_->commandList_->Close();
	ID3D12CommandList* lists[] = {instance_->commandList_.Get()};
	DirectXCommon::GetCommandQueue()->ExecuteCommandLists(1, lists);
	// 専用のフェンスで完了を待つ
	++instance_->fenceValue_;
	DirectXCommon::GetCommandQueue()->Signal(instance_->fence_.Get(), instance_->fenceValue_);

	if (instance_->fence_->GetCompletedValue() < instance_->fenceValue_) {
		instance_->fence_->SetEventOnCompletion(instance_->fenceValue_, instance_->fenceEvent_);
		WaitForSingleObject(instance_->fenceEvent_, INFINITE);
	}
	instance_->allocator_->Reset();
	instance_->commandList_->Reset(instance_->allocator_.Get(), nullptr);
	instance_->stagingOffset_ = 0;
	instance_->hasPending_ = false;
}