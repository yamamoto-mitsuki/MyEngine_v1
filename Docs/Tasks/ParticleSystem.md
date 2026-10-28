# E：Particle System（パーティクルを Component で置いて、エディタで調整する）

参考：
- Unity の Particle System（Main・Emission・Shape・Over Lifetime の分け方を手本にした）：https://docs.unity3d.com/Manual/PartSysMainModule.html ・ https://docs.unity3d.com/Manual/PartSysShapeModule.html
- 円の中に均等に点を取る（半径は乱数の平方根）：https://mathworld.wolfram.com/DiskPointPicking.html
- 球の中に均等に点を取る（半径は乱数の3乗根）：https://mathworld.wolfram.com/BallPointPicking.html
- イージングの形の一覧：https://easings.net/ja
- cpprefjp `std::span`（配列の一部を、写さずに「ここからここまで」と指す）：https://cpprefjp.github.io/reference/span/span.html
- cpprefjp `std::erase_if`（条件に合う要素を全部消す）：https://cpprefjp.github.io/reference/vector/vector/erase_if_free.html
- cpprefjp `std::ranges::find`（射影：メンバを指定して探す）：https://cpprefjp.github.io/reference/algorithm/ranges_find.html
- cpprefjp `std::cbrt`（3乗根）：https://cpprefjp.github.io/reference/cmath/cbrt.html
- cpprefjp `std::fmod`（小数の割り算の余り）：https://cpprefjp.github.io/reference/cmath/fmod.html

## このファイルについて
- AL4_01 の設計 [ParticleTitleClear.md](../../../AL4_01/Docs/ParticleTitleClear.md)（`AL4_01/Docs/ParticleTitleClear.md`）の **2章・パート E**。[AL4_01Game.md](AL4_01Game.md) の 13章の Step **E**。
- ユーザーの頼み：パーティクルを Component にして、エディタで置いて・調整できるようにする（Effects → Particle System）。設定は Component、粒は System が持つ（Trail と同じ作り）。ゲームからは `Play / Stop(clear) / Emit(count) / IsAlive`。停止中に Inspector で調整できる Preview。ついでに `ParticleManager::CreateGroup` がシーンを読み直すたびに増える件を直す。
- **写す場所**：`AL4_01/MyEngine_v1`（サブモジュール）で写す → エンジンでコミット・push → `source/repos/MyEngine_v1` で pull（[EngineSubmodule.md](EngineSubmodule.md) の6章）。
- **ゲーム（AL4_01）のコードは1行も変わらない**（エンジンに足すだけ。前に保存したシーン・Prefab もそのまま読める）。
- 確かめた環境：AL4_01 の develop（4282746）を一時フォルダに clone ＋ エンジン develop（c674d9b）＋ このファイルのエンジン。Debug・Release でビルドし、Release とエディタの両方で画面を撮って確かめた（7章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Graphics/Pipeline/ShaderConstants.h`・`Shader/Data/Particle/Particle.PS.shader`・`Graphics/Renderer/Renderer.h/.cpp`・`DrawRequest.h`・`RenderQueue.h/.cpp`・`SceneRenderer.cpp`・`Math/Color.h` | 粒の配列をそのまま描く `Renderer::DrawParticleInstances`・丸いぼかし（テクスチャ無しのとき）・霧を混ぜるか・被写界深度の前に描くか |
| B | `Components/ParticleSystemComponent.h`・`Systems/ParticleSystem.h/.cpp`（新しく作る） | 設定（Component）と、出す・動かす・描く（System）・Inspector・Preview |
| C | `Engine.cpp`・`Window/WindowManager.cpp`・`ECS/GameComponent.h`・`Editor/Windows/HierarchyWindow.cpp`・`MyEngine_v1.vcxproj` | つなぐ（Initialize・Update・Draw・Release・Hierarchy の ＋ → Effects） |
| D | `Particle/ParticleManager.h`・`Scene/SceneManager.cpp` | 前からある ParticleManager のグループが、シーンを入れ替えるたびに増える件 |

- A・D はそれぞれだけでビルドが通る。B は C の vcxproj の行を足すまでコンパイルされない（B → C の順で写して、C の後にビルド）。

> **10/25 追記：Converge（中心へ集まる）を足した。** `ParticleSystemComponent.h` まで写し終わっていたら、次の2行を「Over Lifetime」の `ease` の下に足す（3章の全文はもう足した形）。`ParticleSystem.cpp` はこれから写すので、3章の全文のままでよい（`LiveParticle` の2行・`EmitParticles` の2行・`UpdateEmitter` の `if (component.converge)`・Inspector の2行が増えている）。
> ```cpp
> 	bool converge = false;                         // 中心へ集まる（出た所から Entity の位置へ動き、寿命の終わりにちょうど着く。Speed・Gravity・Drag は使わない）
> 	Ease::Type convergeEase = Ease::Type::InQuad;  // 集まり方（InQuad：だんだん速く吸い込まれる）
> ```

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 形：Trail と同じ（設定は Component、粒は System）
```
Entity「Burst」
 ├ Transform                 … 出る場所の真ん中・向き（Shape は Entity の向きで回る）
 └ Particle System（Component）… どう出すかの設定だけ（memcpy できる型。保存・Undo・Inspector は今の仕組みのまま）

ParticleSystem（System。ParticleSystem.cpp の中だけ）
 └ emitters：Entity ごとの粒の列（std::vector）と、再生中か・今の秒・端数 … 実行中だけ。保存しない
```
- Component に `std::vector` は入れられない（バイトのまま写すので）。なので **粒は System が持つ**。Trail の点の列・AudioSource と鳴っている音と同じ関係。
- **1つの Entity に Particle System は1つ**。「キラキラ」と「飛び散る」の2つが要る物は、子を2つ作る（6章）。
- 粒の型は新しく小さく作った（`LiveParticle`：位置・速さ・初めの色・初めの大きさ・年・寿命）。前の `Particle`（ParticleManager）は行列や Transform を持っていて大きいので使わない。

### 1.2 1フレームの流れ（`ParticleSystem::Update`）
```
Play / Stop が切り替わった？ → 粒を全部捨てる（Preview の粒をゲームに持ち込まない）
停止中？ → Preview の1つだけ動かして終わり（1.4）
1. 消えた Entity・外した Component の粒を捨てる（erase_if）
2. Particle System を持つ Entity ごとに（ForEach）：
     無効（Enabled を外した・親が無効）→ 何もしない（粒は止まったまま残る）
     初めて見つけた → Play On Start なら出し始める
     ① 粒を動かす：age += dt、velocity += gravity × dt、velocity *= (1 − drag × dt)、position += velocity × dt
     ② 寿命の粒を消す（erase_if。並びはそのまま）
     ③ 出す：始めに Burst Count 個 ／ 毎フレーム「Rate × dt」を端数ごと足して、整数の分だけ
     ④ Duration 秒たった：Looping なら次の1回へ（Burst もまた出る）。でなければ出すのをやめる
