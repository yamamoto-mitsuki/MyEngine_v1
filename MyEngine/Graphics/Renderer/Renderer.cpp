#include "MyEngine/Graphics/Renderer/Renderer.h"

#include <cmath>
#include <utility>
#include <algorithm>

#include <Windows.h>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Renderer/DrawRequest.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

// 静的メンバ変数
Renderer* Renderer::instance_ = nullptr;


//=============================================================================
// 共通作成部分
//=============================================================================
// ===== デフォルトモデルマテリアル作成ヘルパー =====
static Material3dData MakeDefaultModelMaterial(float r, float g, float b, float a, const Transform& uvTransform) {
	Material3dData mat;
	mat.color = {r, g, b, a};
	mat.uvTransform = MakeUVTransformMatrix(uvTransform);
	mat.ambient = {0.0f, 0.0f, 0.0f};
	mat.diffuse = {1.0f, 1.0f, 1.0f};
	mat.specular = {0.0f, 0.0f, 0.0f};
	mat.shininess = 1.0f;
	mat.emissive = {0.0f, 0.0f, 0.0f};
	return mat;
}

// ===== メッシュの共通作成部分 =====
template<class TConfig> 
void Renderer::PushMesh(const TConfig& config, std::vector<Vertex3dData>&& vertices, std::vector<uint32_t>&& indices, 
	const Matrix4x4& worldMatrix) {
	if (config.shadingType != ShadingType::Unlit) {
		MY_ASSERT_MSG(config.directionalLight != nullptr, "ShadingType::Unlit以外には光源を設置してください");
	}
	// 色変換
	float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(config.color & 0xFF) / 255.0f;
	// bind情報、頂点データ
	MeshRequest req;
	req.vertices = std::move<vertices>;
	req.indices = std::move(indices);
	req.materialData = MakeDefaultModelMaterial(r, g, b, a, config.uvTransform);
	req.materialData.textureIndex = config.textureHandle;
	req.transformationMatricesData.wvpMatrix = config.camera ? config.camera->CalcWVP(worldMatrix) : worldMatrix;
	req.transformationMatricesData.worldMatrix = worldMatrix;
	req.cameraData.worldPosition = config.camera ? config.camera->GetTranslation() : Vector3{};
	req.lightData = config.directionalLight ? config.directionalLight->GetBuffer()->GetGPUVirtualAddress() : 0;
	req.shadingType = config.shadingType;
	req.blendMode = config.blendMode;
	req.rasterizerType = config.rasterizerType;
	req.depthMode = config.depthMode;
	req.windowTitle = config.windowTitle;
	RenderQueue::Request(std::move(req));
}

// ===== スプライトの共通作成部分 =====
template<class TConfig>
void Renderer::PushSprite(const TConfig& config, Vector2 lb, Vector2 lt, Vector2 rb, Vector2 rt, 
	Vector2 uvLb, Vector2 uvLt, Vector2 uvRb, Vector2 uvRt) {
	// 色変換
	float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(config.color & 0xFF) / 255.0f;
	// bind情報、頂点情報
	SpriteRequest req;
	req.materialData.color = {r, g, b, a};
	req.materialData.uvTransform = MakeUVTransformMatrix(config.uvTransform);
	req.materialData.textureIndex = config.srvIndex;
	req.vertices[0] = {
	    {lb.x, lb.y, 0.0f, 1.0f},
        uvLb
    };
	req.vertices[1] = {
	    {lt.x, lt.y, 0.0f, 1.0f},
        uvLt
    };
	req.vertices[2] = {
	    {rb.x, rb.y, 0.0f, 1.0f},
        uvRb
    };
	req.vertices[3] = {
	    {rt.x, rt.y, 0.0f, 1.0f},
        uvRt
    };
	req.windowTitle = config.windowTitle;
	RenderQueue::Request(std::move(req));
}


//=============================================================================
// 初期化
//=============================================================================
void Renderer::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼び出されています。");
	instance_ = new Renderer();

	LogManager::Log("Initialized");
}


