# TextEffects：文字の演出（光・斜めの光・波・跳ねる・震える）

## このファイルについて
- [Text.md](Text.md) の続き（パートF）。Text Component に演出を足す。
  - **光（Glow）**：縁取りのさらに外へ、ふわっと光る（ゲームオーバー・スコア）
  - **斜めの光（Shine）**：キラッとした帯が、文字の上を左から右へ通る（タイトル）
  - **文字1つずつの動き（Motion）**：**Wave**（波でゆらゆら）・**Bounce**（トランポリンのように跳ねる）・**Shake**（ガタガタ震える）
- **Text.md を全部（A〜E）写してから、このファイルを写す**。Text.md の `Text2d.PS.shader` を写している途中なら、そのまま Text.md の物を写し終えてよい（このファイルで差し替える）。シェーダーの `Material` と C++ の `TextMaterialData` の並びは必ず一緒に変える（片方だけだと、足りない所をゴミの値で読んで、変な光が出る）。
- 確かめた環境：Text.md ＋ このファイル、AL4_01 のコピー。Release でビルドが通り、テストシーンで撮った（5章）。

| ファイル | 何をするか |
|---|---|
| `Shader/Data/Text/Text2d.hlsli.shader`・`Text2d.VS.shader`（新しく作る）・`Text2d.PS.shader`（全体を差し替え） | 文字のかたまりの中の場所を PS へ運ぶ・光・斜めの光 |
| `Graphics/Pipeline/ShaderConstants.h`・`Graphics/Renderer/Renderer.h/.cpp` | マテリアルに光・斜めの光を足す |
| `Components/TextComponent.h`・`Systems/TextRenderSystem.cpp`（全体を差し替え） | 演出の項目・文字1つずつの動き・時計 |
| `MyEngine_v1.vcxproj`（2ファイル） | 新しいシェーダー |

---

## 1. SDF は C++ とシェーダーのどちらでやっているのか
**両方で「SDF を作っている」のではなく、作るのが C++、使うのがシェーダー**。

| | C++（FontManager::MakeGlyph） | シェーダー（Text2d.PS） |
|---|---|---|
| すること | フォントの輪郭（曲線）から、各ピクセルの「縁までの距離」を計算して、**アトラスに焼く** | アトラスから距離を**読んで**、その場所を何色で塗るか決める |
| いつ | その文字が初めて出たときの **1回だけ** | **毎フレーム、画面のピクセルごと** |
| 重さ | 1文字数ミリ秒（輪郭の線を全部調べる） | 読むだけなので軽い |
| 変えると | 文字の形そのもの（作り直しが要る） | 見た目の演出（縁取り・光・色）。**作り直さずに毎フレーム変えられる** |

Shadertoy の SDF は「距離を数式で毎回計算する」形（円なら `length(p) - r`）。フォントの輪郭は数式1つでは書けない（曲線が何十本もある）ので、**重い計算は C++ で1回だけやって画像に焼き、シェーダーは読むだけ**にしている。Unity の TextMeshPro も同じ分け方（フォントアセットを作るときに SDF を焼く）。

だから演出は、**シェーダーで距離の使い方を変えるだけ**で増やせる（今回の光と斜めの光）。

---

## 2. 演出のしくみ

### 2.1 光（Glow）
今の縁取りは「距離が `0.5 − 縁取りの太さ` より大きい所を塗る」。光はその外側に、**距離に合わせて薄くなる色**を敷く。

```
距離    0 ………… 0.5−縁取り−光 ………… 0.5−縁取り …… 0.5 ……→ 1
        (外)    ↑光の外の端（0）        ↑光が一番強い（1）  ↑文字の縁
                └── smoothstep で 0→1、2乗して外ほど早く薄く ──┘
```

- 2乗するのは、まっすぐ（1乗）だと板のように見えるため。外へ行くほど早く薄くなると「ふわっ」とした光になる。
- 光の上に文字（縁取り込み）を重ねる：`rgb = lerp(光の色, 文字の色, 文字の a)`、`a = 文字の a ＋ 光の a ×（1 − 文字の a）`（後ろに敷く重ね方）。
- 距離を入れてあるのは縁から 8 ピクセル（基準の大きさ）まで（Text.md 1.3）なので、**縁取りと光を合わせて、文字の大きさの 1/6 くらいまで**。それより外は距離が 0 で、光が届かない。

### 2.2 斜めの光（Shine）
「文字のかたまりの中の、左から何割の所か」が要る（アトラスの UV は文字ごとにバラバラなので使えない）。

- **頂点の `position` の z・w に入れて運ぶ**：2D の頂点（`Vertex2dData`）は position が float4 で、z・w は使っていなかった（VS が 0・1 にしていた）。そこに「かたまりの中の場所（左上 0,0 〜 右下 1,1）」を入れ、**文字用の VS（`Text2d.VS`）** が `TEXCOORD1` として PS へ渡す。頂点の形は変わらないので、C++ の頂点の型・入力レイアウトはそのまま。
- PS：`帯の真ん中からの距離 = |x ＋ y × 0.3 − 帯の位置|` が帯の幅より小さい所を、光の色に寄せる（真ん中ほど強く）。`y × 0.3` で、下へ行くほど左にずれる「/」の向きの帯になる。光は**文字の中だけ**（縁取り・光の所には乗せない）。
- **帯の位置は C++ で決める**（`CalcShinePosition`）：1回 ＝「通る時間（Shine Duration）＋ 待つ時間（Shine Interval）」。`std::fmod`（割った余り）で今が何回目のどこかを出し、通っている間だけ、左の外 → 右の外へ動かす。シェーダーに時間を渡さなくてよい。

### 2.3 文字1つずつの動き（Motion）
**頂点を動かす**（C++。`CalcMotionOffset`）。シェーダーで動かすと、文字の四角の外に絵がはみ出せない（PS は四角の中しか塗れない）ので、四角ごと動かす。

