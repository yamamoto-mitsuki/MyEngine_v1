#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include <d3d12.h>
#include <wrl/client.h>

#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Transform.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"

// 前方宣言
class RenderContext;
class Camera;
class DirectionalLight;


/// <summary>
/// モデルを読みこみ、番号で管理する。描画リクエストも送る。
/// </summary>
class ModelManager {
public:
	// ===== MTLファイルから読み込んだマテリアル情報 =====
	struct MtlMaterial {
		std::string name;
		Vector3 ambient = {0.0f, 0.0f, 0.0f};  // Ka: 環境光色
		Vector3 diffuse = {0.8f, 0.8f, 0.8f};  // Kd: 拡散反射色
		Vector3 specular = {1.0f, 1.0f, 1.0f}; // Ks: 鏡面反射色
		Vector3 emissive = {0.0f, 0.0f, 0.0f}; // Ke: 自己発光色
		float shininess = 32.0f;               // Ns: 鏡面反射指数
		float dissolve = 1.0f;                 // d: 不透明度（1=完全不透明）
		float metallic = 0.0f;                 // 0=非金属 1=金属
		float roughness = 0.5f;                // 表面の粗さ（0=鏡面, 1=完全拡散）
		int32_t illum = 1;                     // illum: 照明モデル番号
		std::string textureFilePath;           // map_Kd: ディフューズテクスチャ
		std::string ambientTexFilePath;        // map_Ka: アンビエントテクスチャ
		std::string specularTexFilePath;       // map_Ks: スペキュラテクスチャ
		std::string bumpTexFilePath;           // map_bump/bump: バンプマップ
		uint32_t srvIndex = 0;                 // TextureManager::Load()が返したSRVスロット番号
	};

	// ===== メッシュデータ（1つのusemtlに対応）=====
	struct SubMesh {
		// CPU側で使う。ロード時のバッファ構築に使用
		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
		std::string materialName; // materialMapのキー
		// GPU側で使う。描画時にバインド
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		D3D12_INDEX_BUFFER_VIEW ibv{};
		uint32_t indexCount = 0;
	};

	// ===== モデルデータ（複数メッシュ・マテリアル）=====
	struct ModelAsset {
		std::vector<SubMesh> meshes;
		std::map<std::string, MtlMaterial> materialMap;
	};

	/// <summary>
	/// 初期化。TextureManager::Init()の後に呼ぶ
	/// </summary>
	static void Initialize();

	/// <summary>
	/// 全モデルの解放。WinMainのreturn 0の前に呼ぶ
	/// </summary>
	static void Release();

	/// <summary>
	/// OBJファイルを読み込む。同じパスを渡すとキャッシュから返す
	/// </summary>
	/// <param name="objFilePath">OBJファイルのパス（例: "resources/cube/cube.obj"）</param>
	/// <returns>描画時に使うモデルハンドル</returns>
	static uint32_t Load(const std::string& objFilePath);

	/// <summary>
	/// モデルハンドルから
	/// </summary>
	/// <param name="modelHandle">Loadで呼んだモデルハンドル</param>
	/// <returns></returns>
	static const ModelAsset* GetModelAsset(uint32_t modelHandle);

	/// <summary>
	/// ModelAsset構造体の中のmaterialMapを取得
	/// </summary>
	/// <param name="handle">objファイルを読んだときに返ってくるmodelHandle</param>
	/// <param name="name">SubMeshの中にあるmaterialName</param>
	/// <returns></returns>
	static MtlMaterial* GetMtlMaterial(uint32_t handle, const std::string name);

private:
	static ModelManager& GetInstance();
	// コピー・ムーブ禁止
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;
	ModelManager() = default;

	/// <summary>
	/// OBJファイルを読み込む
	/// </summary>
	static ModelAsset LoadObjFile(const std::string& directoryPath, const std::string& filename);

	/// <summary>
	/// メッシュのCPU頂点をGPU常駐バッファへ転送する（ロード時1回のみ）
	/// </summary>
	static void MakeMeshBuffer(SubMesh& mesh, const std::string& modelName);

	std::unordered_map<uint32_t, ModelAsset> models_;        // キー → ModelAsset
	std::unordered_map<std::string, uint32_t> pathToHandle_; // ファイルパス → キー（重複防止）
	uint32_t modelsKey_ = 1;
};