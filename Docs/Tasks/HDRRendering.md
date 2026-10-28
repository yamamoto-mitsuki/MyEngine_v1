# G-2：HDR レンダリング（3D を float の絵に描いてトーンマップで画面へ・色を float にして Intensity で光らせる）

参考：
- sRGB の式（リニアとの行き来）：https://en.wikipedia.org/wiki/SRGB#Transformation
- Reinhard ほか「Photographic Tone Reproduction for Digital Images」（2002。x / (1 + x) の形）：https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf
- ACES の近似（Stephen Hill の式。MJP の BakingLab に入っている物・MIT ライセンス）：https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
- GT（内村式）トーンマップ：内村創「HDR Theory and practice」（CEDEC 2017）https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp ・ 式のグラフ https://www.desmos.com/calculator/gslcdxvipg
- Unity の HDR カラー（色見本＋Intensity）：https://docs.unity3d.com/Manual/hdr-color-picker-reference.html

## このファイルについて
- AL4_01 の設計 [ParticleTitleClear.md](../../../AL4_01/Docs/ParticleTitleClear.md) の **3.3 の「別の案（HDR）」**。[AL4_01Game.md](AL4_01Game.md) の 13章の Step **G-2**。[PostEffect.md](PostEffect.md) の予定1（HDR レンダリング）・予定2（トーンマップと露出）。
- ユーザーの頼み：ゴールだけ光らせるのを、グロー層ではなく HDR で作る（クオリティ優先）。色は 8 ビットをやめて float。Component・Inspector は「色（0〜1 の色見本）＋ Intensity（倍率。初めは 1）」。3D の描画先は float、2D・UI は SDR のまま。トーンマップと露出を足し、初めの値は今の見た目。ACES なども選べる。ポスト無しの道・Scene ビュー・Release のウィンドウ、どれも同じに描ける。
- **[KawaseBloom.md](KawaseBloom.md) の後**に写す。**写す場所**：`AL4_01/MyEngine_v1`（サブモジュール）→ エンジンでコミット・push → `source/repos/MyEngine_v1` で pull。
- **ゲーム（AL4_01）のコードは変わらない**。前に保存したシーン・Prefab もそのまま読める（Intensity が無い物は 1）。初めの値（ToneMap None・Exposure 1・Intensity 1）なら、見た目はほぼ今のまま（違いは 1.5）。
- 確かめた環境：AL4_01 develop（4282746）＋ 今の作業中のリソース ＋ エンジン develop（689de5b。川瀬式ブルームまで）＋ このファイル。パートごとに Debug・Release でビルドし、Release とエディタで画面を撮った（6章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `RenderTexture.h`・`PSOManager.cpp`・`Skybox.cpp`・`DepthOfFieldPass.cpp`・`BloomPass.cpp`・`BloomBright.PS`・`ToneMap.hlsli`（新しく作る）・`Lens.PS`・`LensPass.h/.cpp`・`PostProcess.h/.cpp`・`SceneRenderer.h/.cpp`・`MyEngine_v1.vcxproj` | 3D を HDR（`R16G16B16A16_FLOAT`）の絵に描き、最後のパスで露出・トーンマップして画面（SDR）へ。Settings → PostEffect → **ToneMap**（Mode・Exposure） |
| B | `Math/Color.h`・`Renderer.h/.cpp`・線とギズモ（Camera・GizmoLines・LightGizmo・Animation・Collision・ElasticSurface・WindZone）・色を渡す System（Primitive・Polygon・ArrowStrip・Sprite・Trail・SceneTransition）・Model Renderer（Component・保存・Inspector・System） | 色を float に（Renderer の Config・線の色 `uint32_t` 0xRRGGBBAA → `Vector4`。`ToColorCode` などを消す） |
| C | `Math/Color.h`・Primitive の見た目（`PrimitiveMaterial`）・Particle System・Model Renderer・Sprite Renderer・Trail | **Intensity**（明るさの倍率）を足して、Renderer へ渡すときに掛ける（`ApplyIntensity`） |

- **A・B・C の順に写す。それぞれだけでビルドが通る**（A の後も B の後も、見た目はほぼ今のまま。C の後に Intensity を上げると光る）。

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 絵の流れ（前と後）
```
前：3D ──→ postRT_（SDR：0〜1 で頭打ち）→ 被写界深度 → ブルーム（work_ SDR）→ レンズ → 画面（SDR）→ 2D
                    ↑ ここで 1 を超えた明るさが切れる。Threshold 1 のブルームは「1 ちょうどの所」しか拾えない

後：3D ──→ postRT_（HDR：float）→ 被写界深度（HDR）→ ブルーム（work_ HDR）→ レンズ＋露出＋トーンマップ → 画面（SDR）→ 2D
                    ↑ 1 を超えた明るさが残る。Threshold 1 で「1 より明るい物だけ」光る      ↑ ここで初めて 0〜1 に収める
```
- **3D の PSO の描画先の形式は、描く形（DrawCategory）で決める**：Model・Particle・Line は HDR、Sprite・Text は SDR（`PSOManager::MakePSO`）。2D はトーンマップの後の画面に重ねるので、今のまま。
- **画面の 0〜1 に収めるのは、最後のレンズのパス（`LensPass`）だけ**。レンズを切っていても、露出とトーンマップだけ掛けて通す（前から「全部 0 ならただのコピー」で最後の出力を兼ねていた所に足した）。
- **ポスト無しの道もここを通る**。3D の PSO が HDR の形なので、SDR の画面へ直に描けない（PSO の描画先の形式と、張った描画先の形式が違うと D3D12 のエラー）。なので `PostEffectMode::None`（View → Scene View → Post Effects を外したとき）も、HDR の `postRT_` に描いて、ブルーム・レンズ・被写界深度を外し、トーンマップだけ掛ける。

| 道 | 被写界深度 | ブルーム | レンズ | 露出・トーンマップ |
|---|---|---|---|---|
| Game ビュー・Release のウィンドウ（`All`） | ○ | ○ | ○ | ○ |
| Scene ビュー（`NoLens`） | ○ | ○ | × | ○ |
| Scene ビューで Post Effects を外す（`None`） | × | × | × | ○ |

- 前の `SceneRenderer::RenderInternal`・`RenderWorld`（ポスト無しで画面へ直に描く道）は使わなくなるので消す。`postProcess_` が無いと描けないので assert にする（`InitializePost` は Engine がいつも呼んでいる）。

### 1.2 トーンマップと露出（Settings → PostEffect → ToneMap）
| Mode | 式 | 見た目 |
|---|---|---|
| **None**（初めの値） | `saturate(x)`（1 で切る） | **今までの見た目**。1 を超えた所は白く飛ぶ（色が抜ける） |
| Reinhard | `x / (1 + x)` | どこまで明るくても飛ばない。そのぶん全体が暗く・淡くなる（1 が 0.5 になる） |
| ACES | 映画の色の標準（ACES）の RRT＋ODT を分数の式で近づけた物 | 明るい所は白へ柔らかく飽和し、暗い所は締まる。色が少し濃く、全体は少し暗い（Exposure を少し上げて使う） |
| GT | グランツーリスモの内村式（暗い所の曲線・真ん中の直線・明るい所の曲線をつなぐ） | 真ん中の明るさは傾き 1 の直線なので、色も明るさもほとんど変わらず、明るい所だけ柔らかく白へ。アニメ調に向く |

- **Exposure**：トーンマップの前に掛ける倍率（2 で1段明るい、0.5 で1段暗い）。**ブルームの光り方は変わらない**（ブルームは露出の前の HDR の値で抜き出すので、「Intensity が 1 を超えた物だけ光る」は Exposure を変えても同じ。Unity URP の Post Exposure と同じ順）。
- 掛ける順：レンズの歪み・色収差・放射ブラーは HDR のまま（明るい所ほど筋も明るい）→ 露出・トーンマップ → **風の線・ビネットは収めた後**（前と同じ見た目になるように。前は 1 で切った絵に掛けていた）。

