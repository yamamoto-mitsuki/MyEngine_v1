#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include <format>
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

// 静的メンバ変数 
RenderContext* RenderContext::instance_ = nullptr;

static UINT SlotOf(const RootSignatureInfo& rs, RootBind bind) {
	auto it = rs.slotOf.find(bind);
	MY_ASSERT_MSG(it != rs.slotOf.end(), std::format("slotOf に RootBind::{} が無い。NameToRoleの名前とHLSLの変数名が不一致の可能性", magic_enum::enum_name(bind)));
	return it->second;
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
	// 共通
	instance_->matricesDataRingBuffer_ = nullptr;
	instance_->cameraDataRingBuffer_ = nullptr;
	// 頂点
	instance_->vertex2dDataRingBuffer_ = nullptr;
	instance_->vertex3dDataRingBuffer_ = nullptr;
	instance_->vertexLineDataRingBuffer_ = nullptr;
	// インデックス
	instance_->index2dDataRingBuffer_ = nullptr;
	instance_->index3dDataRingBuffer_ = nullptr;
	// マテリアル
	instance_->material2dDataRingBuffer_ = nullptr;
	instance_->material3dDataRingBuffer_ = nullptr;
	instance_->materialLineDataRingBuffer_ = nullptr;
}

//=============================================================================
// 描画カウント・頂点カウントをリセット
//=============================================================================
void RenderContext::ResetDrawCallIndex() {
	// Draw Call
	instance_->drawCallIndex_ = 0;
	instance_->drawCallLineIndex_ = 0;
	instance_->drawCallParticleIndex_ = 0;
	// 頂点
	instance_->vertex2dIndex_ = 0;
	instance_->vertex3dIndex_ = 0;
	instance_->vertexLineIndex_ = 0;
	// インデックス
	instance_->index2dIndex_ = 0;
	instance_->index3dIndex_ = 0;
	// パーティクル
	instance_->particleIndex_ = 0;
}


