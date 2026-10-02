#pragma once
#include "MyEngine/Math/Geometry/AABB.h"
#include "MyEngine/Math/Geometry/Capsule.h"
#include "MyEngine/Math/Geometry/Sphere.h"
#include "MyEngine/Math/Geometry/Ray.h"
#include "MyEngine/Math/Geometry/Plane.h"

// 当たり判定をまとめた名前空間
namespace Collision {
// ===== 点の判定 =====
bool IsPointInSphere(const Vector3& point, const Sphere& sphere);
bool IsPointInAABB(const Vector3& point, const AABB& aabb);

// ===== 形状同士 =====
bool SphereSphere(const Sphere& a, const Sphere& b);
bool AABBAABB(const AABB& a, const AABB& b);
bool SphereAABB(const Sphere& sphere, const AABB& aabb);
bool CapsuleSphere(const Capsule& capsule, const Sphere& sphere);
bool CapsuleAABB(const Capsule& capsule, const AABB& aabb);

// ===== レイキャスト t: レイの原点からの距離（不要ならnullptr） =====
bool RaySphere(const Ray& ray, const Sphere& sphere, float* t = nullptr);
bool RayAABB(const Ray& ray, const AABB& aabb, float* t = nullptr);
bool RayPlane(const Ray& ray, const Plane& plane, float* t = nullptr);

// ===== ユーティリティ =====
Vector3 ClosestPointOnAABB(const Vector3& point, const AABB& aabb);
Vector3 RayPoint(const Ray& ray, float t); // レイ上の点を取得
Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end); // 線分の上で point に一番近い点
}