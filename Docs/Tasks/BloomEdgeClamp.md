# ブルームが画面の反対の端に出る不具合の直し（サンプラーを CLAMP に）

参考：
- D3D12_TEXTURE_ADDRESS_MODE（WRAP・CLAMP などの意味）：https://learn.microsoft.com/ja-jp/windows/win32/api/d3d12/ne-d3d12-d3d12_texture_address_mode
- D3D12_STATIC_SAMPLER_DESC：https://learn.microsoft.com/ja-jp/windows/win32/api/d3d12/ns-d3d12-d3d12_static_sampler_desc

## このファイルについて
- 不具合：画面の端にかかった光る物（青い泡・緑の泡・ゴールの光）のブルームが、**画面の反対の端**にも出る。
  - 上の端に泡がかかる → 下の端の同じ横位置がぼんやり光る
  - 右の端に緑の泡 → 左の端の同じ高さが緑に光る
- **写す場所**：`AL4_01/MyEngine_v1`（サブモジュール。川瀬式ブルーム・トーンマップ入りのエンジン cf641e7 に合わせて書いた）→ エンジンでコミット・push → `source/repos/MyEngine_v1` で pull。
- **Release は `AL4_01/project/MyEngine/Shader` の写しを読む**。シェーダー（パートA の 2〜4）は、写しの方にも同じ直しを入れる（入れないと Release だけ直らない。写すときは A-2〜A-4 の 8 ファイルをまとめて入れる。一部だけだと、`gSamplerClamp` が無くてシェーダーのコンパイルで止まる）。
- ゲーム（AL4_01）のコードは変わらない。
- パートA が不具合の直し。パートB は同じ種類の直しを被写界深度（DOF）とレンズに入れる物（やらなくてもブルームは直る）。

---

## 1. なぜ反対の端に出るか

### 1.1 サンプラーが WRAP（繰り返し）になっている
今のエンジンのサンプラーは、`RootSignatureManager::MakeStaticSampler` で **全部 WRAP** に作っている。

```
UV が 0〜1 をはみ出したときの読み方
  WRAP  ：繰り返す。 UV = -0.05 → 0.95 を読む（反対の端）   ← 今
  CLAMP ：端で止める。UV = -0.05 → 0.00 を読む（同じ端）
```

WRAP はモデルのテクスチャを「タイル状に並べる」ために要る。でも**画面の絵**を読むポストエフェクトでは、
画面の上の外側を読んだつもりが、画面の下の端を読んでしまう。

### 1.2 ブルームは「はみ出して読む」
ブルームのぼかし（`Tent`・`BloomGaussian`）は、今の画素の周り（±1〜±4 画素）を読んで混ぜる。
画面の端の画素では、周りの半分が画面の外になる → WRAP なので反対の端の色が混ざる。

しかも小さい段ほど 1 画素が大きい：

| 段 | 大きさ（1920×1080 のとき） | 4 画素ぶんが画面の何割か（縦） |
|---|---|---|
| 1/4 | 480×270 | 1.5% |
| 1/32 | 60×33 | **12%** |

川瀬式の 1/32 の段は ±4 画素読むので、画面の端から 12% の所まで反対の端の光が混ざる。
それを拡大して足すので、「反対の端がぼんやり光る」形になる（画像の下の端の青い光）。

### 1.3 `saturate(uv)` では直らない
「UV を 0〜1 に収めれば良い」と思うが、それでも少し混ざる。
画素の中心は `0.5 / 幅` にあるので、UV = 0 を読むと、**0 番の画素と −1 番の画素の真ん中**を読んだことになる（バイリニアで半々に混ぜる）。
WRAP だと −1 番 ＝ 反対の端の画素なので、結局半分混ざる。だから**サンプラー側を CLAMP にする**のが正しい直し方。

### 1.4 直し方
- `Resources/Texture.hlsli` に **2つ目のサンプラー `gSamplerClamp`（s1）** を足す。
- サンプラーの読み方は reflection（シェーダーから取れる情報）では分からないので、今のフィルタと同じく**規約で決める**：
  変数名が `gSamplerClamp` なら CLAMP、それ以外は今まで通り WRAP。
