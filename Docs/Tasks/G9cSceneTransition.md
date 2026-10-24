# G9c：シーンの切り替えの幕・ポーズ画面（Scene Transition・Menu）

参考：
- Unity の SceneManager.LoadScene：https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.LoadScene.html
- Unity の Time.timeScale（0 にしてポーズ。Update は呼ばれ続ける）：https://docs.unity3d.com/ScriptReference/Time-timeScale.html
- Unity の Button と、キー・パッドで選ぶ仕組み（Navigation）：https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/script-Button.html
- cpprefjp `std::numeric_limits`：https://cpprefjp.github.io/reference/limits/numeric_limits.html
- Microsoft Learn `PostMessage`（メッセージを積んで、すぐ戻る）：https://learn.microsoft.com/ja-jp/windows/win32/api/winuser/nf-winuser-postmessagew
- Microsoft Learn `WM_CLOSE`（× を押したのと同じ知らせ）：https://learn.microsoft.com/ja-jp/windows/win32/winmsg/wm-close

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の 13章の **G9c**（G9 のレベル・UI の土台）。**[G9bUIEditor.md](G9bUIEditor.md) の後に写す**（パートB の幕が G9b の `DrawTarget::isSceneHidden` を、パートC の `MenuSystem` が G9b の `SpriteRenderSystem::CalcScreenQuad`・`IsInsideQuad` を使う）。
- ユーザーの質問（設計の相談）への答えをコードにした：
  - **シーン切り替えのコードはどうするか** → 切り替えは今の `SceneManager::LoadScene` のまま。足りないのは**幕**（だんだん暗くする → 切り替える → だんだん明るくする）なので、エンジンに **`SceneTransition`** を作る。ゲームは `SceneManager::LoadScene` の代わりに `SceneTransition::LoadScene` を呼ぶ。
  - **ポーズ画面はどうやって作るか**（必ず Text を使うが、どう設計するか） → 見た目は**普通の Text・Image の Entity**（エディタで置く）。動き（Esc で開く・↑↓で選ぶ・決める・ゲームを止める）は、エンジンの **`Menu`・`Menu Item` Component** が受け持つ。時間は **`Time::SetGamePaused`** で止める。
  - **UI Editor は新しい View か、Scene ビューに重ねるか** → **Scene ビューに重ねる**（G9b で作った。理由は 1.5）。
- **パートの順番**：A（Time のポーズ）→ B（SceneTransition）→ C（Menu）→ D（ゲームの3か所）→ E（エディタでポーズ画面・タイトル画面を作る）。A・B・C はそれぞれでビルドが通る。D は B の後。
- 確かめた環境：本体の master（54a8a07）＋ G9b ＋ このファイルのエンジン、AL4_01 のコピー（G9a までの写経＋パートD）。Debug・Release でビルドし、Release でポーズ画面を置いた Level01 を動かして、画面で確かめた（7章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Time/EngineTime.h`・`Scene/SceneManager.cpp` | ポーズ（時間を 0 にする・Script を呼ばない） |
| B | `Scene/SceneTransition.h/.cpp`（新しく作る）・`ECS/GameComponent.h`・`Window/WindowManager.cpp`・`MyEngine_v1.vcxproj` | シーンを切り替えるときの幕 |
| C | `Components/MenuComponent.h`・`Systems/MenuSystem.h/.cpp`（新しく作る）・`Engine.cpp`・`Window/WindowManager.cpp`・`HierarchyWindow.cpp`・`MyEngine_v1.vcxproj` | メニュー（選ぶ・決める・ポーズを開く）。＋ → Menu・Menu Item |
| D | AL4_01 の `IceCube.cpp`・`LevelInfo.cpp`・`GoalDoor.cpp` | 切り替えを幕付きにする（`SceneManager::` → `SceneTransition::`） |
| E | エディタで | ポーズ画面（Prefab）・タイトル画面 |

---

## 0. 前の写経の答え合わせ
G9a は [G9bUIEditor.md](G9bUIEditor.md) の 0章（写し違い無し）。G9b はまだ写していないので、無し。

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

ゲームのコードは、`SceneManager::` を `SceneTransition::` に替えるだけ（パートD。呼び方・戻り値は同じ）。

### 1.2 ポーズ：時間を止めて、Script を呼ばない
`Time` には、今3つの倍率が掛かっている（一時停止・ゲームのスロー・ヒットストップ）。4つ目に **ポーズ**（`isGamePaused_`）を足す：

| | 誰が書く | 時間 | Script の Update | 使い道 |
|---|---|---|---|---|
| 一時停止（`pauseScale_`） | エディタの Pause ボタン（SceneManager） | 0 | 呼ばない | エディタで止めて見る |
| ゲームのスロー（`gameTimeScale_`） | ゲーム | 倍率 | 呼ぶ | スローモーション |
| ヒットストップ | ゲーム | 0（数秒） | 呼ぶ | 当たった瞬間に止める |
| **ポーズ（`isGamePaused_`）** | **Menu（PauseKey）** | **0** | **呼ばない** | **ポーズ画面** |

- 時間が 0 になる（`GetDeltaTime` が 0）ので、物理・アニメーション・Trigger Activator のフェード・SYSTEM は止まる。
- **Script の Update も呼ばない**（`SceneManager::Update`）。Unity は `Time.timeScale = 0` でも Update を呼び続けるので、Script に「ポーズ中なら return」を書く必要がある。ここでは、ポーズ画面の操作（Enter・Space・↑↓）でプレイヤーが跳んだり、R でやり直したりしないように、エンジンで止める。
- ポーズ画面（`MenuSystem`）・幕（`SceneTransition`）・Text の演出（波・光）は、止めない時間で動くので止まらない。
- Play / Stop / Restart・シーンの切り替えで外れる（`ResetGameTime`。スロー・ヒットストップと同じ）。ポーズ中に「やりなおす」を選んだら、作り直したときに外れる。

### 1.3 ポーズ画面の作り：Text を Menu Item にして並べる
ポーズ画面は、**普通の Text・Image の Entity** でエディタで作る。動きは Component が受け持つ：

```
PauseMenu        ← Menu（Open Mode：PauseKey）。再生を始めたら隠れる
 ├ Dim           ← Image（黒・a 0.6・1280×720。Order 100）。後ろのゲームを暗く
 ├ Title         ← Text「ポーズ」（Order 101）
 ├ Resume        ← Text「つづける」   ＋ Menu Item（Action：Resume）
 ├ Restart       ← Text「やりなおす」 ＋ Menu Item（Action：Restart）
 └ ToTitle       ← Text「タイトルへ」 ＋ Menu Item（Action：LoadScene、Scene：Title）
