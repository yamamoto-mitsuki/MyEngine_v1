#pragma once
#include <string>
#include <memory>
#include <vector>
#include <variant>

#include "MyEngine/Graphics/Renderer/DrawRequest.h"

// 前方宣言
class RenderWindow;


/// <summary>
/// 描画順の並び替え、リクエストの発行
/// </summary>
class RenderQueue {
public:
	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class RenderQueue;
	};
	explicit RenderQueue(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	RenderQueue(const RenderQueue&) = delete;
	RenderQueue& operator=(const RenderQueue&) = delete;

	// 2Dのリクエスト1つ（スプライトか文字のどちらか
	using Request2d = std::variant<SpriteRequest, TextRequest>;

	// 初期化・解放
	static void Initialize();
	static void Release();
	// リクエストに積む
	static void Request(MeshRequest&& req);
	static void Request(LineRequest&& req);
	static void Request(SpriteRequest&& req);
	static void Request(TextRequest&& req);
	static void Request(ParticleRequest&& req);
	// 発行（2Dはスプライトと文字を、order の小さい順に混ぜて描く）
	static void Flush2d(const std::wstring& windowTitle);
	static void FlushTransparentMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushOpaqueMesh(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushParticle(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	static void FlushLine(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB);
	// フレーム末にクリア
	static void Clear();


private:
	~RenderQueue() = default;
	// MeshのRootSignature, PSO切り替え
	static void FlushMeshList(const std::vector<MeshRequest>& meshes, const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB, const char* name);

	// シングルトン
	friend struct std::default_delete<RenderQueue>;
	static std::unique_ptr<RenderQueue> instance_;

	std::vector<MeshRequest> transparentMeshRequests_;
	std::vector<MeshRequest> opaqueMeshRequests_;
	std::vector<Request2d> requests2d_;
	std::vector<ParticleRequest> particleRequests_;
	std::vector<LineRequest> lineRequests_;
};
