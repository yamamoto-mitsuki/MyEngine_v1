# 粒の回転（Particle System の Rotation）と「最初のフレームの Play が消える」の直し

参考：
- Unity の Particle System「Main module」の Start Rotation：https://docs.unity3d.com/Manual/PartSysMainModule.html
- Unity の「Rotation over Lifetime module」（Angular Velocity）：https://docs.unity3d.com/Manual/PartSysRotOverLifeModule.html
- Unity の「Renderer module」（Render Alignment の Velocity・Stretched Billboard）：https://docs.unity3d.com/Manual/PartSysRendererModule.html
- HLSL の `sincos`：https://learn.microsoft.com/windows/win32/direct3dhlsl/dx-graphics-hlsl-sincos ・ `atan2`：https://learn.microsoft.com/windows/win32/direct3dhlsl/dx-graphics-hlsl-atan2
- 2D の回転の式（回転行列）：https://ja.wikipedia.org/wiki/回転行列

## このファイルについて
- AL4_01 の [Polish2.md](../../../AL4_01/Docs/Polish2.md) の **0章**（粒の png をそのまま使うと違和感がある → 回転が要る。エンジンの仕事なので、ゲームのチャットでは止めた）と、同じ所に書いた **「最初のフレームに Script から `ParticleSystem::Play` を呼ぶと消える」** のエンジン側の直し。
- 前提：エンジン develop（2f9a621「トーンマッピング実装」）。中身は [ParticleSystem.md](ParticleSystem.md)（E）・[HDRRendering.md](HDRRendering.md)（Intensity）の続き。
- **写す場所**：`AL4_01/MyEngine_v1`（サブモジュール）→ エンジンでコミット・push → `source/repos/MyEngine_v1` で pull（HDR と同じ）。
- **ゲーム（AL4_01）のコードは変わらない**。前に保存したシーン・Prefab もそのまま読める（新しい値が無い物は 0 度・回らない・Stretch 1＝今の見た目）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Systems/ParticleSystem.cpp`（5か所） | 最初のフレームの `Play`・`Emit`・`Stop` が消えるのを直す |
| B | `Components/ParticleSystemComponent.h`・`Systems/ParticleSystem.cpp`（4か所） | 回転の値（Start Rotation・Angular Velocity・Random Spin Direction・Align To Velocity・Stretch）と、粒の角度を進める所・Inspector |
| C | `Graphics/Pipeline/ShaderConstants.h`・`Graphics/Renderer/Renderer.h / .cpp`・`Shader/Data/Particle/Particle.VS.shader`・`Systems/ParticleSystem.cpp`（Draw） | 角度と向きを GPU へ送り、VS で板の角を回す |
| D | `Systems/AnimationSystem.cpp`（4か所） | A と同じ直しを Animator にも（最初のフレームの `AnimationSystem::Play` が消える） |

- **A・D は単独で写してよい**（どの順でも）。**B → C の順**（C の Draw が B の値を読む）。B だけ写した所でもビルドは通る（値は Inspector に出るが、まだ回らない）。
- **Release の exe は AL4_01 の `project/MyEngine/Shader` の写しを読む**。C は C++ も変えるのでエンジンをビルドすれば PostBuildEvent で写るが、**写っていないと Release で粒が壊れる**（C++ は 96 バイト、古い VS は 80 バイトずつ読むので、2個目から先の粒の位置・色がずれる）。困ったら 6章。

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 最初のフレームの Play が消える（パート A）
**原因**：ゲームが動き出した最初のフレームの中の順番。
```
WindowManager::UpdateAll（1フレーム）
  sceneManager->Update()
    └ ScriptSystem::Update … Script の Start → ParticleSystem::Play(粒) ← ① 粒の状態を作って「再生中」にする
  …（ワールド行列・物理・カメラ）
  ParticleSystem::Update(isGameRunning = true, …)
    └ 「前のフレームは止まっていた（wasGameRunning = false）→ 動き出した」として emitters を全部捨てる ← ② ①が消える