```

- **Menu**（入れ物）：子（孫も）の Menu Item を、**Hierarchy の並びの順**に、上から1つ目・2つ目…の項目にする。選んでいる項目は、色（Selected Color）と大きさ（Selected Scale）を変える。
- **Menu Item**（項目1つ。Unity の Button）：同じ Entity の Text（無ければ Image）が見た目。決めたときにすること（Action）を持つ。
- 操作：

| したいこと | キーボード | パッド | マウス |
|---|---|---|---|
| ポーズを開く・閉じる | Esc | Start（閉じるのは B も） | |
| 選ぶ | ↑↓・W S（端から反対の端へ回る） | 十字・左スティック | 項目に乗せる（動かしたときだけ） |
| 決める | Enter・Space | A | 項目をクリック |

- マウスは**動かしたときだけ**、乗っている項目を選ぶ。止まっているマウスが、キーで選んだ項目を毎フレーム戻さないように。
- マウスが項目の上かは、G9b の `SpriteRenderSystem::CalcScreenQuad`（Text の文字のかたまりの四角）と `IsInsideQuad` で調べる（Image は今ある `IsMouseOver`）。
- **PauseKey** のメニューは、再生を始めたら隠れる（Trigger Activator の Hide On Start と同じ。エディタでは見えたまま置ける。重なって邪魔なら G9b の目で隠す）。Esc で開くと、Menu の Entity を有効にして `Time::SetGamePaused(true)`。開いた・閉じたフレームは、選ぶ・決めるをしない（Esc と Enter を同じフレームに押しても、決めない）。
- **Always** のメニューは、いつも開いている（タイトル画面・クリア画面）。

**なぜ Script ではなく Component（エンジン）にしたか**：
- **Script はポーズ中に呼ばれない**（1.2）ので、Script に書くと、閉じるための Esc を受けられない。エンジンの System なら、止めない時間で動かせる。
- タイトル画面・クリア画面・ポーズ画面が、同じ Component で作れる（Trigger Activator と同じ考え。Script を書かずにエディタで置ける）。
- 全部のレベルに同じポーズ画面を置くのは、**Prefab** にして置く（6章）。

項目を決めたときにすること（Action）：

| Action | すること |
|---|---|
| None | 何もしない |
| Resume | 閉じて、ゲームに戻る（PauseKey のメニューだけ） |
| Restart | 今のシーンを最初から（`SceneTransition::ReloadScene`。ポーズのまま幕を閉じる） |
| LoadScene | Scene に書いたシーンへ（`SceneTransition::LoadScene`） |
| Quit | 終わる。エディタでは再生を止める（エディタは閉じない）。製品版はウィンドウに `WM_CLOSE` を送る（× を押したのと同じ） |

- `PostMessage`：ウィンドウにメッセージを**積むだけ**で、すぐ戻る（`SendMessage` は相手が処理し終わるまで待つ）。積んだ `WM_CLOSE` は、次のフレームの `ProcessMessage` で処理され、ウィンドウが閉じて、いつもの終わり方（`Finalize`）で終わる。

### 1.4 選んだ項目を大きくする：元の大きさを覚えておく
選んだ項目の Transform の Scale を、Selected Scale 倍にする。毎フレーム「今の Scale × 1.15」にすると、どんどん大きくなるので、**再生を始めた最初のフレームに、元の Scale を覚えておく**（`MenuItemComponent::baseScale`。実行中だけの値で、保存しない）。選んでいれば `baseScale × Selected Scale`、いなければ `baseScale` を書く。Stop すると、Play を押した瞬間の状態に戻る。

### 1.5 UI Editor は「新しい View」か「Scene ビューに重ねる」か（質問の答え）
**Scene ビューに重ねる**ことにした（G9b の UIGizmo）。

| | 新しい View（UI だけを編集する窓） | **Scene ビューに重ねる（G9b）** |
|---|---|---|
| 作る物 | 描き先（RenderTexture）・カメラ・ドッキングの窓をもう1組 | クリック・ドラッグの処理だけ（今の Scene ビューの絵を使う） |
| 3D との重なり | 見えない（文字が足場にかぶるか分からない） | **見える** |
| UI の位置 | 同じ（画面のピクセル） | 同じ。UI は Scene ビューにも Game ビューにも、画面の同じ所に重なっているので、そのまま選べる |
| Unity | | 同じ（Scene ビューで UI を編集する。2D ボタンで見やすくするだけ） |

UI が多くて 3D の物が選びにくいときは、View → Scene View → **Pick UI** を外す・**目**で UI を隠す（G9b）。

### 1.6 1フレームの中の順番
- `MenuSystem::Update`・`SceneTransition::Update` は、`TriggerActivatorSystem::Update` の後（WindowManager::UpdateAll の終わりの方）。Menu が決めた切り替えを、同じフレームの SceneTransition が受けて、幕を閉じ始める。
- Script の Update（`SceneManager::Update`）は、その前。Esc を押したフレームは、Script がもう動いた後に、Menu が開いてポーズにする（次のフレームから Script が止まる）。
- 幕は、Text の描画の後に積む（`SceneTransition::Draw`。Order が一番大きいので、どの UI よりも手前）。

---

## 2. パートA：Time のポーズ（エンジン）

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
	// ポーズ画面（Menu Component）は止めない時間（GetUnscaleDeltaTime）で動く。Play / Stop / Restart・シーンの切り替えで外れる
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

## 3. パートB：シーンを切り替える幕（エンジン）

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

## 4. パートC：メニュー（エンジン）

### `MyEngine/Components/MenuComponent.h`（新しく作る）
```cpp
#pragma once
#include <type_traits>

