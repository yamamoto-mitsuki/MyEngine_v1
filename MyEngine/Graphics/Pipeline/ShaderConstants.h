#pragma once
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector4.h"
#include <cstdint>

// このファイルはShaderのレジスタに送る情報の構造体をまとめたファイル

//=============================================================================
// 全共通データ
//=============================================================================

// Object3d.VS.hlslのregister(b0)
struct TransformationMatrixData {
	Matrix4x4 wvpMatrix = MakeIdentity4x4();
	Matrix4x4 worldMatrix = MakeIdentity4x4();
};

// VSに送るカメラ情報
struct CameraData {
	Vector3 worldPosition;
	float padding = 0.0f;
};

//=============================================================================
// マテリアルデータ
//=============================================================================

// PSのregister(b0)
struct Material2dData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Matrix4x4 uvTransform = MakeIdentity4x4();
	uint32_t textureIndex = 0; // 使用するテクスチャのインデックス（0はテクスチャなし）
};

// モデル用のPS
struct Material3dData {
	Vector4 color;         // 乗算色 (w=alpha)
	Matrix4x4 uvTransform; // UV変換行列
	Vector3 ambient;       // Ka: 環境光色
	float padA = 0.0f;
	Vector3 diffuse; // Kd: 拡散反射色
	float padD = 0.0f;
	Vector3 specular;          // Ks: 鏡面反射色
	float shininess;           // Ns: 鏡面反射指数
	Vector3 emissive;          // Ke: 自己発光色
	uint32_t textureIndex = 0; // 使用するテクスチャのインデックス（0はテクスチャなし）
};

// Line3D用マテリアル(b0, PS)
struct MaterialLineData {
	Vector3 cameraWorldPos;  // カメラのワールド座標
	float fadeStartDistance; // フェード開始距離
	float fadeEndDistance;   // フェード終了距離(この距離でalpha=0)
	float pad0 = 0.0f;
	float pad1 = 0.0f;
	float pad2 = 0.0f;
};

//=============================================================================
// 光源
//=============================================================================

// 平行光源
struct DirectionalLightData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 direction = {0.0f, -1.0f, 0.0f};
	float intensity = 1.0f;
};