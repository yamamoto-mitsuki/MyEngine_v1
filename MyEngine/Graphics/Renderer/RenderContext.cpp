#include "RenderContext.h"

#include <format>
#include <iterator>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Particle/ParticleManager.h"

// 静的メンバ変数
RenderContext* RenderContext::instance_ = nullptr;

static UINT SlotOf(const RootSignatureInfo& rs, RootBind bind) {
	auto it = rs.slotOf.find(bind);
	MY_ASSERT_MSG(it != rs.slotOf.end(), std::format("slotOf に RootBind::{} がありません。NameToRoleの名前とHLSLの変数名が不一致の可能性があります。", 
		magic_enum::enum_name(bind)));
	return it->second;
}
// シェーダーが使っている定数だけを結ぶ。使っていない定数バッファはコンパイルで消えるので、スロットが無い
static void BindConstantIfUsed(ID3D12GraphicsCommandList* cmdList, const RootSignatureInfo& rs, RootBind bind, D3D12_GPU_VIRTUAL_ADDRESS address) {
	auto it = rs.slotOf.find(bind);
	if (it != rs.slotOf.end()) {
		cmdList->SetGraphicsRootConstantBufferView(it->second, address);
	}
}


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void RenderContext::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new RenderContext();
	instance_->InitInternal();
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void RenderContext::Release() {
	delete instance_; // ComPtrとFrameUploadBufferは、メンバの破棄で一緒に解放される
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// このフレームのライト
//=============================================================================
void RenderContext::SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights, const SpotLightListData& spotLights) {
	// GPUの処理が終わるのを待ってから次のフレームに進む作りなので、毎フレーム同じ場所を上書きしてよい
	std::memcpy(instance_->frameDirectionalLightMappedPtr_, &directionalLight, sizeof(DirectionalLightData));
	std::memcpy(instance_->framePointLightsMappedPtr_, &pointLights, sizeof(PointLightListData));
	std::memcpy(instance_->frameSpotLightsMappedPtr_, &spotLights, sizeof(SpotLightListData));
}


//=============================================================================
// フレームの終わり
//=============================================================================
void RenderContext::ResetFrame() { instance_->frameUploadBuffer_.Reset(); }


//=============================================================================
// メッシュ描画
//=============================================================================
void RenderContext::DrawMesh(const MeshRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;

	// ===== 定数（動的・静的共通） =====
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	D3D12_GPU_VIRTUAL_ADDRESS transformAddress = upload.PushConstant(req.objectTransformData);

	// ===== ジオメトリ =====
	uint32_t indexCount = 0;
	if (req.isStatic) {
		// --- 静的（Model）: GPU常駐バッファをバインドする ---
		cmdList->IASetVertexBuffers(0, 1, &req.vbv); // 頂点バッファ
		cmdList->IASetIndexBuffer(&req.ibv);         // インデックスバッファ
		indexCount = req.indexCount;                 // インデックス数
	} else {
		// --- 動的（Primitive）: 頂点とインデックスをこのフレーム用のバッファに書いて VBV / IBV を組む ---
		FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), req.vertices.size());
		FrameUploadBuffer::Allocation indices = upload.PushArray(req.indices.data(), req.indices.size());
		// VertexBufferView（型の区別は StrideInBytes で伝える）
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = vertices.gpuAddress;
		vbv.SizeInBytes = static_cast<UINT>(vertices.size);
		vbv.StrideInBytes = sizeof(Vertex3dData);
		// IndexBufferView（型の区別は Format で伝える）
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = indices.gpuAddress;
		ibv.SizeInBytes = static_cast<UINT>(indices.size);
		ibv.Format = DXGI_FORMAT_R32_UINT;

		cmdList->IASetVertexBuffers(0, 1, &vbv);
		cmdList->IASetIndexBuffer(&ibv);
		indexCount = static_cast<uint32_t>(req.indices.size());
	}
	// トポロジ
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Model, req.shadingType);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(SlotOf(rs, RootBind::Material), materialAddress);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), transformAddress);
	// --- ライト（そのシェーダーが使っている種類だけ結ぶ） ---
	BindConstantIfUsed(cmdList, rs, RootBind::DirectionalLight, instance_->frameDirectionalLightBuffer_->GetGPUVirtualAddress());
	BindConstantIfUsed(cmdList, rs, RootBind::PointLights, instance_->framePointLightsBuffer_->GetGPUVirtualAddress());
	BindConstantIfUsed(cmdList, rs, RootBind::SpotLights, instance_->frameSpotLightsBuffer_->GetGPUVirtualAddress());
	// --- IBL（PBRのRootSignatureにだけ存在する） ---
	if (req.shadingType == ShadingType::PBR) {
		auto it = rs.slotOf.find(RootBind::IBL);
		MY_ASSERT_MSG(it != rs.slotOf.end(), "PBRのRootSignatureにIBLスロットがありません");
		MY_ASSERT_MSG(req.iblParamsAddress != 0, "PBRにはIBLEnvironmentの設定が必要です");
		cmdList->SetGraphicsRootConstantBufferView(it->second, req.iblParamsAddress);
	}

	// ===== DrawCall =====
