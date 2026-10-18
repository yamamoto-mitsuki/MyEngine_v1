#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/String/ConvertString.h"
#include <format>

// 静的メンバ変数
std::unique_ptr<ShaderCompiler> ShaderCompiler::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void ShaderCompiler::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initializeが2回以上呼び出されています");
	instance_ = std::make_unique<ShaderCompiler>(ConstructorKey());
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void ShaderCompiler::Release() {
	instance_.reset();
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
	// リフレクションは1回だけ作り、3つの情報を読む
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection = CreateReflection(result.Get());
	refl.resources = MakeShaderResourceBinding(reflection.Get());
	refl.inputs = MakeShaderInputParameter(reflection.Get());
	// CSの[numthreads(x, y, z)]。VS・PSでは 0 が入る
	reflection->GetThreadGroupSize(&refl.threadGroupSize[0], &refl.threadGroupSize[1], &refl.threadGroupSize[2]);
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
// シェーダーコンパイルの結果から、中身を調べる道具（リフレクション）を作る
//=============================================================================
Microsoft::WRL::ComPtr<ID3D12ShaderReflection> ShaderCompiler::CreateReflection(IDxcResult* result) {
	// コンパイル結果から、リフレクション用の部分を取り出す
	Microsoft::WRL::ComPtr<IDxcBlob> reflBlob;
	HRESULT hr = result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflBlob), nullptr);
	MY_ASSERT_MSG(SUCCEEDED(hr) && reflBlob != nullptr, "リフレクション情報の取得に失敗（直前のシェーダーコンパイルエラーを確認）");
	// DxcBuffer に包んで、読める形（ID3D12ShaderReflection）にする
	DxcBuffer buf{reflBlob->GetBufferPointer(), reflBlob->GetBufferSize(), DXC_CP_ACP};
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	hr = DirectXCommon::GetDxcUtils()->CreateReflection(&buf, IID_PPV_ARGS(&reflection));
	MY_ASSERT_MSG(SUCCEEDED(hr), "リフレクションの作成に失敗しました");
	return reflection;
}


//=============================================================================
// リフレクションからレジスタなど情報を取得する
//=============================================================================
std::vector<ShaderResourceBinding> ShaderCompiler::MakeShaderResourceBinding(ID3D12ShaderReflection* reflection) {
	// Shader情報取得
	D3D12_SHADER_DESC shaderDesc{};
	reflection->GetDesc(&shaderDesc);

	// --- 取得した情報を ShaderResourceBinding にいれる ---
	std::vector<ShaderResourceBinding> binds;
	// Bind情報の数分ループ
	for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
		D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
		reflection->GetResourceBindingDesc(i, &bindDesc);
		binds.push_back({
		    bindDesc.Name,      // シェーダー内の変数名（例: gCamera）
		    bindDesc.Type,      // リソース種別（例: CBV, SRV, SAMPLER...）
		    bindDesc.BindPoint, // レジスタ番号（例: t0 → 0を返す）
		    bindDesc.BindCount, // バインド数（例: Texture2D tex[]なら0, ないなら1）
		    bindDesc.Space,     // レジスタスペース番号（例： space0なら0）
		    bindDesc.Dimension, // テクスチャの形状（2D, Cubeなど）
		});
		LogManager::Log(std::format("{} type={} reg={} space={} count={}", bindDesc.Name, (int)bindDesc.Type, bindDesc.BindPoint, bindDesc.Space, bindDesc.BindCount));
	}

	return binds;
}


//=============================================================================
// リフレクションから入力パラメータを取得する
//=============================================================================
std::vector<ShaderInputParameter> ShaderCompiler::MakeShaderInputParameter(ID3D12ShaderReflection* reflection) {
	// Shader情報取得
	D3D12_SHADER_DESC shaderDesc{};
	reflection->GetDesc(&shaderDesc);

	// --- 取得した情報を ShaderInputParameter にいれる ---
	std::vector<ShaderInputParameter> inputs;
	// 入力パラメータ分ループ
	for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc{};
		reflection->GetInputParameterDesc(i, &paramDesc);

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