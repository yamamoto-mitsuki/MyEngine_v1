#define NOMINMAX
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include <cmath>
#include <numbers>
#include <utility>
#include <algorithm>
#include <Windows.h>
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Light/PointLight.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/Renderer/DrawRequest.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Graphics/Renderer/RenderContext.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

// 静的メンバ変数
Renderer* Renderer::instance_ = nullptr;


//=============================================================================
// 共通作成部分
//=============================================================================
// ===== テクスチャ未指定(0)を白1x1に差し替える =====
static uint32_t ResolveTextureIndex(uint32_t textureHandle) { return (textureHandle != 0) ? textureHandle : TextureManager::GetWhiteTextureHandle(); }

// ===== Primitive Material作成ヘルパー =====
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

// ===== Model Materialの共通作成部分 =====
Material3dData Renderer::MakeModelMaterial(const ModelManager::MtlMaterial* mat, uint32_t color, const Transform& uvTransform) {
	// 色変換（0xRRGGBBAA → float4）
	float r = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(color & 0xFF) / 255.0f;
	// マテリアル構築
	Material3dData material;
	material.color = {r, g, b, a};
	material.uvTransform = MakeUVTransformMatrix(uvTransform);
	// 値が設定している場合
	if (mat) {
		material.ambient = mat->ambient;
		material.diffuse = mat->diffuse;
		material.specular = mat->specular;
		material.shininess = mat->shininess;
		material.emissive = mat->emissive;
		material.color.w *= mat->dissolve;
		material.metallic = mat->metallic;
		material.roughness = mat->roughness;
	} else {
		// MTLに該当マテリアルが無いときのデフォルト
		material.ambient = {0.2f, 0.2f, 0.2f};  // Ka: Ambient   環境光（影になっている部分の明るさ）
		material.diffuse = {1.0f, 1.0f, 1.0f};  // Kd: Diffuse   拡散反射（物体本来の色）
		material.specular = {1.0f, 1.0f, 1.0f}; // Ks: Specular  鏡面反射（ハイライトの強さ）
		material.shininess = 32.0f;             // Ns: Shininess 光沢（値が大きいほどハイライトが小さく鋭くなる）
		material.emissive = {0.0f, 0.0f, 0.0f}; // Ke: Emissive  自己発光（光源がなくても発光する色）
		material.metallic = 0.0f;
		material.roughness = 0.5f;
	}
	return material;
}