```
- `wasGameRunning` での「全部捨てる」は、エディタの Preview の粒をゲームに持ち込まない・ゲームの粒を停止中に残さないための物で、捨てること自体は正しい。**捨てるのが「動き出したフレームの Script の後」になっているのが悪い**。
- 製品版（Release）は起動したフレームが「動き出したフレーム」（`wasGameRunning` の初めの値が false）。エディタは Play を押した次のフレーム（Play で作り直したシーンの Script の Start が、ちょうどそこで回る）。
- Play On Start の粒は、②の後に ParticleSystem::Update 自身が見るので消えない。**Script から呼ぶ物だけが消える**。

**直し方**：`Play`・`Stop`・`Emit` は **ゲーム（Script・SYSTEM）からしか呼ばれない**（どちらもゲームが動いている間しか回らない）。なので「呼ばれた＝ゲームは動いている」として、**その場で切り替え（全部捨てる）を済ませてから**粒を足す。後の `ParticleSystem::Update(true)` は「もう切り替え済み」なので捨てない。
- 切り替えの3行を `SwitchGameRunning(bool)` という関数にまとめ、`Update` と `Play`・`Stop`・`Emit` から呼ぶ（同じ値で何回呼んでも、切り替わったときしか捨てない）。
- 別の案（やめた）：WindowManager でフレームの頭に「動いているか」を決めて、各 System に先に知らせる。全部の System の作りが変わるので、今回は ParticleSystem の中だけで閉じる方にした。
- **AnimationSystem にも同じ形がある**（`AnimationSystem::Play` を最初のフレームの Script から呼ぶと、後の `AnimationSystem::Update` が一覧を捨てる）。同じ直しをパート D でする。

### 1.2 粒の回転（パート B・C）
粒はいつもカメラの方を向く板（ビルボード）。だから **「回す」＝画面の上で板を回す**（2D の回転）で、角度は1つ（float）で足りる。

**値（Inspector の Rotation）**
| 項目 | 中身 | Unity では |
|---|---|---|
| Start Rotation（Min, Max） | 初めの角度（度）。**正で左回り**（Transform の Rotation Z と同じ向き） | Main の Start Rotation |
| Angular Velocity（Min, Max） | 回る速さ（度/秒）。Drag では遅くならない | Rotation over Lifetime |
| Random Spin Direction | 半分の粒を逆に（初めの角度も回る向きも） | Main の Flip Rotation（を 0.5 で固定した物） |
| Align To Velocity | 絵の右（+X）を、**画面の上で進む向き**に向ける。Start Rotation はそこから足す（絵が上向きに描いてあれば -90） | Renderer の Render Alignment = Velocity |
| Stretch | 絵の横（X）だけに掛ける長さの倍率。Align と合わせると進む向きに細長い線 | Stretched Billboard の Length Scale（の簡単な版） |

**どこで回すか**：VS（`Particle.VS`）。CPU で world 行列に回転を入れても、今の VS は world から「中心」と「大きさ」しか取り出さず、カメラの右・上でそのまま四角を組むので捨てられる。そこで GPU へ送る `ParticleData` に **`rotation`（角度）と `direction`（進む向き）** を足して、VS で四角の角を回してからカメラの右・上に乗せる。
```
前：角 (x, y) × 大きさ ──────────────→ center + right × x' + up × y'
後：角 (x, y) × 大きさ → 画面の上で angle だけ回す → center + right × x' + up × y'
        （横は size × Stretch・縦は size）    x' = x cos − y sin
                                              y' = x sin + y cos
```
- **大きさを掛けてから回す**：逆にすると、横に伸ばした板（Stretch）が、回った向きに伸びない（いつも画面の横に伸びる）。
- **Align To Velocity は角度を CPU で作らない**：画面の上の向きは、見ているカメラで変わる（Scene ビューと Game ビューでカメラが違う。Draw は1回で両方に使われる）。そこで CPU は **ワールドの向き（速さ）だけ**を `direction` に入れ、VS がそのビューのカメラの right・up との内積で「画面の上の X・Y」にして、`atan2(Y, X)` で角度にする（右が 0 度・左回りが正。Start Rotation と同じ向き）。
  - 向きが決まらない粒（止まっている・カメラへまっすぐ飛ぶ）は、画面に写した長さがほぼ 0 なので、足さない（Start Rotation だけ）。
  - Converge の粒は速さを使わないので、「出た所 → 中心」の向き。Local の粒は Entity の向きを掛けてワールドに直す。
- **`ParticleData` は 80 → 96 バイト**（`Vector3 direction`＋`float rotation` の 16 バイト。HLSL の `float3`＋`float` と同じ並び）。`static_assert` で大きさを見張る。
- 前からの `ParticleManager`（`Renderer::DrawParticle`）は、足した2つが初めの値（0）のまま渡るので、見た目は変わらない。

---

## 2. パート A：最初のフレームの Play が消えるのを直す（`Systems/ParticleSystem.cpp`）

① `ParticleSystemState` の `wasGameRunning` のコメントと、その下に関数を1つ足す（61行目あたり）
```cpp
// 前
	bool wasGameRunning = false;         // 前のフレームでゲームが動いていたか（Play / Stop の切り替わりで、粒を全部捨てる）
};
std::unique_ptr<ParticleSystemState> state;
// 後
	bool wasGameRunning = false;         // ゲームが動いているとして粒を持っているか（Play / Stop の切り替わりで、粒を全部捨てる。SwitchGameRunning）
};
std::unique_ptr<ParticleSystemState> state;

