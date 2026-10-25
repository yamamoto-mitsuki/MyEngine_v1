# G9c：シーンの切り替えの幕・ポーズ（Scene Transition・Time のポーズ）

参考：
- Unity の SceneManager.LoadScene：https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.LoadScene.html
- Unity の Time.timeScale（0 にしてポーズ。Update は呼ばれ続ける）：https://docs.unity3d.com/ScriptReference/Time-timeScale.html
- cpprefjp `std::numeric_limits`：https://cpprefjp.github.io/reference/limits/numeric_limits.html

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の 13章の **G9c**（G9 のレベル・UI の土台）。[G9bUIEditor.md](G9bUIEditor.md) の後（パートB の幕が G9b の `DrawTarget::isSceneHidden` を使う）。
- ユーザーの質問（設計の相談）への答えをコードにした：
  - **シーン切り替えのコードはどうするか** → 切り替えは今の `SceneManager::LoadScene` のまま。足りないのは**幕**（だんだん暗くする → 切り替える → だんだん明るくする）なので、エンジンに **`SceneTransition`** を作る。ゲームは `SceneManager::LoadScene` の代わりに `SceneTransition::LoadScene` を呼ぶ。
  - **ポーズ画面はどうやって作るか** → 時間は **`Time::SetGamePaused`** で止める（このファイル）。画面そのものは、次の **G9d の UI Screen**（ポーズ画面・クリア画面を作るエディタ）で作る（1.3）。
- **2026-10-25 に書き直した**：前の版にあった **Menu・Menu Item Component（パートC）とエディタでポーズ画面・タイトル画面を作る（パートE）はやめた**（ユーザー：「クオリティが低い・いらない」）。代わりに G9d で Screen を作る。
- **パートA・B は写経済み**（e300a7d「UIをマウスで移動できるように」。G9b と一緒にコミットされている）。残りは **パートC（ゲームの3か所）だけ**。
- 確かめた環境：本体の master（54a8a07）＋ G9b ＋ このファイルのエンジン（前の版。Menu 入り）、AL4_01 のコピー（G9a までの写経＋パートC）。Debug・Release でビルドし、Release で Level01 を動かして、画面で確かめた（5章）。

| パート | ファイル | 何をするか | 状態 |
|---|---|---|---|
| A | `Time/EngineTime.h`・`Scene/SceneManager.cpp` | ポーズ（時間を 0 にする・Script を呼ばない） | 写経済み（0章のコメント1つ） |
| B | `Scene/SceneTransition.h/.cpp`（新しく作る）・`ECS/GameComponent.h`・`Window/WindowManager.cpp`・`MyEngine_v1.vcxproj` | シーンを切り替えるときの幕 | 写経済み |
| C | AL4_01 の `IceCube.cpp`・`LevelInfo.cpp`・`GoalDoor.cpp` | 切り替えを幕付きにする（`SceneManager::` → `SceneTransition::`） | **これから** |

---

## 0. 前の写経の答え合わせ（e300a7d：G9b 全部と G9c のパートA・B）
答えのコードと比べた。**直すのは2つ**（1つは動きが変わる）。ほかは空行・コメントの位置・vcxproj の並びだけで、動きは同じ。

### 0.1 `MyEngine/Systems/PolygonShapeSystem.cpp`（`Draw`）：**表の面に目の印が付いていない**（G9b）
`config.isSceneHidden = ...` が、表の面の `DrawMesh` の**後**（横の面の前）に入っている。今のままだと、Hierarchy の目で隠しても **Polygon の表の面だけ Scene ビューに残る**（横の面は消える）。`config.camera = camera;` のすぐ下へ移す：
```cpp
		Renderer::MeshConfig config;
		PrimitiveRenderSystem::ApplyMaterial(config, renderer.material, environment);
		config.camera = camera;
		config.isSceneHidden = SceneVisibility::IsHidden(entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない
		Renderer::DrawMesh(config, cache.frontVertices, cache.frontIndices);
		const Vector4& color = renderer.material.color;
		const float shade = std::clamp(renderer.sideShade, 0.0f, 1.0f);
		config.color = ToColorCode({color.x * shade, color.y * shade, color.z * shade, color.w});
		Renderer::DrawMesh(config, cache.sideVertices, cache.sideIndices);
```
- `config` は表と横で同じ物を使い回している（横は色だけ変える）ので、最初に1回書けば両方に効く。

