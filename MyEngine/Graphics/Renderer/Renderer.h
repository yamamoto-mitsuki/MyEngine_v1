#pragma once
#include <list>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/IBL/IBLEnvironment.h"
#include "MyEngine/Graphics/Model/ModelManager.h"


// 前方宣言
class RenderContext;
class RenderWindow;
class Camera;
class DirectionalLight;
class PointLight;


/// <summary>
/// AABB,OBBなど基本矩形を描画するクラス
/// </summary>
class Renderer {
public:
	//=============================================================================
	// Model
	//=============================================================================
	// --- モデルの描画設定 ---
	struct ModelConfig {
		uint32_t modelHandle = 0;                                  // モデルハンドル
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		Transform transform;                                       // 拡縮、回転、移動
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		MaterialParams material;                                   // マテリアル調整パラメータ
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		Camera* camera = nullptr;                                  // カメラ設定
		IBLEnvironment* env = nullptr;                             // 環境光源
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D三角形の描画設定 ---
	struct TriangleConfig {
		Vector3 top = {0.0f, 0.5f, 0.0f};                          // 上頂点の座標
		Vector3 right = {0.5f, -0.5f, 0.0f};                       // 右頂点の座標
		Vector3 left = {-0.5f, -0.5f, 0.0f};                       // 左頂点の座標
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		Vector2 uvTop = {0.5f, 0.0f};                              // 上頂点のUV座標
		Vector2 uvRight = {1.0f, 1.0f};                            // 右頂点のUV座標
		Vector2 uvLeft = {0.0f, 1.0f};                             // 左頂点のUV座標
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D球の描画設定 ---
	struct SphereConfig {
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		int subdivision = 16;                                      // 分割数
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D矩形の描画設定 ---
	// 空間に大きさ1*1の板を設置する
	struct Rect3dConfig {
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 頂点を調整できる板 ---
	struct Quad3dConfig {
		Vector3 lb = {-0.5f, 0.0f, 0.5f};                          // 左下の座標
		Vector3 lt = {-0.5f, 0.0f, -0.5f};                         // 左上の座標
		Vector3 rb = {0.5f, 0.0f, 0.5f};                           // 右下の座標
		Vector3 rt = {0.5f, 0.0f, -0.5f};                          // 右上の座標
		Vector2 uvLb = {0.0f, 1.0f};                               // 左下のUV座標
		Vector2 uvLt = {0.0f, 0.0f};                               // 左上のUV座標
		Vector2 uvRb = {1.0f, 1.0f};                               // 右下のUV座標
		Vector2 uvRt = {1.0f, 0.0f};                               // 右上のUV座標
		Vector3 rotate = {0.0f, 0.0f, 0.0f};                       // 回転
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- AABBを生成 ---
	struct AABBConfig {
		AABB aabb;                                                 // AABBの形状設定
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- OBBを生成 ---
	struct OBBConfig {
		OBB obb;                                                   // OBBの形状設定
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	//=============================================================================
	// Particle
	//=============================================================================
	// --- パーティクルの描画設定 ---
	struct ParticleConfig {
		const std::list<Particle>* particles = nullptr; // シミュレーション結果（ParticleManagerが持つ）
		uint32_t textureHandle = 0;                     // グループ共通テクスチャ（エミッター単位）
		Vector4 color = {1.0f,1.0f,1.0f,1.0f};          // グループ全体にかける色
		Transform uvTransform;                          // UVアニメ用
		BlendMode blendMode = BlendMode::Add;           // グループ単位（ドローコール単位なので粒ごとは不可能）
		Camera* camera = nullptr;                       // カメラ
		bool isBillboard = false;                       // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";
	}; 

