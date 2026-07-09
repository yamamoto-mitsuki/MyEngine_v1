#include "MyEngine/Render/PrimitiveRenderer.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Render/Core/RenderContext.h"
#include "MyEngine/Render/Core/RenderWindow.h"
#include "MyEngine/Render/Core/ShaderStructs.h"
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cmath>

PrimitiveRenderer* PrimitiveRenderer::instance_ = nullptr;

//=============================================================================
// デフォルトモデルマテリアル生成ヘルパー
//=============================================================================
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

//=============================================================================
// ShadingModelでソートするコンパレータ
// Flush関数内でPSO切り替え回数を最小化するために使う
// 同じShadingModelの描画をまとめることでSetShadingModel()の呼び出しを削減する
//=============================================================================
template<typename T> static void SortByShadingModel(std::vector<T>& requests) {
	std::sort(requests.begin(), requests.end(), [](const T& a, const T& b) {
		// ShadingModelの数値順（Unlit=0, Lambert=1, HalfLambert=2...）でソート
		// a < bがtrueのとき、aを前に置く
		return static_cast<int>(a.shadingModel) < static_cast<int>(b.shadingModel);
	});
}

//=============================================================================
// 初期化
//=============================================================================
void PrimitiveRenderer::Initialize() {
	assert(instance_ == nullptr && "PrimitiveRenderer::Initialize()が2回以上呼び出されています。");
	instance_ = new PrimitiveRenderer();
}

//=============================================================================
// 描画リクエスト追加
//=============================================================================
void PrimitiveRenderer::DrawTriangle(const TriangleConfig& config) { instance_->requestsTriangle_.push_back(config); }
void PrimitiveRenderer::DrawRect2d(const Rect2dConfig& config) { instance_->requestsRect2d_.push_back(config); }
void PrimitiveRenderer::DrawRect3d(const Rect3dConfig& config) { instance_->requestsRect3d_.push_back(config); }
void PrimitiveRenderer::DrawQuad2d(const Quad2dConfig& config) { instance_->requestsQuad2d_.push_back(config); }
void PrimitiveRenderer::DrawQuad3d(const Quad3dConfig& config) { instance_->requestsQuad3d_.push_back(config); }
void PrimitiveRenderer::DrawAABB(const AABBConfig& config) { instance_->requestsAABB_.push_back(config); }
void PrimitiveRenderer::DrawOBB(const OBBConfig& config) { instance_->requestsOBB_.push_back(config); }
void PrimitiveRenderer::DrawSphere(const SphereConfig& config) { instance_->requestsSphere3d_.push_back(config); }
void PrimitiveRenderer::DrawLines(const LineListConfig& config) { instance_->requestsLines_.push_back(config); }

//=============================================================================
// 全3Dを描画（WindowManagerのPreRenderAllから呼ぶ）
//=============================================================================
void PrimitiveRenderer::Flush3d(const std::wstring& windowTitle) {
	FlushTriangle(windowTitle);
	FlushSphere(windowTitle);
	FlushRect3d(windowTitle);
	FlushQuad3d(windowTitle);
	FlushAABB(windowTitle);
	FlushOBB(windowTitle);
	FlushLines(windowTitle);
}

//=============================================================================
// 全2Dを描画（WindowManagerのPreRenderAllから呼ぶ）
//=============================================================================
void PrimitiveRenderer::Flush2d(const std::wstring& windowTitle, RenderWindow* rw) {
	FlushQuad2d(windowTitle, rw);
	FlushRect2d(windowTitle, rw);
}

//=============================================================================
// 描画リクエストをクリア（PostRenderAllで呼ぶ）
//=============================================================================
void PrimitiveRenderer::ClearRequests() {
	instance_->requestsTriangle_.clear();
	instance_->requestsRect2d_.clear();
	instance_->requestsRect3d_.clear();
	instance_->requestsQuad2d_.clear();
	instance_->requestsQuad3d_.clear();
	instance_->requestsAABB_.clear();
	instance_->requestsOBB_.clear();
	instance_->requestsSphere3d_.clear();
	instance_->requestsLines_.clear();
}