#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/String/FixedString.h"

// ゲームのメニュー（タイトル画面の「はじめる・おわる」、ポーズ画面の「つづける・やりなおす・タイトルへ」）。Entityに付けるComponent（カテゴリ：UI）
// 項目は、子（孫も）に付けた Menu Item（MenuItemComponent）。Hierarchy の並びの順に、上から1つ目・2つ目…になる
// 項目の見た目は、同じ Entity の Text（無ければ Image）。選んでいる項目は、Menu が色と大きさを変える
// 操作：↑↓・W S・パッドの十字・左スティックで選ぶ / Enter・Space・パッドの A で決める / マウスを乗せると選び、クリックで決める
// Open Mode を PauseKey にすると、再生を始めたときに隠れ、Esc・パッドの Start で開く・閉じる（開いている間はゲームを止める＝Time::SetGamePaused）
// 動かすのは MenuSystem（再生中だけ。止めない時間で動くので、ゲームを止めていても選べる）

/// <summary>
/// メニューをいつ開くか
/// </summary>
enum class MenuOpenMode {
	Always,   // いつも開いている（タイトル画面・クリア画面）
	PauseKey, // Esc・パッドの Start で開く・閉じる。開いている間はゲームを止める（ポーズ画面）
};

/// <summary>
/// 項目を決めたときにすること
/// </summary>
enum class MenuAction {
	None,      // 何もしない
	Resume,    // メニューを閉じて、ゲームに戻る（PauseKey のメニューだけ）
	Restart,   // 今のシーンを最初から（SceneTransition::ReloadScene）
	LoadScene, // Scene に書いたシーンへ（SceneTransition::LoadScene）
	Quit,      // ゲームを終わる（エディタでは、再生を止める）
};


/// <summary>
/// メニュー（項目を選んで決める入れ物。Unity なら、Button を並べた Canvas と、それを動かす Script）
/// </summary>
struct MenuComponent {
	bool enabled = true;                                // false の間は操作を受けない
	MenuOpenMode openMode = MenuOpenMode::Always;       // いつ開くか
	Vector4 normalColor = {1.0f, 1.0f, 1.0f, 1.0f};     // 選んでいない項目の色（Text・Image の Color に書く）
	Vector4 selectedColor = {1.0f, 0.85f, 0.3f, 1.0f};  // 選んでいる項目の色
	float selectedScale = 1.15f;                        // 選んでいる項目の大きさの倍率（Transform の Scale に掛ける）

