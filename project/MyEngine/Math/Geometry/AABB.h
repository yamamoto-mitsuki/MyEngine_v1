#pragma once
#include "MyEngine/Math/Vector3.h"

// 当たり判定に使うAABB
struct AABB {
	Vector3 min; // 最小点
	Vector3 max; // 最大点
};

// AABB同士の当たり判定
bool IsIsCollidingSphereSphereAABB(const AABB& a, const AABB& b);