![左上 None・右上 Reinhard・左下 ACES・右下 GT（ゴールの扉 Intensity 3・右の粒 Intensity 4）](images/hdr_tonemap.png)

### 1.3 色を float に・Intensity
- **前**：Component・Inspector は `Vector4`（0〜1）、Renderer の Config は `uint32_t` の 0xRRGGBBAA（1色8ビット）。8 ビットは 0〜1 しか持てないので、**1 を超える明るさを Renderer へ運べない**（ここが根っこ）。
- **後**：Renderer の Config・線の色も `Vector4`。Component・Inspector は「**Color（0〜1 の色見本）＋ Intensity（倍率。初めは 1）**」で持ち、Renderer へ渡すときに `ApplyIntensity(color, intensity)` で掛ける（Unity の HDR カラーと同じ考え。色合いを変えずに明るさだけ上げ下げできる）。
- **Intensity はリニア（光の強さ）で掛ける**。Config の色は今までどおり sRGB（見た目の値）で、Renderer が `SrgbToLinear` でリニアに直す。sRGB の値にそのまま 3 を掛けると、直したときに 3 の 2.4 乗くらい（白なら 12.8）になってしまう。なので `ApplyIntensity` は「リニアに直して × intensity → sRGB の曲線で書き直す」（1 を超える値も同じ曲線を伸ばす。`SrgbToLinear` に通すと、ちょうどリニア × intensity に戻る）。例：白 × 3 → Config は (1.61, 1.61, 1.61)、シェーダーでは (3, 3, 3)。
- **色は 0〜1 に収めてから** Intensity を掛ける（`ClampColor`。前の `ToColorCode` も 0〜1 に収めていた）。1 を超えさせるのは Intensity だけ、と決めておくと、どの物が光るか分かりやすい。
- 保存：Color は前と同じ `[r, g, b, a]`、Intensity は新しい項目 `intensity`（前のファイルには無い＝ 1 のまま読む）。

**Intensity を付けた Renderer**（Color の下に **Intensity**）：

| Component | 保存の名前 | 光らせたい物の例 |
|---|---|---|
| **Primitive Renderer**（と、同じ見た目を使う **Polygon Renderer・Elastic Surface・Arrow Strip**） | `intensity`（Elastic Surface の支柱は `postIntensity`） | ゴールの扉・円の玉・光る矢印 |
| **Particle System**（Start の Color A・B の下。全部の粒に掛ける） | `intensity` | ゴール・円のキラキラ |
| **Model Renderer** | `intensity` | 光るモデル |
| **Sprite Renderer**（3D の板） | `intensity` | 光の板・光る看板（Blend を Add に） |
| **Trail** | `intensity` | 光る軌跡 |

- **付けない物**：Image・Text（2D はトーンマップの後の画面＝SDR に重ねるので、1 を超えても意味が無い）。ライトはもう Intensity を持っている。前からの `ParticleManager` は古い仕組みなので触らない。
- マテリアルの **Emissive**（自己発光の色）には倍率を付けていない（7章の「後でやること」）。

### 1.4 何が光るか（Threshold 1）
- ブルームの Threshold は AL4_01 では **1.0**（`Parameters/PostEffect/Bloom.json`）。HDR になったので「シーンの値が 1 を超えた所だけ」光る。**Intensity を 1 より大きくした物が光る**。
- ただし Intensity 1 でも 1 を超えることがある：
  - **Additive（足す）の物**：後ろの色に足されるので、明るい背景の前では 1 を超える。ゴールの扉（Additive・Unlit）は Intensity 1 でも、明るい空の前の真ん中が少し光る（下の絵の左上）。
  - **Add の粒が重なった所**：重なるほど足される。
  - **光の当たる面のハイライト**：氷のキューブ（Core は Half Lambert）の上の縁が 1 を少し超えていて、うっすら光るようになった（下の 2 枚目）。気になるときは Threshold を 1.1〜1.2 に上げる。

![ゴールの扉の Intensity：左上 1・右上 1.5・左下 2・右下 3（None）。Additive なので 1.5 でほぼ白く飛ぶ](images/hdr_intensity.png)

![氷のキューブ：左が前（SDR）、右が HDR（Threshold 1）。上の縁のハイライトがうっすら光る](images/hdr_cube_highlight.png)

---

## 2. パートA：3D を HDR の絵に描いて、トーンマップで画面へ

### `Graphics/RenderTarget/RenderTexture.h`
enum を、こう直す（コメントだけ）：
```cpp
// 描画先の形式
enum class RenderTextureFormat {
	SDR,   // R8G8B8A8_UNORM_SRGB：画面に出す絵（0〜1 で頭打ち。書くときに GPU が sRGB へ直す）
	HDR,   // R16G16B16A16_FLOAT：3D の絵・ブルームの段・IBL（1 を超える明るさを残す）
	RG16F, // R16G16_FLOAT：IBL の BRDF の表
};
```
`private:` の `static DXGI_FORMAT ToDXGIFormat(RenderTextureFormat format);` の行を消して、`public:` の `GetDepthSRVSlot` の下へ移す（PSO の描画先の形式を、描く先の RenderTexture とそろえるのに外から使う）：
```cpp
	/// <summary>
	/// 形式 → DXGI_FORMAT（PSO の描画先の形式を、描く先の RenderTexture とそろえるのに使う）
	/// </summary>
	static DXGI_FORMAT ToDXGIFormat(RenderTextureFormat format);
```

### `Graphics/Pipeline/PSOManager.cpp`
include（`RenderStates.h` の下）：
```cpp
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
```
`std::unique_ptr<PSOManager> PSOManager::instance_ = nullptr;` の下：
```cpp
namespace {
// ===== 描く形 → 描画先の形式 =====
// 3D は float の絵（HDR。1 を超える明るさを残す）に描き、2D はトーンマップの後の画面（SDR）に重ねる（SceneRenderer の描く順）
RenderTextureFormat GetTargetFormat(DrawCategory category) {
	switch (category) {
	case DrawCategory::Model:
	case DrawCategory::Particle:
	case DrawCategory::Line:
		return RenderTextureFormat::HDR;
	case DrawCategory::Sprite:
	case DrawCategory::Text:
		return RenderTextureFormat::SDR;
	}
	return RenderTextureFormat::SDR;
}
} // namespace
```
`MakePSO` の `desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;` を：
```cpp
	desc.RTVFormats[0] = RenderTexture::ToDXGIFormat(GetTargetFormat(p.category)); // 3D は HDR、2D は SDR
```
- PSO を探す鍵（`PSOKey`）は変えなくてよい。形式は描く形で決まり、描く形はシェーダー（`shaderProgramID`）で決まっているので、同じ鍵で形式が違う PSO は出てこない。

### `Scene/Skybox.cpp`
include（`PSOManager.h` の下）：
```cpp
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"
```
`CreatePSO` の：
```cpp
	// シーンの描画先（SceneRenderer の postRT_。HDR）と一致させる
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = RenderTexture::ToDXGIFormat(RenderTextureFormat::HDR);
```
- 空の .hdr の画像は 1 を超える明るさを持てる。今までは SDR の絵に書いた所で 1 で切れていた（空がとても明るい画像だと、これから光ることがある）。

### `Graphics/PostEffect/DepthOfFieldPass.cpp`
`CreatePSO` の `desc.RTVFormats[0] = …` の行を：
```cpp
	desc.RTVFormats[0] = RenderTexture::ToDXGIFormat(RenderTextureFormat::HDR); // 書く先は HDR の RenderTexture（PostProcess の depthOfFieldRT_。この後にブルーム・レンズが読む）
```
- 縮めた絵・ぼかした絵（CS の `ComputeTexture`）はもともと `R16G16B16A16_FLOAT` なので、そのまま。