### 0.2 `MyEngine/Time/EngineTime.h`（`SetGamePaused` の上のコメント）：Menu の名前が残っている
Menu はやめたので、2行目を：
```cpp
	// ポーズ（ゲームのポーズ画面を開いている間）。止めている間は時間が0になり、Script の Update も呼ばれない
	// ポーズ画面（G9d の UI Screen）・幕（SceneTransition）は止めない時間（GetUnscaleDeltaTime）で動く。Play / Stop / Restart・シーンの切り替えで外れる
```

### 0.3 直さなくてよい所
- `SpriteRenderSystem.cpp` の `DrawSprites`：`isSceneHidden` を書く場所が if の後（答えは前）。`DrawSprite3d` の前なので同じ。
- `EngineTime.h` の一番上の説明（「…の3つが掛かる」）：答えは「ポーズ（ゲームのポーズ画面）の4つ」。直すなら：
```cpp
// 経過時間には「一時停止（エンジン）」「ゲームのスロー（ゲーム）」「ヒットストップ（ゲーム）」「ポーズ（ゲームのポーズ画面）」の4つが掛かる
```
- `SceneGizmo.cpp` の `HandleClick` の上のコメント（`canUseMouse` の説明）・`Renderer.h` の DrawTarget の説明・`EditorViewport.cpp` の「目で隠した物は Scene だけ描かない」のコメントが無い。あると後で読むときに分かりやすい（G9b の md のとおり）。

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 シーンの切り替え：幕はエンジンが持つ
今の切り替えは `SceneManager::LoadScene("Level02")`・`ReloadScene()`。呼ぶと、**次のフレームの頭**で Entity が全部作り直される。これに「幕」を足したい。

幕を **Entity**（画面いっぱいの黒い Image）にすると、困る：
- 切り替えると、幕の Entity も**一緒に消える**（新しいシーンには無い）→ 新しいシーンを明るくしていく物がいない。
- 全部のシーンに、同じ幕の Entity を置くことになる。

なので、幕は **Entity ではなく `SceneTransition`（エンジンの static なクラス）が持つ**。シーンを作り直しても消えない。

```
 フレーム     …        N            N+1             N+2 …
 幕の濃さ   0 → だんだん → 1        1（待つ）         だんだん → 0
 状態       Closing（閉じる）   閉じきった：          Switching      Opening（開く）
                                LoadScene を頼む  →  頭で切り替わる
```
- **Switching**（1フレーム待つ）：切り替えたフレームは、シーンの読み込みで時間が長くなる（`deltaTime` が上限の 0.1 秒まで伸びる）。そのフレームで開き始めると、一気に明るくなってしまうので、1フレームは進めない。
- 時間は**止めない時間**（`Time::GetUnscaleDeltaTime`）で数える。ポーズ中に「やりなおす」を選んでも、幕は動く。エディタの一時停止（Pause ボタン）の間は止める。
- **Stop** を押したら、幕を片付ける（暗いまま残さない）。
- 幕が動いている間（`IsBusy`）は、後から頼まれた切り替えを**断る**。ゴールした後に落ちて「やり直し」が重なる・2回決める、などを防ぐ。
- 幕は **Scene ビューには描かない**（G9b の `DrawTarget::isSceneHidden` を true にする）。再生中に Scene ビューで物を見ていても、暗くならない。
- 一番手前に描く：`order` を int の一番大きい値（`std::numeric_limits<int>::max()`：その型で表せる一番大きい値。`(std::numeric_limits<int>::max)()` と括弧で囲むのは、Windows.h の `max` マクロに置き換えられないようにするため）にする。