	// --- 実行中だけの値（MenuSystem が書く。保存しない）---
	bool isStarted = false;               // 再生を始めたときの準備（PauseKey なら隠す）をしたか
	bool isOpen = false;                  // 開いているか
	int selected = 0;                     // 選んでいる項目（0 が一番上）
	int stickDirection = 0;               // 前のフレームの左スティックの向き（-1 上・0 真ん中・1 下。倒した瞬間だけ1つ動かす）
	Vector2 lastMouse = {0.0f, 0.0f};     // 前のフレームのマウスの位置（動かしたときだけ、乗っている項目を選ぶ。キーで選んだ後に戻されないように）
};

static_assert(std::is_trivially_copyable_v<MenuComponent>);


/// <summary>
/// メニューの項目1つ（Unity の Button）。Text（か Image）と同じ Entity に付けて、Menu の子に置く
/// </summary>
struct MenuItemComponent {
	bool enabled = true;                   // false の項目は飛ばす（選べない）
	MenuAction action = MenuAction::None;  // 決めたときにすること
	FixedString<64> scene = "";            // action が LoadScene のとき：切り替えるシーンの名前（Title・Level01 など）

	// --- 実行中だけの値（MenuSystem が書く。保存しない）---
	bool isStarted = false;              // 再生を始めたときの Scale を覚えたか
	Vector3 baseScale = {1.0f, 1.0f, 1.0f}; // 再生を始めたときの Transform の Scale（選んだときは、これに Selected Scale を掛ける）
};

static_assert(std::is_trivially_copyable_v<MenuItemComponent>);
```

### `MyEngine/Systems/MenuSystem.h`（新しく作る）
```cpp
#pragma once
#include "MyEngine/Components/MenuComponent.h"


/// <summary>
/// Menu（項目を選んで決める）を動かす。タイトル画面・ポーズ画面に使う
/// <para>再生中だけ動く。止めない時間で動くので、ポーズ中（Time::IsGamePaused）でも選べる</para>
/// <para>決めたときの切り替えは SceneTransition（幕を閉じてから切り替える）</para>
/// </summary>
class MenuSystem {
public:
	// Menu・Menu Item を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	// 毎フレーム（WindowManager::UpdateAll。当たり判定の後）。停止中は何もしない
	static void Update(bool isGameRunning);
};
```

### `MyEngine/Systems/MenuSystem.cpp`（新しく作る）
```cpp
#include "MenuSystem.h"

#include <memory>
#include <string>
#include <vector>

#include <Windows.h>

#include "MyEngine/Components/ImageComponent.h"
#include "MyEngine/Components/TextComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Engine.h"
#include "MyEngine/Input/InputManager.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Scene/SceneTransition.h"
#include "MyEngine/Systems/SpriteRenderSystem.h"
#include "MyEngine/Systems/TextRenderSystem.h"
#include "MyEngine/Time/EngineTime.h"

namespace {
constexpr float kStickThreshold = 0.5f; // 左スティックをこれより倒したら、上・下に1つ動かす

/// <summary>
/// MenuSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct MenuState {
	std::vector<Handle<Entity>> items;    // 今のメニューの項目（毎フレーム集め直す。配列のメモリは使い回す）
	std::vector<Handle<Entity>> children; // 子を集める作業用
};
std::unique_ptr<MenuState> state;

//=============================================================================
// 項目を集める
//=============================================================================
// parent の子・孫…を Hierarchy の並びの順にたどって、選べる Menu Item を out に足す（無効な Entity の下は見ない）
void CollectItems(Handle<Entity> parent, std::vector<Handle<Entity>>& out) {
	EntityManager::GetChildren(parent, state->children);
	const std::vector<Handle<Entity>> children = state->children; // 下の呼び出しが state->children を書き換えるので、写してから回す
	for (const Handle<Entity> child : children) {
		if (!EntityManager::IsActiveInHierarchy(child)) {
			continue;
		}
		if (const MenuItemComponent* item = EntityManager::Get<MenuItemComponent>(child); item != nullptr && item->enabled) {
			out.push_back(child);
		}
		CollectItems(child, out); // 孫も（項目を Items などの空の Entity にまとめてもよいように）
	}
}

//=============================================================================
// 入力
//=============================================================================
// 上下に何個動かすか（上が -1、下が +1。押した瞬間だけ。スティックは倒した瞬間だけ）
int ReadMove(MenuComponent& menu) {
	int move = 0;
	if (InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsButtonTriggered(GamepadButton::DpadUp)) {
		--move;
	}
	if (InputManager::IsKeyTriggered(DIK_DOWN) || InputManager::IsKeyTriggered(DIK_S) || InputManager::IsButtonTriggered(GamepadButton::DpadDown)) {
		++move;
	}
	// スティックは上が＋なので、上に倒したら -1（上の項目へ）
	const float stickY = InputManager::GetLeftStickY();
	const int stickDirection = (stickY > kStickThreshold) ? -1 : ((stickY < -kStickThreshold) ? 1 : 0);
	if (stickDirection != 0 && stickDirection != menu.stickDirection) {
		move += stickDirection;
	}
	menu.stickDirection = stickDirection;
	return move;
}

