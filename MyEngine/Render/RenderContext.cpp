#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Utils/Const.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Render/TextureManager.h"
#include "MyEngine/Math/Vector4.h"
#include <cassert>
#include <format>

// ===== インスタンスを取得 =====
RenderContext& RenderContext::GetInstance() {
	static RenderContext instance;
	return instance;
}

// ===== 解放処理 =====
void RenderContext::Release() {
	// シングルトンはプログラム終了まで存在するため、dxCommonより先にGPUリソースを解放する
	GetInstance().materialRingBuffer_ = nullptr;
	GetInstance().modelMaterialRingBuffer_ = nullptr;
	GetInstance().matricesRingBuffer_ = nullptr;
	GetInstance().cameraDataRingBuffer_ = nullptr;
	GetInstance().vertexData3dRingBuffer_ = nullptr;
	GetInstance().vertexData2dRingBuffer_ = nullptr;
	GetInstance().indexData3dRingBuffer_ = nullptr;
	GetInstance().indexData2dRingBuffer_ = nullptr;
	GetInstance().lineVertex3dRingBuffer_ = nullptr;
	GetInstance().lineMaterialRingBuffer_ = nullptr;
	GetInstance().lineMatricesRingBuffer_ = nullptr;
	GetInstance().dxCommon_ = nullptr;
}

// ===== 初期化 =====
void RenderContext::Init(DirectXCommon* dxCommon) { GetInstance().InternalInit(dxCommon); }

// ===== 描画カウント・頂点カウントをリセット =====
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

// ===== 2D描画開始処理 =====
void RenderContext::StartDrawSprite() {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();
	// 2D用RootSignatureをセット
	commandList->SetGraphicsRootSignature(ctx.dxCommon_->GetRootSignature2d());
	// SRVDescriptorHeapをバインド
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	commandList->SetDescriptorHeaps(1, heaps);
}

// ===== 3D描画開始処理 =====
void RenderContext::StartDrawModel() {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	commandList->SetDescriptorHeaps(1, heaps);
}

// ===== ビューポートとシザーの設定 =====
void RenderContext::SetViewportAndScissor(float width, float height, float offsetX, float offsetY) {
	// ビューポート
	D3D12_VIEWPORT viewport{};
	viewport.Width = width;
	viewport.Height = height;
	viewport.TopLeftX = offsetX;
	viewport.TopLeftY = offsetY;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	// シザー
	D3D12_RECT scissor{};
	scissor.left = static_cast<LONG>(offsetX);
	scissor.top = static_cast<LONG>(offsetY);
	scissor.right = static_cast<LONG>(offsetX + width);
	scissor.bottom = static_cast<LONG>(offsetY + height);

	auto* commandList = GetInstance().dxCommon_->GetCommandList();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
}

// ===== シェーディングの設定 =====
void RenderContext::SetShadingModel(ShadingModel model) {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();
	ctx.currentShadingModel_ = model;

	ID3D12RootSignature* targetRootSignature = nullptr;
	switch (model) {
	// Lightingあり
	case ShadingModel::Lambert:
	case ShadingModel::HalfLambert:
		targetRootSignature = ctx.dxCommon_->GetRootSignature3dLit();
		break;
	// Lightingなし
	case ShadingModel::Unlit:
	default:
		targetRootSignature = ctx.dxCommon_->GetRootSignature3dNoLit();
		break;
	}
	commandList->SetGraphicsRootSignature(targetRootSignature);
}

//=============================
// 描画時にPSO設定
//=============================
ID3D12PipelineState* RenderContext::SelectPSO(ShadingModel model, bool hasTexture) {
	switch (model) {
	case ShadingModel::Lambert:
		return hasTexture ? dxCommon_->GetPSO3dLitTex() : dxCommon_->GetPSO3dLitNoTex();
	case ShadingModel::HalfLambert:
		return hasTexture ? dxCommon_->GetPSO3dHalfLitTex() : dxCommon_->GetPSO3dHalfLitNoTex();
	case ShadingModel::Unlit:
	default:
		return hasTexture ? dxCommon_->GetPSO3dNoLitTex() : dxCommon_->GetPSO3dNoLitNoTex();
	}
}

