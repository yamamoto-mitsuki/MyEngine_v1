#include "MyEngine/Graphics/Renderer/RenderQueue.h"

#include <algorithm>

#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
RenderQueue* RenderQueue::instance_ = nullptr;


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void RenderQueue::Initialize() { 
	instance_ = new RenderQueue(); 
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void RenderQueue::Release() { 
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}

// ===== クリア =====
void RenderQueue::Clear() { 
	instance_->meshRequests_.clear();
	instance_->particleRequests_.clear();
	instance_->spriteRequests_.clear();
	instance_->lineRequests_.clear();
}

//=============================================================================
// リクエスト
//=============================================================================
// ===== モデル群リクエスト =====
void RenderQueue::Request(MeshRequest&& req) {
	// PSOKey → SortKey
	PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Model, req.shadingType, req.blendMode, req.rasterizerType, req.depthMode);
	req.sortKey = PSOManager::GetSortKey(psoKey);
	// リクエストに積む
	instance_->meshRequests_.push_back(std::move(req));
}

// ===== パーティクル群リクエスト =====
void RenderQueue::Request(ParticleRequest&& req) { 
	PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Particle, ShadingType::Unlit, req.blendMode, RasterizerType::SolidNone, DepthMode::TestNoWrite);
	req.sortKey = PSOManager::GetSortKey(psoKey);
	instance_->particleRequests_.push_back(std::move(req)); 
}

// ===== スプライト群リクエスト =====
void RenderQueue::Request(SpriteRequest&& req) { instance_->spriteRequests_.push_back(std::move(req)); }

// ===== Line群リクエスト =====
void RenderQueue::Request(LineRequest&& req) { instance_->lineRequests_.push_back(std::move(req)); }