ゲームのコードは、`SceneManager::` を `SceneTransition::` に替えるだけ（パートC。呼び方・戻り値は同じ）。

### 1.2 ポーズ：時間を止めて、Script を呼ばない
`Time` には、今3つの倍率が掛かっている（一時停止・ゲームのスロー・ヒットストップ）。4つ目に **ポーズ**（`isGamePaused_`）を足す：

| | 誰が書く | 時間 | Script の Update | 使い道 |
|---|---|---|---|---|
| 一時停止（`pauseScale_`） | エディタの Pause ボタン（SceneManager） | 0 | 呼ばない | エディタで止めて見る |
| ゲームのスロー（`gameTimeScale_`） | ゲーム | 倍率 | 呼ぶ | スローモーション |
| ヒットストップ | ゲーム | 0（数秒） | 呼ぶ | 当たった瞬間に止める |
| **ポーズ（`isGamePaused_`）** | **ポーズ画面（G9d の UI Screen）** | **0** | **呼ばない**（G9d で「開いている UI Screen の中の Script だけ呼ぶ」にする） | **ポーズ画面** |

- 時間が 0 になる（`GetDeltaTime` が 0）ので、物理・アニメーション・Trigger Activator のフェード・SYSTEM は止まる。
- **Script の Update も呼ばない**（`SceneManager::Update`）。Unity は `Time.timeScale = 0` でも Update を呼び続けるので、Script に「ポーズ中なら return」を書く必要がある。ここでは、ポーズ画面の操作（Enter・Space・↑↓）でプレイヤーが跳んだり、R でやり直したりしないように、エンジンで止める。
- 幕（`SceneTransition`）・Text の演出（波・光）は、止めない時間で動くので止まらない。
- Play / Stop / Restart・シーンの切り替えで外れる（`ResetGameTime`。スロー・ヒットストップと同じ）。ポーズ中に「やりなおす」を選んだら、作り直したときに外れる。
- 今（G9c だけ）は、`SetGamePaused(true)` を呼ぶ物がまだ無い。G9d の UI Screen（Pause Game にチェック）が開いたときに呼ぶ。

### 1.3 ポーズ画面は G9d の UI Screen で作る（Menu をやめた理由）
前の版では、ポーズ画面を「シーンに置いた Text に **Menu Item** を付けて、**Menu** が ↑↓・Enter を受ける」形にしていた。やめた理由：
- 見た目の自由が少ない（選んだ項目は色と大きさが変わるだけ。出し方・消し方の動きも無い）。
- ポーズ画面を全部のレベルに Prefab で置く必要があり、直したら置き直しになる。

G9d では、ポーズ画面・クリア画面を **`resources/ui/〇〇.uiscreen`** のファイル（全シーン共通）にして、専用の編集モードで作る：
- 出し方・消し方（上下左右から滑り込む・フェード・拡大、秒数・イージング、要素ごとの遅れ）、出している間ゲームを止めるか。
- ゲームからは `UIScreen::Open("Pause")`・`Close`。項目を選ぶ動き（つづける・やりなおす）は**ゲームの Script** で書く（ポーズ中でも、開いている UI Screen の中の Script は呼ぶ）。

### 1.4 1フレームの中の順番
- `SceneTransition::Update` は、`TriggerActivatorSystem::Update` の後（WindowManager::UpdateAll の終わりの方）。同じフレームの Script（`SceneManager::Update`。その前）が頼んだ切り替えを受けて、幕を閉じ始める。
- 幕は、Text の描画の後に積む（`SceneTransition::Draw`。Order が一番大きいので、どの UI よりも手前）。

---

## 2. パートA：Time のポーズ（エンジン。写経済み）