#ifdef _DEBUG
	// 例: "suzanne / Material.001"
	std::string marker = req.debugName ? *req.debugName : std::string("Primitive");
	if (req.debugSubName && !req.debugSubName->empty()) {
		marker += " / " + *req.debugSubName;
	}
	GPU_MARKER(cmdList, marker.c_str());
#endif
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}


//=============================================================================
// パーティクル描画
//=============================================================================
void RenderContext::DrawParticles(const ParticleRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Particle);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// ===== このフレーム用のバッファに書く =====
	// インスタンス配列（VSがStructuredBufferとして読む）
	FrameUploadBuffer::Allocation instances = upload.PushArray(req.instances.data(), req.instances.size());
	// グループのマテリアル
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);

	// ===== バインド =====
	// VSのParticle
	cmdList->SetGraphicsRootShaderResourceView(rs.slotOf.at(RootBind::Particles), instances.gpuAddress);
	// マテリアル
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);

	// quadをインスタンス数分
	UINT count = static_cast<UINT>(req.instances.size());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &instance_->particleQuadVBV_);
	cmdList->IASetIndexBuffer(&instance_->particleQuadIBV_);
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, count, 0, 0, 0);
}


//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const SpriteRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;

	// ===== このフレーム用のバッファに書く =====
	static constexpr uint32_t kIndices[] = {0, 1, 2, 1, 3, 2};
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), req.vertices.size());
	FrameUploadBuffer::Allocation indices = upload.PushArray(kIndices, std::size(kIndices));

	// ===== ジオメトリ =====
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = vertices.gpuAddress;
	vbv.SizeInBytes = static_cast<UINT>(vertices.size);
	vbv.StrideInBytes = sizeof(Vertex2dData);
	// IndexBufferView
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = indices.gpuAddress;
	ibv.SizeInBytes = static_cast<UINT>(indices.size);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignatureID
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Sprite);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(static_cast<UINT>(std::size(kIndices)), 1, 0, 0, 0);
}


//=============================================================================
// Line3D描画
//=============================================================================
void RenderContext::DrawLines(const LineRequest& req) {
	// 奇数は切り捨て。描く線が無ければ何もしない
	size_t vertexCount = req.vertices.size() & ~size_t(1);
	if (vertexCount == 0) {
		return;
	}
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Line);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// ===== このフレーム用のバッファに書く =====
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	D3D12_GPU_VIRTUAL_ADDRESS transformAddress = upload.PushConstant(req.objectTransformData);
	FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), vertexCount);

	// ===== ジオメトリ =====
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = vertices.gpuAddress;
	vbv.SizeInBytes = static_cast<UINT>(vertices.size);
	vbv.StrideInBytes = sizeof(VertexLineData);
	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// ===== ShaderConstantsバインド =====
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), transformAddress);

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
}