//=============================================================================
// 〇〇Config → 〇〇Request
//=============================================================================
// ===== Model =====
void Renderer::DrawModel(const ModelConfig& config) {
	// 早期リターン
	if (config.shadingType != ShadingType::Unlit) {
		MY_ASSERT_MSG(config.directionalLight != nullptr, "ShadingType::Unlit以外には光源を設置してください");
	}
	// モデル
	const ModelManager::ModelAsset* asset = ModelManager::GetModelAsset(config.modelHandle);
	if (!asset) {
		LogManager::Warning("登録されていないModelHandleを参照");
		return;
	}
	// 色変換
	float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(config.color & 0xFF) / 255.0f;
	// 行列
	Matrix4x4 worldMatrix = MakeAffineMatrix(config.transform.scale, config.transform.rotation, config.transform.translation);
	Matrix4x4 wvpMatrix = config.camera ? config.camera->CalcWVP(worldMatrix) : worldMatrix;
	// SubMeshごとに1つのMeshRequest
	for (const ModelManager::SubMesh& mesh : asset->meshes) {
		MeshRequest req;
		// 静的ジオメトリ：GPU常駐バッファを指すだけ
		req.isStatic = true;
		req.vbv = mesh.vbv;
		req.ibv = mesh.ibv;
		req.indexCount = mesh.indexCount;
		// マテリアル構築（旧Flush3dのMtlMaterial→Material3dData変換をそのまま）
		req.materialData = /* mat有無で分岐して構築（旧コードそのまま） */;
		req.materialData.textureIndex = (config.textureHandle != 0) ? config.textureHandle : 0;
		req.transformationMatricesData = {wvpMatrix, worldMatrix};
		req.cameraData.worldPosition = config.camera ? config.camera->GetTranslation() : Vector3{};
		req.lightCBV = config.directionalLight ? config.directionalLight->GetBuffer()->GetGPUVirtualAddress() : 0;
		// 描画設定
		req.shadingType = config.shadingType;
		req.blendMode = config.blendMode;
		req.windowTitle = config.windowTitle;

		RenderQueue::Request(std::move(req));
	}
}

// ===== Triangle =====
void Renderer::DrawTriangle(const TriangleConfig& config) {
	// 頂点データ
	std::vector<Vertex3dData> vertices = {
	    {{config.top.x, config.top.y, config.top.z, 1.0f},       config.uvTop,   {0.0f, 0.0f, 1.0f}},
	    {{config.right.x, config.right.y, config.right.z, 1.0f}, config.uvRight, {0.0f, 0.0f, 1.0f}},
	    {{config.left.x, config.left.y, config.left.z, 1.0f},    config.uvLeft,  {0.0f, 0.0f, 1.0f}},
	};
	// WorldMatrix
	Matrix4x4 worldMatrix = MakeAffineMatrix(config.transform.scale, config.transform.rotation, config.transform.translation);
	// メッシュ作成
	PushMesh(config, std::move(vertices), {0, 1, 2}, worldMatrix);
}

// ===== Rect3d =====
void Renderer::DrawRect3d(const Rect3dConfig& config) { 
	// WorldMatrix
	Matrix4x4 worldMatrix = MakeAffineMatrix(config.transform.scale, config.transform.rotation, config.transform.translation);
	// Vector4 → Vector3 に変換
	auto toWorld = [&](const Vector4& v) -> Vector3 {
		Vector4 r = v * worldMatrix;
		return {r.x, r.y, r.z};
	};
	Vector3 lb = toWorld({-0.5f, 0.0f, 0.5f, 1.0f});
	Vector3 lt = toWorld({-0.5f, 0.0f, -0.5f, 1.0f});
	Vector3 rb = toWorld({0.5f, 0.0f, 0.5f, 1.0f});
	Vector3 rt = toWorld({0.5f, 0.0f, -0.5f, 1.0f});
	Vector3 normal = CalcQuadNormal(lb, rb, lt);
	// 頂点データ
	std::vector<Vertex3dData> vertices = {
	    {{lb.x, lb.y, lb.z, 1.0f}, {0.0f, 1.0f}, normal},
	    {{lt.x, lt.y, lt.z, 1.0f}, {0.0f, 0.0f}, normal},
	    {{rb.x, rb.y, rb.z, 1.0f}, {1.0f, 1.0f}, normal},
	    {{rt.x, rt.y, rt.z, 1.0f}, {1.0f, 0.0f}, normal},
	};
	// メッシュ作成
	PushMesh(config, std::move(vertices), {0, 1, 2, 1, 3, 2}, MakeIdentity4x4());
}