```
- **端数の持ち越し**（`rateCarry`）：Rate 10 で 60fps なら、1フレームに 0.166… 個。そのまま整数にすると 0 個のまま。端数を次のフレームへ持ち越して、1 を越えたフレームに1個出す。フレームの速さが変わっても、1秒に出る数が変わらない。
- **出す・動かす時間**は `Time::GetDeltaTime`（スロー・ヒットストップ・ポーズで遅くなる・止まる）。ポーズ画面の後ろで止めたくない物は、ポーズしない UI Screen にする（クリア画面の Pause Game を外すのと同じ）。

### 1.3 出る場所と向き（Shape）
| Shape | 出る場所 | 飛ぶ向き（Speed の向き） |
|---|---|---|
| Point | Entity の位置 | 全部の向き（ランダム） |
| Sphere | 半径 Radius の球の中（From Surface なら表面） | 真ん中から外へ |
| Circle | **XY の面**の円の中（From Surface なら輪）。横から見るゲームで画面と同じ向き | 真ん中から外へ |
| Cone | XZ の面の円（半径 Radius）の中 | 上（+Y）へ。縁から出た粒ほど外へ傾く（縁で Cone Angle） |
| Box | Box Size の箱の中（From Surface なら表面） | 箱の真ん中から外へ |

- どれも **Entity の向き・大きさで回る・伸びる**（Rotation の Z で Circle を傾ける、X で Cone を横へ向ける）。
- **均等に散らす**：真ん中からの距離を「0〜1 の乱数」のまま使うと、真ん中に集まって見える（円の面積は半径の2乗、球の体積は半径の3乗に比例するので、外側ほど広い）。円は乱数の**平方根**、球は**3乗根**（`std::cbrt`）にすると、どこも同じ濃さになる。
- **Cone**：傾きを「真ん中からの距離 × Cone Angle」にした。粒どうしが交差せず、ラッパの形に広がる。Radius 0 でも、距離の乱数で傾きが決まるので広がる（設計では「開き角の中でランダム」だったのを変えた。8章）。

### 1.4 停止中の Preview
- 今までの作りでは、粒は再生中しか動かない（停止中は Trail も粒を持たない）。それだと Inspector で値を変えても、Play を押さないと見られない。
- Inspector の **▶ Preview**：押した Particle System だけを、停止中も**止めない時間**（`GetUnscaleDeltaTime`）で流す。ボタンは **■ Stop Preview** に変わる。
  - **選ぶ物を変えたら止まる**（Hierarchy で別の物を選んだ・選ぶのをやめた）。1つだけ持つ。
  - Looping を外した物は、出し終わって粒が全部消えたら止まる（ボタンが ▶ Preview に戻る）。
  - Play を押したら Preview の粒は捨てて、ゲームの粒になる（Play On Start もまた最初から見る）。
- 再生中に ▶ Preview を押すと、その Particle System を**最初からもう一度**流す（`ParticleSystem::Play` と同じ。Burst の見た目を何回も見るのに使う）。
- Inspector の Preview ボタンは、どの Entity の物かを知らない（Describe の関数は Component の値しかもらわない）。Inspector は **Component の置き場所そのもの**を渡してくるので、`ForEach` でアドレスが同じ物を探して、持ち主の Entity を見つける（`FindOwner`）。

### 1.5 描く（`Renderer::DrawParticleInstances`）
- **ビルボードは今までどおり VS でやる**（`Particle.VS` が `gCamera.right / up` で四角を組む）。だから Scene ビューでもデバッグカメラの方を向く。System は粒の「中心」と「大きさ」だけを行列に入れる（回転は入れない）。
- Entity 1つ = 1回のインスタンス描画。粒の配列を `std::span`（配列を写さずに指す物）で渡す。
- 前からある `DrawParticle`（`std::list` の粒）は、中で配列に並べ直して `DrawParticleInstances` を呼ぶようにした（同じ処理を2か所に書かない）。
- **Texture が空なら丸いぼかし**：白い四角だと粒に見えないので、PS で板の真ん中からの距離で透明にする（`isSoftCircle`）。絵の素材が無くても、置いてすぐ粒に見える。色は Color で付ける。
- **霧（Use Fog）**：今まではいつも霧を混ぜていた。外すと霧の中でもそのままの色（光る粒をくっきり見せる）。PS の空き（`padB`）に印を入れて、`ApplyFog` を呼ぶかを分けるだけ。
- **被写界深度（Use Depth Of Field）**：今までのパーティクルは被写界深度の**後**に描くので、どこに置いても**ぼけない**（置く距離を変えても同じ）。印を付けた粒だけ、被写界深度の**前**（不透明な物と空の後）に描いて、一緒にぼかす。
  - 粒は深度を書かないので、**ぼけ方は粒の後ろにある物の距離で決まる**（DOF は深度の絵から、ピクセルごとにぼかす量を決めるので）。奥の層（背景の板の近く）に置く葉・湯気は正しくぼける。ピントの合う所に置いた粒の後ろが遠い背景だと、背景と同じだけぼけてしまうので、そういう粒には付けない。
  - 粒ごとに自分の距離でぼかすには、粒にも深度を書く（半透明の縁が欠ける）か、粒ごとにぼかす（重い）必要がある。今はやらない（8章）。

![Use Fog・Use Depth Of Field（テスト用に霧と DOF を強めたシーン。左上の葉は DOF あり＝壁と一緒にぼける・右上は無し。左のオレンジは霧あり＝霧の色に寄る・右は無し）](images/particle_fog_dof.png)
- **Normal の重ね方は奥から順には並べない**（このゲームの光る粒は Add だけでよい）。粒は「古い方 → 新しい方」の順で描くので、新しい粒が手前になる。消すときも並びを変えない（`std::erase_if`）ので、前後が毎フレーム入れ替わってちらつくことはない。

### 1.6 設計（ParticleTitleClear.md の 2章）から変えた所
| 設計 | このファイル | 理由 |
|---|---|---|
| `bool glow`（グロー層にも描く） | **入れなかった** | G2（ゴールだけ光らせる）を「グロー層」にするか「HDR」にするか、まだ決めていない。HDR なら `glow` の代わりに `Intensity`（色の強さ）を足す。どちらでも、G2 で足せば前のシーンはそのまま読める |
| Texture 0 は白い四角 | **丸いぼかし**（PS で作る） | 白い四角は粒に見えない。素材無しで置いてすぐ使える |
| 寿命の粒は「後ろと入れ替えて pop」 | `std::erase_if`（並びを保つ） | 入れ替えると描く順が毎フレーム変わり、Normal で重ねたときにちらつく。どちらも1回なめるだけなので、速さは変わらない |
| Cone は「+Y を中心に開き角の中」でランダム | 縁から出た粒ほど外へ傾ける | 交差せず、ラッパの形に広がる（Unity の Cone と同じ見た目） |
| 上限は `ParticleManager::kMaxParticles` | `ParticleSystem::kMaxParticles`（4096） | 新しい System は ParticleManager を使わない |
| （書いていなかった） | Enabled を外す・親が無効 → 止まる・描かない（粒は残る） | Trail の Enabled と同じ |
| （書いていなかった） | 再生中の ▶ Preview は「最初からもう一度」 | Burst を何回も見られる |
| （書いていなかった） | **Use Fog・Use Depth Of Field** を足した（初めは霧あり・DOF なし＝今までと同じ） | ユーザーの頼み（10/25）。奥の葉・湯気はぼかしたい、光る粒は霧で薄めたくない |
| `ClearParticles` でグループごと消すか、`DestroyGroup` を足す | `ClearGroups` にして、シーンを入れ替えるときに呼ぶ（パートD） | エミッターはシーンが持つので、シーンを捨てた後なら全部捨ててよい。`DestroyGroup` は、エミッターのコピー・ムーブまで直す必要があって大きい |

---

## 2. パートA：粒の配列をそのまま描く・丸いぼかし

### `Graphics/Pipeline/ShaderConstants.h`
`MaterialParticleData` に1つ足す（PS の `Material` と同じ並びにする）：
```cpp
// パーティクルのマテリアル（Particle/Particle.PS の Material と同じ並び）
struct MaterialParticleData {
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	Matrix4x4 uvTransform = MakeIdentity4x4();
	uint32_t textureIndex = 0;
	uint32_t isSoftCircle = 0; // 1：テクスチャの代わりに、丸いぼかし（真ん中が濃く、縁へ向かって透明）
	uint32_t useFog = 1;       // 1：霧を混ぜる（0：霧の中でも、そのままの色）
};
```

### `Shader/Data/Particle/Particle.PS.shader`
`Material` の `padA`・`padB` を `isSoftCircle`・`useFog` に変えて、`main` の頭（テクスチャを読む所）を分ける：
```hlsl
struct Material
{
    float32_t4 color;
    float4x4 uvTransform;
    uint32_t textureIndex;
    uint32_t isSoftCircle; // 1：テクスチャの代わりに丸いぼかし（Particle System の Texture が空のとき）
    uint32_t useFog;       // 1：霧を混ぜる（0：霧の中でも、そのままの色）
    float padC;
};
ConstantBuffer<Material> gMaterial : register(b20);


PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 texColor;
    if (gMaterial.isSoftCircle != 0) {
        // 丸いぼかし：板の真ん中からの距離 d（真ん中 0・辺の真ん中 1）で、真ん中 1 → 縁 0 へ薄くする。2乗で芯を濃く
        float32_t2 d = input.texcoord * 2.0f - 1.0f;
        float fade = saturate(1.0f - dot(d, d));
        texColor = float32_t4(1.0f, 1.0f, 1.0f, fade * fade);
    } else {
        float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
        texColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    }
    if (texColor.a == 0.0) { discard; }
```
その少し下、霧を混ぜる行を `if` で囲む：
```hlsl
    if (gMaterial.useFog != 0) {
        finalColor = ApplyFog(finalColor, input.worldPosition); // 霧
    }
```
- `dot(d, d)` は「真ん中からの距離の2乗」。四隅は 2 になるので `saturate` で 0 に切る（板の角は描かない＝丸く見える）。
- **Release の exe は `AL4_01/project/MyEngine/Shader` の写しを読む**。写しは、エンジン（MyEngine_v1.vcxproj）をビルドし直したときの PostBuildEvent（xcopy）で自動で新しくなる。このパートは C++ も変えるのでエンジンが必ずビルドし直される。**.shader だけを直したときは写らない**（lib が作り直されないので PostBuildEvent が走らない）ので、そのときはエンジンを「リビルド」する。

### `Graphics/Renderer/Renderer.h`
include に `<span>` を足す：
```cpp
#include <memory>
#include <list>
#include <span>
#include <vector>
```
`ParticleConfig` の下に、新しい設定を足す：
```cpp
	// --- 粒の配列をそのまま描く設定（ParticleSystem が使う。ParticleConfig も中でこれに直して描く）---
	// 粒は VS でカメラの方へ向けるので、ここで向きは計算しない（world から読むのは、中心と大きさだけ）
	struct ParticleInstancesConfig : DrawTarget {
		std::span<const ParticleData> instances;  // 粒ごとの行列と色（色は sRGB の見た目の値。中でリニアに直す）。DrawParticleInstances の中で写すので、呼んだ後は消してよい
		uint32_t textureHandle = 0;               // 粒の絵（0 なら白）
		bool isSoftCircle = false;                // 絵の代わりに丸いぼかしで描く（textureHandle は使わない）
		bool useFog = true;                       // 霧（Fog）を混ぜる
		bool useDepthOfField = false;             // 被写界深度でぼかす（DOF の前に描く。ぼけ方は粒の後ろにある物の距離で決まる）
		Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 全部の粒に掛ける色（sRGB）
		Transform uvTransform;                    // UVの拡縮、回転、移動
		BlendMode blendMode = BlendMode::Add;     // 重ね方（1回の描画で1つなので、粒ごとには変えられない）
	};
