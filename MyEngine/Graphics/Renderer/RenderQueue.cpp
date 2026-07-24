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
// Meshの描画を並び変えて発行（透明・不透明は並び替えまだ）
//=============================================================================
void RenderQueue::FlushMesh(const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	auto& meshes = instance_->meshRequests_;
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	// --- ソート（切替コストの高い順に並ぶ：RootSig > Shader > Blend > Raster > Depth） ---
	std::sort(meshes.begin(), meshes.end(), [](const MeshRequest& a, const MeshRequest& b) { return a.sortKey < b.sortKey; });

	// 並び替えに使用する初期値
	uint64_t currentKey = UINT64_MAX;
	uint32_t scopeIndex = UINT32_MAX;
	ShaderProgramID currentShaderProg = static_cast<ShaderProgramID>(UINT32_MAX);

	// ===== リクエストが来た分ループ
	for (const MeshRequest& req : meshes) {
		// ウィンドウ名があっているか確認
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// 描画形状、ShadingTypeからシェーダーファイルの組み合わせを取得
		ShaderProgramID shaderProg = PSOManager::GetShaderProgramID(DrawCategory::Model, req.shadingType);

		// --- シェーダーファイルの組み合わせが変わったとき ---
		if (shaderProg != currentShaderProg) {
			// GPUProfiler: 前の区間を閉じてから新しい区間を開く
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(shaderProg); // ルートシグニチャとルートパラメータ番号を取得
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());                  // ルートシグニチャ再セット
			
			// テクスチャがあるならセットする
			auto texSlot = rs.slotOf.find(RootBind::BindlessTexture);
			if (texSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootDescriptorTable(texSlot->second, heapStart);
			}
			// PBRで環境マップがセットされていたら更新
			auto texCubeSlot = rs.slotOf.find(RootBind::BindlessTextureCube);
			if (req.iblParamsAddress && texCubeSlot != rs.slotOf.end() && req.shadingType == ShadingType::PBR) {
				cmdList->SetGraphicsRootDescriptorTable(texCubeSlot->second, heapStart);
			}
			currentShaderProg = shaderProg;
		}

		// --- PSOが変わったとき ---
		if (req.sortKey != currentKey) {
			// GPUProfiler: 前の区間を閉じてから新しい区間を開く
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}
			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Model, req.shadingType, req.blendMode, req.rasterizerType, req.depthMode);
			cmdList->SetPipelineState(PSOManager::GetPSO(psoKey).Get()); // PSOをセット
			scopeIndex = GPUProfiler::Begin(cmdList, PSOManager::GetStateName(psoKey));
			currentKey = req.sortKey;
		}


		// Draw Callする関数へ
		RenderContext::DrawMesh(req);
	}
	// GPUProfiler: 計測終了
	if (scopeIndex != UINT32_MAX) {
		GPUProfiler::End(cmdList, scopeIndex);
	}
}


//=============================================================================
// Particleの描画を並び変えて発行
//=============================================================================
void RenderQueue::FlushParticle(const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	// 並び替え条件
	std::sort(instance_->particleRequests_.begin(), instance_->particleRequests_.end(), 
		[](const ParticleRequest& a, const ParticleRequest& b) { return a.sortKey < b.sortKey; });
	// 並び替え初期値
	uint64_t currentParticleKey = UINT64_MAX;
	uint32_t scopeIndex = UINT32_MAX;
	bool particleRSSet = false;

	// ===== リクエストが来た分だけループ =====
	for (const ParticleRequest& req : instance_->particleRequests_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// --- 最初の1回だけRootSignatureを設定 ---
		if (!particleRSSet) {
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(DrawCategory::Particle));
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get()); // RootSignatureをセット
			// RootSignature切り替え後はバインドレスSRVを再セット
			auto texSlot = rs.slotOf.find(RootBind::BindlessTexture);
			if (texSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootDescriptorTable(texSlot->second, heapStart);
			}
			particleRSSet = true;
		}

		// --- PSOが変わったとき ---
		if (req.sortKey != currentParticleKey) {
			// GPUProfiler: 前の区間を閉じてから新しい区間を開く
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}

			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Particle, ShadingType::Unlit, req.blendMode, RasterizerType::SolidNone, DepthMode::TestNoWrite);
			scopeIndex = GPUProfiler::Begin(cmdList, PSOManager::GetStateName(psoKey));
			cmdList->SetPipelineState(PSOManager::GetPSO(psoKey).Get()); // PSOセット
			currentParticleKey = req.sortKey;
		}
		// DrawCallする関数へ
		RenderContext::DrawParticles(req);
	}
	// GPUProfiler: 計測終了
	if (scopeIndex != UINT32_MAX) {
		GPUProfiler::End(cmdList, scopeIndex);
	}
}


//=============================================================================
// Lineの描画を並び変えて発行
//=============================================================================
void RenderQueue::FlushLine(const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	// 並び替え初期値
	bool lineStateSet = false;        // 最初だけセットするため
	uint32_t scopeIndex = UINT32_MAX; // GPUProfiler用

	// ===== リクエストが来た分だけループ =====
	for (const LineRequest& req : instance_->lineRequests_) {
		// ウィンドウ名があっているか確認 =====
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// 最初のみ RootSignature, PipelineState を切り替える
		if (!lineStateSet) {
			// GPUProfiler: 新しい区間のスタート
			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Line, ShadingType::Unlit);
			scopeIndex = GPUProfiler::Begin(cmdList, PSOManager::GetStateName(psoKey));
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(DrawCategory::Line));
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
			cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(DrawCategory::Line, ShadingType::Unlit)).Get());
			lineStateSet = true;
		}

		// Draw Callする関数へ
		RenderContext::DrawLines(req);
	}
	// GPUProfiler: 計測終了
	if (scopeIndex != UINT32_MAX) {
		GPUProfiler::End(cmdList, scopeIndex);
	}
}

//=============================================================================
// 2dの描画を並び変えて発行
//=============================================================================
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
			continue;
		}

		// 最初のみ RootSignature, PipelineState, DepthMode を切り替える
		if (!spriteStateSet) {
			// RootSignature
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(DrawCategory::Sprite));
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
			// PipelineState
			auto texSlot = rs.slotOf.find(RootBind::BindlessTexture);
			if (texSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootDescriptorTable(texSlot->second, heapStart);
			}
			// 2dは深度無効。DepthMode::Disableを明示
			cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(DrawCategory::Sprite, ShadingType::Unlit, BlendMode::Normal, RasterizerType::SolidBack, DepthMode::Disable)).Get());
			spriteStateSet = true;
		}

		// Draw Callする関数へ
		RenderContext::DrawSprite(req, rw);
	}
}