//=============================================================================
// 発行
//=============================================================================
// ===== 3d ===== 
void RenderQueue::Flush3d(const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	auto& meshes = instance_->meshRequests_;
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	// --- ソート（切替コストの高い順に並ぶ：RootSig > Shader > Blend > Raster > Depth） ---
	std::sort(meshes.begin(), meshes.end(), [](const MeshRequest& a, const MeshRequest& b) { return a.sortKey < b.sortKey; });

	// ===== 1. ループ（Model） =====
	uint64_t currentKey = UINT64_MAX; // 無効値
	uint32_t scopeIndex = UINT32_MAX; // GPUProfiler用

	for (const MeshRequest& req : meshes) {
		// ウィンドウ名があっているか確認
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			//LogManager::Warning(std::format("RenderTarget WindowTitle {} != {}", req.windowTitle, windowTitle));
			continue;
		}

		// キーが変わったとき
		if (req.sortKey != currentKey) {
			// GPUProfiler: 前の区間を閉じてから新しい区間を開く
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}
			// --- RootSignatureが変わったタイミングで変更 ---
			if ((req.sortKey >> 32) != (currentKey >> 32)) {
				// RootSignatureをSet
				RootSignatureID rsID = RootSignatureManager::GetRootSignatureID(DrawCategory::Model, req.shadingType);
				cmdList->SetGraphicsRootSignature(RootSignatureManager::GetRootSignature(rsID).Get());
				// RootSignature切り替えは全ルートバインドを無効化するので、バインドレスSRVを再セット
				if (auto slot = RootSignatureManager::GetBindSlot(rsID, RootBind::BindlessTexture)) {
					cmdList->SetGraphicsRootDescriptorTable(slot.value(), heapStart);
				}
			}

			// --- PSOが変わっているとき ---
			// PipelineStateをSet
			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Model, req.shadingType, req.blendMode, req.rasterizerType, req.depthMode);
			cmdList->SetPipelineState(PSOManager::GetPSO(psoKey).Get());
			// GPUProfiler: 新しい区間のスタート
			scopeIndex = GPUProfiler::Begin(cmdList, PSOManager::GetStateName(psoKey));

			// キーを更新
			currentKey = req.sortKey;
		}

		// Draw Callする関数へ
		RenderContext::DrawMesh(req);
	}
	// GPUProfiler: 計測終了
	if (scopeIndex != UINT32_MAX) {
		GPUProfiler::End(cmdList, scopeIndex);
	}

	// ===== 2. ループ（Line） =====
	bool lineStateSet = false; // Lineを描画する場合は1度だけ RootSignature, PipelineState を切り替えたいので、そのためのFlag
	for (const LineRequest& req : instance_->lineRequests_) {
		// ウィンドウ名があっているか確認 =====
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			//LogManager::Warning(std::format("RenderTarget WindowTitle {} != {}", req.windowTitle, windowTitle));
			continue;
		}
		// 最初のみ RootSignature, PipelineState を切り替える
		if (!lineStateSet) {
			cmdList->SetGraphicsRootSignature(RootSignatureManager::GetRootSignature(RootSignatureID::Line).Get());
			cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(DrawCategory::Line, ShadingType::Unlit)).Get());
			lineStateSet = true;
		}

		// Draw Callする関数へ
		RenderContext::DrawLines(req);
	}

	// ===== 3. ループ（Particle） =====
	// 並び替え
	std::sort(instance_->particleRequests_.begin(), instance_->particleRequests_.end(), 
		[](const ParticleRequest& a, const ParticleRequest& b) { return a.sortKey < b.sortKey; });

	uint64_t currentParticleKey = UINT64_MAX;
	bool particleRSSet = false;
	for (const ParticleRequest& req : instance_->particleRequests_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			//LogManager::Warning(std::format("RenderTarget WindowTitle {} != {}", req.windowTitle, windowTitle));
			continue;
		}
		// 最初の1回だけRootSignatureを設定
		if (!particleRSSet) {
			cmdList->SetGraphicsRootSignature(RootSignatureManager::GetRootSignature(RootSignatureID::Particle).Get());
			// RootSignature切り替え後はバインドレスSRVを再セット
			if (auto slot = RootSignatureManager::GetBindSlot(RootSignatureID::Particle, RootBind::BindlessTexture)) {
				cmdList->SetGraphicsRootDescriptorTable(slot.value(), heapStart);
			}
			particleRSSet = true;
		}
		// キーが変わったとき（＝BlendModeが変わったとき）だけPSO切替
		if (req.sortKey != currentParticleKey) {
			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Particle, ShadingType::Unlit, req.blendMode, RasterizerType::SolidNone, DepthMode::TestNoWrite);
			cmdList->SetPipelineState(PSOManager::GetPSO(psoKey).Get());
			currentParticleKey = req.sortKey;
		}
		RenderContext::DrawParticles(req);
	}
}

// ===== 2d =====
void RenderQueue::Flush2d(const std::wstring& windowTitle, RenderWindow* rw) { 
	auto* cmdList = DirectXCommon::GetCommandList(); 
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	// スプライトは重なり順なのでソートしない
	bool spriteStateSet = false; // Spriteを描画する場合は1度だけ RootSignature, PipelineState を切り替えたいので、そのためのFlag
	for (const SpriteRequest& req : instance_->spriteRequests_) {
		// ウィンドウ名があっているか確認
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			//LogManager::Warning(std::format("RenderTarget WindowTitle {} != {}", req.windowTitle, windowTitle));
			continue;
		}

		// 最初のみ RootSignature, PipelineState, DepthMode を切り替える
		if (!spriteStateSet) {
			// RootSignature
			RootSignatureID rsID = RootSignatureID::Sprite;
			cmdList->SetGraphicsRootSignature(RootSignatureManager::GetRootSignature(rsID).Get());
			// PipelineState
			if (auto slot = RootSignatureManager::GetBindSlot(rsID, RootBind::BindlessTexture)) {
				cmdList->SetGraphicsRootDescriptorTable(slot.value(), heapStart);
			}
			// 2dは深度無効。DepthMode::Disableを明示
			cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(DrawCategory::Sprite, ShadingType::Unlit, BlendMode::Normal, RasterizerType::SolidBack, DepthMode::Disable)).Get());
			spriteStateSet = true;
		}

		// Draw Callする関数へ
		RenderContext::DrawSprite(req, rw);
	}
}
