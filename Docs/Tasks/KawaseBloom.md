# G-1：川瀬式ブルーム（MGF）を、今のブルームと切り替えられるようにする

参考：
- 川瀬正樹「Frame Buffer Postprocessing Effects in DOUBLE-S.T.E.A.L (Wreckless)」GDC 2003（MGF：Multiple Gaussian Filter）
- ガウスぼかしを横・縦に分ける（分離可能なフィルタ）：https://en.wikipedia.org/wiki/Gaussian_blur#Mechanics
- 前のブルーム（デュアルフィルタ式）：今の `BloomPass`（[PostEffect.md](PostEffect.md)）

## このファイルについて
- AL4_01 の設計 [ParticleTitleClear.md](../../../AL4_01/Docs/ParticleTitleClear.md) の **3.2（パート G の G1）**。[AL4_01Game.md](AL4_01Game.md) の 13章の Step **G-1**。
- ユーザーの頼み：ブルームを川瀬式（MGF）に切り替えられるようにする（Dual はそのまま残す。Settings → PostEffect → Bloom に Mode・段の重み）。
- **[ParticleSystem.md](ParticleSystem.md) の後**に写す（同じ作業用コピーで、続けて確かめた）。
- **写す場所**：`AL4_01/MyEngine_v1`（サブモジュール）→ エンジンでコミット・push → `source/repos/MyEngine_v1` で pull。
- **ゲームのコードは変わらない**。初めの値は Mode = Dual なので、写しただけでは今と同じ見た目。
- 確かめた環境：AL4_01 develop（4282746）＋ エンジン develop（c674d9b）＋ ParticleSystem.md ＋ このファイル。Debug・Release でビルドし、Release で Dual と Kawase の絵を撮って見比べ、エディタで Settings の項目を確かめた（5章）。

| ファイル | 何をするか |
|---|---|
| `Graphics/PostEffect/BloomPass.h / .cpp` | `BloomMode { Dual, Kawase }`。川瀬式の段（1/4〜1/32）と作業用の RenderTexture。`Record` をぼかし方で分ける。`Param` に向き・段の重み・段の SRV |
| `Shader/Data/Post/Bloom.hlsli.shader` | `BloomParam` に3つ足す（C++ の `Param` と同じ並び） |
| `Shader/Data/Post/BloomGaussian.PS.shader`（新しく作る） | 9タップの1方向ガウス |
| `Shader/Data/Post/BloomKawaseComposite.PS.shader`（新しく作る） | 4つの段を重み付きで足して、シーンに足す |
| `Graphics/PostEffect/PostProcess.cpp` | Settings → PostEffect → Bloom に **Mode・Weight1〜4** |
| `MyEngine_v1.vcxproj` | 新しいシェーダー2つ（ソリューションに見せるだけ。無くても動く） |

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 今のブルーム（Dual）と川瀬式（Kawase）
```
共通：シーン ─(明るい所を抜き出す・1/2)→ 抜き出し

Dual   ：抜き出し → 1/4 → 1/8 → 1/16（テントで縮めながらぼかす）
         → 1/16 から 1/2 へ戻しながら、同じ大きさの段を足していく → シーン ＋ 強さ × 結果

Kawase ：抜き出し ─縮める→ 段0(1/4) ─横ガウス→ 作業0 ─縦ガウス→ 段0'
         段0'      ─縮める→ 段1(1/8) ─横→ ─縦→ 段1'
         段1'      ─縮める→ 段2(1/16) ─横→ ─縦→ 段2'
         段2'      ─縮める→ 段3(1/32) ─横→ ─縦→ 段3'
         合成：シーン ＋ 強さ × (W1·段0' ＋ W2·段1' ＋ W3·段2' ＋ W4·段3')   ← 1回の PS で4枚を読む
```
- **川瀬式の形**：段ごとに同じ幅（9ピクセル）でぼかすが、段の大きさが半分ずつ小さいので、**画面の上では段ごとに2倍ずつ広く**ぼける。全部を足すと「芯の周りの強い光」と「遠くまで届く薄いにじみ」が重なる。**段ごとの重み（Weight1〜4）でにじみの形を変えられる**（Weight1 を大きくすると芯が強く、Weight4 を大きくすると遠くまで薄く広がる）。
- **次の段は、ぼかした後の段から縮める**：ぼかしが段ごとに重なるので、小さい段ほど丸く柔らかくなる（縮める前の段から作ると、小さい段がカクカクした四角になりやすい）。
- **横と縦に分ける**：ガウスは「横に1回・縦に1回」に分けても、2次元で1回ぼかしたのと同じ結果になる（分離可能）。9×9 の 81回読む所が、9＋9 の 18回で済む。
- **縮めるのは今の `BloomDown`（テント）をそのまま**使う。抜き出し（`BloomBright`）も同じ。中間は今と同じ HDR（`R16G16B16A16_FLOAT`）。
- **読む絵と書く絵を同じにしない**：縦にぼかすときは「作業用 → 段」へ書く（段から読んで段へ書くと、GPU が壊れた結果を出す）。なので段ごとに2枚（段・作業用）持つ。