// ゲームが動き出した・止まったら、粒を全部捨てる（Preview の粒をゲームに持ち込まない・ゲームの粒を停止中に残さない。Play On Start も、また最初から見る）
// 切り替わったときだけ捨てる（同じ値で何回呼んでもよい）
void SwitchGameRunning(bool isGameRunning) {
	if (isGameRunning == state->wasGameRunning) {
		return;
	}
	state->emitters.clear();
	state->previewEntity = {};
	state->wasGameRunning = isGameRunning;
}
```

② `ParticleSystem::Update` の頭（399行目あたり）
```cpp
// 前
	// ===== Play / Stop が切り替わったら、全部捨てる =====
	// Preview の粒をゲームに持ち込まない・ゲームの粒を停止中に残さない（Play On Start も、また最初から見る）
	if (isGameRunning != state->wasGameRunning) {
		emitters.clear();
		state->previewEntity = {};
		state->wasGameRunning = isGameRunning;
	}
// 後
	// ===== Play / Stop が切り替わったら、全部捨てる =====
	// ゲームが動き出した最初のフレームは、ここより先に Script が Play・Emit を呼んでいて、そこで切り替え済み（その分は捨てない）
	SwitchGameRunning(isGameRunning);
```
（その下の `std::vector<EmitterState>& emitters = state->emitters;` は、1. の `erase_if` が使うので残す）

③ 「ゲームから使う」の見出しの下と `Play`（486行目あたり）
```cpp
// 前
//=============================================================================
// ゲームから使う
//=============================================================================
void ParticleSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	if (!EntityManager::IsAlive(entity)) {
// 後
//=============================================================================
// ゲームから使う
//=============================================================================
// Play・Stop・Emit を呼ぶのはゲーム（Script・SYSTEM。どちらもゲームが動いている間だけ回る）なので、呼ばれたらゲームは動いている
// → 頭で SwitchGameRunning(true) をしておく。ゲームが動き出した最初のフレームは、Script が ParticleSystem::Update より先に回るので、
//   ここで切り替えないと、後の Update が「動き出した」として、この Play・Emit の分まで捨ててしまう
void ParticleSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	SwitchGameRunning(true);
	if (!EntityManager::IsAlive(entity)) {
```

④ `Stop`（496行目あたり）・⑤ `Emit`（510行目あたり）の assert のすぐ下に、同じ1行
```cpp
	MY_ASSERT_MSG(state != nullptr, "ParticleSystem::Initialize()を先に呼んでください");
	SwitchGameRunning(true); // ← 足す
```

### 解説
- **Inspector の Preview は壊れない**：停止中の Preview（`StartPreview`）は `Play` を通らない（`Restart` を直接呼ぶ）。再生中の Preview は `Play` を通るが、そのときはもう `wasGameRunning` が true なので、何も捨てない。
- **Stop を押して止めたとき**：止めた後は Script も SYSTEM も回らないので `Play` などは呼ばれず、次の `Update(false)` で今までどおり全部捨てる。
- `IsAlive` は読むだけなので、切り替えない（最初のフレームに `Play` → `IsAlive` なら、`Play` の方で切り替わっている）。
- これで Polish2 の `WindParticles` は `ParticleSystem::Play` で流す作りにも戻せる（今の Emit の作りのままでもよい。8章）。

---

## 3. パート B：回転の値（`ParticleSystemComponent.h`・`ParticleSystem.cpp`）

### `Components/ParticleSystemComponent.h`（1か所）
「生きている間」の最後（`convergeEase`）と「見た目」の間に足す（80行目あたり）
```cpp
// 前
	Ease::Type convergeEase = Ease::Type::InQuad; // 集まり方（InQuad：だんだん速く吸い込まれる）

	// ===== 見た目 =====
// 後
	Ease::Type convergeEase = Ease::Type::InQuad; // 集まり方（InQuad：だんだん速く吸い込まれる）

	// ===== 回転（Rotation）=====
	// 角度は画面の上での板の回り方（粒はいつもカメラの方を向くので、回すのは画面の上だけ）。正で左回り（Transform の Rotation Z と同じ）
	Vector2 startRotation = {0.0f, 0.0f};   // 初めの角度（度。Min〜Max の乱数。-180〜180 で全部の向き）
	Vector2 angularVelocity = {0.0f, 0.0f}; // 回る速さ（度/秒。Min〜Max の乱数）
	bool randomSpinDirection = false;       // 半分の粒を逆にする（初めの角度も回る向きも。葉が全部同じ向きに回らないように）
	bool alignToVelocity = false;           // 絵の右（+X）を、画面の上で進む向きに向ける（Start Rotation はそこから足す。風の筋・火花）
	float stretch = 1.0f;                   // 絵の横（X）だけに掛ける長さの倍率（Align To Velocity と合わせると、進む向きに細長い線）

	// ===== 見た目 =====
```

### `Systems/ParticleSystem.cpp`（4か所）
① `LiveParticle` に角度と回る速さ（34行目あたり。上の3行は桁をそろえただけ）
```cpp
// 前
	float startSize = 0.0f; // 初めの大きさ（m）
	float age = 0.0f;       // 出てからの秒
	float lifetime = 1.0f;  // 消えるまでの秒
};
// 後
	float startSize = 0.0f;       // 初めの大きさ（m）
	float age = 0.0f;             // 出てからの秒
	float lifetime = 1.0f;        // 消えるまでの秒
	float rotation = 0.0f;        // 今の角度（ラジアン。画面の上で、正で左回り）
	float angularVelocity = 0.0f; // 回る速さ（ラジアン/秒）
};
```

② `EmitParticles` の最後（219行目あたり）
```cpp
// 前
		particle.lifetime = (std::max)(RandomRange(component.lifetime), kMinLifetime);
		emitter.particles.push_back(particle);