```
今どこ ＝ 時間 × 速さ − 何文字目 × ずれ（Spread）   ← 文字ごとに少し遅れるので、波が伝わって見える
Wave   ：y ＝ sin(今どこ × 2π) × 大きさ          （上 → 下 → 元。ゆらゆら）
Bounce ：y ＝ |sin(今どこ × π)| × 大きさ         （0 から上がって 0 に戻る山が続く。0 の所で尖る＝着地して跳ね返る。トランポリン）
Shake  ：x・y を、違う速さ（×3.7・×5.3）の sin で  （同じ形の繰り返しに見えない。文字ごとにずれるのでバラバラにガタガタ）
```

- 時計は TextRenderSystem が持つ（`Time::GetUnscaleDeltaTime` を足していく）。一時停止・ヒットストップでも止まらないので、**停止中のエディタでも動いて見える**（置きながら調整できる）。
- 影も同じだけ動く（影の光・斜めの光は付けない）。斜めの光は「動かす前の場所」で決めるので、文字が跳ねても光は横にまっすぐ通る。

---

## 3. ゲーム（AL4_01Game.md）での使い方の案

| 場面 | 演出 | 値の目安 |
|---|---|---|
| タイトル「トランポリン・サバイバル」 | **Wave**（ゆらゆら）＋ **Shine** | Amount 10・Speed 0.8・Spread 0.08 / Shine Width 30・Duration 0.8・Interval 1.5 |
| スコア（増えたときだけ） | **Bounce**（数字がトランポリンで跳ねる） | Amount 12・Speed 3・Spread 0.12。0.5 秒たったら None に戻す（下の Script） |
| ゲームオーバー | **Shake**（少しだけ）＋ **Glow**（赤い光）。Image の黒い幕を Order の小さい方に | Shake Amount 2.5・Speed 3・Spread 0.3 / Glow Width 10・赤 |
| 硬直したとき「STUN!」（プレイヤーの近くに出すなら G9 で） | Shake ＋ 黄色の Glow | |
| ヒップドロップで図形を壊した「+100」（G5） | Bounce ＋ Shine を短く（Duration 0.3） | |
| 落ちる寸前の「DANGER」（端に近いとき） | Glow の a を sin で点滅（Script で `glowColor.w` を書く） | |

スコアが増えたときだけ跳ねさせる Script（G5 の GameManager などに）：

```cpp
// スコアを足したとき
void GameManager::AddScore(int amount) {
	score_ += amount;
	if (TextComponent* text = scoreText_.GetComponent<TextComponent>()) {
		text->text = std::format("SCORE {:06}", score_);
		text->motion = TextMotion::Bounce; // 跳ねさせる
		scorePopTimer_ = 0.5f;             // 0.5 秒だけ
	}
}

// Update
if (scorePopTimer_ > 0.0f) {
	scorePopTimer_ -= Time::GetDeltaTime();
	if (scorePopTimer_ <= 0.0f) {
		if (TextComponent* text = scoreText_.GetComponent<TextComponent>()) {
			text->motion = TextMotion::None; // 戻す（跳ねている途中でも、その場で止まる）
		}
	}
}
```

- 跳ねている途中で None にすると、その高さから一気に元に戻る。気になるなら Speed × 0.5 秒が整数になるようにする（Speed 2 なら、0.5 秒でちょうど1回跳ねて着地）。

---

## 4. パートF：コード

### `MyEngine/Shader/Data/Text/Text2d.hlsli.shader`（新しく作る）

```
#META
stage: Include
path:  Text/Text2d.hlsli
#META_END

#HLSL
struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0; // アトラスの UV
    float32_t2 local    : TEXCOORD1; // 文字のかたまりの中の場所（左上が 0,0、右下が 1,1。斜めの光に使う）
};
#HLSL_END
```

### `MyEngine/Shader/Data/Text/Text2d.VS.shader`（新しく作る）

```
#META
stage:   VS
path:    Text/Text2d.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Text2d.hlsli"

// 頂点は Sprite と同じ形（Vertex2dData）。position の z・w は使わない所なので、文字のかたまりの中の場所を入れて運ぶ
struct VertexShaderInput {
    float32_t4 position : POSITION0; // x・y：画面の正規化座標 / z・w：かたまりの中の場所（0〜1）
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = float32_t4(input.position.x, input.position.y, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    output.local = input.position.zw;
    return output;
}
#HLSL_END
```

### `MyEngine/Shader/Data/Text/Text2d.PS.shader`（全体を差し替え。`vs:` が `Text2dVS` に変わる）

