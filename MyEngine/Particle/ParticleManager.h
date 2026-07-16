#pragma once
#include <list>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Math/MathIncludes.h"

// 前方宣言
class Camera;

// 1個分のデータ
struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
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
	static void Release();
	static void Update();
	static void Draw();

	/// <summary>
	/// パーティクルを1個登録する
	/// </summary>
	/// <param name="p"></param>
	static void Register(const Particle& p);

	// セッター
	static void SetCamera(Camera* camera) { instance_->camera_ = camera; }

private:
	static ParticleManager* instance_;

	Camera* camera_;

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_; // StructerdBuffer本体
	ParticleData* mappedInstances_ = nullptr;               // 永続マップポインタ
	uint32_t srvSlot_ = 0;                                  // StructuredBufferのSRVHeap番号
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};            // StrucuredBufferのSRVHeapのGPU開始位置
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vbv_{};
	D3D12_INDEX_BUFFER_VIEW ibv_{};

	std::list<Particle> particles_;
	uint32_t instanceCount_; // 何個呼び出すカウント
};