```
`DrawParticle` の宣言の下に：
```cpp
	/// <summary>
	/// 粒の配列を、そのまま1回のインスタンス描画で描くリクエストを追加する（空なら何もしない）
	/// </summary>
	static void DrawParticleInstances(const ParticleInstancesConfig& config);
```
- `std::span<const ParticleData>`：配列（`std::vector` など）の「先頭と個数」だけを持つ物。写さないので軽い。**指している配列が先に消えると壊れる**ので、受け取った関数の中だけで使う（ここではリクエストへ写してから返す）。

### `Graphics/Renderer/Renderer.cpp`
`DrawParticle` の `MY_ASSERT_MSG(config.camera ...)` から下を、丸ごと置き換える（`DrawParticleInstances` も足す）：
```cpp
	MY_ASSERT_MSG(config.camera != nullptr, "パーティクルにはカメラを設定してください");

	// list の粒を配列に並べ直して、配列の方で描く（描く処理を2か所に書かない）
	std::vector<ParticleData> instances;
	instances.reserve(config.particles->size());
	for (const Particle& p : *config.particles) {
		instances.push_back({p.world, p.color}); // フェード済みの色（sRGB のまま。DrawParticleInstances がリニアに直す）
	}
	ParticleInstancesConfig instancesConfig;
	static_cast<DrawTarget&>(instancesConfig) = config; // 描き分け（ウィンドウ・層・目で隠したか）をまとめて写す
	instancesConfig.instances = instances;
	instancesConfig.textureHandle = config.textureHandle;
	instancesConfig.color = config.color;
	instancesConfig.uvTransform = config.uvTransform;
	instancesConfig.blendMode = config.blendMode;
	DrawParticleInstances(instancesConfig);
}

void Renderer::DrawParticleInstances(const ParticleInstancesConfig& config) {
	if (config.instances.empty()) {
		return;
	}
	// ConstantBuffer,StructuredBufferに送る情報を作成（リクエストが持つので、ここで写す）
	ParticleRequest req;
	req.instances.reserve(config.instances.size());
	for (const ParticleData& instance : config.instances) {
		req.instances.push_back({instance.world, SrgbToLinear(instance.color)}); // 色は sRGB → リニア（A はそのまま）
	}
	// 全部の粒で同じマテリアル
	req.materialData.color = SrgbToLinear(config.color);
	req.materialData.uvTransform = MakeUVTransformMatrix(config.uvTransform);
	req.materialData.textureIndex = ResolveTextureIndex(config.textureHandle);
	req.materialData.isSoftCircle = config.isSoftCircle ? 1u : 0u;
	req.materialData.useFog = config.useFog ? 1u : 0u;
	req.blendMode = config.blendMode;
	req.isBeforeDepthOfField = config.useDepthOfField;
	static_cast<DrawTarget&>(req) = config; // 描き分け（ウィンドウ・層・目で隠したか）をまとめて写す（DrawTarget の所だけ代入する）
	RenderQueue::Request(std::move(req));
}
```

### `Graphics/Renderer/DrawRequest.h`
`ParticleRequest` に1つ足す（`blendMode` の下）：
```cpp
	bool isBeforeDepthOfField = false; // true：被写界深度の前に描く（不透明な物と一緒にぼける）。false：後に描く（ぼけない）
```

### `Graphics/Renderer/RenderQueue.h`
`FlushParticle` に引数を1つ足す：
```cpp
	// isBeforeDepthOfField：true なら被写界深度の前に描く粒（Use Depth Of Field）だけ、false なら後に描く粒だけ
	static void FlushParticle(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB, bool isBeforeDepthOfField);
```

### `Graphics/Renderer/RenderQueue.cpp`
`FlushParticle` の頭を合わせて、ループの中で「今描く方」だけにする：
```cpp
void RenderQueue::FlushParticle(const std::wstring& windowTitle, D3D12_GPU_VIRTUAL_ADDRESS cameraCB, bool isBeforeDepthOfField) {
```
```cpp
		if (!IsDrawnInView(req, windowTitle)) {
			continue;
		}
		// 被写界深度の前・後のうち、今描く方だけ（前の粒は SceneRenderer::RenderOpaque、後の粒は RenderTransparent で描く）
		if (req.isBeforeDepthOfField != isBeforeDepthOfField) {
			continue;
		}
```

### `Graphics/Renderer/SceneRenderer.cpp`
`RenderOpaque` の最後（空の後）に、DOF の前に描く粒を足す：
```cpp
	if (instance_->skybox_) {
		GPU_SCOPE(cmdList, "Skybox");
		instance_->skybox_->Draw(camera, camCB);
	}
	// Use Depth Of Field の粒は、ここ（被写界深度の前）で描いて一緒にぼかす。深度は書かないので、ぼけ方は粒の後ろにある物の距離で決まる
	{
		GPU_SCOPE(cmdList, "Particle (Before DOF)");
		RenderQueue::FlushParticle(windowTitle, camCB, true);
	}
}
```
`RenderTransparent` の `FlushParticle` に引数を足す：
```cpp
		RenderQueue::FlushParticle(windowTitle, camCB, false); // ふつうの粒（被写界深度の後。ぼけない）
