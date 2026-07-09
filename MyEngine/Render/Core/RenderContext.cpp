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
namespace Slot3dNoLit {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1; // [1] b0 マテリアル(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
}
namespace Slot3dLit {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1; // [1] b0 マテリアル(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
constexpr UINT Light = 3;    // [3] b1 DirectionalLight(PS)
constexpr UINT Camera = 4;   // [4] b2 CameraData(PS)
}
namespace Slot2d {
constexpr UINT SRV = 0;        // [0] t0〜 バインドレステクスチャ
constexpr UINT Material = 1;   // [1] b0 マテリアル(PS)
constexpr UINT WindowSize = 2; // [2] b0 ウィンドウサイズ(VS)
} 
namespace SlotLine3d {
constexpr UINT SRV = 0;      // [0] t0〜 バインドレス（Line3Dでは未使用）
constexpr UINT Material = 1; // [1] b0 LineMaterialCB(PS)
constexpr UINT Matrices = 2; // [2] b0 行列(VS)
} 
namespace SlotInstLit {
constexpr UINT SRV = 0;       // [0] t0 space0 バインドレステクスチャ
constexpr UINT Material = 1;  // [1] b0 マテリアル(PS)
constexpr UINT Instances = 2; // [2] t0 space1 インスタンス配列(VS)
constexpr UINT Light = 3;     // [3] b1 ライト(PS)
constexpr UINT Camera = 4;    // [4] b2 カメラ(PS)
} 
namespace SlotInstNoLit {
constexpr UINT SRV = 0;
constexpr UINT Material = 1;
constexpr UINT Instances = 2;
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
	instance_->materialRingBuffer_ = nullptr;
	instance_->modelMaterialRingBuffer_ = nullptr;
	instance_->matricesRingBuffer_ = nullptr;
	instance_->cameraDataRingBuffer_ = nullptr;
	instance_->vertexData3dRingBuffer_ = nullptr;
	instance_->vertexData2dRingBuffer_ = nullptr;
	instance_->indexData3dRingBuffer_ = nullptr;
	instance_->indexData2dRingBuffer_ = nullptr;
	instance_->lineVertex3dRingBuffer_ = nullptr;
	instance_->lineMaterialRingBuffer_ = nullptr;
	instance_->lineMatricesRingBuffer_ = nullptr;
	instance_->instanceDataBuffer_ = nullptr;
}

//=============================================================================
// 描画カウント・頂点カウントをリセット
//=============================================================================
void RenderContext::ResetDrawCallIndex() {
	instance_->drawCallIndex_ = 0;
	instance_->vertexIndex3D_ = 0;
	instance_->vertexIndex2D_ = 0;
	instance_->indexIndex3D_ = 0;
	instance_->indexIndex2D_ = 0;
	instance_->lineVertexIndex_ = 0;
	instance_->lineDrawCallIndex_ = 0;
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
		targetRootSignature = PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Lambert, false));
		break;
	case ShadingModel::Unlit:
	default:
		targetRootSignature = PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Unlit,false));
		break;
	}
	cmdList->SetGraphicsRootSignature(targetRootSignature);

	// RootSignature切り替え後はバインドレスSRVを再セットする必要がある
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	bool isLit = (model != ShadingModel::Unlit);
	cmdList->SetGraphicsRootDescriptorTable(isLit ? Slot3dLit::SRV : Slot3dNoLit::SRV, heapStart);
}

void RenderContext::SetShadingModelInstanced(ShadingModel model) {
	auto* cmdList = DirectXCommon::GetCommandList();
	instance_->currentShadingModel_ = model;

	bool isLit = (model != ShadingModel::Unlit);
	ID3D12RootSignature* rs = isLit ? PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Lambert, true)) 
		                            : PSOManager::GetRootSignature(RootSignatureKey::ModelKey(ShadingModel::Unlit, true));
	cmdList->SetGraphicsRootSignature(rs);

	// ルートシグネチャ切替後はバインドレスSRVを再セット
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(isLit ? SlotInstLit::SRV : SlotInstNoLit::SRV, heapStart);
}