//=============================================================================
// 3D三角形を描画
// ShadingModelでソートしてPSO切り替え回数を最小化する
//=============================================================================
void PrimitiveRenderer::FlushTriangle(const std::wstring& windowTitle) {
	// ShadingModelでソートしてPSO切り替え回数を最小化
	SortByShadingModel(instance_->requestsTriangle_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1); // 無効値で初期化

	for (const TriangleConfig& req : instance_->requestsTriangle_) {
		// ウィンドウの名前が一致しないかつデフォルトの名前ではない場合スキップ
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}

		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}
		// ShadingModelが変わったときだけPSO・RootSignatureを切り替える
		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}

		// 色
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		// ワールド行列
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
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// 2D矩形を描画
//=============================================================================
void PrimitiveRenderer::FlushRect2d(const std::wstring& windowTitle, RenderWindow* rw) {
	for (const Rect2dConfig& req : instance_->requestsRect2d_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"")
			continue;

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

		if (req.rotate != 0.0f) {
			vLB = RotateAround2d(vLB, req.position, req.rotate);
			vLT = RotateAround2d(vLT, req.position, req.rotate);
			vRB = RotateAround2d(vRB, req.position, req.rotate);
			vRT = RotateAround2d(vRT, req.position, req.rotate);
		}

		RenderContext::DrawSpriteDesc desc;
		desc.material.color = {r, g, b, a};
		desc.material.uvTransform = MakeUVTransformMatrix(req.uvTransform);
		desc.material.textureIndex = req.srvIndex;
		desc.vertices[0] = {
		    {vLB.x, vLB.y, 0.0f, 1.0f},
            {0.0f, 1.0f}
        };
		desc.vertices[1] = {
		    {vLT.x, vLT.y, 0.0f, 1.0f},
            {0.0f, 0.0f}
        };
		desc.vertices[2] = {
		    {vRB.x, vRB.y, 0.0f, 1.0f},
            {1.0f, 1.0f}
        };
		desc.vertices[3] = {
		    {vRT.x, vRT.y, 0.0f, 1.0f},
            {1.0f, 0.0f}
        };
		RenderContext::DrawSprite(desc, rw);
	}
}

//=============================================================================
// 3D矩形を描画
// ShadingModelでソートしてPSO切り替え回数を最小化する
//=============================================================================
void PrimitiveRenderer::FlushRect3d(const std::wstring& windowTitle) {
	SortByShadingModel(instance_->requestsRect3d_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1);
	for (const Rect3dConfig& req : instance_->requestsRect3d_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"")
			continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}

		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}

		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		// ローカル空間の4頂点（XZ平面上の板）
		Vector4 localLB = {-0.5f, 0.0f, 0.5f, 1.0f};
		Vector4 localLT = {-0.5f, 0.0f, -0.5f, 1.0f};
		Vector4 localRB = {0.5f, 0.0f, 0.5f, 1.0f};
		Vector4 localRT = {0.5f, 0.0f, -0.5f, 1.0f};

		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);
		auto toWorld = [&](const Vector4& v) -> Vector3 {
			Vector4 result = v * worldMatrix;
			return {result.x, result.y, result.z};
		};
		Vector3 lb = toWorld(localLB);
		Vector3 lt = toWorld(localLT);
		Vector3 rb = toWorld(localRB);
		Vector3 rt = toWorld(localRT);

		// 法線を計算（lb→rb × lb→lt の外積）
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

		// 頂点座標はワールド変換済みなのでWorldMatrixは単位行列
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
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// 2D自由四角形を描画
//=============================================================================
void PrimitiveRenderer::FlushQuad2d(const std::wstring& windowTitle, RenderWindow* rw) {
	for (const Quad2dConfig& req : instance_->requestsQuad2d_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"")
			continue;

		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		Vector2 lb = req.lb, lt = req.lt, rb = req.rb, rt = req.rt;
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
		desc.material.textureIndex = req.srvIndex;
		desc.vertices[0] = {
		    {lb.x, lb.y, 0.0f, 1.0f},
            req.uvLb
        };
		desc.vertices[1] = {
		    {lt.x, lt.y, 0.0f, 1.0f},
            req.uvLt
        };
		desc.vertices[2] = {
		    {rb.x, rb.y, 0.0f, 1.0f},
            req.uvRb
        };
		desc.vertices[3] = {
		    {rt.x, rt.y, 0.0f, 1.0f},
            req.uvRt
        };
		RenderContext::DrawSprite(desc, rw);
	}
}