### `MyEngine/Time/EngineTime.h`（6か所）
クラスの上の説明の2行目を：
```cpp
// 経過時間には「一時停止（エンジン）」「ゲームのスロー（ゲーム）」「ヒットストップ（ゲーム）」「ポーズ（ゲームのポーズ画面）」の4つが掛かる
```
`GetTimeScale` を（ポーズ中も 0）：
```cpp
	// 今掛かっている倍率（一時停止 × ゲームのスロー。ヒットストップ中・ポーズ中は0）
	static float GetTimeScale() {
		if (instance_->hitStopRemaining_ > 0.0f || instance_->isGamePaused_) {
			return 0.0f;
		}
		return instance_->pauseScale_ * instance_->gameTimeScale_;
	}
```
`IsHitStopping` の下（「エンジンから使う」の前）に：
```cpp
	// ポーズ（ゲームのポーズ画面を開いている間）。止めている間は時間が0になり、Script の Update も呼ばれない
	// ポーズ画面（G9d の UI Screen）・幕（SceneTransition）は止めない時間（GetUnscaleDeltaTime）で動く。Play / Stop / Restart・シーンの切り替えで外れる
	static void SetGamePaused(bool isPaused) { instance_->isGamePaused_ = isPaused; }
	static bool IsGamePaused() { return instance_->isGamePaused_; }
```
`SetDeltaTime` の頭を（ポーズ中はヒットストップを減らさない。コメントと if の条件）：
```cpp
	// フレームの頭に1回（Engine::BeginFrame）。ヒットストップの残りもここで減らす（一時停止・ポーズ中は減らさない）
	static void SetDeltaTime(float time) {
		instance_->deltaTime_ = time;
		if (instance_->pauseScale_ > 0.0f && !instance_->isGamePaused_ && instance_->hitStopRemaining_ > 0.0f) {
```
`ResetGameTime` を：
```cpp
	// ゲームのスロー・ヒットストップ・ポーズを戻す（SceneManager がシーンを作り直すときに呼ぶ）
	static void ResetGameTime() {
		instance_->gameTimeScale_ = 1.0f;
		instance_->hitStopRemaining_ = 0.0f;
		instance_->isGamePaused_ = false;
	}
```
メンバ変数の最後（`hitStopRemaining_` の下）に：
```cpp
	bool isGamePaused_ = false;     // ポーズ中か（ゲームのポーズ画面が書く）
```

### `MyEngine/Scene/SceneManager.cpp`（`Update` の isRunning）
```cpp
		// Script は一時停止中・ゲームのポーズ中は呼ばない（SYSTEM は時間0で回る）
		// ポーズ中に呼ぶと、ポーズ画面を選ぶキー（Enter・↑↓）でプレイヤーまで動いてしまうので
		const bool isRunning = (playState_ == PlayState::Playing) && !Time::IsGamePaused();
```

---

## 3. パートB：シーンを切り替える幕（エンジン。写経済み）

### `MyEngine/Scene/SceneTransition.h`（新しく作る）
```cpp
#pragma once
#include <string>

#include "MyEngine/Math/Vector4.h"


/// <summary>
/// シーンを切り替えるときの幕（画面をだんだん暗くする → 切り替える → だんだん明るくする）
/// <para>ゲームのコードは SceneManager::LoadScene・ReloadScene の代わりにこれを呼ぶ（呼び方は同じ。幕が付くだけ）</para>
/// <para>幕は Entity ではなく、ここが持つ（Entity にすると、シーンを切り替えたときに幕も消えてしまう）</para>
/// <para>時間は止めない時間（GetUnscaleDeltaTime）で数える（ポーズ中に「やり直す」を選んでも、幕は動く）</para>
/// </summary>
class SceneTransition {
public:
	static constexpr float kDefaultFadeTime = 0.4f; // 幕を閉じる・開くのにかける秒数（それぞれ）

	// ===== ゲームから使う =====
	// 幕を閉じてから、name のシーンへ切り替える（SceneManager::LoadScene と同じ。シーンが無い・停止中・幕が動いている間は false）
	static bool LoadScene(const std::string& name, float fadeTime = kDefaultFadeTime);
	// 幕を閉じてから、今のシーンを最初からやり直す（SceneManager::ReloadScene と同じ。停止中・幕が動いている間は false）
	static bool ReloadScene(float fadeTime = kDefaultFadeTime);
	// 幕が動いているか（閉じている・開いている途中）。ゴールした後にもう一度頼まない・入力を受けないのに使う
	static bool IsBusy();
	// 幕の色（初めは黒。a は使わない）
	static void SetColor(const Vector4& color);

	// ===== エンジンから使う =====
	// 毎フレーム（WindowManager::UpdateAll）。幕を進めて、閉じきったら切り替えを頼む。停止中は幕を片付ける
	static void Update(bool isGameRunning);
	// 幕を積む（WindowManager::DrawAll。UI より手前。Scene ビューには描かない）
	static void Draw();
};
```