```
- DOF が無いビュー（DOF を切った・ポスト無し）でも、`RenderOpaque` → `RenderTransparent` の順に両方通るので、粒は全部描かれる（描く順が少し前になるだけ）。

### `Math/Color.h`
`MultiplyAlpha` の下に足す：
```cpp
// 色と色を掛ける（RGBA をそれぞれ掛ける。白 (1,1,1,1) を掛けても変わらない。パーティクルの「初めの色 × 最後の色」）
inline Vector4 MultiplyColor(const Vector4& a, const Vector4& b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
```

---

## 3. パートB：Component と System

### `Components/ParticleSystemComponent.h`（新しく作る）
```cpp
#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Math/Easing.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"

// パーティクル（小さな板をたくさん出して、動かして消す）。Entityに付けるComponent（カテゴリ：Effects）
// 粒の列は ParticleSystem が持つ。Component に可変長の配列は入れられない
// ここは「どう出すか」の設定だけ（Trail と点の列の関係と同じ）。Unity の Particle System の Main・Emission・Shape・Over Lifetime の小さい版

/// <summary>
/// 粒が出る場所と、飛ぶ向き（向きは Speed の向き）
/// </summary>
enum class ParticleShape {
	Point,  // Entity の位置から、全部の向きへ
	Sphere, // 球の中（From Surface なら表面）から、外へ
	Circle, // XY の面の円の中（From Surface なら輪）から、外へ（横から見るゲームで、輪から飛び散る）
	Cone,   // XZ の面の円から、上（+Y）を中心に Cone Angle まで開いて
	Box,    // 箱の中（From Surface なら表面）から、外へ
};

/// <summary>
/// 重ね方
/// </summary>
enum class ParticleBlend {
	Add,    // 足す（光る粒。重なるほど明るい）
	Normal, // 普通（奥から順には並べないので、重なると縁が汚くなることがある）
};

/// <summary>
/// 出た粒が、Entity に付いていくか
/// </summary>
enum class ParticleSpace {
	World, // 出た所に残る（Entity が動いても粒は付いていかない。煙・火花）
	Local, // Entity と一緒に動く・回る（動く物にまとわりつくキラキラ）
};


/// <summary>
/// パーティクルの設定
/// <para>Min〜Max の項目（Vector2）は、x が小さい方・y が大きい方。逆に入れても、その間で選ぶ</para>
/// </summary>
struct ParticleSystemComponent {
	bool enabled = true; // false の間は出さない・動かさない・描かない（粒は消さずに残す）

	// ===== 再生 =====
	bool playOnStart = true; // Play で流し始める（false：ゲームが ParticleSystem::Play を呼ぶまで出さない。使ったら飛び散る用）
	bool looping = true;     // Duration 秒ごとにくり返す（Burst もまた出る）。false：1回で出すのをやめる
	float duration = 1.0f;   // 1回の長さ（秒）

	// ===== 出す量 =====
	float rateOverTime = 10.0f; // 1秒に出す数（0 で Burst だけ）
	int burstCount = 0;         // 1回の始めに、まとめて出す数
	int maxParticles = 256;     // 同時に出ている数の上限（ParticleSystem::kMaxParticles まで）。いっぱいの間は出さない

	// ===== 出る場所と向き（Shape）=====
	ParticleShape shape = ParticleShape::Sphere;
	float radius = 0.5f;                  // Sphere・Circle・Cone の半径（m）
	float coneAngle = 25.0f;              // Cone の開き（度。真上から何度まで）
	Vector3 boxSize = {1.0f, 1.0f, 1.0f}; // Box の大きさ（m）
	bool fromSurface = false;             // 形の表面（縁）から出す（円の輪から飛び散る）

	// ===== 粒の初めの値（Min〜Max の乱数）=====
	Vector2 lifetime = {0.5f, 1.0f};          // 消えるまでの秒
	Vector2 speed = {1.0f, 2.0f};             // Shape の向きへの速さ（m/s）
	Vector2 size = {0.1f, 0.2f};              // 大きさ（m。板の1辺）
	Vector4 colorA = {1.0f, 1.0f, 1.0f, 1.0f}; // この2色の間でランダム（同じ色にすれば1色）
	Vector4 colorB = {1.0f, 1.0f, 1.0f, 1.0f};

	// ===== 生きている間（Over Lifetime）=====
	Vector3 gravity = {0.0f, 0.0f, 0.0f};          // 加速（m/s²。下へ落とすなら -Y、上へ昇らせるなら +Y）
	float drag = 0.0f;                             // 1秒あたりの減速の割合（0 で減らない。大きいほどすぐ止まる火花）
	float endSizeRate = 1.0f;                      // 最後の大きさ（初めの大きさに掛ける。0 で消えるように縮む）
	Vector4 endColor = {1.0f, 1.0f, 1.0f, 0.0f};   // 最後の色（初めの色に掛ける。A 0 でフェードアウト）
	Ease::Type ease = Ease::Type::Linear;          // 大きさと色の変わり方
	bool converge = false;                         // 中心へ集まる（出た所から Entity の位置へ動き、寿命の終わりにちょうど着く。Speed・Gravity・Drag は使わない）
	Ease::Type convergeEase = Ease::Type::InQuad;  // 集まり方（InQuad：だんだん速く吸い込まれる）

	// ===== 見た目 =====
	uint32_t textureHandle = 0;                // 粒の絵。0 なら丸いぼかし（白。色は Color で付ける）
	ParticleBlend blend = ParticleBlend::Add;  // 重ね方
	ParticleSpace space = ParticleSpace::World; // 粒が Entity に付いていくか
	bool useFog = true;                         // 霧（Fog）を混ぜる（外すと、霧の中でもそのままの色。光る粒をくっきり見せる）
	bool useDepthOfField = false;               // 被写界深度でぼかす（奥の層の葉・湯気など。ぼけ方は粒の後ろにある物の距離で決まる）
};

static_assert(std::is_trivially_copyable_v<ParticleSystemComponent>);
```

### `Systems/ParticleSystem.h`（新しく作る）
```cpp
#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Components/ParticleSystemComponent.h"


/// <summary>
/// ParticleSystemComponent を持つEntityの粒を出し・動かし・描く
/// <para>粒は実行中だけの状態（保存しない。Play / Stop で消える。停止中は持たない。ただし Inspector の Preview 中の1つだけは動かす）</para>
/// <para>一覧などの中身は ParticleSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class ParticleSystem {
public:
	static constexpr int kMaxParticles = 4096; // 1つの Particle System が同時に出せる数の上限（Max Particles の上限）

	// ParticleSystemComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::FlushDestroy の後に呼ぶ（ワールド行列も計算済み）
	/// <para>消えた・外した Particle System の粒を捨て、出して、動かして、寿命の粒を消す</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false＝Preview 中の1つ以外の粒を全部捨てる）</param>
	/// <param name="deltaTime">経過秒（Time::GetDeltaTime。スロー・一時停止が掛かった物）</param>
	static void Update(bool isGameRunning, float deltaTime);

	// 粒の描画リクエストを積む（WindowManager::DrawAll から。粒はどのビューでもカメラの方を向く）
	static void Draw();

	// ===== ゲームから（Script・SYSTEM の中で）使う =====

	// 最初から出し始める（Burst も出る）。出している途中で呼んでも最初から。Play On Start を外した物は、これを呼ぶまで出さない
	static void Play(Handle<Entity> entity);
	// 出すのをやめる。clear：出ている粒も消す（false なら、出ている粒は寿命まで残る）
	static void Stop(Handle<Entity> entity, bool clear = false);
	// 今すぐ count 個出す（Play していなくても出る。Component の値で出す）。Component が付く前（RequestAdd した同じフレーム）は何もしない
	static void Emit(Handle<Entity> entity, int count);
	// 出している途中か、粒が1つでも残っているか（Burst が全部消えたら false。消し終わってから Entity を消すときに）
	static bool IsAlive(Handle<Entity> entity);

	// ===== エディタ（Inspector の Preview ボタン）=====
	// 停止中にも、この1つだけ流す（止めない時間で。選んでいる物を変えたら止まる）。component は Inspector が描いている Component
	static void StartPreview(const ParticleSystemComponent& component);
	static void StopPreview();
	// component を Preview しているか
	static bool IsPreviewing(const ParticleSystemComponent& component);
};
```

### `Systems/ParticleSystem.cpp`（新しく作る）
```cpp
#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Math/Color.h"
#include "MyEngine/Math/RandomEngine.h"
#include "MyEngine/Time/EngineTime.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#endif

namespace {
constexpr float kMinDuration = 0.01f;           // Duration の下限（秒。0 だとくり返しが止まらないので）
constexpr float kMinLifetime = 0.01f;           // 寿命の下限（秒。0 で割らないように）
constexpr float kMinDirectionLengthSq = 1e-4f;  // これより短い向きは、向きが決まらないとみなす
constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;

/// <summary>
/// 生きている粒1つ（実行中だけ。保存しない）
/// </summary>
struct LiveParticle {
	Vector3 position;         // 位置（World ならワールド座標、Local なら Entity から見た座標）
	Vector3 velocity;         // 速さ（m/s。position と同じ座標で）
	Vector3 startPosition;    // 出た所（Converge のとき、ここから center へ動かす）
	Vector3 center;           // 集まる先（Converge。World なら出た瞬間の Entity の位置、Local なら Entity から見た原点）
	Vector4 startColor;       // 初めの色（sRGB）
	float startSize = 0.0f;   // 初めの大きさ（m）
	float age = 0.0f;         // 出てからの秒
	float lifetime = 1.0f;    // 消えるまでの秒
};

/// <summary>
/// Particle System 1つ分の、実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct EmitterState {
	Handle<Entity> entity;               // 持ち主
	std::vector<LiveParticle> particles; // 生きている粒（古い方 → 新しい方。Normal で重ねるとき、新しい粒が手前に描かれる）
	bool isStartChecked = false;         // Play On Start を見たか（最初に見つけたときに1回だけ。先に Play・Stop を呼ばれたら見ない）
	bool isPlaying = false;              // 出している途中か
	bool isBurstPending = false;         // 次に出すとき、Burst をまとめて出す（Play したとき・くり返しの頭）
	float time = 0.0f;                   // 今の1回（Duration）の中の秒
	float rateCarry = 0.0f;              // Rate Over Time の端数（整数にならなかった分を、次のフレームへ持ち越す）
};

/// <summary>
/// ParticleSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct ParticleSystemState {
	std::vector<EmitterState> emitters;  // 見つけた Particle System の一覧（数しか無いので、順に見れば足りる）
	std::vector<ParticleData> instances; // 描くときの粒の置き場（Draw の中だけで使う。配列のメモリを使い回す）
	Handle<Entity> previewEntity;        // 停止中にも流す1つ（Inspector の Preview。無効なら無し）
	bool wasGameRunning = false;         // 前のフレームでゲームが動いていたか（Play / Stop の切り替わりで、粒を全部捨てる）
};
std::unique_ptr<ParticleSystemState> state;

// Entityの状態を探す（無ければ nullptr）
EmitterState* FindEmitter(Handle<Entity> entity) {
	const auto it = std::ranges::find(state->emitters, entity, &EmitterState::entity);
	return (it != state->emitters.end()) ? &*it : nullptr;
}

// Entityの状態を探し、無ければ足す
EmitterState& FindOrAddEmitter(Handle<Entity> entity) {
	if (EmitterState* emitter = FindEmitter(entity)) {
		return *emitter;
	}
	state->emitters.push_back({});
	state->emitters.back().entity = entity;
	return state->emitters.back();
}

// Entity のワールド行列（Transform が無ければ単位行列）
Matrix4x4 WorldMatrixOf(Handle<Entity> entity) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	return (transform != nullptr) ? transform->worldMatrix : MakeIdentity4x4();
}

// 最初から出し始める（Burst は次に出すときにまとめて）
void Restart(EmitterState& emitter) {
	emitter.isStartChecked = true;
	emitter.isPlaying = true;
	emitter.isBurstPending = true;
	emitter.time = 0.0f;
	emitter.rateCarry = 0.0f;
}

//=============================================================================
// 乱数
//=============================================================================
// Min〜Max の乱数（x と y のどちらが大きくてもよい。同じなら、その値）
float RandomRange(const Vector2& range) {
	const float low = (std::min)(range.x, range.y);
	const float high = (std::max)(range.x, range.y);
	return (low < high) ? RandomEngine::GetFloat(low, high) : low;
}

// 長さ1のランダムな向き（球の中の点を長さ1に伸ばす。真ん中に近すぎて向きが決まらない点は選び直す）
Vector3 RandomDirection() {
	while (true) {
		const Vector3 point = RandomEngine::GetInsideUnitSphere();
		const float lengthSq = LengthSq(point);
		if (lengthSq > kMinDirectionLengthSq) {
			return point * (1.0f / std::sqrt(lengthSq));
		}
	}
}

//=============================================================================
// 出る場所と向き（Shape）
//=============================================================================
/// <summary>
/// 粒が出る場所と、飛ぶ向き（どちらも Entity から見た座標）
/// </summary>
struct SpawnPoint {
	Vector3 position;  // 出る場所
	Vector3 direction; // 飛ぶ向き（長さ1）
};