//=============================
// 2Dスプライト描画
//=============================
void RenderContext::DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow) {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();

	assert(ctx.drawCallIndex_ < kMaxDrawCalls && "DrawSprite: 描画コール数が上限を超えました");
	assert(ctx.vertexIndex2D_ + 4 <= kMaxVertices && "DrawSprite: 頂点数が上限を超えました");
	assert(ctx.indexIndex2D_ + 6 <= kMaxVertices && "DrawSprite: インデックス数が上限を超えました");

	// ===== PSO切り替え =====
	bool hasTexture = (desc.srvIndex != 0);
	commandList->SetPipelineState(hasTexture ? ctx.dxCommon_->GetPSO2dTex() : ctx.dxCommon_->GetPSO2dNoTex());

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

	// ===== VBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.vertexData2dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(sizeof(VertexData2D) * 4);
	vbv.StrideInBytes = sizeof(VertexData2D);

	// ===== IBV設定 =====
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = ctx.indexData2dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(sizeof(indices));
	ibv.Format = DXGI_FORMAT_R32_UINT;

	// ===== SRVDescriptorHeapをバインド =====
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	commandList->SetDescriptorHeaps(1, heaps);

	commandList->IASetVertexBuffers(0, 1, &vbv);
	commandList->IASetIndexBuffer(&ibv);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// スロット0: マテリアル(b0, PS)
	commandList->SetGraphicsRootConstantBufferView(0, ctx.materialRingBuffer_->GetGPUVirtualAddress() + materialSlotOffset);
	// スロット1: テクスチャ(t0, PS)
	if (hasTexture) {
		const TextureManager::TextureData* texData = TextureManager::GetTextureData(desc.srvIndex);
		if (texData) {
			commandList->SetGraphicsRootDescriptorTable(1, texData->srvHandleGPU);
		}
	}
	// スロット2: ウィンドウサイズ(b1, VS)
	commandList->SetGraphicsRootConstantBufferView(2, renderWindow->GetWindowSizeBuffer()->GetGPUVirtualAddress());
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	ctx.vertexIndex2D_ += 4;
	ctx.indexIndex2D_ += 6;
	ctx.drawCallIndex_++;
}