//=============================================================================
// ShadingModelに対応するPSOを選択する
//=============================================================================
ID3D12PipelineState* RenderContext::SelectPSO(ShadingModel model, BlendMode blendMode) { return PSOManager::GetPSO({PSOKey::Model(model, false, blendMode)}); }
ID3D12PipelineState* RenderContext::SelectInstancedPSO(ShadingModel model) { return PSOManager::GetPSO({PSOKey::Model(model, true)});
}

//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow) {
	auto* cmdList = DirectXCommon::GetCommandList();
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->vertexIndex2D_ + 4 <= kMaxVertices, "頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->indexIndex2D_ + 6 <= kMaxVertices, "インデックス数が上限を超えました");

	// ===== PSO切り替え =====
	cmdList->SetPipelineState(PSOManager::GetPSO({ShaderID::Sprite2d}));

	// ===== マテリアル書き込み =====
	size_t materialSlotOffset = instance_->drawCallIndex_ * instance_->alignedMaterialSlotSize_;
	std::memcpy(instance_->materialMappedPtr_ + materialSlotOffset, &desc.material, sizeof(Material));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = instance_->vertexIndex2D_ * sizeof(VertexData2D);
	std::memcpy(instance_->vertexData2dMappedPtr_ + vertexByteOffset, desc.vertices, sizeof(VertexData2D) * 4);

	// ===== インデックス書き込み =====
	uint32_t indices[] = {0, 1, 2, 1, 3, 2};
	size_t indexByteOffset = instance_->indexIndex2D_ * sizeof(uint32_t);
	std::memcpy(instance_->indexData2dMappedPtr_ + indexByteOffset, indices, sizeof(indices));

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertexData2dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(sizeof(VertexData2D) * 4);
	vbv.StrideInBytes = sizeof(VertexData2D);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = instance_->indexData2dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(sizeof(indices));
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（StartDrawSpriteで1回だけセット済みなので毎回不要）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::Material, instance_->materialRingBuffer_->GetGPUVirtualAddress() + materialSlotOffset);
	// スロット[2]: ウィンドウサイズ(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::WindowSize, renderWindow->GetWindowSizeBuffer()->GetGPUVirtualAddress());

	// DrawCallとカウントを増加
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	instance_->vertexIndex2D_ += 4;
	instance_->indexIndex2D_ += 6;
	instance_->drawCallIndex_++;
}