//=============================================================================
// メッシュ描画
//=============================================================================
void RenderContext::DrawMesh(const MeshRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数上限を超えました");

	// ===== リングバッファ書き込み（動的・静的共通） =====
	// マテリアル
	size_t materialSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterial3dDataSlotSize_;
	std::memcpy(instance_->material3dDataMappedPtr_ + materialSlotOffset, &req.materialData, sizeof(Material3dData));
	// 行列
	size_t matricesSlotOffset = instance_->drawCallIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matricesSlotOffset, &req.objectTransformData, sizeof(ObjectTransformData));
	// ライト
	size_t directionalLightSlotOffset = instance_->drawCallIndex_ * instance_->alignedDirectionlLightDataSlotSize_;
	std::memcpy(instance_->directionalLightDataMappedPtr_ + directionalLightSlotOffset, &req.directionalLightData, sizeof(DirectionalLightData));
	size_t pointLightSlotOffset = instance_->drawCallIndex_ * instance_->alignedPointLightDataSlotSize_;
	std::memcpy(instance_->pointLightDataMappedPtr_ + pointLightSlotOffset, &req.pointLightListData, sizeof(PointLightListData));

	// ===== ジオメトリ =====
	uint32_t indexCount = 0;
	if (req.isStatic) {
		// --- 静的（Model）: GPU常駐バッファをバインドする ---
		cmdList->IASetVertexBuffers(0, 1, &req.vbv); // 頂点バッファ
		cmdList->IASetIndexBuffer(&req.ibv);         // インデックスバッファ
		indexCount = req.indexCount;                 // インデックス数
	} else {
		// --- 動的（Primitive）: リングバッファへ書いて VBV / IBV を組む ---
		MY_ASSERT_MSG(instance_->vertex3dIndex_ + req.vertices.size() <= kMaxVertices, "頂点数が上限を超えました");
		MY_ASSERT_MSG(instance_->index3dIndex_ + req.indices.size() <= kMaxVertices, "インデックス数が上限を超えました");
		// 頂点バッファ
		size_t vertexByteOffset = instance_->vertex3dIndex_ * sizeof(Vertex3dData);
		size_t vertexDataSize = sizeof(Vertex3dData) * req.vertices.size();
		std::memcpy(instance_->vertex3dDataMappedPtr_ + vertexByteOffset, req.vertices.data(), vertexDataSize);
		// インデックスバッファ
		size_t indexByteOffset = instance_->index3dIndex_ * sizeof(uint32_t);
		size_t indexDataSize = sizeof(uint32_t) * req.indices.size();
		std::memcpy(instance_->index3dDataMappedPtr_ + indexByteOffset, req.indices.data(), indexDataSize);
		// VertexBufferView
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = instance_->vertex3dDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
		vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
		vbv.StrideInBytes = sizeof(Vertex3dData);
		// IndexBufferView
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = instance_->index3dDataRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
		ibv.SizeInBytes = static_cast<UINT>(indexDataSize);
		ibv.Format = DXGI_FORMAT_R32_UINT;

		cmdList->IASetVertexBuffers(0, 1, &vbv);
		cmdList->IASetIndexBuffer(&ibv);
		indexCount = static_cast<uint32_t>(req.indices.size());
		instance_->vertex3dIndex_ += req.vertices.size();
		instance_->index3dIndex_ += req.indices.size();
	}
	// トポロジ
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Model, req.shadingType);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(SlotOf(rs, RootBind::Material), 
		instance_->material3dDataRingBuffer_->GetGPUVirtualAddress() + materialSlotOffset);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), 
		instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	// --- Lit系のみ存在するスロット ---
	if (req.shadingType != ShadingType::Unlit) {
		// DirectionalLight
		cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind ::DirectionalLight), 
			instance_->directionalLightDataRingBuffer_->GetGPUVirtualAddress() + directionalLightSlotOffset);
		// PointLight
		cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::PointLights), 
			instance_->pointLightDataRingBuffer_->GetGPUVirtualAddress() + pointLightSlotOffset);
	
		// IBL（PBRのRootSignatureにだけ存在する）
		if (req.shadingType == ShadingType::PBR) {
			auto it = rs.slotOf.find(RootBind::IBL);
			MY_ASSERT_MSG(it != rs.slotOf.end(), "PBRのRootSignatureにIBLスロットがありません");
			MY_ASSERT_MSG(req.iblParamsAddress != 0, "PBRにはIBLEnvironmentの設定が必要です");
			cmdList->SetGraphicsRootConstantBufferView(it->second, req.iblParamsAddress);
		}
	}

	// ===== DrawCall =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
	instance_->drawCallIndex_++;
}


//=============================================================================
// パーティクル描画
//=============================================================================
void RenderContext::DrawParticles(const ParticleRequest& req) {
	auto& inst = *instance_;
	auto* cmdList = DirectXCommon::GetCommandList();
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Particle);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// リングバッファの残り容量に収める
	UINT count = static_cast<UINT>(req.instances.size());
	MY_ASSERT_MSG(inst.particleIndex_ + count <= kMaxParticleInstances, "パーティクルのリングバッファが不足しています");

	// インスタンス配列を今フレームのオフセット位置へコピー
	size_t instByteOffset = inst.particleIndex_ * sizeof(ParticleData);
	std::memcpy(inst.particleDataMappedPtr_ + instByteOffset, req.instances.data(), sizeof(ParticleData) * count);
	// グループマテリアルをスロットへコピー
	size_t matSlotOffset = inst.drawCallParticleIndex_ * inst.alignedMaterialParticleDataSlotSize_;
	std::memcpy(inst.materialParticleDataMappedptr_ + matSlotOffset, &req.materialData, sizeof(MaterialParticleData));

	// --- バインド ---
	// VSのParticle
	cmdList->SetGraphicsRootShaderResourceView(rs.slotOf.at(RootBind::Particles), inst.particleDataRingBuffer_->GetGPUVirtualAddress() + instByteOffset);
	// マテリアル
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material),  inst.materialParticleDataRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);

	// quadをインスタンス数分
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &inst.particleQuadVBV_);
	cmdList->IASetIndexBuffer(&inst.particleQuadIBV_);
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, count, 0, 0, 0);

	// オフセットを進める
	inst.particleIndex_ += count;
	inst.drawCallParticleIndex_++;
}