//=============================
// モデル描画
//=============================
void RenderContext::DrawModel(const DrawModelDesc& desc) {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();

	assert(ctx.drawCallIndex_ < kMaxDrawCalls && "DrawModelMesh: 描画コール数が上限を超えました");
	assert(ctx.vertexIndex3D_ + desc.vertices.size() <= kMaxVertices && "DrawModelMesh: 頂点数が上限を超えました");
	assert(ctx.indexIndex3D_ + desc.indices.size() <= kMaxVertices && "DrawModelMesh: インデックス数が上限を超えました");

	// ===== PSO切り替え（モデル用PSO）=====
	bool hasTexture = (desc.srvIndex != 0);
	bool isLit = (ctx.currentShadingModel_ != ShadingModel::Unlit);
	commandList->SetPipelineState(ctx.SelectPSO(ctx.currentShadingModel_, hasTexture));

	// ===== CBVリングバッファ書き込み =====
	// modelMaterialRingBuffer_ 
	size_t modelMatSlotOffset = ctx.drawCallIndex_ * ctx.alignedModelMaterialSlotSize_;
	std::memcpy(ctx.modelMaterialMappedPtr_ + modelMatSlotOffset, &desc.material, sizeof(ModelMaterialCB));
	// TransformationMatrix
	size_t matricesSlotOffset = ctx.drawCallIndex_ * ctx.alignedMatricesSlotSize_;
	std::memcpy(ctx.matricesMapperPtr_ + matricesSlotOffset, &desc.matrices, sizeof(TransformationMatrix));
	// Camera
	size_t cameraSlotOffset = ctx.drawCallIndex_ * ctx.alignedCameraDataSlotSize_;
	std::memcpy(ctx.cameraDataMappedPtr_ + cameraSlotOffset, &desc.cameraData, sizeof(CameraDataCB));

	// ===== 頂点書き込み =====
	size_t vertexByteOffset = ctx.vertexIndex3D_ * sizeof(VertexData3D);
	size_t vertexDataSize = sizeof(VertexData3D) * desc.vertices.size();
	std::memcpy(ctx.vertexData3dMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	// ===== インデックス書き込み =====
	size_t indexByteOffset = ctx.indexIndex3D_ * sizeof(uint32_t);
	size_t indexDataSize = sizeof(uint32_t) * desc.indices.size();
	std::memcpy(ctx.indexData3dMappedPtr_ + indexByteOffset, desc.indices.data(), indexDataSize);

	// ===== VBV設定 =====
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.vertexData3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(VertexData3D);

	// ===== IBV設定 =====
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = ctx.indexData3dRingBuffer_->GetGPUVirtualAddress() + indexByteOffset;
	ibv.SizeInBytes = static_cast<UINT>(indexDataSize);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	// ===== SRVDescriptorHeapをバインド =====
	ID3D12DescriptorHeap* heaps[] = {ctx.dxCommon_->GetSRVDescriptorHeap()};
	commandList->SetDescriptorHeaps(1, heaps);

	commandList->IASetVertexBuffers(0, 1, &vbv);
	commandList->IASetIndexBuffer(&ibv);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// スロット0: モデルマテリアル(b0, PS)
	commandList->SetGraphicsRootConstantBufferView(0, ctx.modelMaterialRingBuffer_->GetGPUVirtualAddress() + modelMatSlotOffset);
	// スロット1: TransformationMatrix(b0, VS)
	commandList->SetGraphicsRootConstantBufferView(1, ctx.matricesRingBuffer_->GetGPUVirtualAddress() + matricesSlotOffset);
	// スロット2: テクスチャ(t0, PS)
	if (hasTexture) {
		const TextureManager::TextureData* texData = TextureManager::GetTextureData(desc.srvIndex);
		if (texData) {
			commandList->SetGraphicsRootDescriptorTable(2, texData->srvHandleGPU);
		}
	}
	// スロット3: DirectionalLight(b1, PS) Litのみ
	if (isLit && desc.directionalLight) {
		commandList->SetGraphicsRootConstantBufferView(3, desc.directionalLight->GetBuffer()->GetGPUVirtualAddress());
	}
	// スロット4: CameraData(b2, PS) Litのみ
	if (isLit) {
		commandList->SetGraphicsRootConstantBufferView(4, ctx.cameraDataRingBuffer_->GetGPUVirtualAddress() + cameraSlotOffset);
	}

	commandList->DrawIndexedInstanced(static_cast<UINT>(desc.indices.size()), 1, 0, 0, 0);

	ctx.vertexIndex3D_ += desc.vertices.size();
	ctx.indexIndex3D_ += desc.indices.size();
	ctx.drawCallIndex_++;
}

//=============================
// Line3D描画
//=============================
void RenderContext::DrawLines3d(const DrawLines3dDesc& desc) {
	auto& ctx = GetInstance();
	auto* commandList = ctx.dxCommon_->GetCommandList();

	if (desc.vertices.empty()) return;
	// 奇数個の場合は最後を切り捨て(LINELISTは2頂点で1本)
	size_t vertexCount = desc.vertices.size() & ~size_t(1);
	if (vertexCount == 0) return;

	assert(ctx.lineVertexIndex_ + vertexCount <= kMaxLineVertices && "DrawLines3d: ライン頂点数が上限を超えました");
	assert(ctx.lineDrawCallIndex_ < kMaxDrawCalls && "DrawLines3d: ドローコール数が上限を超えました");

	commandList->SetPipelineState(ctx.dxCommon_->GetPSO3dLine());
	commandList->SetGraphicsRootSignature(ctx.dxCommon_->GetRootSignature3dLine());

	size_t matSlotOffset = ctx.lineDrawCallIndex_ * ctx.alignedLineMaterialSlotSize_;
	std::memcpy(ctx.lineMaterialMappedPtr_ + matSlotOffset, &desc.material, sizeof(LineMaterialCB));

	size_t matrixSlotOffset = ctx.lineDrawCallIndex_ * ctx.alignedLineMatricesSlotSize_;
	std::memcpy(ctx.lineMatricesMappedPtr_ + matrixSlotOffset, &desc.matrices, sizeof(TransformationMatrix));

	size_t vertexByteOffset = ctx.lineVertexIndex_ * sizeof(LineVertex);
	size_t vertexDataSize = sizeof(LineVertex) * vertexCount;
	std::memcpy(ctx.lineVertexMappedPtr_ + vertexByteOffset, desc.vertices.data(), vertexDataSize);

	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = ctx.lineVertex3dRingBuffer_->GetGPUVirtualAddress() + vertexByteOffset;
	vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
	vbv.StrideInBytes = sizeof(LineVertex);

	commandList->IASetVertexBuffers(0, 1, &vbv);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	// スロット0: LineMaterialCB(b0, PS)
	commandList->SetGraphicsRootConstantBufferView(0, ctx.lineMaterialRingBuffer_->GetGPUVirtualAddress() + matSlotOffset);
	// スロット1: TransformationMatrix(b0, VS)
	commandList->SetGraphicsRootConstantBufferView(1, ctx.lineMatricesRingBuffer_->GetGPUVirtualAddress() + matrixSlotOffset);

	commandList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);

	ctx.lineVertexIndex_ += vertexCount;
	ctx.lineDrawCallIndex_++;
}