### 1.2 1フレームに積む描画の数
- Kawase は1ビュー「抜き出し 1 ＋ 段 4 × (縮める・横・縦) 3 ＋ 合成 1」＝ **14回**。Scene・Game の2ビューで 28回。
- 今の上限 `kMaxPassPerFrame = 32` だとぎりぎりなので、**64 に上げた**（定数バッファのスロット。1つ 256 バイトなので 16KB。G-3 のスターでも足りなくならないように）。

### 1.3 見比べ（Release・テスト用に Threshold 0.6）
![左から Dual／Kawase（重み 1,1,1,1）／Kawase（重み 2,1,0.6,0.4）。キューブの光り方](images/kawase_bloom_cube.png)
- Dual は芯の周りに強く、Kawase（1,1,1,1）は外へ広く薄く届く。Kawase は Weight1 を上げると芯が強くなる（右）。
- 今のゲームの Bloom の Threshold は **1.0**（Parameters/PostEffect/Bloom.json）。シーンの絵は SDR（0〜1）なので、今はほとんど何も光らない。光らせる物を決めるのは G-2（HDR）。

---

## 2. `Graphics/PostEffect/BloomPass.h`

`class BloomPass` の上に、ぼかし方の enum を足す：
```cpp
/// <summary>
/// ブルームのぼかし方
/// </summary>
enum class BloomMode {
	Dual,   // デュアルフィルタ式：テントで縮めながらぼかし、小さい段から戻しながら足す（軽い。芯の周りに柔らかく広がる）
	Kawase, // 川瀬式（MGF）：1/4〜1/32 の段ごとに横・縦のガウスでぼかし、全部の段を重みを付けて足す（芯の強い光と、遠くまで届く薄いにじみ）
};
```
定数と調整項目を、こう直す：
```cpp
	static constexpr uint32_t kLevelCount = 4;       // 縮小の段数（Dual：1/2〜1/16、Kawase：1/4〜1/32）
	static constexpr uint32_t kMaxPassPerFrame = 64; // 1フレームに積むパス数の上限（Kawase は1ビュー14パス。Scene・Game の2ビューで28）

	// --- 調整項目 ---
	BloomMode mode = BloomMode::Dual; // ぼかし方
	float kThreshold = 0.75f; // これより明るい所だけ光る
	float kKnee = 0.25f;      // しきい値付近の柔らかさ
	float kIntensity = 0.9f;  // 合成の強さ
	std::array<float, kLevelCount> kKawaseWeights = {1.0f, 1.0f, 1.0f, 1.0f}; // 川瀬式の段ごとの重み（1/4・1/8・1/16・1/32。小さい段ほど広く薄く届く）
```
`Param` の最後に3つ足す（**シェーダーの `BloomParam` と同じ並び**。HLSL は 16 バイトごとに区切るので、`float2` の後ろに `float2` の空きを入れて、`float4`・`uint4` を区切りの頭から始める）：
```cpp
		uint32_t srcIndex = 0;
		uint32_t addIndex = 0;
		float padding = 0.0f;
		float direction[2] = {0.0f, 0.0f};  // ガウスでぼかす向き（横 (1,0)・縦 (0,1)）
		float padding2[2] = {0.0f, 0.0f};
		float levelWeights[kLevelCount] = {}; // 川瀬式の合成：段ごとの重み
		uint32_t levelIndices[kLevelCount] = {}; // 川瀬式の合成：段の SRV スロット
	};
```
private の関数を足す（`BindAndDraw` の下）：
```cpp
	void SetTarget(RenderTexture& target); // 描画先を張り、ビューポートをその大きさに合わせる
	void RecordDual();   // Dual：縮めながらぼかし、戻しながら足す（down_[0] に明るい所が入っている状態で呼ぶ）
	void RecordKawase(); // Kawase：段ごとに縮めて横・縦にぼかす（down_[0] に明るい所が入っている状態で呼ぶ）
```
メンバを足す（`compositePSO_` と `up_` の下）：
```cpp
	Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianPSO_;        // 川瀬式：1方向のガウス
	Microsoft::WRL::ComPtr<ID3D12PipelineState> kawaseCompositePSO_; // 川瀬式：段を全部足して合成
```
```cpp
	std::array<std::unique_ptr<RenderTexture>, kLevelCount> kawaseLevels_; // 川瀬式の段（1/4〜1/32。縮めた絵 → 縦にぼかした結果）
	std::array<std::unique_ptr<RenderTexture>, kLevelCount> kawaseTemp_;   // 川瀬式の段ごとの作業用（横にぼかした結果）
```

