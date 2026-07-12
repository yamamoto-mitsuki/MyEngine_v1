#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"

#include <format>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

#define SHADER_DIR L"MyEngine/Shader/" // シェーダーのファイルパスの記述を楽にするためのマクロ

// 静的メンバ変数
ShaderCompiler* ShaderCompiler::instance_ = nullptr;

namespace {
// シェーダーコンパイル時に必要な情報
struct ShaderInfo {
	const wchar_t* path;
	const wchar_t* profile;
};
// シェーダーのバージョン
constexpr const wchar_t* kVSProfile = L"vs_6_0";
constexpr const wchar_t* kPSProfile = L"ps_6_0";
// シェーダーファイルごとの情報（並び順は enum class ShaderFile と一致させる）
constexpr std::array<ShaderInfo, magic_enum::enum_count<ShaderFile>()> kShaderTable = {
    {
     // VS
        {SHADER_DIR L"Model/Object3d.VS.hlsl", kVSProfile},  // Object3dVS
        {SHADER_DIR L"Sprite/Sprite2d.VS.hlsl", kVSProfile}, // Sprite2dVS
        {SHADER_DIR L"Line/Line3d.VS.hlsl", kVSProfile},     // Line3dVS
        // PS
        {SHADER_DIR L"Model/Object3dLambert.PS.hlsl", kPSProfile},     // LambertPS
        {SHADER_DIR L"Model/Object3dHalfLambert.PS.hlsl", kPSProfile}, // HalfLambertPS
        {SHADER_DIR L"Model/Object3dNoLit.PS.hlsl", kPSProfile},       // UnlitPS
        {SHADER_DIR L"Sprite/Sprite2d.PS.hlsl", kPSProfile},           // Sprite2dPS
        {SHADER_DIR L"Line/Line3d.PS.hlsl", kPSProfile},               // Line3dPS
    }
};
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void ShaderCompiler::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initializeが2回以上呼び出されています");
	instance_ = new ShaderCompiler();

	LogManager::Log("Initialized");
}

// ===== 解放 =====
void ShaderCompiler::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// シェーダーファイルを取得
//=============================================================================
IDxcBlob* ShaderCompiler::GetShaderFile(ShaderFile file) {
	auto& cache = instance_->cache_[static_cast<size_t>(file)];
	// 登録済みの場合
	if (cache) {
		return cache.Get();
	}
	// 未登録の場合
	const ShaderInfo& info = kShaderTable[static_cast<size_t>(file)]; // シェーダーのファイルパス、バージョンを取得
	cache = CompileShader(info.path, info.profile);                   // コンパイル
	return cache.Get();
}


//=============================================================================
// すべてのファイルをコンパイル
//=============================================================================
void ShaderCompiler::CompileAll() { 
	for (ShaderFile file : magic_enum::enum_values<ShaderFile>()) {
		GetShaderFile(file);
	}
	LogManager::Log("All Shaders Compiled");
}


//=============================================================================
// DXCでコンパイル
//=============================================================================
Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const std::wstring& path, const wchar_t* profile) {
	auto utils = DirectXCommon::GetDxcUtils();
	auto compiler = DirectXCommon::GetDxcCompiler();
	auto includeHandler = DirectXCommon::GetIncludeHandler();
	
	// 1. hlslを読む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
	HRESULT hr = utils->LoadFile(path.c_str(), nullptr, &source);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "シェーダーファイルの読み込みに失敗しました");
	}
	DxcBuffer buffer{};
	buffer.Ptr = source->GetBufferPointer();
	buffer.Size = source->GetBufferSize();
	buffer.Encoding = DXC_CP_UTF8;

	// 2.コンパイル引数
	LPCWSTR args[] = {
	    path.c_str(),             // コンパイル対象
		L"-E", L"main",           // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile,           // ShaderProfileの設定 
		L"-Zi", L"-Qembed_debug", // デバック用の情報を埋め込む
		L"-Od",                   // 最適化を外しておく
		L"-Zpr",                  // メモリレイアウトは行優先
	};
	Microsoft::WRL::ComPtr<IDxcResult> result;
	hr = compiler->Compile(&buffer, args, _countof(args), includeHandler, IID_PPV_ARGS(&result));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "Compile呼び出しに失敗しました");
	}

	// 3.警告・エラー
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> error;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr);
	if (error != nullptr && error->GetStringLength() != 0) {
		LogManager::Error(error->GetStringPointer());
		MY_ASSERT_MSG(false, "シェーダーコンパイルエラー");
	}

	// 4.結果取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "シェーダーバイナリの取得に失敗しました");
	}

	LogManager::Log(ConvertString(std::format(L"Compile Succeeded : {}", path)));
	return shaderBlob;
}