### `Graphics/PostEffect/BloomPass.cpp`
`CreatePSO` の4行（`// 中間はHDR、合成先はSDR…` から `desc.RTVFormats[0] = isComposite ? …` まで）を：
```cpp
	// 中間の段も、合成先（PostProcess の work_）も HDR（合成した絵は、この後のレンズのパスでトーンマップして画面へ）
	desc.RTVFormats[0] = RenderTexture::ToDXGIFormat(RenderTextureFormat::HDR);
```

### `Shader/Data/Post/BloomBright.PS.shader`
`#include "Post/Bloom.hlsli"` の下と、`main` の1行目を：
```hlsl
#include "Post/Bloom.hlsli"

// 抜き出す明るさの上限。シーンの絵は HDR なので、1点だけ極端に明るい所（鏡のような面のハイライトなど）があると、
// ぼかしたときに大きな光の玉になって、フレームごとにちらつく。抜き出す前にこの値で切る
static const float kMaxBrightness = 64.0f;

float32_t4 main(VSOutput input) : SV_TARGET
{
    // max(…, 0) は負の値と NaN（0 で割った所など）も 0 にする（float の絵は NaN をそのまま持つので、ぼかすと黒い四角が広がる）
    float32_t3 color = min(max(gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord).rgb, 0.0f), kMaxBrightness);
```
（`float brightness = …` から下は今のまま）
- HLSL の `max` は「片方が NaN なら、もう片方を返す」決まり（IEEE 754 の maxNum）なので、NaN が 0 になる。

### `Shader/Data/Post/ToneMap.hlsli.shader`（新しく作る）
```hlsl
#META
stage: Include
path : Post/ToneMap.hlsli
#META_END

#HLSL
// トーンマップ：HDR の明るさ（0〜いくらでも）を、画面に出せる 0〜1 に収める曲線
// どれもリニアの値を受け取って、リニアの 0〜1 を返す（画面の _SRGB の形へ書くときに GPU が sRGB へ直す）

// 種類（C++ の ToneMapMode と同じ並び）
static const uint32_t kToneMapNone = 0;
static const uint32_t kToneMapReinhard = 1;
static const uint32_t kToneMapACES = 2;
static const uint32_t kToneMapGT = 3;

// ===== Reinhard：x / (1 + x) =====
// どこまで明るくても 1 に近づくだけで、白く飛ばない。そのぶん全体が暗く・淡くなる（1 が 0.5 になる）
float32_t3 ToneMapReinhard(float32_t3 color)
{
    return color / (1.0f + color);
}

// ===== ACES（映画の色の標準）の近似：Stephen Hill の式（BakingLab）=====
// 明るい所は白へ柔らかく飽和し、暗い所は締まる。色が少し濃くなる（1 が 0.8 くらいになるので、Exposure を少し上げて使う）
// 1. sRGB の色を ACES の色の空間へ（RRT の彩度の調整も込み）
static const float32_t3x3 kACESInput = {
    {0.59719f, 0.35458f, 0.04823f},
    {0.07600f, 0.90834f, 0.01566f},
    {0.02840f, 0.13383f, 0.83777f},
};
// 3. ACES の色の空間から sRGB へ戻す（ODT の彩度の調整も込み）
static const float32_t3x3 kACESOutput = {
    {1.60475f, -0.53108f, -0.07367f},
    {-0.10208f, 1.10813f, -0.00605f},
    {-0.00327f, -0.07276f, 1.07602f},
};
// 2. RRT（映画のフィルムのような曲線）と ODT（画面へ出す曲線）を合わせた形を、分数の式で近づけた物
float32_t3 RRTAndODTFit(float32_t3 v)
{
    float32_t3 a = v * (v + 0.0245786f) - 0.000090537f;
    float32_t3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}
float32_t3 ToneMapACES(float32_t3 color)
{
    color = mul(kACESInput, color);
    color = RRTAndODTFit(color);
    color = mul(kACESOutput, color);
    return saturate(color);
}

// ===== GT（グランツーリスモ。内村式）=====
// 暗い所は少し締め（曲線）、真ん中は傾き 1 の直線（色も明るさも変わらない）、明るい所だけ 1 へ柔らかく近づく
// 真ん中が変わらないので、絵の色がずれにくい（アニメ調に向く）
static const float kGTMaxBrightness = 1.0f; // P：一番明るい値（画面の白）
static const float kGTContrast = 1.0f;      // a：直線の部分の傾き
static const float kGTLinearStart = 0.22f;  // m：直線の部分が始まる明るさ
static const float kGTLinearLength = 0.4f;  // l：直線の部分の長さ
static const float kGTBlack = 1.33f;        // c：暗い所の締め方（大きいほど暗い所が黒く締まる）
float ToneMapGTChannel(float x)
{
    const float P = kGTMaxBrightness;
    const float a = kGTContrast;
    const float m = kGTLinearStart;
    const float l0 = ((P - m) * kGTLinearLength) / a;
    const float S0 = m + l0;                // 明るい所の曲線が始まる明るさ
    const float S1 = m + a * l0;            // そのときの出力
    const float C2 = (a * P) / (P - S1);
    const float CP = -C2 / P;
    // どの部分にいるかの重み（暗い所 w0・真ん中 w1・明るい所 w2。足して 1）
    const float w0 = 1.0f - smoothstep(0.0f, m, x);
    const float w2 = step(S0, x);
    const float w1 = 1.0f - w0 - w2;
    // linear は HLSL の予約語（頂点の値の補間の仕方）なので、変数の名前に使えない
    const float toe = m * pow(max(x, 0.0f) / m, kGTBlack);    // 暗い所（つま先）
    const float middle = m + a * (x - m);                     // 真ん中（直線）
    const float shoulder = P - (P - S1) * exp(CP * (x - S0)); // 明るい所（肩）
    return toe * w0 + middle * w1 + shoulder * w2;
}
float32_t3 ToneMapGT(float32_t3 color)
{
    return float32_t3(ToneMapGTChannel(color.r), ToneMapGTChannel(color.g), ToneMapGTChannel(color.b));
}

// ===== 種類を選んで掛ける =====
float32_t3 ApplyToneMap(float32_t3 color, uint32_t mode)
{
    switch (mode)
    {
    case kToneMapReinhard:
        return ToneMapReinhard(color);
    case kToneMapACES:
        return ToneMapACES(color);
    case kToneMapGT:
        return ToneMapGT(color);
    default: // None：1 で切るだけ（今までの見た目）
        return saturate(color);
    }
}
#HLSL_END
```
#### 解説
- **ACES の3段**：① `kACESInput`（sRGB → ACES の作業用の色の空間。白の基準も合わせる）② `RRTAndODTFit`（映画の標準の曲線を、`(v² + …) / (…v² + …)` の分数の式で近づけた物。本物の ACES は表を引く重い計算）③ `kACESOutput`（sRGB へ戻す）。行列は「色の空間を移す」掛け算（`mul(行列, 色)`）。明るい所で色が白へ寄るのは ① と ③ の行列の彩度の調整のため。
- **GT の重み**：`smoothstep(0, m, x)` は x が 0 → m で 0 → 1 に滑らかに変わる。`step(S0, x)` は x が S0 以上なら 1。3 つの式（つま先・直線・肩）を、この重みでつないでいる。真ん中（m〜S0）は `middle` だけが効くので、出力 = 入力（a が 1 なら）。
- **`linear` は HLSL の予約語**（頂点シェーダーから渡す値の補間の仕方の指定）。変数の名前にするとコンパイルエラーになる（Lens.PS の `line` と同じ）。

