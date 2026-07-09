#pragma once
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
#include "MyEngine/Render/Core/DirectXCommon.h"
#include "MyEngine/Math/Transform.h"
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class PrimitiveRenderer;
class RenderWindow;

class RenderContext {
public:
	static constexpr size_t kMaxVertices = 100000; 
	static constexpr size_t kMaxLineVertices = 65536;
	static constexpr size_t kMaxInstances = 8192;
	static constexpr size_t kMaxDrawCalls = 256;

	// 3D描画コール1回分の情報（PrimitiveRenderer）
	struct DrawModelDesc {
		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
	    Material3dData material;
		TransformationMatrixData matrices;
		CameraData cameraData;
		DirectionalLight* directionalLight = nullptr;
	};

	// モデル描画のMesh1回分の情報
	struct DrawStaticMeshDesc {
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		uint32_t indexCount = 0;
		Material3dData material;
		TransformationMatrixData matrices;
		CameraData cameraData;
		DirectionalLight* directionalLight = nullptr;
		BlendMode blendMode = BlendMode::Normal;
	};

	// 2Dスプライト描画コール1回分の情報
	struct DrawSpriteDesc {
		Vertex2dData vertices[4] = {};
		Material2dData material;
	};

	// Line3D描画コール1回分の情報
	struct DrawLines3dDesc {
		std::vector<VertexLineData> vertices;
		MaterialLineData material;         // カメラ位置・フェード距離
		TransformationMatrixData matrices; // WVP行列
	};

public:
	RenderContext(const RenderContext&) = delete;
	RenderContext& operator=(const RenderContext&) = delete;
	RenderContext(const RenderContext&&) = delete;
	RenderContext& operator=(const RenderContext&&) = delete;
	
	
	static void Initialize();
	static void Release();
	static void ResetDrawCallIndex();
	static void StartDrawSprite();
	static void StartDrawModel();

	/// <summary>
	/// 3Dオブジェクトを描画する（Primitive Object）
	/// </summary>
	static void DrawModel(const DrawModelDesc& desc);

	/// <summary>
	/// Model Objectを描画
	/// </summary>
	static void DrawStaticMesh(const DrawStaticMeshDesc& desc);

	/// <summary>
	/// 2Dスプライトを描画する
	/// </summary>
	static void DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow);

	/// <summary>
	/// 3Dラインをまとめて描画する（LINELIST）
	/// verticesは2頂点ペアで1本。奇数個を渡すと最後の頂点は無視される
	/// </summary>
	static void DrawLines3d(const DrawLines3dDesc& desc);

	/// <summary>
	///  GPUページフォルトのアドレスをどのリソースが原因かログに出力する
	/// </summary>
	static void LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA);

	static void SetViewportAndScissor(float width, float height, float offsetX = 0.0f, float offsetY = 0.0f);
	static void SetShadingModel(ShadingModel model);
	static void SetShadingModelInstanced(ShadingModel model);

private:
	RenderContext() = default;
	~RenderContext() = default;

	static RenderContext* instance_;

	void InitInternal();
	ID3D12PipelineState* SelectPSO(ShadingModel model, BlendMode blendMode = BlendMode::Normal);
	ID3D12PipelineState* SelectInstancedPSO(ShadingModel model);
	static size_t AlignTo256(size_t size);

	ShadingModel currentShadingModel_ = ShadingModel::Unlit;

	// --- リングバッファ --
	// 共通
	Microsoft::WRL::ComPtr<ID3D12Resource> matricesDataRingBuffer_ = nullptr; // WVP, WorldMatrix
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraDataRingBuffer_ = nullptr;   // Camera
	// 頂点データ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertex2dDataRingBuffer_ = nullptr;   // 2D
	Microsoft::WRL::ComPtr<ID3D12Resource> vertex3dDataRingBuffer_ = nullptr;   // 3D
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexLineDataRingBuffer_ = nullptr; // Line
	// インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> index2dDataRingBuffer_ = nullptr;    // 2D
	Microsoft::WRL::ComPtr<ID3D12Resource> index3dDataRingBuffer_ = nullptr;    // 3D
	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> material2dDataRingBuffer_ = nullptr;   // 2D
	Microsoft::WRL::ComPtr<ID3D12Resource> material3dDataRingBuffer_ = nullptr;   // 3D
	Microsoft::WRL::ComPtr<ID3D12Resource> materialLineDataRingBuffer_ = nullptr; // Line

	// --- 永続マップポインタ ---
	// 共通
	uint8_t* matricesDataMapperPtr_ = nullptr; // Matrices
	uint8_t* cameraDataMappedPtr_ = nullptr;   // Camera
	// 頂点データ
	uint8_t* vertex2dDataMappedPtr_ = nullptr;   // 2D
	uint8_t* vertex3dDataMappedPtr_ = nullptr;   // 3D
	uint8_t* vertexLineDataMappedPtr_ = nullptr; // Line
	// インデックス
	uint8_t* index2dDataMappedPtr_ = nullptr; // 2D
	uint8_t* index3dDataMappedPtr_ = nullptr; // 3D
	// マテリアル
	uint8_t* material2dDataMappedPtr_ = nullptr;
	uint8_t* material3dDataMappedPtr_ = nullptr;
	uint8_t* materialLineDataMappedPtr_ = nullptr;
	
	// --- CBufferの大きさ ---
	// 共通
	size_t alignedMatricesDataSlotSize_ = AlignTo256(sizeof(TransformationMatrixData));
	size_t alignedCameraDataSlotSize_ = AlignTo256(sizeof(CameraData));
	// マテリアル
	size_t alignedMaterial2dDataSlotSize_ = AlignTo256(sizeof(Material2dData));
	size_t alignedMaterial3dDataSlotSize_ = AlignTo256(sizeof(Material3dData));
	size_t alignedMaterialLineDataSlotSize_ = AlignTo256(sizeof(MaterialLineData));

	// --- カウント用変数 ---
	// Draw Call
	size_t drawCallIndex_ = 0;
	size_t drawCallLineIndex_ = 0;
	// 頂点
	size_t vertex3dIndex_ = 0;
	size_t vertex2dIndex_ = 0;
	size_t vertexLineIndex_ = 0;
	// インデックス
	size_t index3dIndex_ = 0;
	size_t index2dIndex_ = 0;
};
