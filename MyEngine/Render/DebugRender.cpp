#include "MyEngine/Render/DebugRender.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Render/DirectionalLight.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Render/RenderWindow.h"
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Math/Vector4.h"
#include <Windows.h>
#include <cassert>
#include <cmath>

// 静的メンバ変数
std::vector<DebugRender::TriangleConfig> DebugRender::requestsTriangle_;
std::vector<DebugRender::Rect2dConfig> DebugRender::requestsRect2d_;
std::vector<DebugRender::Rect3dConfig> DebugRender::requestsRect3d_;
std::vector<DebugRender::Quad2dConfig> DebugRender::requestsQuad2d_;
std::vector<DebugRender::Quad3dConfig> DebugRender::requestsQuad3d_;
std::vector<DebugRender::SphereConfig> DebugRender::requestsSphere3d_;
std::vector<DebugRender::LineListConfig>  DebugRender::requestsLines_;
std::unordered_map<int, DebugRender::SphereGeometry> DebugRender::sphereGeometryCache_;

// ===== 標準のMaterialData =====
static ModelMaterialCB MakeDefaultModelMaterial(float r, float g, float b, float a, const Transform& uvTransform) {
	ModelMaterialCB mat;
	mat.color = {r, g, b, a};
	mat.uvTransform = MakeUVTransformMatrix(uvTransform);
	mat.ambient = {0.0f, 0.0f, 0.0f};
	mat.diffuse = {1.0f, 1.0f, 1.0f};
	mat.specular = {0.0f, 0.0f, 0.0f};
	mat.shininess = 1.0f;
	mat.emissive = {0.0f, 0.0f, 0.0f};
	return mat;
}

// ===== インスタンスの取得 =====
DebugRender& DebugRender::GetInstance() {
	static DebugRender instance;
	return instance;
}

// ===== 描画リクエスト追加 =====
void DebugRender::DrawTriangle(const TriangleConfig& config) { requestsTriangle_.push_back(config); }
void DebugRender::DrawRect2d(const Rect2dConfig& config) { requestsRect2d_.push_back(config); }
void DebugRender::DrawRect3d(const Rect3dConfig& config) { requestsRect3d_.push_back(config); }
void DebugRender::DrawQuad2d(const Quad2dConfig& config) { requestsQuad2d_.push_back(config); }
void DebugRender::DrawQuad3d(const Quad3dConfig& config) { requestsQuad3d_.push_back(config); }
void DebugRender::DrawSphere(const SphereConfig& config) { requestsSphere3d_.push_back(config); }
void DebugRender::DrawLines(const LineListConfig& config) { requestsLines_.push_back(config); }

// ===== 全3Dを描画 =====
void DebugRender::Flush3d(const std::wstring& windowTitle, RenderContext* ctx) {
	FlushTriangle(windowTitle, ctx);
	FlushSphere(windowTitle, ctx);
	FlushRect3d(windowTitle, ctx);
	FlushQuad3d(windowTitle, ctx);
	FlushLines(windowTitle, ctx);
}

// ===== 全2Dを描画 =====
void DebugRender::Flush2d(const std::wstring& windowTitle, RenderContext* ctx, RenderWindow* rw) {
	FlushQuad2d(windowTitle, ctx, rw);
	FlushRect2d(windowTitle, ctx, rw);
}

// ===== 描画リクエストをクリア =====
void DebugRender::ClearRequests() {
	requestsTriangle_.clear();
	requestsRect2d_.clear();
	requestsRect3d_.clear();
	requestsQuad2d_.clear();
	requestsQuad3d_.clear();
	requestsSphere3d_.clear();
	requestsLines_.clear();
}

