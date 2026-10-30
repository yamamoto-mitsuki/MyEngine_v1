#include "MyEngine/Graphics/Compute/ComputeSelfTest.h"

#include <span>
#include <format>
#include <vector>
#include <cstdint>
#include <numeric>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/GpuResources.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"

namespace {
struct TestInformation {
	uint32_t count; // 要素の数
};

constexpr uint32_t kElementCount = 1000; // わざと64の倍数にしない（はみ出たスレッドを止める処理も確かめる）
constexpr uint32_t kMaxErrorLogs = 5;    // 合わなかった要素をログに出す数（全部出すと読めない）

// CSと同じ計算をCPUでする（答え合わせ用）
uint32_t Expected(uint32_t input, uint32_t index) { return input * 2 + index; }
}

//=============================================================================
// 確かめる
//=============================================================================
void ComputeSelfTest::Run() { 
	const size_t byteSize = sizeof(uint32_t) * kElementCount;

	// ===== 1. 入力（CPU → GPU）=====
	// std::iota: 先頭から 100,101,102..と1ずつ増やして埋める
	std::vector<uint32_t> input(kElementCount);
	std::iota(input.begin(), input.end(), 100u);
	Microsoft::WRL::ComPtr<ID3D12Resource> inputBuffer = 
		UploadContext::CreateBufferWithData(input.data(), byteSize, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, L"ComputeSelfTest_Input");
	UploadContext::Flush(); // コピーを流して待つ（CSが読む前に、中身が届いている必要がある）

	// ===== 2. 出力（CSが書く。ALLOQ_UNORDERD_ACCESSがないとUAVにできない）=====
	Microsoft::WRL::ComPtr<ID3D12Resource> outputBuffer = GpuResources::CreateDefaultBuffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	outputBuffer->SetName(L"ComputeSelfTest_Output");

	// ===== 3. 定数（要素の数）=====
	TestInformation* information = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> informationBuffer = GpuResources::CreateMappedUploadBuffer(sizeof(TestInformation), reinterpret_cast<void**>(&information));
	information->count = kElementCount;

	// ===== 4. 結果をCPUで読むための置き場（Readbackヒープ。GPU → CPU）=====
	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer = GpuResources::CreateReadbackBuffer(byteSize);
	readbackBuffer->SetName(L"ComputeSelfTest_Readback");

	// ===== 5. コマンドを積む =====
	ID3D12GraphicsCommandList* cmdList = DirectXCommon::GetCommandList();
	// --- 出力をCSが書ける状態にする ---
	DirectXCommon::TransitionBarrier(outputBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	// --- CSを動かす ---
	const ComputePipeline& pipeline = PSOManager::GetComputePipeline("ComputeTestCS");
	pipeline.Bind(cmdList);
	pipeline.SetShaderResource(cmdList, "gInput", inputBuffer->GetGPUVirtualAddress());
	pipeline.SetUnorderedAccess(cmdList, "gOutput", outputBuffer->GetGPUVirtualAddress());
	pipeline.SetConstantBuffer(cmdList, "gTestInformation", informationBuffer->GetGPUVirtualAddress());
	pipeline.Dispatch(cmdList, kElementCount);
	// --- 書き終わった出力をコピー元にして、Readbackへ写す ---
	DirectXCommon::TransitionBarrier(outputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cmdList->CopyBufferRegion(readbackBuffer.Get(), 0, outputBuffer.Get(), 0, byteSize);

	// ===== 6. 実行して、終わるまで待つ =====
	DirectXCommon::ExecuteAndWait();

	// ===== 7. 答え合わせ =====
	uint32_t* mapped = nullptr;
	const D3D12_RANGE readRange{0, byteSize}; // CPUが読む範囲
	readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
	const std::span<const uint32_t> results(mapped, kElementCount); // ポインタと数を1つにまとめる（範囲外を読まないように）
	uint32_t errorCount = 0;
	for (uint32_t i = 0; i < kElementCount; ++i) {
		const uint32_t expected = Expected(input[i], i);
		if (results[i] == expected) {
			continue;
		}
		if (errorCount < kMaxErrorLogs) {
			LogManager::Error(std::format("[compute] self test: [{}] GPU={} CPU={}", i, results[i], expected));
		}
		++errorCount;
	}
	const D3D12_RANGE writtenRange{0, 0}; // CPUからは何も書いていない
	readbackBuffer->Unmap(0, &writtenRange);

	MY_ASSERT_MSG(errorCount == 0, std::format("CSの結果がCPUと合いません（{} / {} 個が違う）", errorCount, kElementCount));
	LogManager::Log(std::format("[compute] self test OK ({} / {})", kElementCount, kElementCount));
}