---

## 3. `Graphics/PostEffect/BloomPass.cpp`（丸ごと）
Dual の処理は中身を変えずに `RecordDual` へ移した（`Record` の中のラムダ `setTarget` は、Kawase でも使うので関数 `SetTarget` にした）。
```cpp
#define NOMINMAX
#include "BloomPass.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"

using namespace Microsoft::WRL;


//=============================================================================
// 初期化
//=============================================================================
void BloomPass::Initialize(uint32_t width, uint32_t height) {
	// 縮小テクスチャを2列作る。加算合成を使わずに済むよう、行きと帰りで別々に持つ
	uint32_t w = width;
	uint32_t h = height;
	for (uint32_t i = 0; i < kLevelCount; ++i) {
		w = std::max(w / 2u, 1u);
		h = std::max(h / 2u, 1u);
		// 足し込みで飽和しないようHDRにする。深度は使わない
		down_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
		up_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
	}
	// 川瀬式の段（1/4・1/8・1/16・1/32）。段ごとに「縮めた絵・縦にぼかした結果」と「横にぼかした結果」の2枚
	w = std::max(width / 2u, 1u);
	h = std::max(height / 2u, 1u);
	for (uint32_t i = 0; i < kLevelCount; ++i) {
		w = std::max(w / 2u, 1u);
		h = std::max(h / 2u, 1u);
		kawaseLevels_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
		kawaseTemp_[i] = std::make_unique<RenderTexture>(w, h, RenderTextureFormat::HDR, false);
	}
	// パスごとに別スロットへ書く。コマンドの実行はフレーム末なので使い回すと壊れる
	cb_ = DirectXCommon::CreateMappedUploadBuffer(kSlotSize * kMaxPassPerFrame, reinterpret_cast<void**>(&cbMapped_));
	cb_->SetName(L"BloomCB");

	CreateRootSignature();
	brightPSO_ = CreatePSO("BloomBrightPS");
	downPSO_ = CreatePSO("BloomDownPS");
	upPSO_ = CreatePSO("BloomUpPS");
	compositePSO_ = CreatePSO("BloomCompositePS");
	gaussianPSO_ = CreatePSO("BloomGaussianPS");
	kawaseCompositePSO_ = CreatePSO("BloomKawaseCompositePS");
	LogManager::Log("Initialized");
}

//=============================================================================
// RootSignature / PSO
//=============================================================================
void BloomPass::CreateRootSignature() {
	const ShaderReflection& vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS");
	const ShaderReflection& ps = ShaderPackageLoader::GetShaderReflection("BloomCompositePS");
	auto resources = RootSignatureManager::MergeStages(vs.resources, ps.resources);
	rsInfo_ = RootSignatureManager::MakeRootSignatureInfo(resources);
}

ComPtr<ID3D12PipelineState> BloomPass::CreatePSO(const char* psName) {
	IDxcBlob* vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS").blob.Get();
	IDxcBlob* ps = ShaderPackageLoader::GetShaderReflection(psName).blob.Get();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rsInfo_.rootSignature.Get();
	desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	desc.InputLayout = {nullptr, 0};
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	desc.NumRenderTargets = 1;
	// 中間はHDR、合成先はSDR。フォーマットが違うのでPSOを分ける
	const std::string name = psName;
	const bool isComposite = (name == "BloomCompositePS" || name == "BloomKawaseCompositePS");
	desc.RTVFormats[0] = isComposite ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DirectXCommon::GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	MY_ASSERT_MSG(SUCCEEDED(hr), "Bloom PSO作成失敗");
	return pso;
}

//=============================================================================
// 1パス分のバインドと描画
//=============================================================================
void BloomPass::BindAndDraw(ID3D12PipelineState* pso, const Param& param) {
	MY_ASSERT_MSG(slot_ < kMaxPassPerFrame, "1フレームのブルームパス数が上限を超えました");
	auto* cmdList = DirectXCommon::GetCommandList();

	size_t offset = slot_ * kSlotSize;
	std::memcpy(cbMapped_ + offset, &param, sizeof(Param));
	++slot_;

	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootSignature(rsInfo_.rootSignature.Get());
	cmdList->SetPipelineState(pso);
	cmdList->SetGraphicsRootConstantBufferView(rsInfo_.slotOf.at(RootBind::Bloom), cb_->GetGPUVirtualAddress() + offset);
	cmdList->SetGraphicsRootDescriptorTable(rsInfo_.slotOf.at(RootBind::BindlessTexture), DirectXCommon::GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0); // フルスクリーン三角形
}

// 描画先ごとにビューポートを張り直す必要がある
void BloomPass::SetTarget(RenderTexture& target) {
	auto* cmdList = DirectXCommon::GetCommandList();
	target.PreDraw();
	D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(target.GetWidth()), static_cast<float>(target.GetHeight()), 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(target.GetWidth()), static_cast<LONG>(target.GetHeight())};
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);
}

//=============================================================================
// 光の層を作る
//=============================================================================
void BloomPass::Record(RenderTexture& scene) {
	// --- 明るい部分を抜き出して半分の解像度へ（Dual・Kawase で同じ）---
	{
		Param p{};
		p.texelSize[0] = 1.0f / scene.GetWidth();
		p.texelSize[1] = 1.0f / scene.GetHeight();
		p.threshold = std::max(kThreshold + addThreshold, 0.0f); // 負にすると全部真っ白になる
		p.knee = std::max(kKnee, 0.0001f);
		p.srcIndex = scene.GetSRVSlot();
		SetTarget(*down_[0]);
		BindAndDraw(brightPSO_.Get(), p);
		down_[0]->PostDraw();
	}

	// --- ぼかし方ごと ---
	if (mode == BloomMode::Kawase) {
		RecordKawase();
	} else {
		RecordDual();
	}
}

// ===== Dual：縮めながらぼかし、戻しながら足す =====
void BloomPass::RecordDual() {
	// --- 段々に縮めながらぼかす ---
	for (uint32_t i = 1; i < kLevelCount; ++i) {
		Param p{};
		p.texelSize[0] = 1.0f / down_[i - 1]->GetWidth();
		p.texelSize[1] = 1.0f / down_[i - 1]->GetHeight();
		p.srcIndex = down_[i - 1]->GetSRVSlot();
		SetTarget(*down_[i]);
		BindAndDraw(downPSO_.Get(), p);
		down_[i]->PostDraw();
	}

	// --- 一番小さい段から戻しながら、同じ大きさの縮小結果を足していく ---
	for (int i = static_cast<int>(kLevelCount) - 2; i >= 0; --i) {
		RenderTexture& src = (i == static_cast<int>(kLevelCount) - 2) ? *down_[kLevelCount - 1] : *up_[i + 1];
		Param p{};
		p.texelSize[0] = 1.0f / src.GetWidth();
		p.texelSize[1] = 1.0f / src.GetHeight();
		p.srcIndex = src.GetSRVSlot();
		p.addIndex = down_[i]->GetSRVSlot();
		SetTarget(*up_[i]);
		BindAndDraw(upPSO_.Get(), p);
		up_[i]->PostDraw();
	}
}

// ===== Kawase（MGF）：段ごとに縮めて、横 → 縦にガウスでぼかす =====
//   抜き出し(1/2) ─縮める→ 段0(1/4) ─横→ 作業0 ─縦→ 段0'
//   段0'          ─縮める→ 段1(1/8) ─横→ 作業1 ─縦→ 段1'  … 段3(1/32) まで
// 次の段は、ぼかした後の段から縮める（ぼかしが段ごとに重なって、小さい段ほど広く広がる）
// 合成（Composite）で 段0'〜段3' を重みを付けて全部足す
void BloomPass::RecordKawase() {
	RenderTexture* source = down_[0].get(); // 縮める元（初めは抜き出した絵）
	for (uint32_t i = 0; i < kLevelCount; ++i) {
		RenderTexture& level = *kawaseLevels_[i];
		RenderTexture& temp = *kawaseTemp_[i];
		const float texelX = 1.0f / level.GetWidth();
		const float texelY = 1.0f / level.GetHeight();

		// --- 縮める（今のテントをそのまま使う）---
		{
			Param p{};
			p.texelSize[0] = 1.0f / source->GetWidth();
			p.texelSize[1] = 1.0f / source->GetHeight();
			p.srcIndex = source->GetSRVSlot();
			SetTarget(level);
			BindAndDraw(downPSO_.Get(), p);
			level.PostDraw();
		}
		// --- 横にぼかす（段 → 作業用）---
		{
			Param p{};
			p.texelSize[0] = texelX;
			p.texelSize[1] = texelY;
			p.direction[0] = 1.0f;
			p.srcIndex = level.GetSRVSlot();
			SetTarget(temp);
			BindAndDraw(gaussianPSO_.Get(), p);
			temp.PostDraw();
		}
		// --- 縦にぼかす（作業用 → 段。読む絵と書く絵を同じにしない）---
		{
			Param p{};
			p.texelSize[0] = texelX;
			p.texelSize[1] = texelY;
			p.direction[1] = 1.0f;
			p.srcIndex = temp.GetSRVSlot();
			SetTarget(level);
			BindAndDraw(gaussianPSO_.Get(), p);
			level.PostDraw();
		}
		source = &level;
	}
}

//=============================================================================
// 合成（描画先は呼び出し側が設定済み）
//=============================================================================
void BloomPass::Composite(RenderTexture& scene, float width, float height) {
	auto* cmdList = DirectXCommon::GetCommandList();
	D3D12_VIEWPORT vp{0.0f, 0.0f, width, height, 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);

	Param p{};
	p.intensity = kIntensity + addIntensity;
	p.srcIndex = scene.GetSRVSlot();
	if (mode == BloomMode::Kawase) {
		// 川瀬式：段を全部、重みを付けて足す（1回の PS で4枚を読む。Bindless なので SRV の番号を渡すだけ）
		for (uint32_t i = 0; i < kLevelCount; ++i) {
			p.levelWeights[i] = kKawaseWeights[i];
			p.levelIndices[i] = kawaseLevels_[i]->GetSRVSlot();
		}
		BindAndDraw(kawaseCompositePSO_.Get(), p);
	} else {
		p.addIndex = up_[0]->GetSRVSlot();
		BindAndDraw(compositePSO_.Get(), p);
	}

	// 上乗せ分は使ったら消す
	addIntensity = 0.0f;
	addThreshold = 0.0f;
}
```

