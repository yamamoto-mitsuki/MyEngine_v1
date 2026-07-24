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
	static void FlushMesh(const std::wstring& windowTitle);
	static void FlushParticle(const std::wstring& windowTitle);
	static void FlushLine(const std::wstring& windowTitle);
	// フレーム末にクリア
	static void Clear();


private:
	RenderQueue() = default;
	~RenderQueue() = default;
	// インスタンス
	static RenderQueue* instance_;

	std::vector<MeshRequest> meshRequests_;
	std::vector<SpriteRequest> spriteRequests_;
	std::vector<ParticleRequest> particleRequests_;
	std::vector<LineRequest> lineRequests_;
};