### `Shader/Data/Post/Lens.PS.shader`
include（`Resources/Texture.hlsli` の下）：
```hlsl
#include "Post/ToneMap.hlsli"
```
`LensParam` の最後の `float2 padding;` を（C++ の `Param` と同じ並び。大きさは同じ 48 バイト）：
```hlsl
    float exposure; // 露出（トーンマップの前に掛ける倍率）
    uint32_t toneMap; // トーンマップの種類（ToneMap.hlsli の kToneMap〇〇）
```
`main` の、放射ブラーの `if` の閉じかっこの下（風の線の前）に：
```hlsl
    // ===== 露出・トーンマップ：HDR の明るさを、画面の 0〜1 に収める =====
    // ここまでは HDR のまま（明るい所ほど色収差・放射ブラーの筋も明るい）。風の線・ビネットは収めた後に掛ける（今までの見た目と同じ）
    color = ApplyToneMap(color * gLens.exposure, gLens.toneMap);
```

### `Graphics/PostEffect/LensPass.h`
include の下、`class LensPass` の説明の上に enum を足し、クラスの説明を直す：
```cpp
/// <summary>
/// トーンマップ（HDR の明るさを、画面に出せる 0〜1 に収める曲線）。シェーダーは Post/ToneMap.hlsli
/// </summary>
enum class ToneMapMode {
	None,     // 1 で切るだけ（今までの見た目。1 を超えた所は白く飛ぶ）
	Reinhard, // x / (1 + x)。どこまで明るくても白く飛ばない。全体が暗く・淡くなる
	ACES,     // 映画の色の標準の近似。明るい所は白へ柔らかく飽和し、暗い所は締まる。色が少し濃くなる
	GT,       // グランツーリスモ（内村式）。真ん中の明るさはそのまま、明るい所だけ柔らかく白へ（色がずれにくい。アニメ調に向く）
};


/// <summary>
/// 最後のパス（HDR の絵 → 画面）。レンズの効果（歪み・色収差・放射ブラー）→ 露出・トーンマップ → 風の線・ビネットの順に掛ける
/// <para>レンズを切っていても、露出とトーンマップだけ掛けて通す（3D の絵は HDR なので、どの道もここを通って画面へ出る）</para>
/// </summary>
```
`kMaxPassPerFrame` の下：
```cpp
	// --- 露出・トーンマップ（GlobalVariables から入る。レンズを切っていても、いつも掛ける）---
	ToneMapMode toneMap = ToneMapMode::None; // 曲線（None なら今までの見た目）
	float kExposure = 1.0f;                  // 露出：トーンマップの前に掛ける倍率（2 で1段明るい・0.5 で1段暗い。ブルームの光り方は変わらない）
```
`Render` の `<param name="enabled">` を：
```cpp
	/// <param name="enabled">false ならレンズの効果を掛けず、露出・トーンマップだけ掛けて写す</param>
```
`Param` の最後の `float padding[2] = {};` を：
```cpp
		float exposure = 1.0f;                    // 露出（トーンマップの前に掛ける倍率）
		uint32_t toneMap = 0;                     // トーンマップの種類（ToneMapMode の番号）
```

### `Graphics/PostEffect/LensPass.cpp`
`Render` の `Param param{};` のあたりを：
```cpp
	// --- パラメータを作る。切ってあるときはレンズの値が全部0＝露出・トーンマップだけ掛けたコピーになる ---
	Param param{};
	param.srcIndex = source.GetSRVSlot();
	param.exposure = kExposure;
	param.toneMap = static_cast<uint32_t>(toneMap);
```

### `Graphics/PostEffect/PostProcess.h`
`PostEffectMode` の `None` のコメントと、クラスの説明：
```cpp
	None,   // 掛けない（被写界深度・ブルーム・レンズを外す。露出・トーンマップだけは掛ける）
```
```cpp
/// <summary>
/// ポストエフェクトをまとめて順に掛ける（被写界深度 → ブルーム → レンズ・露出・トーンマップ）。ブルーム・レンズ・トーンマップの有無と数値は GlobalVariables で持つ
/// <para>被写界深度の値はシーンの Depth Of Field Component から（ApplyDepthOfField に渡す）</para>
/// <para>途中の絵は全部 HDR。画面（SDR）の 0〜1 に収めるのは、最後のレンズのパス（LensPass）</para>
/// </summary>
```
`Prepare`・`Composite`：
```cpp
	void Prepare(RenderTexture& scene, bool useBloom = true);         // useBloom が false ならブルームを掛けない（ポストエフェクトを切ったビュー）
	void Composite(float width, float height, bool useLens = true); // useLens が false ならレンズを掛けずに、露出・トーンマップだけ掛けて通す
```
メンバのコメント：
```cpp
	std::unique_ptr<RenderTexture> depthOfFieldRT_; // 被写界深度の合成の置き場（HDR）
	std::unique_ptr<RenderTexture> work_;           // ブルーム合成の置き場（HDR）
```

### `Graphics/PostEffect/PostProcess.cpp`
`Initialize` の2行と、ログ：
```cpp
	// どちらも HDR（シーンの絵のまま、1 を超える明るさを残す。画面の 0〜1 に収めるのは最後のレンズのパス）
	depthOfFieldRT_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::HDR, false);
	work_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::HDR, false);
```
```cpp
	LogManager::Log(std::format("PostEffect Bloom={} Lens={} Threshold={:.2f} Intensity={:.2f} Vignette={:.2f} ToneMap={} Exposure={:.2f}",
		kBloomEnabled, kLensEnabled, bloom_.kThreshold, bloom_.kIntensity, lens_.kVignette, static_cast<int>(lens_.toneMap), lens_.kExposure));
```
`Prepare`：
```cpp
void PostProcess::Prepare(RenderTexture& scene, bool useBloom) {
```
```cpp
	if (useBloom && kBloomEnabled != 0) {
```
`Composite` のコメント：
```cpp
	// 切っていても最終出力として通す（HDR の絵を、露出・トーンマップで画面の 0〜1 に収めるのもここ）
```
`RegisterGV` の `post.Category("Lens")…` の行の下：
```cpp
	// 露出・トーンマップ（HDR の絵 → 画面）。Enabled は無い（3D の絵は HDR なので、いつも掛ける。None なら 1 で切るだけ＝今までの見た目）
	post.Category("ToneMap").Add("Mode", GlobalVariables::MakeEnumCombo(lens_.toneMap)).Add("Exposure", lens_.kExposure);
```
`ApplyGV` の最後：
```cpp

	lens_.toneMap = GlobalVariables::GetEnumCombo<ToneMapMode>(gv->Get<GlobalVariables::ComboItem>(s, "ToneMap", "Mode"));
	lens_.kExposure = gv->Get<float>(s, "ToneMap", "Exposure");
```
- 保存は `Parameters/PostEffect/ToneMap.json`（Mode は何番目か：0 None・1 Reinhard・2 ACES・3 GT）。無ければ初めの値（None・1）で読む。

### `Graphics/Renderer/SceneRenderer.h`
private の `RenderInternal`・`RenderWorld` の宣言を2行消す。メンバのコメント：
```cpp
	std::unique_ptr<RenderTexture> postRT_;    // 3D を描く置き場（HDR・深度を読める）
	std::unique_ptr<PostProcess> postProcess_; // ポストエフェクトと、HDR の絵 → 画面（トーンマップ）
```

