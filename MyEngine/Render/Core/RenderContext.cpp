#include "MyEngine/Render/Core/RenderContext.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Render/Core/PSOManager.h"
#include "MyEngine/Render/Core/RenderWindow.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Utils/Const.h"
#include <cassert>
#include <format>

RenderContext* RenderContext::instance_ = nullptr;

//=============================================================================
// RootParameterのスロット番号定数
//=============================================================================
namespace Slot3dLit {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1; // [1] b0 マテリアル(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
constexpr UINT Light = 3;    // [3] b1 DirectionalLight(PS)
constexpr UINT Camera = 4;   // [4] b2 CameraData(PS)
}
namespace Slot3dNoLit {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1; // [1] b0 マテリアル(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
} 
namespace SlotLine3d {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレス（Line3Dでは未使用）
constexpr UINT Material = 1; // [1] b0 LineMaterialCB(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
} 
namespace Slot2d {
constexpr UINT SRV = 0;        // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1;   // [1] b0 マテリアル(PS)
constexpr UINT WindowSize = 2; // [2] b0 ウィンドウサイズ(VS)
} 



//=============================================================================
// 初期化
//=============================================================================
void RenderContext::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new RenderContext();
	instance_->InitInternal();
	LogManager::Log("Initialized");
}

//=============================================================================
// 解放処理
//=============================================================================
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
// 2D描画開始処理
//=============================================================================
void RenderContext::StartDrawSprite() {
	auto* cmdList = DirectXCommon::GetCommandList();
	// 2D用RootSignatureをセット
	cmdList->SetGraphicsRootSignature(PSOManager::GetRootSignature(RootSignatureKey::Sprite2d));

	// SRVDescriptorHeapをセット（バインドレス：1回だけセットで全テクスチャにアクセス可能）
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);

	// バインドレスSRVをスロット0にセット（Heapの先頭から全スロットを開放）
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(Slot2d::SRV, heapStart);
}

//=============================================================================
// 3D描画開始処理
//=============================================================================
void RenderContext::StartDrawModel() {
	auto* cmdList = DirectXCommon::GetCommandList();

	// SRVDescriptorHeapをセット（バインドレス：1回だけセットで全テクスチャにアクセス可能）
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
}

//=============================================================================
// ビューポートとシザーの設定
//=============================================================================
void RenderContext::SetViewportAndScissor(float width, float height, float offsetX, float offsetY) {
	D3D12_VIEWPORT viewport{};
	viewport.Width = width;
	viewport.Height = height;
	viewport.TopLeftX = offsetX;
	viewport.TopLeftY = offsetY;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissor{};
	scissor.left = static_cast<LONG>(offsetX);
	scissor.top = static_cast<LONG>(offsetY);
	scissor.right = static_cast<LONG>(offsetX + width);
	scissor.bottom = static_cast<LONG>(offsetY + height);

	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);
}

//=============================================================================
// シェーディングモデルの設定
// RootSignatureを切り替えてバインドレスSRVをセットする
//=============================================================================
void RenderContext::SetShadingModel(ShadingModel model) {
	auto* cmdList = DirectXCommon::GetCommandList();
	instance_->currentShadingModel_ = model;

	// RootSignatureをセット
	ID3D12RootSignature* targetRootSignature = nullptr;
	switch (model) {
	case ShadingModel::Lambert:
	case ShadingModel::HalfLambert:
	    // HalfでもなくてもBind情報は共通
		targetRootSignature = PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Lambert));
		break;
	case ShadingModel::Unlit:
	default:
		targetRootSignature = PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Unlit));
		break;
	}
	cmdList->SetGraphicsRootSignature(targetRootSignature);

	// RootSignature切り替え後はバインドレスSRVを再セットする必要がある
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	bool isLit = (model != ShadingModel::Unlit);
	cmdList->SetGraphicsRootDescriptorTable(isLit ? Slot3dLit::SRV : Slot3dNoLit::SRV, heapStart);
}

//=============================================================================
// ShadingModelに対応するPSOを選択する
//=============================================================================
ID3D12PipelineState* RenderContext::SelectPSO(ShadingModel model, BlendMode blendMode) { return PSOManager::GetPSO({PSOKey::Model(model, blendMode)}); }