---

## 4. シェーダー・Settings

### `Shader/Data/Post/Bloom.hlsli.shader`
`BloomParam` の `float padding;` の下に足す（C++ の `Param` と同じ並び）：
```hlsl
    float32_t2 direction;      // ガウスでぼかす向き（横 (1,0)・縦 (0,1)）
    float32_t2 padding2;
    float32_t4 levelWeights;   // 川瀬式の合成：段ごとの重み（1/4・1/8・1/16・1/32）
    uint32_t4 levelIndices;    // 川瀬式の合成：段の SRV スロット
```

### `Shader/Data/Post/BloomGaussian.PS.shader`（新しく作る）
```hlsl
#META
stage: PS
path : Post/BloomGaussian.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
#include "Post/Bloom.hlsli"

// 9タップの1方向ガウス（σ = 2 ピクセル）。重みは exp(-x² / (2σ²)) を、足して 1 になるように割った物
// 真ん中と、片側4つ（離れた順）。横・縦の2回に分けると、9×9 の2次元のぼかしと同じになる（81回読む所が 18回で済む）
static const float kWeights[5] = {0.2042f, 0.1802f, 0.1238f, 0.0663f, 0.0276f};

float32_t4 main(VSOutput input) : SV_TARGET
{
    float32_t2 step = gBloom.direction * gBloom.texelSize; // 1ピクセル分、ぼかす向きへ
    float32_t3 sum = gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord).rgb * kWeights[0];
    for (int i = 1; i < 5; ++i) {
        sum += gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord + step * i).rgb * kWeights[i];
        sum += gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord - step * i).rgb * kWeights[i];
    }
    return float32_t4(sum, 1.0f);
}
#HLSL_END
```
- 重みの出し方：x = 0〜4 で exp(−x²/8) は 1・0.8825・0.6065・0.3247・0.1353。真ん中1つと左右2つずつを足すと 4.898 なので、全部をこれで割る（足して 1 にしないと、ぼかすたびに明るさが変わる）。