- ブルームのシェーダーは全部 `gSamplerClamp` で読む。

> ルートシグネチャはシェーダーで**使っている**物だけで作られる（使っていない変数はコンパイラが消すので reflection に出てこない）。
> だからモデルのシェーダーは今まで通り s0（WRAP）だけ、ブルームは s1（CLAMP）だけになる。
>
> ブルームのルートシグネチャは `BloomCompositePS` の reflection だけで作り、それを 6 つの PSO で共有している（`BloomPass::CreateRootSignature`）。
> なので、**ブルームのシェーダーは全部そろえて** `gSamplerClamp` にすること。1つでも `gSampler`（s0）のまま残ると、
> ルートシグネチャに s0 が無いので、その PSO の作成で「Bloom PSO作成失敗」の assert が出る。

---

## 2. パートA：ブルームを CLAMP で読む（不具合の直し）

### A-1 `Graphics/Pipeline/RootSignatureManager.cpp`
`MakeStaticSampler` を丸ごと差し替え。

```cpp
//=============================================================================
// Resourceから Sampler 作成
//=============================================================================
D3D12_STATIC_SAMPLER_DESC RootSignatureManager::MakeStaticSampler(const MergedBind& m) {
	D3D12_STATIC_SAMPLER_DESC s{};
	s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // フィルタはreflectionで取れないので規約

	// --- UV がはみ出したときの読み方（これも reflection で取れないので、変数名で決める規約）---
	// gSamplerClamp：端の色で止める。画面の絵を読むポストエフェクト用（WRAP だと画面の反対の端の色が混ざる）
	// それ以外      ：繰り返す。モデルのテクスチャをタイル状に並べる用
	// 変数名は Resources/Texture.hlsli と合わせる
	const bool isClamp = (m.bind.name == "gSamplerClamp");
	const D3D12_TEXTURE_ADDRESS_MODE addressMode = isClamp ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	s.AddressU = s.AddressV = s.AddressW = addressMode;

	s.MaxLOD = D3D12_FLOAT32_MAX;
	s.ShaderRegister = m.bind.registerNumber;
	s.RegisterSpace = m.bind.space;
	s.ShaderVisibility = m.visibility;
	return s;
}
```

### A-2 `Shader/Data/Resources/Texture.hlsli.shader`（丸ごと）

```hlsl
#META
stage: Include
path : Resources/Texture.hlsli
#META_END

#HLSL
// バインドレス2Dテクスチャ（Material.textureIndex で引く）
Texture2D<float32_t4> gTextures[] : register(t0);

// サンプラー。UV がはみ出したときの読み方は、RootSignatureManager::MakeStaticSampler が変数名で決める
SamplerState gSampler : register(s0);      // 繰り返す（WRAP）。モデル・スプライトのテクスチャ用
SamplerState gSamplerClamp : register(s1); // 端の色で止める（CLAMP）。画面の絵を読むポストエフェクト用
#HLSL_END
```

### A-3 `Shader/Data/Post/Bloom.hlsli.shader`
`Tent` を丸ごと差し替え（`gSampler` → `gSamplerClamp` と、コメント1行）。

```hlsl
// 3x3のテント型フィルタ。1回のパスで滑らかにぼかす
// 画面の端では外側を読むので、CLAMP のサンプラーで読む（WRAP だと反対の端の光が混ざる）
float32_t3 Tent(uint32_t index, float32_t2 uv, float32_t2 texel)
{
    float32_t3 sum = float32_t3(0, 0, 0);
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(-texel.x, -texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(0.0f, -texel.y)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(texel.x, -texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(-texel.x, 0.0f)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv).rgb * 4.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(texel.x, 0.0f)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(-texel.x, texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(0.0f, texel.y)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSamplerClamp, uv + float32_t2(texel.x, texel.y)).rgb * 1.0f;
    return sum * (1.0f / 16.0f);
}
```

### A-4 残りのブルームのシェーダー（5つ）
どれも **ファイルの中の `gSampler` を全部 `gSamplerClamp` に置き換える**だけ（Ctrl+H の「ファイル内」で「単語単位」をオン）。