//======================================================================================================
// カメラに影響する全三角形を描画
//======================================================================================================
void DebugRender::FlushTriangle(const std::wstring& windowTitle, RenderContext* ctx) {
	for (const TriangleConfig& req : requestsTriangle_) {
		if (req.windowTitle != windowTitle)
			continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			assert(req.directionalLight != nullptr && "ShadingModel::Unlit以外には光源を設置してください");
		}
		// 色
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;
		// ワールド座標
		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);
		// 描画設定
		RenderContext::DrawModelDesc desc;
		desc.vertices = {
		    {{req.top.x, req.top.y, req.top.z, 1.0f},       req.uvTop,   {0.0f, 0.0f, 1.0f}},
		    {{req.right.x, req.right.y, req.right.z, 1.0f}, req.uvRight, {0.0f, 0.0f, 1.0f}},
		    {{req.left.x, req.left.y, req.left.z, 1.0f},    req.uvLeft,  {0.0f, 0.0f, 1.0f}},
		};
		desc.indices = {0, 1, 2};
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;
		desc.matrices.worldMatrix = worldMatrix;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.srvIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::SetShadingModel(req.shadingModel);
		ctx->DrawModel(desc);
	}
}

//======================================================================================================
// 全2d矩形を描画
//======================================================================================================
void DebugRender::FlushRect2d(const std::wstring& windowTitle, RenderContext* ctx, RenderWindow* rw) {
	for (const Rect2dConfig& req : requestsRect2d_) {
		if (req.windowTitle != windowTitle) {
			continue;
		}
		// 色
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;
		// position(中心)から4頂点を計算
		float hw = req.size.x * 0.5f;
		float hh = req.size.y * 0.5f;
		Vector2 vLB = {req.position.x - hw, req.position.y + hh};
		Vector2 vLT = {req.position.x - hw, req.position.y - hh};
		Vector2 vRB = {req.position.x + hw, req.position.y + hh};
		Vector2 vRT = {req.position.x + hw, req.position.y - hh};
		// Z軸回転
		if (req.rotate != 0.0f) {
			vLB = RotateAround2d(vLB, req.position, req.rotate);
			vLT = RotateAround2d(vLT, req.position, req.rotate);
			vRB = RotateAround2d(vRB, req.position, req.rotate);
			vRT = RotateAround2d(vRT, req.position, req.rotate);
		}
		// 描画設定
		RenderContext::DrawSpriteDesc desc;
		desc.material.color = {r, g, b, a};
		desc.material.uvTransform = MakeUVTransformMatrix(req.uvTransform);
		desc.srvIndex = req.srvIndex;
		desc.vertices[0] = {{vLB.x, vLB.y, 0.0f, 1.0f}, {0.0f, 1.0f}};
		desc.vertices[1] = {{vLT.x, vLT.y, 0.0f, 1.0f}, {0.0f, 0.0f}};
		desc.vertices[2] = {{vRB.x, vRB.y, 0.0f, 1.0f}, {1.0f, 1.0f}};
		desc.vertices[3] = {{vRT.x, vRT.y, 0.0f, 1.0f}, {1.0f, 0.0f}};
		ctx->DrawSprite(desc, rw);
	}
}