//=============================
// 初期化（内部）
//=============================
void RenderContext::InternalInit(DirectXCommon* dxCommon) {
	// dxCommonは最初の1回だけセット
	if (!dxCommon_) {
		dxCommon_ = dxCommon;
		LogManager::Log("[RenderContext] dxCommon_ セット完了");
	}

	if (!materialRingBuffer_) {
		materialRingBuffer_ = dxCommon_->CreateBufferResource(alignedMaterialSlotSize_ * kMaxDrawCalls);
		materialRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&materialMappedPtr_));
		LogManager::Log("[RenderContext] materialRingBuffer_ 生成完了");
	}
	if (!modelMaterialRingBuffer_) {
		modelMaterialRingBuffer_ = dxCommon_->CreateBufferResource(alignedModelMaterialSlotSize_ * kMaxDrawCalls);
		modelMaterialRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&modelMaterialMappedPtr_));
		LogManager::Log("[RenderContext] modelMaterialRingBuffer_ 生成完了");
	}
	if (!matricesRingBuffer_) {
		matricesRingBuffer_ = dxCommon_->CreateBufferResource(alignedMatricesSlotSize_ * kMaxDrawCalls);
		matricesRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&matricesMapperPtr_));
		LogManager::Log("[RenderContext] matricesRingBuffer_ 生成完了");
	}
	if (!cameraDataRingBuffer_) {
		cameraDataRingBuffer_ = dxCommon_->CreateBufferResource(alignedCameraDataSlotSize_ * kMaxDrawCalls);
		cameraDataRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cameraDataMappedPtr_));
		LogManager::Log("[RenderContext] cameraDataRingBuffer_ 生成完了");
	}
	if (!vertexData3dRingBuffer_) {
		vertexData3dRingBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData3D) * kMaxVertices);
		vertexData3dRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData3dMappedPtr_));
		LogManager::Log("[RenderContext] vertexData3dRingBuffer_ 生成完了");
	}
	if (!vertexData2dRingBuffer_) {
		vertexData2dRingBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData2D) * kMaxVertices);
		vertexData2dRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData2dMappedPtr_));
		LogManager::Log("[RenderContext] vertexData2dRingBuffer_ 生成完了");
	}
	if (!indexData3dRingBuffer_) {
		indexData3dRingBuffer_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * kMaxVertices);
		indexData3dRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData3dMappedPtr_));
		LogManager::Log("[RenderContext] indexData3dRingBuffer_ 生成完了");
	}
	if (!indexData2dRingBuffer_) {
		indexData2dRingBuffer_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * kMaxVertices);
		indexData2dRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData2dMappedPtr_));
		LogManager::Log("[RenderContext] indexData2dRingBuffer_ 生成完了");
	}
	if (!lineVertex3dRingBuffer_) {
		lineVertex3dRingBuffer_ = dxCommon_->CreateBufferResource(sizeof(LineVertex) * kMaxLineVertices);
		lineVertex3dRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lineVertexMappedPtr_));
		LogManager::Log("[RenderContext] lineVertex3dRingBuffer_ 生成完了");
	}
	if (!lineMaterialRingBuffer_) {
		lineMaterialRingBuffer_ = dxCommon_->CreateBufferResource(alignedLineMaterialSlotSize_ * kMaxDrawCalls);
		lineMaterialRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lineMaterialMappedPtr_));
		LogManager::Log("[RenderContext] lineMaterialRingBuffer_ 生成完了");
	}
	if (!lineMatricesRingBuffer_) {
		lineMatricesRingBuffer_ = dxCommon_->CreateBufferResource(alignedLineMatricesSlotSize_ * kMaxDrawCalls);
		lineMatricesRingBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lineMatricesMappedPtr_));
		LogManager::Log("[RenderContext] lineMatricesRingBuffer_ 生成完了");
	}

	LogManager::Log("[RenderContext] 初期化完了");
}

size_t RenderContext::AlignTo256(size_t size) { return (size + 255) & ~255; }