// ===== Quad3d =====
void Renderer::DrawQuad3d(const Quad3dConfig& config) { 
	// 頂点を算出
	Vector3 lb = config.lb, lt = config.lt, rb = config.rb, rt = config.rt; 
	if (config.rotate.x != 0.0f || config.rotate.y != 0.0f || config.rotate.z != 0.0f) {
		Vector3 center = {(lb.x + lt.x + rb.x + rt.x) * 0.25f, (lb.y + lt.y + rb.y + rt.y) * 0.25f, (lb.z + lt.z + rb.z + rt.z) * 0.25f};
		lb = RotateAround3d(lb, center, config.rotate);
		lt = RotateAround3d(lt, center, config.rotate);
		rb = RotateAround3d(rb, center, config.rotate);
		rt = RotateAround3d(rt, center, config.rotate);
	}
	// 法線
	Vector3 normal = CalcQuadNormal(lb, rb, lt);
	// 頂点データ
	std::vector<Vertex3dData> vertices = {
	    {{lb.x, lb.y, lb.z, 1.0f}, config.uvLb, normal},
	    {{lt.x, lt.y, lt.z, 1.0f}, config.uvLt, normal},
	    {{rb.x, rb.y, rb.z, 1.0f}, config.uvRb, normal},
	    {{rt.x, rt.y, rt.z, 1.0f}, config.uvRt, normal},
	};
	// メッシュ作成
	PushMesh(config, std::move(vertices), {0, 1, 2, 1, 3, 2}, MakeIdentity4x4());
}

// ===== AABB =====
void Renderer::DrawAABB(const AABBConfig& config) {
	// AABB
	const AABB& box = config.aabb;
	Vector3 center = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f};
	Vector3 corners[8];
	for (int i = 0; i < 8; ++i) {
		corners[i] = {(i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z};
	}
	// 頂点データ
	std::vector<Vertex3dData> vertices;
	std::vector<uint32_t> indices;
	MakeBoxGeometry(corners, center, vertices, indices);
	// メッシュ作成
	PushMesh(config, std::move(vertices), std::move(indices), MakeIdentity4x4());
}

// ===== OBB =====
void Renderer::DrawOBB(const OBBConfig& config) {
	// OBB
	const OBB& obb = config.obb;
	Vector3 corners[8];
	for (int i = 0; i < 8; ++i) {
		float sx = (i & 1) ? obb.size.x : -obb.size.x;
		float sy = (i & 2) ? obb.size.y : -obb.size.y;
		float sz = (i & 4) ? obb.size.z : -obb.size.z;
		corners[i] = {
		    obb.center.x + obb.axes[0].x * sx + obb.axes[1].x * sy + obb.axes[2].x * sz,
		    obb.center.y + obb.axes[0].y * sx + obb.axes[1].y * sy + obb.axes[2].y * sz,
		    obb.center.z + obb.axes[0].z * sx + obb.axes[1].z * sy + obb.axes[2].z * sz,
		};
	}
	// 頂点生成
	std::vector<Vertex3dData> vertices;
	std::vector<uint32_t> indices;
	MakeBoxGeometry(corners, obb.center, vertices, indices);
	// メッシュ作成
	PushMesh(config, std::move(vertices), std::move(indices), MakeIdentity4x4());
}

// ===== Sphere =====
void Renderer::DrawSphere(const SphereConfig& config) {
	// ジオメトリキャッシュ（分割数ごとに1回生成）
	auto it = instance_->sphereGeometryCache_.find(config.subdivision);
	if (it == instance_->sphereGeometryCache_.end()) {
		it = instance_->sphereGeometryCache_.emplace(config.subdivision, GenerateSphereGeometry(config.subdivision)).first;
	}
	const SphereGeometry& geo = it->second;
	Matrix4x4 worldMatrix = MakeAffineMatrix(config.transform.scale, config.transform.rotation, config.transform.translation);
	// キャッシュはコピーして渡す（Requestが所有するため）
	PushMesh(config, std::vector<Vertex3dData>(geo.vertices), std::vector<uint32_t>(geo.indices), worldMatrix);
}

