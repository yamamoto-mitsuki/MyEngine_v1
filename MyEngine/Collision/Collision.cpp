#include "MyEngine/Collision/Collision.h"

#include <cmath>
#include <chrono>
#include <algorithm>

#include "MyEngine/Collision/Profiling/CollisionProfiler.h"



// COLLISION_PROFILING を定義している間だけ計測する
#ifdef _DEBUG
struct ScopeTimer_ {
	const char* name_;
	std::chrono::high_resolution_clock::time_point start_;
	explicit ScopeTimer_(const char* n) : name_(n), start_(std::chrono::high_resolution_clock::now()) {}
	~ScopeTimer_() {
		double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start_).count();
		CollisionProfiler::Record(name_, ms);
	}
};
#define COLLISION_PROFILE() ScopeTimer_ _t_(__FUNCTION__)
#endif

namespace Collision {
//=============================================================================
// 点の判定
//=============================================================================
bool IsPointInSphere(const Vector3& point, const Sphere& sphere) { return LengthSq(point - sphere.center) <= sphere.radius * sphere.radius; }

bool IsPointInAABB(const Vector3& point, const AABB& aabb) {
	return point.x >= aabb.min.x && point.x <= aabb.max.x && // x
		   point.y >= aabb.min.y && point.y <= aabb.max.y && // y
		   point.z >= aabb.min.z && point.z <= aabb.max.z;   // z
}

//=============================================================================
// 形状同士
//=============================================================================

// ===== 球同士 =====
bool SphereSphere(const Sphere& a, const Sphere& b) {
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	float r = a.radius + b.radius;
	return LengthSq(a.center - b.center) <= r * r;
}

// ===== AABB同士 =====
bool AABBAABB(const AABB& a, const AABB& b) { 
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	return a.min.x <= b.max.x && a.max.x >= b.min.x && 
		   a.min.y <= b.max.y && a.max.y >= b.min.y && 
		   a.min.z <= b.max.z && a.max.z >= b.min.z; 
}

// ===== 球とAABB =====
bool SphereAABB(const Sphere& sphere, const AABB& aabb) {
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	Vector3 closest = ClosestPointOnAABB(sphere.center, aabb);
	return LengthSq(sphere.center - closest) <= sphere.radius * sphere.radius;
}

//=============================================================================
// レイキャスト
//=============================================================================

// ===== 球 =====
bool RaySphere(const Ray& ray, const Sphere& sphere, float* t) {
	// 直線（Rayの軌跡）の式 StartPos + dist * Direction と 球の式 X^2 + Y^2 + Z^2 = r^2 を変化させる
	// 
	// r^2を左辺に移動させると
	// X^2 + Y^2 + Z^2 - r^2 = 0
	// 球の式に直線の式を合わせると
	// X: StartPos.x + dist * Direction.x
	// Y: StartPos.y + dist * Direction.y
	// Z: StartPos.z + dist * Direction.z
	// (StartPos.x + dist * Direction.x)^2 + (StartPos.y + dist * Direction.y)^2 + (StartPos.z + dist * Direction.z)^2 - r^2 = 0
	// 
	// ここでStartPos...P, dist...t, Direction...D とする
	// (P.x + t * D.x)^2を展開すると
	// (P.x)^2 + (2 * P.x * t * D.x) + (t * D.x)^2 のようになる。y,zも展開する。
	// すると t^2 , t が発生するので
	// (D.x^2 + D.y^2 + D.z^2) * t^2 + 2 * (P.x * D.x + P.y * D.y + P.z * D.z) * t + (P.x^2 + P.y^2 + P.z^2 - r^2) = 0
	// 
	// よって ax^2 + bx + c = 0 という二次方程式になる。
	// そして二次方程式の解 √b^2 - 4ac という中身を求めて - なら √ が成立しないので false を返す
	//
	// t (dist) は「当たるまでの時間(距離)」で、まだ未知数だから前半の係数計算には登場しない。
	// 光線は前方へ無限の長さで伸びている前提（後からtの長さで制限可能）。
	//
	// a = dirの長さ^2。バグで単位ベクトル以外が来ても壊れないための安全弁。
	// b = 2 * Dot(oc, dir)。(P + tD)^2 を展開した時の公式の「2ab」の2。
	// c = ocの長さ^2 - 半径^2。
	//
	// disc (判別式: b^2 - 4ac) は解の公式の「ルートの中身」。
	// マイナスなら現実世界に答えがない（＝外れた）ので、std::sqrtを計算する前に秒でreturnする。

#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	Vector3 oc = ray.origin - sphere.center; 
	float a = Dot(ray.direction, ray.direction);
	float b = 2.0f * Dot(oc, ray.direction);
	float c = Dot(oc, oc) - sphere.radius * sphere.radius;
	float disc = b * b - 4.0f * a * c;
	if (disc < 0.0f) { return false; }
	if (t) { *t = (-b - std::sqrtf(disc)) / (2.0f * a); }
	return true;
}

// ===== AABB =====
bool RayAABB(const Ray& ray, const AABB& aabb, float* t) {
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	// スラブ法
	float tmin = 0.0f;
	float tmax = 1e30f;

	const float* org  = &ray.origin.x;
	const float* dir  = &ray.direction.x;
	const float* bmin = &aabb.min.x;
	const float* bmax = &aabb.max.x;

	for (int i = 0; i < 3; ++i) {
		if (std::abs(dir[i]) < 1e-8f) {
			// rayの速度（単位ベクトルの長さ）が0に近く、AABBの幅にないなら終了
			if (org[i] < bmin[i] || org[i] > bmax[i]) {
				return false;
			}
		} else {
			// 距離 / 速さ = 時間
			float t1 = (bmin[i] - org[i]) / dir[i];
			float t2 = (bmax[i] - org[i]) / dir[i];
			// 逆方向から飛んできてたら入れ替える
			if (t1 > t2) {
				std::swap(t1, t2);
			}
			// 入口: 一番遅い時間に更新
			// 出口: 一番速い時間に更新
			tmin = std::max(tmin, t1);
			tmax = std::min(tmax, t2);
			if (tmin > tmax) {
				return false;
			}
		}
	}
	if (t) {
		*t = tmin;
	}
	return true;
}

// ===== 平面 =====
bool RayPlane(const Ray& ray, const Plane& plane, float* t) { 
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	float denom = Dot(plane.normal, ray.direction); 
	if (std::abs(denom) < 1e-8f) {
		return false;
	}
	float tVal = (plane.distance - Dot(plane.normal, ray.origin)) / denom;
	if (tVal < 0.0f) {
		return false;
	}
	if (t) {
		*t = tVal;
	}
	return true;
}

//=============================================================================
// ユーティリティ
//=============================================================================

Vector3 ClosestPointOnAABB(const Vector3& point, const AABB& aabb) {
	return {
		std::max(aabb.min.x, std::min(point.x, aabb.max.x)), 
		std::max(aabb.min.y, std::min(point.y, aabb.max.y)), 
		std::max(aabb.min.z, std::min(point.z, aabb.max.z))
	};
}

Vector3 RayPoint(const Ray& ray, float t) { return ray.origin + ray.direction * t; }

}