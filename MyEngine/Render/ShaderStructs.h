#pragma once
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Math/Matrix4x4.h"

// シェーディングモデル
enum class ShadingModel {
	Unlit,       // Lightingなし
	Lambert,     // Lambert
	HalfLambert, // Half Lambert
};

// Object3d.VS.hlslのregister(b0)
struct TransformationMatrix {
	Matrix4x4 wvpMatrix = MakeIdentity4x4();
	Matrix4x4 worldMatrix = MakeIdentity4x4();
};

// PSのregister(b0)
struct Material {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Matrix4x4 uvTransform = MakeIdentity4x4();
};

// モデル用のPS
struct ModelMaterialCB {
	Vector4 color;         // 乗算色 (w=alpha)
	Matrix4x4 uvTransform; // UV変換行列
	Vector3 ambient;       // Ka: 環境光色
	float padA = 0.0f;
	Vector3 diffuse;       // Kd: 拡散反射色
	float padD = 0.0f;
	Vector3 specular;      // Ks: 鏡面反射色
	float shininess;       // Ns: 鏡面反射指数
	Vector3 emissive;      // Ke: 自己発光色
	float padE = 0.0f;
};

// VSのCBuffer
struct CameraDataCB {
	Vector3 worldPosition;
	float padding = 0.0f;
};

// 3D頂点データ構造
struct VertexData3D {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// 2D頂点データ構造
struct VertexData2D {
	Vector4 position;
	Vector2 texcoord;
};

// Line3D頂点データ構造
struct LineVertex {
	Vector4 position;
	Vector4 color;
};

// Line3D用マテリアル(b0, PS)
struct LineMaterialCB {
	Vector3 cameraWorldPos;  // カメラのワールド座標
	float fadeStartDistance; // フェード開始距離
	float fadeEndDistance;   // フェード終了距離(この距離でalpha=0)
	float pad0 = 0.0f;
	float pad1 = 0.0f;
	float pad2 = 0.0f;
};