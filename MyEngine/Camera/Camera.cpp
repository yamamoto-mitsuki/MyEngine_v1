#include "MyEngine/Camera/Camera.h"

void Camera::Init(float fovY, float aspectRatio, float nearZ, float farZ) {
	fovY_ = fovY;
	aspectRatio_ = aspectRatio;
	nearZ_ = nearZ;
	farZ_ = farZ;
	// 初期行列を計算しておく
	Update();
}

void Camera::Update() {
	Matrix4x4 cameraMatrix = MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	viewMatrix_ = Inverse(cameraMatrix);
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);
}

Matrix4x4 Camera::CalcWVP(const Matrix4x4& worldMatrix) const {
	return Multiply(worldMatrix, Multiply(viewMatrix_, projectionMatrix_));
}