// 決めるキーを押したか（Enter・Space・パッドの A）
bool IsDecideTriggered() {
	return InputManager::IsKeyTriggered(DIK_RETURN) || InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A);
}

// ポーズのキーを押したか（Esc・パッドの Start）
bool IsPauseTriggered() { return InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start); }

// マウスが項目の上にあるか（Text なら文字のかたまり、無ければ Image の四角）
bool IsMouseOverItem(Handle<Entity> item) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(item);
	if (transform == nullptr || !InputManager::IsMouseInGame()) {
		return false;
	}
	const Vector2 mouse = InputManager::GetMousePositionInGame();
	if (const TextComponent* text = EntityManager::Get<TextComponent>(item); text != nullptr && text->enabled) {
		const Vector2 size = TextRenderSystem::CalcTextSize(item);
		return SpriteRenderSystem::IsInsideQuad(mouse, SpriteRenderSystem::CalcScreenQuad(transform->worldMatrix, text->anchor, text->pivot, size));
	}
	return SpriteRenderSystem::IsMouseOver(item); // Image（無ければ false）
}

// マウスが乗っている項目の番号（どれにも乗っていなければ -1）
int FindHoveredItem(const std::vector<Handle<Entity>>& items) {
	for (size_t i = 0; i < items.size(); ++i) {
		if (IsMouseOverItem(items[i])) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

//=============================================================================
// 開く・閉じる・決める
//=============================================================================
// PauseKey のメニューを開く・閉じる（メニューの Entity を有効・無効にして、ゲームを止める・動かす）
void SetOpen(MenuComponent& menu, Entity& owner, bool isOpen) {
	menu.isOpen = isOpen;
	owner.isActive = isOpen;
	Time::SetGamePaused(isOpen);
	if (isOpen) {
		menu.selected = 0; // 開くたびに一番上から
	}
}

// ゲームを終わる。エディタでは再生を止めるだけ（エディタは閉じない）、製品版ではウィンドウを閉じる（× を押したのと同じ）
void QuitGame() {
#ifdef USE_IMGUI
	if (SceneManager* main = SceneManager::GetMain()) {
		main->Stop();
	}
#else
	PostMessage(Engine::GetWindowManager()->GetMainHWND(), WM_CLOSE, 0, 0);
#endif
}

// 項目を決めた
void Decide(MenuComponent& menu, Entity& owner, const MenuItemComponent& item) {
	switch (item.action) {
	case MenuAction::None:
		break;
	case MenuAction::Resume:
		if (menu.openMode == MenuOpenMode::PauseKey) {
			SetOpen(menu, owner, false);
		}
		break;
	case MenuAction::Restart:
		// ゲームは止めたまま幕を閉じる（作り直すと、ポーズは Time::ResetGameTime で外れる）
		SceneTransition::ReloadScene();
		break;
	case MenuAction::LoadScene:
		SceneTransition::LoadScene(item.scene.ToString());
		break;
	case MenuAction::Quit:
		QuitGame();
		break;
	}
}

// 選んでいる項目は Selected Color・Selected Scale、ほかは Normal Color・元の大きさにする
void ApplyLook(const MenuComponent& menu, const std::vector<Handle<Entity>>& items) {
	for (size_t i = 0; i < items.size(); ++i) {
		const bool isSelected = (static_cast<int>(i) == menu.selected);
		const Vector4& color = isSelected ? menu.selectedColor : menu.normalColor;
		if (TextComponent* text = EntityManager::Get<TextComponent>(items[i])) {
			text->color = color;
		} else if (ImageComponent* image = EntityManager::Get<ImageComponent>(items[i])) {
			image->color = color;
		}
		const MenuItemComponent* item = EntityManager::Get<MenuItemComponent>(items[i]);
		TransformComponent* transform = EntityManager::Get<TransformComponent>(items[i]);
		if (item != nullptr && transform != nullptr) {
			transform->scale = item->baseScale * (isSelected ? menu.selectedScale : 1.0f);
		}
	}
}

// メニュー1つ分の毎フレーム
void UpdateMenu(Handle<Entity> entity, MenuComponent& menu) {
	Entity* owner = EntityManager::Get(entity);
	if (owner == nullptr) {
		return;
	}
	// --- 再生を始めた最初のフレーム：PauseKey なら隠す（エディタでは見えたまま置ける）---
	if (!menu.isStarted) {
		menu.isStarted = true;
		menu.isOpen = (menu.openMode == MenuOpenMode::Always);
		if (menu.openMode == MenuOpenMode::PauseKey) {
			owner->isActive = false;
		}
		menu.lastMouse = InputManager::GetMousePositionInGame();
	}
	// 幕が動いている間は操作を受けない（決めた後に、もう一度決めない）
	if (!menu.enabled || SceneTransition::IsBusy()) {
		return;
	}
	// --- PauseKey：Esc・Start で開く・閉じる。パッドの B でも閉じる（開いた・閉じたフレームは、選ぶ・決めるをしない）---
	if (menu.openMode == MenuOpenMode::PauseKey) {
		const bool isCloseByB = menu.isOpen && InputManager::IsButtonTriggered(GamepadButton::B);
		if (IsPauseTriggered() || isCloseByB) {
			SetOpen(menu, *owner, !menu.isOpen);
			return;
		}
	}
	if (!menu.isOpen || !EntityManager::IsActiveInHierarchy(entity)) {
		return;
	}
	// --- 項目を集める（Hierarchy の並びの順）---
	std::vector<Handle<Entity>>& items = state->items;
	items.clear();
	CollectItems(entity, items);
	if (items.empty()) {
		return;
	}
	const int count = static_cast<int>(items.size());
	// --- 選ぶ：キー・スティック（端まで行ったら反対の端へ）---
	// ((x % n) + n) % n：マイナスの番号も 0〜n-1 にする（C++ の % は、マイナスを割るとマイナスが残る）
	menu.selected = (((menu.selected + ReadMove(menu)) % count) + count) % count;
	// --- 選ぶ：マウス（動かしたときだけ。止まっているマウスが、キーで選んだ項目を戻さないように）---
	const int hovered = FindHoveredItem(items);
	const Vector2 mouse = InputManager::GetMousePositionInGame();
	const bool isMouseMoved = (mouse.x != menu.lastMouse.x) || (mouse.y != menu.lastMouse.y);
	menu.lastMouse = mouse;
	if (hovered >= 0 && isMouseMoved) {
		menu.selected = hovered;
	}
	ApplyLook(menu, items);
	// --- 決める：キー、または項目の上でクリック ---
	const bool isClicked = (hovered >= 0) && InputManager::IsMouseButtonTriggered(0);
	if (isClicked) {
		menu.selected = hovered;
	}
	if (IsDecideTriggered() || isClicked) {
		if (const MenuItemComponent* item = EntityManager::Get<MenuItemComponent>(items[menu.selected])) {
			Decide(menu, *owner, *item);
		}
	}
}

//=============================================================================
// Inspector と 保存
//=============================================================================
void DescribeMenu(ComponentUI& ui, MenuComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと操作を受けない"));
	ui.Label("項目は、子（孫も）に付けた Menu Item（Hierarchy の並びの順）。再生中だけ動く");
	ui.Field("Open Mode###openMode", value.openMode,
	    Tip("Always：いつも開いている（タイトル画面） / PauseKey：Esc・パッドの Start で開く・閉じる。開いている間はゲームを止める（ポーズ画面。再生を始めたら隠れる）"));
	// ===== 見た目 =====
	ui.Separator("Look");
	ui.ColorField("Normal Color###normalColor", value.normalColor);
	ui.ColorField("Selected Color###selectedColor", value.selectedColor);
	ui.Field("Selected Scale###selectedScale", value.selectedScale, AtLeast(0.0f).Tip("選んでいる項目の大きさの倍率（1 なら変えない）"));
	ui.Label("色は項目の Text（無ければ Image）の Color に、大きさは Transform の Scale に書く");
}

void DescribeMenuItem(ComponentUI& ui, MenuItemComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと選べない（飛ばす）"));
	ui.Field("Action###action", value.action,
	    Tip("決めたときにすること。Resume：閉じてゲームに戻る / Restart：最初から / LoadScene：Scene のシーンへ / Quit：終わる（エディタでは再生を止める）"));
	// --- 選んだ動きに要る値だけを見せる（保存も、使っている方だけが書かれる）---
	switch (value.action) {
	case MenuAction::LoadScene:
		ui.Field("Scene###scene", value.scene, Tip("切り替えるシーンの名前（Title・Level01 など。resources/scenes の .scene.json の名前）"));
		break;
	case MenuAction::None:
	case MenuAction::Resume:
	case MenuAction::Restart:
	case MenuAction::Quit:
		break; // 選ぶ物は無い
	}
	ui.Label("見た目は、同じ Entity の Text（無ければ Image）。Menu の子に置く");
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void MenuSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<MenuState>();
	// Menu・Menu Item の置き場所・保存・Inspector を登録する（見せ方は Describe〜 1つで、保存と Inspector の両方に使う）
	EntityManager::RegisterComponent<MenuComponent>();
	ComponentSerializer::Register<MenuComponent>("Menu", &DescribeMenu);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<MenuComponent>>("Menu", "UI", &DescribeMenu));
	EntityManager::RegisterComponent<MenuItemComponent>();
	ComponentSerializer::Register<MenuItemComponent>("Menu Item", &DescribeMenuItem);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<MenuItemComponent>>("Menu Item", "UI", &DescribeMenuItem));
}

void MenuSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void MenuSystem::Update(bool isGameRunning) {
	MY_ASSERT_MSG(state != nullptr, "MenuSystem::Initialize()を先に呼んでください");
	if (!isGameRunning) {
		return; // 停止中は何もしない（Stop で、Play を押した瞬間の状態に戻る）
	}
	// --- 項目の元の大きさを覚える（再生を始めた最初のフレームに1回。選んだ項目を大きくしても、元に戻せるように）---
	EntityManager::ForEach<MenuItemComponent>([](Handle<Entity> entity, MenuItemComponent& item) {
		if (item.isStarted) {
			return;
		}
		item.isStarted = true;
		if (const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity)) {
			item.baseScale = transform->scale;
		}
	});
	EntityManager::ForEach<MenuComponent>([](Handle<Entity> entity, MenuComponent& menu) { UpdateMenu(entity, menu); });
}
```
- `CollectItems` は自分を呼び直す（再帰）。中で `state->children` を書き換えるので、回す前に写しを取る（写さずに回すと、孫を集めたときに、回している配列の中身が変わる）。
- `menu.selected = (((menu.selected + move) % count) + count) % count`：C++ の `%` は、マイナスを割るとマイナスが残る（`-1 % 3` は `-1`）。`+ count` してもう一度 `% count` すると、いつも 0〜count−1 になる（一番上で↑を押すと、一番下へ）。
- `<Windows.h>` は `PostMessage`・`WM_CLOSE` のため（エディタでは使わないが、`#ifdef` の外に置いても害は無い）。
- `DescribeMenuItem` の switch は、`LoadScene` のときだけ Scene の欄を出す（Trigger Activator の Source と同じ。保存も、使っている方だけが書かれる）。

