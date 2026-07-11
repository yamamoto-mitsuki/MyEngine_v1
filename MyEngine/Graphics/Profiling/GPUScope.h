#pragma once
#include <d3d12.h>
#ifdef _DEBUG
#include <pix.h>
#endif

#include "MyEngine/Graphics/Profiling/GPUProfiler.h"


/// <summary>
/// 1つの区間を PIXツリー + GPU計測 の両方で囲むRAII（Resource Acquisition Is Initialization）
/// </summary>
class GPUScope {
public:
	GPUScope(ID3D12GraphicsCommandList* cmd, const char* name) : cmd_(cmd) {
	#ifdef _DEBUG
		// PIXツリー
		PIXBeginEvent(cmd_, PIX_COLOR_DEFAULT, name);
	#endif
		// GPU計測開始
		index_ = GPUProfiler::Begin(cmd_, name);
	}

	~GPUScope() { 
		// GPU計測終了
		GPUProfiler::End(cmd_, index_);
	#ifdef _DEBUG
		// PIXツリー終了
		PIXEndEvent(cmd_);
	#endif
	}

private:
	ID3D12GraphicsCommandList* cmd_;
	uint32_t index_;
};

// 同じスコープに複数回書けるように2段階マクロ
#define GPU_SCOPE_CONCAT2(a, b) a##b
#define GPU_SCOPE_CONCAT(a, b)  GPU_SCOPE_CONCAT2(a,b) // __LINE__の展開を確実にする
#define GPU_SCOPE(cmd, name) GPUScope GPU_SCOPE_CONCAT(gpuScope, __LINE__)(cmd, name)