### `Shader/Data/Post/BloomKawaseComposite.PS.shader`（新しく作る）
```hlsl
#META
stage: PS
path : Post/BloomKawaseComposite.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
#include "Post/Bloom.hlsli"

// 川瀬式（MGF）の合成：シーン ＋ 強さ × (w1·段1 ＋ w2·段2 ＋ w3·段3 ＋ w4·段4)
// 段は大きさが違う（1/4〜1/32）が、UV（0〜1）で読むので、そのまま重ねられる（小さい段は拡大して読まれ、広く・柔らかく広がる）
float32_t4 main(VSOutput input) : SV_TARGET
{
    float32_t3 scene = gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord).rgb;
    float32_t3 bloom = float32_t3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; ++i) {
        bloom += gTextures[gBloom.levelIndices[i]].Sample(gSampler, input.texcoord).rgb * gBloom.levelWeights[i];
    }
    return float32_t4(scene + bloom * gBloom.intensity, 1.0f);
}
#HLSL_END
```
- .shader はフォルダごと起動時に読まれるので、置くだけで `BloomGaussianPS`・`BloomKawaseCompositePS` の名前で使える。
- **Release は `AL4_01/project/MyEngine/Shader` の写しを読む**。写しはエンジンをビルドし直したときに自動で新しくなる（このファイルは C++ も変えるので必ず写る）。