	//=============================================================================
	// Sprite
	//=============================================================================
	// --- スプライトの描画設定 ---
	// 座標系は正規化座標。画面中央が原点、右がX+、上がY+、画面端が±1。
	// 例: position={0,0}で画面中央、{-1,1}で左上、{1,-1}で右下
	struct SpriteConfig {
	public:
		Vector2 position = {0.0f, 0.0f};                           // 中心座標（画面中央が原点、端が±1）
		Vector2 scale = {1.0f, 1.0f};                              // 拡大率（1.0でテクスチャ原寸）
		float rotate = 0.0f;                                       // 回転（ラジアン、反時計回り）
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		BlendMode blendMode = BlendMode::None;                     // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
		Vector2 pivot = {0.5f, 0.5f};                              // 矩形のどこをpositionに合わせるか。基本触らない！（0,0=左上 1,1=右下）
	};

	// --- 頂点を調整できるスプライト ---
	struct Quad2dConfig {
		Vector2 lb = {-0.1f, -0.1f};                               // 左下の座標
		Vector2 lt = {-0.1f, +0.1f};                               // 左上の座標
		Vector2 rb = {+0.1f, -0.1f};                               // 右下の座標
		Vector2 rt = {+0.1f, +0.1f};                               // 右上の座標
		float rotate = 0.0f;                                       // 回転（ラジアン、反時計回り）
		Vector2 uvLb = {0.0f, 1.0f};                               // 左下のUV座標
		Vector2 uvLt = {0.0f, 0.0f};                               // 左上ののUV座標
		Vector2 uvRb = {1.0f, 1.0f};                               // 右下のUV座標
		Vector2 uvRt = {1.0f, 0.0f};                               // 右上のUV座標
		uint32_t color = 0xFFFFFFFF;                               // 色
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		BlendMode blendMode = BlendMode::None;                     // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	//=============================================================================
	// Line
	//=============================================================================
	// --- 3Dライン1本の定義 ---
	struct LineSegment {
		Vector3 start;               // スタート座標
		Vector3 end;                 // 終了座標
		uint32_t color = 0xFFFFFFFF; // 色
	};

	// --- 3Dライン群の描画設定(複数の線を1リクエストでまとめて描く) ---
	struct LineListConfig {
		std::vector<LineSegment> lines;  // 座標（ワールド座標）
		float fadeStartDistance = 30.0f; // フェード開始距離
		float fadeEndDistance = 50.0f;   // フェード終了距離(この距離でalpha=0)
		uint32_t color = 0xFFFFFFFF;     // 色
		Camera* camera = nullptr;        // カメラ設定
		std::wstring windowTitle = L"";  // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// コピー・ムーブ禁止
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	// 初期化
	static void Initialize();

	/// <summary>
	/// モデルの描画リクエストを追加する
	/// </summary>
	static void DrawModel(const ModelConfig& config);

	/// <summary>
	/// パーティクルの描画リクエストを追加
	/// </summary>
	/// <param name="config"></param>
	static void DrawParticle(const ParticleConfig& config);

	/// <summary>
	/// 3D三角形の描画リクエストを追加する
	/// <para>カメラを設定すると透視投影が適用される</para>
	/// </summary>
	static void DrawTriangle(const TriangleConfig& config);

	/// <summary>
	/// 2d矩形の描画リクエストを追加する
	/// 板のようなものを生成する
	/// </summary>
	static void DrawSprite(const SpriteConfig& config);

	/// <summary>
	/// 3d矩形の描画リクエストを追加する
	/// 板のようなものを生成する
	/// </summary>
	static void DrawRect3d(const Rect3dConfig& config);

	/// <summary>
	/// 2d矩形の描画リクエストを追加する
	/// 入力した4頂点を繋げたものを生成する
	/// </summary>
	static void DrawQuad2d(const Quad2dConfig& config);

	/// <summary>
	/// 3d矩形の描画リクエストを追加する
	/// 入力した4頂点を繋げたものを生成する
	/// </summary>
	static void DrawQuad3d(const Quad3dConfig& config);