### `MyEngine/Engine.cpp`（3か所）
include（`TriggerActivatorSystem.h` の下）：
```cpp
#include "MyEngine/Systems/MenuSystem.h"
```
`Initialize` の `TriggerActivatorSystem::Initialize();` の下に：
```cpp
	MenuSystem::Initialize();             // Menu・Menu ItemをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```
`Finalize` の `TriggerActivatorSystem::Release();` の下に：
```cpp
	MenuSystem::Release();             // シーン（ウィンドウ）の破棄より後にする
```

### `MyEngine/Window/WindowManager.cpp`（2か所）
include（`TriggerActivatorSystem.h` の下。パートB の `SceneTransition.h` の上）：
```cpp
#include "MyEngine/Systems/MenuSystem.h"
```
`UpdateAll` の `TriggerActivatorSystem::Update(...)` の下（パートB の `SceneTransition::Update` の上）に：
```cpp
	MenuSystem::Update(isGameRunning);      // メニューを選ぶ・決める・ポーズを開く（止めない時間で動く。決めたら SceneTransition に頼む）
```

### `MyEngine/Editor/Windows/HierarchyWindow.cpp`（2か所）
include（`LightComponent.h` の下）：
```cpp
#include "MyEngine/Components/MenuComponent.h"
```
`DrawCreateMenu` の Text の下に：
```cpp
	// --- メニュー：項目（Menu Item）を子に並べる入れ物。項目は、選んでいる Menu の子に作る（Scene ビューでドラッグして並べる）---
	if (ImGui::MenuItem("Menu")) {
		EditorHistory::RequestCreate("Menu", {}, {ComponentSnapshot::Make(MenuComponent{})});
	}
	if (ImGui::MenuItem("Menu Item", nullptr, false, selected_.IsValid())) {
		TextComponent text;
		text.text = "Item";
		EditorHistory::RequestCreate("Menu Item", selected_, {ComponentSnapshot::Make(text), ComponentSnapshot::Make(MenuItemComponent{})});
	}
```
- Menu Item は、選んでいる Entity（Menu）の子に、「Item」と書いた Text を付けて作る。どれも原点に出るので、G9b の UI のドラッグで並べる。

### `MyEngine_v1.vcxproj`
```
    <ClCompile Include="MyEngine\Systems\MenuSystem.cpp" />
    <ClInclude Include="MyEngine\Components\MenuComponent.h" />
    <ClInclude Include="MyEngine\Systems\MenuSystem.h" />
```

---

## 5. パートD：ゲーム（AL4_01）の切り替えを幕付きにする
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

## 6. パートE：エディタで作る