```
#META
stage:   PS
path:    Text/Text2d.PS.hlsl
profile: ps_6_0
#META_END

#PROGRAM
category: Text
shading:  Unlit
vs:       Text2dVS
#PROGRAM_END

#HLSL
#include "Text2d.hlsli"
#include "Resources/Texture.hlsli"

// 文字のマテリアル（C++ の TextMaterialData と同じ並び。float4 を先に、float は後ろにまとめる）
struct Material {
    float32_t4 color;        // 文字の色
    float32_t4 outlineColor; // 縁取りの色
    float32_t4 glowColor;    // 光（グロー）の色
    float32_t4 shineColor;   // 斜めに通る光の色
    float32_t outlineWidth;  // 縁取りの太さ（SDF の値で。0 なら縁取りなし）
    uint textureIndex;       // フォントのアトラス
    float32_t glowWidth;     // 光の広さ（SDF の値で。縁取りの外側へ。0 なら無し）
    float32_t shinePosition; // 斜めの光の帯の真ん中（かたまりの左が 0、右が 1。外なら見えない）
    float32_t shineWidth;    // 帯の半分の幅（かたまりの幅に対して。0 なら無し）
};
ConstantBuffer<Material> gMaterial : register(b20);

static const float32_t kShineSlant = 0.3f; // 斜めの光の傾き（下へ行くほど左へずれる量＝「/」の向き。かたまりの幅に対して）

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

// SDF（Signed Distance Field）：アトラスの1ピクセルに「文字の縁までの距離」が入っている
//   0.5 ＝ ちょうど縁、0.5 より大きい ＝ 文字の内側、小さい ＝ 外側（縁から離れるほど 0 に近づく）
// 距離なので、しきい値（どこから塗るか）を変えるだけで、縁取り・光を足せる
PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float32_t distance = gTextures[gMaterial.textureIndex].Sample(gSampler, input.texcoord).r;

    // 画面の1ピクセル進むと distance がどれだけ変わるか（fwidth。大きく描くほど小さい）
    // その幅でなめらかに変えると、どの大きさで描いても、縁がちょうど1ピクセルでなめらかになる
    float32_t edgeWidth = max(fwidth(distance), 1e-4f);

    // ===== 1. 文字の中（縁の 0.5 より内側）=====
    float32_t fill = smoothstep(0.5f - edgeWidth, 0.5f + edgeWidth, distance);
    float32_t3 letterColor = gMaterial.color.rgb;

    // ===== 2. 斜めに通る光（文字の中だけ）=====
    // 帯の真ん中からの横の距離（下へ行くほど左へずらして「/」の向きの帯にする）。真ん中は 1、帯の端で 0
    if (gMaterial.shineWidth > 0.0f) {
        float32_t across = abs(input.local.x + input.local.y * kShineSlant - gMaterial.shinePosition);
        float32_t shine = 1.0f - smoothstep(0.0f, gMaterial.shineWidth, across);
        letterColor = lerp(letterColor, gMaterial.shineColor.rgb, shine * gMaterial.shineColor.a);
    }

    // ===== 3. 縁取り：縁より outlineWidth だけ外の線まで、縁取りの色。その上に文字を重ねる =====
    float32_t4 text = float32_t4(letterColor, gMaterial.color.a * fill);
    if (gMaterial.outlineWidth > 0.0f) {
        float32_t outlineEdge = 0.5f - gMaterial.outlineWidth;
        float32_t outline = smoothstep(outlineEdge - edgeWidth, outlineEdge + edgeWidth, distance);
        text.rgb = lerp(gMaterial.outlineColor.rgb, letterColor, fill);
        text.a = lerp(gMaterial.outlineColor.a * outline, gMaterial.color.a, fill);
    }

    // ===== 4. 光（グロー）：縁取りのさらに外へ、離れるほど薄くなる光を、文字の後ろに敷く =====
    if (gMaterial.glowWidth > 0.0f) {
        float32_t glowStart = 0.5f - gMaterial.outlineWidth - gMaterial.glowWidth; // ここで 0、縁取りの外の線で 1
        float32_t glow = smoothstep(glowStart, 0.5f - gMaterial.outlineWidth, distance);
        float32_t glowAlpha = gMaterial.glowColor.a * glow * glow; // 2乗：外へ行くほど早く薄くなる（ふわっとした光）
        // 光の上に文字を重ねる（文字が透けている所ほど、光が見える）
        text.rgb = lerp(gMaterial.glowColor.rgb, text.rgb, text.a);
        text.a = text.a + glowAlpha * (1.0f - text.a);
    }

    output.color = text;
    return output;
}
#HLSL_END
```

### `MyEngine/Graphics/Pipeline/ShaderConstants.h`（1か所。`TextMaterialData` を差し替え）

```cpp
// 文字のマテリアル（Text/Text2d.PS の Material と同じ並び。HLSL は 16 バイトごとに区切るので、Vector4 を先に、float は後ろにまとめる）
struct TextMaterialData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};        // 文字の色
	Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 1.0f}; // 縁取りの色
	Vector4 glowColor = {1.0f, 1.0f, 1.0f, 1.0f};    // 光（グロー）の色
	Vector4 shineColor = {1.0f, 1.0f, 1.0f, 1.0f};   // 斜めに通る光の色
	float outlineWidth = 0.0f;                       // 縁取りの太さ（SDF の値で 0〜0.5。0 なら縁取りなし）
	uint32_t textureIndex = 0;                       // フォントのアトラスの SRV の番号（FontManager::GetAtlasTextureIndex）
	float glowWidth = 0.0f;                          // 光の広さ（SDF の値で。縁取りの外側へ。0 なら無し）
	float shinePosition = 0.0f;                      // 斜めの光の帯の真ん中（文字のかたまりの左が 0、右が 1）
	float shineWidth = 0.0f;                         // 帯の半分の幅（かたまりの幅に対して。0 なら無し）
};
```

### `MyEngine/Graphics/Renderer/Renderer.h`（1か所。`Text2dConfig` の `outlineWidth` の下に5行）

```cpp
		Vector4 glowColor = {1.0f, 1.0f, 1.0f, 1.0f};    // 光（グロー）の色
		float glowWidth = 0.0f;                          // 光の広さ（SDF の値で。縁取りの外側へ。0 なら無し）
		Vector4 shineColor = {1.0f, 1.0f, 1.0f, 1.0f};   // 斜めに通る光の色
		float shinePosition = 0.0f;                      // 斜めの光の帯の真ん中（文字のかたまりの左が 0、右が 1）
		float shineWidth = 0.0f;                         // 帯の半分の幅（かたまりの幅に対して。0 なら無し）
```

### `MyEngine/Graphics/Renderer/Renderer.cpp`（1か所。`DrawText2d` の `outlineWidth` の下に5行）