// ===== Rect2d =====
void Renderer::DrawRect2d(const Rect2dConfig& config) {
	// 頂点情報
	float hw = config.size.x * 0.5f, hh = config.size.y * 0.5f;
	Vector2 lb = {config.position.x - hw, config.position.y + hh};
	Vector2 lt = {config.position.x - hw, config.position.y - hh};
	Vector2 rb = {config.position.x + hw, config.position.y + hh};
	Vector2 rt = {config.position.x + hw, config.position.y - hh};
	if (config.rotate != 0.0f) {
		lb = RotateAround2d(lb, config.position, config.rotate);
		lt = RotateAround2d(lt, config.position, config.rotate);
		rb = RotateAround2d(rb, config.position, config.rotate);
		rt = RotateAround2d(rt, config.position, config.rotate);
	}
	PushSprite(config, lb, lt, rb, rt, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f});
}

// ===== Quad2d =====
void Renderer::DrawQuad2d(const Quad2dConfig& config) {
	// 頂点情報
	Vector2 lb = config.lb, lt = config.lt, rb = config.rb, rt = config.rt;
	if (config.rotate != 0.0f) {
		Vector2 center = {(lb.x + lt.x + rb.x + rt.x) * 0.25f, (lb.y + lt.y + rb.y + rt.y) * 0.25f};
		lb = RotateAround2d(lb, center, config.rotate);
		lt = RotateAround2d(lt, center, config.rotate);
		rb = RotateAround2d(rb, center, config.rotate);
		rt = RotateAround2d(rt, center, config.rotate);
	}
	PushSprite(config, lb, lt, rb, rt, config.uvLb, config.uvLt, config.uvRb, config.uvRt);
}

// ===== Line =====
void Renderer::DrawLines(const LineListConfig& config) {
	// 早期リターン
	if (config.lines.empty()) {
		return;
	}
	// bind情報
	LineRequest req;
	req.materialData.cameraWorldPos = config.camera ? config.camera->GetTranslation() : Vector3{};
	req.materialData.fadeStartDistance = config.fadeStartDistance;
	req.materialData.fadeEndDistance = config.fadeEndDistance;
	Matrix4x4 identity = MakeIdentity4x4();
	req.transformationMatricesData.wvpMatrix = config.camera ? config.camera->CalcWVP(identity) : identity;
	req.transformationMatricesData.worldMatrix = identity;
	req.vertices.reserve(config.lines.size() * 2);
	for (const LineSegment& seg : config.lines) {
		// 色変換
		float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(config.color & 0xFF) / 255.0f;
		Vector4 col = {r, g, b, a};
		req.vertices.push_back({
		    {seg.start.x, seg.start.y, seg.start.z, 1.0f},
            col
        });
		req.vertices.push_back({
		    {seg.end.x, seg.end.y, seg.end.z, 1.0f},
            col
        });
	}
	req.windowTitle = config.windowTitle;
	RenderQueue::Request(std::move(req));
}

//=============================================================================
// 2D頂点をcenterを原点としてZ軸回転させる
//=============================================================================
Vector2 Renderer::RotateAround2d(const Vector2& point, const Vector2& center, float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	float ox = point.x - center.x;
	float oy = point.y - center.y;
	return {ox * c - oy * s + center.x, ox * s + oy * c + center.y};
}


//=============================================================================
// 3D頂点をcenterを原点としてXYZ回転させる
//=============================================================================
Vector3 Renderer::RotateAround3d(const Vector3& point, const Vector3& center, const Vector3& rotation) {
	Vector4 local = {point.x - center.x, point.y - center.y, point.z - center.z, 1.0f};
	// 回転行列（scale={1,1,1}, translation={0,0,0}）
	Matrix4x4 rotMat = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, rotation, {0.0f, 0.0f, 0.0f});
	Vector4 rotated = local * rotMat;
	return {rotated.x + center.x, rotated.y + center.y, rotated.z + center.z};
}


//=============================================================================
// 4頂点の四角形から表面法線を計算する
//=============================================================================
Vector3 Renderer::CalcQuadNormal(const Vector3& lb, const Vector3& rb, const Vector3& lt) {
	Vector3 edgeR = rb - lb;
	Vector3 edgeU = lt - lb;
	return Normalize(Cross(edgeR, edgeU));
}