// 形の中（From Surface なら表面）から、出る場所と向きを1つ選ぶ
// 中に均等に散らすため、真ん中からの距離は「0〜1 の乱数」をそのまま使わず、球なら3乗根・円なら平方根にする
// （球の中の体積は半径の3乗、円の面積は半径の2乗に比例するので、そのままだと真ん中に集まって見える）
SpawnPoint SampleShape(const ParticleSystemComponent& component) {
	switch (component.shape) {
	case ParticleShape::Point:
		return {{0.0f, 0.0f, 0.0f}, RandomDirection()};

	case ParticleShape::Sphere: {
		const Vector3 direction = RandomDirection();
		const float distance = component.fromSurface ? 1.0f : std::cbrt(RandomEngine::GetFloat(0.0f, 1.0f));
		return {direction * (component.radius * distance), direction};
	}

	case ParticleShape::Circle: {
		// XY の面（横から見るゲームで、画面と同じ向きの円）
		const float angle = RandomEngine::GetFloat(0.0f, kTwoPi);
		const Vector3 direction = {std::cos(angle), std::sin(angle), 0.0f};
		const float distance = component.fromSurface ? 1.0f : std::sqrt(RandomEngine::GetFloat(0.0f, 1.0f));
		return {direction * (component.radius * distance), direction};
	}

	case ParticleShape::Cone: {
		// XZ の面の円から出て、上（+Y）へ。縁から出た粒ほど外へ傾ける（縁で Cone Angle。真ん中はまっすぐ上）
		// → 粒どうしが交差せず、ラッパの形に広がる。Radius 0 でも、傾きは乱数で決まるので広がる
		const float angle = RandomEngine::GetFloat(0.0f, kTwoPi);
		const Vector3 outward = {std::cos(angle), 0.0f, std::sin(angle)};
		const float distance = component.fromSurface ? 1.0f : std::sqrt(RandomEngine::GetFloat(0.0f, 1.0f));
		const float tilt = component.coneAngle * kDegToRad * distance;
		const Vector3 direction = outward * std::sin(tilt) + Vector3{0.0f, std::cos(tilt), 0.0f};
		return {outward * (component.radius * distance), direction};
	}

	case ParticleShape::Box: {
		const Vector3 half = {std::abs(component.boxSize.x) * 0.5f, std::abs(component.boxSize.y) * 0.5f, std::abs(component.boxSize.z) * 0.5f};
		Vector3 position = RandomEngine::GetVector3({-half.x, -half.y, -half.z}, half);
		if (component.fromSurface) {
			// 面を1つ選んで（広い面ほど選ばれやすく）、その軸だけ端へ寄せる
			const float areaX = half.y * half.z; // 左右の面の広さ（の 1/4。比べるだけなので、そろっていればよい）
			const float areaY = half.x * half.z; // 上下の面
			const float areaZ = half.x * half.y; // 前後の面
			const float pick = RandomEngine::GetFloat(0.0f, areaX + areaY + areaZ);
			const float side = (RandomEngine::GetInt(0, 1) == 0) ? -1.0f : 1.0f;
			if (pick < areaX) {
				position.x = half.x * side;
			} else if (pick < areaX + areaY) {
				position.y = half.y * side;
			} else {
				position.z = half.z * side;
			}
		}
		// 向きは箱の真ん中から外へ（ちょうど真ん中に出た粒は、どの向きでも）
		const Vector3 direction = (LengthSq(position) > kMinDirectionLengthSq) ? Normalize(position) : RandomDirection();
		return {position, direction};
	}
	}
	return {{0.0f, 0.0f, 0.0f}, RandomDirection()};
}

//=============================================================================
// 出す・動かす
//=============================================================================
// 粒を count 個出す（Max Particles を超える分は出さない）
void EmitParticles(EmitterState& emitter, const ParticleSystemComponent& component, const Matrix4x4& worldMatrix, int count) {
	const int limit = std::clamp(component.maxParticles, 1, ParticleSystem::kMaxParticles);
	const int room = limit - static_cast<int>(emitter.particles.size());
	const int spawnCount = (std::min)(count, room);
	for (int i = 0; i < spawnCount; ++i) {
		SpawnPoint point = SampleShape(component);
		// World：出た瞬間のワールド座標にする（この後 Entity が動いても付いていかない）
		if (component.space == ParticleSpace::World) {
			point.position = TransformPoint(point.position, worldMatrix);
			point.direction = Normalize(TransformNormal(point.direction, worldMatrix)); // 回転だけ効かせる（Scale で速さは変えない）
		}
		LiveParticle particle;
		particle.position = point.position;
		particle.startPosition = point.position;
		particle.center = (component.space == ParticleSpace::World) ? TransformPoint({0.0f, 0.0f, 0.0f}, worldMatrix) : Vector3{0.0f, 0.0f, 0.0f};
		particle.velocity = point.direction * RandomRange(component.speed);
		particle.startColor = Lerp(component.colorA, component.colorB, RandomEngine::GetFloat(0.0f, 1.0f));
		particle.startSize = RandomRange(component.size);
		particle.lifetime = (std::max)(RandomRange(component.lifetime), kMinLifetime);
		emitter.particles.push_back(particle);
	}
}

// 1フレーム進める（粒を動かして寿命の物を消し、再生中なら出す）
void UpdateEmitter(EmitterState& emitter, const ParticleSystemComponent& component, const Matrix4x4& worldMatrix, float deltaTime) {
	// ===== 1. 動かす =====
	// drag：1秒あたり drag の割合で遅くする（1フレームで 1 - drag×dt 倍。負にならないように 0 で止める）
	const float dragScale = (std::max)(0.0f, 1.0f - component.drag * deltaTime);
	for (LiveParticle& particle : emitter.particles) {
		particle.age += deltaTime;
		// --- Converge：速さは使わず、年の割合で「出た所 → 中心」の間に置く（寿命の終わりにちょうど中心。突き抜けない）---
		if (component.converge) {
			const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
			particle.position = Lerp(particle.startPosition, particle.center, Ease::Apply(t, component.convergeEase));
			continue;
		}
		particle.velocity += component.gravity * deltaTime;
		particle.velocity *= dragScale;
		particle.position += particle.velocity * deltaTime;
	}
	// ===== 2. 寿命の粒を消す（並びはそのまま。Normal で重ねたとき、前後が入れ替わってちらつかないように）=====
	std::erase_if(emitter.particles, [](const LiveParticle& particle) { return particle.age >= particle.lifetime; });

	// ===== 3. 出す =====
	if (!emitter.isPlaying) {
		return;
	}
	// --- Burst（1回の始めにまとめて）---
	if (emitter.isBurstPending) {
		emitter.isBurstPending = false;
		EmitParticles(emitter, component, worldMatrix, component.burstCount);
	}
	// --- Rate Over Time（端数を持ち越して、整数の分だけ。フレームの速さで出る数が変わらない）---
	emitter.rateCarry += (std::max)(0.0f, component.rateOverTime) * deltaTime;
	const float whole = std::floor(emitter.rateCarry);
	emitter.rateCarry -= whole;
	EmitParticles(emitter, component, worldMatrix, static_cast<int>((std::min)(whole, static_cast<float>(ParticleSystem::kMaxParticles))));

	// ===== 4. 1回の終わり =====
	emitter.time += deltaTime;
	const float duration = (std::max)(component.duration, kMinDuration);
	if (emitter.time >= duration) {
		if (component.looping) {
			emitter.time = std::fmod(emitter.time, duration); // 次の1回へ（Burst もまた出る）
			emitter.isBurstPending = true;
		} else {
			emitter.isPlaying = false; // 出すのをやめる（出ている粒は寿命まで残る）
		}
	}
}

// 描かない・動かさない物か（無効・親まで含めて無効）
bool IsSleeping(Handle<Entity> entity, const ParticleSystemComponent& component) { return !component.enabled || !EntityManager::IsActiveInHierarchy(entity); }

//=============================================================================
// 停止中の Preview
//=============================================================================
// Preview の1つだけを、止めない時間で動かす（ほかの粒は持たない）
void UpdatePreview() {
	Handle<Entity> preview = state->previewEntity;
#ifdef USE_IMGUI
	// 選んでいる物を変えたら止める（Inspector から Preview が見えなくなるので）
	if (HierarchyWindow::GetSelected() != preview) {
		preview = {};
	}
#endif
	std::erase_if(state->emitters, [preview](const EmitterState& emitter) { return emitter.entity != preview; });
	const ParticleSystemComponent* component = EntityManager::Get<ParticleSystemComponent>(preview);
	EmitterState* emitter = FindEmitter(preview);
	if (component == nullptr || emitter == nullptr) {
		ParticleSystem::StopPreview();
		return;
	}
	if (!IsSleeping(preview, *component)) {
		UpdateEmitter(*emitter, *component, WorldMatrixOf(preview), Time::GetUnscaleDeltaTime());
	}
	// 1回で終わる物は、出し終わって粒が全部消えたら Preview も終わる（ボタンが ▶ Preview に戻る）
	if (!emitter->isPlaying && emitter->particles.empty()) {
		ParticleSystem::StopPreview();
	}
}