### `MyEngine/Scene/SceneTransition.cpp`（新しく作る）
```cpp
#include "SceneTransition.h"

#include <algorithm>
#include <format>
#include <limits>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Math/Color.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Time/EngineTime.h"

namespace {
/// <summary>
/// 幕が今どうしているか
/// </summary>
enum class FadeState {
	None,      // 何もしていない（幕は見えない）
	Closing,   // 閉じている途中（だんだん暗くする）
	Switching, // 閉じきって、切り替えを頼んだ（切り替わるのは次のフレームの頭。そのフレームは暗いまま待つ）
	Opening,   // 開いている途中（切り替わった後、だんだん明るくする）
};

/// <summary>
/// 閉じきったら何をするか
/// </summary>
enum class SwitchType {
	Load,   // sceneName のシーンへ（SceneManager::LoadScene）
	Reload, // 今のシーンを最初から（SceneManager::ReloadScene）
};

/// <summary>
/// 幕の状態（Entity ではないので、シーンを切り替えても消えない）
/// </summary>
struct TransitionState {
	FadeState state = FadeState::None;
	SwitchType switchType = SwitchType::Load;
	std::string sceneName;                              // switchType が Load のとき：切り替えるシーン
	float fadeTime = SceneTransition::kDefaultFadeTime; // 閉じる・開くのにかける秒数（それぞれ）
	float alpha = 0.0f;                                 // 幕の濃さ（0 で見えない、1 で真っ暗）
	Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f};           // 幕の色（a は使わない。alpha を使う）
};
TransitionState transition;

// 幕を閉じ始めてよいか（再生中で、幕が動いていない）。what はログに出す名前
bool CanBegin(const std::string& what) {
	const SceneManager* main = SceneManager::GetMain();
	// 停止中はゲームのコードが動かないので、ふつうは来ない（エディタで開いているシーンを、勝手に替えないように）
	if (main == nullptr || main->GetPlayState() == PlayState::Editing) {
		LogManager::Log(std::format("SceneTransition::{}：停止中なので切り替えません（Play 中だけ効きます）", what));
		return false;
	}
	// 幕が動いている間は、後から頼まれた物を断る（ゴールした後に落ちて「やり直し」が重なる、など）
	return transition.state == FadeState::None;
}

// 幕を閉じ始める
void Begin(SwitchType switchType, const std::string& sceneName, float fadeTime) {
	transition.state = FadeState::Closing;
	transition.switchType = switchType;
	transition.sceneName = sceneName;
	transition.fadeTime = (std::max)(fadeTime, 0.0f);
}

// 閉じきった：切り替えを頼む（切り替わるのは、次のフレームの頭）
void RequestSwitch() {
	switch (transition.switchType) {
	case SwitchType::Load:
		SceneManager::LoadScene(transition.sceneName);
		break;
	case SwitchType::Reload:
		SceneManager::ReloadScene();
		break;
	}
}
} // namespace


//=============================================================================
// ゲームから使う
//=============================================================================
bool SceneTransition::LoadScene(const std::string& name, float fadeTime) {
	if (!CanBegin(std::format("LoadScene(\"{}\")", name))) {
		return false;
	}
	// シーンが無いのに閉じると、暗くなってから何も起きない（名前の打ち間違いは、閉じる前に断る）
	if (!SceneManager::HasScene(name)) {
		LogManager::Warning(std::format("SceneTransition::LoadScene(\"{}\")：シーンがありません", name));
		return false;
	}
	Begin(SwitchType::Load, name, fadeTime);
	return true;
}

bool SceneTransition::ReloadScene(float fadeTime) {
	if (!CanBegin("ReloadScene()")) {
		return false;
	}
	Begin(SwitchType::Reload, "", fadeTime);
	return true;
}

bool SceneTransition::IsBusy() { return transition.state != FadeState::None; }

void SceneTransition::SetColor(const Vector4& color) { transition.color = color; }

//=============================================================================
// 毎フレーム
//=============================================================================
void SceneTransition::Update(bool isGameRunning) {
	// 停止中：幕を片付ける（再生中に Stop を押したら、暗いまま残さない）
	if (!isGameRunning) {
		transition.state = FadeState::None;
		transition.alpha = 0.0f;
		return;
	}
	// エディタの一時停止中は、幕も止める
	const SceneManager* main = SceneManager::GetMain();
	if (main != nullptr && main->GetPlayState() == PlayState::Paused) {
		return;
	}
	// 1フレームに変える濃さ（濃さ 1 を fadeTime 秒で変えきる。0 秒なら、すぐ変えきる）
	// 止めない時間で数える（ポーズ中に「やり直す」を選んだときも、幕は動く）
	const float step = (transition.fadeTime > 0.0f) ? Time::GetUnscaleDeltaTime() / transition.fadeTime : 1.0f;
	switch (transition.state) {
	case FadeState::None:
		break;
	case FadeState::Closing:
		transition.alpha = (std::min)(transition.alpha + step, 1.0f);
		if (transition.alpha >= 1.0f) {
			RequestSwitch();
			transition.state = FadeState::Switching;
		}
		break;
	case FadeState::Switching:
		// このフレームの頭で切り替わった。読み込みで長くなったフレームの時間で一気に明るくしないよう、ここでは進めない
		transition.state = FadeState::Opening;
		break;
	case FadeState::Opening:
		transition.alpha = (std::max)(transition.alpha - step, 0.0f);
		if (transition.alpha <= 0.0f) {
			transition.state = FadeState::None;
		}
		break;
	}
}

//=============================================================================
// 描く
//=============================================================================
void SceneTransition::Draw() {
	if (transition.alpha <= 0.0f) {
		return;
	}
	// 画面全体の四角（正規化座標の -1〜1）。画像は無し＝白に色を掛ける
	Renderer::Quad2dConfig config;
	config.lb = {-1.0f, -1.0f};
	config.lt = {-1.0f, 1.0f};
	config.rb = {1.0f, -1.0f};
	config.rt = {1.0f, 1.0f};
	config.color = ToColorCode({transition.color.x, transition.color.y, transition.color.z, transition.alpha});
	config.blendMode = BlendMode::Normal;
	config.rasterizerType = RasterizerType::SolidNone;
	config.order = (std::numeric_limits<int>::max)(); // 一番手前（どの UI の Order よりも大きい）
	config.isSceneHidden = true; // Scene ビューには描かない（再生中でも、置いた物が見えるように。Game ビューだけ暗くなる）
	Renderer::DrawQuad2d(config);
}
```
- `CanBegin` の `what` は、ログに「どの関数から呼ばれたか」を出すため。
- `Switching` を `Opening` に変えるのは「切り替えを頼んだ次のフレーム」。WindowManager の順番で、`SceneManager::Update`（フレームの頭で切り替える）の後に `SceneTransition::Update` が来るので、そのときには、もう新しいシーンになっている。
- `RequestSwitch` の `SceneManager::LoadScene` は、名前を `LoadScene` で調べてあるので、ここで失敗するのは、閉じている間にシーンファイルが消されたときくらい（そのときも、幕は開く＝暗いまま止まらない）。

