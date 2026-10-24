#include "MyEngine/Graphics/Renderer/RenderQueue.h"

#include <algorithm>

#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Profiling/GPUProfiler.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
std::unique_ptr<RenderQueue> RenderQueue::instance_ = nullptr;


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void RenderQueue::Initialize() { 
	instance_ = std::make_unique<RenderQueue>(ConstructorKey());
	instance_->isLayerVisible_.fill(true); // 初めは全部の層を描く（fill：配列の全部の要素を同じ値にする）
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void RenderQueue::Release() { 
	instance_.reset();
	instance_ = nullptr;
	LogManager::Log("Released");
}

// ===== クリア =====
void RenderQueue::Clear() { 
	instance_->transparentMeshRequests_.clear();
	instance_->opaqueMeshRequests_.clear();
	instance_->particleRequests_.clear();
	instance_->requests2d_.clear();
	instance_->lineRequests_.clear();
}

// ===== 層ごとに描くか =====
void RenderQueue::SetLayerVisible(DrawLayer layer, bool isVisible) { instance_->isLayerVisible_[static_cast<size_t>(layer)] = isVisible; }

//=============================================================================
// リクエスト
//=============================================================================
// ===== モデル群リクエスト =====
void RenderQueue::Request(MeshRequest&& req) {
	// PSOKey → SortKey
	PSOKey psoKey = PSOManager::GetPSOKey(req.category, req.shadingType, req.blendMode, req.rasterizerType, req.depthMode);
	req.sortKey = PSOManager::GetSortKey(psoKey);
	// リクエストに積む
	if (req.depthMode == DepthMode::TestNoWrite) {
		instance_->transparentMeshRequests_.push_back(std::move(req)); // 半透明
	} else {
		instance_->opaqueMeshRequests_.push_back(std::move(req)); // 不透明(TestWrite)
	}
}

// ===== パーティクル群リクエスト =====
void RenderQueue::Request(ParticleRequest&& req) { 
	PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Particle, ShadingType::Unlit, req.blendMode, RasterizerType::SolidNone, DepthMode::TestNoWrite);
	req.sortKey = PSOManager::GetSortKey(psoKey);
	instance_->particleRequests_.push_back(std::move(req)); 
}

// ===== スプライト群リクエスト =====
void RenderQueue::Request(SpriteRequest&& req) { instance_->requests2d_.emplace_back(std::move(req)); }

// ===== 文字リクエスト（スプライトと同じ列に積む）=====
void RenderQueue::Request(TextRequest&& req) { instance_->requests2d_.emplace_back(std::move(req)); }

// ===== Line群リクエスト =====
void RenderQueue::Request(LineRequest&& req) { instance_->lineRequests_.push_back(std::move(req)); }


//=============================================================================
// Mesh
//=============================================================================
// ===== RootSignature, PSOソート =====
void RenderQueue::FlushMeshList(const std::vector<MeshRequest>& meshes, const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB, const char* name) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// SRVセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	// キー初期化
	uint64_t currentKey = UINT64_MAX;
	uint32_t scopeIndex = UINT32_MAX;
	uint32_t currentShaderProg = static_cast<uint32_t>(UINT32_MAX);

	// リクエスト分ループ
	for (const MeshRequest& req : meshes) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// このビューで切ってある層の物は描かない（Game ビューのグリッドなど）
		if (!instance_->isLayerVisible_[static_cast<size_t>(req.layer)]) {
			continue;
		}
		// --- シェーダー(RootSignature)が変わったとき ---
		uint32_t shaderProg = PSOManager::GetShaderProgramID(req.category, req.shadingType);
		if (shaderProg != currentShaderProg) {
			// GPUProfiler
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}
			// RootSignature
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(shaderProg);
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
			// CBuffer: Camera
			auto camSlot = rs.slotOf.find(RootBind::Camera);
			if (cameraCB && camSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootConstantBufferView(camSlot->second, cameraCB);
			}
			// CBuffer: Texture
			auto texSlot = rs.slotOf.find(RootBind::BindlessTexture);
			if (texSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootDescriptorTable(texSlot->second, heapStart);
			}
			// CBuffer: TextureCube
			auto texCubeSlot = rs.slotOf.find(RootBind::BindlessTextureCube);
			if (texCubeSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootDescriptorTable(texCubeSlot->second, heapStart);
			}
			currentShaderProg = shaderProg;
		}
		// --- PSOが変わったとき ---
		if (req.sortKey != currentKey) {
			// GPUProfiler
			if (scopeIndex != UINT32_MAX) {
				GPUProfiler::End(cmdList, scopeIndex);
			}
			// PSO
			PSOKey psoKey = PSOManager::GetPSOKey(req.category, req.shadingType, req.blendMode, req.rasterizerType, req.depthMode);
			cmdList->SetPipelineState(PSOManager::GetPSO(psoKey).Get());
			scopeIndex = GPUProfiler::Begin(cmdList, (std::string(name) + "/" + PSOManager::GetStateName(psoKey)).c_str());
			currentKey = req.sortKey;
		}

		RenderContext::DrawMesh(req);
	}
	if (scopeIndex != UINT32_MAX) {
		GPUProfiler::End(cmdList, scopeIndex);
	}
}

// ===== 不透明 =====
void RenderQueue::FlushOpaqueMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB) {
	auto& meshes = instance_->opaqueMeshRequests_;
	// 状態切替を減らす＝状態キーでソート
	std::sort(meshes.begin(), meshes.end(), [](const MeshRequest& a, const MeshRequest& b) { return a.sortKey < b.sortKey; });
	FlushMeshList(meshes, windowTitle, cameraCB, "Opaque");
}

