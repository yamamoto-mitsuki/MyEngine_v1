# G11：被写界深度（DOF）とグリッドの出し分け

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の **G11**（P2・世界観②）。G10（霧・背景の層）の続き。
- やること：
  - **被写界深度（Depth Of Field・DOF）**：ピントの合った「遊ぶ面」ははっきり、**奥の層ほどぼける**（元のゲームの一番の特徴）。ぼかすのは **CS（Compute Shader）**。値はシーンに置く **Depth Of Field Component**（Fog と同じ形）。
  - **グリッドの出し分け**：View メニューで、**Scene ビューと Game ビューのグリッドを別々に On / Off**。
- 確かめた環境：本体の develop（7a25da1。G10 の写経まで入った物）＋ このファイルのエンジン、AL4_01 のコピー（今の Level01 に Depth Of Field と G10 の木の層を足した物）。Release で撮った（6章）。Debug でエディタを起動して、View メニュー・Scene / Game ビューのグリッドを確かめた。D3D12 のデバッグレイヤーのエラーは出なかった。

![DOF あり](images/g11_dof.png)
（DOF あり。遊ぶ面ははっきり、近い木（約 31m）は少し、遠い木（約 56m）・山・空ほど大きくぼける）

![DOF なし](images/g11_nodof.png)
（DOF なし。同じ場所）

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Graphics/Compute/ComputeTexture.h/.cpp`（新しく作る）・`Graphics/Compute/ComputePipeline.h/.cpp`・`Graphics/Pipeline/RootSignatureManager.cpp` | CS で**テクスチャ**を読み書きできるようにする（今まではバッファだけ） |
| B | `Graphics/GPU/DirectXCommon.h/.cpp`・`Graphics/RenderTarget/RenderTexture.h/.cpp` | **深度をシェーダーから読める** RenderTexture |
| C | `Graphics/PostEffect/DepthOfFieldPass.h/.cpp`（新しく作る）・`Shader/Data/Post/DepthOfField*`（新しく4つ）・`Graphics/PostEffect/PostProcess.h/.cpp`・`Graphics/Pipeline/RootSignatureManager.h/.cpp` | DOF 本体（CS 2本＋合成の PS 1本） |
| D | `Graphics/Renderer/SceneRenderer.h/.cpp`・`RenderStates.h`・`DrawRequest.h`・`RenderQueue.h/.cpp`・`Renderer.h/.cpp`・`Editor/Viewport/EditorGrid.cpp`・`EditorViewport.h/.cpp`・`Editor/ImGuiManager.cpp` | 描く順番（半透明は DOF の後）・ビューごとの設定（霧・DOF・グリッド）・View メニュー |
| E | `Components/DepthOfFieldComponent.h`・`Systems/DepthOfFieldSystem.h/.cpp`（新しく作る）・`Systems/CameraSystem.h/.cpp`・`Systems/WindZoneSystem.cpp`・`Engine.cpp`・`Window/WindowManager.cpp` | Depth Of Field Component（ピントは Camera Follow の Target） |
| F | エディタで | Level01 に Depth Of Field を付ける・値の目安 |

- **パートの順番**：**A → B → C → D → E → F**。A と B はどちらが先でもよい。C は A・B を使う。D は C の `ApplyDepthOfField` を使う。E は D の `SceneRenderer::SetDepthOfField` を使う。どのパートの後でもビルドは通る（C の後は DOF の仕組みがあるだけで、まだ使われない）。
- 新しく作るファイルは、VS のソリューションエクスプローラーで同じフォルダに追加する（`.shader` も。`MyEngine_v1.vcxproj` に入る）。

---

## 0. 前回（G10）の写し違い
写経の答え合わせをした。**直すのは 1 つ**（もう 1 つは直さなくても動く）。

### 0.1 `Renderer.cpp` の `PushMesh`（直す）
`if (config.material != nullptr)` の中が `MakeDefaultModelMaterial` になっていた。これだと **Primitive Renderer の Material（光沢・金属・自己発光など）が効かない**（色だけになる）。G10 の md の書き方が悪かった（「色の番号を開く4行を」の下に、1 行目だけ見せたので、2 か所とも書き換えたように読めた）。中は**元のまま**の `MakeModelMaterial` に戻す：
```cpp
	req.materialData = MakeDefaultModelMaterial(color.x, color.y, color.z, color.w, config.uvTransform, config.alphaCutoff);
	// --- マテリアル：Config が material を持っていて、何かを指していれば、その値（光沢・金属感・自己発光など）---
	if constexpr (requires { config.material; }) {
		if (config.material != nullptr) {
			req.materialData = MakeModelMaterial(nullptr, config.color, config.uvTransform, *config.material, true);
			req.materialData.alphaCutoff = config.alphaCutoff; // 切り抜きは、どの Config にもある alphaCutoff の方を使う
		}
	}
```

### 0.2 `Skybox.PS` （直さなくてよい）
`direction` を作ったのに、`Sample` の中でもう一度 `normalize(i.dir)` していた。同じ値なので動きは同じ。揃えるなら：
```cpp
    float3 direction = normalize(i.dir);
    float3 c = gTexturesCube[gSkybox.cubeIndex].Sample(gSampler, direction).rgb * gSkybox.intensity;
```

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 DOF は「深度でぼけの大きさを決めて、その分だけまわりの色を混ぜる」
カメラのレンズは、ピントの合った奥行きの点だけが 1 点に写り、それより手前・奥の点は**円に広がって**写る。この円を **CoC（Circle of Confusion・錯乱円）** と言う。DOF は、ピクセルごとに CoC の大きさを決めて、その大きさだけまわりの色を混ぜる。

- **CoC の大きさは深度から**：ピクセルの深度 → カメラからの奥行き（m）→ ピントの奥行きとの差 → CoC。このゲームは横から見るので、**奥の層（木・山・空）ほど奥行きが大きい → ぼける**。
- **ピントは「遊ぶ面」に**：Camera Follow の Target（キューブ）の奥行きに合わせる。カメラの Z が変わっても、Scene ビューのカメラで見ても、キューブのいる面にピントが合う。

```
奥行き（カメラから）  0m      16.5m        31m        56m          96m
                      カメラ   キューブ     近い木     遠い木        空・山
CoC（Far 40・Range 3）        0（ピント）   0.3        0.93          1（一番ぼける）
```

### 1.2 なぜ CS か
ぼかしは「1 ピクセルの答えを出すのに、まわりを何十個も読む」仕事。描く（三角形をラスタライズする）必要が無いので、**CS で 1 スレッド ＝ 1 ピクセル**にすると素直に書ける（PS だとフルスクリーンの三角形を描く必要がある）。課題の「技術的挑戦」の **DOF・フォグ＋CS** の CS の所。

### 1.3 作り（3 回に分ける）

```
3D を描いた絵（色・深度）
   │ CS 1：DepthOfFieldDownsample（半分の大きさに縮める。A にぼけの大きさ）
   ▼
half_（640×360・RGBA16F）  RGB＝2×2 の平均の色、A＝2×2 の CoC の平均（マイナスが手前）
   │ CS 2：DepthOfFieldBlur（うずまきの順にまわりを読んで混ぜる）
   ▼
blur_（640×360・RGBA16F）  RGB＝ぼかした色、A＝手前のぼけのはみ出し
   │ PS：DepthOfFieldComposite（元の大きさで、元の絵とぼかした絵を CoC で混ぜる）
   ▼
DOF を掛けた絵 → 半透明を描く → ブルーム → レンズ → 画面
```

- **半分の大きさでぼかす**：ぼけた絵は細かさが要らない。読む点が 4 分の 1 で済む。
- **合成は元の大きさの深度で CoC を計算し直す**：半分の大きさの CoC のままだと、ピントの合った物の縁まで混ぜてしまう。
- **合成は PS**：書く先が SDR の RenderTexture（`R8G8B8A8_UNORM_SRGB`）で、`_SRGB` の形は UAV（CS が書く窓口）にできないため。

### 1.4 ぼかし方（集めるぼかし・黄金角）
本当のレンズは「点が円に**広がる**」（散らす）。でも GPU で「まわりへ書く」と、同じピクセルに何人も書いて取り合いになる。そこで逆に「**自分の所まで広がってくる点を集める**」書き方にする（Dennis Gustafsson の「Bokeh depth of field in a single pass」の形）。

1. 自分のまわりの点を、**黄金角（約 137.5 度）ずつ回しながら、少しずつ外へ**読む（うずまき）。この角度だと、点が偏らずに円の中へ散らばる。
2. 読んだ点の CoC（半径）が、**その点までの距離より大きければ**、その点の色はここまで広がってくる → 混ぜる。
3. **自分より奥の点は、自分の CoC の 2 倍より大きく広げない**。ピントの合ったキューブの所に、後ろのぼけた空が混ざらないように（空はキューブの後ろにあるので、前へ出てこない）。逆に、**手前の点は自分の上にかぶってよい**（手前のぼけた草は、ピントの合ったキューブの上に広がる）。

### 1.5 半透明は DOF の後に描く
深度を書かない物（半透明の Primitive・Arrow Strip・輪・線・パーティクル）は、深度バッファに奥行きが残らない。そのまま DOF を掛けると、**後ろにある物（空）と同じだけぼける**（最初に作ったとき、遊ぶ面の矢印と輪がぼけた）。
→ **不透明な物と空を描く → DOF → ぼかした絵に、半透明を描く**（Unreal の半透明の「Render After DOF」と同じ考え）。半透明を描くときは、元の深度を張るので、不透明な物の後ろに隠れる。半透明はぼけないが、このゲームの半透明はどれも遊ぶ面の物なので、その方が良い。

### 1.6 深度を読むには TYPELESS
深度バッファを `D24_UNORM_S8_UINT` で作ると、深度としてしか使えない（読む窓口 SRV を作れない）。**`R24G8_TYPELESS`（型なし）で作ると、窓口ごとに形を決められる**：書くとき（DSV）は `D24_UNORM_S8_UINT`、読むとき（SRV）は `R24_UNORM_X8_TYPELESS`（24bit の深度を 0〜1 で読み、8bit のステンシルは読まない）。

### 1.7 グリッドの出し分け
グリッドは `Renderer::DrawModel` で描く物を 1 回だけ積み、それを Scene と Game の 2 回描いている。ビューごとに積み直すのではなく、**描く物に「層（DrawLayer）」の印を付けて、RenderQueue がビューごとに飛ばす**（Unity の Layer とカメラの Culling Mask の小さい版）。今は層が `Default` と `EditorGrid` だけだが、Collider の線なども後で同じ形で切れる。

ついでに、Scene ビューだけ霧を切る仕組み（G10 の `SetViewFogEnabled`）も、**ビューごとの設定 `SceneViewOptions`（霧・DOF・グリッド）にまとめて、`RenderToTexture` に渡す**形にした（「描く前に切って、描いたら戻す」を 3 つ並べずに済む）。View メニューの印も `EditorViewport::ViewFlags` 1 つにまとめた。

![View メニュー](images/g11_view_menu.png)
（View メニュー。Game ビューのグリッドは初めは OFF）

---

## パートA：CS でテクスチャを読み書きする

今の `ComputePipeline` はバッファ（`StructuredBuffer`・`RWStructuredBuffer`）しか結べない。ルートの SRV・UAV（ルートシグネチャに直接置く窓口）は**バッファ専用**で、テクスチャは結べないため。テクスチャは、ヒープに窓口を作って、その場所を**表（DescriptorTable）**で指す。

### A.1 `Graphics/Compute/ComputeTexture.h`（新しく作る）
CS が書いて、CS・PS が読むテクスチャ。書く窓口（UAV）と読む窓口（SRV）を両方持つ。
```cpp
#pragma once
#include <cstdint>

#include <d3d12.h>
#include <wrl.h>


/// <summary>
/// CS が書いて、CS・PS が読むテクスチャ（書く窓口 UAV と、読む窓口 SRV を両方持つ）
/// <para>使い方：BeginWrite（読む状態 → 書く状態）→ CS で書く → EndWrite（書く状態 → 読む状態）→ 別の CS・PS が gTextures[GetSRVSlot()] で読む</para>
/// </summary>
class ComputeTexture {
public:
	/// <summary>
	/// 1枚作る（初めは読む状態）
	/// </summary>
	/// <param name="format">中身の形（R16G16B16A16_FLOAT など。_SRGB の形は UAV にできないので使えない）</param>
	/// <param name="name">PIX・デバッグ表示用の名前</param>
	ComputeTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, const wchar_t* name);
	// SRV・UAV のスロットを返す
	~ComputeTexture();

	// コピー禁止（スロットを2回返してしまう）
	ComputeTexture(const ComputeTexture&) = delete;
	ComputeTexture& operator=(const ComputeTexture&) = delete;

	// 読む状態 → CS が書ける状態（ComputePipeline::Dispatch の前に呼ぶ）
	void BeginWrite();
	// CS が書ける状態 → 読む状態（Dispatch の後に呼ぶ。書き終わるのを待ってから、次の CS・PS が読むようになる）
	void EndWrite();

	// ===== ゲッター =====
	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }
	// 読む窓口の番号（HLSL の gTextures[番号] で読む）
	uint32_t GetSRVSlot() const { return srvSlot_; }
	// 書く窓口（ComputePipeline::SetUnorderedAccessTexture に渡す）
	D3D12_GPU_DESCRIPTOR_HANDLE GetUAVGPUHandle() const { return uavHandleGPU_; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_; // 中身
	uint32_t srvSlot_ = 0;                            // 読む窓口の、SRVヒープの番号
	uint32_t uavSlot_ = 0;                            // 書く窓口の、SRVヒープの番号（UAV も同じヒープに置く）
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandleGPU_{};      // 書く窓口のGPUの場所
	uint32_t width_ = 0;
	uint32_t height_ = 0;
};
```

### A.2 `Graphics/Compute/ComputeTexture.cpp`（新しく作る）
```cpp
#include "ComputeTexture.h"

