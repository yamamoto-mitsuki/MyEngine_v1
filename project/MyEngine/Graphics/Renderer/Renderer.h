#pragma once
#include <memory>
#include <list>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/IBL/IBLEnvironment.h"
#include "MyEngine/Graphics/Model/ModelManager.h"


// 前方宣言
struct Entity;
class RenderContext;
class RenderWindow;
class Camera;


/// <summary>
/// AABB,OBBなど基本矩形を描画するクラス
/// </summary>
class Renderer {
public:
	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class Renderer;
	};
	explicit Renderer(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	//=============================================================================
	// Model
	//=============================================================================
	// --- モデルの描画設定 ---
	struct ModelConfig {
		uint32_t modelHandle = 0;                                  // モデルハンドル
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		Transform transform;                                       // 拡縮、回転、移動
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		const Matrix4x4* worldMatrix = nullptr;                    // 指定するとtransformの代わりに使う（階層・ロール用）
		const std::vector<Matrix4x4>* nodeMatrices = nullptr;      // ノードの行列（AnimationPlayer::GetNodeMatrices）。nullptr・数が合わないときはモデルに入っている行列
		Handle<Entity> entity;                                     // 持ち主のEntity、スキンのあるメッシュを CS で曲げるとき、出力バッファを見分けるのに使う（無効なら曲げない）
		uint32_t color = 0xFFFFFFFF;                               // 色
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		MaterialParams material;                                   // マテリアル調整パラメータ
		bool overrideMaterial = false;                             // true：material をモデルの mtl / glTF の値の代わりに使う（false なら mtl に無いときだけ）
		Camera* camera = nullptr;                                  // カメラ設定
		IBLEnvironment* env = nullptr;                             // 環境光源
		BillboardMode billboard = BillboardMode::None;             // カメラの方を向かせる（Full / AxisY。cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D三角形の描画設定 ---
	struct TriangleConfig {
		Vector3 top = {0.0f, 0.5f, 0.0f};                          // 上頂点の座標
		Vector3 right = {0.5f, -0.5f, 0.0f};                       // 右頂点の座標
		Vector3 left = {-0.5f, -0.5f, 0.0f};                       // 左頂点の座標
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		Vector2 uvTop = {0.5f, 0.0f};                              // 上頂点のUV座標
		Vector2 uvRight = {1.0f, 1.0f};                            // 右頂点のUV座標
		Vector2 uvLeft = {0.0f, 1.0f};                             // 左頂点のUV座標
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D球の描画設定 ---
	struct SphereConfig {
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		int subdivision = 16;                                      // 分割数
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		const MaterialParams* material = nullptr;                  // 光沢・金属感・自己発光など（nullptr なら既定の値。切り抜きは alphaCutoff の方を使う）
		IBLEnvironment* env = nullptr;                             // 環境光（PBR に要る。無ければ PBR の代わりに Half Lambert で描く）
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D矩形の描画設定 ---
	// 空間に大きさ1*1の板を設置する
	struct Rect3dConfig {
		Transform transform;                                       // 拡縮、回転、移動
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 3D空間に置く画像の板（スプライト）---
	// 板は worldMatrix のローカルの XY 平面に立つ（表は -Z 側。カメラを置いたときの向きのまま見える）。裏からも見える
	struct Sprite3dConfig {
		const Matrix4x4* worldMatrix = nullptr;                    // 置く場所・向き・大きさ（Entity のワールド行列。nullptr なら原点）
		Vector2 size = {1.0f, 1.0f};                               // 板の幅・高さ（ローカル座標）
		Vector2 pivot = {0.5f, 0.5f};                              // 板のどこを原点に合わせるか（0,0=左下 1,1=右上）
		Vector2 uvMin = {0.0f, 0.0f};                              // 画像のどこを貼るか：左上のUV（左右反転するなら uvMin.x と uvMax.x を入れ替える）
		Vector2 uvMax = {1.0f, 1.0f};                              // 画像のどこを貼るか：右下のUV
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル（0なら白）
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidNone; // ラスタライザ設定（既定は両面）
		DepthMode depthMode = DepthMode::TestNoWrite;              // 深度設定（TestNoWrite＝半透明として奥から順に描く）
		BillboardMode billboard = BillboardMode::None;             // カメラの方を向かせる（Full / AxisY）
		Camera* camera = nullptr;                                  // 半透明を奥から並べるのに使う
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
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- 頂点を自分で並べる形（軌跡の帯など、毎フレーム形が変わる物）---
	// 頂点と番号は DrawMesh の引数で渡す（ここに持たせると、毎フレーム配列ごと写すことになるので）
	struct MeshConfig {
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidNone; // ラスタライザ設定（既定は両面。並べ方の向きを気にしなくてよいように）
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		const MaterialParams* material = nullptr;                  // 光沢・金属感・自己発光など（nullptr なら既定の値。切り抜きは alphaCutoff の方を使う）
		IBLEnvironment* env = nullptr;                             // 環境光（PBR に要る。無ければ PBR の代わりに Half Lambert で描く）
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // 使わない（頂点はワールド座標で渡す）。PushMesh が読むので置いてある
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- AABBを生成 ---
	struct AABBConfig {
		AABB aabb;                                                 // AABBの形状設定
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		Camera* camera = nullptr;                                  // カメラ設定
		bool isBillboard = false;                                  // trueで常にカメラの方を向く（cameraが必要）
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// --- OBBを生成 ---
	struct OBBConfig {
		OBB obb;                                                   // OBBの形状設定
		uint32_t color = 0xFFFFFFFF;                               // 色
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		const MaterialParams* material = nullptr;                  // 光沢・金属感・自己発光など（nullptr なら既定の値。切り抜きは alphaCutoff の方を使う）
		IBLEnvironment* env = nullptr;                             // 環境光（PBR に要る。無ければ PBR の代わりに Half Lambert で描く）
		Camera* camera = nullptr;                                  // カメラ設定
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
		int order = 0;                                             // 重ねる順（大きいほど手前。同じなら積んだ順。文字とも比べる）
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
		int order = 0;                                             // 重ねる順（大きいほど手前。同じなら積んだ順。文字とも比べる）
	};

	//=============================================================================
	// Text
	//=============================================================================
	// --- 画面に重ねる文字（文字の並べ方は TextRenderSystem が決めて、頂点で渡す）---
	struct Text2dConfig {
		Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};        // 文字の色
		Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 1.0f}; // 縁取りの色
		float outlineWidth = 0.0f;                       // 縁取りの太さ（SDF の値で 0〜0.5。0 なら縁取りなし）
		Vector4 glowColor = {1.0f, 1.0f, 1.0f, 1.0f};    // 光（グロー）の色
		float glowWidth = 0.0f;                          // 光の広さ（SDF の値で。縁取りの外側へ。0 なら無し）
		Vector4 shineColor = {1.0f, 1.0f, 1.0f, 1.0f};   // 斜めに通る光の色
		float shinePosition = 0.0f;                      // 斜めの光の帯の真ん中（文字のかたまりの左が 0、右が 1）
		float shineWidth = 0.0f;                         // 帯の半分の幅（かたまりの幅に対して。0 なら無し）
		uint32_t fontTextureIndex = 0;                   // フォントのアトラス（FontManager::GetAtlasTextureIndex）
		BlendMode blendMode = BlendMode::Normal;         // ブレンド設定
		int order = 0;                                   // 重ねる順（スプライトの order と同じ）
		std::wstring windowTitle = L"";                  // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
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
		DepthMode depthMode = DepthMode::TestWrite; // Disable にすると、物の後ろ・中でも見える（骨のギズモなど）
		std::wstring windowTitle = L"";  // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};

	// コピー・ムーブ禁止
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	// 初期化
	static void Initialize();
	// 解放
	static void Release();

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
	/// 3D空間に置く画像の板の描画リクエストを追加する（Sprite Renderer が使う）
	/// <para>大きさ・ずらし・画像の切り出しを決められる板。ビルボードは Y 軸だけ（AxisY）もできる</para>
	/// </summary>
	static void DrawSprite3d(const Sprite3dConfig& config);

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
	/// 画面に重ねる文字の描画リクエストを追加する（何文字あっても1回の描画）
	/// </summary>
	/// <param name="vertices">1文字につき4つ（左下・左上・右下・右上の順。位置は正規化座標、UV はアトラスの場所）。中で動かすので std::move で渡すと写さずに済む</param>
	static void DrawText2d(const Text2dConfig& config, std::vector<Vertex2dData> vertices);

	/// <summary>
	/// 3d矩形の描画リクエストを追加する
	/// 入力した4頂点を繋げたものを生成する
	/// </summary>
	static void DrawQuad3d(const Quad3dConfig& config);

	/// <summary>
	/// 頂点と番号を自分で並べた形の描画リクエストを追加する（1回の描画で出る）
	/// <para>頂点はワールド座標。番号は3つで三角形1つ（0, 1, 2, ...）</para>
	/// </summary>
	/// <param name="vertices">頂点（位置の w は 1）。中で動かすので、使い終わった配列を std::move で渡すと写さずに済む</param>
	/// <param name="indices">三角形の頂点の番号</param>
	static void DrawMesh(const MeshConfig& config, std::vector<Vertex3dData> vertices, std::vector<uint32_t> indices);

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

	/// <summary>
	/// このフレームで使うライトを設定する（LightSystemが1フレームに1回呼ぶ）
	/// <para>Unlit以外の描画は、すべてこのライトで照らされる</para>
	/// </summary>
	static void SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights, const SpotLightListData& spotLights);

	// スプライトの基準解像度
	// 大きさは「この解像度でのテクスチャ原寸 × scale」で決まるので、
	// 描画先(ウィンドウ / RenderTexture)の解像度が変わっても画面上の比率は変わらない。
	static constexpr float kSpriteBaseWidth = 1280.0f;
	static constexpr float kSpriteBaseHeight = 720.0f;


private:
	~Renderer() = default;
	// シングルトン
	friend struct std::default_delete<Renderer>;
	static std::unique_ptr<Renderer> instance_;

	// 球のジオメトリキャッシュ
	struct SphereGeometry {
		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
	};
	std::unordered_map<int, SphereGeometry> sphereGeometryCache_;

	// メッシュの共通作成部分（カメラの方を向かせるかは config.isBillboard で決める）
	template<class TConfig> static void PushMesh(const TConfig& config, std::vector<Vertex3dData>&& vertices, std::vector<uint32_t>&& indices, const Matrix4x4& worldMatrix);
	// 同じ物で、カメラの方を向かせ方を billboard で渡す（Y 軸だけ回す AxisY も使える）
	template<class TConfig> static void PushMesh(const TConfig& config, std::vector<Vertex3dData>&& vertices, std::vector<uint32_t>&& indices, const Matrix4x4& worldMatrix, BillboardMode billboard);

	template<class TConfig> static void PushSprite(const TConfig& config, Vector2 lb, Vector2 lt, Vector2 rb, Vector2 rt, 
		Vector2 uvLb, Vector2 uvLt, Vector2 uvRb, Vector2 uvRt);

	// MTLマテリアル + Config の色から Material3dData を構築する
	// mat が nullptr か、overrideMaterial が true なら、params の値を使う
	static Material3dData MakeModelMaterial(const ModelManager::MtlMaterial* mat, uint32_t color, const Transform& uvTransform, const MaterialParams& params, bool overrideMaterial);

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