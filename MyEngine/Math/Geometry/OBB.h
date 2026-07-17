#pragma once
#include "MyEngine/Math/Vector3.h"

// 当たり判定に使用するOBB
struct OBB {
	Vector3 center;  // 中心点
	Vector3 axes[3] = {
	    {1.0f, 0.0f, 0.0f},
        {0.0f,1.0f,0.0f},
		{0.0f,0.0f,1.0f},
    }; // 座標軸。正規化・直交必須
	Vector3 size;    // 座標方向の長さの半分。中心から面までの距離
};