#include "MyEngine/Graphics/GPU/DirectXCommon.h"


//=============================================================================
// 作成・破棄
//=============================================================================
// ===== 作成 =====
ComputeTexture::ComputeTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, const wchar_t* name) : width_(width), height_(height) {
	// ===== 中身（ALLOW_UNORDERED_ACCESS が無いと UAV にできない）=====
	// 初めは読む状態（ALL_SHADER_RESOURCE ＝ CS からも PS からも読める状態）
	resource_ = DirectXCommon::CreateTextureResource(width, height, format, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	resource_->SetName(name);

	ID3D12Device* device = DirectXCommon::GetDevice();
	ID3D12DescriptorHeap* heap = DirectXCommon::GetSRVDescriptorHeap();

	// ===== 読む窓口（SRV）=====
	srvSlot_ = DirectXCommon::AllocateSRVSlot();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(resource_.Get(), &srvDesc, DirectXCommon::GetCPUDescriptorHandle(heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_));

	// ===== 書く窓口（UAV）=====
	// SRV と同じヒープに置く（描くとき・CS を動かすときに SetDescriptorHeaps で見せるのは、このヒープ1つだけ）
	uavSlot_ = DirectXCommon::AllocateSRVSlot();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(resource_.Get(), nullptr, &uavDesc, DirectXCommon::GetCPUDescriptorHandle(heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavSlot_));
	uavHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavSlot_);
}

// ===== 破棄 =====
// スロットを返す（返さないと、作り直すたびにヒープの空きが減っていく）
ComputeTexture::~ComputeTexture() {
	DirectXCommon::FreeSRVSlot(srvSlot_);
	DirectXCommon::FreeSRVSlot(uavSlot_);
}


//=============================================================================
// 状態の切り替え
//=============================================================================
// ===== 読む → 書く =====
void ComputeTexture::BeginWrite() {
	DirectXCommon::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

// ===== 書く → 読む =====
void ComputeTexture::EndWrite() {
	DirectXCommon::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
}
```

> **`ALL_SHADER_RESOURCE`**：`NON_PIXEL_SHADER_RESOURCE`（CS・VS が読める）と `PIXEL_SHADER_RESOURCE`（PS が読める）を両方立てた状態。ぼかした絵は CS（次の CS）と PS（合成）の両方が読むので、この状態で待たせる。

### A.3 `Graphics/Compute/ComputePipeline.h`
`SetUnorderedAccess` の下に 2 つ足す：
```cpp
	/// <summary>
	/// Texture2D の配列（tレジスタ・読むだけ。gTextures[] のバインドレス）に、SRVヒープの先頭を結ぶ
	/// <para>どのテクスチャを読むかは、定数バッファに入れた番号（GetSRVSlot）で、CS の中で選ぶ。先に SetDescriptorHeaps でヒープを見せておく</para>
	/// </summary>
	/// <param name="name">HLSLの変数名（"gTextures"）</param>
	/// <param name="heapStart">SRVヒープの先頭（GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()）</param>
	void SetTextureTable(ID3D12GraphicsCommandList* cmdList, const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE heapStart) const;

	/// <summary>
	/// RWTexture2D（uレジスタ・読み書き）に、テクスチャの書く窓口（UAV）を結ぶ
	/// <para>テクスチャは ALLOW_UNORDERED_ACCESS 付きで作り、UNORDERED_ACCESS の状態にしておく（ComputeTexture::BeginWrite）</para>
	/// </summary>
	/// <param name="name">HLSLの変数名（"gOutput" など）</param>
	/// <param name="uavHandle">書く窓口のGPUの場所（ComputeTexture::GetUAVGPUHandle）</param>
	void SetUnorderedAccessTexture(ID3D12GraphicsCommandList* cmdList, const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE uavHandle) const;
```

### A.4 `Graphics/Compute/ComputePipeline.cpp`
`SetUnorderedAccess` の下に 2 つ足す：
```cpp
// ===== テクスチャを読む（t。バインドレスの配列）=====
void ComputePipeline::SetTextureTable(ID3D12GraphicsCommandList* cmdList, const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE heapStart) const {
	cmdList->SetComputeRootDescriptorTable(FindSlot(name, D3D_SIT_TEXTURE), heapStart);
}

// ===== テクスチャへ書く（u）=====
void ComputePipeline::SetUnorderedAccessTexture(ID3D12GraphicsCommandList* cmdList, const std::string& name, D3D12_GPU_DESCRIPTOR_HANDLE uavHandle) const {
	cmdList->SetComputeRootDescriptorTable(FindSlot(name, D3D_SIT_UAV_RWTYPED), uavHandle);
}
```

- `D3D_SIT_UAV_RWTYPED`：HLSL の `RWTexture2D<float4>` の種類（リフレクションで分かる）。`RWStructuredBuffer` は `D3D_SIT_UAV_RWSTRUCTURED`。
- `gTextures[]`（バインドレス）は描くときと同じ形で、ヒープの先頭を渡して、CS の中で番号で選ぶ。

### A.5 `Graphics/Pipeline/RootSignatureManager.cpp` の `MakeRootParameter`
最後の `else { ... }` の中を差し替える（`RWTexture2D` も表にする）：
```cpp
	} else {
		// ByteAddressBuffer などはまだ作っていない（下の範囲に入れると、違う種類で結んでしまう）
		MY_ASSERT_MSG(b.type == D3D_SIT_TEXTURE || b.type == D3D_SIT_STRUCTURED || b.type == D3D_SIT_UAV_RWTYPED,
		    std::format("'{}' の種類（D3D_SHADER_INPUT_TYPE = {}）は、まだルートシグネチャに入れられません", b.name, static_cast<int>(b.type)));

		// --- テクスチャ = DescriptorTable（読むテクスチャの配列 gTextures[]（bindless）か、CS が書く RWTexture2D）---
		// ルートの SRV・UAV（上の2つ）はバッファしか結べないので、テクスチャはヒープに置いた窓口を表（テーブル）で指す
		const bool isWritable = (b.type == D3D_SIT_UAV_RWTYPED);
		D3D12_DESCRIPTOR_RANGE1& range = ranges.emplace_back();
		range.RangeType = isWritable ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = (b.count == 0) ? UINT_MAX : b.count; // 0=無制限
		range.BaseShaderRegister = b.registerNumber;
		range.RegisterSpace = b.space;
		// 書く物は、中身が CS の中で変わる（DATA_VOLATILE）とドライバーに伝える
		range.Flags = isWritable ? (D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE) : D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &range; // ranges.reserve済み前提
	}
```

- `DATA_VOLATILE`：「表の先の中身は、シェーダーが動いている間に変わる」とドライバーに伝える印。CS が書く物に付ける（付けないと、中身が変わらない前提で速くする工夫をされることがある）。

---

## パートB：深度を読める RenderTexture

### B.1 `Graphics/GPU/DirectXCommon.h`
`CreateDepthStencilTextureResource` に引数を 1 つ足す：
```cpp
	/// 深度StencilTextureResourceを生成する
	/// </summary>
	/// <param name="isShaderReadable">true：シェーダーからも読めるように、型なし（R24G8_TYPELESS）で作る</param>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height, bool isShaderReadable = false);
```

### B.2 `Graphics/GPU/DirectXCommon.cpp`
関数を差し替える：
```cpp
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDepthStencilTextureResource(int32_t width, int32_t height, bool isShaderReadable) {
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // クリアは深度として書く形（DSV と同じ形）

	// D24_UNORM_S8_UINT で作ると、深度としてしか使えない（読む窓口 SRV を作れない）
	// 型なし（TYPELESS）で作ると、窓口ごとに形を決められる（DSV は D24_UNORM_S8_UINT、SRV は R24_UNORM_X8_TYPELESS）
	const DXGI_FORMAT format = isShaderReadable ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
	return CreateTextureResource(
	    static_cast<uint32_t>(width), static_cast<uint32_t>(height), format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue);
}
```

### B.3 `Graphics/RenderTarget/RenderTexture.h`（全体を差し替え）
足した物：コンストラクタの `isDepthReadable`・`BindWithDepthOf`・`BeginShaderRead`・`EndShaderRead`・`GetDepthSRVSlot`・`depthSrvSlot_`・`isDepthReadable_`。
```cpp
#pragma once
#include <vector>
#include <cstdint>

#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// IBL使用時にHDR画像として使いたいのでFormatを区別する
enum class RenderTextureFormat {
	SDR,
	HDR,
	RG16F,
};


/// <summary>
/// レンダリング結果を画像に焼き付けるクラス
/// </summary>
class RenderTexture {
public:
	// 1枚作る（isDepthReadable：深度をシェーダーから読めるようにする。被写界深度が奥行きを知るのに使う。useDepth が true のときだけ）
	RenderTexture(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth = true, bool isDepthReadable = false);
	// SRVスロットの解放
	~RenderTexture();

	// コピー・ムーブ禁止
	RenderTexture(const RenderTexture&) = delete;
	RenderTexture& operator=(const RenderTexture&) = delete;


	/// <summary>
	/// 描画開始
	/// <para>リソースをPIXEL_SHADER_RESOURCE→RENDER_TARGETに切り替えてRTV/DSVをクリアする</para>
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画終了
	/// <para>リソースをRENDER_TARGET→PIXEL_SHADER_RESOURCEに戻す</para>
	/// </summary>
	void PostDraw();

	/// <summary>
	/// 描画先にし直す（クリアしない）。深度は depthSource の物を使う
	/// <para>被写界深度でぼかした絵に、元の絵の深度で前後を比べながら半透明を描くときに使う。PreDraw の後に呼ぶ</para>
	/// </summary>
	void BindWithDepthOf(RenderTexture& depthSource);

	/// <summary>
	/// 色と深度（isDepthReadable のとき）を、CS からも PS からも読める状態にする
	/// <para>PostDraw の後に呼ぶ。読み終わったら EndShaderRead（次の PreDraw より前）</para>
	/// </summary>
	void BeginShaderRead();

	/// <summary>
	/// BeginShaderRead の前の状態（色は PS だけが読める状態、深度は書く状態）に戻す
	/// </summary>
	void EndShaderRead();

