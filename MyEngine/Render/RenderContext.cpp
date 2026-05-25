#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Render/PSOManager.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Utils/Const.h"
#include <cassert>
#include <format>

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

//=============================================================================
// シングルトン取得
//=============================================================================
RenderContext& RenderContext::GetInstance() {
	static RenderContext instance;
	return instance;
}

//=============================================================================
// 解放処理
//=============================================================================
void RenderContext::Release() {
	auto& ctx = GetInstance();
	ctx.materialRingBuffer_ = nullptr;
	ctx.modelMaterialRingBuffer_ = nullptr;
	ctx.matricesRingBuffer_ = nullptr;
	ctx.cameraDataRingBuffer_ = nullptr;
	ctx.vertexData3dRingBuffer_ = nullptr;
	ctx.vertexData2dRingBuffer_ = nullptr;
	ctx.indexData3dRingBuffer_ = nullptr;
	ctx.indexData2dRingBuffer_ = nullptr;
	ctx.lineVertex3dRingBuffer_ = nullptr;
	ctx.lineMaterialRingBuffer_ = nullptr;
	ctx.lineMatricesRingBuffer_ = nullptr;
	ctx.dxCommon_ = nullptr;
}

//=============================================================================
// 初期化
//=============================================================================
void RenderContext::Init(DirectXCommon* dxCommon) { GetInstance().InternalInit(dxCommon); }

//=============================================================================
// 描画カウント・頂点カウントをリセット
//=============================================================================
void RenderContext::ResetDrawCallIndex() {
	auto& ctx = GetInstance();
	ctx.drawCallIndex_ = 0;
	ctx.vertexIndex3D_ = 0;
	ctx.vertexIndex2D_ = 0;
	ctx.indexIndex3D_ = 0;
	ctx.indexIndex2D_ = 0;
	ctx.lineVertexIndex_ = 0;
	ctx.lineDrawCallIndex_ = 0;
}

//=============================================================================
// 2D描画開始処理
//=============================================================================
void RenderContext::StartDrawSprite() {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();
	// 2D用RootSignatureをセット
	cmdList->SetGraphicsRootSignature(PSOManager::GetRootSignature(BuiltinRootSig::Sprite2d));

	// SRVDescriptorHeapをセット（バインドレス：1回だけセットで全テクスチャにアクセス可能）
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);

	// バインドレスSRVをスロット0にセット（Heapの先頭から全スロットを開放）
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = ctx.dxCommon_->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(Slot2d::SRV, heapStart);
}

//=============================================================================
// 3D描画開始処理
//=============================================================================
void RenderContext::StartDrawModel() {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();

	// SRVDescriptorHeapをセット（バインドレス：1回だけセットで全テクスチャにアクセス可能）
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
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

	auto* cmdList = GetInstance().dxCommon_->GetCommandList();
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);
}

//=============================================================================
// シェーディングモデルの設定
// RootSignatureを切り替えてバインドレスSRVをセットする
//=============================================================================
void RenderContext::SetShadingModel(ShadingModel model) {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();
	ctx.currentShadingModel_ = model;

	// RootSignatureをセット
	ID3D12RootSignature* targetRootSignature = nullptr;
	switch (model) {
	case ShadingModel::Lambert:
	case ShadingModel::HalfLambert:
		targetRootSignature = PSOManager::GetRootSignature(BuiltinRootSig::Model3dLit);
		break;
	case ShadingModel::Unlit:
	default:
		targetRootSignature = PSOManager::GetRootSignature(BuiltinRootSig::Model3dNoLit);
		break;
	}
	cmdList->SetGraphicsRootSignature(targetRootSignature);

	// RootSignature切り替え後はバインドレスSRVを再セットする必要がある
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = ctx.dxCommon_->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	bool isLit = (model != ShadingModel::Unlit);
	cmdList->SetGraphicsRootDescriptorTable(isLit ? Slot3dLit::SRV : Slot3dNoLit::SRV, heapStart);
}

//=============================================================================
// ShadingModelに対応するPSOを選択する
//=============================================================================
ID3D12PipelineState* RenderContext::SelectPSO(ShadingModel model, bool hasTexture) {
	switch (model) {
	case ShadingModel::Lambert:
		return hasTexture ? PSOManager::GetPSO(BuiltinPSO::Model3dLitTex) : PSOManager::GetPSO(BuiltinPSO::Model3dLitNoTex);
	case ShadingModel::HalfLambert:
		return hasTexture ? PSOManager::GetPSO(BuiltinPSO::Model3dHalfLitTex) : PSOManager::GetPSO(BuiltinPSO::Model3dHalfLitNoTex);
	case ShadingModel::Unlit:
	default:
		return hasTexture ? PSOManager::GetPSO(BuiltinPSO::Model3dNoLitTex) : PSOManager::GetPSO(BuiltinPSO::Model3dNoLitNoTex);
	}
}