// Inspector が描いている Component から、持ち主の Entity を探す（無ければ無効な Handle）
// Inspector は Component の置き場所そのものを渡してくるので、アドレスで比べる
Handle<Entity> FindOwner(const ParticleSystemComponent& component) {
	Handle<Entity> owner;
	EntityManager::ForEach<ParticleSystemComponent>([&component, &owner](Handle<Entity> entity, const ParticleSystemComponent& value) {
		if (&value == &component) {
			owner = entity;
		}
	});
	return owner;
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// Particle System の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Trail と同じ書き方）
// 保存されるのは Field に書いた物だけなので、Shape で使わない項目も隠さずに全部出す
void DescribeParticleSystem(ComponentUI& ui, ParticleSystemComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと出さない・動かさない・描かない（粒は残る）"));
	// 試しに流す（停止中：選んでいる間だけ流す／再生中：最初からもう一度。保存・読み込みのときは Button が false を返すので何も起きない）
	if (ParticleSystem::IsPreviewing(value)) {
		if (ui.Button("■ Stop Preview")) {
			ParticleSystem::StopPreview();
		}
	} else if (ui.Button("▶ Preview")) {
		ParticleSystem::StartPreview(value);
	}

	ui.Separator("Playback");
	ui.Field("Play On Start###playOnStart", value.playOnStart, Tip("Play で流し始める。外すと ParticleSystem::Play を呼ぶまで出さない（使ったら飛び散る物）"));
	ui.Field("Looping###looping", value.looping, Tip("Duration 秒ごとにくり返す（Burst もまた出る）。外すと1回で出すのをやめる"));
	ui.Field("Duration###duration", value.duration, AtLeast(kMinDuration).Tip("1回の長さ（秒）"));

	ui.Separator("Emission");
	ui.Field("Rate Over Time###rateOverTime", value.rateOverTime, AtLeast(0.0f).Tip("1秒に出す数。0 で Burst だけ"));
	ui.Field("Burst Count###burstCount", value.burstCount, AtLeast(0.0f).Tip("1回の始めに、まとめて出す数"));
	ui.Field("Max Particles###maxParticles", value.maxParticles, Range(1.0f, static_cast<float>(ParticleSystem::kMaxParticles)).Tip("同時に出ている数の上限。いっぱいの間は出さない"));

	ui.Separator("Shape");
	ui.Field("Shape###shape", value.shape, Tip("Point：点から全部の向き／Sphere：球から外へ／Circle：XY の円から外へ／Cone：上（+Y）へ開く／Box：箱から外へ"));
	ui.Field("Radius###radius", value.radius, AtLeast(0.0f).Tip("Sphere・Circle・Cone の半径（m）"));
	ui.Field("Cone Angle###coneAngle", value.coneAngle, Range(0.0f, 90.0f).Tip("Cone の開き（度）。縁から出た粒ほど外へ傾く"));
	ui.Field("Box Size###boxSize", value.boxSize, AtLeast(0.0f).Tip("Box の大きさ（m）"));
	ui.Field("From Surface###fromSurface", value.fromSurface, Tip("形の表面（縁）から出す。Circle なら輪から飛び散る"));

	ui.Separator("Start (Min, Max)");
	ui.Field("Lifetime###lifetime", value.lifetime, AtLeast(kMinLifetime).Tip("消えるまでの秒。左と右の間でランダム"));
	ui.Field("Speed###speed", value.speed, Tip("Shape の向きへの速さ（m/s）。負なら内へ（吸い込む）"));
	ui.Field("Size###size", value.size, AtLeast(0.0f).Tip("大きさ（m。板の1辺）"));
	ui.ColorField("Color A###colorA", value.colorA);
	ui.ColorField("Color B###colorB", value.colorB);
	ui.Label("初めの色は Color A と B の間でランダム（同じにすれば1色）");

	ui.Separator("Over Lifetime");
	ui.Field("Gravity###gravity", value.gravity, Tip("加速（m/s²）。落とすなら Y を負、昇らせるなら正。Local なら Entity の向きで"));
	ui.Field("Drag###drag", value.drag, AtLeast(0.0f).Tip("1秒あたりの減速の割合。大きいほどすぐ止まる（火花）"));
	ui.Field("End Size Rate###endSizeRate", value.endSizeRate, AtLeast(0.0f).Tip("最後の大きさ（初めの大きさに掛ける）。0 で縮んで消える"));
	ui.ColorField("End Color###endColor", value.endColor);
	ui.Label("最後の色は初めの色に掛ける（A 0 でフェードアウト）");
	ui.Field("Ease###ease", value.ease, Tip("大きさと色の変わり方（OutBack などは大きさが少し行き過ぎる）"));
	ui.Field("Converge###converge", value.converge, Tip("中心（Entity の位置）へ集まる。寿命の終わりにちょうど着く（Speed・Gravity・Drag は使わない）。Shape の From Surface と合わせると、輪・球の表面から吸い込まれる"));
	ui.Field("Converge Ease###convergeEase", value.convergeEase, Tip("集まり方。InQuad：だんだん速く吸い込まれる／OutQuad：初め速く、中心でゆっくり止まる"));

	ui.Separator("Renderer");
	ui.AssetField("Texture###texture", value.textureHandle, AssetType::Texture, Tip("粒の絵（白で作って、色は Color で付ける）。無ければ丸いぼかし"));
	ui.Field("Blend###blend", value.blend, Tip("Add：光る粒（重なるほど明るい）／Normal：普通（奥から順には並べないので、重なると縁が汚いことがある）"));
	ui.Field("Space###space", value.space, Tip("World：出た所に残る／Local：Entity と一緒に動く・回る"));
	ui.Field("Use Fog###useFog", value.useFog, Tip("霧（Fog）を混ぜる。外すと霧の中でもそのままの色（光る粒をくっきり）"));
	ui.Field("Use Depth Of Field###useDepthOfField", value.useDepthOfField, Tip("被写界深度でぼかす（奥の層の葉・湯気など）。ぼけ方は粒の後ろにある物の距離で決まるので、ピントの合う所の粒は外しておく"));
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void ParticleSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<ParticleSystemState>();

	// Particle System の置き場所・保存・Inspector を登録する。見せ方は DescribeParticleSystem 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<ParticleSystemComponent>();
	ComponentSerializer::Register<ParticleSystemComponent>("Particle System", &DescribeParticleSystem);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<ParticleSystemComponent>>("Particle System", "Effects", &DescribeParticleSystem));
}

void ParticleSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void ParticleSystem::Update(bool isGameRunning, float deltaTime) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	std::vector<EmitterState>& emitters = state->emitters;

	// ===== Play / Stop が切り替わったら、全部捨てる =====
	// Preview の粒をゲームに持ち込まない・ゲームの粒を停止中に残さない（Play On Start も、また最初から見る）
	if (isGameRunning != state->wasGameRunning) {
		emitters.clear();
		state->previewEntity = {};
		state->wasGameRunning = isGameRunning;
	}

	// ===== 停止中は、Preview の1つだけ =====
	if (!isGameRunning) {
		UpdatePreview();
		return;
	}

	// ===== 1. 消えた・外した Particle System の粒を捨てる =====
	// RequestAdd したばかり（次のフレームに付く）の物は残す（同じフレームに Play・Emit した分を捨てないように）
	std::erase_if(emitters, [](const EmitterState& emitter) {
		if (!EntityManager::IsAlive(emitter.entity)) {
			return true;
		}
		return EntityManager::Get<ParticleSystemComponent>(emitter.entity) == nullptr && !EntityManager::IsAddPending<ParticleSystemComponent>(emitter.entity);
	});

	// ===== 2. 出す・動かす =====
	EntityManager::ForEach<ParticleSystemComponent>([deltaTime](Handle<Entity> entity, const ParticleSystemComponent& component) {
		if (IsSleeping(entity, component)) {
			return; // 止めておく（粒は残す。有効に戻したら続きから）
		}
		EmitterState& emitter = FindOrAddEmitter(entity);
		// 初めて見つけたら、Play On Start を見る（先にゲームが Play・Stop を呼んでいたら、そちらに従う）
		if (!emitter.isStartChecked) {
			emitter.isStartChecked = true;
			if (component.playOnStart) {
				Restart(emitter);
			}
		}
		UpdateEmitter(emitter, component, WorldMatrixOf(entity), deltaTime);
	});
}

