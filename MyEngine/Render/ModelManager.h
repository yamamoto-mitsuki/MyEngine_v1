#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
#include "MyEngine/Math/Transform.h"

// 前方宣言
class RenderContext;
class Camera;
class DirectionalLight;
class DirectXCommon;

class ModelManager {
public:
	// ===== CPU側マテリアルデータ（MTLファイルから読み込んだ情報）=====
	struct MaterialData {
		std::string name;
		Vector3 ambient = {0.2f, 0.2f, 0.2f};  // Ka: 環境光色
		Vector3 diffuse = {0.8f, 0.8f, 0.8f};  // Kd: 拡散反射色
		Vector3 specular = {0.0f, 0.0f, 0.0f}; // Ks: 鏡面反射色
		Vector3 emissive = {0.0f, 0.0f, 0.0f}; // Ke: 自己発光色
		float shininess = 32.0f;               // Ns: 鏡面反射指数
		float dissolve = 1.0f;                 // d: 不透明度（1=完全不透明）
		int32_t illum = 1;                     // illum: 照明モデル番号
		std::string textureFilePath;           // map_Kd: ディフューズテクスチャ
		std::string ambientTexFilePath;        // map_Ka: アンビエントテクスチャ
		std::string specularTexFilePath;       // map_Ks: スペキュラテクスチャ
		std::string bumpTexFilePath;           // map_bump/bump: バンプマップ
		uint32_t srvIndex = 0;                 // TextureManager::Load()が返したSRVスロット番号
	};

	// ===== メッシュデータ（1つのusemtlに対応）=====
	struct MeshData {
		// CPU側で使う。ロード時のバッファ構築に使用
		std::vector<VertexData3D> vertices;
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
	struct ModelData {
		std::vector<MeshData> meshes;
		std::map<std::string, MaterialData> materialMap;
	};

	// ===== マテリアル上書き設定 =====
	// falseの項目はMTLファイルの値をそのまま使う
	// trueにした項目だけConfigの値で上書きする
	struct MaterialOverride {
		bool overrideAmbient = false;
		Vector3 ambient = {0.2f, 0.2f, 0.2f}; // Ka: 環境光色  陰の部分でも消えない最低限の明るさ
		bool overrideDiffuse = false;
		Vector3 diffuse = {0.8f, 0.8f, 0.8f}; // Kd: 拡散反射色 物体の色。光が当たった時に出る色
		bool overrideSpecular = false;
		Vector3 specular = {0.0f, 0.0f, 0.0f}; // Ks: 鏡面反射色 ハイライト(光沢)の色。金属なら白、銅なら橙など。
		bool overrideShininess = false;
		float shininess = 32.0f;               // Ns: 鏡面反射指数 Ks(ハイライト)の広がり。値が大きいほど鋭い光沢。
		bool overrideEmissive = false;
		Vector3 emissive = {0.0f, 0.0f, 0.0f}; // Ke: 自己発光色 ライティングに関係なく光っているように見せる色。ネオン・LEDなどの表現に使う
		bool overrideDissolve = false;
		float dissolve = 1.0f;                 // d: 不透明度 アルファとして使っている
	};

	// ===== 描画リクエストの設定 =====
	struct ModelConfig {
		uint32_t modelHandle = 0;         // ModelManager::Load()が返したハンドル
		uint32_t textureHandle = 0;       // TextureManager::Load()が返したハンドル
		Transform transform;         // ワールドトランスフォーム
		Transform uvTransform;       // UV変換（scale/rotation.z/translationを使用）
		uint32_t color = 0xFFFFFFFF; // モデル全体への乗算色（0xRRGGBBAA）
		ShadingModel shadingModel = ShadingModel::Unlit;
		MaterialOverride materialOverride; // MTL値の上書き設定
		Camera* camera = nullptr;
		DirectionalLight* directionalLight = nullptr;
		std::wstring windowTitle = L"";
	};

	// ===== インスタンス描画リクエスト（同じモデルを複数transformで一括描画）=====
	struct InstancedConfig {
		uint32_t modelHandle = 0;
		uint32_t textureHandle = 0;
		std::vector<Transform> transforms; // 各インスタンスのワールド変換
		Transform uvTransform;             // UV変換（全インスタンス共通）
		uint32_t color = 0xFFFFFFFF;       // 全インスタンス共通の乗算色
		ShadingModel shadingModel = ShadingModel::Unlit;
		MaterialOverride materialOverride;
		Camera* camera = nullptr;
		DirectionalLight* directionalLight = nullptr;
		std::wstring windowTitle = L"";
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
	/// モデルの描画リクエストを追加する（毎フレーム呼ぶ）
	/// </summary>
	static void DrawModel(const ModelConfig& config);

	/// <summary>
	/// 同じモデルを複数transformでまとめて描画予約する（毎フレーム呼ぶ）
	/// </summary>
	static void DrawModelInstanced(const InstancedConfig& config);

	/// <summary>
	/// 描画リクエストをすべて発行する
	/// WindowManagerのPreRenderAll内でPrimitiveRenderer::Flush3dの後に呼ぶ
	/// </summary>
	static void Flush3d(const std::wstring& windowTitle);

	/// <summary>
	/// 描画リクエストをクリアする
	/// WindowManagerのPostRenderAll内でPrimitiveRenderer::ClearRequestsの後に呼ぶ
	/// </summary>
	static void ClearRequests();

private:
	static ModelManager& GetInstance();
	// コピー・ムーブ禁止
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;
	ModelManager() = default;

	/// <summary>OBJファイルを読み込む</summary>
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
	/// <summary>MTLファイルを読み込む</summary>
	static std::map<std::string, MaterialData> LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	/// <summary>メッシュのCPU頂点をGPU常駐バッファへ転送する（ロード時1回のみ）</summary>
	static void BuildMeshBuffer(MeshData& mesh);

	static void FlushInstanced3d(const std::wstring& windowTitle);
	// マテリアルCB構築（通常・インスタンス共通で使える）
	static ModelMaterialCB BuildMaterialCB(const ModelData& modelData, const MeshData& mesh, uint32_t color, 
										   const MaterialOverride& ov, const Transform& uvTransform, bool unlit, 
										   const MaterialData** outMat);

	std::unordered_map<uint32_t, ModelData> models_;         // キー → ModelData
	std::unordered_map<std::string, uint32_t> pathToHandle_; // ファイルパス → キー（重複防止）
	uint32_t modelsKey_ = 1;

	std::vector<ModelConfig> requests_;
	std::vector<InstancedConfig> instancedRequests_;
};