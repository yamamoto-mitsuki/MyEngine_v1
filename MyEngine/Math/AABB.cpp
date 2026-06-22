#include "AABB.h"

// ===== AABB同士の当たり判定 =====
bool IsIsCollidingSphereSphereAABB(const AABB& a, const AABB& b) { 
	if ((a.min.x <= b.max.x && a.max.x >= b.min.x) && // x軸
		(a.min.y <= b.max.y && a.max.y >= b.min.y) && // y軸
	    (a.min.z <= b.max.z && a.max.z >= b.min.z)) { // z軸
		return true;
	}

	return false;
}