//=============================================================================
// 3Dモデル描画
//=============================================================================
void RenderContext::DrawModel(const DrawModelDesc& desc) {
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->vertexIndex3D_ + desc.vertices.size() <= kMaxVertices, "頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->indexIndex3D_ + desc.indices.size() <= kMaxVertices, "インデックス数が上限を超えました");
	
	auto* cmdList = DirectXCommon::GetCommandList();
	bool isLit = (instance_->currentShadingModel_ != ShadingModel::Unlit);

	// ===== PSO切り替え =====
	cmdList->SetPipelineState(instance_->SelectPSO(instance_->currentShadingModel_));

	// ===== CBVリングバッファ書き込み =====
	size_t modelMatSlotOffset = instance_->drawCallIndex_ * instance_->alignedModelMaterialSlotSize_;
	std::memcpy(instance_->modelMaterialMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(ModelMaterialCB));

	size_t matricesSlotOffset = instance_->drawCallIndex_ * instance_->alignedMatricesSlotSize_;
	std::memcpy(instance_->matricesMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraDataCB));

	// ===== 頂点・インデックス書き込み =====
	size_t vertexByteOffset = instance_->vertexIndex3D_ * sizeof(VertexData3D);
	size_t vertexDataSize = sizeof(VertexData3D) * desc.vertices.size();
	std::memcpy(instance_->vertexData3dMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	size_t indexByteOffset = instance_->indexIndex3D_ * sizeof(uint32_t);
	size_t indexDataSize = sizeof(uint32_t) * desc.indices.size();
	std::memcpy(instance_->indexData3dMappedPtr_ + indexByteOffset, desc.indices.data(), indexDataSize);

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->vertexData3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(VertexData3D);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = instance_->indexData3dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(indexDataSize);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（SetShadingModelで切り替え時に再セット済み）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Material : Slot3dNoLit::Material, instance_->modelMaterialRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Matrices : Slot3dNoLit::Matrices, instance_->matricesRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
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

	instance_->vertexIndex3D_ += desc.vertices.size();
	instance_->indexIndex3D_ += desc.indices.size();
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
	size_t modelMatSlotOffset = instance_->drawCallIndex_ * instance_->alignedModelMaterialSlotSize_;
	std::memcpy(instance_->modelMaterialMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(ModelMaterialCB));

	size_t matricesSlotOffset = instance_->drawCallIndex_ * instance_->alignedMatricesSlotSize_;
	std::memcpy(instance_->matricesMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraDataCB));

	// ===== 頂点・インデックスバッファのバインド（常駐バッファを指すだけ。memcpy無し）=====
	cmdList->IASetVertexBuffers(0, 1, &desc.vbv);
	cmdList->IASetIndexBuffer(&desc.ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== CBVバインド =====
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Material : Slot3dNoLit::Material, instance_->modelMaterialRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Matrices : Slot3dNoLit::Matrices, instance_->matricesRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
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
// インスタンス描画（同じメッシュをN体まとめて1ドロー）
//=============================================================================
void RenderContext::DrawStaticMeshInstanced(const DrawStaticMeshInstancedDesc& desc) {
	if (desc.instanceCount == 0) {
		return;
	}
	MY_ASSERT_MSG(instance_->drawCallIndex_ < kMaxDrawCalls, "描画コール数が上限を超えました");
	MY_ASSERT_MSG(instance_->instanceWriteIndex_ + desc.instanceCount <= kMaxInstances, "インスタンス数が上限を超えました");

	auto* cmdList = DirectXCommon::GetCommandList();
	bool isLit = (instance_->currentShadingModel_ != ShadingModel::Unlit);
	cmdList->SetPipelineState(instance_->SelectInstancedPSO(instance_->currentShadingModel_));

	// ===== インスタンス配列をリングへ書き込む =====
	size_t instByteOffset = instance_->instanceWriteIndex_ * sizeof(TransformationMatrix);
	std::memcpy(instance_->instanceDataMappedPtr_ + instByteOffset, desc.instances, sizeof(TransformationMatrix) * desc.instanceCount);

	// ===== マテリアル・カメラCBV（1ドロー共通）=====
	size_t modelMatSlotOffset = instance_->drawCallIndex_ * instance_->alignedModelMaterialSlotSize_;
	std::memcpy(instance_->modelMaterialMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(ModelMaterialCB));
	size_t cameraSlotOffset = instance_->drawCallIndex_ * instance_->alignedCameraDataSlotSize_;
	std::memcpy(instance_->cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraDataCB));

	// ===== 頂点・インデックスバインド =====
	cmdList->IASetVertexBuffers(0, 1, &desc.vbv);
	cmdList->IASetIndexBuffer(&desc.ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== バインド（インスタンス用ルートシグネチャのスロット）=====
	cmdList->SetGraphicsRootConstantBufferView(isLit ? SlotInstLit::Material : SlotInstNoLit::Material, instance_->modelMaterialRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	cmdList->SetGraphicsRootShaderResourceView(isLit ? SlotInstLit::Instances : SlotInstNoLit::Instances, instance_->instanceDataBuffer_->GetGPUVirtualAddress() + instByteOffset);
	if (isLit && desc.directionalLight) {
		cmdList->SetGraphicsRootConstantBufferView(SlotInstLit::Light, desc.directionalLight->GetBuffer()->GetGPUVirtualAddress());
	}
	if (isLit) {
		cmdList->SetGraphicsRootConstantBufferView(SlotInstLit::Camera, instance_->cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	// ===== 描画 =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(desc.indexCount, desc.instanceCount, 0, 0, 0);

	instance_->instanceWriteIndex_ += desc.instanceCount;
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
	MY_ASSERT_MSG(instance_->lineVertexIndex_ + vertexCount <= kMaxLineVertices, "ライン頂点数が上限を超えました");
	MY_ASSERT_MSG(instance_->lineDrawCallIndex_ < kMaxDrawCalls, "ドローコール数が上限を超えました");

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
	size_t matSlotOffset = instance_->lineDrawCallIndex_ * instance_->alignedLineMaterialSlotSize_;
	std::memcpy(instance_->lineMaterialMappedPtr_ + matSlotOffset, &desc.material, sizeof(LineMaterialCB));

	size_t matrixSlotOffset = instance_->lineDrawCallIndex_ * instance_->alignedLineMatricesSlotSize_;
	std::memcpy(instance_->lineMatricesMappedPtr_ + matrixSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = instance_->lineVertexIndex_ * sizeof(LineVertex);
	size_t vertexDataSize = sizeof(LineVertex) * vertexCount;
	std::memcpy(instance_->lineVertexMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	// ===== VBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = instance_->lineVertex3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(LineVertex);

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// スロット[0]: バインドレスSRV（上でセット済み・Line3Dでは未使用）
	// スロット[1]: LineMaterialCB(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Material, instance_->lineMaterialRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Matrices, instance_->lineMatricesRingBuffer_->GetGPUVirtualAddress() + matrixSlotOffset);

	// DrawCallとカウントを増加
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);

	instance_->lineVertexIndex_ += vertexCount;
	instance_->lineDrawCallIndex_++;
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

	Make(materialRingBuffer_, &materialMappedPtr_, alignedMaterialSlotSize_ * kMaxDrawCalls, "materialRingBuffer_");
	Make(modelMaterialRingBuffer_, &modelMaterialMappedPtr_, alignedModelMaterialSlotSize_ * kMaxDrawCalls, "modelMaterialRingBuffer_");
	Make(matricesRingBuffer_, &matricesMapperPtr_, alignedMatricesSlotSize_ * kMaxDrawCalls, "matricesRingBuffer_");
	Make(cameraDataRingBuffer_, &cameraDataMappedPtr_, alignedCameraDataSlotSize_ * kMaxDrawCalls, "cameraDataRingBuffer_");
	Make(vertexData3dRingBuffer_, &vertexData3dMappedPtr_, sizeof(VertexData3D) * kMaxVertices, "vertexData3dRingBuffer_");
	Make(vertexData2dRingBuffer_, &vertexData2dMappedPtr_, sizeof(VertexData2D) * kMaxVertices, "vertexData2dRingBuffer_");
	Make(indexData3dRingBuffer_, &indexData3dMappedPtr_, sizeof(uint32_t) * kMaxVertices, "indexData3dRingBuffer_");
	Make(indexData2dRingBuffer_, &indexData2dMappedPtr_, sizeof(uint32_t) * kMaxVertices, "indexData2dRingBuffer_");
	Make(lineVertex3dRingBuffer_, &lineVertexMappedPtr_, sizeof(LineVertex) * kMaxLineVertices, "lineVertex3dRingBuffer_");
	Make(lineMaterialRingBuffer_, &lineMaterialMappedPtr_, alignedLineMaterialSlotSize_ * kMaxDrawCalls, "lineMaterialRingBuffer_");
	Make(lineMatricesRingBuffer_, &lineMatricesMappedPtr_, alignedLineMatricesSlotSize_ * kMaxDrawCalls, "lineMatricesRingBuffer_");
	Make(instanceDataBuffer_, &instanceDataMappedPtr_, sizeof(TransformationMatrix) * kMaxInstances, "instanceDataBuffer_");
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
	    {"materialRingBuffer_",      instance_->materialRingBuffer_.Get()     },
        {"modelMaterialRingBuffer_", instance_->modelMaterialRingBuffer_.Get()},
	    {"matricesRingBuffer_",      instance_->matricesRingBuffer_.Get()     },
        {"cameraDataRingBuffer_",    instance_->cameraDataRingBuffer_.Get()   },
	    {"vertexData3dRingBuffer_",  instance_->vertexData3dRingBuffer_.Get() },
        {"vertexData2dRingBuffer_",  instance_->vertexData2dRingBuffer_.Get() },
	    {"indexData3dRingBuffer_",   instance_->indexData3dRingBuffer_.Get()  },
        {"indexData2dRingBuffer_",   instance_->indexData2dRingBuffer_.Get()  },
	    {"lineVertex3dRingBuffer_",  instance_->lineVertex3dRingBuffer_.Get() },
        {"lineMaterialRingBuffer_",  instance_->lineMaterialRingBuffer_.Get() },
	    {"lineMatricesRingBuffer_",  instance_->lineMatricesRingBuffer_.Get() },
        {"instanceDataBuffer_",      instance_->instanceDataBuffer_.Get()     },
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