//=============================================================================
// 3D自由四角形を描画
// ShadingModelでソートしてPSO切り替え回数を最小化する
//=============================================================================
void PrimitiveRenderer::FlushQuad3d(const std::wstring& windowTitle) {
	SortByShadingModel(instance_->requestsQuad3d_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1);
	for (const Quad3dConfig& req : instance_->requestsQuad3d_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}

		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}

		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		Vector3 lb = req.lb, lt = req.lt, rb = req.rb, rt = req.rt;
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

		// 法線を計算（lb→rb × lb→lt の外積）
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
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// AABBを描画
//=============================================================================
void PrimitiveRenderer::FlushAABB(const std::wstring& windowTitle) {
	SortByShadingModel(instance_->requestsAABB_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1);
	for (const AABBConfig& req : instance_->requestsAABB_) {
		// ウィンドウ名確認
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// ライト有無確認
		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}
		// シェーディング
		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}
		// 16進数で指定した色を変換
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		const AABB& box = req.aabb;
		Vector3 center = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f};
		// 8頂点（bit0=X,bit1=Y,bit2=Z / 0=min,1=max）
		Vector3 corners[8];
		for (int i = 0; i < 8; ++i) {
			corners[i] = {
			    (i & 1) ? box.max.x : box.min.x,
			    (i & 2) ? box.max.y : box.min.y,
			    (i & 4) ? box.max.z : box.min.z,
			};
		}

		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
		MakeBoxGeometry(corners, center, vertices, indices);

		// 頂点座標はワールド空間直指定なのでWorldMatrixは単位行列
		Matrix4x4 identity = MakeIdentity4x4();
		RenderContext::DrawModelDesc desc;
		desc.vertices = vertices;
		desc.indices = indices;
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(identity) : identity;
		desc.matrices.worldMatrix = identity;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// OBBを描画
//=============================================================================
void PrimitiveRenderer::FlushOBB(const std::wstring& windowTitle) {
	SortByShadingModel(instance_->requestsOBB_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1);
	for (const OBBConfig& req : instance_->requestsOBB_) {
		// ウィンドウ名確認
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// ライトの有無
		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}
		// シェーディング
		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}
		// 色
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		const OBB& obb = req.obb;
		// 8頂点 = center ± axis*size（bit0=X軸,bit1=Y軸,bit2=Z軸 / 0=-,1=+）
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

		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
		MakeBoxGeometry(corners, obb.center, vertices, indices);

		Matrix4x4 identity = MakeIdentity4x4();
		RenderContext::DrawModelDesc desc;
		desc.vertices = vertices;
		desc.indices = indices;
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(identity) : identity;
		desc.matrices.worldMatrix = identity;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;

		MY_ASSERT_MSG(desc.vertices.size() == 24, "OBB vertex count unexpected");
		MY_ASSERT_MSG(desc.indices.size() == 36, "OBB index count unexpected");

		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// Line3Dを描画
