#pragma once
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Utils/Transform.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// 前方宣言
class RenderContext;
class RenderWindow;
class Camera;
class DirectionalLight;

class DebugRender {
public:
	// 3D三角形の描画設定
	struct TriangleConfig {
		Vector3 top = {0.0f, 0.5f, 0.0f};
		Vector3 right = {0.5f, -0.5f, 0.0f};
		Vector3 left = {-0.5f, -0.5f, 0.0f};
		Transform transform;
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform;
		Vector2 uvTop = {0.5f, 0.0f};
		Vector2 uvRight = {1.0f, 1.0f};
		Vector2 uvLeft = {0.0f, 1.0f};
		uint32_t srvIndex = 0;
		ShadingModel shadingModel = ShadingModel::Unlit;
		DirectionalLight* directionalLight = nullptr;
		Camera* camera = nullptr;
		std::wstring windowTitle = L"";
	};

	// 3D球の描画設定
	struct SphereConfig {
		Transform transform;
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform; // UV変換。scale/rotation.z/translationを使用
		uint32_t srvIndex = 0;
		int subdivision = 16;
		ShadingModel shadingModel = ShadingModel::Unlit;
		Camera* camera = nullptr;
		DirectionalLight* directionalLight = nullptr;
		std::wstring windowTitle = L"";
	};

	// 2D矩形の描画設定
	// 座標はピクセル座標(0~ウィンドウサイズ)で指定する
	struct Rect2dConfig {
		Vector2 position = {0.0f, 0.0f}; // 中心座標(スクリーン座標系)
		Vector2 size = {100.0f, 100.0f}; // 幅・高さ
		float rotate = 0.0f;  // 回転
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform; // UV変換。scale/rotation.z/translationを使用
		uint32_t srvIndex = 0;
		ShadingModel shadingModel = ShadingModel::Unlit;
		DirectionalLight* directionalLight = nullptr;
		std::wstring windowTitle = L"";
	};

	// 3D矩形の描画設定
	// 空間に大きさ1*1の板を設置する
	struct Rect3dConfig {
		Transform transform;
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform;
		uint32_t srvIndex = 0;
		ShadingModel shadingModel = ShadingModel::Unlit;
		DirectionalLight* directionalLight = nullptr;
		Camera* camera = nullptr;
		std::wstring windowTitle = L"";
	};
	// 頂点を調整できるスプライト
	struct Quad2dConfig {
		Vector2 lb = {0.0f, 100.0f};   // 右下
		Vector2 lt = {0.0f, 0.0f};     // 左上
		Vector2 rb = {100.0f, 100.0f}; // 右下
		Vector2 rt = {100.0f, 0.0f};   // 右上
		float rotate = 0.0f;
		Vector2 uvLb = {0.0f, 1.0f};
		Vector2 uvLt = {0.0f, 0.0f};
		Vector2 uvRb = {1.0f, 1.0f};
		Vector2 uvRt = {1.0f, 0.0f};
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform;
		uint32_t srvIndex = 0;
		ShadingModel shadingModel = ShadingModel::Unlit;
		DirectionalLight* directionalLight = nullptr;
		std::wstring windowTitle = L"";
	};
	// 頂点を調整できる板
	struct Quad3dConfig {
		Vector3 lb = {-0.5f, 0.0f,  0.5f}; // 左下
		Vector3 lt = {-0.5f, 0.0f, -0.5f}; // 左上
		Vector3 rb = { 0.5f, 0.0f,  0.5f}; // 右下
		Vector3 rt = { 0.5f, 0.0f, -0.5f}; // 右上
		Vector2 uvLb = {0.0f, 1.0f};
		Vector2 uvLt = {0.0f, 0.0f};
		Vector2 uvRb = {1.0f, 1.0f};
		Vector2 uvRt = {1.0f, 0.0f};
		Vector3 rotate = { 0.0f,0.0f,0.0f };
		uint32_t color = 0xFFFFFFFF;
		Transform uvTransform;
		uint32_t srvIndex = 0;
		ShadingModel shadingModel = ShadingModel::Unlit;
		DirectionalLight* directionalLight = nullptr;
		Camera* camera = nullptr;
		std::wstring windowTitle = L"";
	};
	// OBBを生成
	struct OBBConfig {

	};

