#pragma once
#include <string>
#include <vector>

#include "MyEngine/Graphics/Renderer/DrawRequest.h"

// 前方宣言
class RenderWindow;

/// <summary>
/// 描画順の並び替え、リクエストの発行
/// </summary>
class RenderQueue {
public:
	RenderQueue(const RenderQueue&) = delete;
	RenderQueue& operator=(const RenderQueue&) = delete;

	static void Initialize();
	static void Release();

	// リクエストに積む
	static void Request(MeshRequest&& req);
	static void Request(LineRequest&& req);
	static void Request(SpriteRequest&& req);
	static void Request(ParticleRequest&& req);
	// 発行
	static void Flush2d(const std::wstring& windowTitle, RenderWindow* rw);
	static void FlushTransparentMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushOpaqueMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushParticle(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushLine(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	// フレーム末にクリア
	static void Clear();


private:
	RenderQueue() = default;
	~RenderQueue() = default;
	// MeshのRootSignature, PSO切り替え
	static void FlushMeshList(const std::vector<MeshRequest>& meshes, const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB, const char* name);

	static RenderQueue* instance_;
	std::vector<MeshRequest> transparentMeshRequests_;
	std::vector<MeshRequest> opaqueMeshRequests_;
	std::vector<SpriteRequest> spriteRequests_;
	std::vector<ParticleRequest> particleRequests_;
	std::vector<LineRequest> lineRequests_;
};