// ===== メッシュの共通作成部分 =====
template<class TConfig> 
void Renderer::PushMesh(const TConfig& config, std::vector<Vertex3dData>&& vertices, std::vector<uint32_t>&& indices, 
	const Matrix4x4& worldMatrix) {
	if (config.shadingType != ShadingType::Unlit) {
		MY_ASSERT_MSG(config.directionalLight != nullptr, "ShadingType::Unlit以外には光源を設置してください");
	}
	// --- 色変換 ---
	float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(config.color & 0xFF) / 255.0f;
	// --- bind情報、頂点データ ---
	MeshRequest req;
	req.vertices = std::move(vertices);
	req.indices = std::move(indices);
	req.materialData = MakeDefaultModelMaterial(r, g, b, a, config.uvTransform);
	req.materialData.textureIndex = ResolveTextureIndex(config.textureHandle);
	req.objectTransformData.worldMatrix = worldMatrix;
	req.objectTransformData.isBillboard = config.isBillboard ? 1u : 0u;
	req.directionalLightData = config.directionalLight ? config.directionalLight->GetData() : DirectionalLightData{};
	// ポイントライト
	if (config.pointLights) {
		uint32_t n = std::min((uint32_t)config.pointLights->size(), kMaxPointLights);
		for (uint32_t i = 0; i < n; ++i) {
			req.pointLightListData.lights[i] = (*config.pointLights)[i]->GetData();
		}
		req.pointLightListData.count = n;
	}

	req.shadingType = config.shadingType;
	req.blendMode = config.blendMode;
	req.rasterizerType = config.rasterizerType;
	req.depthMode = config.depthMode;
	req.windowTitle = config.windowTitle;
	// 半透明のソートに使うカメラとの距離
	Vector3 worldPos = {worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]};
	Vector3 cameraPos = config.camera ? config.camera->GetTranslation() : Vector3(0.0f,0.0f,0.0f);
	Vector3 d = cameraPos - worldPos;
	req.cameraDistanceSq = Dot(d, d); 
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
	req.materialData.textureIndex = ResolveTextureIndex(config.textureHandle);
	req.blendMode = config.blendMode;
	req.rasterizerType = config.rasterizerType;
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
	// 参照するモデル
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
	// SubMeshごとに1つのMeshRequest
	for (const ModelManager::SubMesh& mesh : asset->meshes) {
		MeshRequest req;
		// 静的ジオメトリ：GPU常駐バッファを指すだけ
		req.isStatic = true;
		req.vbv = mesh.vbv;
		req.ibv = mesh.ibv;
		req.indexCount = mesh.indexCount;
		// マテリアル構築
		const ModelManager::MtlMaterial* mat = ModelManager::GetMtlMaterial(config.modelHandle, mesh.materialName);
		req.materialData = MakeModelMaterial(mat, config.color, config.uvTransform);
		req.materialData.textureIndex = ResolveTextureIndex((config.textureHandle != 0) ? config.textureHandle : (mat ? mat->srvIndex : 0));
		req.objectTransformData.worldMatrix = worldMatrix;
		req.objectTransformData.isBillboard = config.isBillboard;
		req.cameraData.worldPosition = config.camera ? config.camera->GetTranslation() : Vector3{};
		req.iblParamsAddress = config.env ? config.env->GetParametersAddress() : 0;
		req.directionalLightData = config.directionalLight ? config.directionalLight->GetData() : DirectionalLightData{};
		// 参照分ポイントライトを設定
		if (config.pointLights) {
			uint32_t n = std::min((uint32_t)config.pointLights->size(), kMaxPointLights);
			for (uint32_t i = 0; i < n; ++i) {
				req.pointLightListData.lights[i] = (*config.pointLights)[i]->GetData();
			}
			req.pointLightListData.count = n;
		}
		// 描画設定
		req.shadingType = config.shadingType;
		req.blendMode = config.blendMode;
		req.rasterizerType = config.rasterizerType;
		req.depthMode = config.depthMode;
		req.windowTitle = config.windowTitle;
		req.debugName = &ModelManager::GetModelName(config.modelHandle);
		req.debugSubName = &mesh.materialName;

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
	// ワールド行列（ビルボードでも通常でも同じ作り方でOK。向きはVSが決める）
	Matrix4x4 worldMatrix = MakeAffineMatrix(config.transform.scale, config.transform.rotation, config.transform.translation);

	// 頂点はローカル座標のまま積む（VSがワールドへ変換する）
	std::vector<Vertex3dData> vertices;
	if (config.isBillboard) {
		// カメラを向くXY平面の板（±0.5）
		vertices = {
		    {{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		    {{-0.5f, +0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		    {{+0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
		    {{+0.5f, +0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
		};
	} else {
		// 水平なXZ平面の板
		vertices = {
		    {{-0.5f, 0.0f, +0.5f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		    {{-0.5f, 0.0f, -0.5f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		    {{+0.5f, 0.0f, +0.5f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
		    {{+0.5f, 0.0f, -0.5f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		};
	}
	PushMesh(config, std::move(vertices), {0, 1, 2, 1, 3, 2}, worldMatrix);
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


//=============================================================================
// Particle
//=============================================================================
void Renderer::DrawParticle(const ParticleConfig& config) {
	if (!config.particles || config.particles->empty()) {
		return;
	}
	MY_ASSERT_MSG(config.camera != nullptr, "パーティクルにはカメラを設定してください");

	// ConstantBuffer,StructuredBufferに送る情報を作成
	ParticleRequest req;
	req.instances.reserve(config.particles->size());
	for (const Particle& p : *config.particles) {
		ParticleData data;
		data.world = p.world;
		data.color = p.color; // フェード済みの色
		req.instances.push_back(data);
	}
	// グループマテリアル
	req.materialData.color = config.color;
	req.materialData.uvTransform = MakeUVTransformMatrix(config.uvTransform);
	req.materialData.textureIndex = ResolveTextureIndex(config.textureHandle);
	req.blendMode = config.blendMode;
	req.windowTitle = config.windowTitle;
	RenderQueue::Request(std::move(req));
}


//=============================================================================
// Sprite
//=============================================================================
// ===== Rect2d =====
void Renderer::DrawSprite(const SpriteConfig& config) {
	// テクスチャ原寸(px)を基準解像度上の大きさとして扱う
	Vector2 texSize = TextureManager::GetTextureSize(ResolveTextureIndex(config.textureHandle));
	float width = texSize.x * config.scale.x;
	float height = texSize.y * config.scale.y;
	// pivotを原点にしたローカル矩形（基準解像度のピクセル単位、Y+が上）
	float left = -config.pivot.x * width;
	float right = left + width;
	float top = config.pivot.y * height;
	float bottom = top - height;
	// 頂点データ
	Vector2 lb = {left, bottom};
	Vector2 lt = {left, top};
	Vector2 rb = {right, bottom};
	Vector2 rt = {right, top};
	if (config.rotate != 0.0f) {
		const Vector2 origin = {0.0f, 0.0f};
		lb = RotateAround2d(lb, origin, config.rotate);
		lt = RotateAround2d(lt, origin, config.rotate);
		rb = RotateAround2d(rb, origin, config.rotate);
		rt = RotateAround2d(rt, origin, config.rotate);
	}
	// 中心へ移動してから正規化座標へ
	Vector2 center = NdcToBasePixel(config.position);
	lb = BasePixelToNdc(lb + center);
	lt = BasePixelToNdc(lt + center);
	rb = BasePixelToNdc(rb + center);
	rt = BasePixelToNdc(rt + center);
	PushSprite(config, lb, lt, rb, rt, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f});
}

// ===== Quad2d =====
void Renderer::DrawQuad2d(const Quad2dConfig& config) {
	// 頂点情報
	Vector2 lb = config.lb, lt = config.lt, rb = config.rb, rt = config.rt;
	if (config.rotate != 0.0f) {
		// 正規化座標のまま回すと歪むので、ピクセル空間に直してから回す
		Vector2 lbPx = NdcToBasePixel(lb), ltPx = NdcToBasePixel(lt);
		Vector2 rbPx = NdcToBasePixel(rb), rtPx = NdcToBasePixel(rt);
		Vector2 center = {(lbPx.x + ltPx.x + rbPx.x + rtPx.x) * 0.25f, (lbPx.y + ltPx.y + rbPx.y + rtPx.y) * 0.25f};
		lb = BasePixelToNdc(RotateAround2d(lbPx, center, config.rotate));
		lt = BasePixelToNdc(RotateAround2d(ltPx, center, config.rotate));
		rb = BasePixelToNdc(RotateAround2d(rbPx, center, config.rotate));
		rt = BasePixelToNdc(RotateAround2d(rtPx, center, config.rotate));
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
	req.objectTransformData.worldMatrix = identity;
	req.vertices.reserve(config.lines.size() * 2);
	// 全体色は先に1回だけ分解しておく
	float baseR = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float baseG = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float baseB = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float baseA = static_cast<float>(config.color & 0xFF) / 255.0f;
	for (const LineSegment& seg : config.lines) {
		// 線ごとの色 × 全体色
		Vector4 col = {
		    static_cast<float>((seg.color >> 24) & 0xFF) / 255.0f * baseR,
		    static_cast<float>((seg.color >> 16) & 0xFF) / 255.0f * baseG,
		    static_cast<float>((seg.color >> 8) & 0xFF) / 255.0f * baseB,
		    static_cast<float>(seg.color & 0xFF) / 255.0f * baseA,
		};
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