//=============================================================================
// 描画
//=============================================================================
void ParticleSystem::Draw() {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	std::vector<ParticleData>& instances = state->instances;
	for (const EmitterState& emitter : state->emitters) {
		const ParticleSystemComponent* component = EntityManager::Get<ParticleSystemComponent>(emitter.entity);
		if (emitter.particles.empty() || component == nullptr || IsSleeping(emitter.entity, *component)) {
			continue;
		}

		// --- 粒ごとの行列と色を並べる（今の年の割合 t で、初めの値から最後の値へ）---
		const Matrix4x4 worldMatrix = WorldMatrixOf(emitter.entity);
		const bool isLocal = component->space == ParticleSpace::Local;
		const Vector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
		instances.clear();
		for (const LiveParticle& particle : emitter.particles) {
			const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
			const float eased = Ease::Apply(t, component->ease); // OutBack などは 1 を少し越える（大きさの「ポン」）
			const float size = particle.startSize * MathUtility::Lerp(1.0f, component->endSizeRate, eased);
			// 色は 0〜1 に収める（行き過ぎると A が負になり、Add で暗くなるので）
			const Vector4 color = MultiplyColor(particle.startColor, Lerp(white, component->endColor, std::clamp(eased, 0.0f, 1.0f)));
			const Vector3 position = isLocal ? TransformPoint(particle.position, worldMatrix) : particle.position;
			// VS は world から中心（4行目）と大きさ（1・2行目の長さ）だけを読み、カメラの方へ向けて四角を組む
			Matrix4x4 world = MakeScaleMatrix({size, size, size});
			world.m[3][0] = position.x;
			world.m[3][1] = position.y;
			world.m[3][2] = position.z;
			instances.push_back({world, color});
		}

		// --- 1回のインスタンス描画で描く ---
		Renderer::ParticleInstancesConfig config;
		config.instances = instances;
		config.textureHandle = component->textureHandle;
		config.isSoftCircle = (component->textureHandle == 0); // 絵が無ければ丸いぼかし
		config.blendMode = (component->blend == ParticleBlend::Add) ? BlendMode::Add : BlendMode::Normal;
		config.useFog = component->useFog;
		config.useDepthOfField = component->useDepthOfField;
		config.isSceneHidden = SceneVisibility::IsHidden(emitter.entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない
		Renderer::DrawParticleInstances(config);
	}
}

//=============================================================================
// ゲームから使う
//=============================================================================
void ParticleSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	if (!EntityManager::IsAlive(entity)) {
		return;
	}
	Restart(FindOrAddEmitter(entity));
}

void ParticleSystem::Stop(Handle<Entity> entity, bool clear) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	if (!EntityManager::IsAlive(entity)) {
		return;
	}
	EmitterState& emitter = FindOrAddEmitter(entity);
	emitter.isStartChecked = true; // 先に Stop したら、Play On Start でも流し始めない
	emitter.isPlaying = false;
	emitter.isBurstPending = false;
	if (clear) {
		emitter.particles.clear();
	}
}

void ParticleSystem::Emit(Handle<Entity> entity, int count) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	const ParticleSystemComponent* component = EntityManager::Get<ParticleSystemComponent>(entity);
	if (component == nullptr || count <= 0) {
		return;
	}
	EmitParticles(FindOrAddEmitter(entity), *component, WorldMatrixOf(entity), count);
}

bool ParticleSystem::IsAlive(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	const EmitterState* emitter = FindEmitter(entity);
	return (emitter != nullptr) && (emitter->isPlaying || !emitter->particles.empty());
}

//=============================================================================
// エディタ（Inspector の Preview）
//=============================================================================
void ParticleSystem::StartPreview(const ParticleSystemComponent& component) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	const Handle<Entity> owner = FindOwner(component);
	if (!owner.IsValid()) {
		return;
	}
	// 再生中は、最初からもう一度流すだけ（ゲームの粒はそのまま）
	if (state->wasGameRunning) {
		Play(owner);
		return;
	}
	state->emitters.clear();
	state->previewEntity = owner;
	Restart(FindOrAddEmitter(owner));
}

void ParticleSystem::StopPreview() {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	state->previewEntity = {};
	// 停止中に持っている粒は Preview の物だけなので、全部捨てる
	if (!state->wasGameRunning) {
		state->emitters.clear();
	}
}

bool ParticleSystem::IsPreviewing(const ParticleSystemComponent& component) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	return state->previewEntity.IsValid() && EntityManager::Get<ParticleSystemComponent>(state->previewEntity) == &component;
}
```

#### 解説
- **`std::ranges::find(state->emitters, entity, &EmitterState::entity)`**：3つ目の引数（射影）で「各要素の `entity` メンバ」と比べる。ラムダを書かずに「このメンバがこの値の物」を探せる（Trail と同じ書き方）。
- **`std::erase_if(vector, 条件)`**：条件に合う要素を全部消す（C++20）。並びは保つ。1回なめるだけなので速い。
- **`std::fmod(time, duration)`**：小数の割り算の余り。2.5 秒で Duration 1 なら 0.5 になる（1フレームが長くて Duration を越えても、次の1回の途中から続く）。
- **`ParticleSystem::Draw` で `instances` をメンバ（state）に持つ**：毎フレーム `std::vector` を作り直すと、そのたびにメモリを取る。`clear()` は中身を消すだけでメモリは残るので、使い回すと速い。`DrawParticleInstances` が中でリクエストへ写すので、次の Entity で上書きしてよい。
- **World と Local**：World は出た瞬間に Entity の行列を掛けてワールド座標にして持つ（後で Entity が動いても付いていかない）。Local は Entity から見た座標のまま持ち、**描くたびに**今の行列を掛ける（Entity と一緒に動く・回る）。Gravity も Local なら Entity の向きで効く。
- **Preview と `HierarchyWindow`**：エディタの無い製品版（Release）には `HierarchyWindow` が無いので `#ifdef USE_IMGUI` で囲む。製品版はいつも再生中なので、Preview の道は通らない。

---

## 4. パートC：つなぐ

### `Engine.cpp`
include（`TrailSystem.h` の下）：
```cpp
#include "MyEngine/Systems/ParticleSystem.h"
```
`Engine::Initialize` の `TrailSystem::Initialize();` の下：
```cpp
	ParticleSystem::Initialize();  // Particle SystemをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```
`Engine::Finalize` の `TrailSystem::Release();` の下：
```cpp
	ParticleSystem::Release();  // シーン（ウィンドウ）の破棄より後にする
```

### `Window/WindowManager.cpp`
include（`TrailSystem.h` の下）：
```cpp
#include "MyEngine/Systems/ParticleSystem.h"
```
`UpdateAll` の `TrailSystem::Update(...)` の下：
```cpp
	ParticleSystem::Update(isGameRunning, Time::GetDeltaTime()); // パーティクル（Particle System Component）。停止中は Inspector の Preview の1つだけ動く
```
`DrawAll` の `TrailSystem::Draw(gameCamera);` の下：
```cpp
	ParticleSystem::Draw();                                   // パーティクル（Particle System Component）の描画。粒はビューごとのカメラへ VS で向ける
```

### `ECS/GameComponent.h`
ゲームの Component・Script から `ParticleSystem::Play` などを使えるように（`TrailSystem.h` の下）：
```cpp
#include "MyEngine/Systems/ParticleSystem.h"
```

### `Editor/Windows/HierarchyWindow.cpp`
include（`LightComponent.h` の下）：
```cpp
#include "MyEngine/Components/ParticleSystemComponent.h"
```
`DrawCreateMenu` の「3D Object」のメニューの `ImGui::EndMenu(); }` の後（「カメラ」の上）に：
```cpp
	// --- 効果：パーティクル（初めの値のまま、停止中は Inspector の Preview で流して見る）---
	if (ImGui::BeginMenu("Effects")) {
		if (ImGui::MenuItem("Particle System")) {
			EditorHistory::RequestCreate("Particle System", top, {ComponentSnapshot::Make(ParticleSystemComponent{})});
		}
		ImGui::EndMenu();
	}
```

### `MyEngine_v1.vcxproj`
```
    <ClCompile Include="MyEngine\Systems\ParticleSystem.cpp" />
    <ClInclude Include="MyEngine\Components\ParticleSystemComponent.h" />
    <ClInclude Include="MyEngine\Systems\ParticleSystem.h" />
```
（それぞれ Trail の行の下。VS の「既存の項目の追加」でもよい）

---

## 5. パートD：ParticleManager のグループが増え続ける件

**何が起きていたか**：`ParticleEmitter` は初めて `Emit` したときに `ParticleManager::CreateGroup` でグループを作る。グループは消す手段が無いので、シーンを読み直す（Play / Stop・`LoadScene`・やり直し）たびに、新しいシーンのエミッターがまたグループを作って、**使われないグループが増えていく**（中の粒も描かれ続ける）。`ClearParticles` は粒を消すだけで、しかもどこからも呼ばれていなかった。

**直し方**：エミッターはシーンが持つ物なので、シーンを入れ替えた（古いシーンが消えた）後なら、グループは全部捨ててよい。

### `Particle/ParticleManager.h`
`ClearParticles` を丸ごと置き換える：
```cpp
	/// <summary>
	/// 全部のグループを、中の粒ごと消す（SceneManager がシーンを入れ替えるときに呼ぶ）
	/// <para>グループはエミッター（シーンが持つ）が初めて Emit したときに作るので、古いシーンのエミッターが消えた後なら、全部捨ててよい</para>
	/// <para>捨てないと、シーンを読み直す・Play / Stop するたびに、使われないグループが増えていく</para>
	/// </summary>
	static void ClearGroups() {
		if (instance_) {
			instance_->groups_.clear();
		}
	}
```

### `Scene/SceneManager.cpp`
include（`GameComponent.h` の下）：
```cpp
#include "MyEngine/Particle/ParticleManager.h"
```
`ChangeScene` の `currentScene_ = std::move(next);` の下：
```cpp
	// 古いシーンのエミッター（ParticleEmitter）はここで消えたので、その粒のグループも捨てる（新しいシーンのエミッターは、初めて出すときに作り直す）
	ParticleManager::ClearGroups();
```
- `currentScene_ = std::move(next);` の行で古いシーン（と、メンバのエミッター）が消える。新しいシーンのエミッターは、まだ1回も `Emit` していない（`Emit` は Update の中）ので、ここで捨ててもずれない。
- 気をつけること：シーンの**外**（static など）にエミッターを置くと、そのエミッターのグループも消えて、次の `Emit` で assert になる。エミッターはシーンのメンバに置く（今までどおり）。
- 新しい物は Particle System を使う。ParticleManager・ParticleEmitter は CG3_Project が使っているので残す。