### `Graphics/Renderer/SceneRenderer.cpp`
`InitializePost`：
```cpp
	// 3D を描く置き場。HDR（1 を超える明るさを残す。3D の PSO の描画先の形式と同じ）。被写界深度が深度を読むので、深度を読める形で作る
	instance_->postRT_ = std::make_unique<RenderTexture>(width, height, RenderTextureFormat::HDR, true, true);
```
`// シーン描画の本体（…）` の見出しから `RenderWorldForPost` の上のコメントまで（`RenderInternal`・`RenderWorld` の2つの関数）を、こう直す：
```cpp
//=============================================================================
// シーン描画の本体
//=============================================================================
// ===== 3D。HDR の postRT_ へ描き、被写界深度まで掛けた絵を返す =====
// 3D の PSO は HDR の絵に描く形なので、ポストエフェクトを切ったビューもここを通る（その後、トーンマップだけ掛けて画面へ）
// 半透明・線・パーティクルは、被写界深度でぼかした後に描く（深度を書かないので、深度でぼけ方を決められない。
// 先に描くと、後ろにある物（空など）と同じだけぼける。Unreal の半透明の「Render After DOF」と同じ考え）
RenderTexture* SceneRenderer::RenderWorldForPost(const Camera* camera, const std::wstring& windowTitle) {
```
`RenderToTexture`（丸ごと）：
```cpp
void SceneRenderer::RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, PostEffectMode mode, const SceneViewOptions& options) {
	MY_ASSERT_MSG(instance_->postProcess_ != nullptr, "InitializePost を先に呼んでください（3D は HDR の絵に描いてから画面へ写すので、その置き場が要る）");
	// ポストエフェクトを掛けないビューは、被写界深度も掛けない（ブルーム・レンズは下で外す）
	SceneViewOptions viewOptions = options;
	if (mode == PostEffectMode::None) {
		viewOptions.depthOfField = false;
	}
	ApplyViewOptions(viewOptions);
	float width = static_cast<float>(target->GetWidth());
	float height = static_cast<float>(target->GetHeight());
	// 一旦 HDR の絵へ描き、光の層を作ってから target へ合成する（露出・トーンマップで 0〜1 に収める）
	RenderTexture* scene = RenderWorldForPost(camera, windowTitle);
	instance_->postProcess_->Prepare(*scene, mode != PostEffectMode::None); // 張り替える処理はここで全部済ませる
	target->PreDraw();                                                      // その後に描画先を張る
	instance_->postProcess_->Composite(width, height, mode == PostEffectMode::All);
	RenderUI(windowTitle, width, height); // UIは効果の外側に重ねる
	target->PostDraw();
}
```
`RenderToWindow` の頭（`if (!instance_->postProcess_) { … }` の4行を消して、assert を足す）：
```cpp
void SceneRenderer::RenderToWindow(const Camera* camera, RenderWindow* window, const std::wstring& windowTitle, float width, float height) {
	MY_ASSERT_MSG(instance_->postProcess_ != nullptr, "InitializePost を先に呼んでください（3D は HDR の絵に描いてから画面へ写すので、その置き場が要る）");
	ApplyViewOptions({}); // ウィンドウ（製品版のゲーム）は全部掛ける
	RenderTexture* scene = RenderWorldForPost(camera, windowTitle);
```
（その下は今のまま）

### `MyEngine_v1.vcxproj`
（`Lens.PS.shader` の行の下。ソリューションエクスプローラーに見せるだけ）
```
    <None Include="MyEngine\Shader\Data\Post\ToneMap.hlsli.shader" />
```
- **Release は `AL4_01/project/MyEngine/Shader` の写しを読む**（エンジンをビルドし直すと写る。このパートは C++ も変えるので必ず写る）。

---

## 3. パートB：色を float に

### `Math/Color.h`（丸ごと）
`ToColorCode`・`FromColorCode`・`FromColorCodeLinear` を消して、`ClampColor` を足す（消した関数を使っている所は、全部この後で直す）：
```cpp
#pragma once
#include <algorithm>
#include <cmath>

#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"

// 色の書き方の変換
// Component・Inspector・Renderer の Config は、どれも float の Vector4（RGBA）で持つ（1色8ビットの 0xRRGGBBAA はやめた。1 を超える明るさを運べないので）
// 色の空間：Inspector・コードで書く色は sRGB（色見本・画像と同じ、見た目の値）。シェーダーへ送るときにリニア（光の強さに比例する値）へ直す
//   シェーダーはリニアで計算して、3D は float の絵（HDR）に書く。最後にトーンマップで 0〜1 に収め、画面（_SRGB の形）へ書くときに GPU が sRGB へ戻す
//   画像も _SRGB の形で読むので、読むときにリニアになる
//   色だけ直さないと、色見本で選んだ色より明るく・薄く出る（0.2 の暗い灰色が、画面では 0.48 の灰色になる）

// A（透け方）にだけ倍率を掛ける（Alpha Group でまとめて透明にする。RGB はそのまま）
inline Vector4 MultiplyAlpha(const Vector4& color, float alpha) { return {color.x, color.y, color.z, color.w * alpha}; }

// 色と色を掛ける（RGBA をそれぞれ掛ける。白 (1,1,1,1) を掛けても変わらない。パーティクルの「初めの色 × 最後の色」）
inline Vector4 MultiplyColor(const Vector4& a, const Vector4& b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

// RGBA を 0〜1 に収める（色見本の範囲。行き過ぎると A が負になり、Add で暗くなるので）
inline Vector4 ClampColor(const Vector4& color) {
	return {std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f), std::clamp(color.z, 0.0f, 1.0f), std::clamp(color.w, 0.0f, 1.0f)};
}

// ===== sRGB（見た目の値）→ リニア（シェーダーで計算する値）=====
// GPU が _SRGB の形の画像を読むときと同じ式（暗い所は直線、それより上は 2.4 乗の曲線）。0 と 1 は変わらない
// 1 を超える値（HDR の色）も、同じ曲線をそのまま伸ばして直す

// 1色分
inline float SrgbToLinear(float value) {
	if (value <= 0.04045f) {
		return value / 12.92f;
	}
	return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

// RGB（光の色・自己発光・霧の色など、A の無い色）
inline Vector3 SrgbToLinear(const Vector3& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z)}; }

// RGBA（A は透け方の割合なので、直さない）
inline Vector4 SrgbToLinear(const Vector4& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), color.w}; }
```

### `Graphics/Renderer/Renderer.h`
クラスの説明に2行足す：
```cpp
/// <summary>
/// AABB,OBBなど基本矩形を描画するクラス
/// <para>Config の色は sRGB の見た目の値（Vector4 の RGBA）。中でリニアに直してシェーダーへ送る</para>
/// <para>3D の色の RGB は 1 を超えてよい（3D は HDR の絵に描くので、1 を超えた分も残り、ブルームで光る）</para>
/// </summary>
```
**`uint32_t color = 0xFFFFFFFF;` を全部 `Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};` にする**（コメントはそのまま）。13 か所：`ModelConfig`・`TriangleConfig`・`SphereConfig`・`Rect3dConfig`・`Sprite3dConfig`・`Quad3dConfig`・`MeshConfig`・`AABBConfig`・`OBBConfig`・`SpriteConfig`・`Quad2dConfig`・`LineSegment`・`LineListConfig`。
- Visual Studio の「置換」（Ctrl+H）で、このファイルだけを対象に `uint32_t color = 0xFFFFFFFF;` → `Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};` とすると速い。

private の `MakeModelMaterial` の引数：
```cpp
	static Material3dData MakeModelMaterial(const ModelManager::MtlMaterial* mat, const Vector4& color, const Transform& uvTransform, const MaterialParams& params, bool overrideMaterial);
```