	/// <summary>
	/// 3D球の描画リクエストを追加する
	/// </summary>
	static void DrawSphere(const SphereConfig& config);

	/// <summary>
	/// AABBの描画リクエストを追加する
	/// </summary>
	static void DrawAABB(const AABBConfig& config);

	/// <summary>
	/// OBBの描画リクエストを追加する
	/// </summary>
	static void DrawOBB(const OBBConfig& config);

	/// <summary>
	/// 3Dライン群の描画リクエスト(複数の線を1ドローコールで描く)
	/// </summary>
	static void DrawLines(const LineListConfig& config);

private:
	// スプライトの基準解像度
	// 大きさは「この解像度でのテクスチャ原寸 × scale」で決まるので、
	// 描画先(ウィンドウ / RenderTexture)の解像度が変わっても画面上の比率は変わらない。
	static constexpr float kSpriteBaseWidth = 1280.0f;
	static constexpr float kSpriteBaseHeight = 720.0f;

	Renderer() = default;
	~Renderer() = default;
	static Renderer* instance_;

	// 球のジオメトリキャッシュ
	struct SphereGeometry {
		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
	};
	std::unordered_map<int, SphereGeometry> sphereGeometryCache_;

	// メッシュの共通作成部分
	template<class TConfig> static void PushMesh(const TConfig& config, std::vector<Vertex3dData>&& vertices, 
		std::vector<uint32_t>&& indices, const Matrix4x4& worldMatrix);

	template<class TConfig> static void PushSprite(const TConfig& config, Vector2 lb, Vector2 lt, Vector2 rb, Vector2 rt, 
		Vector2 uvLb, Vector2 uvLt, Vector2 uvRb, Vector2 uvRt);

	// MTLマテリアル + Config の色から Material3dData を構築する（materialがnullptrならデフォルト値）
	static Material3dData MakeModelMaterial(const ModelManager::MtlMaterial* mat, uint32_t color, const Transform& uvTransform);

	// 重心を操作
	static Vector2 RotateAround2d(const Vector2& point, const Vector2& center, float radian);
	static Vector3 RotateAround3d(const Vector3& point, const Vector3& center, const Vector3& rotation);

	/// <summary>正規化座標(-1〜1) → 基準解像度でのピクセル座標（画面中央が原点、Y+が上）</summary>
	static Vector2 NdcToBasePixel(const Vector2& ndc) { return {ndc.x * kSpriteBaseWidth * 0.5f, ndc.y * kSpriteBaseHeight * 0.5f}; }

	/// <summary>基準解像度でのピクセル座標 → 正規化座標(-1〜1)</summary>
	static Vector2 BasePixelToNdc(const Vector2& px) { return {px.x / (kSpriteBaseWidth * 0.5f), px.y / (kSpriteBaseHeight * 0.5f)}; }

	/// <summary>
	/// 4頂点の四角形から表面法線を計算する
	/// </summary>
	static Vector3 CalcQuadNormal(const Vector3& lb, const Vector3& rb, const Vector3& lt);

	/// <summary>
	/// 8頂点のボックスをModelDescの頂点・インデックスに変換する（AABB/OBB共通）
	/// <para>面ごとに外向き法線を計算し、裏面カリングと整合する巻き順で6面を生成する</para>
	/// </summary>
	/// <param name="corners">ボックスの8頂点（ビット: bit0=X,bit1=Y,bit2=Z / 0=min,1=max）</param>
	/// <param name="center">ボックス中心（面の外向き判定に使用）</param>
	static void MakeBoxGeometry(const Vector3 corners[8], const Vector3& center, std::vector<Vertex3dData>& outVertices, std::vector<uint32_t>& outIndices);

	/// <summary>
	/// 球のジオメトリを生成する（頂点・インデックス）
	/// subdivision: 経線・緯線の分割数
	/// </summary>
	static SphereGeometry GenerateSphereGeometry(int subdivision);
};