---

## 6. 使い方

### エディタで置く
1. Hierarchy の **＋ → Effects → Particle System**（または Add Component → **Effects → Particle System**）。初めは白い丸いぼかしが Sphere から 1秒に10個出る。
2. Inspector の **▶ Preview** を押すと、停止中でも流れる（値を変えるとすぐ反映）。**■ Stop Preview** か、ほかの物を選ぶと止まる。
3. Play で、Play On Start の物が流れ始める。

![＋ → Effects → Particle System で作った所](images/particle_create.png)

![停止中の Preview（Circle・From Surface・Burst 32 の輪）](images/particle_preview.png)

| 見出し | 項目 | 中身 |
|---|---|---|
| — | Enabled・▶ Preview | 外すと止まる・描かない（粒は残る）／停止中に試しに流す（再生中は最初からもう一度） |
| Playback | Play On Start・Looping・Duration | Play で流し始める／Duration 秒ごとにくり返す（Burst もまた）／1回の長さ |
| Emission | Rate Over Time・Burst Count・Max Particles | 1秒に出す数／1回の始めにまとめて出す数／同時に出ている数の上限（4096 まで） |
| Shape | Shape・Radius・Cone Angle・Box Size・From Surface | 1.3 の表 |
| Start (Min, Max) | Lifetime・Speed・Size・Color A・Color B | 左と右の間でランダム。色は A と B の間 |
| Over Lifetime | Gravity・Drag・End Size Rate・End Color・Ease・Converge・Converge Ease | 加速／減速の割合／最後の大きさ（掛ける）／最後の色（掛ける。A 0 で消える）／変わり方／中心へ集まる（寿命の終わりにちょうど着く）／集まり方 |
| Renderer | Texture・Blend・Space・Use Fog・Use Depth Of Field | 無ければ丸いぼかし／Add・Normal／World・Local／霧を混ぜるか（初めは ✓）／被写界深度でぼかすか（初めは外す。1.5） |

### よく使う形（AL4_01 の ParticleTitleClear.md 2.4 の値）
| 物 | 値 |
|---|---|
| 円の中心のキラキラ（Sparkle） | Shape Sphere・Radius 0.2／Rate 8／Speed 0.1〜0.3／Lifetime 0.6〜1.0／End Color A 0／Add／Color A・B を円の色に |
| 使った瞬間に輪から飛び散る（Burst） | Play On Start を外す・Looping を外す／Shape Circle・From Surface／Rate 0・Burst 24／Speed 3〜5・Drag 2／End Size Rate 0 |
| ゴールのキラキラ | Shape Box（扉の大きさ）／Rate 15／Gravity Y 0.5 |
| ゴールで飛び散る | Play On Start・Looping を外す／Shape Sphere・From Surface／Burst 60・Speed 4〜8・Drag 1.5・Gravity Y 1 |
| **中心へ吸い込まれる** | Shape Circle（か Sphere）・From Surface／Rate 40／Lifetime 0.8〜1.2／**Converge ✓・Converge Ease InQuad**／End Size Rate 0.3（中心で小さく）。動く物に付けるなら Space Local（中心が付いていく） |

![Converge（Circle・From Surface・半径 1.5。左右は 0.25 秒違い。輪から出た粒が、小さくなりながら真ん中へ吸い込まれる）](images/particle_converge.png)

### ゲームから使う
```cpp
#include "MyEngine/Systems/ParticleSystem.h" // GameComponent.h を include していれば要らない

// 子の「Burst」を最初から（Play On Start を外した、使ったら飛び散る物）
GameObject burst = gameObject.FindChild("Burst");
if (burst) {
	ParticleSystem::Play(burst.handle);
}

// 子の「Sparkle」を止める（出ている粒は寿命まで残る。すぐ消すなら Stop(handle, true)）
if (GameObject sparkle = gameObject.FindChild("Sparkle")) {
	ParticleSystem::Stop(sparkle.handle);
}

// 今すぐ 10 個（Play していなくても、Component の値で出る）
ParticleSystem::Emit(burst.handle, 10);

// 出している途中か・粒が残っているか（全部消えてから Entity を消す、など）
if (!ParticleSystem::IsAlive(burst.handle)) { … }
```
- 子が無ければ `FindChild` は消えた GameObject と同じ扱いで、`if (burst)` が false。Handle が無効でも Play・Stop は何もしない。
- Prefab から作った同じフレームに `Play` してもよい（Component が付くのは次のフレームの頭だが、Play した事は覚えている）。`Emit` は Component が付いてから（次のフレームから）。

---

## 7. 確かめたこと
作業用コピー（AL4_01 の Level01 に、テスト用の Particle System を5つ足したシーン）で：

**Release**（画面を時間ごとに撮った）：

![Release（オレンジ＝Cone・水色＝Sphere・葉＝Box と Normal と Texture）](images/particle_release.png)

| 物 | 結果 |
|---|---|
| Circle・From Surface・Burst 32・Looping・Duration 1.2・Drag 2.5・End Size Rate 0 | 1.2 秒ごとに輪が出て、広がりながら止まり、縮んで消える |
| Cone 20°・Rate 40・Gravity −9.8 | 上へ噴いて落ちる噴水になる（オレンジ → 赤くなって消える） |
| Sphere・Rate 25・Gravity +0.5 | 水色のキラキラが昇って消える |
| Box 4×0.5×1・Texture leaf.png・Normal・Gravity −0.8 | 葉が降る（テクスチャの絵・Normal の重ね方） |
| 幕が開く前（ゲームの時間が止まっている間） | 粒も止まる |
| Use Fog・Use Depth Of Field（テスト用に霧と DOF を強くしたシーン・壁の手前に「あり・なし」を並べた） | 霧なしはそのままの色・霧ありは霧の色に寄る。DOF ありは後ろの壁と同じだけぼけ、なしはくっきり（1.5 の写真） |
| Converge（Circle・From Surface・Rate 40・InQuad・End Size Rate 0.3） | 輪から出た粒が、中心へ吸い込まれて消える（突き抜けない）。6章の写真 |

**Debug のエディタ**（ImGui に入力を流すテスト用の仕掛け。外した）：

| 操作 | 結果 |
|---|---|
| 停止中に Particle System の Entity を選ぶ | Inspector に「Particle System」と全部の項目 |
| ▶ Preview | ボタンが ■ Stop Preview になり、停止中の Scene ビューに輪が 1.2 秒ごとに出る。`IsAlive` が true |
| ほかの Entity を選ぶ | Preview が止まり、粒が消える。`IsAlive` が false |
| ＋ → Effects → Particle System | 「Particle System」の Entity ができる（Entities 39 → 40・シーンに * が付く・初めの値） |
| Save | シーンファイルに `"Particle System": { "enabled": …, "space": "World" }` の全部の項目が出る。一部の項目が無いシーン（coneAngle・boxSize を書かなかった物）も、初めの値で読めて警告が出ない |
| Play | Play On Start の物が流れる |
| Play On Start を外した物 | 流れない（`IsAlive` false）→ `Play` で Burst が出る（`IsAlive` true）→ 寿命で消えると false |
| `Stop(handle, true)` | すぐ消える（`IsAlive` false） |
| `Stop(handle)` | 出すのをやめて、出ている粒は寿命まで残る（すぐは true、1 秒後に false） |
| 止まっている物に `Emit` | Play していなくても出る（`IsAlive` true） |

Debug・Release とも、新しいファイル・直したファイルに警告なし。ログに警告・エラー無し。

確かめていないこと：
- Space の Local（Entity を動かしながら見ていない。コードは描くときに行列を掛けるだけ）。
- Hierarchy の目で隠した Particle System（ほかの Renderer と同じ `SceneVisibility::IsHidden`）。
- ParticleManager の `ClearGroups`（Play / Stop・シーンの切り替えのたびに通っていて、落ちない所までは見た。ParticleEmitter を使うシーンは AL4_01 に無い）。

---

## 8. 決めたこと・後でやること
- **ゴールだけ光らせる（G2）は、グロー層と HDR のどちらにするか決めてから**、Particle System に `glow`（グロー層）か `Intensity`（HDR）を足す。今のシーンはそのまま読める。
- **Use Depth Of Field の粒のぼけ方は、後ろにある物の距離で決まる**（1.5）。粒ごとに自分の距離でぼかすのは、要るなら後で。
- **回転する粒**は入れない（VS が四角を `right / up` で組むので、回すには VS とインスタンスのデータを変える。要るなら後で）。
- **Normal の並べ替え**（奥から順）はしない。半透明の煙などを重ねて汚く見えたら、描く前に粒をカメラからの距離で並べ替える（Normal のときだけ）。
- **大きさは Entity の Scale に関係ない**（Shape の大きさ・Local の位置には効く）。
- 1フレームにたくさん出すとき、全部同じ場所・同じ年で出る（動く Entity から Rate を大きく出すと、フレームごとにかたまって見える）。気になったら、1フレームの中で出る時刻と位置をずらす（Unity の「Emit over distance」に近い物）。
- GPU パーティクル（CS）は G13（P3）。数が 4096 を超えて重くなったら。
