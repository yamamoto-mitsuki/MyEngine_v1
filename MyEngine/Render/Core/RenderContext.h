#pragma once
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
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

	// 3D描画コール1回分の情報（PrimitiveRenderer・ModelManager共通）
	struct DrawModelDesc {
		std::vector<VertexData3D> vertices;
		std::vector<uint32_t> indices;
		ModelMaterialCB material;
		TransformationMatrix matrices;
		CameraDataCB cameraData;
		DirectionalLight* directionalLight = nullptr;
	};

	struct DrawStaticMeshDesc {
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		uint32_t indexCount = 0;
		ModelMaterialCB material;
		TransformationMatrix matrices;
		CameraDataCB cameraData;
		DirectionalLight* directionalLight = nullptr;
	};

	// インスタンス描画1回分（同じメッシュをN体まとめて描く）
	struct DrawStaticMeshInstancedDesc {
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		uint32_t indexCount = 0;
		const TransformationMatrix* instances = nullptr; // 各体のWVP/World 配列
		uint32_t instanceCount = 0;
		ModelMaterialCB material;                     // 全インスタンス共通
		CameraDataCB cameraData;                      // 共通
		DirectionalLight* directionalLight = nullptr; // 共通
	};

	// 2Dスプライト描画コール1回分の情報
	struct DrawSpriteDesc {
		VertexData2D vertices[4] = {};
		Material material;
	};

	// Line3D描画コール1回分の情報
	struct DrawLines3dDesc {
		std::vector<LineVertex> vertices;
		LineMaterialCB material;       // カメラ位置・フェード距離
		TransformationMatrix matrices; // WVP行列
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
	/// 3Dオブジェクトを描画する（PrimitiveRenderer・ModelManager共通）
	/// </summary>
	static void DrawModel(const DrawModelDesc& desc);

	/// <summary>
	/// 
	/// </summary>
	static void DrawStaticMesh(const DrawStaticMeshDesc& desc);

	/// <summary>
	/// 同じ静的メッシュをN体まとめて1ドローで描く
	/// </summary>
	static void DrawStaticMeshInstanced(const DrawStaticMeshInstancedDesc& desc);

	/// <summary>
	/// 2Dスプライトを描画する
	/// </summary>
	static void DrawSprite(const DrawSpriteDesc& desc, RenderWindow* renderWindow);

	/// <summary>
	/// 3Dラインをまとめて描画する（LINELIST）
	/// verticesは2頂点ペアで1本。奇数個を渡すと最後の頂点は無視される
	/// </summary>
	static void DrawLines3d(const DrawLines3dDesc& desc);

	static void SetViewportAndScissor(float width, float height, float offsetX = 0.0f, float offsetY = 0.0f);
	static void SetShadingModel(ShadingModel model);
	static void SetShadingModelInstanced(ShadingModel model);

private:
	RenderContext() = default;
	~RenderContext() = default;

	static RenderContext* instance_;

	void InitInternal();
	ID3D12PipelineState* SelectPSO(ShadingModel model, bool hasTexture);
	ID3D12PipelineState* SelectInstancedPSO(ShadingModel model, bool hasTexture);
	static size_t AlignTo256(size_t size);

	ShadingModel currentShadingModel_ = ShadingModel::Unlit;

	// マテリアル用リングバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> modelMaterialRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> matricesRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraDataRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexData3dRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexData2dRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexData3dRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexData2dRingBuffer_ = nullptr;
	// Line3D専用リングバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> lineVertex3dRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> lineMaterialRingBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> lineMatricesRingBuffer_ = nullptr;
	// インスタンス配列用リングバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceDataBuffer_ = nullptr;

	// CBuffer
	size_t alignedMaterialSlotSize_ = AlignTo256(sizeof(Material));
	size_t alignedMatricesSlotSize_ = AlignTo256(sizeof(TransformationMatrix));
	size_t alignedModelMaterialSlotSize_ = AlignTo256(sizeof(ModelMaterialCB));
	size_t alignedCameraDataSlotSize_ = AlignTo256(sizeof(CameraDataCB));
	size_t alignedLineMaterialSlotSize_ = AlignTo256(sizeof(LineMaterialCB));
	size_t alignedLineMatricesSlotSize_ = AlignTo256(sizeof(TransformationMatrix));

	// 永続マップポインタ
	uint8_t* materialMappedPtr_ = nullptr;
	uint8_t* modelMaterialMappedPtr_ = nullptr;
	uint8_t* matricesMapperPtr_ = nullptr;
	uint8_t* cameraDataMappedPtr_ = nullptr;
	uint8_t* vertexData3dMappedPtr_ = nullptr;
	uint8_t* vertexData2dMappedPtr_ = nullptr;
	uint8_t* indexData3dMappedPtr_ = nullptr;
	uint8_t* indexData2dMappedPtr_ = nullptr;
	// Line3D用マップポインタ
	uint8_t* lineVertexMappedPtr_ = nullptr;
	uint8_t* lineMaterialMappedPtr_ = nullptr;
	uint8_t* lineMatricesMappedPtr_ = nullptr;
	// インスタンス配列用リングバッファ
	uint8_t* instanceDataMappedPtr_ = nullptr;

	size_t drawCallIndex_ = 0;
	size_t vertexIndex3D_ = 0;
	size_t vertexIndex2D_ = 0;
	size_t indexIndex3D_ = 0;
	size_t indexIndex2D_ = 0;
	// Line3D用カウント(通常のdrawCallIndexとは別管理)
	size_t lineVertexIndex_ = 0;
	size_t lineDrawCallIndex_ = 0;
	size_t instanceWriteIndex_ = 0;
};