//======================================================================================================
// 全3d矩形を描画
//======================================================================================================
void DebugRender::FlushRect3d(const std::wstring& windowTitle, RenderContext* ctx) {
	for (const Rect3dConfig& req : requestsRect3d_) {
		if (req.windowTitle != windowTitle) continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			assert(req.directionalLight != nullptr && "ShadingModel::Unlit以外には光源を設置してください");
		}
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >>  8) & 0xFF) / 255.0f;
		float a = static_cast<float>( req.color        & 0xFF) / 255.0f;
		// ローカル空間の4頂点(scale={1,1,1}のとき±0.5のXZ平面上の板)
		Vector4 localLB = {-0.5f, 0.0f,  0.5f, 1.0f};
		Vector4 localLT = {-0.5f, 0.0f, -0.5f, 1.0f};
		Vector4 localRB = { 0.5f, 0.0f,  0.5f, 1.0f};
		Vector4 localRT = { 0.5f, 0.0f, -0.5f, 1.0f};
		// scale・rotation・translationをまとめて適用
		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);
		// 各頂点にワールド行列を掛けてワールド座標に変換
		auto toWorld = [&](const Vector4& v) -> Vector3 {
			Vector4 result = v * worldMatrix;
			return { result.x, result.y, result.z };
		};
		Vector3 lb = toWorld(localLB);
		Vector3 lt = toWorld(localLT);
		Vector3 rb = toWorld(localRB);
		Vector3 rt = toWorld(localRT);
		// 法線を計算(lb→rb × lb→lt の外積)
		Vector3 edgeR = {rb.x - lb.x, rb.y - lb.y, rb.z - lb.z};
		Vector3 edgeU = {lt.x - lb.x, lt.y - lb.y, lt.z - lb.z};
		Vector3 normal = {
			edgeR.y * edgeU.z - edgeR.z * edgeU.y,
			edgeR.z * edgeU.x - edgeR.x * edgeU.z,
			edgeR.x * edgeU.y - edgeR.y * edgeU.x,
		};
		float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
		if (len > 0.0001f) { 
			normal.x /= len;
			normal.y /= len; 
			normal.z /= len; 
		}

		// 頂点座標はワールド変換済みなのでDrawModel側のWorldMatrixは単位行列
		Matrix4x4 identity = MakeIdentity4x4();
		RenderContext::DrawModelDesc desc;
		desc.vertices = {
			{{lb.x, lb.y, lb.z, 1.0f}, {0.0f, 1.0f}, normal},
			{{lt.x, lt.y, lt.z, 1.0f}, {0.0f, 0.0f}, normal},
			{{rb.x, rb.y, rb.z, 1.0f}, {1.0f, 1.0f}, normal},
			{{rt.x, rt.y, rt.z, 1.0f}, {1.0f, 0.0f}, normal},
		};
		desc.indices = {0, 1, 2, 1, 3, 2};
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(identity) : identity;
		desc.matrices.worldMatrix = identity;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.srvIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::SetShadingModel(req.shadingModel);
		ctx->DrawModel(desc);
	}
}

//======================================================================================================
// 全2D自由四角形を描画
//======================================================================================================
void DebugRender::FlushQuad2d(const std::wstring& windowTitle, RenderContext* ctx, RenderWindow* rw) {
	for (const Quad2dConfig& req : requestsQuad2d_) {
		if (req.windowTitle != windowTitle)
			continue;
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;
		Vector2 lb = req.lb;
		Vector2 lt = req.lt;
		Vector2 rb = req.rb;
		Vector2 rt = req.rt;
		// rotate != 0 のとき重心を中心にZ軸回転
		if (req.rotate != 0.0f) {
			Vector2 center = {
				(lb.x + lt.x + rb.x + rt.x) * 0.25f,
				(lb.y + lt.y + rb.y + rt.y) * 0.25f,
			};
			lb = RotateAround2d(lb, center, req.rotate);
			lt = RotateAround2d(lt, center, req.rotate);
			rb = RotateAround2d(rb, center, req.rotate);
			rt = RotateAround2d(rt, center, req.rotate);
		}

		RenderContext::DrawSpriteDesc desc;
		desc.material.color = {r, g, b, a};
		desc.material.uvTransform = MakeUVTransformMatrix(req.uvTransform);
		desc.srvIndex = req.srvIndex;
		desc.vertices[0] = {{lb.x, lb.y, 0.0f, 1.0f}, req.uvLb};
		desc.vertices[1] = {{lt.x, lt.y, 0.0f, 1.0f}, req.uvLt};
		desc.vertices[2] = {{rb.x, rb.y, 0.0f, 1.0f}, req.uvRb};
		desc.vertices[3] = {{rt.x, rt.y, 0.0f, 1.0f}, req.uvRt};
		ctx->DrawSprite(desc, rw);
	}
}