//=============================================================================
// 球のジオメトリを生成（頂点・インデックス）
// 生成したジオメトリはsphereGeometryCache_にキャッシュされる
//=============================================================================
Renderer::SphereGeometry Renderer::GenerateSphereGeometry(int subdivision) {
	const float kPi = 3.14159265358979f;
	const float kLonEvery = kPi * 2.0f / static_cast<float>(subdivision);
	const float kLatEvery = kPi / static_cast<float>(subdivision);
	const int kVertPerRow = subdivision + 1;

	SphereGeometry geo;
	geo.vertices.reserve(kVertPerRow * kVertPerRow);
	geo.indices.reserve(subdivision * subdivision * 6);

	for (int latIndex = 0; latIndex <= subdivision; ++latIndex) {
		float lat = -kPi / 2.0f + kLatEvery * latIndex;
		float v = 1.0f - static_cast<float>(latIndex) / static_cast<float>(subdivision);
		for (int lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
			float lon = kLonEvery * lonIndex;
			float u = static_cast<float>(lonIndex) / static_cast<float>(subdivision);
			float x = std::cos(lat) * std::cos(lon);
			float y = std::sin(lat);
			float z = std::cos(lat) * std::sin(lon);
			geo.vertices.push_back({
			    {x, y, z, 1.0f},
                {u, v},
                {x, y, z}
            });
		}
	}

	for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
		for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			uint32_t lb = latIndex * kVertPerRow + lonIndex;
			uint32_t lt = (latIndex + 1) * kVertPerRow + lonIndex;
			uint32_t rb = latIndex * kVertPerRow + (lonIndex + 1);
			uint32_t rt = (latIndex + 1) * kVertPerRow + (lonIndex + 1);
			geo.indices.insert(geo.indices.end(), {lb, lt, rb, lt, rt, rb});
		}
	}

	return geo;
}


//=============================================================================
// 8頂点のボックスを6面(24頂点・36インデックス)のジオメトリに変換する
// 面ごとに外向き法線を計算し、cross(rb-lb, lt-lb)が外向きになる巻き順で生成する
// （裏面カリング D3D12_CULL_MODE_BACK と整合）
//=============================================================================
void Renderer::MakeBoxGeometry(const Vector3 corners[8], const Vector3& center, std::vector<Vertex3dData>& outVertices, std::vector<uint32_t>& outIndices) {
	// 各面の頂点を（lb, lt, rb, rt）の順で指定する
	// cross（rb - lb, lt - lb）が外向きになるようにする
	const int kFaces[6][4] = {
	    {1, 5, 3, 7}, // +x
	    {0, 2, 4, 6}, // -x
	    {2, 3, 6, 7}, // +Y
	    {0, 4, 1, 5}, // -Y
	    {4, 6, 5, 7}, // +Z
	    {0, 1, 2, 3}, // -Z
	};
	outVertices.clear();
	outIndices.clear();
	outVertices.reserve(24);
	outIndices.reserve(36);

	for (int f = 0; f < 6; ++f) {
		const Vector3& lb = corners[kFaces[f][0]];
		const Vector3& lt = corners[kFaces[f][1]];
		const Vector3& rb = corners[kFaces[f][2]];
		const Vector3& rt = corners[kFaces[f][3]];

		// ボックスでは「面中心 - ボックス中心」が外向き法線方向に一致する
		Vector3 faceCenter = {
		    (lb.x + lt.x + rb.x + rt.x) * 0.25f,
		    (lb.y + lt.y + rb.y + rt.y) * 0.25f,
		    (lb.z + lt.z + rb.z + rt.z) * 0.25f,
		};
		Vector3 normal = {faceCenter.x - center.x, faceCenter.y - center.y, faceCenter.z - center.z};
		float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
		if (len > 0.0001f) {
			normal.x /= len;
			normal.y /= len;
			normal.z /= len;
		}

		uint32_t base = static_cast<uint32_t>(outVertices.size());
		outVertices.push_back({
		    {lb.x, lb.y, lb.z, 1.0f},
            {0.0f, 1.0f},
            normal
        });
		outVertices.push_back({
		    {lt.x, lt.y, lt.z, 1.0f},
            {0.0f, 0.0f},
            normal
        });
		outVertices.push_back({
		    {rb.x, rb.y, rb.z, 1.0f},
            {1.0f, 1.0f},
            normal
        });
		outVertices.push_back({
		    {rt.x, rt.y, rt.z, 1.0f},
            {1.0f, 0.0f},
            normal
        });
		outIndices.insert(outIndices.end(), {base + 0, base + 2, base + 1, base + 2, base + 3, base + 1});
	}
}