//=============================================================================
void PrimitiveRenderer::FlushLines(const std::wstring& windowTitle) {
	for (const LineListConfig& req : instance_->requestsLines_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		if (req.lines.empty()) {
			continue;
		}
		Vector3 camPos = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};

		MaterialLineData mat;
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
			float b = static_cast<float>((seg.color >> 8) & 0xFF) / 255.0f;
			float a = static_cast<float>(seg.color & 0xFF) / 255.0f;
			Vector4 col = {r, g, b, a};
			desc.vertices.push_back({
			    {seg.start.x, seg.start.y, seg.start.z, 1.0f},
                col
            });
			desc.vertices.push_back({
			    {seg.end.x, seg.end.y, seg.end.z, 1.0f},
                col
            });
		}
		RenderContext::DrawLines3d(desc);
	}
}

//=============================================================================
// 球を描画
// ShadingModelでソートしてPSO切り替え回数を最小化する
//=============================================================================
void PrimitiveRenderer::FlushSphere(const std::wstring& windowTitle) {
	SortByShadingModel(instance_->requestsSphere3d_);

	ShadingModel currentModel = static_cast<ShadingModel>(-1);
	for (const SphereConfig& req : instance_->requestsSphere3d_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}

		if (req.shadingModel != ShadingModel::Unlit) {
			MY_ASSERT_MSG(req.directionalLight != nullptr, "ShadingModel::Unlit以外には光源を設置してください");
		}

		if (req.shadingModel != currentModel) {
			RenderContext::SetShadingModel(req.shadingModel);
			currentModel = req.shadingModel;
		}

		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		// 球のジオメトリをキャッシュから取得（なければ生成）
		auto it = instance_->sphereGeometryCache_.find(req.subdivision);
		if (it == instance_->sphereGeometryCache_.end()) {
			instance_->sphereGeometryCache_[req.subdivision] = GenerateSphereGeometry(req.subdivision);
			it = instance_->sphereGeometryCache_.find(req.subdivision);
		}
		const SphereGeometry& geo = it->second;

		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);

		RenderContext::DrawModelDesc desc;
		desc.vertices = geo.vertices;
		desc.indices = geo.indices;
		desc.material = MakeDefaultModelMaterial(r, g, b, a, req.uvTransform);
		desc.matrices.wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;
		desc.matrices.worldMatrix = worldMatrix;
		desc.cameraData.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};
		desc.material.textureIndex = req.srvIndex;
		desc.directionalLight = req.directionalLight;
		RenderContext::DrawModel(desc);
	}
}

//=============================================================================
// 2D頂点をcenterを原点としてZ軸回転させる
//=============================================================================
Vector2 PrimitiveRenderer::RotateAround2d(const Vector2& point, const Vector2& center, float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	float ox = point.x - center.x;
	float oy = point.y - center.y;
	return {ox * c - oy * s + center.x, ox * s + oy * c + center.y};
}

//=============================================================================
// 3D頂点をcenterを原点としてXYZ回転させる
//=============================================================================
Vector3 PrimitiveRenderer::RotateAround3d(const Vector3& point, const Vector3& center, const Vector3& rotation) {
	Vector4 local = {point.x - center.x, point.y - center.y, point.z - center.z, 1.0f};
	// 回転行列（scale={1,1,1}, translation={0,0,0}）
	Matrix4x4 rotMat = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, rotation, {0.0f, 0.0f, 0.0f});
	Vector4 rotated = local * rotMat;
	return {rotated.x + center.x, rotated.y + center.y, rotated.z + center.z};
}

//=============================================================================
// 球のジオメトリを生成（頂点・インデックス）
// 生成したジオメトリはsphereGeometryCache_にキャッシュされる
//=============================================================================
PrimitiveRenderer::SphereGeometry PrimitiveRenderer::GenerateSphereGeometry(int subdivision) {
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
void PrimitiveRenderer::MakeBoxGeometry(const Vector3 corners[8], const Vector3& center, std::vector<Vertex3dData>& outVertices, std::vector<uint32_t>& outIndices) {
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