//======================================================================================================
// 全3D自由四角形を描画
//======================================================================================================
void DebugRender::FlushQuad3d(const std::wstring& windowTitle, RenderContext* ctx) {
	for (const Quad3dConfig& req : requestsQuad3d_) {
		if (req.windowTitle != windowTitle) continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			assert(req.directionalLight != nullptr && "ShadingModel::Unlit以外には光源を設置してください");
		}
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >>  8) & 0xFF) / 255.0f;
		float a = static_cast<float>( req.color        & 0xFF) / 255.0f;
		Vector3 lb = req.lb;
		Vector3 lt = req.lt;
		Vector3 rb = req.rb;
		Vector3 rt = req.rt;
		// rotate != 0 のとき重心を中心にXYZ回転
		if (req.rotate.x != 0.0f || req.rotate.y != 0.0f || req.rotate.z != 0.0f) {
			Vector3 center = {
				(lb.x + lt.x + rb.x + rt.x) * 0.25f,
				(lb.y + lt.y + rb.y + rt.y) * 0.25f,
				(lb.z + lt.z + rb.z + rt.z) * 0.25f,
			};
			lb = RotateAround3d(lb, center, req.rotate);
			lt = RotateAround3d(lt, center, req.rotate);
			rb = RotateAround3d(rb, center, req.rotate);
			rt = RotateAround3d(rt, center, req.rotate);
		}
		// 法線を計算(lb→rb × lb→lt の外積)
		Vector3 edgeR = {rb.x - lb.x, rb.y - lb.y, rb.z - lb.z};
		Vector3 edgeU = {lt.x - lb.x, lt.y - lb.y, lt.z - lb.z};
		Vector3 normal = {
			edgeR.y * edgeU.z - edgeR.z * edgeU.y,
			edgeR.z * edgeU.x - edgeR.x * edgeU.z,
			edgeR.x * edgeU.y - edgeR.y * edgeU.x,
		};
		float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
		if (len > 0.0001f) { normal.x /= len; normal.y /= len; normal.z /= len; }

		// 頂点座標はワールド空間直指定なのでWorldMatrixは単位行列
		Matrix4x4 worldMatrix = MakeIdentity4x4();
		RenderContext::DrawModelDesc desc;
		desc.vertices = {
			{{lb.x, lb.y, lb.z, 1.0f}, req.uvLb, normal},
			{{lt.x, lt.y, lt.z, 1.0f}, req.uvLt, normal},
			{{rb.x, rb.y, rb.z, 1.0f}, req.uvRb, normal},
			{{rt.x, rt.y, rt.z, 1.0f}, req.uvRt, normal},
		};
		desc.indices = {0, 1, 2, 1, 3, 2};
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;
		desc.matrices.worldMatrix = worldMatrix;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.srvIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::SetShadingModel(req.shadingModel);
		ctx->DrawModel(desc);
	}
}

//======================================================================================================
// 全Line3Dを描画
//======================================================================================================
void DebugRender::FlushLines(const std::wstring& windowTitle, RenderContext* ctx) {
	for (const LineListConfig& req : requestsLines_) {
		if (req.windowTitle != windowTitle) continue;
		if (req.lines.empty()) continue;

		Vector3 camPos = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};

		LineMaterialCB mat;
		mat.cameraWorldPos = camPos;
		mat.fadeStartDistance = req.fadeStartDistance;
		mat.fadeEndDistance = req.fadeEndDistance;

		Matrix4x4 worldMatrix = MakeIdentity4x4();
		Matrix4x4 wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;

		RenderContext::DrawLines3dDesc desc;
		desc.material = mat;
		desc.matrices.wvpMatrix = wvpMatrix;
		desc.matrices.worldMatrix = worldMatrix;

		desc.vertices.reserve(req.lines.size() * 2);
		for (const LineSegment& seg : req.lines) {
			float r = static_cast<float>((seg.color >> 24) & 0xFF) / 255.0f;
			float g = static_cast<float>((seg.color >> 16) & 0xFF) / 255.0f;
			float b = static_cast<float>((seg.color >>  8) & 0xFF) / 255.0f;
			float a = static_cast<float>( seg.color        & 0xFF) / 255.0f;
			Vector4 col = {r, g, b, a};
			desc.vertices.push_back({{seg.start.x, seg.start.y, seg.start.z, 1.0f}, col});
			desc.vertices.push_back({{seg.end.x,   seg.end.y,   seg.end.z,   1.0f}, col});
		}

		ctx->DrawLines3d(desc);
	}
}

