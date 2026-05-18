#pragma once
#include "MyEngine/Math/Matrix4x4.h"

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

Vector4 operator*(const Vector4& m1, const Matrix4x4& m2);