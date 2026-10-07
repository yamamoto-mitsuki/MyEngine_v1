#pragma once
#include "MyEngine/Math/Geometry/AABB.h"
#include "MyEngine/Math/Geometry/Capsule.h"
#include "MyEngine/Math/Geometry/Sphere.h"
#include "MyEngine/Math/Geometry/Ray.h"
#include "MyEngine/Math/Geometry/Plane.h"

// 当たり判定をまとめた名前空間
namespace Collision {

/// <summary>
/// 押し出しで動かさない軸を判定する（true の軸へは押さない。横から見るゲームで奥行き(Z)へ押し出されないように）
/// </summary>
struct AxisLock {
	bool x = false;
	bool y = false;
	bool z = false;
};

// ===== 点の判定 =====
bool IsPointInSphere(const Vector3& point, const Sphere& sphere);
bool IsPointInAABB(const Vector3& point, const AABB& aabb);

// ===== 形状同士 =====
bool SphereSphere(const Sphere& a, const Sphere& b);
bool AABBAABB(const AABB& a, const AABB& b);
bool SphereAABB(const Sphere& sphere, const AABB& aabb);
bool CapsuleSphere(const Capsule& capsule, const Sphere& sphere);
bool CapsuleAABB(const Capsule& capsule, const AABB& aabb);

// ===== 押し出し =====
// a に push を足すと、a と b がちょうど触れた所になる。lock の軸へは押さない（その分、ほかの軸で押す）
bool PushOutSphereSphere(const Sphere& a, const Sphere& b, Vector3* push, AxisLock lock = {});
bool PushOutSphereAABB(const Sphere& a, const AABB& b, Vector3* push, AxisLock lock = {});
bool PushOutAABBSphere(const AABB& a, const Sphere& b, Vector3* push, AxisLock lock = {});
bool PushOutAABBAABB(const AABB& a, const AABB& b, Vector3* push, AxisLock lock = {});

// ===== レイキャスト t: レイの原点からの距離（不要ならnullptr） =====
bool RaySphere(const Ray& ray, const Sphere& sphere, float* t = nullptr);
bool RayAABB(const Ray& ray, const AABB& aabb, float* t = nullptr);
bool RayPlane(const Ray& ray, const Plane& plane, float* t = nullptr);

// ===== ユーティリティ =====
Vector3 RemoveLockedAxes(const Vector3& vector, AxisLock lock); // lock の軸の成分を0にする（その軸へは動かさない）
Vector3 ClosestPointOnAABB(const Vector3& point, const AABB& aabb);
Vector3 RayPoint(const Ray& ray, float t); // レイ上の点を取得
Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end); // 線分の上で point に一番近い点
}