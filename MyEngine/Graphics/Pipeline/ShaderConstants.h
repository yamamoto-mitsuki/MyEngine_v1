#pragma once
#include <cstdint>

#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector4.h"

// このファイルはShaderのレジスタに送る情報の構造体をまとめたファイル

//=============================================================================
// VS
//=============================================================================

// 座標変換
struct TransformationMatrixData {
	Matrix4x4 wvpMatrix = MakeIdentity4x4();
	Matrix4x4 worldMatrix = MakeIdentity4x4();
};

// カメラ
struct CameraData {
	Vector3 worldPosition;
	float padding = 0.0f;
};

// パーティクル
struct ParticleData {
	Matrix4x4 wvp; // ビルボード込みのWVP
	Matrix4x4 world;
	Vector4 color;
};

//=============================================================================
// PS
//=============================================================================

// Spriteのマテリアル
struct Material2dData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Matrix4x4 uvTransform = MakeIdentity4x4();
	uint32_t textureIndex = 0; // 使用するテクスチャのインデックス（0はテクスチャなし）
};

// パーティクルのマテリアル
struct MaterialParticleData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Matrix4x4 uvTransform = MakeIdentity4x4();
	uint32_t textureIndex = 0;
};

// モデル用のPS
struct Material3dData {
	Vector4 color;             // 色
	Matrix4x4 uvTransform;     // UV変換行列
	Vector3 ambient;           // Ka: 環境光色
	float padA = 0.0f;
	Vector3 diffuse;           // Kd: 拡散反射色
	float padB = 0.0f;
	Vector3 specular;          // Ks: 鏡面反射色
	float shininess;           // Ns: 鏡面反射指数
	Vector3 emissive;          // Ke: 自己発光色
	uint32_t textureIndex = 0; // 使用するテクスチャのインデックス（0はテクスチャなし）
	float metallic = 0.0f;     // 0=非金属 1=金属
	float roughness = 0.5f;    // 表面の粗さ（0=鏡面, 1=完全拡散）
	float padC = 0.0f;
	float padD = 0.0f;
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

// ポイントライト
struct PointLightData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 色
	Vector3 position = {0.0f, 0.0f, 0.0f}; // 位置
	float intensity = 1.0f; // 輝度
};