// 後
		particle.lifetime = (std::max)(RandomRange(component.lifetime), kMinLifetime);
		// --- 回転：初めの角度と回る速さ（Random Spin Direction なら、半分の粒はどちらも逆向き）---
		const float spinSign = (component.randomSpinDirection && RandomEngine::GetInt(0, 1) == 0) ? -1.0f : 1.0f;
		particle.rotation = RandomRange(component.startRotation) * kDegToRad * spinSign;
		particle.angularVelocity = RandomRange(component.angularVelocity) * kDegToRad * spinSign;
		emitter.particles.push_back(particle);
```

③ `UpdateEmitter` の「1. 動かす」のループの頭（230行目あたり）
```cpp
// 前
	for (LiveParticle& particle : emitter.particles) {
		particle.age += deltaTime;
		// --- Converge：…
// 後
	for (LiveParticle& particle : emitter.particles) {
		particle.age += deltaTime;
		particle.rotation += particle.angularVelocity * deltaTime; // 回す（Converge でも回る。Drag では遅くならない）
		// --- Converge：…
```
（Converge の `continue` より前に置く。後ろに置くと Converge の粒が回らない）

④ `DescribeParticleSystem`：Over Lifetime の最後（`Converge Ease`）の下に足す（365行目あたり）
```cpp
	ui.Field("Converge Ease###convergeEase", value.convergeEase, Tip("集まり方。InQuad：だんだん速く吸い込まれる／OutQuad：初め速く、中心でゆっくり止まる"));
	// ↓ ここから足す

	ui.Separator("Rotation");
	ui.Field("Start Rotation###startRotation", value.startRotation, Tip("初めの角度（度）。左と右の間でランダム。画面の上で、正で左回り（-180, 180 で全部の向き）"));
	ui.Field("Angular Velocity###angularVelocity", value.angularVelocity, Tip("回る速さ（度/秒）。左と右の間でランダム。正で左回り、負で右回り"));
	ui.Field("Random Spin Direction###randomSpinDirection", value.randomSpinDirection, Tip("半分の粒を逆にする（初めの角度も回る向きも）。葉が全部同じ向きに回らないように"));
	ui.Field("Align To Velocity###alignToVelocity", value.alignToVelocity, Tip("絵の右（+X）を、画面の上で進む向きに向ける（Start Rotation はそこから足す）。Converge なら中心の方へ"));
	ui.Field("Stretch###stretch", value.stretch, AtLeast(0.0f).Tip("絵の横（X）だけに掛ける長さの倍率。Align To Velocity と合わせると、進む向きに細長い線（風の筋・火花）"));
```

### 解説
- **保存も Inspector もこの `Field` だけ**（`DescribeParticleSystem` が両方に使われる）。`###` の右が保存の名前（`startRotation` など）。前のシーンファイルには無いので、読むと初めの値（0・false・1）になる。
- `RandomRange` は今までの「Min〜Max の乱数（逆に入れてもよい）」。`kDegToRad` は `GameComponent.h` の物（Cone Angle でも使っている）。
- **`spinSign`（-1 か 1）を角度と速さの両方に掛ける**ので、Start Rotation 0〜30・Angular Velocity 90〜180 なら、半分は「0〜30 度で左回り」、半分は「0〜-30 度で右回り」（左右対称）。`RandomEngine::GetInt(0, 1)` は 0 か 1（両端を含む。Box の From Surface と同じ使い方）。
- 角度は粒の中ではラジアンで持つ（GPU の `sincos` がラジアン）。Inspector は度（Transform の Rotation と同じ）。

---

## 4. パート C：GPU へ送って、VS で回す

### `Graphics/Pipeline/ShaderConstants.h`（1か所。66行目あたり）
```cpp
// 前
// パーティクル
struct ParticleData {
	Matrix4x4 world;
	Vector4 color;
};
// 後
// パーティクル（粒1つ分。StructuredBuffer<Particle> gParticles として Particle.VS が読む）
// VS は world から中心（4行目）と大きさ（1行目の長さ＝横・2行目の長さ＝縦）だけを読み、カメラの右・上で板を組む（world の向きは使わない）
struct ParticleData {
	Matrix4x4 world;
	Vector4 color;
	Vector3 direction = {0.0f, 0.0f, 0.0f}; // 絵の右（+X）を向ける向き（ワールド。長さ 0 なら向けない＝Align To Velocity を使わない粒）
	float rotation = 0.0f;                  // 板を画面の上で回す角度（ラジアン。正で左回り。direction があれば、その向きからさらに回す）
};
static_assert(sizeof(ParticleData) == 96, "HLSLのParticleと大きさが違います");
```

### `Graphics/Renderer/Renderer.cpp`（1か所。`DrawParticleInstances` の中、444行目あたり）
```cpp
// 前
	for (const ParticleData& instance : config.instances) {
		req.instances.push_back({instance.world, SrgbToLinear(instance.color)}); // 色は sRGB → リニア（A はそのまま）
	}
// 後
	for (const ParticleData& instance : config.instances) {
		// 丸ごと写してから、色だけ sRGB → リニア（A はそのまま）。向き・角度などはそのまま渡る
		ParticleData& linear = req.instances.emplace_back(instance);
		linear.color = SrgbToLinear(instance.color);
	}
```

### `Graphics/Renderer/Renderer.h`（コメント2行。229行目あたり）
```cpp
// 前
	// 粒は VS でカメラの方へ向けるので、ここで向きは計算しない（world から読むのは、中心と大きさだけ）
	struct ParticleInstancesConfig : DrawTarget {
		std::span<const ParticleData> instances;  // 粒ごとの行列と色（色は sRGB の見た目の値。中でリニアに直す）。DrawParticleInstances の中で写すので、呼んだ後は消してよい
// 後
	// 粒は VS でカメラの方へ向けるので、ここで向きは計算しない（world から読むのは、中心と大きさだけ。画面の上の回転は direction・rotation）
	struct ParticleInstancesConfig : DrawTarget {
		std::span<const ParticleData> instances;  // 粒ごとの行列・色・向き・角度（色は sRGB の見た目の値。中でリニアに直す）。DrawParticleInstances の中で写すので、呼んだ後は消してよい
```

### `Shader/Data/Particle/Particle.VS.shader`（全体を差し替え）
```hlsl
#META
stage:   VS
path:    Particle/Particle.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Particle.hlsli"
#include "Buffers/Camera.hlsli"


// C++ の ParticleData（ShaderConstants.h）と同じ並び（96 バイト）
struct Particle
{
    float4x4 world;       // 中心（4行目）と大きさ（1行目の長さ＝横・2行目の長さ＝縦）だけを使う
    float32_t4 color;
    float32_t3 direction; // 絵の右（+X）を向ける向き（ワールド。長さ 0 なら向けない）
    float rotation;       // 画面の上で回す角度（ラジアン。正で左回り）
};
StructuredBuffer<Particle> gParticles : register(t0);

// これより短い向き（画面に写した長さの2乗）は、向きが決まらないとみなす（カメラへまっすぐ飛ぶ粒・止まった粒）
static const float kMinScreenDirectionSq = 1e-8f;


struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};


VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle particle = gParticles[instanceId];
    float4x4 world = particle.world;
    // world から中心位置とスケールを取り出す
    float3 center = float3(world._41, world._42, world._43);
    float sx = length(float3(world._11, world._12, world._13));
    float sy = length(float3(world._21, world._22, world._23));

    // --- 回す角度（Align To Velocity なら、進む向きを画面に写した向きの角度を足す）---
    // 画面の右・上（カメラの right・up）に写した長さが、画面の上での X・Y。atan2(Y, X) がその向きの角度（右が 0・左回りが正）
    float angle = particle.rotation;
    float2 screenDirection = float2(dot(particle.direction, gCamera.right), dot(particle.direction, gCamera.up));
    if (dot(screenDirection, screenDirection) > kMinScreenDirectionSq) {
        angle += atan2(screenDirection.y, screenDirection.x);
    }

    // --- 板の角を、大きさを掛けてから画面の上で回す（2D の回転：x' = x cos − y sin、y' = x sin + y cos）---
    // 回してから大きさを掛けると、横に伸ばした板（Stretch）が回った向きに伸びなくなるので、この順
    float2 corner = float2(input.position.x * sx, input.position.y * sy);
    float s;
    float c;
    sincos(angle, s, c);
    float2 rotated = float2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);

    // カメラ基底でクアッドを組む（回した角を、カメラの右・上に乗せる）
    float3 worldPos = center + gCamera.right * rotated.x
                             + gCamera.up * rotated.y;
    output.position = mul(float4(worldPos, 1.0f), gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.worldPosition = worldPos;
    return output;
}
#HLSL_END
```

### `Systems/ParticleSystem.cpp` の `ParticleSystem::Draw`（2か所）
① `white` の下（454行目あたり）
```cpp
// 前
		const Vector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
		instances.clear();
// 後
		const Vector4 white = {1.0f, 1.0f, 1.0f, 1.0f};
		const float stretch = (std::max)(component->stretch, 0.0f);
		instances.clear();
```

② 粒ごとの行列を作る所（463行目あたり）
```cpp
// 前
			// VS は world から中心（4行目）と大きさ（1・2行目の長さ）だけを読み、カメラの方へ向けて四角を組む
			Matrix4x4 world = MakeScaleMatrix({size, size, size});
			world.m[3][0] = position.x;
			world.m[3][1] = position.y;
			world.m[3][2] = position.z;
			instances.push_back({world, color});
// 後
			// --- 進む向き（Align To Velocity のときだけ。ワールドの向きで渡し、VS が画面に写して角度にする）---
			// 画面に写すのは VS（Scene ビューと Game ビューでカメラが違うので、ここでは角度にできない）
			Vector3 direction = {0.0f, 0.0f, 0.0f};
			if (component->alignToVelocity) {
				// Converge は速さを使わないので、出た所から中心への向き
				direction = component->converge ? (particle.center - particle.startPosition) : particle.velocity;
				if (isLocal) {
					direction = TransformNormal(direction, worldMatrix); // Entity から見た向き → ワールドの向き
				}
			}
			// VS は world から中心（4行目）と大きさ（1行目の長さ＝横・2行目の長さ＝縦）だけを読み、カメラの方へ向けて四角を組む
			Matrix4x4 world = MakeScaleMatrix({size * stretch, size, size});
			world.m[3][0] = position.x;
			world.m[3][1] = position.y;
			world.m[3][2] = position.z;
			instances.push_back({world, color, direction, particle.rotation});
```

### 解説
- **`emplace_back(instance)`**：`push_back` と違い、置き場所の中で直接作って、作った物の参照を返す（C++17 から）。丸ごと写してから色だけ書き換えるので、`ParticleData` に後で何か足しても、ここを直し忘れない（前の `{instance.world, …}` だと、足した値が 0 に戻って消える）。
- **`sincos(angle, s, c)`**：sin と cos を一度に出す HLSL の関数（`s = sin(angle)`、`c = cos(angle)`。別々に呼ぶより速いことがある）。出す先の変数は先に宣言しておく。
- **`atan2(y, x)`**：点 (x, y) の向きの角度（-π〜π。右が 0・上が π/2）。`atan(y / x)` と違い、x が 0 でも割らず、左向き（x が負）も正しく出る。
- **`TransformNormal`** は向きだけ変える（平行移動を足さない）。Entity の Scale も掛かるが、角度にしか使わないので長さは関係ない。
- `Stretch` は world の1行目（横）だけに掛ける。VS は1行目・2行目の長さを別々に読むので（前から `sx`・`sy` を分けていた）、ここを変えるだけで横長になる。

---

## 4.5 パート D：Animator の最初のフレームの Play（`Systems/AnimationSystem.cpp`）
パート A と同じ形。原因も同じ（`AnimationSystem::Update` も `WindowManager::UpdateAll` の中で、Script より後に回る）。Animator は `Stop`・`GetPlayer` が一覧に足さない（無ければ何もしない・nullptr）ので、足すのは `Play` だけ。

① `AnimationState` の `wasGameRunning` のコメントと、その下に関数を1つ足す（50行目あたり）
```cpp
// 前
	bool wasGameRunning = false; // 前のフレームでゲームが動いていたか
（…骨のギズモの3行…）
};
std::unique_ptr<AnimationState> state;
// 後
	bool wasGameRunning = false; // ゲームが動いているとして一覧を持っているか（Play / Stop の切り替わりで、一覧を捨てる。SwitchGameRunning）
（…骨のギズモの3行はそのまま…）
};
std::unique_ptr<AnimationState> state;

// ===== ゲームが動き出した・止まったら、一覧を捨てる（停止中の Preview Time のポーズをゲームに持ち込まない。Play On Start も、また最初から見る）=====
// 切り替わったときだけ捨てる（同じ値で何回呼んでもよい）
void SwitchGameRunning(bool isGameRunning) {
	if (isGameRunning == state->wasGameRunning) {
		return;
	}
	state->animators.clear();
	state->wasGameRunning = isGameRunning;
}
```

② `AnimationSystem::Update` の頭（163行目あたり）
```cpp
// 前
	// ===== 停止中 =====
	if (isGameRunning != state->wasGameRunning) {
		animators.clear();
		state->wasGameRunning = isGameRunning;
	}
// 後
	// ===== Play / Stop が切り替わったら、一覧を捨てる =====
	// ゲームが動き出した最初のフレームは、ここより先に Script が Play を呼んでいて、そこで切り替え済み（その分は捨てない）
	SwitchGameRunning(isGameRunning);
```
（その上の `std::vector<AnimatorState>& animators = state->animators;` は、下の `erase_if`・`ForEach` が使うので残す）

③ `AnimationSystem::Play` の assert の下（233行目あたり）
```cpp
// 前
bool AnimationSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	const AnimatorComponent* component = EntityManager::Get<AnimatorComponent>(entity);
// 後
bool AnimationSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	// Play を呼ぶのはゲーム（Script・SYSTEM。ゲームが動いている間だけ回る）なので、ここで切り替えておく
	// ゲームが動き出した最初のフレームは、Script が AnimationSystem::Update より先に回るので、切り替えないと後の Update がこの Play を捨てる
	SwitchGameRunning(true);
	const AnimatorComponent* component = EntityManager::Get<AnimatorComponent>(entity);
```

### 解説
- 前の動き：`Play` は true を返すのに、後の `Update` が一覧を捨てて、「初めて見つけた Animator」として作り直す。Play On Start を外した物は `Restart(…, false)` で**最初の時刻で止まる**（`IsPlaying` が false）。Play On Start ✓ の物は動くので気づきにくい。
- `SwitchGameRunning` は ParticleSystem.cpp と同じ名前・同じ形だが、どちらも無名の namespace の中なので、ぶつからない（それぞれのファイルの中だけの関数）。中で捨てる物（emitters／animators）が違うので、1つにまとめない。

---

## 5. 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| Particle System の Inspector | Over Lifetime と Renderer の間に **Rotation**（5つ）が出る。前に作った Particle System は全部 0・外れ・Stretch 1 で、見た目が前と同じ |
| Texture に `leaf.png`・Start Rotation **(90, 90)** で ▶ Preview | 葉が左へ 90 度倒れる（元の絵で左下にある柄が、右下へ） |
| Angular Velocity **(90, 90)** | 葉が左回りに、4 秒で1回転 |
| Start Rotation (-180, 180)・Angular Velocity (60, 120)・Random Spin Direction ✓ | 向きがばらばらで、右回り・左回りが混ざる |
| Texture 空・Shape Circle・Speed 3〜5・Gravity (0, -6, 0)・**Align To Velocity ✓・Stretch 6** | 輪から外へ飛ぶ細長い線になり、落ちるにつれて線も下へ曲がる。Scene ビューを回しても、いつも進む向きに沿う |
| Play On Start を外し、Script の `Start` で `ParticleSystem::Play` を呼ぶ（製品版・エディタの Play どちらも） | 出る（前は出なかった） |
| ゴール・ジャンプの円（Play On Start ✓・`Play` を後で呼ぶ）、Preview | 今までどおり |
| Animator の Play On Start を外し、Script の `Start` で `AnimationSystem::Play` を呼ぶ | 歩き出す（前は最初のポーズで止まっていた）。停止中の Preview Time のポーズも今までどおり |

![左：Start Rotation 0・真ん中：90（左回りに倒れる）・右：Angular Velocity 90 で回っている途中](images/particle_rotation_angle.png)

## 6. 困ったとき

| 症状 | 原因 |
|---|---|
| Release で粒が変な所に出る・色がおかしい・2個目から崩れる | Release が読む `AL4_01/project/MyEngine/Shader/Data/Particle/Particle.VS.shader` が古い（80 バイトずつ読んでいる）。エンジンをビルドし直す（PostBuildEvent で写る）か、手で写す |
| `static_assert` で止まる（96） | `ParticleData` の足し方が違う（`Vector3`＋`float` の順・型） |
| Converge の粒だけ回らない | パート B ③の1行を `continue` の後ろに書いた |
| Align To Velocity なのに向かない | Speed 0 で Gravity も 0（動かない粒は向きが無い）。Stretch 1 の丸いぼかしだと、向いても見た目が変わらない |
| 絵が 90 度ずれて進む | 絵の「前」が上向きに描いてある。Start Rotation を (-90, -90) にする（Align は絵の右を進む向きにする） |
| 最初のフレームの Play がまだ消える | パート A の `SwitchGameRunning(true);` を `Play` に足していない（`Stop`・`Emit` だけ足した）。Animator ならパート D の③ |
| `SwitchGameRunning` が見つからない | 関数を、使う所（Update・Play）より上に書いていない（無名の namespace の中・`state` の下に置く） |

## 7. この後（今回はやっていない）
- **Speed に合わせて伸ばす**（Unity の Stretched Billboard の Speed Scale）・**出始めのフェードイン**（Polish2 の E-2 に書いた「ふっと現れる」）。

## 8. ゲーム（AL4_01）でできるようになること
エンジンを写して AL4_01 のサブモジュールを進めた後、ゲームのチャットで：
- Polish2 の **風の粒を「筋」か「葉」にできる**（2026-10-27 に Polish2.md の 0章・E-2 を直した。値は作業用コピーで撮って決めた）。筋：Size 0.03〜0.05・Align To Velocity ✓・Stretch 14・Texture 空・Add。葉：`leaf.png`・Size 0.5〜0.8・Start Rotation (-180, 180)・Angular Velocity (90, 240)・Random Spin Direction ✓・Normal。足元の粒は丸いぼかしのまま。
- `WindParticles` は Emit の作りのままにする（風の Enabled に合わせて出す・止めるのが一番短く書ける。Polish2 のコメントもその理由に直した）。

## 作業用コピーで確かめたこと（2026-10-27）
- AL4_01（「Stage7まで」＋ Polish2 を写した物）とエンジン 2f9a621 の写しに、このページのコードを入れた。**Debug・Release・Development でビルドが通った**（エラー 0。このページで触ったファイルから警告 0）。
- **回転**：Level01 に、葉の絵を Start Rotation 0・90・Angular Velocity 90 で1枚ずつ出す物を置いて、製品版で撮った（上の絵）。90 は左回りに倒れ（柄が左下 → 右下）、Angular Velocity 90 の葉は 0.5 秒ほど後に撮った絵で左回りに進んでいた（角度は目で見ただけで、測っていない）。
- **Align To Velocity・Stretch**：Circle から飛ばして Gravity で落とす粒（Stretch 6）が、外向きの線になって落ちながら下へ曲がるのを撮った（下の絵の右）。
- **最初のフレームの Play**：Play On Start を外した葉の Particle System に、`Start` で `ParticleSystem::Play` を呼ぶだけのテスト用 Script を付けて製品版で起動した。パート A ありで葉が出た（下の絵の左）。**`Play` の `SwitchGameRunning(true);` だけを外してビルドし直すと、葉が1枚も出なかった**（前の動きが再現した）。

![パート A あり：左の葉（最初のフレームに Play）が出る。右は Align To Velocity・Stretch 6 の線](images/particle_rotation_play.png)
![パート A の Play の1行を外す：左の葉が出ない](images/particle_rotation_nofix.png)

- **Animator（パート D）**：エンジンの Test シーンの歩く人の1人を Play On Start を外し、`Start` で `AnimationSystem::Play` を呼んで、3・30 フレーム目に `IsPlaying` をログに出すテスト用 Script を付けた。パート D 無しは `Play` が true を返すのに 3・30 フレーム目とも **false**（再現）、パート D ありは両方 **true**。Debug・Release でビルドが通った。
- **エディタ（Inspector の Rotation の見た目・Preview・Scene ビューを回したときの Align・Animator の Preview Time）は見ていない**。上の表で確かめてほしい。