### `Graphics/PostEffect/PostProcess.cpp`
`RegisterGV` の Bloom の行を、こう直す（`.Add` を続ける）：
```cpp
	post.Category("Bloom").Add("Enabled", kBloomEnabled).Add("Threshold", bloom_.kThreshold).Add("Knee", bloom_.kKnee).Add("Intensity", bloom_.kIntensity)
	    // ぼかし方（Dual：今までの／Kawase：川瀬式）と、川瀬式の段ごとの重み（Weight1 が 1/4 の段 … Weight4 が 1/32 の段）
	    .Add("Mode", GlobalVariables::MakeEnumCombo(bloom_.mode))
	    .Add("Weight1", bloom_.kKawaseWeights[0])
	    .Add("Weight2", bloom_.kKawaseWeights[1])
	    .Add("Weight3", bloom_.kKawaseWeights[2])
	    .Add("Weight4", bloom_.kKawaseWeights[3]);
```
`ApplyGV` の `bloom_.kIntensity = …` の下：
```cpp
	bloom_.mode = GlobalVariables::GetEnumCombo<BloomMode>(gv->Get<GlobalVariables::ComboItem>(s, "Bloom", "Mode"));
	bloom_.kKawaseWeights[0] = gv->Get<float>(s, "Bloom", "Weight1");
	bloom_.kKawaseWeights[1] = gv->Get<float>(s, "Bloom", "Weight2");
	bloom_.kKawaseWeights[2] = gv->Get<float>(s, "Bloom", "Weight3");
	bloom_.kKawaseWeights[3] = gv->Get<float>(s, "Bloom", "Weight4");
```
- `MakeEnumCombo` は enum の名前（Dual・Kawase）をそのままコンボの選択肢にする。保存は何番目か（0・1）。
- 前に保存した `Parameters/PostEffect/Bloom.json` に Mode・Weight は無いが、無い項目は初めの値（Dual・1.0）で読む。Settings で Save すると足される。

