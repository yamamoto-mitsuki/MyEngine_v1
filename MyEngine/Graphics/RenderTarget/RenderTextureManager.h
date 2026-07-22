#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"


/// <summary>
/// 作成したRenderTextureを管理するクラス 
/// </summary>
class RenderTextureManager {
public:
	struct Handle {
	private: 
		static constexpr uint32_t kInvalid = 0xFFFFFFFF; // ハンドルの初回登録を判断するための無効値

	public:
		uint32_t index = kInvalid;
		bool IsValid() const { return index != kInvalid; }
	};

	// コピー・ムーブ禁止
	RenderTextureManager(const RenderTextureManager&) = delete;
	RenderTextureManager& operator=(const RenderTextureManager&) = delete;
	// 初期化・解放
	static void Initialize();
	static void Finalize();

	/// <summary>
	/// 生成する。名前で管理
	/// </summary>
	static Handle Create(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth = true);

	/// <summary>
	/// 取得
	/// </summary>
	static RenderTexture* Get(Handle hanle);

private:
	RenderTextureManager() = default;
	~RenderTextureManager() = default;
	static RenderTextureManager* instance_;
	// 添え字が Handle::index
	std::vector<std::unique_ptr<RenderTexture>> renderTextures_;
};