```cpp
	req.materialData.glowColor = config.glowColor;
	req.materialData.glowWidth = config.glowWidth;
	req.materialData.shineColor = config.shineColor;
	req.materialData.shinePosition = config.shinePosition;
	req.materialData.shineWidth = config.shineWidth;
```

### `MyEngine/Components/TextComponent.h`（全体を差し替え）

```cpp
#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Components/ImageComponent.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/String/FixedString.h"

// 画面に重ねる文字（スコア・ゲームオーバー・説明）。Entityに付けるComponent（カテゴリ：UI）
// 置き方は Image と同じ：Transform の translation の x・y が画面のピクセル（1280×720 の画面のつもり。Y は上が＋）、
// 回転は Inspector の Rotation の z、大きさには scale の x・y も掛かる。Anchor・Pivot・Order も Image と同じ（Order で Image とも重なる）
// 文字は Script から書き換える：GetComponent<TextComponent>()->text = std::format("SCORE {}", score_);
// 文字の形は SDF（縁までの距離の画像）にして描くので、大きくしてもぼやけない。縁取り・影も付けられる
// 演出：光（グロー）・斜めに通る光（シェーダー）、文字1つずつの動き 波・跳ねる・震える（頂点を動かす）


/// <summary>
/// 行のそろえ方（文字が何行かあるとき、どちらの端にそろえるか）
/// </summary>
enum class TextAlign {
	Left,   // 左にそろえる
	Center, // 真ん中にそろえる
	Right,  // 右にそろえる
};

/// <summary>
/// 文字1つずつの動き（どれも、文字ごとに少しずつ時間をずらして動かす）
/// </summary>
enum class TextMotion {
	None,   // 動かない
	Wave,   // 波：上下にゆらゆら（タイトル）
	Bounce, // 跳ねる：トランポリンのように、下に着いては上へ跳ねる（スコアが増えたとき）
	Shake,  // 震える：細かくガタガタ（ゲームオーバー・ダメージ）
};

/// <summary>
/// 画面に重ねる文字（Unity の UI の Text）
/// </summary>
struct TextComponent {
	bool enabled = true;                             // false の間は描かない
	FixedString<256> text = "Text";                  // 出す文字（UTF-8。\n で改行。255 バイト＝日本語で85文字くらいまで）
	uint32_t fontHandle = 0;                         // フォント（FontManager::Load の番号。0 なら既定のフォント＝メイリオ）。保存はパスで
	float fontSize = 36.0f;                          // 文字の大きさ（ピクセル。全角の1文字がだいたいこの幅）
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};        // 文字の色（RGBA。0〜1）
	TextAlign align = TextAlign::Center;             // 行のそろえ方
	ScreenAnchor anchor = ScreenAnchor::Center;      // 画面のどこから位置を数えるか（Image と同じ）
	Vector2 pivot = {0.5f, 0.5f};                    // 文字のかたまりのどこを位置に合わせるか（0,0=左下 0,1=左上 1,1=右上）
	float lineSpacing = 1.0f;                        // 行の間の倍率（1 でフォントの決めた間。小さくすると詰まる）
	float letterSpacing = 0.0f;                      // 文字と文字の間に足す（ピクセル。マイナスで詰める）
	float outlineWidth = 0.0f;                       // 縁取りの太さ（ピクセル。0 なら無し。文字の大きさの 1/6 くらいまで）
	Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 1.0f}; // 縁取りの色
	bool shadow = false;                             // 影を付ける（同じ文字を、ずらして先に描く）
	Vector2 shadowOffset = {3.0f, -3.0f};            // 影のずれ（ピクセル。右下なら x が＋、y が−）
	Vector4 shadowColor = {0.0f, 0.0f, 0.0f, 0.6f};  // 影の色
	// --- 演出（TextEffects.md）---
	float glowWidth = 0.0f;                          // 光（グロー）の広さ（ピクセル。縁取りの外へ。0 なら無し。縁取りと合わせて、文字の大きさの 1/6 くらいまで）
	Vector4 glowColor = {1.0f, 0.8f, 0.3f, 0.8f};    // 光の色
	bool shine = false;                              // 斜めの光が、文字の上を左から右へ通る
	Vector4 shineColor = {1.0f, 1.0f, 1.0f, 0.9f};   // 斜めの光の色（a で強さ）
	float shineWidth = 24.0f;                        // 斜めの光の帯の半分の幅（ピクセル）
	float shineDuration = 0.6f;                      // 光が端から端まで通る秒数
	float shineInterval = 2.0f;                      // 光が通ってから、次に通るまでの秒数
	TextMotion motion = TextMotion::None;            // 文字1つずつの動き
	float motionAmount = 8.0f;                       // 動く大きさ（ピクセル）
	float motionSpeed = 2.0f;                        // 動く速さ（1秒に何回）
	float motionSpread = 0.1f;                       // 隣の文字との、動きのずれ（1回の動きの割合。0.1 なら隣は 1/10 回分遅れる。0 なら全部一緒）
	int order = 0;                                   // 重ねる順（大きいほど手前。Image の Order と比べる）
	BlendMode blend = BlendMode::Normal;             // 重ね方（Normal：透明な所が透ける / Add：足して光る）
};

static_assert(std::is_trivially_copyable_v<TextComponent>);
```

### `MyEngine/Systems/TextRenderSystem.cpp`（全体を差し替え）
Text.md の物から変わった所：include（`<cmath>`・`<numbers>`・`EngineTime.h`）、定数4つ、`TextLayout` の `min`・`max`、`TextRenderState` の `time`、`LayoutText` の `layout.min/max`、`CalcMotionOffset`・`CalcShinePosition`（新しい）、`MakeVertices`（引数と z・w）、`DescribeText` の Glow・Shine・Motion、`Draw` の時計・光・斜めの光。

