#pragma once
#include <string>
#include <memory>
#include "MyEngine/Graphics/RenderTarget/RenderTextureCube.h"


/// <summary>
/// 
/// </summary>
class IBLEnvironment {
public:
	static void Initlaize();
	static void Finalize();

	/// <summary>
	/// 静的環境マップ 
	/// </summary>
	/// <param name="hdrPath">.hdrのパス</param>
	static void MakeFromHDR(const std::string& hdrPath);

	static RenderTextureCube* GetIrradiance();


private:
	static IBLEnvironment* instance_;
	// 所有リソース
	

};