//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow) {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();

	assert(ctx.drawCallIndex_ < kMaxDrawCalls && "DrawSprite: 描画コール数が上限を超えました");
	assert(ctx.vertexIndex2D_ + 4 <= kMaxVertices && "DrawSprite: 頂点数が上限を超えました");
	assert(ctx.indexIndex2D_ + 6 <= kMaxVertices && "DrawSprite: インデックス数が上限を超えました");

	// ===== PSO切り替え =====
	bool hasTexture = (desc.material.textureIndex != 0);
	cmdList->SetPipelineState(hasTexture ? PSOManager::GetPSO(BuiltinPSO::Sprite2dTex) : PSOManager::GetPSO(BuiltinPSO::Sprite2dNoTex));

	// ===== マテリアル書き込み =====
	size_t materialSlotOffset = ctx.drawCallIndex_ * ctx.alignedMaterialSlotSize_;
	std::memcpy(ctx.materialMappedPtr_ + materialSlotOffset, &desc.material, sizeof(Material));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = ctx.vertexIndex2D_ * sizeof(VertexData2D);
	std::memcpy(ctx.vertexData2dMappedPtr_ + vertexByteOffset, desc.vertices, sizeof(VertexData2D) * 4);

	// ===== インデックス書き込み =====
	uint32_t indices[] = {0, 1, 2, 1, 3, 2};
	size_t indexByteOffset = ctx.indexIndex2D_ * sizeof(uint32_t);
	std::memcpy(ctx.indexData2dMappedPtr_ + indexByteOffset, indices, sizeof(indices));

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.vertexData2dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(sizeof(VertexData2D) * 4);
	vbv.StrideInBytes = sizeof(VertexData2D);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = ctx.indexData2dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(sizeof(indices));
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（StartDrawSpriteで1回だけセット済みなので毎回不要）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::Material, ctx.materialRingBuffer_->GetGPUVirtualAddress() + materialSlotOffset);
	// スロット[2]: ウィンドウサイズ(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(Slot2d::WindowSize, renderWindow->GetWindowSizeBuffer()->GetGPUVirtualAddress());

	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	ctx.vertexIndex2D_ += 4;
	ctx.indexIndex2D_ += 6;
	ctx.drawCallIndex_++;
}