### `Graphics/Renderer/Renderer.cpp`
`MakeModelMaterial` の頭：
```cpp
Material3dData Renderer::MakeModelMaterial(const ModelManager::MtlMaterial* mat, const Vector4& color, const Transform& uvTransform, 
	const MaterialParams& params, bool overrideMaterial) {
	// マテリアル構築（色は sRGB → リニア）
	Material3dData material;
	material.color = SrgbToLinear(color);
```
`PushMesh`（引数が5つの方）の色変換：
```cpp
	// --- 色変換（sRGB → リニア）---
	const Vector4 color = SrgbToLinear(config.color);
```
`PushSprite`：
```cpp
	// bind情報、頂点情報（色は sRGB → リニア。2D も画面へ書くときに sRGB へ戻る）
	SpriteRequest req;
	req.materialData.color = SrgbToLinear(config.color);
```
`DrawLines` の色（`Vector4 col = {segment.x * base.x, …}` まで）：
```cpp
	// 全体色は先に1回だけ直しておく（sRGB → リニア）
	const Vector4 base = SrgbToLinear(config.color);
	for (const LineSegment& seg : config.lines) {
		// 線ごとの色 × 全体色（どちらもリニアに直してから掛ける）
		const Vector4 col = MultiplyColor(SrgbToLinear(seg.color), base);
```
（その下の `req.vertices.push_back(…)` は今のまま）

### 線・ギズモの色（0xRRGGBBAA → Vector4）
数は 0〜255 を 255 で割った値（0x66 = 102 → 0.4、0xCC = 204 → 0.8、0xA5 = 165 → 0.647、0xE0 = 224 → 0.878、0x44 = 68 → 0.267、0x80 = 128 → 0.502）。`Vector4` は float が4つ並んだだけの型なので、`constexpr` で作れる。

`Camera/Camera.h`：include（`Vector3.h` の下）と色：
```cpp
#include "MyEngine/Math/Vector4.h"
```
```cpp
	Vector4 debugColor = {1.0f, 1.0f, 0.0f, 1.0f}; // 表示色（黄色）
```
`Camera/Camera.cpp`：`DrawFrustum` の `0xFFFFFFFF` 4か所を `{1.0f, 1.0f, 1.0f, 1.0f}` に：
```cpp
		config.lines.push_back({corner[e[0]], corner[e[1]], {1.0f, 1.0f, 1.0f, 1.0f}});
```
（十字の3本も、`0xFFFFFFFF` の所を `{1.0f, 1.0f, 1.0f, 1.0f}` に）

`Editor/Gizmos/GizmoLines.h`・`GizmoLines.cpp`：4つの関数の引数 **`uint32_t color` を `const Vector4& color` に**（.h の宣言4か所・.cpp の定義4か所。`uint32_t division` はそのまま）。

`Editor/Gizmos/LightGizmo.cpp`：
```cpp
constexpr Vector4 kRangeColor = {1.0f, 0.647f, 0.0f, 1.0f};    // 範囲の線の色（オレンジ）
constexpr Vector4 kDirectionColor = {1.0f, 0.878f, 0.4f, 1.0f}; // 平行光源の線の色（黄色）
```
`Systems/AnimationSystem.cpp`：
```cpp
constexpr Vector4 kJointColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 骨（Joint）の球の色（白）
constexpr Vector4 kBoneColor = {1.0f, 0.8f, 0.2f, 1.0f};  // 親子をつなぐ線の色（黄色）
```
`Systems/CollisionSystem.cpp`：色3つと、`PushBox`・`PushPolygon` の引数、`DrawGizmos` の2行：
```cpp
constexpr Vector4 kIdleColor = {0.4f, 1.0f, 0.4f, 1.0f};      // 当たっていない Collider の線の色（緑）
constexpr Vector4 kTriggerColor = {0.4f, 0.8f, 1.0f, 1.0f};   // 当たっていない Is Trigger の Collider の線の色（水色。すり抜ける物と見分ける）
constexpr Vector4 kHitColor = {1.0f, 0.267f, 0.267f, 1.0f}; // 当たっている Collider の線の色（赤）
```
```cpp
void PushBox(std::vector<Renderer::LineSegment>& lines, const AABB& box, const Vector4& color) {
```
```cpp
void PushPolygon(std::vector<Renderer::LineSegment>& lines, const ExtrudedPolygon& polygon, const Vector4& color) {
```
```cpp
		const Vector4 idleColor = collider.isTrigger ? kTriggerColor : kIdleColor;
		const Vector4 color = IsTouching(collider.entity) ? kHitColor : idleColor;
```
`Systems/ElasticSurfaceSystem.cpp`：
```cpp
constexpr Vector4 kSurfaceLineColor = {1.0f, 1.0f, 1.0f, 1.0f};       // 確かめる用の線：点をつないだ線（白）
constexpr Vector4 kRestLineColor = {0.502f, 0.502f, 0.502f, 1.0f}; // 確かめる用の線：平らな位置（灰色）
```
`Systems/WindZoneSystem.cpp`：
```cpp
constexpr Vector4 kGizmoColor = {1.0f, 0.502f, 1.0f, 1.0f}; // ギズモの線の色（うすい赤紫。空・Collider の緑・水色・赤と見分けられるように）
```

### 色を渡す System（`ToColorCode(…)` → `ClampColor(…)`）
前の `ToColorCode` も 0〜1 に収めていたので、同じ値になる。**パートC で、Intensity を付ける物はもう一度直す**（Image と幕はこのまま）。

| ファイル | 行 |
|---|---|
| `Systems/PrimitiveRenderSystem.h`（`ApplyMaterial`） | `config.color = ClampColor(material.color);` |
| `Systems/PolygonShapeSystem.cpp`（横の面） | `config.color = ClampColor({color.x * shade, color.y * shade, color.z * shade, color.w});` |
| `Systems/ArrowStripSystem.cpp` | `config.color = ClampColor(color);` |
| `Systems/SpriteRenderSystem.cpp`（Sprite Renderer） | `config.color = ClampColor(MultiplyAlpha(sprite.color, groupAlpha));` |
| `Systems/SpriteRenderSystem.cpp`（Image） | `config.color = ClampColor(MultiplyAlpha(image->color, groupAlpha));` |
| `Systems/TrailSystem.cpp` | `config.color = ClampColor(component->color);` |
| `Scene/SceneTransition.cpp`（幕） | `config.color = ClampColor({transition.color.x, transition.color.y, transition.color.z, transition.alpha});` |

### Model Renderer の色（0xRRGGBBAA → Vector4）
`Components/ModelRendererComponent.h`：
```cpp
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 色（sRGB。RGBA 0〜1。テクスチャの色に掛ける）
```
`Components/EngineComponents.cpp`：`#include "MyEngine/Math/Color.h"` を消す（使わなくなる）。`DescribeModelRenderer` の色の4行を：
```cpp
	// --- 色（sRGB。保存の形は前と同じ [r, g, b, a]）---
	ui.ColorField("color", value.color);
```
- 前も保存は `ColorField`（`[r, g, b, a]` の配列）だったので、**前のシーンもそのまま読める**。

`Editor/Inspector/ModelRendererEditor.cpp`：`// --- 色（0xRRGGBBAAで持っているので…）---` から `if (ImGui::ColorEdit4(…)) { … }` の閉じかっこまでを：
```cpp
	// --- 色（sRGB の 0〜1）---
	ImGui::ColorEdit4("Color", &render.color.x);
```
`Systems/ModelRenderSystem.cpp`：include（`Renderer.h` の下）と色：
```cpp
#include "MyEngine/Math/Color.h"
```
```cpp
		config.color = ClampColor(render.color);
```

- ここまでで `ToColorCode`・`FromColorCode`・`FromColorCodeLinear`・`0xFFFFFFFF` の色が、エンジンから無くなる（ゲームのコードは使っていない）。**ビルドのエラーが残ったら、その行も同じように直す**（エラーの出る所＝ 8 ビットの色が残っている所）。

---

## 4. パートC：Intensity（明るさの倍率）

