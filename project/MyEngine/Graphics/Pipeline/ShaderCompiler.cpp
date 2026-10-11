#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/String/ConvertString.h"
#include <format>

// 静的メンバ変数
ShaderCompiler* ShaderCompiler::instance_ = nullptr;


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
const ShaderReflection& ShaderCompiler::CompileShaderReflection(const std::wstring& path, const std::wstring& profile, const std::wstring& entry) {
	auto& cache = instance_->cache_;
	// 既にあるならキャッシュ
	if (auto it = cache.find(path); it != cache.end()) {
		return it->second;
	}
	// コンパイルしてルートシグニチャなど取得
	Microsoft::WRL::ComPtr<IDxcResult> result = CompileShader(kShaderGeneratedDir + path, profile, entry);
	ShaderReflection refl;
	refl.blob = GetShaderBlob(result.Get()); // エラーチェック込み
	refl.resources = MakeShaderResourceBinding(result.Get());
	refl.inputs = MakeShaderInputParameter(result.Get());
	return cache.emplace(path, std::move(refl)).first->second;
}


//=============================================================================
// シェーダーコンパイル
//=============================================================================
Microsoft::WRL::ComPtr<IDxcResult> ShaderCompiler::CompileShader(const std::wstring& fullPath, const std::wstring& profile, const std::wstring& entry) {
	auto utils = DirectXCommon::GetDxcUtils();
	auto compiler = DirectXCommon::GetDxcCompiler();
	auto includeHandler = DirectXCommon::GetIncludeHandler();

	// --- hlslを読む ---
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
	HRESULT hr = utils->LoadFile(fullPath.c_str(), nullptr, &source);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "シェーダーファイルの読み込みに失敗しました");
	}
	DxcBuffer buffer{};
	buffer.Ptr = source->GetBufferPointer();
	buffer.Size = source->GetBufferSize();
	buffer.Encoding = DXC_CP_UTF8;

	// --- コンパイル引数 ---
	LPCWSTR args[] = {
	    fullPath.c_str(),       // コンパイル対象
	    L"-E", entry.c_str(),   // エントリーポイントの指定。基本的にmain以外にはしない
	    L"-T", profile.c_str(), // ShaderProfileの設定
	    L"-I", kShaderGeneratedDir, // 生成先＝検索パス。常に一致する
	    L"-Zi",                 // 
	    L"-Qembed_debug",       // デバック用の情報を埋め込む
	    L"-Od",                 // 最適化を外しておく
	    L"-Zpr",                // メモリレイアウトは行優先
	};
	Microsoft::WRL::ComPtr<IDxcResult> result;
	hr = compiler->Compile(&buffer, args, _countof(args), includeHandler, IID_PPV_ARGS(&result));
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "Compile呼び出しに失敗しました");
	}

	return result;
}

//=============================================================================
// シェーダーコンパイル結果からShaderBlobを取得する
//=============================================================================
Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::GetShaderBlob(IDxcResult* result) {
	HRESULT hr;
	// 警告・エラーチェック
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> error;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr);
	if (error != nullptr && error->GetStringLength() != 0) {
		LogManager::Error(error->GetStringPointer());
		MY_ASSERT_MSG(false, "シェーダーコンパイルエラー");
	}
	// 結果取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	if (FAILED(hr)) {
		LogManager::Error(std::format("Error Code: 0x{:08X}", (uint32_t)hr));
		MY_ASSERT_MSG(false, "シェーダーバイナリの取得に失敗しました");
	}

	return shaderBlob;
}

//=============================================================================
// シェーダーコンパイルの結果からレジスタなど情報を取得する
//=============================================================================
std::vector<ShaderResourceBinding> ShaderCompiler::MakeShaderResourceBinding(IDxcResult* result) {
	auto utils = DirectXCommon::GetDxcUtils();
	// コンパイル結果を取得
	Microsoft::WRL::ComPtr<IDxcBlob> reflBlob;
	HRESULT hr = result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflBlob), nullptr);
	if (FAILED(hr) || reflBlob == nullptr) {
		MY_ASSERT_MSG(false, "リフレクション情報の取得に失敗（直前のシェーダーコンパイルエラーを確認）");
		return {};
	}
	// DxcBuffer生成
	DxcBuffer buf{reflBlob->GetBufferPointer(), reflBlob->GetBufferSize(), DXC_CP_ACP};
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflect;
	utils->CreateReflection(&buf, IID_PPV_ARGS(&reflect));
	// Shader情報取得
	D3D12_SHADER_DESC shaderDesc{};
	reflect->GetDesc(&shaderDesc);

	// --- 取得した情報を ShaderResourceBinding にいれる ---
	std::vector<ShaderResourceBinding> binds;
	// Bind情報の数分ループ
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
		reflect->GetResourceBindingDesc(i, &bindDesc);
		binds.push_back({
		    bindDesc.Name,      // シェーダー内の変数名（例: gCamera）
		    bindDesc.Type,      // リソース種別（例: CBV, SRV, SAMPLER...）
		    bindDesc.BindPoint, // レジスタ番号（例: t0 → 0を返す）
		    bindDesc.BindCount, // バインド数（例: Tetxture2D tex[]なら0, ないなら1）
		    bindDesc.Space,     // レジスタスペース番号（例： space0なら0）
		    bindDesc.Dimension, // テクスチャの形状（2D, Cubeなど）
		});
		LogManager::Log(std::format("{} type={} reg={} space={} count={}", bindDesc.Name, (int)bindDesc.Type, bindDesc.BindPoint, bindDesc.Space, bindDesc.BindCount));
	}

	return binds;
}

//=============================================================================
// シェーダーコンパイルの結果から入力パラメータを取得する
//=============================================================================
std::vector<ShaderInputParameter> ShaderCompiler::MakeShaderInputParameter(IDxcResult* result) {
	auto utils = DirectXCommon::GetDxcUtils();
	// コンパイル結果を取得
	Microsoft::WRL::ComPtr<IDxcBlob> reflBlob;
	result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflBlob), nullptr);
	// DxcBuffer生成
	DxcBuffer buf{reflBlob->GetBufferPointer(), reflBlob->GetBufferSize(), DXC_CP_ACP};
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflect;
	utils->CreateReflection(&buf, IID_PPV_ARGS(&reflect));
	// Shader情報取得
	D3D12_SHADER_DESC shaderDesc{};
	reflect->GetDesc(&shaderDesc);

	// --- 取得した情報を ShaderInputParameter にいれる ---
	std::vector<ShaderInputParameter> inputs;
	// 入力パラメータ分ループ
	for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc{};
		reflect->GetInputParameterDesc(i, &paramDesc);

		inputs.push_back({
		    paramDesc.SemanticName,    // POSITION
		    paramDesc.SystemValueType, // SV_Positionなど
		    paramDesc.SemanticIndex,   // TEXCOORD0 の 0
		    paramDesc.Register,        // Register番号
		    paramDesc.ComponentType,   // Float32など
		    paramDesc.Mask,            // xyzw
		    paramDesc.ReadWriteMask    // 実際に使用する成分
		});
		LogManager::Log(std::format("{} ", paramDesc.SemanticName));
	}

	return inputs;
}