//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const SpriteRequest& req, RenderWindow* renderWindow) {
	auto* cmdList = DirectXCommon::GetCommandList();
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->vertex2dIndex_ + 4 <= kMaxVertices, "頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->index2dIndex_ + 6 <= kMaxVertices, "インデックス数が上限を超えました");

	// ===== リングバッファ書き込み =====
	// Material
	size_t material2DSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterial2dDataSlotSize_;
	std::memcpy(instance_->material2dDataMappedPtr_ + material2DSlotOffset, &req.materialData, sizeof(Material2dData));

	// ===== ジオメトリ =====
	// 頂点バッファ
	size_t vertexByteOffset = instance_->vertex2dIndex_ * sizeof(Vertex2dData);
	std::memcpy(instance_->vertex2dDataMappedPtr_ + vertexByteOffset, req.vertices.data(), sizeof(Vertex2dData) * 4);
	// インデックスバッファ
	uint32_t indices[] = {0, 1, 2, 1, 3, 2};
	size_t indexByteOffset = instance_->index2dIndex_ * sizeof(uint32_t);
	std::memcpy(instance_->index2dDataMappedPtr_ + indexByteOffset, indices, sizeof(indices));
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertex2dDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(sizeof(Vertex2dData) * 4);
	vbv.StrideInBytes = sizeof(Vertex2dData);
	// IndexBufferView
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = instance_->index2dDataRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(sizeof(indices));
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignatureID
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Sprite);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), 
		instance_->material2dDataRingBuffer_->GetGPUVirtualAddress() + material2DSlotOffset);
	// WindowSize
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::WindowSize),
		renderWindow->GetWindowSizeBuffer()->GetGPUVirtualAddress());

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	instance_->vertex2dIndex_ += 4;
	instance_->index2dIndex_ += 6;
	instance_->drawCallIndex_++;
}


//=============================================================================
// Line3D描画
//=============================================================================
void RenderContext::DrawLines(const LineRequest& req) {
	// 中身が空ではないか
	if (req.vertices.empty()) {
		return;
	}
	// 奇数は切り捨て
	size_t vertexCount = req.vertices.size() & ~size_t(1);
	MY_ASSERT_MSG(instance_->vertexLineIndex_ + vertexCount <= kMaxLineVertices, "ライン頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->drawCallLineIndex_ < kMaxDrawCalls, "ドローコール数が上限を超えました");
	auto* cmdList = DirectXCommon::GetCommandList();
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Line);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// ===== リングバッファ書き込み =====
	// Material
	size_t matSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMaterialLineDataSlotSize_;
	std::memcpy(instance_->materialLineDataMappedPtr_ + matSlotOffset, &req.materialData, sizeof(MaterialLineData));
	// TransformationMatrix
	size_t matrixSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matrixSlotOffset, &req.objectTransformData, sizeof(ObjectTransformData));

	// ===== ジオメトリ =====
	// 頂点バッファ
	size_t vertexByteOffset = instance_->vertexLineIndex_ * sizeof(VertexLineData);
	size_t vertexDataSize = sizeof(VertexLineData) * vertexCount;
	std::memcpy(instance_->vertexLineDataMappedPtr_ + vertexByteOffset, req.vertices.data(), vertexDataSize);
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertexLineDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(VertexLineData);
	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// ===== ShaderConstantsバインド =====
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), 
		instance_->materialLineDataRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), 
		instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matrixSlotOffset);

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
	instance_->vertexLineIndex_ += vertexCount;
	instance_->drawCallLineIndex_++;

}

