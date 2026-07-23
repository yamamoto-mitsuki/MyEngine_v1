#pragma once
#include <string>
#include <memory>
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/RenderTarget/RenderTextureCube.h"
#include "MyEngine/Graphics/IBL/CubeBakePass.h"


/// <summary>
/// 
/// </summary>
class IBLEnvironment {
public:
	/// <summary>
	/// 静的環境マップ 
	/// </summary>
	/// <param name="hdrPath">.hdrのパス</param>
	void MakeFromHDR(const std::string& hdrPath);

	RenderTextureCube* GetEnvironment() const { return env_.get(); }
	RenderTextureCube* GetIrradiance() const { return irradiance_.get(); }
	RenderTextureCube* GetPrefilter() const { return prefilter_.get(); }
	RenderTexture* GetBrdfLUT() const { return brdfLut_.get(); }

private:
	// 所有リソース
	std::unique_ptr<RenderTextureCube> env_, irradiance_, prefilter_;
	std::unique_ptr<RenderTexture> brdfLut_;
};