	// 3Dライン1本の定義
	struct LineSegment {
		Vector3 start;
		Vector3 end;
		uint32_t color = 0xFFFFFFFF; // RGBA
	};

	// 3Dライン群の描画設定(複数の線を1リクエストでまとめて描く)
	struct LineListConfig {
		std::vector<LineSegment> lines;  // 座標（ワールド座標）
		float fadeStartDistance = 30.0f; // フェード開始距離
		float fadeEndDistance = 50.0f;   // フェード終了距離(この距離でalpha=0)
		Camera* camera = nullptr;
		std::wstring windowTitle = L"";
	};

	// コピー・ムーブ禁止
	DebugRender(const DebugRender&) = delete;
	DebugRender& operator=(const DebugRender&) = delete;

	// 初期化
	static void Initialize();

	/// <summary>
	/// 3D三角形の描画リクエストを追加する
	/// <para>カメラを設定すると透視投影が適用される</para>
	/// </summary>
	static void DrawTriangle(const TriangleConfig& config);

	/// <summary>
	/// 2d矩形の描画リクエストを追加する
	/// 板のようなものを生成する
	/// </summary>
	static void DrawRect2d(const Rect2dConfig& config);

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
	/// 3Dライン群の描画リクエスト(複数の線を1ドローコールで描く)
	/// </summary>
	static void DrawLines(const LineListConfig& config);

	/// <summary>
	/// 3D描画リクエストをすべて実行する
	/// </summary>
	static void Flush3d(const std::wstring& windowTitle);

	/// <summary>
	/// 2D描画リクエストをすべて実行する
	/// </summary>
	static void Flush2d(const std::wstring& windowTitle, RenderWindow* rw);

	/// <summary>
	/// 描画リクエストをクリアする
	/// </summary>
	static void ClearRequests();

private:
	DebugRender() = default;
	~DebugRender() = default;

	static DebugRender* instance_;

	std::vector<TriangleConfig> requestsTriangle_;
	std::vector<Rect2dConfig> requestsRect2d_; 
	std::vector<Rect3dConfig> requestsRect3d_; 
	std::vector<Quad2dConfig> requestsQuad2d_;
	std::vector<Quad3dConfig> requestsQuad3d_; 
	std::vector<SphereConfig> requestsSphere3d_;
	std::vector<LineListConfig> requestsLines_;

	// 球のジオメトリキャッシュ
	struct SphereGeometry {
		std::vector<VertexData3D> vertices;
		std::vector<uint32_t> indices;
	};
	std::unordered_map<int, SphereGeometry> sphereGeometryCache_;

	// 内部ヘルパー
	static void FlushTriangle(const std::wstring& windowTitle);
	static void FlushRect2d(const std::wstring& windowTitle, RenderWindow* rw);
	static void FlushRect3d(const std::wstring& windowTitle);
	static void FlushQuad2d(const std::wstring& windowTitle, RenderWindow* rw);
	static void FlushQuad3d(const std::wstring& windowTitle);
	static void FlushSphere(const std::wstring& windowTitle);
	static void FlushLines(const std::wstring& windowTitle);

	// 重心を操作
	static Vector2 RotateAround2d(const Vector2& point, const Vector2& center, float radian);
	static Vector3 RotateAround3d(const Vector3& point, const Vector3& center, const Vector3& rotation);

	/// <summary>
	/// 球のジオメトリを生成する（頂点・インデックス）
	/// subdivision: 経線・緯線の分割数
	/// </summary>
	static SphereGeometry GenerateSphereGeometry(int subdivision);
};