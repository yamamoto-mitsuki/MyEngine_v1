#pragma once
#include <array>
#include <vector>

#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"


// ===== 3Dメッシュ =====
struct MeshRequest {
public:
	// --- ソートキーの材料になるもの ---
	ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
	BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
	RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
	DepthMode depthMode = DepthMode::TestWrite;                // 深度設定

	// ウィンドウ名識別時に使用
	std::wstring windowTitle;

private:
	// --- ジオメトリ ---
	std::vector<Vertex3dData> vertices; // ピクセルを結ぶ頂点
	std::vector<uint32_t> indices;      // 頂点インデックスの格納場所
	D3D12_VERTEX_BUFFER_VIEW vbv{};     // VertexBufferView
	D3D12_INDEX_BUFFER_VIEW ibv{};      // IndexBufferView

	// --- ShaderとのBind情報として使う構造体 ---
	TransformationMatrixData transformationMatricesData;
	Material3dData materialData;
	CameraData cameraData;
	DirectionalLightData* lightData;
};

// ===== 2Dスプライト =====
struct SpriteRequest {
	// ウィンドウ名識別時に使用
	std::wstring windowTitle;

	// --- ジオメトリ ---
	std::array<Vertex2dData, 4> vertices;

	// --- Shaderとのバインド情報 ---
	Material2dData materialData;
};

// ===== Line =====
struct LineRequest {
	// ウィンドウ名識別時に使用
	std::wstring windowTitle;

	// --- ジオメトリ ---
	std::vector<VertexLineData> vertices;

	// --- Shaderとのバインド情報 ---
	MaterialLineData materialData;

};