//=============================================================================
// 3Dモデル描画（DebugRender・ModelManager共通）
// スロット番号はSlot3dLit / Slot3dNoLit名前空間を参照
//=============================================================================
void RenderContext::DrawModel(const DrawModelDesc& desc) {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();

	assert(ctx.drawCallIndex_ < kMaxDrawCalls && "DrawModel: 描画コール数が上限を超えました");
	assert(ctx.vertexIndex3D_ + desc.vertices.size() <= kMaxVertices && "DrawModel: 頂点数が上限を超えました");
	assert(ctx.indexIndex3D_ + desc.indices.size() <= kMaxVertices && "DrawModel: インデックス数が上限を超えました");

	// ===== PSO切り替え =====
	bool hasTexture = (desc.material.textureIndex != 0);
	bool isLit = (ctx.currentShadingModel_ != ShadingModel::Unlit);
	cmdList->SetPipelineState(ctx.SelectPSO(ctx.currentShadingModel_, hasTexture));

	// ===== CBVリングバッファ書き込み =====
	size_t modelMatSlotOffset = ctx.drawCallIndex_ * ctx.alignedModelMaterialSlotSize_;
	std::memcpy(ctx.modelMaterialMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(ModelMaterialCB));

	size_t matricesSlotOffset = ctx.drawCallIndex_ * ctx.alignedMatricesSlotSize_;
	std::memcpy(ctx.matricesMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	size_t cameraSlotOffset = ctx.drawCallIndex_ * ctx.alignedCameraDataSlotSize_;
	std::memcpy(ctx.cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraDataCB));

	// ===== 頂点・インデックス書き込み =====
	size_t vertexByteOffset = ctx.vertexIndex3D_ * sizeof(VertexData3D);
	size_t vertexDataSize = sizeof(VertexData3D) * desc.vertices.size();
	std::memcpy(ctx.vertexData3dMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	size_t indexByteOffset = ctx.indexIndex3D_ * sizeof(uint32_t);
	size_t indexDataSize = sizeof(uint32_t) * desc.indices.size();
	std::memcpy(ctx.indexData3dMappedPtr_ + indexByteOffset, desc.indices.data(), indexDataSize);

	// ===== VBV・IBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.vertexData3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(VertexData3D);

	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = ctx.indexData3dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(indexDataSize);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット[0]: バインドレスSRV（SetShadingModelで切り替え時に再セット済み）
	// スロット[1]: マテリアル(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Material : Slot3dNoLit::Material, ctx.modelMaterialRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(isLit ? Slot3dLit::Matrices : Slot3dNoLit::Matrices, ctx.matricesRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	// スロット[3]: DirectionalLight(b1, PS) Litのみ
	if (isLit && desc.directionalLight) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Light, desc.directionalLight->GetBuffer()->GetGPUVirtualAddress());
	}
	// スロット[4]: CameraData(b2, PS) Litのみ
	if (isLit) {
		cmdList->SetGraphicsRootConstantBufferView(Slot3dLit::Camera, ctx.cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	cmdList->DrawIndexedInstanced(static_cast<UINT>(desc.indices.size()), 1, 0, 0, 0);

	ctx.vertexIndex3D_ += desc.vertices.size();
	ctx.indexIndex3D_ += desc.indices.size();
	ctx.drawCallIndex_++;
}

//=============================================================================
// Line3D描画
// スロット番号はSlotLine3d名前空間を参照
//=============================================================================
void RenderContext::DrawLines3d(const DrawLines3dDesc& desc) {
	auto& ctx = GetInstance();
	auto* cmdList = ctx.dxCommon_->GetCommandList();

	if (desc.vertices.empty())
		return;
	// LINELISTは2頂点で1本のため奇数個の場合は最後を切り捨て
	size_t vertexCount = desc.vertices.size() & ~size_t(1);
	if (vertexCount == 0)
		return;

	assert(ctx.lineVertexIndex_ + vertexCount <= kMaxLineVertices && "DrawLines3d: ライン頂点数が上限を超えました");
	assert(ctx.lineDrawCallIndex_ < kMaxDrawCalls && "DrawLines3d: ドローコール数が上限を超えました");

	// ===== PSO・RootSignatureをセット =====
	cmdList->SetPipelineState(PSOManager::GetPSO(BuiltinPSO::Line3d));
	cmdList->SetGraphicsRootSignature(PSOManager::GetRootSignature(BuiltinRootSig::Line3d));

	// RootSignature切り替え後はバインドレスSRVを再セットする
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart = ctx.dxCommon_->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(SlotLine3d::SRV, heapStart);

	// ===== CBVリングバッファ書き込み =====
	size_t matSlotOffset = ctx.lineDrawCallIndex_ * ctx.alignedLineMaterialSlotSize_;
	std::memcpy(ctx.lineMaterialMappedPtr_ + matSlotOffset, &desc.material, sizeof(LineMaterialCB));

	size_t matrixSlotOffset = ctx.lineDrawCallIndex_ * ctx.alignedLineMatricesSlotSize_;
	std::memcpy(ctx.lineMatricesMappedPtr_ + matrixSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = ctx.lineVertexIndex_ * sizeof(LineVertex);
	size_t vertexDataSize = sizeof(LineVertex) * vertexCount;
	std::memcpy(ctx.lineVertexMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	// ===== VBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.lineVertex3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(LineVertex);

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// スロット[0]: バインドレスSRV（上でセット済み・Line3Dでは未使用）
	// スロット[1]: LineMaterialCB(b0, PS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Material, ctx.lineMaterialRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// スロット[2]: 行列(b0, VS)
	cmdList->SetGraphicsRootConstantBufferView(SlotLine3d::Matrices, ctx.lineMatricesRingBuffer_->GetGPUVirtualAddress() + matrixSlotOffset);

	cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);

	ctx.lineVertexIndex_ += vertexCount;
	ctx.lineDrawCallIndex_++;
}

//=============================================================================
// 初期化（内部）
//=============================================================================
void RenderContext::InternalInit(DirectXCommon* dxCommon) {
	// dxCommonは最初の1回だけセット
	if (!dxCommon_) {
		dxCommon_ = dxCommon;
		LogManager::Log("[RenderContext] dxCommon_ セット完了");
	}

	// リングバッファをまとめて生成するためのラムダ
	auto Make = [&](auto& buf, auto** ptr, size_t size, const char* name) {
		if (!buf) {
			buf = dxCommon_->CreateBufferResource(size);
			buf->Map(0, nullptr, reinterpret_cast<void**>(ptr));
			LogManager::Log(std::string("[RenderContext] ") + name + " 生成完了");
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

	LogManager::Log("[RenderContext] 初期化完了");
}

size_t RenderContext::AlignTo256(size_t size) { return (size + 255) & ~255; }