//=============================================================================
// 初期化（内部）
//=============================================================================
void RenderContext::InitInternal() {
	// ===== リングバッファ作成 =====
	// リングバッファをまとめて生成するためのラムダ
	auto Make = [&](auto& buf, auto** ptr, size_t size, const char* name) {
		if (!buf) {
			buf = DirectXCommon::CreateMappedUploadBuffer(size, reinterpret_cast<void**>(ptr));
			buf->SetName(ConvertString(name).c_str());
			LogManager::Log(name);
		}
	};
	// 共通
	Make(matricesDataRingBuffer_, &matricesDataMapperPtr_, alignedMatricesDataSlotSize_ * kMaxDrawCalls, "matricesDataRingBuffer_");
	Make(cameraDataRingBuffer_, &cameraDataMappedPtr_, alignedCameraDataSlotSize_ * kMaxDrawCalls, "cameraDataRingBuffer_");
	// 頂点
	Make(vertex3dDataRingBuffer_, &vertex3dDataMappedPtr_, sizeof(Vertex3dData) * kMaxVertices, "vertexData3dRingBuffer_");
	Make(vertex2dDataRingBuffer_, &vertex2dDataMappedPtr_, sizeof(Vertex2dData) * kMaxVertices, "vertexData2dRingBuffer_");
	Make(vertexLineDataRingBuffer_, &vertexLineDataMappedPtr_, sizeof(VertexLineData) * kMaxLineVertices, "lineVertex3dRingBuffer_");
	// インデックス
	Make(index2dDataRingBuffer_, &index2dDataMappedPtr_, sizeof(uint32_t) * kMaxVertices, "indexData2dRingBuffer_");
	Make(index3dDataRingBuffer_, &index3dDataMappedPtr_, sizeof(uint32_t) * kMaxVertices, "indexData3dRingBuffer_");
	// マテリアル
	Make(material2dDataRingBuffer_, &material2dDataMappedPtr_, alignedMaterial2dDataSlotSize_ * kMaxDrawCalls, "material2dDataRingBuffer_");
	Make(material3dDataRingBuffer_, &material3dDataMappedPtr_, alignedMaterial3dDataSlotSize_ * kMaxDrawCalls, "material3dDataRingBuffer_");
	Make(materialParticleDataRingBuffer_, &materialParticleDataMappedptr_, alignedMaterialParticleDataSlotSize_ * kMaxDrawCalls, "materialParticleDataRingBuffer_");
	Make(materialLineDataRingBuffer_, &materialLineDataMappedPtr_, alignedMaterialLineDataSlotSize_ * kMaxDrawCalls, "materialLineDataRingBuffer_");
	// ライト
	Make(directionalLightDataRingBuffer_, &directionalLightDataMappedPtr_, alignedDirectionlLightDataSlotSize_ * kMaxDrawCalls, "directionalLightDataRingBuffer_");
	Make(pointLightDataRingBuffer_, &pointLightDataMappedPtr_, alignedPointLightDataSlotSize_ * kMaxDrawCalls, "pointLightDataRingBuffer_");
	// パーティクル
	Make(particleDataRingBuffer_, &particleDataMappedPtr_, sizeof(ParticleData) * kMaxParticleInstances, "particleRingBuffer_");

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
	struct Entry {
		const char* name;
		ID3D12Resource* resource;
	};

	Entry buffers[] = {
		// 共通
	    {"matricesDataRingBuffer_",   instance_->matricesDataRingBuffer_.Get()  },
	    {"cameraDataRingBuffer_",     instance_->cameraDataRingBuffer_.Get()    },
		// 頂点
	    {"vertex2dDataRingBuffer_",   instance_->vertex2dDataRingBuffer_.Get()  },
	    {"vertex3dDataRingBuffer_",   instance_->vertex3dDataRingBuffer_.Get()  },
	    {"vertexLineDataRingBuffer_",   instance_->vertexLineDataRingBuffer_.Get()  },
		// インデックス
	    {"indexData2dRingBuffer_",    instance_->index2dDataRingBuffer_.Get()   },
	    {"indexData3dRingBuffer_",    instance_->index3dDataRingBuffer_.Get()   },
		// マテリアル
	    {"material2dDataRingBuffer_", instance_->material2dDataRingBuffer_.Get()},
        {"material3dDataRingBuffer_", instance_->material3dDataRingBuffer_.Get()},
	    {"materialParticleRingBuffer_",instance_->materialParticleDataRingBuffer_.Get()},
        {"materialLineDataRingBuffer_",  instance_->materialLineDataRingBuffer_.Get() },
		// ライト
	    {"directionalLightDataRingBuffer_", instance_->directionalLightDataRingBuffer_.Get()},
	    {"pointLightDataRingBuffer_", instance_->pointLightDataRingBuffer_.Get()},
		// パーティクル
	    {"particleDataRingBuffer_", instance_->particleDataRingBuffer_.Get()},
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
			return;
		}
	}
	LogManager::Error("[DRED] fault VA does not match any RenderContext ring buffer");
}

size_t RenderContext::AlignTo256(size_t size) { return (size + 255) & ~255; }