	// ===== ゲッター =====
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() { return srvHandleGPU_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() { return rtvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() { return dsvHandle_; }
	uint32_t GetWidth() { return width_; }
	uint32_t GetHeight() { return height_; }
	// ヒープ上のスロット番号
	uint32_t GetSRVSlot() const { return srvSlot_; }
	// 深度を読む窓口のスロット番号（isDepthReadable で作ったときだけ使える）
	uint32_t GetDepthSRVSlot() const { return depthSrvSlot_; }


private:
	// ===== 内部ヘルパー =====
	void CreateResource();
	void CreateDSVResource();
	static DXGI_FORMAT ToDXGIFormat(RenderTextureFormat format);
	// RTV, SRV, DSV
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ = {};
	uint32_t srvSlot_ = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_ = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ = {};
	Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
	uint32_t depthSrvSlot_ = 0; // 深度を読む窓口（isDepthReadable_ のときだけ）
	// Format
	DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// サイズ
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	// Flag
	bool useDepth_;
	bool isDepthReadable_ = false; // 深度をシェーダーから読めるか
};
```

### B.4 `Graphics/RenderTarget/RenderTexture.cpp`（全体を差し替え）
変えた所：コンストラクタ（assert と `isDepthReadable_`）・デストラクタ（深度の SRV も返す）・`BindWithDepthOf`・`BeginShaderRead`・`EndShaderRead`（新しい）・`CreateDSVResource`（読む窓口）。
```cpp
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

using namespace Microsoft::WRL;


//=============================================================================
// enum format → DXGI_FORMAT変換
//=============================================================================
DXGI_FORMAT RenderTexture::ToDXGIFormat(RenderTextureFormat format) {
	switch (format) {
	case RenderTextureFormat::SDR:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // SDR
	case RenderTextureFormat::HDR:   return DXGI_FORMAT_R16G16B16A16_FLOAT;  // HDR
	case RenderTextureFormat::RG16F: return DXGI_FORMAT_R16G16_FLOAT;        // RG16F
	}
	MY_ASSERT_MSG(false, "未対応のRenderTextureFormatです");
	return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}


//=============================================================================
// コンストラクタ・デストラクタ
//=============================================================================
// ===== コンストラクタ =====
RenderTexture::RenderTexture(uint32_t width, uint32_t height, RenderTextureFormat format, bool useDepth, bool isDepthReadable) {
	MY_ASSERT_MSG(useDepth || !isDepthReadable, "深度を読むには、深度あり（useDepth）で作ってください");
	// これから作るTextureの設定
	width_ = width;
	height_ = height;
	format_ = ToDXGIFormat(format);
	useDepth_ = useDepth;
	isDepthReadable_ = isDepthReadable;

	// RTV, SRV作成
	CreateResource();
	// DSV作成するか
	if (useDepth_) {
		CreateDSVResource();
	}

	LogManager::Log(std::format("RenderTexture created {}x{} SRVSlot={}", width, height, srvSlot_));
}

// ===== デストラクタ =====
// SRVスロットを返す（返さないと、作り直すたびに空きが減り続け、256個を使い切ったところで止まる）
RenderTexture::~RenderTexture() {
	DirectXCommon::FreeSRVSlot(srvSlot_);
	if (isDepthReadable_) {
		DirectXCommon::FreeSRVSlot(depthSrvSlot_);
	}
}


//=============================================================================
// 描画開始
//=============================================================================
void RenderTexture::PreDraw() {
	// ===== ResourceStateを切り替える =====
	// PIXEL_SHADER_RESOURCE（ImGuiが読める状態）→ RENDER_TARGET（描画先として使える状態）
	DirectXCommon::TransitionBarrier(resource_.Get(),
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // ImGuiが読んでいた状態
	    D3D12_RESOURCE_STATE_RENDER_TARGET);        // 描画先として使える状態

	// ===== RTVとDSVをセットしてクリア =====
	auto* cmdList = DirectXCommon::GetCommandList();
	if (useDepth_) {
		cmdList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);
		cmdList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	} else {
		// 深度なし（IBL畳み込みなどフルスクリーンパス用）
		cmdList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
	}
	cmdList->ClearRenderTargetView(rtvHandle_, RenderWindow::kClearColor, 0, nullptr);
}


//=============================================================================
// 描画終了
//=============================================================================
void RenderTexture::PostDraw() {
	// ===== ResourceStateを元に戻す =====
	// RENDER_TARGET（描画先として使える状態）→ PIXEL_SHADER_RESOURCE（ImGuiが読める状態）
	DirectXCommon::TransitionBarrier(resource_.Get(),
	    D3D12_RESOURCE_STATE_RENDER_TARGET,          // 描画先として使っていた状態
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ImGuiが読める状態
}


//=============================================================================
// ほかの RenderTexture の深度で描く
//=============================================================================
void RenderTexture::BindWithDepthOf(RenderTexture& depthSource) {
	MY_ASSERT_MSG(depthSource.useDepth_, "depthSource は深度あり（useDepth）で作ってください");
	// 大きさが違うと、深度と色のピクセルが合わない
	MY_ASSERT_MSG(depthSource.width_ == width_ && depthSource.height_ == height_, "depthSource と大きさが違います");
	DirectXCommon::GetCommandList()->OMSetRenderTargets(1, &rtvHandle_, false, &depthSource.dsvHandle_);
}


//=============================================================================
// CS・PS から読む
//=============================================================================
// ===== 読める状態にする =====
void RenderTexture::BeginShaderRead() {
	// 色：PS だけが読める状態 → CS からも PS からも読める状態（ALL_SHADER_RESOURCE ＝ NON_PIXEL ＋ PIXEL）
	// PIXEL_SHADER_RESOURCE のまま CS で読むと、デバッグレイヤーに止められる
	DirectXCommon::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	// 深度：書く状態 → 読む状態
	if (isDepthReadable_) {
		DirectXCommon::TransitionBarrier(depthResource_.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	}
}

// ===== 元に戻す =====
void RenderTexture::EndShaderRead() {
	DirectXCommon::TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (isDepthReadable_) {
		DirectXCommon::TransitionBarrier(depthResource_.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
}


//=============================================================================
// RTVとSRV共通のリソースを生成してRTV/SRVを登録する
//=============================================================================
void RenderTexture::CreateResource() {
	// Resource
	resource_ = DirectXCommon::CreateRenderTargetTextureResource(width_, height_, format_, RenderWindow::kClearColor);
	resource_->SetName(L"RenderTexture");

	// ===== RTV登録 =====
	// rtvがどこのアドレスにあるか登録
	rtvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvDescriptorHeap_->SetName(L"RenderTexture_RTVHeap");
	// view情報
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format_;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

	// ===== SRV登録 =====
	// srvがどこのアドレスにあるか登録
	srvSlot_ = DirectXCommon::AllocateSRVSlot(); // DescriptorHeapからスロット番号取得
	srvHandleCPU_ = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	srvHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot_);
	// view情報
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format_;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	DirectXCommon::GetDevice()->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}


//=============================================================================
// 深度バッファを生成する
//=============================================================================
void RenderTexture::CreateDSVResource() {
	// Resource（読めるようにするときは、型なしで作る）
	depthResource_ = DirectXCommon::CreateDepthStencilTextureResource(static_cast<int32_t>(width_), static_cast<int32_t>(height_), isDepthReadable_);
	// DSVDescriptorHeap
	dsvDescriptorHeap_ = DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	// DSV
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DirectXCommon::GetDevice()->CreateDepthStencilView(depthResource_.Get(), &dsvDesc, dsvHandle_);
	dsvDescriptorHeap_->SetName(L"RenderTexture_DSVHeap");

	// ===== 深度を読む窓口（SRV）=====
	// 同じ中身を、書くとき（DSV）は D24_UNORM_S8_UINT、読むとき（SRV）は R24_UNORM_X8_TYPELESS として見る
	// R24_UNORM_X8：上の 24bit（深度）を 0〜1 の数として読み、残りの 8bit（ステンシル）は読まない
	if (isDepthReadable_) {
		depthSrvSlot_ = DirectXCommon::AllocateSRVSlot();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, depthSrvSlot_);
		DirectXCommon::GetDevice()->CreateShaderResourceView(depthResource_.Get(), &srvDesc, srvHandle);
	}
}
```

- **`BeginShaderRead` が要る理由**：`PostDraw` の後、色は `PIXEL_SHADER_RESOURCE`（PS だけが読める状態）。そのまま CS で読むと、デバッグレイヤーに止められる。深度は `DEPTH_WRITE`（書く状態）のままなので、読む状態に変える。
- **`BindWithDepthOf`**：ぼかした絵（深度なしの RenderTexture）に、元の絵の深度を張って半透明を描くため（1.5）。

---

## パートC：DOF 本体

### C.1 `Graphics/Pipeline/RootSignatureManager.h`
`RootBind` に 1 つ足す（合成の PS の定数 `gDof`）：
```cpp
	Lens,                 // レンズ効果（PS）
	DepthOfField,         // 被写界深度のパラメータ（合成の PS。CS は変数名で結ぶので、ここは使わない）
	BindlessTexture,      // バインドレステクスチャ
```

### C.2 `Graphics/Pipeline/RootSignatureManager.cpp` の `NameToRole`
```cpp
	    {"gLens",                 RootBind::Lens                },
	    {"gDof",                  RootBind::DepthOfField        },
	    {"gTextures",             RootBind::BindlessTexture     },
```

### C.3 `Graphics/PostEffect/DepthOfFieldPass.h`（新しく作る）
`DepthOfFieldSettings`（シーンの Component から作る値）もここに置く（Graphics が Component を知らなくて済むように）。
```cpp
#pragma once
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "MyEngine/Graphics/Compute/ComputeTexture.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"

// 前方宣言
class Camera;

// 被写界深度の値（DepthOfFieldSystem が、シーンの Depth Of Field Component から毎フレーム作る）
struct DepthOfFieldSettings {
	bool enabled = false;        // false ならぼかさない（Depth Of Field Component が無い・切ってある）
	bool hasFocusPoint = false;  // true：focusPoint の奥行きにピントを合わせる（ビューごとに、そのカメラからの奥行きを計算する）
	Vector3 focusPoint = {};     // ピントを合わせる点（ワールド座標。Camera Follow の Target の位置）
	float focusDistance = 10.0f; // hasFocusPoint が false のときの、ピントの合う奥行き（m）
	float focusRange = 3.0f;     // ピントの前後この奥行き（m）までは、ぼけない
	float farTransition = 40.0f; // その奥から、一番ぼけるまでの奥行き（m）
	float nearTransition = 4.0f; // その手前から、一番ぼけるまでの奥行き（m。0 なら手前はぼかさない）
	float maxBlur = 8.0f;        // 一番ぼけた所の、ぼけの半径（ピクセル。高さ 720 の画面で）
};


/// <summary>
/// 被写界深度（ピントの合った所だけはっきり見え、手前と奥がぼける）
/// <para>CS 1：半分の大きさに縮めて、ピクセルごとのぼけの大きさ（深度から計算）を A に入れる</para>
/// <para>CS 2：ぼけの大きさだけ、まわりの色を混ぜてぼかす</para>
/// <para>PS  ：元の絵と、ぼかした絵を、ぼけの大きさで混ぜる</para>
/// </summary>
class DepthOfFieldPass {
public:
	static constexpr uint32_t kMaxPassPerFrame = 8; // 1フレームに通るビューの上限（Game と Scene で2回通る）
	static constexpr float kMaxBlurLimit = 32.0f;   // ぼけの半径の上限（ピクセル。大きいほど重くなる）

	void Initialize(uint32_t width, uint32_t height);

	/// <summary>
	/// scene の色と深度から、ぼかした絵を作る（CS 2本）
	/// <para>scene は深度を読める RenderTexture（isDepthReadable）。この後に Composite を呼ぶ</para>
	/// </summary>
	void Record(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings);

	/// <summary>
	/// 今バインドされている描画先へ、元の絵とぼかした絵を混ぜて描く（Record の後）
	/// </summary>
	void Composite(float width, float height);

	/// <summary>
	/// フレーム末に定数バッファの参照位置を戻す
	/// </summary>
	void ResetSlot() { slot_ = 0; }

private:
	// シェーダーの DofParam と同じ並びにすること
	struct Param {
		uint32_t fullWidth = 0;            // 元の絵の大きさ
		uint32_t fullHeight = 0;
		uint32_t halfWidth = 0;            // 半分の大きさ（CS のはみ出したスレッドを止める）
		uint32_t halfHeight = 0;
		Vector2 halfTexelSize = {};        // 1 / 半分の大きさ（UV の計算用）
		float projectionA = 0.0f;          // 投影行列の m[2][2]（深度 → 奥行き(m) に戻すのに使う）
		float projectionB = 0.0f;          // 投影行列の m[3][2]
		float focusDistance = 0.0f;        // ピントの合う奥行き（m）
		float focusRange = 0.0f;           // 前後この奥行きまではぼけない（m）
		float farTransition = 1.0f;        // 奥：一番ぼけるまでの奥行き（m）
		float nearTransition = 0.0f;       // 手前：一番ぼけるまでの奥行き（m。0 なら手前はぼかさない）
		float maxRadius = 0.0f;            // 一番ぼけた所の半径（半分の大きさのピクセル）
		float radiusStep = 0.0f;           // ぼかすときに読む点の間隔（半分の大きさのピクセル）
		uint32_t colorIndex = 0;           // 元の絵の色（SRV の番号）
		uint32_t depthIndex = 0;           // 元の絵の深度（SRV の番号）
		uint32_t halfIndex = 0;            // 縮めた色＋ぼけの大きさ（SRV の番号）
		uint32_t blurIndex = 0;            // ぼかした色（SRV の番号）
		float padding[2] = {};             // 16 バイトの区切りにそろえる
	};
	static_assert(sizeof(Param) == 80, "シェーダーの DofParam（16 バイト × 5）と大きさをそろえる");
	static constexpr uint32_t kSlotSize = 256;

	void CreateRootSignature();
	void CreatePSO();
	// 定数（ビューのカメラで、ピントの奥行きと深度の戻し方が変わる）
	Param MakeParam(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings) const;
	// CS を1本動かして、output に書く
	void Dispatch(const char* csName, ComputeTexture& output, D3D12_GPU_DESCRIPTOR_HANDLE heapStart) const;

	RootSignatureInfo rsInfo_{};                           // 合成の PS 用
	Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePSO_;
	std::unique_ptr<ComputeTexture> half_;                 // 半分の大きさの色（RGB）＋ぼけの大きさ（A。マイナスが手前）
	std::unique_ptr<ComputeTexture> blur_;                 // ぼかした色（RGB）＋手前のぼけのはみ出し（A）
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_;
	uint8_t* cbMapped_ = nullptr;
	uint32_t slot_ = 0;
	D3D12_GPU_VIRTUAL_ADDRESS currentCB_ = 0;              // Record で書いた定数バッファ（Composite でも同じ物を使う）
	RenderTexture* scene_ = nullptr;                       // Record で読んだ絵（Composite の後に、読む状態から戻す）
};
```

### C.4 `Graphics/PostEffect/DepthOfFieldPass.cpp`（新しく作る）
```cpp
#define NOMINMAX
#include "DepthOfFieldPass.h"

#include <algorithm>
#include <cstring>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/Compute/ComputePipeline.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/ShaderPackageLoader.h"

using namespace Microsoft::WRL;

namespace {
constexpr const char* kDownsampleCS = "DepthOfFieldDownsampleCS"; // 縮める CS（Post/DepthOfFieldDownsample.CS.hlsl）
constexpr const char* kBlurCS = "DepthOfFieldBlurCS";             // ぼかす CS（Post/DepthOfFieldBlur.CS.hlsl）
constexpr const char* kCompositePS = "DepthOfFieldCompositePS";   // 混ぜる PS（Post/DepthOfFieldComposite.PS.hlsl）
constexpr float kRadiusStep = 0.5f;     // ぼかすときに読む点の間隔（半分の大きさのピクセル。小さいほどなめらかで重い）
constexpr float kReferenceHeight = 720.0f; // Max Blur（ピクセル）を決めた画面の高さ。大きい画面では、同じ見た目になるよう広げる

// ===== ピントの合う奥行き（m）=====
// 点に合わせるときは、その点をカメラの前の向きに測った長さ（真ん中から外れた所でも、同じ奥行きの面にピントが合う）
float CalcFocusDistance(const Camera& camera, const DepthOfFieldSettings& settings) {
	if (!settings.hasFocusPoint) {
		return settings.focusDistance;
	}
	// ビュー行列の3列目が、カメラの前の向き（SceneRenderer::UploadCameraCB の right・up と同じ取り方）
	const Matrix4x4& view = camera.GetViewMatrix();
	const Vector3 forward = {view.m[0][2], view.m[1][2], view.m[2][2]};
	return (std::max)(Dot(settings.focusPoint - camera.GetTranslation(), forward), 0.0f);
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void DepthOfFieldPass::Initialize(uint32_t width, uint32_t height) {
	// 半分の大きさ（ぼかす量が同じなら、読む点の数が 4 分の 1 で済む。ぼけた絵なので細かさは要らない）
	const uint32_t halfWidth = (std::max)((width + 1) / 2, 1u);
	const uint32_t halfHeight = (std::max)((height + 1) / 2, 1u);
	// 色を足し込むので HDR（16bit の小数）にする。A にぼけの大きさ（マイナスもある）を入れる
	half_ = std::make_unique<ComputeTexture>(halfWidth, halfHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, L"DepthOfField_Half");
	blur_ = std::make_unique<ComputeTexture>(halfWidth, halfHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, L"DepthOfField_Blur");
	// ビューごとに別スロットへ書く。コマンドの実行はフレーム末なので使い回すと壊れる
	cb_ = DirectXCommon::CreateMappedUploadBuffer(kSlotSize * kMaxPassPerFrame, reinterpret_cast<void**>(&cbMapped_));
	cb_->SetName(L"DepthOfFieldCB");

	// CS は初めて使うときに作られるので、ここで作っておく（遊んでいる途中で、作る分だけ止まらないように）
	PSOManager::GetComputePipeline(kDownsampleCS);
	PSOManager::GetComputePipeline(kBlurCS);
	CreateRootSignature();
	CreatePSO();
	LogManager::Log("Initialized");
}

//=============================================================================
// RootSignature / PSO（合成の PS）
//=============================================================================
void DepthOfFieldPass::CreateRootSignature() {
	const ShaderReflection& vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS");
	const ShaderReflection& ps = ShaderPackageLoader::GetShaderReflection(kCompositePS);
	auto resources = RootSignatureManager::MergeStages(vs.resources, ps.resources);
	rsInfo_ = RootSignatureManager::MakeRootSignatureInfo(resources);
}

void DepthOfFieldPass::CreatePSO() {
	IDxcBlob* vs = ShaderPackageLoader::GetShaderReflection("FullscreenVS").blob.Get();
	IDxcBlob* ps = ShaderPackageLoader::GetShaderReflection(kCompositePS).blob.Get();

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
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 書く先は SDR の RenderTexture（この後にブルーム・レンズが読む）
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = DirectXCommon::GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&compositePSO_));
	MY_ASSERT_MSG(SUCCEEDED(hr), "DepthOfField PSO作成失敗");
}

//=============================================================================
// 定数
//=============================================================================
DepthOfFieldPass::Param DepthOfFieldPass::MakeParam(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings) const {
	Param param{};
	// --- 大きさ ---
	param.fullWidth = scene.GetWidth();
	param.fullHeight = scene.GetHeight();
	param.halfWidth = half_->GetWidth();
	param.halfHeight = half_->GetHeight();
	param.halfTexelSize = {1.0f / static_cast<float>(param.halfWidth), 1.0f / static_cast<float>(param.halfHeight)};
	// --- 深度（0〜1）→ カメラからの奥行き（m）の戻し方 ---
	// 透視投影は 深度 = A + B / 奥行き（A = m[2][2]、B = m[3][2]）なので、奥行き = B / (深度 − A)
	const Matrix4x4& projection = camera.GetProjectionMatrix();
	param.projectionA = projection.m[2][2];
	param.projectionB = projection.m[3][2];
	// --- ピント ---
	param.focusDistance = CalcFocusDistance(camera, settings);
	param.focusRange = (std::max)(settings.focusRange, 0.0f);
	param.farTransition = (std::max)(settings.farTransition, 0.01f); // 0 で割らないように
	param.nearTransition = (std::max)(settings.nearTransition, 0.0f);
	// --- ぼけの半径：高さ 720 の画面のピクセル → この絵のピクセル → 半分の大きさのピクセル ---
	const float blur = std::clamp(settings.maxBlur, 0.0f, kMaxBlurLimit);
	param.maxRadius = blur * (static_cast<float>(param.fullHeight) / kReferenceHeight) * 0.5f;
	param.radiusStep = kRadiusStep;
	// --- 読むテクスチャの番号 ---
	param.colorIndex = scene.GetSRVSlot();
	param.depthIndex = scene.GetDepthSRVSlot();
	param.halfIndex = half_->GetSRVSlot();
	param.blurIndex = blur_->GetSRVSlot();
	return param;
}

//=============================================================================
// ぼかした絵を作る
//=============================================================================
void DepthOfFieldPass::Record(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings) {
	MY_ASSERT_MSG(slot_ < kMaxPassPerFrame, "1フレームの被写界深度のパス数が上限を超えました");
	auto* cmdList = DirectXCommon::GetCommandList();

	// --- 定数（CS 2本と合成の PS で、同じ物を見る）---
	const Param param = MakeParam(scene, camera, settings);
	const size_t offset = slot_ * kSlotSize;
	std::memcpy(cbMapped_ + offset, &param, sizeof(Param));
	++slot_;
	currentCB_ = cb_->GetGPUVirtualAddress() + offset;

	// --- 元の絵の色と深度を、CS からも読める状態にする（Composite の最後に戻す）---
	scene_ = &scene;
	scene.BeginShaderRead();

	// --- CS が読み書きする窓口のヒープを見せる ---
	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	const D3D12_GPU_DESCRIPTOR_HANDLE heapStart = heaps[0]->GetGPUDescriptorHandleForHeapStart();

	// --- 1. 半分の大きさに縮めて、ぼけの大きさを作る ---
	Dispatch(kDownsampleCS, *half_, heapStart);
	// --- 2. ぼかす ---
	Dispatch(kBlurCS, *blur_, heapStart);
}

// ===== CS を1本動かす =====
void DepthOfFieldPass::Dispatch(const char* csName, ComputeTexture& output, D3D12_GPU_DESCRIPTOR_HANDLE heapStart) const {
	auto* cmdList = DirectXCommon::GetCommandList();
	const ComputePipeline& pipeline = PSOManager::GetComputePipeline(csName);
	output.BeginWrite();
	pipeline.Bind(cmdList);
	pipeline.SetConstantBuffer(cmdList, "gDof", currentCB_);
	pipeline.SetTextureTable(cmdList, "gTextures", heapStart);
	pipeline.SetUnorderedAccessTexture(cmdList, "gOutput", output.GetUAVGPUHandle());
	pipeline.Dispatch(cmdList, output.GetWidth(), output.GetHeight()); // 1スレッド ＝ 半分の大きさの1ピクセル
	output.EndWrite(); // 書き終わってから、次の CS・PS が読む
}

//=============================================================================
// 合成（描画先は呼び出し側が設定済み）
//=============================================================================
void DepthOfFieldPass::Composite(float width, float height) {
	MY_ASSERT_MSG(scene_ != nullptr, "Record を先に呼んでください");
	auto* cmdList = DirectXCommon::GetCommandList();
	D3D12_VIEWPORT vp{0.0f, 0.0f, width, height, 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);

	ID3D12DescriptorHeap* heaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootSignature(rsInfo_.rootSignature.Get());
	cmdList->SetPipelineState(compositePSO_.Get());
	cmdList->SetGraphicsRootConstantBufferView(rsInfo_.slotOf.at(RootBind::DepthOfField), currentCB_);
	cmdList->SetGraphicsRootDescriptorTable(rsInfo_.slotOf.at(RootBind::BindlessTexture), heaps[0]->GetGPUDescriptorHandleForHeapStart());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(3, 1, 0, 0); // フルスクリーン三角形

	// 読み終わったので、元の絵を元の状態へ（次のビューで、また描画先になる）
	scene_->EndShaderRead();
	scene_ = nullptr;
}
```

- **深度 → 奥行き**：透視投影の行列（`MakePerspectiveFovMatrix`）は `m[2][2] = f/(f−n)`、`m[3][2] = −n·f/(f−n)`。深度 `d = A + B / z` なので `z = B / (d − A)`。深度 1（何も描いていない所・空）なら `z = Far`、深度 0 なら `z = Near` になる。
- **ピントの奥行きをビューごとに測る**：`CalcFocusDistance` で、Target の位置をカメラの前の向きに測る。Game ビューのカメラ（16.5m 後ろ）と Scene ビューのカメラ（もっと遠い）で値が違っても、どちらもキューブの面にピントが合う。
- **Max Blur を高さ 720 で決める**：画面が大きくなっても、同じ見た目のぼけ方になるように広げる。

### C.5 `Shader/Data/Post/DepthOfField.hlsli.shader`（新しく作る）
3 本のシェーダーで使う定数と、CoC の計算。
```cpp
#META
stage: Include
path : Post/DepthOfField.hlsli
#META_END

#HLSL
#include "Resources/Texture.hlsli"

// ===== 被写界深度の値（C++ の DepthOfFieldPass::Param と同じ並び）=====
struct DofParam
{
    uint32_t2 fullSize;       // 元の絵の大きさ（ピクセル）
    uint32_t2 halfSize;       // 半分の大きさ（ピクセル。CS のはみ出したスレッドを止める）
    float32_t2 halfTexelSize; // 1 / 半分の大きさ（UV の計算用）
    float projectionA;        // 投影行列の m[2][2]
    float projectionB;        // 投影行列の m[3][2]
    float focusDistance;      // ピントの合う奥行き（m）
    float focusRange;         // 前後この奥行きまではぼけない（m）
    float farTransition;      // 奥：一番ぼけるまでの奥行き（m）
    float nearTransition;     // 手前：一番ぼけるまでの奥行き（m。0 なら手前はぼかさない）
    float maxRadius;          // 一番ぼけた所の半径（半分の大きさのピクセル）
    float radiusStep;         // ぼかすときに読む点の間隔（半分の大きさのピクセル）
    uint32_t colorIndex;      // 元の絵の色（SRV の番号）
    uint32_t depthIndex;      // 元の絵の深度（SRV の番号）
    uint32_t halfIndex;       // 縮めた色＋ぼけの大きさ（SRV の番号）
    uint32_t blurIndex;       // ぼかした色（SRV の番号）
    float32_t2 padding;
};
ConstantBuffer<DofParam> gDof : register(b0);

// ===== 深度バッファの値（0〜1）→ カメラからの奥行き（m）=====
// 透視投影の深度は 深度 = A + B / 奥行き なので、奥行き = B / (深度 − A)
// 深度は手前ほど細かく、奥ほど粗い（0.99 と 1.0 の間に、何百 m も入っている）ので、そのままでは「何 m 奥か」に使えない
float ToViewDepth(float depth)
{
    return gDof.projectionB / (depth - gDof.projectionA);
}

// ===== ぼけの大きさ（-1〜1。0：ピントが合っている / プラス：奥にぼける / マイナス：手前にぼける）=====
// ピントの前後 focusRange まではぼけず、そこから Transition の奥行きで一番ぼける（1）まで増える
float CalcCoc(float depth)
{
    float distance = ToViewDepth(depth) - gDof.focusDistance; // ピントの所から、どれだけ奥か（マイナスは手前）
    float farCoc = saturate((distance - gDof.focusRange) / gDof.farTransition);
    float nearCoc = (gDof.nearTransition > 0.0f) ? saturate((-distance - gDof.focusRange) / gDof.nearTransition) : 0.0f;
    return farCoc - nearCoc;
}
#HLSL_END
```

### C.6 `Shader/Data/Post/DepthOfFieldDownsample.CS.shader`（新しく作る）
```cpp
#META
stage: CS
path : Post/DepthOfFieldDownsample.CS.hlsl
profile : cs_6_0
#META_END

#HLSL
// ===== 被写界深度 1：半分の大きさに縮める（1スレッド = 半分の大きさの1ピクセル = 元の絵の 2×2）=====
// RGB：4つの色の平均 / A：4つのぼけの大きさ（CalcCoc）の平均
#include "Post/DepthOfField.hlsli"

RWTexture2D<float32_t4> gOutput : register(u0); // 縮めた色＋ぼけの大きさ（DepthOfFieldPass の half_）

[numthreads(8, 8, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t2 pixel = dispatchThreadId.xy;
    // 最後のグループには、絵の外のスレッドがいる。何もせず帰る
    if (any(pixel >= gDof.halfSize))
    {
        return;
    }
    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    float coc = 0.0f;
    for (uint32_t i = 0; i < 4; ++i)
    {
        // 元の絵の 2×2（i の 1 の位で横、2 の位で縦にずらす）。絵の大きさが奇数のときは、はみ出さないように端で止める
        uint32_t2 source = min(pixel * 2 + uint32_t2(i & 1, i >> 1), gDof.fullSize - 1);
        color += gTextures[gDof.colorIndex].Load(int32_t3(source, 0)).rgb;
        coc += CalcCoc(gTextures[gDof.depthIndex].Load(int32_t3(source, 0)).r);
    }
    gOutput[pixel] = float32_t4(color * 0.25f, coc * 0.25f);
}
#HLSL_END
```

### C.7 `Shader/Data/Post/DepthOfFieldBlur.CS.shader`（新しく作る）
```cpp
#META
stage: CS
path : Post/DepthOfFieldBlur.CS.hlsl
profile : cs_6_0
#META_END

#HLSL
// ===== 被写界深度 2：ぼかす（1スレッド = 半分の大きさの1ピクセル）=====
// まわりの点を、うずまき（黄金角ずつ回して、外へ広げる）の順に読んで混ぜる
// 読んだ点のぼけの大きさが「その点までの距離」より大きければ、その点の色はここまでにじんでくる、と考える
// （ぼけた点は、自分のぼけの半径の円に色を広げる。それを、広げられる側から集める書き方）
#include "Post/DepthOfField.hlsli"

RWTexture2D<float32_t4> gOutput : register(u0); // ぼかした色＋手前のぼけのはみ出し（DepthOfFieldPass の blur_）

static const float kGoldenAngle = 2.39996323f; // 黄金角（ラジアン。約 137.5 度）。この角度ずつ回すと、点が偏らずに円に散らばる

[numthreads(8, 8, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t2 pixel = dispatchThreadId.xy;
    if (any(pixel >= gDof.halfSize))
    {
        return;
    }
    float32_t4 center = gTextures[gDof.halfIndex].Load(int32_t3(pixel, 0));
    float centerSize = abs(center.a) * gDof.maxRadius; // 自分のぼけの半径（ピクセル）
    float32_t3 sum = center.rgb; // 混ぜた色の合計
    float count = 1.0f;          // 混ぜた数
    float nearSpill = max(-center.a, 0.0f); // 手前のぼけが、ここにどれだけかぶっているか（合成で使う）

    float angle = 0.0f;
    [loop]
    for (float radius = gDof.radiusStep; radius < gDof.maxRadius; radius += gDof.radiusStep / radius)
    {
        // --- 次の点（radius += 間隔 / radius：外ほど円周が長いので、1 周に置く点を増やす。円の中に点がほぼ同じ濃さで並ぶ）---
        angle += kGoldenAngle;
        float32_t2 uv = (float32_t2(pixel) + 0.5f + float32_t2(cos(angle), sin(angle)) * radius) * gDof.halfTexelSize;
        uv = clamp(uv, gDof.halfTexelSize * 0.5f, 1.0f - gDof.halfTexelSize * 0.5f); // 絵の外を読まない（サンプラーが WRAP なので、反対の端が混ざる）
        // sample は HLSL の予約語（補間の指定）なので、変数名に使えない
        float32_t4 neighbor = gTextures[gDof.halfIndex].SampleLevel(gSampler, uv, 0.0f);

        // --- その点のぼけの半径 ---
        float neighborSize = abs(neighbor.a) * gDof.maxRadius;
        // 自分より奥の点は、自分の 2 倍より大きくにじませない
        // （ピントの合った物に、後ろのぼけた背景がかぶらない。背景はピントの合った物の後ろにあるので、前へ出てこない）
        if (neighbor.a > center.a)
        {
            neighborSize = min(neighborSize, centerSize * 2.0f);
        }
        // --- ぼけの半径が、ここまでの距離より大きいほど混ぜる（境目の 1 ピクセルはなめらかに）---
        float weight = smoothstep(radius - 0.5f, radius + 0.5f, neighborSize);
        sum += lerp(sum / count, neighbor.rgb, weight); // 混ぜない点は、今までの平均を足す（平均を変えない）
        count += 1.0f;
        nearSpill = max(nearSpill, max(-neighbor.a, 0.0f) * weight);
    }
    gOutput[pixel] = float32_t4(sum / count, nearSpill);
}
#HLSL_END
```

- **点の数**：`radius += 間隔 / radius` で外へ進む。半径 R までの点の数は約 `R² ÷ (2 × 間隔)`。Max Blur 8（半分の大きさで半径 4）なら 16 個、Max Blur 32 なら 256 個（重い）。
- **`sample` は HLSL の予約語**（G8 の `line` と同じ）。変数名に使うとコンパイルで止まる。

### C.8 `Shader/Data/Post/DepthOfFieldComposite.PS.shader`（新しく作る）
```cpp
#META
stage: PS
path : Post/DepthOfFieldComposite.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
// ===== 被写界深度 3：元の絵と、ぼかした絵を混ぜる（元の大きさ）=====
// ぼけの大きさは元の大きさの深度で計算し直す（半分の大きさのままだと、ピントの合った物の縁までぼける）
#include "Post/DepthOfField.hlsli"

struct VSOutput
{
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
};

float32_t4 main(VSOutput input) : SV_TARGET
{
    int32_t3 pixel = int32_t3(input.position.xy, 0);
    float32_t3 sharp = gTextures[gDof.colorIndex].Load(pixel).rgb;
    float coc = abs(CalcCoc(gTextures[gDof.depthIndex].Load(pixel).r));
    // ぼかした絵は半分の大きさなので、線形補間で引き伸ばして読む（端は WRAP で反対側を読まないように止める）
    float32_t2 uv = clamp(input.texcoord, gDof.halfTexelSize * 0.5f, 1.0f - gDof.halfTexelSize * 0.5f);
    float32_t4 blur = gTextures[gDof.blurIndex].SampleLevel(gSampler, uv, 0.0f);
    // 自分のぼけと、手前のぼけのかぶり（blur.a）の大きい方。半径（元の大きさのピクセル）が 0.5 から 2 の間で、ぼかした絵に切り替える
    float radius = max(coc, blur.a) * gDof.maxRadius * 2.0f;
    float t = smoothstep(0.5f, 2.0f, radius);
    return float32_t4(lerp(sharp, blur.rgb, t), 1.0f);
}
#HLSL_END
```

### C.9 `Graphics/PostEffect/PostProcess.h`（全体を差し替え）
足した物：`ApplyDepthOfField`・`depthOfField_`・`depthOfFieldRT_`・`class Camera;`。`Prepare` はそのまま。
```cpp
#pragma once
#include <memory>
#include "MyEngine/Graphics/PostEffect/LensPass.h"
#include "MyEngine/Graphics/PostEffect/BloomPass.h"
#include "MyEngine/Graphics/PostEffect/DepthOfFieldPass.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

// 前方宣言
class Camera;

// ポストエフェクトをどこまで掛けるか（SceneRenderer::RenderToTexture に渡す）
enum class PostEffectMode {
	None,   // 掛けない
	All,    // 全部（Game ビュー・製品版）
	NoLens, // レンズ（歪み・色収差・周辺を暗く）だけ外す。Scene ビュー用（絵を歪ませると、ギズモやクリックで選ぶ位置が物とずれる）
};

/// <summary>
/// ポストエフェクトをまとめて順に掛ける（被写界深度 → ブルーム → レンズ）。ブルーム・レンズの有無と数値は GlobalVariables で持つ
/// <para>被写界深度の値はシーンの Depth Of Field Component から（ApplyDepthOfField に渡す）</para>
/// </summary>
class PostProcess {
public:
	void Initialize(uint32_t width, uint32_t height);

	/// <summary>
	/// 被写界深度を掛ける（3D の途中。不透明な物と空を描いた後、半透明を描く前に呼ぶ）
	/// </summary>
	/// <param name="scene">不透明な物と空を描いた絵（深度を読める RenderTexture。PostDraw 済み）</param>
	/// <returns>ぼかした絵。描画先として張ったまま（深度は scene の物）なので、この後に半透明を描いて PostDraw する</returns>
	RenderTexture* ApplyDepthOfField(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings);

	void Prepare(RenderTexture& scene);
	void Composite(float width, float height, bool useLens = true); // useLens が false ならレンズを掛けずに通す（Scene ビュー）
	void ResetSlot();
	BloomPass& GetBloomPass() { return bloom_; }
	LensPass& GetLensPass() { return lens_; }

private:
	// --- 調整項目 ---
	int kBloomEnabled = 1; // GlobalVariables は bool を持たないので 0/1 で扱う
	int kLensEnabled = 1;

	void RegisterGV();
	void ApplyGV();

	RenderTexture* source_ = nullptr; // Prepare が決めた、レンズへの入力
	DepthOfFieldPass depthOfField_;
	BloomPass bloom_;
	LensPass lens_;
	std::unique_ptr<RenderTexture> depthOfFieldRT_; // 被写界深度の合成の置き場
	std::unique_ptr<RenderTexture> work_;           // ブルーム合成の置き場
};
```

### C.10 `Graphics/PostEffect/PostProcess.cpp`
`Initialize` の最初の 2 行を：
```cpp
void PostProcess::Initialize(uint32_t width, uint32_t height) {
	depthOfFieldRT_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::SDR, false);
	work_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::SDR, false);
	depthOfField_.Initialize(width, height);
	bloom_.Initialize(width, height);
```

`Prepare` の下に足す：
```cpp
//=============================================================================
// 被写界深度（3D の途中で掛ける）
//=============================================================================
// ブルームより先（ぼけた明るい所が、ぼけた形のまま光るように）
RenderTexture* PostProcess::ApplyDepthOfField(RenderTexture& scene, const Camera& camera, const DepthOfFieldSettings& settings) {
	depthOfField_.Record(scene, camera, settings);
	depthOfFieldRT_->PreDraw();
	depthOfField_.Composite(static_cast<float>(depthOfFieldRT_->GetWidth()), static_cast<float>(depthOfFieldRT_->GetHeight()));
	// この後に描く半透明が、不透明な物の後ろに隠れるように、scene の深度を張る（Composite の中で、深度は書く状態に戻っている）
	depthOfFieldRT_->BindWithDepthOf(scene);
	return depthOfFieldRT_.get();
}
```

`ResetSlot` の最初に 1 行：
```cpp
void PostProcess::ResetSlot() {
	depthOfField_.ResetSlot();
	bloom_.ResetSlot();
```

---

## パートD：描く順番・ビューごとの設定・グリッド

### D.1 `Graphics/Pipeline/RenderStates.h`
`BillboardMode` の下に足す：
```cpp
// 描く物の層（ビューごとに、描く・描かないを選べる。Unity の Layer とカメラの Culling Mask の小さい版）
enum class DrawLayer {
	Default,    // ふつうの物（いつも描く）
	EditorGrid, // エディタのグリッド（Scene・Game ビューで別々に切れる。View メニュー）
};
```

### D.2 `Graphics/Renderer/DrawRequest.h` の `MeshRequest`
「その他」に 1 行（`windowTitle` の下）：
```cpp
	std::wstring windowTitle; // ウィンドウ名識別時に使用
	DrawLayer layer = DrawLayer::Default;      // 層（ビューごとに描く・描かないを選ぶ。RenderQueue::SetLayerVisible）
```

### D.3 `Graphics/Renderer/Renderer.h` の `ModelConfig`
最後に 1 行（`windowTitle` の下）：
```cpp
		std::wstring windowTitle = L"";                            // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
		DrawLayer layer = DrawLayer::Default;                      // 層（エディタのグリッドのように、ビューごとに切りたい物だけ変える）
```

### D.4 `Graphics/Renderer/Renderer.cpp` の `DrawModel`
`req.windowTitle = config.windowTitle;` の下に 1 行：
```cpp
			req.windowTitle = config.windowTitle;
			req.layer = config.layer;
			req.debugName = &ModelManager::GetModelName(config.modelHandle);
```

### D.5 `Graphics/Renderer/RenderQueue.h`
include を 2 つ（`<array>` と magic_enum）：
```cpp
#pragma once
#include <array>
#include <string>
#include <memory>
#include <vector>
#include <variant>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Graphics/Renderer/DrawRequest.h"
```

`Clear` の下に足す：
```cpp
	// フレーム末にクリア
	static void Clear();
	// この後に描くビューで、その層（DrawLayer）の物を描くか（エディタのグリッドを Game ビューだけ消す、など）
	// SceneRenderer が、ビューを描く前に毎回決める
	static void SetLayerVisible(DrawLayer layer, bool isVisible);
```

メンバーの最後に足す：
```cpp
	std::vector<LineRequest> lineRequests_;
	// 層ごとに、今のビューで描くか（番号は DrawLayer の順。magic_enum::enum_count で層の数を数える）
	std::array<bool, magic_enum::enum_count<DrawLayer>()> isLayerVisible_{};
```

> **`magic_enum::enum_count<DrawLayer>()`**：enum の値の数（ここでは 2）をコンパイルのときに数える（magic_enum は外部ライブラリ。RootSignatureManager でも使っている）。層を足しても、配列の大きさを直さなくてよい。

### D.6 `Graphics/Renderer/RenderQueue.cpp`
`Initialize` に 1 行：
```cpp
void RenderQueue::Initialize() {
	instance_ = std::make_unique<RenderQueue>(ConstructorKey());
	instance_->isLayerVisible_.fill(true); // 初めは全部の層を描く（fill：配列の全部の要素を同じ値にする）
	LogManager::Log("Initialized");
```

`Clear` の下に足す：
```cpp
// ===== 層ごとに描くか =====
void RenderQueue::SetLayerVisible(DrawLayer layer, bool isVisible) { instance_->isLayerVisible_[static_cast<size_t>(layer)] = isVisible; }
```

`FlushMeshList` のループの最初（`windowTitle` の `continue` の下）に足す：
```cpp
		if (req.windowTitle != windowTitle && req.windowTitle != L"") {
			continue;
		}
		// このビューで切ってある層の物は描かない（Game ビューのグリッドなど）
		if (!instance_->isLayerVisible_[static_cast<size_t>(req.layer)]) {
			continue;
		}
```

> **`std::array::fill(値)`**：配列の全部の要素を同じ値にする。

### D.7 `Editor/Viewport/EditorGrid.cpp` の `Initialize`
最後に 1 行：
```cpp
	model_.rasterizerType = RasterizerType::SolidNone;
	model_.layer = DrawLayer::EditorGrid; // Scene・Game ビューで別々に切れるように（View メニュー）
```

### D.8 `Graphics/Renderer/SceneRenderer.h`（全体を差し替え）
変えた所：`SceneViewOptions`（新しい）・`RenderToTexture` の引数・`SetViewFogEnabled` を消して `SetDepthOfField`・private に `ApplyViewOptions`・`GetViewDepthOfField`・`RenderWorldForPost`・`RenderOpaque`・`RenderTransparent`・メンバーの `isViewFogEnabled_` を消して `depthOfField_`・`viewOptions_`。
```cpp
#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "MyEngine/Scene/Skybox.h"
#include "MyEngine/Graphics/PostEffect/PostProcess.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"

// 前方宣言
class Skybox;
class Camera;
class RenderWindow;
class RenderTexture;

// ビューごとに切れる物（エディタの View メニュー）。SceneRenderer::RenderToTexture に渡す
// クラスの外に置くのは、引数の既定値（= {}）に使うため（クラスの中の struct の初期値は、クラスを書き終わるまで使えない）
struct SceneViewOptions {
	bool fog = true;          // 霧（Fog Component）を掛ける
	bool depthOfField = true; // 被写界深度（Depth Of Field Component）を掛ける（ポストエフェクトを掛けるときだけ）
	bool editorGrid = true;   // エディタのグリッド（DrawLayer::EditorGrid）を描く
};


/// <summary>
/// 1カメラ分のシーンを RenderTextureに描くクラス
/// </summary>
class SceneRenderer {
public:
	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class SceneRenderer;
	};
	explicit SceneRenderer(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	static void Initialize();
	static void Release();
	static void InitializeSkybox();
	static void InitializePost(uint32_t width, uint32_t height);

	// --- camera視点で targetにシーン1回分を描く ---
	// Textureに描く（options：このビューだけ霧・被写界深度・グリッドを切るとき）
	static void RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, PostEffectMode mode = PostEffectMode::All,
		const SceneViewOptions& options = {});
	// Windowに描く
	static void RenderToWindow(const Camera* camera, RenderWindow* window, const std::wstring& windowTitle, float width, float height);

	// カメラのリングバッファ参照位置リセット
	static void ResetViewIndex() {
		instance_->cameraSlot_ = 0;
		instance_->skybox_->ResetSlot();
		if (instance_->postProcess_) {
			instance_->postProcess_->ResetSlot();
		}
	}

	static Skybox* GetSkybox() { return instance_->skybox_.get(); }
	static PostProcess* GetPost() { return instance_->postProcess_.get(); }

	// --- 霧（カメラの定数バッファに入れて、3D のシェーダーが色に混ぜる）---
	// 霧の値（FogSystem が、シーンの Fog Component から毎フレーム書く。Fog Component が無ければ「霧なし」を書く）
	static void SetFog(const FogData& fog) { instance_->fog_ = fog; }

	// --- 被写界深度（ポストエフェクトで、ピントの前後を深度でぼかす）---
	// 被写界深度の値（DepthOfFieldSystem が、シーンの Depth Of Field Component から毎フレーム書く。無ければ「ぼかさない」を書く）
	static void SetDepthOfField(const DepthOfFieldSettings& settings) { instance_->depthOfField_ = settings; }


private:
	~SceneRenderer() = default;
	// このビューだけの設定を覚える（霧はカメラの定数バッファ、被写界深度はポストエフェクト、グリッドは RenderQueue が見る）
	static void ApplyViewOptions(const SceneViewOptions& options);
	// このビューに掛ける被写界深度（View メニューで切ってあれば「ぼかさない」）
	static DepthOfFieldSettings GetViewDepthOfField();
	static void SetViewportAndScissor(float width, float height);
	static void RenderInternal(const Camera* camera, const std::wstring& windowTitle, float width, float height);
	static void RenderWorld(const Camera* camera, const std::wstring& windowTitle, float width, float height);
	static RenderTexture* RenderWorldForPost(const Camera* camera, const std::wstring& windowTitle);
	static void RenderOpaque(const Camera* camera, D3D12_GPU_VIRTUAL_ADDRESS camCB, const std::wstring& windowTitle);
	static void RenderTransparent(D3D12_GPU_VIRTUAL_ADDRESS camCB, const std::wstring& windowTitle);
	static void RenderUI(const std::wstring& windowTitle, float width, float height);
	static D3D12_GPU_VIRTUAL_ADDRESS UploadCameraCB(const Camera* camera);

	// シングルトン
	friend struct std::default_delete<SceneRenderer>;
	static std::unique_ptr<SceneRenderer> instance_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<RenderTexture> postRT_;
	std::unique_ptr<PostProcess> postProcess_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraCB_;
	uint8_t* cameraCBMapped_ = nullptr;
	uint32_t cameraSlot_ = 0;

	FogData fog_;                       // 霧の値（初期値は霧なし）
	DepthOfFieldSettings depthOfField_; // 被写界深度の値（初期値はぼかさない）
	SceneViewOptions viewOptions_;      // 今描いているビューの設定（RenderToTexture・RenderToWindow が毎回決める）
};
```

> **`SceneViewOptions` をクラスの外に置く理由**：引数の既定値 `= {}` に使うため。クラスの中に書いた struct の初期値（`bool fog = true;`）は、外側のクラスを書き終わるまで使えない決まりがあり、`= {}` がコンパイルエラーになる。

### D.9 `Graphics/Renderer/SceneRenderer.cpp`（全体を差し替え）
変えた所：`InitializePost`（深度を読める postRT_）・`ApplyViewOptions`・`GetViewDepthOfField`（新しい）・`RenderWorld` を `RenderOpaque` と `RenderTransparent` に分けた・`RenderWorldForPost`（新しい。DOF を不透明と半透明の間に挟む）・`RenderToTexture`・`RenderToWindow`・`UploadCameraCB` の霧の行。
```cpp
#define NOMINMAX
#include "SceneRenderer.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
#include "MyEngine/Graphics/Renderer/RenderQueue.h"
#include "MyEngine/Scene/Skybox.h"

std::unique_ptr<SceneRenderer> SceneRenderer::instance_ = nullptr;
namespace {
static constexpr uint32_t kMaxViewsPerFrame = 4;
static constexpr uint32_t kCameraSlots = 8;
static constexpr uint32_t kCameraSlotSize = 256;
}


//=============================================================================
// 初期化 / 解放
//=============================================================================
// ===== 初期化 =====
void SceneRenderer::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = std::make_unique<SceneRenderer>(ConstructorKey());
	// GPUに送るカメラ構造体をMapして作成
	instance_->cameraCB_ = DirectXCommon::CreateMappedUploadBuffer(kCameraSlotSize * kMaxViewsPerFrame,
		reinterpret_cast<void**>(&instance_->cameraCBMapped_));
	// 天球生成。初期化はまだ
	instance_->skybox_ = std::make_unique<Skybox>();
	LogManager::Log("Initialized");
}

// ===== 天球 =====
void SceneRenderer::InitializeSkybox() { instance_->skybox_->Initialize(); }

// ===== ポストエフェクト =====
void SceneRenderer::InitializePost(uint32_t width, uint32_t height) {
	// 3D を描く置き場。被写界深度が深度を読むので、深度を読める形で作る
	instance_->postRT_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::SDR, true, true);
	instance_->postProcess_ = std::make_unique<PostProcess>();
	instance_->postProcess_->Initialize(width, height);
}

// ===== 解放 =====
void SceneRenderer::Release() {
	instance_.reset();
}


//=============================================================================
// ビューごとの設定
//=============================================================================
// ===== このビューだけの設定を覚える =====
void SceneRenderer::ApplyViewOptions(const SceneViewOptions& options) {
	instance_->viewOptions_ = options;
	RenderQueue::SetLayerVisible(DrawLayer::EditorGrid, options.editorGrid); // グリッドは描く物を1回だけ積み、ビューごとに飛ばす
}

// ===== このビューに掛ける被写界深度 =====
DepthOfFieldSettings SceneRenderer::GetViewDepthOfField() {
	DepthOfFieldSettings settings = instance_->depthOfField_;
	settings.enabled = settings.enabled && instance_->viewOptions_.depthOfField;
	return settings;
}


//=============================================================================
// Viewport と Scissorの設定
//=============================================================================
void SceneRenderer::SetViewportAndScissor(float width, float height) {
	D3D12_VIEWPORT vp{0.0f, 0.0f, width, height, 0.0f, 1.0f};
	D3D12_RECT sc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	auto* cmdList = DirectXCommon::GetCommandList();
	cmdList->RSSetViewports(1, &vp);
	cmdList->RSSetScissorRects(1, &sc);
}


//=============================================================================
// シーン描画の本体（描画先は呼び出し側が設定済みであること）
//=============================================================================
void SceneRenderer::RenderInternal(const Camera* camera, const std::wstring& windowTitle, float width, float height) {
	RenderWorld(camera, windowTitle, width, height);
	RenderUI(windowTitle, width, height);
}

// ===== 3D（ポストエフェクトを掛けないとき）=====
void SceneRenderer::RenderWorld(const Camera* camera, const std::wstring& windowTitle, float width, float height) {
	SetViewportAndScissor(width, height);
	D3D12_GPU_VIRTUAL_ADDRESS camCB = camera ? UploadCameraCB(camera) : 0;
	RenderOpaque(camera, camCB, windowTitle);
	RenderTransparent(camCB, windowTitle);
}

// ===== 3D（ポストエフェクトを掛けるとき。postRT_ へ描き、被写界深度まで掛けた絵を返す）=====
// 半透明・線・パーティクルは、被写界深度でぼかした後に描く（深度を書かないので、深度でぼけ方を決められない。
// 先に描くと、後ろにある物（空など）と同じだけぼける。Unreal の半透明の「Render After DOF」と同じ考え）
RenderTexture* SceneRenderer::RenderWorldForPost(const Camera* camera, const std::wstring& windowTitle) {
	RenderTexture* work = instance_->postRT_.get();
	const float width = static_cast<float>(work->GetWidth());
	const float height = static_cast<float>(work->GetHeight());
	const DepthOfFieldSettings depthOfField = GetViewDepthOfField();
	D3D12_GPU_VIRTUAL_ADDRESS camCB = camera ? UploadCameraCB(camera) : 0;

	// --- 不透明な物と空 ---
	work->PreDraw();
	SetViewportAndScissor(width, height);
	RenderOpaque(camera, camCB, windowTitle);
	// --- 被写界深度を掛けないなら、そのまま半透明も描く ---
	if (!depthOfField.enabled || camera == nullptr) {
		RenderTransparent(camCB, windowTitle);
		work->PostDraw();
		return work;
	}
	// --- 被写界深度：ぼかした絵へ、元の深度で前後を比べながら半透明を描く ---
	work->PostDraw();
	RenderTexture* blurred = instance_->postProcess_->ApplyDepthOfField(*work, *camera, depthOfField);
	SetViewportAndScissor(width, height);
	RenderTransparent(camCB, windowTitle);
	blurred->PostDraw();
	return blurred;
}

// ===== 不透明な物と空 =====
void SceneRenderer::RenderOpaque(const Camera* camera, D3D12_GPU_VIRTUAL_ADDRESS camCB, const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	RenderQueue::FlushOpaqueMesh(windowTitle, camCB); // 不透明Model（関数内で計測済み）
	if (instance_->skybox_) {
		GPU_SCOPE(cmdList, "Skybox");
		instance_->skybox_->Draw(camera, camCB);
	}
}

// ===== 深度を書かない物（半透明・線・パーティクル）=====
void SceneRenderer::RenderTransparent(D3D12_GPU_VIRTUAL_ADDRESS camCB, const std::wstring& windowTitle) {
	auto* cmdList = DirectXCommon::GetCommandList();
	RenderQueue::FlushTransparentMesh(windowTitle, camCB); // 半透明Model（関数内で計算済み）
	{
		GPU_SCOPE(cmdList, "Line");
		RenderQueue::FlushLine(windowTitle, camCB);
	}
	{
		GPU_SCOPE(cmdList, "Particle");
		RenderQueue::FlushParticle(windowTitle, camCB);
	}
}

// ===== 2D。ポストエフェクトの後に重ねるので、ぼけたり歪んだりしない =====
void SceneRenderer::RenderUI(const std::wstring& windowTitle, float width, float height) {
	auto* cmdList = DirectXCommon::GetCommandList();
	SetViewportAndScissor(width, height);
	GPU_SCOPE(cmdList, "2D");
	RenderQueue::Flush2d(windowTitle);
}


//=============================================================================
// シーン一括描画
//=============================================================================
// ===== Texture =====
void SceneRenderer::RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, PostEffectMode mode, const SceneViewOptions& options) {
	ApplyViewOptions(options);
	// ポストエフェクトを使わないときはそのまま描く
	if (mode == PostEffectMode::None || !instance_->postProcess_) {
		target->PreDraw();
		RenderInternal(camera, windowTitle, static_cast<float>(target->GetWidth()), static_cast<float>(target->GetHeight()));
		target->PostDraw();
		return;
	}
	float width = static_cast<float>(target->GetWidth());
	float height = static_cast<float>(target->GetHeight());
	// 一旦別のテクスチャへ描き、光の層を作ってから target へ合成する
	RenderTexture* scene = RenderWorldForPost(camera, windowTitle);
	instance_->postProcess_->Prepare(*scene); // 張り替える処理はここで全部済ませる
	target->PreDraw();                       // その後に描画先を張る
	instance_->postProcess_->Composite(width, height, mode != PostEffectMode::NoLens);
	RenderUI(windowTitle, width, height); // UIは効果の外側に重ねる
	target->PostDraw();
}

// ===== Window =====
void SceneRenderer::RenderToWindow(const Camera* camera, RenderWindow* window, const std::wstring& windowTitle, float width, float height) {
	ApplyViewOptions({}); // ウィンドウ（製品版のゲーム）は全部掛ける
	if (!instance_->postProcess_) {
		RenderInternal(camera, windowTitle, width, height);
		return;
	}
	RenderTexture* scene = RenderWorldForPost(camera, windowTitle);

	// 効果のパスでRTVが切り替わっているので、ウィンドウへ張り直す
	instance_->postProcess_->Prepare(*scene);
	window->BindRenderTarget(); // 描画先はウィンドウ
	instance_->postProcess_->Composite(width, height);
	RenderUI(windowTitle, width, height); // UIは効果の外側に重ねる
}

//=============================================================================
// GPU転送用のカメラを更新
//=============================================================================
// ===== UploadCameraCB：% をやめて Assert =====
D3D12_GPU_VIRTUAL_ADDRESS SceneRenderer::UploadCameraCB(const Camera* camera) {
	MY_ASSERT_MSG(instance_->cameraSlot_ < kMaxViewsPerFrame, "1フレームのビュー数が kMaxViewsPerFrame を超えました");
	uint32_t slot = instance_->cameraSlot_++;
	auto* dst = reinterpret_cast<CameraData*>(instance_->cameraCBMapped_ + slot * kCameraSlotSize);
	// --- 転送情報 ---
	const Matrix4x4& v = camera->GetViewMatrix();
	dst->viewProj = v * camera->GetProjectionMatrix();
	dst->worldPosition = camera->GetTranslation();
	dst->right = {v.m[0][0], v.m[1][0], v.m[2][0]};
	dst->up = {v.m[0][1], v.m[1][1], v.m[2][1]};
	// 霧：切ってあるビュー（エディタの Scene ビュー）には「霧なし」（濃さの上限が 0）を送る
	dst->fog = instance_->viewOptions_.fog ? instance_->fog_ : FogData{};
	return instance_->cameraCB_->GetGPUVirtualAddress() + slot * kCameraSlotSize;
}
```

### D.10 `Editor/Viewport/EditorViewport.h`
`// ===== ゲッター =====` から `#endif` までを差し替える（`GetShowSceneEffects`・`GetShowSceneFog` を消して `ViewFlags` 1 つに）：
```cpp
#ifdef USE_IMGUI
	// View メニューで切り替える、ビューごとの見え方（全ウィンドウ共通）
	struct ViewFlags {
		bool sceneEffects = true;      // Scene ビューにもポストエフェクトを掛ける（レンズは Scene では掛けない）
		bool sceneFog = true;          // Scene ビューにも霧（Fog Component）を掛ける。Game ビューにはいつも掛ける
		bool sceneDepthOfField = true; // Scene ビューにも被写界深度（Depth Of Field Component）を掛ける（Post Effects が入っているときだけ）
		bool sceneGrid = true;         // Scene ビューにグリッドを描く
		bool gameGrid = false;         // Game ビューにグリッドを描く（遊ぶときと同じ見た目にしたいので、初めは描かない）
	};
#endif

	// ===== ゲッター =====
#ifdef USE_IMGUI
	DebugCamera* GetDebugCamera() { return debugCamera_.get(); }
	bool IsGameHovered() const { return gameView_.IsHovered(); }
	bool IsSceneHovered() const { return sceneView_.IsHovered(); }
	// View メニューの印（ImGui::MenuItem に、メンバーのアドレスを渡して切り替える）
	static ViewFlags& GetViewFlags();
#endif
```

private の `bool isShowGrid_ = true;` を**消す**（`ViewFlags` に移した）：
```cpp
	// グリッドなど
	std::unique_ptr<EditorGrid> grid_;
	bool isShowCameraFrustum_ = true;
```

### D.11 `Editor/Viewport/EditorViewport.cpp`
`Draw` の `if (isShowGrid_)` を：
```cpp
	// どちらかのビューで描くなら積む（描かないビューでは、RenderQueue が飛ばす）
	const ViewFlags& flags = GetViewFlags();
	if (flags.sceneGrid || flags.gameGrid) {
		grid_->Draw();
	}
```

`Render` の「同じキューを2カメラで2回描く」の所を：
```cpp
	// --- 同じキューを2カメラで2回描く ---
	const ViewFlags& flags = GetViewFlags();
	// Scene はレンズだけ外す（絵を歪ませると、ギズモ・クリックで選ぶ位置が物とずれる）。View メニューで全部切れる
	const PostEffectMode sceneMode = flags.sceneEffects ? PostEffectMode::NoLens : PostEffectMode::None;
	// 霧・被写界深度も Scene だけ切れる（遠くの物を霧やぼけに隠さずに置きたいとき）。グリッドは Scene・Game 別々に切れる
	const SceneViewOptions sceneOptions = {.fog = flags.sceneFog, .depthOfField = flags.sceneDepthOfField, .editorGrid = flags.sceneGrid};
	const SceneViewOptions gameOptions = {.editorGrid = flags.gameGrid}; // 書かない物は初期値の true（霧・被写界深度はいつも掛ける）
	SceneRenderer::RenderToTexture(debugCamera_.get(), sceneRT, windowTitle, sceneMode, sceneOptions); // Scene：レンズ以外
	SceneRenderer::RenderToTexture(gameCamera, gameRT, windowTitle, PostEffectMode::All, gameOptions);  // Game：全部
```

`GetShowSceneEffects`・`GetShowSceneFog` の 2 つの関数を消して、代わりに：
```cpp
//=============================================================================
// View メニューの印
//=============================================================================
EditorViewport::ViewFlags& EditorViewport::GetViewFlags() {
	static ViewFlags flags; // 全ウィンドウ共通（初期値は ViewFlags に書いた値）
	return flags;
}
```

> **`{.fog = ..., .editorGrid = ...}`（指示付き初期化子・C++20）**：struct のメンバーを名前で指定して作る。書かなかったメンバーは、struct に書いた初期値（ここでは `true`）になる。並びは struct と同じ順に書く。

### D.12 `Editor/ImGuiManager.cpp` の View メニュー
`// Scene ビューの絵` から `ImGui::EndMenu();` の前までを：
```cpp
			// Scene ビューの絵（ブルームなど。レンズは Scene では掛けない）
			EditorViewport::ViewFlags& viewFlags = EditorViewport::GetViewFlags();
			ImGui::SeparatorText("Scene View");
			ImGui::MenuItem("Post Effects", nullptr, &viewFlags.sceneEffects);
			ImGui::MenuItem("Fog", nullptr, &viewFlags.sceneFog);
			ImGui::MenuItem("Depth of Field", nullptr, &viewFlags.sceneDepthOfField, viewFlags.sceneEffects); // Post Effects を切っていると押せない
			ImGui::MenuItem("Grid###sceneGrid", nullptr, &viewFlags.sceneGrid);
			// Game ビューの絵
			ImGui::SeparatorText("Game View");
			ImGui::MenuItem("Grid###gameGrid", nullptr, &viewFlags.gameGrid);
```

- `ImGui::MenuItem` の 4 つ目の引数は「押せるか」。Post Effects を切っていると DOF も掛からないので、押せなくする。
- `Grid###sceneGrid` と `Grid###gameGrid`：同じ「Grid」の名前が 2 つあると ImGui が見分けられないので、`###` の後ろで別の ID にする。

![Game ビューのグリッド](images/g11_game_grid.png)
（View → Game View → Grid を入れた所。Game ビューにもグリッドが出る）

---

## パートE：Depth Of Field Component

### E.1 `Components/DepthOfFieldComponent.h`（新しく作る）
```cpp
#pragma once
#include <type_traits>

// 被写界深度（ピントの合った所だけはっきり見え、手前と奥がぼける）。シーンに1つ、どの Entity に付けてもよい Component（カテゴリ：Rendering3D）
// 値は DepthOfFieldSystem が毎フレーム SceneRenderer へ渡し、ポストエフェクト（DepthOfFieldPass）が深度でぼかす
// 有効な物が2つ以上あれば、先に見つかった1つだけを使う。1つも無ければぼかさない。停止中のエディタでも掛かる（Scene ビューは View → Scene View → Depth of Field で切れる）
// 奥行き（深度）を書かない物（Alpha Clip なしの Sprite の板・半透明・パーティクル）は、後ろにある物と同じだけぼける

/// <summary>
/// ピントをどこに合わせるか。Inspector では、選んだ合わせ方の値だけが出る
/// </summary>
enum class DepthOfFieldFocus {
	FollowTarget, // メインカメラの Camera Follow の Target（遊ぶキャラ）の奥行きに合わせる。Target が無ければ Focus Distance の初期値
	Distance,     // カメラから Focus Distance（m）の奥行きに合わせる
};


/// <summary>
/// 被写界深度（Unity の Post Processing の Depth of Field の小さい版）
/// </summary>
struct DepthOfFieldComponent {
	bool enabled = true;                                       // false の間はぼかさない
	DepthOfFieldFocus focus = DepthOfFieldFocus::FollowTarget; // ピントの合わせ方
	float focusDistance = 16.5f;                               // カメラからピントの合う所までの奥行き（m。Distance のとき）
	float focusRange = 3.0f;                                   // ピントの前後この奥行き（m）までは、ぼけない（遊ぶ面の手前・奥の飾りも、はっきり見せる幅）
	float farTransition = 40.0f;                               // その奥から、一番ぼけるまでの奥行き（m。小さいほど、すぐ奥がぼける）
	float nearTransition = 4.0f;                               // その手前から、一番ぼけるまでの奥行き（m。0 なら手前はぼかさない）
	float maxBlur = 8.0f;                                      // 一番ぼけた所の、ぼけの半径（ピクセル。高さ 720 の画面で。大きいほど重い）
};

static_assert(std::is_trivially_copyable_v<DepthOfFieldComponent>);
```

### E.2 `Systems/DepthOfFieldSystem.h`（新しく作る）
```cpp
#pragma once
#include "MyEngine/Components/DepthOfFieldComponent.h"

/// <summary>
/// Depth Of Field（被写界深度）を登録し、シーンの Depth Of Field Component の値を、毎フレーム SceneRenderer へ渡す
/// <para>ぼかすのはポストエフェクト（DepthOfFieldPass。CS 2本＋合成の PS）</para>
/// </summary>
class DepthOfFieldSystem {
public:
	// DepthOfFieldComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 有効な Depth Of Field Component を探して、SceneRenderer::SetDepthOfField で渡す（無ければぼかさない）
	/// <para>停止中も呼ぶ（エディタで値を変えて、すぐ見られるように）。ピントを合わせる Target の位置を使うので、Transform を計算し終わった後に呼ぶ</para>
	/// </summary>
	static void Update();
};
```

### E.3 `Systems/DepthOfFieldSystem.cpp`（新しく作る）
`FogSystem.cpp` と同じ形（`Describe` 1 つで保存と Inspector・最初に見つかった 1 つを使う）。
```cpp
#include "DepthOfFieldSystem.h"

#include <memory>

#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/PostEffect/DepthOfFieldPass.h"
#include "MyEngine/Graphics/Renderer/SceneRenderer.h"
#include "MyEngine/Systems/CameraSystem.h"

namespace {
//=============================================================================
// Inspector と 保存
//=============================================================================
void DescribeDepthOfField(ComponentUI& ui, DepthOfFieldComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すとぼかさない"));
	ui.Label("シーンに1つ。ピントの合った所だけはっきり見え、手前と奥がぼける（Scene ビューは View → Scene View → Depth of Field で切れる）");
	// ===== ピント =====
	ui.Separator("Focus");
	ui.Field("Focus###focus", value.focus, Tip("ピントの合わせ方。FollowTarget：メインカメラの Camera Follow の Target の奥行き / Distance：カメラから Focus Distance の奥行き"));
	// --- 選んだ合わせ方の値だけを見せる（保存も、使っている方だけが書かれる。Camera Follow の Mode と同じ）---
	switch (value.focus) {
	case DepthOfFieldFocus::FollowTarget:
		break; // FollowTarget だけの値は無い（Target は Camera Follow の物を使う）
	case DepthOfFieldFocus::Distance:
		ui.Field("Focus Distance###focusDistance", value.focusDistance, AtLeast(0.0f).Tip("カメラからピントの合う所までの奥行き（m）"));
		break;
	}
	ui.Field("Focus Range###focusRange", value.focusRange, AtLeast(0.0f).Tip("ピントの前後この奥行き（m）までは、ぼけない。遊ぶ面の手前・奥の飾り（草・岩）もはっきり見せたいときは広げる"));
	// ===== ぼけ方 =====
	ui.Separator("Blur");
	ui.Field("Far Transition###farTransition", value.farTransition, AtLeast(0.01f).Tip("ピントの範囲の奥から、一番ぼけるまでの奥行き（m）。小さいほど、すぐ奥がぼける（奥の層ほど、ぼけが大きくなる）"));
	ui.Field("Near Transition###nearTransition", value.nearTransition, AtLeast(0.0f).Tip("ピントの範囲の手前から、一番ぼけるまでの奥行き（m）。0 なら手前はぼかさない"));
	ui.Field("Max Blur###maxBlur", value.maxBlur, Range(0.0f, DepthOfFieldPass::kMaxBlurLimit).Tip("一番ぼけた所の、ぼけの半径（ピクセル。高さ 720 の画面で）。大きいほど重い"));
}

// ===== Depth Of Field Component の値を、ポストエフェクトへ渡す形にする =====
DepthOfFieldSettings ToSettings(const DepthOfFieldComponent& component) {
	DepthOfFieldSettings settings;
	settings.enabled = true;
	settings.focusDistance = component.focusDistance;
	settings.focusRange = component.focusRange;
	settings.farTransition = component.farTransition;
	settings.nearTransition = component.nearTransition;
	settings.maxBlur = component.maxBlur;
	// --- ピントを合わせる点（奥行きはビューごとにカメラから測るので、ここでは点だけ渡す）---
	switch (component.focus) {
	case DepthOfFieldFocus::FollowTarget:
		if (const TransformComponent* target = CameraSystem::GetFollowTarget()) {
			settings.hasFocusPoint = true;
			settings.focusPoint = GetWorldPosition(*target);
		}
		break; // Target が無ければ focusDistance のまま
	case DepthOfFieldFocus::Distance:
		break;
	}
	return settings;
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void DepthOfFieldSystem::Initialize() {
	// Depth Of Field の置き場所・保存・Inspector を登録する（見せ方は DescribeDepthOfField 1つで、保存と Inspector の両方に使う）
	EntityManager::RegisterComponent<DepthOfFieldComponent>();
	ComponentSerializer::Register<DepthOfFieldComponent>("Depth Of Field", &DescribeDepthOfField);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<DepthOfFieldComponent>>("Depth Of Field", "Rendering3D", &DescribeDepthOfField));
}

void DepthOfFieldSystem::Release() {} // 持っている物は無い（SceneRenderer は先に消えていることがあるので、ここでは触らない）

//=============================================================================
// 毎フレーム
//=============================================================================
void DepthOfFieldSystem::Update() {
	// 有効な Depth Of Field を1つ探す（先に見つかった物。無ければぼかさない）
	// ForEach は途中で止められないので、見つけたら印を付けて、後の物は飛ばす（FogSystem と同じ）
	DepthOfFieldSettings settings;
	bool isFound = false;
	EntityManager::ForEach<DepthOfFieldComponent>([&settings, &isFound](Handle<Entity> entity, const DepthOfFieldComponent& component) {
		if (isFound || !component.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		settings = ToSettings(component);
		isFound = true;
	});
	SceneRenderer::SetDepthOfField(settings); // 見つからなければ、初期値（ぼかさない）のまま
}
```

### E.4 `Systems/CameraSystem.h`
前方宣言に 1 行：
```cpp
// 前方宣言
class Camera;
struct TransformComponent;
```

`GetMainCameraEntity` の下に足す：
```cpp
	// メインカメラの Camera Follow が追っている物の Transform（Camera Follow・Target が無ければ nullptr。停止中も返す）
	// 風の線（WindZoneSystem）・被写界深度のピント（DepthOfFieldSystem）が使う
	static const TransformComponent* GetFollowTarget();
```

### E.5 `Systems/CameraSystem.cpp`
最後に足す：
```cpp
const TransformComponent* CameraSystem::GetFollowTarget() {
	// メインカメラの Camera Follow → その Target の Entity → Transform（どこかで無ければ nullptr）
	const CameraFollowComponent* follow = EntityManager::Get<CameraFollowComponent>(GetMainCameraEntity());
	return (follow != nullptr) ? EntityManager::Get<TransformComponent>(EntityManager::Find(follow->target)) : nullptr;
}
```

### E.6 `Systems/WindZoneSystem.cpp`（同じ探し方を `GetFollowTarget` にまとめる）
`UpdateScreenLines` の `if (isGameRunning) {` の中の最初の 4 行を 1 行に：
```cpp
	if (isGameRunning) {
		if (const TransformComponent* followed = CameraSystem::GetFollowTarget()) {
			const WindZoneSystem::WindSample wind = WindZoneSystem::Sample(GetWorldPosition(*followed));
```

include の `#include "MyEngine/Components/CameraFollowComponent.h"` は要らなくなるので消す。

### E.7 `Engine.cpp`
include（`FogSystem.h` の下）：
```cpp
#include "MyEngine/Systems/DepthOfFieldSystem.h"
```
`FogSystem::Initialize();` の下：
```cpp
	FogSystem::Initialize();          // FogをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	DepthOfFieldSystem::Initialize(); // Depth Of FieldをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```
`FogSystem::Release();` の下：
```cpp
	FogSystem::Release();             // シーン（ウィンドウ）の破棄より後にする
	DepthOfFieldSystem::Release();    // シーン（ウィンドウ）の破棄より後にする
```

### E.8 `Window/WindowManager.cpp`
include（`FogSystem.h` の下）：
```cpp
#include "MyEngine/Systems/DepthOfFieldSystem.h"
```
`FogSystem::Update();` の下：
```cpp
	FogSystem::Update();                    // 霧更新
	DepthOfFieldSystem::Update();           // 被写界深度の値（ピントを合わせる Target の位置を使うので、カメラの Update の後）
```

- `CameraSystem::Update` の後に呼ぶ（メインカメラの Entity は、そこで決まる）。

---

## パートF：エディタで

### F.1 Level01 に付ける
1. Hierarchy で **Main Camera** を選ぶ（Fog を付けた Entity。どれでもよいが、Fog と同じ所にまとめると探しやすい）。
2. **Add Component → Rendering3D → Depth Of Field**。
3. 初期値のままで、Game ビューで遊ぶ面がはっきり、空と山がぼけていれば OK。Save。

### F.2 値の目安（今の Level01：カメラ Z −16.5、近い木 Z 15、遠い木 Z 40、空 Z 80）

| 項目 | 初期値 | 目安 |
|---|---|---|
| Focus | FollowTarget | キューブの面にピント。Distance にすると、カメラから決めた距離 |
| Focus Range | 3 | 遊ぶ面の手前の草（Z −2.1）・奥の飾りまではっきり見せる幅。草がぼけるなら広げる |
| Far Transition | 40 | 近い木 0.3・遠い木 0.93・空 1 の CoC。**小さくするほど近い木までぼける**（20 なら近い木も 0.6） |
| Near Transition | 4 | 手前にカメラ寄りの物を置いたとき、ぼかす。いらなければ 0 |
| Max Blur | 8 | 一番ぼけた所の半径（ピクセル）。元のゲームくらいなら 6〜10。上げると重い |

- **背景の板は Alpha Clip を入れる**（G10 の木の板は入れてある）。入れないと深度を書かないので、空と同じだけぼける（遠い木と近い木の差が出ない）。
- 半透明の物（輪・矢印・水）はぼけない（1.5）。
- Scene ビューで置くときにぼけが邪魔なら、**View → Scene View → Depth of Field** を外す。

---

## 2. 確かめたこと
- Release：今の Level01 ＋ Depth Of Field（初期値）＋ G10 の木の層 4 枚（`trees_far` Z 40・`trees_near` Z 15、Alpha Clip あり）で撮った（このファイルの最初の 2 枚）。近い木は少し、遠い木はもっと、山と空は一番ぼける。キューブ・足場・苔の岩・輪・矢印ははっきり。キューブの縁ににじみは見えない。
- 最初は半透明もぼけた（矢印・輪が空と同じだけぼけた）→ 半透明を DOF の後に描くようにした（1.5）。
- Debug：エディタを起動して、Scene ビュー（グリッドあり・DOF あり）、Game ビュー（グリッドなし）、View → Game View → Grid を入れると Game ビューにもグリッドが出る、を確かめた。D3D12 のデバッグレイヤーのエラー・assert なし。
- 確かめていないこと：Scene ビューの DOF を切ったときの見た目（`viewOptions_.depthOfField` が false で「ぼかさない」になるだけ）、ウィンドウの大きさを 1280×720 以外にしたとき。

## 3. 次
- G9（レベル・UI）。プレイ会（10/28）までに G9 の時間を残す。
- DOF の続きにできること（今はやらない）：ぼけた明るい所を丸い玉（ボケ）にする、手前のぼけを別の絵に分けてきれいにする、半透明の物にも「ぼけるか」を選べるようにする。