### `Math/Color.h`
上の説明の最後（`//   色だけ直さないと…` の下）に1行：
```cpp
// 明るさ：色見本は 0〜1。1 より明るくする（光らせる）ときは、色はそのままで Intensity（倍率）を掛ける（ApplyIntensity。Unity の HDR カラーと同じ）
```
ファイルの最後（`SrgbToLinear(const Vector4&)` の下）：
```cpp

// ===== リニア → sRGB（SrgbToLinear の逆。1 を超える値も同じ曲線で）=====
inline float LinearToSrgb(float value) {
	if (value <= 0.0031308f) {
		return value * 12.92f;
	}
	return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

// ===== 明るさの倍率（Intensity）を掛けた色（HDR の色）=====
// 色は 0〜1 に収めてから、リニア（光の強さ）で intensity 倍にして、sRGB の曲線で書き直す
// Renderer は Config の色を SrgbToLinear で直すので、シェーダーに届くのはちょうど「色のリニア × intensity」
//   例：白 × Intensity 3 → Config の色は (1.61, 1.61, 1.61)、シェーダーでは (3, 3, 3)。1 を超えた分が、ブルームの Threshold 1 で光る
//   sRGB のまま 3 を掛けてはいけない（(3, 3, 3) を SrgbToLinear に通すと 12.8 になる。曲線は 1 の上でも 2.4 乗なので）
// A（透け方）は倍率を掛けず、0〜1 に収めるだけ
inline Vector4 ApplyIntensity(const Vector4& color, float intensity) {
	const Vector4 clamped = ClampColor(color);
	// 1 倍なら曲線を通さない（行き来すると、小数の誤差で値が少しずれるので）
	if (intensity == 1.0f) {
		return clamped;
	}
	// 0 以下は真っ黒（負の明るさは無い）
	if (intensity <= 0.0f) {
		return {0.0f, 0.0f, 0.0f, clamped.w};
	}
	return {
	    LinearToSrgb(SrgbToLinear(clamped.x) * intensity),
	    LinearToSrgb(SrgbToLinear(clamped.y) * intensity),
	    LinearToSrgb(SrgbToLinear(clamped.z) * intensity),
	    clamped.w,
	};
}
```
#### 解説
- `LinearToSrgb` は `SrgbToLinear` の逆の式（`(v + 0.055) / 1.055` の 2.4 乗 ↔ `1.055 × v の (1/2.4) 乗 − 0.055`）。どちらも 1 より上でも同じ曲線のまま伸びるので、行って戻るとちょうど元の値になる。
- 数の確かめ：白 × 3 → `LinearToSrgb(3) = 1.055 × 3^(1/2.4) − 0.055 = 1.61`。Renderer で `SrgbToLinear(1.61) = 3`。

### Primitive の見た目（Primitive Renderer・Polygon Renderer・Elastic Surface・Arrow Strip で共通）
`Components/PrimitiveRendererComponent.h`：`PrimitiveMaterial` の `color` の下：
```cpp
	float intensity = 1.0f;                             // 明るさの倍率（色はそのまま。1 より大きいと HDR の明るさになり、ブルームで光る）
```
- Polygon Renderer・Elastic Surface・Arrow Strip の初めの値（`.color = …, .uvScale = …` の書き方）は、並び順どおりなので直さなくてよい（間の項目を飛ばすのは書ける）。

`Systems/PrimitiveRenderSystem.cpp`：`DescribeMaterial` の `ui.ColorField(label("Color", "color")…` の下：
```cpp
	ui.Field(label("Intensity", "intensity").c_str(), material.intensity, AtLeast(0.0f).Tip("明るさの倍率（色はそのまま）。1 より大きくすると HDR の明るさになり、ブルーム（Threshold 1）で光る。Unlit・Additive だと色のまま光る"));
```
`Systems/PrimitiveRenderSystem.h`：`ApplyMaterial` の1行目（パートB で `ClampColor` にした行）：
```cpp
		config.color = ApplyIntensity(material.color, material.intensity); // 明るさの倍率はリニアで掛ける（Color.h）
```
`Systems/PolygonShapeSystem.cpp`：横の面の色（パートB で `ClampColor` にした行）：
```cpp
		config.color = ApplyIntensity({color.x * shade, color.y * shade, color.z * shade, color.w}, renderer.material.intensity);
```
`Systems/ArrowStripSystem.cpp`（同じく）：
```cpp
			config.color = ApplyIntensity(color, strip.material.intensity);
```
- Elastic Surface は `ApplyMaterial` を通るので、直す所は無い（支柱は `postIntensity` の名前で保存される）。

### Particle System
`Components/ParticleSystemComponent.h`：`colorB` の下：
```cpp
	float intensity = 1.0f;                    // 明るさの倍率（全部の粒の色に掛ける。1 より大きいと HDR の明るさになり、ブルームで光る）
```
`Systems/ParticleSystem.cpp`：Inspector の `ui.Label("初めの色は Color A と B の間でランダム（同じにすれば1色）");` の下：
```cpp
	ui.Field("Intensity###intensity", value.intensity, AtLeast(0.0f).Tip("明るさの倍率（色はそのまま。全部の粒に掛ける）。1 より大きくすると HDR の明るさになり、ブルーム（Threshold 1）で光る。Add の粒は重なった所が足されて明るくなる"));
```
`Draw` の `config.isSoftCircle = …` の上：
```cpp
		config.color = ApplyIntensity({1.0f, 1.0f, 1.0f, 1.0f}, component->intensity); // 全部の粒に掛ける色＝明るさの倍率だけ（リニアで掛ける。Color.h）
```
- `ParticleInstancesConfig::color`（全部の粒に掛ける色）を使う。粒ごとの色（初めの色 × 最後の色）はそのまま。シェーダーで「粒の色（リニア）× この色（リニア）」になるので、ちょうど Intensity 倍。

### Model Renderer
`Components/ModelRendererComponent.h`：`color` の下：
```cpp
	float intensity = 1.0f;                   // 明るさの倍率（色はそのまま。1 より大きいと HDR の明るさになり、ブルームで光る）
```
`Components/EngineComponents.cpp`：`ui.ColorField("color", value.color);` の下：
```cpp
	ui.Field("intensity", value.intensity); // 明るさの倍率（前のファイルには無い＝1 のまま）
```
`Editor/Inspector/ModelRendererEditor.cpp`：`ImGui::ColorEdit4("Color", &render.color.x);` の下：
```cpp
	ImGui::DragFloat("Intensity", &render.intensity, 0.01f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SetItemTooltip("明るさの倍率（色はそのまま）。1 より大きくすると HDR の明るさになり、ブルーム（Threshold 1）で光る");
```
`Systems/ModelRenderSystem.cpp`（パートB で `ClampColor` にした行）：
```cpp
		config.color = ApplyIntensity(render.color, render.intensity); // 明るさの倍率はリニアで掛ける（Color.h）
```

### Sprite Renderer（3D の板）
`Components/SpriteRendererComponent.h`：`color` の下：
```cpp
	float intensity = 1.0f;                        // 明るさの倍率（色はそのまま。1 より大きいと HDR の明るさになり、ブルームで光る）
```
`Systems/SpriteRenderSystem.cpp`：`DescribeSpriteRenderer` の `ui.ColorField("Color###color", value.color);` の下（**Image の方ではない**）：
```cpp
	ui.Field("Intensity###intensity", value.intensity, AtLeast(0.0f).Tip("明るさの倍率（色はそのまま）。1 より大きくすると HDR の明るさになり、ブルーム（Threshold 1）で光る。Blend を Add にすると光の板になる"));
```
`DrawSprites` の色（パートB で `ClampColor` にした行。Image の `DrawImages` は `ClampColor` のまま）：
```cpp
		config.color = ApplyIntensity(MultiplyAlpha(sprite.color, groupAlpha), sprite.intensity); // 明るさの倍率はリニアで掛ける（Color.h）
```