//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow) {
	auto* cmdList = DirectXCommon::GetCommandList();
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->vertex2dIndex_ + 4 <= kMaxVertices, "頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->index2dIndex_ + 6 <= kMaxVertices, "インデックス数が上限を超えました");

	// ===== PSO切り替え =====
	cmdList->SetPipelineState(PSOManager::GetPSO({ShaderID::Sprite2d}));

	// ===== マテリアル書き込み =====
	size_t material2DSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterial2dDataSlotSize_;
	std::memcpy(instance_->material2dDataMappedPtr_ + material2DSlotOffset, &desc.material, sizeof(Material2dData));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = instance_->vertex2dIndex_ * sizeof(Vertex2dData);
	std::memcpy(instance_->vertex2dDataMappedPtr_ + vertexByteOffset, desc.vertices, sizeof(Vertex2dData) * 4);

	// ===== インデックス書き込み =====
	uint32_t indices[] = {0, 1, 2, 1, 3, 2};
	size_t indexByteOffset = instance_->index2dIndex_ * sizeof(uint32_t);
	std::memcpy(instance_->index2dDataMappedPtr_ + indexByteOffset, indices, sizeof(indices));

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertex2dDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(sizeof(Vertex2dData) * 4);
	vbv.StrideInBytes = sizeof(Vertex2dData);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = instance_->index2dDataRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(sizeof(indices));
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（StartDrawSpriteで1回だけセット済みなので毎回不要）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::Material, instance_->material2dDataRingBuffer_->GetGPUVirtualAddress() + material2DSlotOffset);
	// スロット[2]: ウィンドウサイズ(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::WindowSize, renderWindow->GetWindowSizeBuffer()->GetGPUVirtualAddress());

	// DrawCallとカウントを増加
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	instance_->vertex2dIndex_ += 4;
	instance_->index2dIndex_ += 6;
	instance_->drawCallIndex_++;
}

//=============================================================================
// 3Dモデル描画
//=============================================================================
void RenderContext::DrawModel(const DrawModelDesc& desc) {
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->vertex3dIndex_ + desc.vertices.size() <= kMaxVertices, "頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->index3dIndex_ + desc.indices.size() <= kMaxVertices, "インデックス数が上限を超えました");
	
	auto* cmdList = DirectXCommon::GetCommandList();
	bool isLit = (instance_->currentShadingModel_ != ShadingModel::Unlit);

	// ===== PSO切り替え =====
	cmdList->SetPipelineState(instance_->SelectPSO(instance_->currentShadingModel_));

	// ===== CBVリングバッファ書き込み =====
	size_t material3dSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterial3dDataSlotSize_;
	std::memcpy(instance_->material3dDataMappedPtr_ + material3dSlotOffset, &desc.material, sizeof(Material3dData));

	size_t matricesSlotOffset = instance_->drawCallIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrixData));

	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraData));

	// ===== 頂点・インデックス書き込み =====
	size_t vertexByteOffset = instance_->vertex3dIndex_ * sizeof(Vertex3dData);
	size_t vertex3DDataSize = sizeof(Vertex3dData) * desc.vertices.size();
	std::memcpy(instance_->vertex3dDataMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertex3DDataSize);

	size_t indexByteOffset = instance_->index3dIndex_ * sizeof(uint32_t);
	size_t indexDataSize = sizeof(uint32_t) * desc.indices.size();
	std::memcpy(instance_->index3dDataMappedPtr_ + indexByteOffset, desc.indices.data(), indexDataSize);

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertex3dDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertex3DDataSize);
	vbv.StrideInBytes = sizeof(Vertex3dData);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = instance_->index3dDataRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(indexDataSize);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（SetShadingModelで切り替え時に再セット済み）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Material : Slot3dNoLit::Material, instance_->material3dDataRingBuffer_->GetGPUVirtualAddress() + material3dSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Matrices : Slot3dNoLit::Matrices, instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	// スロット[3]: DirectionalLight(b1, PS) Litのみ
	if (isLit && desc.directionalLight) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Light, desc.directionalLight->GetBuffer()->GetGPUVirtualAddress());
	}
	// スロット[4]: CameraData(b2, PS) Litのみ
	if (isLit) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Camera, instance_->cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	// DrawCallとカウントを増加
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(static_cast<UINT>(desc.indices.size()), 1, 0, 0, 0);

	instance_->vertex3dIndex_ += desc.vertices.size();
	instance_->index3dIndex_ += desc.indices.size();
	instance_->drawCallIndex_++;
}