### `MyEngine/ECS/GameComponent.h`（1つ足す）
`#include "MyEngine/Scene/SceneManager.h"` の下に（Script・COMPONENT から、include を足さずに使えるように）：
```cpp
#include "MyEngine/Scene/SceneTransition.h"
```

### `MyEngine/Window/WindowManager.cpp`（3か所）
include（`TriggerActivatorSystem.h` の下）：
```cpp
#include "MyEngine/Scene/SceneTransition.h"
```
`UpdateAll` の `TriggerActivatorSystem::Update(...)` の下に：
```cpp
	SceneTransition::Update(isGameRunning); // シーンを切り替える幕を進める（閉じきったら切り替えを頼む。切り替わるのは次のフレームの頭）
```
`DrawAll` の `TextRenderSystem::Draw();` の下に：
```cpp
	SceneTransition::Draw();  // シーンを切り替える幕（一番手前。Game ビューだけ）
```

### `MyEngine_v1.vcxproj`
```
    <ClCompile Include="MyEngine\Scene\SceneTransition.cpp" />
    <ClInclude Include="MyEngine\Scene\SceneTransition.h" />
```

---

## 4. パートC：ゲーム（AL4_01）の切り替えを幕付きにする（これから写す）
`SceneManager::` を `SceneTransition::` に替えるだけ（include は GameComponent.h に足したので要らない）。

