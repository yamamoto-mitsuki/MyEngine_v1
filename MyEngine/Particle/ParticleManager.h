#pragma once

#include <wrl.h>
#include <d3d12.h>

#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"


// 1個分のデータ
struct Particle {
	Transform transform;
	Vector3 velocity;
	uint32_t color;
	float lifeTime;
	float currentTime;
};


/// <summary>
/// バッファ / SRV / 描画を持つシングルトン
/// </summary>
class ParticleManager {
public:
	// 最大数
	static constexpr uint32_t kMaxParticles = 4096;

	static void Initialize();
	static void Update();
	static void Draw();

private:
	static ParticleManager* instance_;

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_; // StructerdBuffer本体
	ParticleData* mappedInstances_ = nullptr; // 永続マップポインタ
	uint32_t instanceCount_;
	uint32_t srvSlot_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
};