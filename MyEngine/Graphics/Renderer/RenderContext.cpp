#include "MyEngine/Graphics/Renderer/RenderContext.h"

#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

// 静的メンバ変数 
RenderContext* RenderContext::instance_ = nullptr;


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
	// 頂点
	instance_->vertex2dIndex_ = 0;
	instance_->vertex3dIndex_ = 0;
	instance_->vertexLineIndex_ = 0;
	// インデックス
	instance_->index2dIndex_ = 0;
	instance_->index3dIndex_ = 0;
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
	std::memcpy(instance_->matricesDataMapperPtr_ + matricesSlotOffset, &req.transformationMatricesData, sizeof(TransformationMatrixData));
	// カメラ
	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &req.cameraData, sizeof(CameraData));
	// ライト
	size_t lightSlotOffset = instance_->drawCallIndex_ * instance_->alignedLightDataSlotSize_;
	std::memcpy(instance_->lightDataMappedPtr_ + lightSlotOffset, &req.lightData, sizeof(DirectionalLightData));

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
	// RootSignatureID
	RootSignatureID rsID = RootSignatureManager::GetRootSignatureID(DrawCategory::Model, req.shadingType);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::Material).value(), 
		instance_->material3dDataRingBuffer_->GetGPUVirtualAddress() + materialSlotOffset);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::TransformationMatrix).value(), 
		instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	// --- Lit系のみ存在するスロット ---
	if (req.shadingType != ShadingType::Unlit) {
		// Light
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::DirectionalLight).value(), 
			instance_->lightDataRingBuffer_->GetGPUVirtualAddress() + lightSlotOffset);
		// camera
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::Camera).value(), 
			instance_->cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	// ===== DrawCall =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
	instance_->drawCallIndex_++;
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
	RootSignatureID rsID = RootSignatureID::Sprite;
	// Material
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::Material).value(), 
		instance_->materialLineDataRingBuffer_->GetGPUVirtualAddress() + material2DSlotOffset);
	// WindowSize
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(rsID, RootBind::WindowSize).value(), 
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

	// ===== リングバッファ書き込み =====
	// Material
	size_t matSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMaterialLineDataSlotSize_;
	std::memcpy(instance_->materialLineDataMappedPtr_ + matSlotOffset, &req.materialData, sizeof(MaterialLineData));
	// TransformationMatrix
	size_t matrixSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matrixSlotOffset, &req.transformationMatricesData, sizeof(TransformationMatrixData));

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
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureManager::GetBindSlot(RootSignatureID::Line, RootBind::Material).value(), 
		instance_->materialLineDataRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(
	    RootSignatureManager::GetBindSlot(RootSignatureID::Line, RootBind::TransformationMatrix).value(), 
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
	Make(materialLineDataRingBuffer_, &materialLineDataMappedPtr_, alignedMaterialLineDataSlotSize_ * kMaxDrawCalls, "materialLineDataRingBuffer_");
	// ライト
	Make(lightDataRingBuffer_, &lightDataMappedPtr_, alignedLightDataSlotSize_ * kMaxDrawCalls, "lightDataRingBuffer_");
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
        {"materialLineDataRingBuffer_",  instance_->materialLineDataRingBuffer_.Get() },
		// ライト
	    {"lightDataRingBuffer_", instance_->lightDataRingBuffer_.Get()},
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