void RenderQueue::FlushTransparentMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB) {
	auto& meshes = instance_->transparentMeshRequests_;
	// 正しくブレンド＝奥→手前（距離²の降順）
	std::sort(meshes.begin(), meshes.end(), [](const MeshRequest& a, const MeshRequest& b) { return a.cameraDistanceSq > b.cameraDistanceSq; });
	FlushMeshList(meshes, windowTitle, cameraCB, "Transparent");
}


//=============================================================================
// Particleの描画を並び変えて発行
//=============================================================================
void RenderQueue::FlushParticle(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB) {
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
			// RootSignature
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(DrawCategory::Particle));
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
			// カメラ
			auto camSlot = rs.slotOf.find(RootBind::Camera);
			if (cameraCB && camSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootConstantBufferView(camSlot->second, cameraCB);
			}
			// Texture
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
void RenderQueue::FlushLine(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	// 並び替え初期値
	bool lineStateSet = false;        // 最初だけセットするため
	uint32_t scopeIndex = UINT32_MAX; // GPUProfiler用
	DepthMode currentDepth = DepthMode::TestWrite; // 今セットしているPSOの深度設定
	// 線のPSOをセットする
	auto setLinePSO = [cmdList](DepthMode depth) {
		cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(DrawCategory::Line, ShadingType::Unlit, 
			BlendMode::Normal, RasterizerType::SolidBack, depth)).Get());
	};

	// ===== リクエストが来た分だけループ =====
	for (const LineRequest& req : instance_->lineRequests_) {
		// ウィンドウ名があっているか確認 =====
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// --- 最初のみ RootSignature, PipelineState を切り替える ---
		if (!lineStateSet) {
			// GPUProfiler: 新しい区間のスタート
			PSOKey psoKey = PSOManager::GetPSOKey(DrawCategory::Line, ShadingType::Unlit);
			scopeIndex = GPUProfiler::Begin(cmdList, PSOManager::GetStateName(psoKey));
			// RootSignature
			const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(DrawCategory::Line));
			cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
			// カメラ
			auto camSlot = rs.slotOf.find(RootBind::Camera);
			if (cameraCB && camSlot != rs.slotOf.end()) {
				cmdList->SetGraphicsRootConstantBufferView(camSlot->second, cameraCB);
			}
			// PSO
			setLinePSO(req.depthMode);
			currentDepth = req.depthMode;
			lineStateSet = true;
		} else if (req.depthMode != currentDepth) {
			setLinePSO(req.depthMode);
			currentDepth = req.depthMode;
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
namespace {
// 2Dの描画で、今コマンドリストにセットしている物（同じ間はセットし直さない）
struct State2d {
	bool isSet = false;                                // まだ何もセットしていなければ false
	DrawCategory category = DrawCategory::Sprite;      // シェーダー（スプライトか文字か）
	BlendMode blend = BlendMode::Normal;               // PSO のブレンド
	RasterizerType raster = RasterizerType::SolidBack; // PSO のラスタライザ
};

// 2Dのリクエストの重ねる順
int OrderOf(const RenderQueue::Request2d& request) {
	return std::visit([](const auto& r) { return r.order; }, request);
}

// 描く物に合わせて RootSignature・PSO をセットする（スプライトと文字が交互に来ても、変わった所だけセットし直す）
void Bind2d(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_DESCRIPTOR_HANDLE heapStart, State2d& current, DrawCategory category, BlendMode blend, RasterizerType raster) {
	// --- シェーダーが変わったら、RootSignature とテクスチャの表を付け替える ---
	const bool isCategoryChanged = !current.isSet || current.category != category;
	if (isCategoryChanged) {
		const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(PSOManager::GetShaderProgramID(category));
		cmdList->SetGraphicsRootSignature(rs.rootSignature.Get());
		auto texSlot = rs.slotOf.find(RootBind::BindlessTexture);
		if (texSlot != rs.slotOf.end()) {
			cmdList->SetGraphicsRootDescriptorTable(texSlot->second, heapStart);
		}
	}
	// --- PSO（2dは深度無効なので DepthMode::Disable 固定）---
	if (isCategoryChanged || current.blend != blend || current.raster != raster) {
		cmdList->SetPipelineState(PSOManager::GetPSO(PSOManager::GetPSOKey(category, ShadingType::Unlit, blend, raster, DepthMode::Disable)).Get());
	}
	current = {true, category, blend, raster};
}
} // namespace

void RenderQueue::Flush2d(const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	// SRVヒープセット
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	// order の小さい順（奥から）に並べる。ウィンドウの数だけ呼ばれるが、2回目からは並んでいるので順番は変わらない
	std::ranges::stable_sort(instance_->requests2d_, {}, OrderOf);

	State2d current;
	for (const Request2d& request : instance_->requests2d_) {
		// --- スプライト ---
		if (const SpriteRequest* sprite = std::get_if<SpriteRequest>(&request)) {
			if (sprite->windowTitle != windowTitle && sprite->windowTitle != L"") {
				continue; // 別のウィンドウの物
			}
			Bind2d(cmdList, heapStart, current, DrawCategory::Sprite, sprite->blendMode, sprite->rasterizerType);
			RenderContext::DrawSprite(*sprite);
			continue;
		}
		// --- 文字（両面を描く。回して裏返っても消えないように）---
		if (const TextRequest* text = std::get_if<TextRequest>(&request)) {
			if (text->windowTitle != windowTitle && text->windowTitle != L"") {
				continue;
			}
			Bind2d(cmdList, heapStart, current, DrawCategory::Text, text->blendMode, RasterizerType::SolidNone);
			RenderContext::DrawText2d(*text);
		}
	}
}