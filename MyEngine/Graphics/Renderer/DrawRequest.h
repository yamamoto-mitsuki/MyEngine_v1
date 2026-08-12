#pragma once
#include <array>
#include <vector>

#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"


// ===== 3Dメッシュ =====
struct MeshRequest {
	// --- ソートキーの材料になるもの ---
	ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
	BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
	RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
	DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
	float cameraDistanceSq = 0.0f;                             // カメラとの距離
	uint64_t sortKey;                                          // ソートキー
	
	// --- ジオメトリ ---
	// 動的（Primitive系）: vertices, indicesを使用。DrawMeshがリングバッファに書く
	std::vector<Vertex3dData> vertices; // ピクセルを結ぶ頂点
	std::vector<uint32_t> indices;      // 頂点インデックスの格納場所
	// 静的（Model）: GPU常駐バッファの場所、個数だけ運ぶ。DrawMeshはバインドのみ
	D3D12_VERTEX_BUFFER_VIEW vbv{};     // VertexBufferView
	D3D12_INDEX_BUFFER_VIEW ibv{};      // IndexBufferView
	uint32_t indexCount = 0;            // DrawIndexedInstancedに渡す個数（静的用。動的はindices.size()）
	bool isStatic = false;              // DrawMeshの経路分岐スイッチ

	// --- ShaderとのBind情報として使う構造体 ---
	ObjectTransformData objectTransformData;
	Material3dData materialData;
	CameraData cameraData;
	DirectionalLightData directionalLightData;
	PointLightListData pointLightListData;
	D3D12_GPU_VIRTUAL_ADDRESS iblParamsAddress = 0; 

	// --- その他 ---
	bool isBillboard = false; // ビルボード（Shaderで計算）
	std::wstring windowTitle; // ウィンドウ名識別時に使用
	const std::string* debugName = nullptr;    // PIX表示用（ModelAssetの文字列を指す。所有しない）
	const std::string* debugSubName = nullptr; // サブメッシュのマテリアル名
};


// ===== パーティクル =====
struct ParticleRequest {
	std::vector<ParticleData> instances; // ビルボード計算済みのインスタンス配列
	MaterialParticleData materialData;
	BlendMode blendMode = BlendMode::Add;
	uint64_t sortKey = 0;
	std::wstring windowTitle = L"";
};


// ===== 2Dスプライト =====
struct SpriteRequest {
	// ウィンドウ名識別時に使用
	std::wstring windowTitle;

	// --- ジオメトリ ---
	std::array<Vertex2dData, 4> vertices;

	// --- Shaderとのバインド情報 ---
	Material2dData materialData;

	// --- 描画設定 ---
	BlendMode blendMode = BlendMode::Normal;
	RasterizerType rasterizerType = RasterizerType::SolidBack;
};


// ===== Line =====
struct LineRequest {
	// ウィンドウ名識別時に使用
	std::wstring windowTitle;

	// --- ジオメトリ ---
	std::vector<VertexLineData> vertices;

	// --- Shaderとのバインド情報 ---
	ObjectTransformData objectTransformData;
	MaterialLineData materialData;
};