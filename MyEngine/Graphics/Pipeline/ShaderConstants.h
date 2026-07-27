#pragma once
#include <cstdint>

#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Graphics/IBL/IBLConfig.h"

// このファイルはShaderのレジスタに送る情報の構造体をまとめたファイル

//=============================================================================
// VS
//=============================================================================

// 座標変換
struct ObjectTransformData {
	Matrix4x4 worldMatrix = MakeIdentity4x4();
};

// カメラ
struct CameraData {
	Matrix4x4 viewProj;
	Vector3 worldPosition;
	float padA = 0.0f;
	Vector3 right; // ビルボード用
	float padB = 0.0f;
	Vector3 up; // ビルボード用
	float padC = 0.0f;
};

// パーティクル
struct ParticleData {
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

// IBLの索引
struct IBLParamsData {
	uint32_t irradianceIndex;
	uint32_t prefilterIndex;
	uint32_t brdfLutIndex;
	uint32_t prefilterMipCount = IBLConfig::kPrefilterMipCount;
	uint32_t enabled = 1;
	float pad[3];
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


// 1個分のポイントライト
struct PointLightData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 色
	Vector3 position = {0.0f, 0.0f, 0.0f}; // 位置
	float intensity = 1.0f; // 輝度
	float radius = 10.0f; // ライトの届く最大距離
	float decay = 1.0f;  // 減衰率
	float padA[2];
};
static constexpr uint32_t kMaxPointLights = 16; // ポイントライトの最大設置数
// 複数のポイントライトを管理
struct PointLightListData {
	PointLightData lights[kMaxPointLights];
	int32_t count = 0;
	float padA[3];
};


//=============================================================================
// 天球
//=============================================================================
struct SkyboxData {
	Matrix4x4 world;
	Matrix4x4 viewProj;
	float intensity = 1.0f;
	int cubeIndex;
	float padA[2];
};