//======================================================================================================
// 全球描画
//======================================================================================================
void DebugRender::FlushSphere(const std::wstring& windowTitle, RenderContext* ctx) {
	for (const SphereConfig& req : requestsSphere3d_) {
		if (req.windowTitle != windowTitle)
			continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			assert(req.directionalLight != nullptr && "ShadingModel::Unlit以外には光源を設置してください");
		}
		// 色
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;
		// 頂点取得
		auto it = sphereGeometryCache_.find(req.subdivision);
		if (it == sphereGeometryCache_.end()) {
			sphereGeometryCache_[req.subdivision] = GenerateSphereGeometry(req.subdivision);
			it = sphereGeometryCache_.find(req.subdivision);
		}
		const SphereGeometry& geo = it->second;
		// ワールド座標
		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);
		// 描画設定
		RenderContext::DrawModelDesc desc;
		desc.vertices = geo.vertices;
		desc.indices = geo.indices;
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;
		desc.matrices.worldMatrix = worldMatrix;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.srvIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::SetShadingModel(req.shadingModel);
		ctx->DrawModel(desc);
	}
}

//======================================================================================================
// 2D頂点1つをcenterを原点としてZ軸回転させる
//======================================================================================================
Vector2 DebugRender::RotateAround2d(const Vector2& point, const Vector2& center, float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	float ox = point.x - center.x;
	float oy = point.y - center.y;
	return {ox * c - oy * s + center.x, ox * s + oy * c + center.y};
}

//======================================================================================================
// 3D頂点1つをcenterを原点としてXYZ回転させる
//======================================================================================================
Vector3 DebugRender::RotateAround3d(const Vector3& point, const Vector3& center, const Vector3& rotation) {
	// centerを原点に移動
	Vector4 local = {
		point.x - center.x,
		point.y - center.y,
		point.z - center.z,
		1.0f};
	// 回転行列を生成(scale={1,1,1}, translation={0,0,0})
	Matrix4x4 rotMat = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, rotation, {0.0f, 0.0f, 0.0f});
	Vector4 rotated = local * rotMat;
	// centerを元に戻す
	return {rotated.x + center.x, rotated.y + center.y, rotated.z + center.z};
}


//======================================================================================================
// 球の頂点生成
//======================================================================================================
DebugRender::SphereGeometry DebugRender::GenerateSphereGeometry(int subdivision) {
	const float kPi = 3.14159265358979f;
	const float kLonEvery = kPi * 2.0f / static_cast<float>(subdivision);
	const float kLatEvery = kPi / static_cast<float>(subdivision);
	const int kVertexCountPerRow = subdivision + 1;

	SphereGeometry geo;
	geo.vertices.reserve(kVertexCountPerRow * kVertexCountPerRow);
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
			uint32_t leftBottom = latIndex * kVertexCountPerRow + lonIndex;
			uint32_t leftTop = (latIndex + 1) * kVertexCountPerRow + lonIndex;
			uint32_t rightBottom = latIndex * kVertexCountPerRow + (lonIndex + 1);
			uint32_t rightTop = (latIndex + 1) * kVertexCountPerRow + (lonIndex + 1);
			geo.indices.push_back(leftBottom);
			geo.indices.push_back(leftTop);
			geo.indices.push_back(rightBottom);
			geo.indices.push_back(leftTop);
			geo.indices.push_back(rightTop);
			geo.indices.push_back(rightBottom);
		}
	}

	return geo;
}