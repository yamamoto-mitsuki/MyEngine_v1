#pragma once
#include "MyEngine/Math/Vector3.h"

// 当たり判定に使用するOBB
struct OBB {
	Vector3 center;
	Vector3 axes[3];
};