| ファイル | 置き換える所 |
|---|---|
| `Post/BloomBright.PS.shader` | `main` の1行目（`color = min(max(...Sample(gSampler, ...`）の1か所 |
| `Post/BloomUp.PS.shader` | `previous = ...Sample(gSampler, ...` の1か所 |
| `Post/BloomGaussian.PS.shader` | `sum = ...` と、for の中の2行の、計3か所 |
| `Post/BloomComposite.PS.shader` | `scene` と `bloom` の2か所 |
| `Post/BloomKawaseComposite.PS.shader` | `scene` と、for の中の `bloom +=` の2か所 |

`BloomDown.PS.shader` は `Tent` しか使っていないので、そのままでよい（A-3 で直る）。

置き換えた後、`Shader/Data/Post` で `gSampler,`（後ろにカンマ）を検索して、**ブルームの7ファイルに1つも残っていない**ことを確かめる
（残っているのは DOF・Lens だけ。そちらはパートB）。

### A-5 Release 用の写し
`AL4_01/project/MyEngine/Shader` の写しの方にも A-2〜A-4 を入れる（`Texture.hlsli` とブルームの 7 ファイル、計 8 ファイルを、サブモジュールの方から上書きコピーでもよい）。

---

## 3. パートB（やってもよい）：DOF とレンズも CLAMP で読む

同じ「画面の絵を周りまで読む」処理なので、同じ種類の混ざりが起きる（ブルームほど目立たない）。

| ファイル | 置き換える所 | 起きていること |
|---|---|---|
| `Post/DepthOfFieldBlur.CS.shader` | `neighbor = ...SampleLevel(gSampler, ...` の1か所 | 画面の端のボケに、反対の端の色が混ざる |
| `Post/DepthOfFieldComposite.PS.shader` | `blur = ...SampleLevel(gSampler, ...` の1か所 | 端の1画素ぶん、反対の端のボケが混ざる |
| `Post/Lens.PS.shader` | `color.r`・`color.g`・`color.b` の3行と、放射ブラーの `sum +=` の、計4か所 | 色収差・放射ブラーで端を読むと、反対の端が半分混ざる（1.3 の理由） |

- DOF・Lens もルートシグネチャはそれぞれ1つのシェーダーから作るので、**そのファイルの中は全部そろえる**こと（1.4 の注意と同じ）。
- `Lens.PS` の `saturate(...)` は残してよい（CLAMP なら要らないが、あっても同じ結果）。
- Release 用の写しにも入れる。

---

## 4. 確かめ方
1. 光る泡（Intensity を上げた物）が、画面の上の端に半分かかる位置にカメラを置く。
   - 直す前：画面の**下の端**の同じ横位置がぼんやり光る。
   - 直した後：下の端は光らない。泡の周りの光は今まで通り。
2. 光る物を画面の右の端にかけて、**左の端**が光らないこと。
3. Settings → PostEffect → Bloom の Mode を Dual・Kawase の両方にして 1・2 を見る（どちらも同じサンプラーを使うので両方直る）。
4. モデルのテクスチャ（地面の岩・木）がタイル状に並ぶ物は、今まで通り繰り返して貼られていること（`gSampler` は WRAP のまま）。
5. Release でも 1 を見る（A-5 を忘れると Release だけ直らない）。

確かめていないこと：このファイルのコードはビルド・実行していない（原因はコードと画像から特定した：画面の上の端の泡の真下・右の端の緑の泡の真横に光が出ていて、WRAP の折り返しと位置が合う）。
写した後、Debug でビルドして上の 1〜5 を見てほしい。PSO 作成の assert が出たら、A-4 の置き換え漏れ（`gSampler` が残っている）を疑う。

---

## 5. 決めたこと
- 読み方（WRAP／CLAMP）は**変数名の規約**で決めた。reflection でサンプラーの設定は取れず、今のフィルタも規約で決めているのに合わせた。
  サンプラーの種類が増えたら（点で読む `gSamplerPoint` など）、`MakeStaticSampler` の分け方を名前→設定の表（`std::unordered_map`）にする。
- BORDER（外は黒）ではなく CLAMP にした。BORDER だと画面の端に近い光ほどブルームが暗く欠ける。CLAMP は端の光がそのまま外まで続いている扱いになり、端で切れても自然に見える。