```cpp
#include "TextRenderSystem.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <string_view>
#include <vector>

#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Graphics/Text/FontManager.h"
#include "MyEngine/Systems/SpriteRenderSystem.h"
#include "MyEngine/Time/EngineTime.h"

namespace {
constexpr char32_t kReplacementCharacter = U'�'; // 壊れた UTF-8 の代わりに出す文字（？の形）
constexpr float kMaxOutlineWidth = 0.45f;             // 縁取りの太さの上限（SDF の値。0.5 にすると、距離を入れた範囲の外（値が 0 の所）まで塗って、四角く見える）。光（グロー）も合わせてここまで
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>; // 1回の動き（sin の1周）
constexpr float kShineSlant = 0.3f;                   // 斜めの光の傾き（Text2d.PS の kShineSlant と同じ値にする。帯を端から端まで通すのに使う）
constexpr float kShakeFrequencyX = 3.7f;              // 震えるときの横の速さの倍率（縦と違う数にして、同じ動きの繰り返しに見えないように）
constexpr float kShakeFrequencyY = 5.3f;              // 震えるときの縦の速さの倍率

/// <summary>
/// 並べた文字1つ（ピクセル。Entity の位置が原点、Y は上が＋）
/// </summary>
struct PlacedGlyph {
	Vector2 min = {0.0f, 0.0f};   // 左下
	Vector2 max = {0.0f, 0.0f};   // 右上
	Vector2 uvMin = {0.0f, 0.0f}; // アトラスの中の左上
	Vector2 uvMax = {0.0f, 0.0f}; // アトラスの中の右下
};

/// <summary>
/// 1行分（glyphs の first 番目から count 個）
/// </summary>
struct TextLine {
	size_t first = 0;   // その行の最初の文字が、glyphs の何番目か
	size_t count = 0;   // その行の、絵のある文字の数（空白は数えない）
	float width = 0.0f; // 行の幅（ピクセル）
};

/// <summary>
/// 文字を並べた結果（毎回作り直す。配列のメモリは使い回す）
/// </summary>
struct TextLayout {
	std::vector<PlacedGlyph> glyphs; // 絵のある文字
	std::vector<TextLine> lines;     // 行（1行でも1つある）
	Vector2 size = {0.0f, 0.0f};     // 文字のかたまりの大きさ（ピクセル）
	Vector2 min = {0.0f, 0.0f};      // 文字のかたまりの左下（ピクセル。Entity の位置が原点）
	Vector2 max = {0.0f, 0.0f};      // 文字のかたまりの右上
};

/// <summary>
/// TextRenderSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct TextRenderState {
	std::vector<char32_t> codepoints; // 1文字ずつに分けた文字（作業用）
	TextLayout layout;                // 並べた結果（作業用）
	float time = 0.0f;                // 演出の時計（秒。一時停止・停止中も進む＝エディタでも動いて見える）
};
std::unique_ptr<TextRenderState> state;

//=============================================================================
// 文字を分ける・並べる
//=============================================================================
// UTF-8 の文字を、1文字ずつの番号（Unicode のコードポイント）に分ける
// UTF-8 は1文字が1〜4バイト。1バイト目の上のビットで長さが分かる（0xxxxxxx=1、110xxxxx=2、1110xxxx=3、11110xxx=4。続きのバイトは 10xxxxxx）
// 1バイト目の残りのビットと、続きのバイトの下6ビットずつをつなげると、文字の番号になる（「あ」= E3 81 82 → U+3042）
void DecodeUtf8(std::string_view text, std::vector<char32_t>& out) {
	out.clear();
	size_t i = 0;
	while (i < text.size()) {
		const uint8_t lead = static_cast<uint8_t>(text[i]);
		size_t length = 0;
		char32_t codepoint = 0;
		if (lead < 0x80) {
			length = 1;
			codepoint = lead;
		} else if ((lead & 0xE0) == 0xC0) {
			length = 2;
			codepoint = lead & 0x1F;
		} else if ((lead & 0xF0) == 0xE0) {
			length = 3;
			codepoint = lead & 0x0F;
		} else if ((lead & 0xF8) == 0xF0) {
			length = 4;
			codepoint = lead & 0x07;
		} else {
			out.push_back(kReplacementCharacter); // 続きのバイトが頭に来た（壊れている）
			++i;
			continue;
		}
		// 続きのバイトをつなげる（足りない・形が違うなら壊れている）
		bool isValid = (i + length <= text.size());
		for (size_t k = 1; isValid && k < length; ++k) {
			const uint8_t next = static_cast<uint8_t>(text[i + k]);
			isValid = ((next & 0xC0) == 0x80);
			codepoint = (codepoint << 6) | (next & 0x3F);
		}
		if (!isValid) {
			out.push_back(kReplacementCharacter);
			++i;
			continue;
		}
		out.push_back(codepoint);
		i += length;
	}
}

// 行を横にずらす量（space は「かたまりの幅 − その行の幅」＝その行の余り）
float AlignOffset(TextAlign align, float space) {
	switch (align) {
	case TextAlign::Left:
		return 0.0f;
	case TextAlign::Center:
		return space * 0.5f;
	case TextAlign::Right:
		return space;
	}
	return 0.0f;
}

// 文字を並べる（ピクセル。Entity の位置が原点、Y は上が＋。Pivot とそろえ方も入れる）
void LayoutText(const TextComponent& text, uint32_t font, TextLayout& layout) {
	std::vector<PlacedGlyph>& glyphs = layout.glyphs;
	std::vector<TextLine>& lines = layout.lines;
	glyphs.clear();
	lines.clear();
	DecodeUtf8(text.text.View(), state->codepoints);

	const float scale = text.fontSize / FontManager::kBaseSize; // 基準の大きさ → 描く大きさ
	const FontMetrics& metrics = FontManager::GetMetrics(font);
	const float ascent = metrics.ascent * scale;                                                                       // ベースラインから上の端まで
	const float lineHeight = (metrics.ascent - metrics.descent + metrics.lineGap) * scale * text.lineSpacing; // 行の頭から次の行の頭まで

	// --- 1. 行ごとに、文字を左から並べる（x は行の頭から、y はベースラインから）---
	lines.push_back({});
	float penX = 0.0f;               // ペンの位置（次の文字を置く所）
	const Glyph* previous = nullptr; // 1つ前の文字（字詰めに使う。FontManager が持っている物を指すだけ）
	for (const char32_t codepoint : state->codepoints) {
		if (codepoint == U'\r') {
			continue; // Windows の改行（\r\n）の \r は飛ばす
		}
		if (codepoint == U'\n') {
			lines.back().width = penX;
			lines.push_back({glyphs.size(), 0, 0.0f});
			penX = 0.0f;
			previous = nullptr;
			continue;
		}
		const Glyph& glyph = FontManager::GetGlyph(font, codepoint);
		if (previous != nullptr) {
			penX += FontManager::GetKerning(font, *previous, glyph) * scale; // 「AV」などは少し詰める
		}
		if (glyph.size.x > 0.0f) {
			PlacedGlyph placed;
			placed.min.x = penX + glyph.offset.x * scale;
			placed.max.x = placed.min.x + glyph.size.x * scale;
			// offset.y は下が＋（stb_truetype の決まり）なので、上が＋に直す。絵の上の端がベースラインから -offset.y の高さ
			placed.max.y = -glyph.offset.y * scale;
			placed.min.y = placed.max.y - glyph.size.y * scale;
			placed.uvMin = glyph.uvMin;
			placed.uvMax = glyph.uvMax;
			glyphs.push_back(placed);
			++lines.back().count;
		}
		penX += glyph.advance * scale + text.letterSpacing;
		previous = &glyph;
	}
	lines.back().width = penX;

	// --- 2. かたまりの大きさ（幅は一番長い行、高さは行の数から）---
	float width = 0.0f;
	for (const TextLine& line : lines) {
		width = (std::max)(width, line.width);
	}
	const float height = static_cast<float>(lines.size() - 1) * lineHeight + (metrics.ascent - metrics.descent) * scale;
	layout.size = {width, height};

	// --- 3. 行ごとに、そろえ方で横にずらし、ベースラインの高さに置く（Pivot の所が Entity の位置になるように）---
	const float left = -text.pivot.x * width;
	const float top = (1.0f - text.pivot.y) * height;
	layout.min = {left, top - height};
	layout.max = {left + width, top};
	for (size_t row = 0; row < lines.size(); ++row) {
		const TextLine& line = lines[row];
		const Vector2 shift = {left + AlignOffset(text.align, width - line.width), top - ascent - static_cast<float>(row) * lineHeight};
		for (size_t i = line.first; i < line.first + line.count; ++i) {
			glyphs[i].min += shift;
			glyphs[i].max += shift;
		}
	}
}

//=============================================================================
// 描く
//=============================================================================
// 使うフォント（選んでいなければ既定のフォント）
uint32_t ResolveFont(uint32_t fontHandle) { return (fontHandle != 0) ? fontHandle : FontManager::GetDefaultFont(); }

// 縁取りの太さ（描く大きさのピクセル）→ SDF の値（シェーダーの outlineWidth）
// SDF は、縁から kSdfPadding ピクセル（基準の大きさで）離れると値が 0.5 変わる。描く大きさの1ピクセルは、基準の大きさでは kBaseSize ÷ fontSize ピクセル
float ToSdfWidth(float pixels, float fontSize) {
	if (pixels <= 0.0f || fontSize <= 0.0f) {
		return 0.0f;
	}
	const float basePixels = pixels * FontManager::kBaseSize / fontSize;
	return (std::min)(basePixels * 0.5f / static_cast<float>(FontManager::kSdfPadding), kMaxOutlineWidth);
}

// index 番目の文字を、今どれだけ動かすか（ピクセル）。文字ごとに motionSpread だけ遅らせるので、波のように伝わって見える
Vector2 CalcMotionOffset(const TextComponent& text, size_t index, float time) {
	const float cycle = time * text.motionSpeed - static_cast<float>(index) * text.motionSpread; // 何回目の動きの、どこにいるか
	switch (text.motion) {
	case TextMotion::None:
		return {0.0f, 0.0f};
	case TextMotion::Wave:
		// 波：上下に sin でゆらゆら（1回で、上 → 下 → 元）
		return {0.0f, std::sin(cycle * kTwoPi) * text.motionAmount};
	case TextMotion::Bounce:
		// 跳ねる：|sin| は「0 から上がって 0 に戻る」山が続く形。0 の所が着地（下に尖る）、上で一瞬止まって見える
		return {0.0f, std::abs(std::sin(cycle * std::numbers::pi_v<float>)) * text.motionAmount};
	case TextMotion::Shake:
		// 震える：横と縦を、違う速さの sin で動かす（同じ形の繰り返しに見えない）。文字ごとにずれるのでバラバラに震える
		return {std::sin(cycle * kTwoPi * kShakeFrequencyX) * text.motionAmount, std::sin((cycle + 0.25f) * kTwoPi * kShakeFrequencyY) * text.motionAmount};
	}
	return {0.0f, 0.0f};
}

// 斜めの光の帯の真ん中（かたまりの左が 0、右が 1）。通っていない間は、見えない所（-10）
// 1回 ＝ 通る時間（shineDuration）＋ 待つ時間（shineInterval）。帯の幅と傾きの分だけ外から始めて、外まで通す
float CalcShinePosition(const TextComponent& text, float halfWidth, float time) {
	const float duration = (std::max)(text.shineDuration, 0.01f);
	const float progress = std::fmod(time, duration + (std::max)(text.shineInterval, 0.0f)) / duration; // 0〜1 の間だけ通っている
	if (progress > 1.0f) {
		return -10.0f;
	}
	// 帯は「/」の向き（下へ行くほど左）。上の端は shinePosition、下の端は shinePosition − kShineSlant にある
	const float start = -halfWidth;              // 上の端も左の外にある所から
	const float end = 1.0f + halfWidth + kShineSlant; // 下の端も右の外に出る所まで
	return start + (end - start) * progress;
}

// 並べた文字を、画面の正規化座標の頂点にする（1文字4つ。左下・左上・右下・右上の順）。offset はずらす量（影。ピクセル）
// position の z・w には「文字のかたまりの中の場所（左上 0,0 〜 右下 1,1）」を入れる（Text2d.VS がシェーダーへ運ぶ。斜めの光に使う）
std::vector<Vertex2dData> MakeVertices(const TextLayout& layout, const TextComponent& text, const Matrix4x4& worldMatrix, const Vector2& offset, float time) {
	std::vector<Vertex2dData> vertices;
	vertices.reserve(layout.glyphs.size() * 4);
	const float width = (std::max)(layout.size.x, 1.0f);
	const float height = (std::max)(layout.size.y, 1.0f);
	for (size_t i = 0; i < layout.glyphs.size(); ++i) {
		const PlacedGlyph& glyph = layout.glyphs[i];
		const Vector2 move = offset + CalcMotionOffset(text, i, time); // 影のずれ ＋ 文字1つずつの動き
		const auto toVertex = [&](float x, float y, float u, float v) {
			const Vector2 point = SpriteRenderSystem::CalcScreenPoint(worldMatrix, text.anchor, {x + move.x, y + move.y});
			const Vector2 local = {(x - layout.min.x) / width, (layout.max.y - y) / height}; // 動かす前の場所（光は文字と一緒に動かない）
			return Vertex2dData{{point.x, point.y, local.x, local.y}, {u, v}};
		};
		// アトラスは上が v=0（画像の上）、画面は上が＋なので、上の端（max.y）に uvMin.y を合わせる
		vertices.push_back(toVertex(glyph.min.x, glyph.min.y, glyph.uvMin.x, glyph.uvMax.y)); // 左下
		vertices.push_back(toVertex(glyph.min.x, glyph.max.y, glyph.uvMin.x, glyph.uvMin.y)); // 左上
		vertices.push_back(toVertex(glyph.max.x, glyph.min.y, glyph.uvMax.x, glyph.uvMax.y)); // 右下
		vertices.push_back(toVertex(glyph.max.x, glyph.max.y, glyph.uvMax.x, glyph.uvMin.y)); // 右上
	}
	return vertices;
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// Text の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Image と同じ書き方）
void DescribeText(ComponentUI& ui, TextComponent& value) {
	ui.Field("Enabled###enabled", value.enabled);
	ui.Field("Text###text", value.text, Multiline().Tip("出す文字（Enter で改行。255 バイト＝日本語で85文字くらいまで）。Script からは text = \"...\" で書き換える"));
	ui.AssetField("Font###font", value.fontHandle, AssetType::Font, Tip("(none) なら既定のフォント（メイリオ）。resources の下に置いたフォントと、Windows に入っているフォントから選べる"));
	ui.Field("Font Size###fontSize", value.fontSize, AtLeast(1.0f).Tip("文字の大きさ（ピクセル。1280×720 の画面のつもりで。全角の1文字がだいたいこの幅）"));
	ui.ColorField("Color###color", value.color);
	ui.Field("Align###align", value.align, Tip("何行かあるとき、どちらの端にそろえるか"));
	ui.Field("Anchor###anchor", value.anchor, Tip("画面のどこから位置を数えるか（位置は Transform の Translation の x・y。ピクセルで、上が＋）"));
	ui.Field("Pivot###pivot", value.pivot, Tip("文字のかたまりのどこを位置に合わせるか（0,0=左下 0,1=左上 1,1=右上）。画面の左上に置くなら、Anchor を TopLeft・Pivot を 0,1"));
	ui.Field("Line Spacing###lineSpacing", value.lineSpacing, AtLeast(0.0f).Tip("行の間の倍率（1 でフォントの決めた間。小さくすると詰まる）"));
	ui.Field("Letter Spacing###letterSpacing", value.letterSpacing, Tip("文字と文字の間に足す（ピクセル。マイナスで詰める）"));
	ui.Field("Order###order", value.order, Tip("重ねる順（大きいほど手前。Image の Order とも比べる）"));
	ui.Field("Blend###blend", value.blend, Tip("Normal：透明な所が透ける / Add：足して光る"));
	ui.Separator("Outline");
	ui.Field("Outline Width###outlineWidth", value.outlineWidth, AtLeast(0.0f).Tip("縁取りの太さ（ピクセル。0 なら無し。文字の大きさの 1/6 くらいで止まる）"));
	ui.ColorField("Outline Color###outlineColor", value.outlineColor);
	ui.Separator("Shadow");
	ui.Field("Shadow###shadow", value.shadow, Tip("影を付ける（同じ文字を、ずらして影の色で先に描く）"));
	ui.Field("Shadow Offset###shadowOffset", value.shadowOffset, Tip("影のずれ（ピクセル。右下なら x が＋、y が−）"));
	ui.ColorField("Shadow Color###shadowColor", value.shadowColor);
	ui.Separator("Glow");
	ui.Field("Glow Width###glowWidth", value.glowWidth, AtLeast(0.0f).Tip("光（グロー）の広さ（ピクセル。縁取りの外へ、離れるほど薄く。0 なら無し）"));
	ui.ColorField("Glow Color###glowColor", value.glowColor);
	ui.Separator("Shine");
	ui.Field("Shine###shine", value.shine, Tip("斜めの光が、文字の上を左から右へ通る（キラッと光る）"));
	ui.ColorField("Shine Color###shineColor", value.shineColor);
	ui.Field("Shine Width###shineWidth", value.shineWidth, AtLeast(0.0f).Tip("光の帯の半分の幅（ピクセル）"));
	ui.Field("Shine Duration###shineDuration", value.shineDuration, AtLeast(0.01f).Tip("光が端から端まで通る秒数"));
	ui.Field("Shine Interval###shineInterval", value.shineInterval, AtLeast(0.0f).Tip("光が通ってから、次に通るまでの秒数"));
	ui.Separator("Motion");
	ui.Field("Motion###motion", value.motion, Tip("文字1つずつの動き（Wave：波 / Bounce：跳ねる / Shake：震える）"));
	ui.Field("Amount###motionAmount", value.motionAmount, Tip("動く大きさ（ピクセル）"));
	ui.Field("Speed###motionSpeed", value.motionSpeed, AtLeast(0.0f).Tip("動く速さ（1秒に何回）"));
	ui.Field("Spread###motionSpread", value.motionSpread, Tip("隣の文字との、動きのずれ（1回の動きの割合。0.1 なら隣は 1/10 回分遅れる。0 なら全部一緒）"));
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void TextRenderSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<TextRenderState>();

	// Text の置き場所・保存・Inspector を登録する。見せ方は DescribeText 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<TextComponent>();
	ComponentSerializer::Register<TextComponent>("Text", &DescribeText);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<TextComponent>>("Text", "UI", &DescribeText));
}

void TextRenderSystem::Release() { state.reset(); }


//=============================================================================
// 描く（停止中も）
//=============================================================================
void TextRenderSystem::Draw() {
	MY_ASSERT_MSG(state != nullptr, "TextRenderSystem::Initialize()を先に呼んでください");
	// 演出の時計を進める（一時停止・ヒットストップでも止めない時間。停止中のエディタでも動いて見える）
	state->time += Time::GetUnscaleDeltaTime();
	const float time = state->time;
	EntityManager::ForEach<TextComponent>([time](Handle<Entity> entity, const TextComponent& text) {
		if (!text.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		const uint32_t font = ResolveFont(text.fontHandle);
		if (transform == nullptr || font == 0) {
			return;
		}
		TextLayout& layout = state->layout;
		LayoutText(text, font, layout);
		if (layout.glyphs.empty()) {
			return; // 空・空白だけ
		}

		Renderer::Text2dConfig config;
		config.fontTextureIndex = FontManager::GetAtlasTextureIndex(font);
		config.outlineWidth = ToSdfWidth(text.outlineWidth, text.fontSize);
		config.blendMode = text.blend;
		config.order = text.order;
		// --- 影：同じ文字を、ずらして影の色で先に描く（縁取りの所も影にする。同じ Order なので、積んだ順＝影が奥）---
		if (text.shadow) {
			config.color = text.shadowColor;
			config.outlineColor = text.shadowColor;
			Renderer::DrawText2d(config, MakeVertices(layout, text, transform->worldMatrix, text.shadowOffset, time));
		}
		// --- 文字（光・斜めの光は、影には付けない）---
		config.color = text.color;
		config.outlineColor = text.outlineColor;
		// 光は縁取りの外へ広げるので、縁取りと合わせて上限まで
		config.glowWidth = (std::min)(ToSdfWidth(text.glowWidth, text.fontSize), kMaxOutlineWidth - config.outlineWidth);
		config.glowColor = text.glowColor;
		if (text.shine) {
			const float halfWidth = text.shineWidth / (std::max)(layout.size.x, 1.0f); // ピクセル → かたまりの幅に対する割合
			config.shineWidth = halfWidth;
			config.shinePosition = CalcShinePosition(text, halfWidth, time);
			config.shineColor = text.shineColor;
		}
		Renderer::DrawText2d(config, MakeVertices(layout, text, transform->worldMatrix, {0.0f, 0.0f}, time));
	});
}


//=============================================================================
// ゲームから使う
//=============================================================================
Vector2 TextRenderSystem::CalcTextSize(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "TextRenderSystem::Initialize()を先に呼んでください");
	const TextComponent* text = EntityManager::Get<TextComponent>(entity);
	if (text == nullptr) {
		return {0.0f, 0.0f};
	}
	const uint32_t font = ResolveFont(text->fontHandle);
	if (font == 0) {
		return {0.0f, 0.0f};
	}
	LayoutText(*text, font, state->layout);
	return state->layout.size;
}
```

### `MyEngine_v1.vcxproj`（2ファイル）
`MyEngine/Shader/Data/Text/` に `Text2d.hlsli.shader`・`Text2d.VS.shader` を「既存の項目」で足して、`Tools\UpdateFilters.ps1`。Release で動かすなら、ゲームの `project/MyEngine/Shader/Data/Text/` にも3つとも写す（Text.md 8章）。

---

## 5. 確かめたこと
Release（エディタ無し）、Text.md のテストシーンに演出を付けて撮った。

![演出](images/text_effects.png)

- タイトル：Wave（0.35 秒ずらして撮った2枚で、文字の高さが波の形に変わる）・Shine（帯を赤く太くして、帯の場所を 0.18 秒ごとに数えた：x = 499 → 569 → 793 → 884 → 1049 → 見えない、と左から右へ通り抜けた）。
- SCORE：Bounce（文字ごとに違う高さ）。GAME OVER：Shake（2枚で文字がバラバラにずれる）＋ 赤い Glow。右上：緑の Glow。
- エラー・警告なし。ビルドは Release で確かめた（エディタの Inspector の Glow・Shine・Motion の欄は、`DescribeText` の `ui.Field` を足しただけ）。
