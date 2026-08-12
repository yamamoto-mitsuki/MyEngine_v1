#include "Camera.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"


//=============================================================================
// 初期化
//=============================================================================
void Camera::Initialize(float fovY, float aspectRatio, float nearZ, float farZ) {
	fovY_ = fovY;
	aspectRatio_ = aspectRatio;
	nearZ_ = nearZ;
	farZ_ = farZ;
	// 初期行列を計算しておく
	Update();
}



//=============================================================================
// 更新
//=============================================================================
void Camera::Update() {
	Matrix4x4 cameraMatrix = MakeAffineMatrix(transform_.scale, transform_.rotation, transform_.translation);
	viewMatrix_ = Inverse(cameraMatrix);
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);
}


//=============================================================================
// 視錐台のデバッグ表示
//=============================================================================
void Camera::DrawFrustum(Camera* viewCamera) const {
	// 早期リターン
	if (!viewCamera || viewCamera == this) {
		return; 
	}

	// --- 錐台の8頂点をNDCから逆変換で求める ---
	// farZ をそのまま使うと巨大になるので、手前を切った射影行列を作り直す
	Matrix4x4 proj = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, debugFarDistance);
	Matrix4x4 invViewProj = Inverse(viewMatrix_ * proj);
	// DirectXのNDCは x,y が -1〜1、z が 0〜1
	const Vector3 kNdc[8] = {
	    {-1.0f, -1.0f, 0.0f},
        {1.0f,  -1.0f, 0.0f},
        {1.0f,  1.0f,  0.0f},
        {-1.0f, 1.0f,  0.0f}, // 近平面
	    {-1.0f, -1.0f, 1.0f},
        {1.0f,  -1.0f, 1.0f},
        {1.0f,  1.0f,  1.0f},
        {-1.0f, 1.0f,  1.0f}, // 遠平面
	};
	Vector3 corner[8];
	for (int i = 0; i < 8; ++i) {
		corner[i] = MathUtility::TransformPoint(kNdc[i], invViewProj);
	}

	// --- 12本の辺 ---
	const int kEdge[12][2] = {
	    {0, 1},
        {1, 2},
        {2, 3},
        {3, 0}, // 近平面
	    {4, 5},
        {5, 6},
        {6, 7},
        {7, 4}, // 遠平面
	    {0, 4},
        {1, 5},
        {2, 6},
        {3, 7}, // 側面
	};
	Renderer::LineListConfig config;
	config.camera = viewCamera;
	config.color = debugColor;
	config.fadeStartDistance = 10000.0f; // フェードさせない
	config.fadeEndDistance = 20000.0f;
	for (const auto& e : kEdge) {
		config.lines.push_back({corner[e[0]], corner[e[1]], 0xFFFFFFFF});
	}

	// --- カメラ位置に十字 ---
	const Vector3& pos = transform_.translation;
	constexpr float kCrossSize = 1.0f;
	config.lines.push_back({
	    pos - Vector3{kCrossSize, 0.0f, 0.0f},
          pos + Vector3{kCrossSize, 0.0f, 0.0f},
          0xFFFFFFFF
    });
	config.lines.push_back({
	    pos - Vector3{0.0f, kCrossSize, 0.0f},
          pos + Vector3{0.0f, kCrossSize, 0.0f},
          0xFFFFFFFF
    });
	config.lines.push_back({
	    pos - Vector3{0.0f, 0.0f, kCrossSize},
          pos + Vector3{0.0f, 0.0f, kCrossSize},
          0xFFFFFFFF
    });

	Renderer::DrawLines(config);
}

Matrix4x4 Camera::CalcWVP(const Matrix4x4& worldMatrix) const {
	return Multiply(worldMatrix, Multiply(viewMatrix_, projectionMatrix_));
}