#pragma once
#include "MyEngine/Math/AABB.h"
#include "MyEngine/Math/Sphere.h"
#include "MyEngine/Math/Ray.h"
#include "MyEngine/Math/Plane.h"

namespace Collision {
// ===== 点の判定 =====
bool IsPointInSphere(const Vector3& point, const Sphere& sphere);
bool IsPointInAABB(const Vector3& point, const AABB& aabb);

// ===== 形状同士 =====
bool SphereSphere(const Sphere& a, const Sphere& b);
bool AABBAABB(const AABB& a, const AABB& b);
bool SphereAABB(const Sphere& sphere, const AABB& aabb);

// ===== レイキャスト t: レイの原点からの距離（不要ならnullptr） =====
bool RaySphere(const Ray& ray, const Sphere& sphere, float* t = nullptr);
bool RayAABB(const Ray& ray, const AABB& aabb, float* t = nullptr);
bool RayPlane(const Ray& ray, const Plane& plane, float* t = nullptr);

// ===== ユーティリティ =====
Vector3 ClosestPointOnAABB(const Vector3& point, const AABB& aabb);
Vector3 RayPoint(const Ray& ray, float t); // レイ上の点を取得
}