//=============================================================================
// 初期化（内部）
//=============================================================================
void RenderContext::InitInternal() {
	// ===== 描画ごとに増えるデータの置き場（1つだけ） =====
	frameUploadBuffer_.Initialize(kFrameUploadBytes, "frameUploadBuffer_");

	// ===== ライト（1フレームに1個。全描画で同じ場所を結ぶので、描画回数分の大きさは要らない） =====
	// 大きさはCreateUploadBufferの中で256の倍数にそろえられる
	frameDirectionalLightBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(DirectionalLightData), reinterpret_cast<void**>(&frameDirectionalLightMappedPtr_));
	frameDirectionalLightBuffer_->SetName(L"frameDirectionalLightBuffer_");
	framePointLightsBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(PointLightListData), reinterpret_cast<void**>(&framePointLightsMappedPtr_));
	framePointLightsBuffer_->SetName(L"framePointLightsBuffer_");
	frameSpotLightsBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(SpotLightListData), reinterpret_cast<void**>(&frameSpotLightsMappedPtr_));
	frameSpotLightsBuffer_->SetName(L"frameSpotLightsBuffer_");
	*frameDirectionalLightMappedPtr_ = DirectionalLightData{}; // 最初のSetFrameLightsまでの既定値（白・真下・強さ1）
	*framePointLightsMappedPtr_ = PointLightListData{};        // ポイントライト0個
	*frameSpotLightsMappedPtr_ = SpotLightListData{};          // スポットライト0個

	// ===== パーティクル用の共通Quad =====
	// 頂点フォーマットは Particle の InputLayout（POSITION + TEXCOORD） = VertexParticleData
	const Vertex2dData quadVertices[4] = {
	    {{-0.5f, +0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 左上
	    {{+0.5f, +0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 右上
	    {{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
	    {{+0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
	};
	const uint32_t quadIndices[6] = {0, 1, 2, 1, 3, 2};
	// 頂点バッファ：Uploadヒープに1回だけ書いてUnmap（永続Mapしない。二度と書き換えないので）
	particleQuadVB_ = DirectXCommon::CreateUploadBuffer(sizeof(quadVertices));
	particleQuadVB_->SetName(L"ParticleQuadVB");
	void* mapped = nullptr;
	particleQuadVB_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, quadVertices, sizeof(quadVertices));
	particleQuadVB_->Unmap(0, nullptr);
	particleQuadVBV_.BufferLocation = particleQuadVB_->GetGPUVirtualAddress();
	particleQuadVBV_.SizeInBytes = sizeof(quadVertices);
	particleQuadVBV_.StrideInBytes = sizeof(Vertex2dData);
	// インデックスバッファ
	particleQuadIB_ = DirectXCommon::CreateUploadBuffer(sizeof(quadIndices));
	particleQuadIB_->SetName(L"ParticleQuadIB");
	particleQuadIB_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, quadIndices, sizeof(quadIndices));
	particleQuadIB_->Unmap(0, nullptr);
	particleQuadIBV_.BufferLocation = particleQuadIB_->GetGPUVirtualAddress();
	particleQuadIBV_.SizeInBytes = sizeof(quadIndices);
	particleQuadIBV_.Format = DXGI_FORMAT_R32_UINT;
}


//=============================================================================
// GPUページフォルトのアドレスをどのリソースが原因かログに出力する
//=============================================================================
void RenderContext::LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA) {
	// 解放後に呼ばれたときは調べない
	if (!instance_) {
		return;
	}

	struct Entry {
		const char* name;
		ID3D12Resource* resource;
	};
	Entry buffers[] = {
	    {"frameUploadBuffer_",           instance_->frameUploadBuffer_.GetResource()  },
	    {"frameDirectionalLightBuffer_", instance_->frameDirectionalLightBuffer_.Get()},
	    {"framePointLightsBuffer_",      instance_->framePointLightsBuffer_.Get()     },
	    {"frameSpotLightsBuffer_",       instance_->frameSpotLightsBuffer_.Get()      },
	    {"particleQuadVB_",              instance_->particleQuadVB_.Get()             },
	    {"particleQuadIB_",              instance_->particleQuadIB_.Get()             },
	};

	for (const Entry& e : buffers) {
		if (!e.resource) {
			continue;
		}
		D3D12_GPU_VIRTUAL_ADDRESS base = e.resource->GetGPUVirtualAddress(); // GPUアドレス
		UINT64 size = e.resource->GetDesc().Width;                           // バッファのバイトサイズ

		if (faultVA >= base && faultVA < base + size) {
			UINT64 offset = faultVA - base;
			LogManager::Error(std::format("[DRED] fault VA is inside '{}'  offset = {} / {} bytes", e.name, offset, size));
			if (e.resource == instance_->frameUploadBuffer_.GetResource()) {
				// 使っていた範囲より後ろなら、確保していない場所を読んでいる
				LogManager::Error(std::format("[DRED] frameUploadBuffer_ used = {} bytes", instance_->frameUploadBuffer_.GetUsedBytes()));
			}
			return;
		}
	}
	LogManager::Error("[DRED] fault VA does not match any RenderContext buffer");
}