//=============================================================================
// 静的メッシュ描画（頂点・インデックスはGPU常駐。CBVだけ毎フレーム更新）
//=============================================================================
void RenderContext::DrawStaticMesh(const DrawStaticMeshDesc& desc) {
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	auto* cmdList = DirectXCommon::GetCommandList();

	// ===== PSO切り替え =====
	bool isLit = (instance_->currentShadingModel_ != ShadingModel::Unlit);
	cmdList->SetPipelineState(instance_->SelectPSO(instance_->currentShadingModel_, desc.blendMode));

	// ===== CBVリングバッファ書き込み =====
	size_t modelMatSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterial3dDataSlotSize_;
	std::memcpy(instance_->material3dDataMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(Material3dData));

	size_t matricesSlotOffset = instance_->drawCallIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrixData));

	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraData));

	// ===== 頂点・インデックスバッファのバインド（常駐バッファを指すだけ。memcpy無し）=====
	cmdList->IASetVertexBuffers(0, 1, &desc.vbv);
	cmdList->IASetIndexBuffer(&desc.ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== CBVバインド =====
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Material : Slot3dNoLit::Material, instance_->material3dDataRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Matrices : Slot3dNoLit::Matrices, instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	if (isLit && desc.directionalLight) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Light, desc.directionalLight->GetBuffer()->GetGPUVirtualAddress());
	}
	if (isLit) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Camera, instance_->cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	// ===== 描画（インデックス描画）=====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(desc.indexCount, 1, 0, 0, 0);

	instance_->drawCallIndex_++;
}

//=============================================================================
// Line3D描画
//=============================================================================
void RenderContext::DrawLines3d(const DrawLines3dDesc& desc) {
	if (desc.vertices.empty()) {
		return;
	}
	// LINELISTは2頂点で1本のため奇数個の場合は最後を切り捨て
	size_t vertexCount = desc.vertices.size() & ~size_t(1);
	if (vertexCount == 0) {
		return;
	}
	MY_ASSERT_MSG(instance_->vertexLineIndex_ + vertexCount <= kMaxLineVertices, "ライン頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->drawCallLineIndex_ < kMaxDrawCalls, "ドローコール数が上限を超えました");

	auto* cmdList = DirectXCommon::GetCommandList();

	// ===== PSO・RootSignatureをセット =====
	cmdList->SetPipelineState(PSOManager::GetPSO({ShaderID::Line3d}));
	cmdList->SetGraphicsRootSignature(PSOManager::GetRootSignature(RootSignatureKey::Line3d));

	// RootSignature切り替え後はバインドレスSRVを再セットする
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(SlotLine3d::SRV, heapStart);

	// ===== CBVリングバッファ書き込み =====
	size_t matSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMaterialLineDataSlotSize_;
	std::memcpy(instance_->materialLineDataMappedPtr_ + matSlotOffset, &desc.material, sizeof(MaterialLineData));

	size_t matrixSlotOffset = instance_->drawCallLineIndex_ * instance_->alignedMatricesDataSlotSize_;
	std::memcpy(instance_->matricesDataMapperPtr_ + matrixSlotOffset, &desc.matrices, sizeof(TransformationMatrixData));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = instance_->vertexLineIndex_ * sizeof(VertexLineData);
	size_t vertexDataSize = sizeof(VertexLineData) * vertexCount;
	std::memcpy(instance_->vertexLineDataMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	// ===== VBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertexLineDataRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(VertexLineData);

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// スロット[0]: バインドレスSRV（上でセット済み・Line3Dでは未使用）
	// スロット[1]: LineMaterialCB(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Material, instance_->materialLineDataRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Matrices, instance_->matricesDataRingBuffer_->GetGPUVirtualAddress() + matrixSlotOffset);

	// DrawCallとカウントを増加
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
	Make(materialLineDataRingBuffer_, &materialLineDataMappedPtr_, alignedMaterialLineDataSlotSize_ * kMaxDrawCalls, "materialLineRingBuffer_");
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