### `MyEngine_v1.vcxproj`
（`BloomComposite.PS.shader` の行の下。ソリューションエクスプローラーに見せるだけ）
```
    <None Include="MyEngine\Shader\Data\Post\BloomGaussian.PS.shader" />
    <None Include="MyEngine\Shader\Data\Post\BloomKawaseComposite.PS.shader" />
```

---

## 5. 使い方・確かめたこと

### 使い方
エディタの **Settings → PostEffect → Bloom**：
- **Mode**：Dual（今まで）／Kawase（川瀬式）。
- **Weight1〜4**：川瀬式の段ごとの重み（1/4・1/8・1/16・1/32）。芯を強くしたいなら Weight1 を上げる、遠くまでにじませたいなら Weight3・4 を上げる。
- Threshold・Knee・Intensity は Dual・Kawase で同じ。Save で `Parameters/PostEffect/Bloom.json` に残る。

![Settings → PostEffect → Bloom（Mode・Weight1〜4）](images/kawase_bloom_settings.png)

### 確かめたこと
| | 結果 |
|---|---|
| ビルド | Debug・Release とも通る。直したファイルに警告なし |
| Release・Mode Dual（Threshold 0.6 にして） | 光る（Dual の処理は関数に移しただけで、中身は変えていない。直す前の絵と並べて比べてはいない） |
| Release・Mode Kawase（重み 1,1,1,1） | にじみが外へ広く薄く届く（1.3 の写真）。PSO の作成・シェーダーの読み込みで警告なし |
| Release・Mode Kawase（重み 2,1,0.6,0.4） | 芯が強くなる |
| エディタの Settings → PostEffect → Bloom | Mode（Dual／Kawase のコンボ）と Weight1〜4 が出る |
| エディタを Mode Kawase のまま起動 | Game ビューのキューブが川瀬式で光る。パスの数の上限の assert・ログの警告が出ない |

確かめていないこと：エディタで Mode を Kawase に切り替えて Save し、次の起動で読めること（保存の形は今ある enum のコンボと同じ）。

---

## 6. 決めたこと・後でやること
| 決めたこと | 理由 |
|---|---|
| 段は設計どおり 1/4〜1/32 の4つ。抜き出しは 1/2 のまま | Dual と抜き出しを共通にできる。1/2 の段も足すと芯がもっと強くなるが、Weight1 で足りた |
| 次の段は**ぼかした後**の段から縮める | 小さい段が丸く柔らかくなる |
| ガウスは 9タップ・σ 2（シェーダーに定数） | 設計どおり。段の大きさで広さが変わるので、σ を変える項目は要らない |
| 重みの初めの値は 1,1,1,1 | 川瀬式の「全部足す」そのまま。好みで Weight で変える |
| `kMaxPassPerFrame` を 32 → 64 | 2ビュー × 14 パスで 28。G-3（スター）でも足りなくならないように |
- 後でやること：G-2（**HDR**。新しいチャットで）。HDR にすると Threshold 1 で「本当に明るい物」だけが光るようになり、川瀬式の遠くまで届くにじみが生きる。