### Trail
`Components/TrailComponent.h`：`color` の下：
```cpp
	float intensity = 1.0f;                          // 明るさの倍率（色はそのまま。1 より大きいと HDR の明るさになり、ブルームで光る）
```
`Systems/TrailSystem.cpp`：Inspector の `ui.ColorField("Color###color", value.color);` の下：
```cpp
	ui.Field("Intensity###intensity", value.intensity, AtLeast(0.0f).Tip("明るさの倍率（色はそのまま）。1 より大きくすると HDR の明るさになり、ブルーム（Threshold 1）で光る"));
```
`Draw` の色（パートB で `ClampColor` にした行）：
```cpp
		config.color = ApplyIntensity(component->color, component->intensity); // 明るさの倍率はリニアで掛ける（Color.h）
```

---

## 5. 使い方

### エディタ
- **光らせる**：光らせたい物の Inspector で **Intensity** を 1 より大きくする（Color の下）。ブルームの Threshold が 1 なら、それだけで光る。
  - ゴールの扉（Primitive Renderer・Additive・Unlit）：Additive は後ろに足されるので、**1.5〜2 でもう白く光る**（1.4 の絵）。色を残したいなら Intensity を低めに、ToneMap を ACES か GT に。
  - パーティクル：Start の Color A・B の下の **Intensity**。2〜4 くらい。
- **Settings → PostEffect → ToneMap**：**Mode**（None／Reinhard／ACES／GT）・**Exposure**。Save で `Parameters/PostEffect/ToneMap.json`。ブルームと同じく、ゲーム全体に1つ（シーンごとではない）。

![Settings → PostEffect → ToneMap](images/hdr_settings.png)
![Primitive Renderer の Intensity（Color の下）](images/hdr_inspector_primitive.png)
![Particle System の Intensity（Color A・B の下）](images/hdr_inspector_particle.png)

### ゲームから
```cpp
// 色はそのまま、明るさだけ上げ下げする（毎フレーム書いてよい）
if (PrimitiveRendererComponent* renderer = door.GetComponent<PrimitiveRendererComponent>()) {
	renderer->material.intensity = 2.0f; // ゴールの扉を光らせる
}
if (ParticleSystemComponent* particle = sparkle.GetComponent<ParticleSystemComponent>()) {
	particle->intensity = 3.0f;
}
```
- `Renderer` を直接呼ぶときは、Config の色に `ApplyIntensity(色, 倍率)` を入れる（`config.color = ApplyIntensity({1.0f, 0.8f, 0.3f, 1.0f}, 3.0f);`）。**sRGB の色にそのまま 3 を掛けない**（1.3）。

---

## 6. 確かめたこと

| | 結果 |
|---|---|
| ビルド | パートA の後・B の後・C の後、それぞれ Debug・Release とも通る。直したファイルに警告なし |
| Release・パートA（None・Exposure 1） | 直す前と並べて画素を比べた：違うのは氷のキューブの上の縁の 74 画素だけ（1 を超えるハイライトが、Threshold 1 のブルームでうっすら光る。1.4 の絵）。ほかは同じ |
| Release・パートB | パートA とほぼ同じ（8 ビットの丸めが無くなった分の、数画素の差） |
| Release・パートC（Intensity 1 のまま） | 元の Level01 で、パートB と同じ絵 |
| Release・ゴールの扉 Intensity 1／1.5／2／3、粒 Intensity 1／4 | Intensity を上げた物が光る（1.4 の絵）。Additive の扉は 1 でも明るい背景の前の真ん中が少し光る |
| Release・ToneMap None／Reinhard／ACES／GT | 4 つとも出る（1.2 の絵）。`ToneMap.json` を書いて起動 |
| エディタ：Inspector | Primitive Renderer（ゴール）の Intensity（2.000）、Particle System の Intensity（4.000）が出て、保存したシーンの値を読む |
| エディタ：Settings → PostEffect → ToneMap | Mode（コンボ）・Exposure が出る。Mode を ACES にすると、Scene・Game ビューの両方にその場で効く |
| エディタ：View → Scene View → Post Effects を外す（`None` の道） | ブルーム・被写界深度が外れ、トーンマップ（ACES）だけ掛かった絵になる（下の絵の右） |
| エディタ：Particle System の Preview・Play 中の Game ビュー | 粒が光る |
| D3D12 のエラー | エディタを Play・Stop まで通して、ログに D3D12 の ERROR・WARNING なし（描画先の形式の食い違いが無い） |

![エディタ：左 Scene（None）・中 Scene（ACES）・右 Game（ACES）](images/hdr_editor_views.png)
![Scene ビュー：左 Post Effects あり・右 Post Effects なし（どちらも ACES）](images/hdr_scene_nopost.png)

確かめていないこと：Model Renderer・Sprite Renderer・Trail の Intensity を画面で上げて見ること（Level01 に光らせる物が無い。Inspector と保存は Primitive・Particle と同じ書き方）。ToneMap を Save して次の起動で読むこと（保存の形は今ある Bloom の Mode と同じ）。

---

## 7. 決めたこと・後でやること

| 決めたこと | 理由 |
|---|---|
| 3D の描画先の形式は、描く形（DrawCategory）で決める（Model・Particle・Line は HDR、Sprite・Text は SDR） | 1か所で済む。PSO の鍵を増やさなくてよい |
| ポスト無しの道（`None`）も HDR の `postRT_` に描いてトーンマップを通す | 3D の PSO は HDR の形なので、SDR の画面に直に描けない。どの道も同じ明るさの決まりで画面へ出る |
| 露出・トーンマップは、最後のレンズのパスに入れる（パスを増やさない） | 前から最後の出力を兼ねていた。Unity URP の Uber パスと同じく、1 回の全画面で済む |
| 風の線・ビネットはトーンマップの後 | 前の見た目（1 で切った絵に掛けていた）と同じになる |
| Exposure はブルームの後（最後のパス）で掛ける | Threshold は露出の前の値で決まる＝「Intensity が 1 を超えた物だけ光る」が、Exposure を変えても崩れない（Unity URP と同じ順） |
| 初めの値は None・Exposure 1 | 今の見た目のまま。光らせるのは Intensity を上げた物だけ |
| Config の色は sRGB のまま（1 を超える値は曲線を伸ばす）。Intensity は `ApplyIntensity` でリニアに掛ける | 色を書く決まり（sRGB）を変えずに済む。Intensity 2 が「光の強さ 2 倍」になる |
| 色は 0〜1 に収めてから Intensity を掛ける | 1 を超えさせるのは Intensity だけ。どれが光るか分かりやすい |
| Intensity は Primitive（共通の見た目）・Particle・Model・Sprite Renderer・Trail に付ける。Image・Text には付けない | 3D の Renderer は全部光らせられるようにそろえた。2D は SDR の画面に重ねるので意味が無い |
| ブルームの抜き出しは 64 で頭打ち・NaN を 0 | HDR の絵は極端な値・NaN をそのまま持つので、ちらつき・黒い四角を防ぐ |

- 後でやること：
  - **マテリアルの Emissive に倍率**（光の当たる面だけ自己発光で光らせる。Unity の Emission の HDR）。今は Color の Intensity だけ。
  - トーンマップ・露出を**シーンごと**に（Unity の Volume のように。今は Settings でゲーム全体に1つ）。
  - **自動露出**（画面の明るさの平均で Exposure を決める。DirectX SDK の HDRLighting の「Key」）。
  - 描画サイズを可変に（[PostEffect.md](PostEffect.md) の予定3）。HDR 出力（予定5）。
  - `RenderTextureCube::ToDXGIFormat` は `RenderTexture` の物と同じ中身（コピペ）。`RenderTexture::ToDXGIFormat` を public にしたので、そちらを呼ぶ形にまとめられる。
- 次は **G-3（川瀬式のスター）**：[PostEffect.md](PostEffect.md) の「G-3（スター）の目標」。HDR になったので、スター用のしきい値（ブルームより高く）で「本当に明るい所」だけから筋を出せる。