### 6.1 ポーズ画面（全部のレベルに置く）
1. Hierarchy の ＋ → **Menu**。名前を `PauseMenu` にする。Inspector の Menu の **Open Mode を PauseKey** に。
2. PauseMenu を選んで ＋ → **Image**（できた Image は一番上にできるので、PauseMenu の上へドラッグして子にする）。名前 `Dim`、Size 1280×720、Color 黒・A 150 くらい、**Order 100**。
3. PauseMenu の子に ＋ → **Text**（同じく子にする）。名前 `Title`、Text「ポーズ」、Order 101。
4. PauseMenu を選んで ＋ → **Menu Item** を3つ（PauseMenu の子にできる）。それぞれ：

| 名前 | Text | Menu Item の Action | Scene |
|---|---|---|---|
| Resume | つづける | Resume | |
| Restart | やりなおす | Restart | |
| ToTitle | タイトルへ | LoadScene | Title（6.2 で作る） |

   Text の **Order を 101**（Dim より手前）に。Font は Level01 と同じ物を選ぶ。
5. Scene ビューで、文字を**ドラッグ**して縦に並べる（G9b）。並びの順は Hierarchy の順（上の項目を Hierarchy でも上に）。
6. 作り終わったら、PauseMenu の**目を閉じる**（Scene ビューで、レベルの物にかぶらないように）。
7. Play → Esc で開く・↑↓で選ぶ・Enter で決める。開いている間、キューブが止まっていること。
8. PauseMenu を右クリック → **Save As Prefab**（`resources/prefabs/PauseMenu.prefab`）。ほかのレベルでは、Project ウィンドウの prefabs の PauseMenu を右クリック → **Place in Scene**。

### 6.2 タイトル画面
1. File → **New Scene...** で `Title` を作る（`resources/scenes/Title.scene.json`）。
2. ＋ → **Menu**（Open Mode は **Always** のまま）。子に Menu Item を2つ：

| 名前 | Text | Action | Scene |
|---|---|---|---|
| Start | はじめる | LoadScene | Level01 |
| Quit | おわる | Quit | |

3. ゲームの名前の Text（Menu の外）・背景の Image を置く。
4. 製品版で最初に開くシーンを Title にする：AL4_01 の `main.cpp` の `Engine::Initialize(config, "Level01");` を `"Title"` に（エディタは、最後に開いていたシーンを開く）。
5. 最後のレベルの LevelInfo の **Next Scene** を `Title` にすると、全部クリアしたらタイトルへ戻る。

---

## 7. 確かめたこと
Release（エディタ無し）で、Level01 のコピーにポーズ画面（6.1 と同じ形。Resume・Restart・Quit）を置き、キーの入力を時間で流して（テストのときだけ InputManager に入れた仕掛け。外した）、0.5 秒ごとに画面を撮った。

| 時間（キーを流し始めてから） | キー | 画面 |
|---|---|---|
| 0〜3 秒 | | ポーズ画面は出ない（再生を始めたら隠れる） |
| 3 秒 | Esc | ポーズ画面が出る。後ろが暗くなる。「つづける」が黄色・少し大きい |
| 5 秒 | ↓ | 「やりなおす」が黄色に（「つづける」は白・元の大きさ） |
| 7 秒 | ↓ | 「おわる」 |
| 9 秒 | ↑ | 「やりなおす」 |
| 11 秒 | Enter | 幕がだんだん暗くなる → 最初から（ポーズ画面は隠れる）→ だんだん明るくなる |
| 15 秒・17 秒 | Esc・Esc | 開く → 閉じる |
| 19〜20.5 秒 | Esc・↓・↓ | 開いて「おわる」 |
| 22 秒 | Enter | **ゲームが自分で終わった**（`WM_CLOSE`） |

エディタ（Debug）では、Hierarchy の ＋ に Menu・Menu Item が出ること、Menu Item の Inspector（Action）、PauseMenu の目を閉じると子も Scene ビューから消えることを確かめた（G9b の 6章の画像）。

![ポーズ画面（Release）](images/g9c_pause.png)

確かめていないこと：
- パッド（Start・十字・A・B・スティック）とマウスでの操作（コードは同じ道を通る）。
- エディタの Play 中の Esc（Game ビューを触っているときだけ効く。エディタのほかの Esc とぶつからないか）。
- タイトル画面（6.2）は作っていない（同じ Menu の Always）。

---

## 8. 決めたこと・後でやること
- 幕は**黒**（`SceneTransition::SetColor` で変えられる）。白くフラッシュ・横から閉じる（ワイプ）などは、要るなら `Draw` を変える。
- ポーズ中も **BGM は鳴ったまま**（止めたいなら、`SetOpen` で `AudioSystem::SetPaused` を呼ぶ。エディタの Pause と同じ関数なので、ぶつからないか見てから）。
- 選んだときの音・決めたときの音は無い（要るなら、Menu に Audio Source の Entity を入れる欄を足す）。
- メニューの中に別のメニュー（設定画面）は無い（Menu の Entity を Resume のように開く Action を足せばできる）。
- ポーズ画面は、レベルごとに Prefab から置く（Prefab と置いた物はつながっていないので、直したら置き直す）。