### `Source/Scripts/IceCube.cpp`（`UpdateDead` の最後）
```cpp
		SceneTransition::ReloadScene();
```

### `Source/Scripts/LevelInfo.cpp`（2か所）
`Update`（R・Back でやり直す）：
```cpp
		SceneTransition::ReloadScene();
```
`LoadNextLevel`：
```cpp
	if (nextScene_.empty() || !SceneTransition::LoadScene(nextScene_)) {
		SceneTransition::ReloadScene();
```
- 幕が動いている間に R を押しても、`ReloadScene` が断るので、やり直しは重ならない。

### `Source/Scripts/GoalDoor.cpp`（`Update` の LevelInfo が無いとき）
```cpp
		SceneTransition::ReloadScene();
```

---

## 5. 確かめたこと
前の版（Menu 入り）で、Release（エディタ無し）の Level01 のコピーにキーの入力を時間で流して（テストのときだけ InputManager に入れた仕掛け。外した）、0.5 秒ごとに画面を撮った。今の版に残っている所（幕・ポーズの時間）の結果：

| 確かめたこと | 結果 |
|---|---|
| ポーズを開く・閉じる（`SetGamePaused` の入り切り） | 開いて閉じた後も、ゲームは続きから動く |
| ポーズ中に「やりなおす」（`SceneTransition::ReloadScene`。R キーと同じ関数） | ポーズのまま幕がだんだん暗くなる（止めない時間で動く）→ 最初から（ポーズが外れる）→ だんだん明るくなる |

確かめていないこと：
- 今の版（Menu 無し）は、ポーズを開く物がまだ無いので、ポーズは G9d の UI Screen で確かめ直す。
- エディタの Play 中に、ゴール・落下・R で幕が出ること（パートC を写したら、Play で見てほしい）。
- Play 中に Stop を押したとき、幕が片付くこと（`Update` の `!isGameRunning` の所）。

---

## 6. 決めたこと・後でやること
- 幕は**黒**（`SceneTransition::SetColor` で変えられる）。白くフラッシュ・横から閉じる（ワイプ）・画像の形で閉じる（マスク）などは、要るなら `Draw` を変える（提出の後）。
- ポーズ中も **BGM は鳴ったまま**（止めたいなら、G9d の UI Screen が開いたときに `AudioSystem::SetPaused` を呼ぶ。エディタの Pause と同じ関数なので、ぶつからないか見てから）。
- ポーズ画面・クリア画面・タイトル画面は **G9d の UI Screen** で作る。
