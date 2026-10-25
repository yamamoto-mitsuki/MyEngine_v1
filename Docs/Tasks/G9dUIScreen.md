# G9d：画面（UI Screen）― ポーズ画面・クリア画面を作るエディタ

参考：
- Unity の Prefab Mode（Prefab だけを開いて直す編集モード。Done の代わりに戻る矢印）：https://docs.unity3d.com/Manual/EditingInPrefabMode.html
- Unity の Canvas Group（子をまとめて透明にする。このエンジンの Alpha Group）：https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/class-CanvasGroup.html
- Unity の Time.timeScale（0 でも Update は呼ばれ続ける）：https://docs.unity3d.com/ScriptReference/Time-timeScale.html
- イージングの形の一覧：https://easings.net/ja
- cpprefjp `std::erase_if`（条件に合う要素を全部消す）：https://cpprefjp.github.io/reference/vector/vector/erase_if_free.html
- cpprefjp `std::move`（範囲を移す。3つの引数の方）：https://cpprefjp.github.io/reference/algorithm/move.html
- cpprefjp `std::exchange`：https://cpprefjp.github.io/reference/utility/exchange.html
- cpprefjp `std::optional`：https://cpprefjp.github.io/reference/optional/optional.html

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の 13章の **G9d**。[G9cSceneTransition.md](G9cSceneTransition.md) の後（このファイルの答えは、G9c の 0章の直しとパートC を入れた状態で作った）。
- ユーザーの頼み：ポーズ画面・クリア画面などを楽に作る仕組み。
  - `resources/ui/〇〇.uiscreen` のファイル（Prefab と同じ扱い。全部のシーンで使える）。
  - 開くと Scene ビューが編集モードになる（**シーンは見えたまま**・1280×720 の枠・Hierarchy には画面の中身だけ・Text と Image を置いてドラッグで動かす）。Done で保存して閉じる。
  - 出し方・消し方（上下左右から滑り込む・フェード・拡大、秒数・イージング、要素ごとの遅れ）、出している間ゲームを止めるか。
  - ゲームからは `UIScreen::Open("Pause")`・`Close`。項目を選ぶ動きはゲームの Script。
- **名前は `UIScreen`**（2026-10-25 にユーザーと決めた）：エンジンでは「Screen」を画面の座標の意味でもう使っている（`ScreenAnchor`・`ScreenQuad`・`ScreenPointToRay`）ので、「1枚の UI の画面」の方は `UIScreen` にして分ける。Component は `UIScreenComponent`（Inspector の名前は UI Screen）、ファイルは `resources/ui/〇〇.uiscreen`。
- 相談で決めたこと：設定は **UI Screen Component**（Inspector・Undo・保存が今の仕組みのまま使える）。子ごとに動きを変える **UI Screen Element** も入れる。**シーンをまたいで開いたままにするのは提出の後**（今はシーンが変わると閉じる）。画面の中に Model を置くのも提出の後。
- **パートの順番**：A（保存しない印・Entity の並び）→ B（UI Screen の本体）→ C（エディタの編集モード）→ D（ゲームの PauseMenu）→ E（エディタでポーズ画面を作る）。A・B・C はそれぞれでビルドが通る。
- 確かめた環境：本体の e300a7d ＋ G9c の 0章 ＋ このファイルのエンジン、AL4_01 のコピー（G9a までの写経＋G9c パートC＋パートD）。Debug・Release でビルドし、Release でポーズを、Debug のエディタで編集モードを動かして確かめた（7章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `ECS/Entity.h`・`Scene/SceneSerializer.cpp`・`Core/SlotMap.h` | シーンに保存しない印（`isDontSave`）。Entity を消しても並びが変わらないようにする |
| B | `Components/UIScreenComponent.h`・`UI/UIScreen.h/.cpp`（新しく作る）・`Systems/ScriptSystem.h/.cpp`・`Scene/SceneManager.cpp`・`Window/WindowManager.cpp`・`Engine.cpp`・`ECS/GameComponent.h`・`MyEngine_v1.vcxproj` | 画面を出す・消す（`UIScreen::Open/Close`）・出し方と消し方・ポーズ中は画面の中の Script だけ呼ぶ |
| C | `Editor/History/EditorHistory.h/.cpp`・`Editor/Viewport/SceneVisibility.h/.cpp`・`ScenePicking.cpp`・`Editor/Gizmos/UIGizmo.cpp`・`SceneGizmo.cpp`・`Editor/Windows/UIScreenEditor.h/.cpp`（新しく作る）・`HierarchyWindow.cpp`・`ProjectWindow.h/.cpp`・`Scene/SceneManager.cpp`・`Window/WindowManager.cpp`・`Engine.cpp`・`MyEngine_v1.vcxproj` | 編集モード（Edit UI Screen・Done・Cancel） |
| D | AL4_01 の `Source/Scripts/UI/PauseMenu.h/.cpp`（新しく作る）・`LevelInfo.h/.cpp`・`AL4_01.vcxproj` | ポーズ画面の項目を選ぶ Script・Esc で出す |
| E | エディタで | `resources/ui/Pause.uiscreen` を作る |

---

## 0. 前の写経の答え合わせ
**`MyEngine/Components/ScreenComponent.h` をもう写し始めていたら、`UIScreenComponent.h` に名前を変えて、中身もパートB の形（型の名前が全部 `UIScreen〜`）に直す**（前の版の md は `Screen` という名前だった）。

G9b と G9c のパートA・B（e300a7d）は [G9cSceneTransition.md](G9cSceneTransition.md) の 0章。**`PolygonShapeSystem.cpp` の `isSceneHidden` の場所の直し（G9c の 0.1）は先に入れておく**（このファイルの答えは、直した後の形で作った）。G9c のパートC（ゲームの3か所）もまだなら、先に写す（このファイルのパートD の `LevelInfo.cpp` は、`SceneTransition::` に替えた後の形で書いてある）。

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 画面のファイル：Prefab と同じ形 ＋ 一番上に UI Screen Component
```
resources/ui/Pause.uiscreen（中身は Prefab と同じ JSON）
Pause            ← UI Screen Component（出し方・消し方・ゲームを止めるか）＋ PauseMenu（ゲームの Script）
 ├ Dim           ← Image（黒・半透明・1280×720）＋ UI Screen Element（これだけフェードで出す）
 ├ Title         ← Text「ポーズ」
 └ Items         ← 空の Entity（項目の入れ物）
    ├ Resume     ← Text「つづける」
    └ Restart    ← Text「やりなおす」
```
- 読む・作るは **PrefabManager をそのまま使う**（`Load`・`Instantiate`・`Save`）。画面のために別の読み書きは作らない。
- 出し方・消し方を **Component** にしたのは、Inspector・Undo・保存が全部今の仕組みのまま使えるから（ファイルの頭に別の欄を作ると、その欄だけのための Inspector と Undo が要る）。
- **動く単位は「一番上の直下の子」1つずつ**（上の図なら Dim・Title・Items の3つ）。Hierarchy の上から順に、Stagger の秒ずつ遅れて動き出す。Items の中の項目は、Items と一緒に動く。
- 出し方の動き（`UIScreenMotionType`）：

| Motion | 動かす物 | 出すとき | 消すとき |
|---|---|---|---|
| None | 何も | すぐ出ている | 最後まで出ている（消し終わる時間に消える） |
| Fade | Alpha Group の Alpha | 0 → 1 | 1 → 0 |
| Slide | Translation | Side の側へ Distance ずれた所から、元の所へ | 元の所から、Side の側へ Distance ずれた所へ |
| Scale | Scale | From Scale 倍 → 1 倍 | 1 倍 → From Scale 倍 |

- Slide・Scale は **With Fade** で一緒にフェードもできる。どれも Duration（秒）と Ease（`Ease::Type`。前からある `Math/Easing.h` の物）を持つ。
- フェードは **G9a の Alpha Group**（`SpriteRenderSystem::CalcGroupAlpha` が自分と親を全部掛ける）。要素に Alpha Group が無ければ、出すときに付ける（付けるのは実行中の Entity だけで、ファイルは変わらない）。
- 時間は**止めない時間**（`GetUnscaleDeltaTime`）。ゲームを止めていても動く。エディタの一時停止（Pause ボタン）の間は止める。

### 1.2 出す・消す（`UIScreen::Open`・`Close`）の流れ
```
フレーム N（Script の Update）   UIScreen::Open("Pause") → PrefabManager::Instantiate（Entity と Transform だけ、すぐできる）
フレーム N+1 の頭               PrefabManager::Flush（ほかの Component・Script が付く）
                                UIScreen::Flush（UI Screen Component を読んで、出し始めの形にする＝透明・ずれた所）← 描く前
                                　　　　　　　 Pause Game なら Time::SetGamePaused(true)
フレーム N+1 〜（UpdateAll）     UIScreen::Update（時間を進めて形を当てる）→ 出しきったら Shown
UIScreen::Close                   すぐゲームを動かす → 消す動き → 消しきったら Destroy
```
- **出し始めの形を描く前に当てる**のが大事：Component が付いたフレームに何もしないと、1フレームだけ「出しきった形」で見えてしまう。なので `PrefabManager::Flush` の**すぐ後**に `UIScreen::Flush` を呼ぶ。
- もう出ている・消している途中の画面を Open しても断る（Esc を続けて押しても2枚重ならない）。
- シーンが変わる（やりなおす・次のレベル）と、画面の Entity も一緒に消える（`UIScreen::Update` が「もう居ない」と見て忘れる）。ポーズも `ResetGameTime` で外れる。
- **Stop** を押したら全部忘れる（Play を押した瞬間の状態に戻るので、画面の Entity も無い）。

### 1.3 ポーズ中は「開いている画面の中の Script」だけ呼ぶ
G9c で「ポーズ中は Script を呼ばない」にした。そのままだと、ポーズ画面で項目を選ぶ Script も止まってしまう。そこで：
- `ScriptSystem::Update(filter)`・`LateUpdate(filter)` に「呼ぶ Script を選ぶ関数」を渡せるようにする。
- `SceneManager::Update` は、ポーズ中なら `UIScreen::IsInsideOpenScreen`（開いている画面の一番上か、その子孫か）を渡す。

| | プレイヤー（IceCube）・LevelInfo | ポーズ画面の PauseMenu |
|---|---|---|
| ポーズしていない | 呼ぶ | （画面が無い） |
| ポーズ中 | 呼ばない（Enter・↑↓で動かない） | **呼ぶ** |

- ポーズ画面を**出す**のは、シーンの Script（LevelInfo の Esc）。**閉じる**のは、画面の中の Script（PauseMenu の Esc・つづける）。ポーズ中は LevelInfo が呼ばれないので、同じ Esc で「閉じてすぐ開く」にはならない。
- `filter` は関数ポインタ（`bool (*)(Handle<Entity>)`）。`ScriptSystem` が `UIScreen` を知らなくてよいように、呼ぶ側（SceneManager）が渡す。

### 1.4 編集モード：シーンに重ねる（シーンは見えたまま）
| | どうするか |
|---|---|
| 入る | Project で .uiscreen をダブルクリック・右クリック → Edit UI Screen・Inspector の **Edit UI Screen**（停止中だけ）。次のフレームの頭で、中身を今のシーンに `Instantiate` |
| 見た目 | **シーンはそのまま描く**（ポーズ画面の後ろにゲームがどう見えるか、見ながら置ける）。Scene ビューの絵の縁に水色の枠と「UI Screen: Pause (1280×720)」、真ん中の十字 |
| 選ぶ | Scene ビューのクリックで選べるのは**画面の中の物だけ**（`SceneVisibility::SetPickRoot`）。Hierarchy にも**画面の中身だけ**（一番上に名前と Done・Cancel） |
| 作る | Hierarchy の ＋・何も無い所へのドロップ・貼り付けは、画面の一番上の子にできる（画面の外にできないように）。一番上は Delete・Duplicate できない |
| 保存 | 画面の一番上に **`Entity::isDontSave`** の印。シーンを保存しても、Play の退避・* の判定にも入らない。**Save（Ctrl+S）は、シーンと画面の両方**を保存する |
| Undo | 入ったときに、シーンの履歴を**脇へよける**（`EditorHistory::Suspend`）。編集中の Ctrl+Z は画面の編集だけ。出るときに**戻す**（`Resume`。画面の編集の履歴は捨てる） |
| 出る | **Done**＝画面のファイルに保存して、中身を消す／**Cancel**＝保存せずに消す |
| Play・シーンを開く | 先に Done と同じことをする（`SceneManager::ChangeScene` の頭。中身はシーンに入っていないので、そのままだと消えてしまう） |

- UI は 1280×720 の画面を、絵の全体に引き伸ばして描いている（`Renderer::kSpriteBaseWidth`）。なので「1280×720 の枠」は Scene ビューの**絵の縁**と同じ所になる。
- 入る・出るは**フレームの頭**（`EditorHistory::Flush` の後）。ImGui を描いている途中で Entity を増やす・消すと、一覧を回している途中の Hierarchy が壊れるので。

### 1.5 Entity を消しても、並びが変わらないようにする（SlotMap）
画面は「Hierarchy の上から順」で動く順・項目の順を決める。ところが今の `SlotMap::Destroy` は、**一番後ろの要素を、消した所へ持ってくる**（隙間を作らないための、よくあるやり方）。なので Entity を1つ消すと、一番後ろの Entity が消した所へ飛んできて、Hierarchy の並びが変わる（シーンでも前から起きていた）。

```
消す前： [A][B][C][D][E]       B を消す
今まで： [A][E][C][D]          ← E が B の所へ（並びが変わる）
直した： [A][C][D][E]          ← 後ろを1つずつ前へ詰める（並びはそのまま）
```
- 詰めるのは `std::move(first, last, dest)`（`<algorithm>` の方。[first, last) の要素を、dest から順に移す）。動いた要素のスロットには、新しい場所を教え直す。
- 1回消すのに、後ろの数だけ動かす（今までは1つ）。Entity は数百個なので、気にならない。
- 並びは「作った順」。Hierarchy で並び替える機能は無いので、項目は**上から順に作る**（6章）。Undo で消した物を戻すと、一番後ろに戻る（前から同じ）。

### 1.6 1フレームの中の順番
```
UpdateAll の頭：FlushComponentChanges → PrefabManager::Flush → UIScreen::Flush（出し始めの形）
                → EditorHistory::Flush → UIScreenEditor::Flush（編集に入る・出る）
各ウィンドウ：  SceneManager::Update（シーン → Script（ポーズ中は画面の中だけ）→ SYSTEM → LateUpdate）
                ……
終わりの方：    CollisionSystem → TriggerActivatorSystem → UIScreen::Update → SceneTransition::Update
```

---

## 2. パートA：シーンに保存しない印・Entity の並び（エンジン）

### `MyEngine/ECS/Entity.h`（`struct Entity` の最後）
```cpp
	bool isActive = true;
	bool isDontSave = false; // シーンに保存しない（子も一緒に。エディタで編集中の画面の中身）。シーンファイル・Play の退避・保存していない変更（*）に入らない
};
```

### `MyEngine/Scene/SceneSerializer.cpp`（無名 namespace の `SaveAll`）
`// 全Entity` の `SaveAll` の上に関数を足して、`SaveAll` の for の頭で飛ばす：
```cpp
// シーンに保存しない物か（自分か親のどれかに isDontSave の印がある）
bool IsDontSave(const Entity& entity) {
	for (const Entity* current = &entity; current != nullptr; current = EntityManager::Get(current->parent)) {
		if (current->isDontSave) {
			return true;
		}
	}
	return false;
}

// 全Entity（保存しない物は飛ばす）
SceneJson SaveAll() {
	std::set<std::string> warnings;
	SceneJson entities = SceneJson::array();
	// 並んでいる順（Hierarchyに出る順）のまま書く。読むときも同じ順に作るので、並びが変わらない
	for (const Entity& entity : EntityManager::GetAll()) {
		if (IsDontSave(entity)) {
			continue; // エディタで編集中の画面の中身（画面のファイルに書く）
		}
		const Entity* parent = EntityManager::Get(entity.parent);
```
- 印は画面の一番上にだけ付ける。子は親をたどって見つける（編集中に足した子にも、印を付けなくてよい）。
- `SaveToText`（Play の退避・* の判定）も `SaveAll` を通るので、ここ1か所で全部に効く。`SaveTree`（Prefab・画面の保存）は通らないので、画面のファイルにはちゃんと書かれる。

### `MyEngine/Core/SlotMap.h`（3か所）
include（`<utility>` の上）：
```cpp
#include <algorithm>
```
`Destroy` の宣言のコメント：
```cpp
	void Destroy(Handle<T> h); // 後ろの要素を前へ詰めて消し（並びは作った順のまま）、スロットを空きリストへ
```
`Destroy` の中（`Slot& slot = ...` から `denseToSlot_.pop_back();` まで）を：
```cpp
	Slot& slot = slots_[handle.index];
	const uint32_t removeIndex = slot.denseIndex;

	// --- 後ろの要素を1つずつ前へ詰めて、末尾を消す（隙間を作らない・並びを保つ）---
	// 末尾と入れ替えると、消すたびに並び（Hierarchy の順・画面の要素の順）が変わってしまうので、詰める
	// std::move（3つの引数）：[first, last) の要素を、dest から順に移す（前へずらす）
	std::move(dense_.begin() + removeIndex + 1, dense_.end(), dense_.begin() + removeIndex);
	std::move(denseToSlot_.begin() + removeIndex + 1, denseToSlot_.end(), denseToSlot_.begin() + removeIndex);
	dense_.pop_back();
	denseToSlot_.pop_back();
	// 前へずれた要素のスロットに、新しい場所を教える
	for (uint32_t i = removeIndex; i < static_cast<uint32_t>(dense_.size()); ++i) {
		slots_[denseToSlot_[i]].denseIndex = i;
	}
```
- その下の「スロットを空きにする」はそのまま。`lastIndex` は使わなくなったので消える。

---

## 3. パートB：画面を出す・消す（エンジン）

### `MyEngine/Components/UIScreenComponent.h`（新しく作る）
```cpp
#pragma once
#include <type_traits>

#include "MyEngine/Math/Easing.h"

// 画面（ポーズ画面・クリア画面など）の出し方・消し方。resources/ui/〇〇.uiscreen の一番上の Entity に付ける（カテゴリ：UI）
// 動かすのは UIScreen（UI/UIScreen.h）。UIScreen::Open で出して、UIScreen::Close で消す
// 動く単位は「一番上の Entity の直下の子」1つずつ。Hierarchy の上から順に、Stagger の秒ずつ遅れて動く


/// <summary>
/// 出し方・消し方の動き
/// </summary>
enum class UIScreenMotionType {
	None,  // 動かない（すぐ出る・消すときは最後まで出たまま）
	Fade,  // だんだん見えてくる・見えなくなる（Alpha Group の Alpha を動かす）
	Slide, // 画面の外の方から滑り込む・外の方へ出ていく（Translation を動かす）
	Scale, // 小さい所から大きくなる・小さくなって消える（Scale を動かす）
};

/// <summary>
/// 滑り込むときの、画面のどちら側か（出すときは「どこから」、消すときは「どこへ」）
/// </summary>
enum class UIScreenSide {
	Left,
	Right,
	Top,
	Bottom,
};

/// <summary>
/// 出し方・消し方1つ分（UI Screen Component に「出す」「消す」の2つ持つ）
/// </summary>
struct UIScreenMotion {
	UIScreenMotionType type = UIScreenMotionType::Fade;
	UIScreenSide side = UIScreenSide::Bottom; // Slide のとき：どちら側から・どちら側へ
	float distance = 60.0f;               // Slide のとき：動く長さ（ピクセル。1280×720 の画面のつもりで）
	float fromScale = 0.6f;               // Scale のとき：一番小さいときの大きさ（元の大きさを 1 として）
	bool withFade = true;                 // Slide・Scale のとき：一緒にフェードもする
	float duration = 0.3f;                // 1つの要素が動ききるまでの秒数（止めない時間で数える）
	Ease::Type ease = Ease::Type::OutCubic; // 動きの速さの変え方（出すときは Out〜、消すときは In〜 が自然）
};

static_assert(std::is_trivially_copyable_v<UIScreenMotion>);


/// <summary>
/// 画面の出し方・消し方（画面のファイルの一番上の Entity に付ける）
/// </summary>
struct UIScreenComponent {
	bool pauseGame = true;       // 出している間、ゲームを止める（Time::SetGamePaused。中の Script は動く）
	UIScreenMotion show;           // 出し方
	UIScreenMotion hide = {.type = UIScreenMotionType::Fade, .duration = 0.2f, .ease = Ease::Type::Linear}; // 消し方
	float showStagger = 0.05f;   // 出すとき、要素1つごとに遅らせる秒数（上から2つ目は 0.05 秒、3つ目は 0.1 秒遅れて動き出す）
	float hideStagger = 0.0f;    // 消すとき、要素1つごとに遅らせる秒数
};

static_assert(std::is_trivially_copyable_v<UIScreenComponent>);


/// <summary>
/// 要素1つだけ、動き方を変える（画面の一番上の Entity の直下の子に付ける。付けなければ UI Screen Component の動きのまま）
/// <para>例：後ろを暗くする Image はフェードだけ、パネルは下から滑り込む</para>
/// </summary>
struct UIScreenElementComponent {
	bool overrideShow = false; // true なら、出し方を show にする
	UIScreenMotion show;
	bool overrideHide = false; // true なら、消し方を hide にする
	UIScreenMotion hide = {.type = UIScreenMotionType::Fade, .duration = 0.2f, .ease = Ease::Type::Linear};
	float delay = 0.0f; // Stagger の遅れに足す秒数（出す・消すの両方）
};

static_assert(std::is_trivially_copyable_v<UIScreenElementComponent>);
```
- UI Screen Component の `hide` の初期値は、**指示付き初期化**（`{.type = ..., .duration = ...}`。C++20。名前を書いた項目だけ値を入れ、ほかはそれぞれの初期値）で「0.2 秒でフェード」にしている。

### `MyEngine/UI/UIScreen.h`（新しく作る）
```cpp
#pragma once
#include <filesystem>
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

// 画面のファイルの置き場所と、名前の後ろに付く部分（"resources/ui/Pause.uiscreen" の画面の名前は "Pause"）
inline constexpr const char* kUIScreenFolder = "resources/ui/";
inline constexpr const char* kUIScreenFileExtension = ".uiscreen";
// 名前から画面のファイルの場所を作る（"Pause" → "resources/ui/Pause.uiscreen"）
inline std::string MakeUIScreenFilePath(const std::string& name) { return std::string(kUIScreenFolder) + name + kUIScreenFileExtension; }


/// <summary>
/// 画面（ポーズ画面・クリア画面など）を出す・消す
/// <para>画面は resources/ui/〇〇.uiscreen のファイル（中身は Prefab と同じ形。全部のシーンで使える）。エディタの Edit UI Screen で作る</para>
/// <para>出すと、ファイルから今のシーンに Entity を作る（Prefab の Instantiate）。消すと、その Entity を消す。シーンが変わると一緒に消える</para>
/// <para>動きは一番上の Entity の UI Screen Component（出し方・消し方・ゲームを止めるか）。時間は止めない時間で数える</para>
/// <para>ゲームを止めている間（ポーズ中）も、開いている画面の中の Script は呼ばれる（項目を選ぶ Script を画面の中に置ける）</para>
/// </summary>
class UIScreen {
public:
	// UI Screen・UI Screen Element を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	// ===== ゲームから使う =====
	// name の画面を出す（Component が付くのは次のフレームの頭なので、見え始めるのは次のフレーム）
	// 停止中・もう出ている・消している途中・ファイルが無いときは false
	static bool Open(const std::string& name);
	// name の画面を消す（消し方の動きが終わったら、Entity を消す）。出ていなければ false
	static bool Close(const std::string& name);
	// name の画面が出ているか（出している途中も true。消している途中は false）
	static bool IsOpen(const std::string& name);

	// ===== エンジンから使う =====
	// entity が、開いている画面の中の物か（画面の一番上の Entity も含む）。ポーズ中に呼ぶ Script を選ぶのに使う
	static bool IsInsideOpenScreen(Handle<Entity> entity);
	// フレームの頭（PrefabManager::Flush の直後）。Component が付いたばかりの画面に、出し始めの形を当てる（描く前に当てるので、一瞬元の形で見えることはない）
	static void Flush();
	// 毎フレーム（WindowManager::UpdateAll）。出す・消す動きを進めて、ゲームを止める・動かす。停止中は全部忘れる
	static void Update(bool isGameRunning);

	// ===== エディタから使う =====
	// 中身が一番上の Entity（UI Screen Component 付き）だけの画面のファイルを作る（Project ウィンドウの Create → UI Screen）
	static bool CreateNewFile(const std::filesystem::path& path);
};
```

### `MyEngine/UI/UIScreen.cpp`（新しく作る）
```cpp
#include "UIScreen.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Components/AlphaGroupComponent.h"
#include "MyEngine/Components/UIScreenComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentStorage.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Scene/PrefabManager.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/Time/EngineTime.h"

namespace {
/// <summary>
/// 開いた画面が今どうしているか
/// </summary>
enum class UIScreenPhase {
	Waiting, // Open した。Component が付くのを待っている（次のフレームの頭の Flush で Showing へ）
	Showing, // 出している途中
	Shown,   // 出きった
	Hiding,  // 消している途中（終わったら Entity を消す）
};

/// <summary>
/// 動く要素1つ（画面の一番上の Entity の直下の子）
/// </summary>
struct UIScreenElement {
	Handle<Entity> entity;
	Vector3 baseTranslation; // 出きったときの位置（ファイルに書いてある位置）
	Vector3 baseScale;       // 出きったときの大きさ
	float baseAlpha = 1.0f;  // 出きったときの Alpha Group の Alpha
	UIScreenMotion show;       // 出し方（UI Screen Element で変えていれば、そちら）
	UIScreenMotion hide;       // 消し方
	float showDelay = 0.0f;  // 出し始めるまでの秒（Stagger × 何番目か ＋ UI Screen Element の Delay）
	float hideDelay = 0.0f;  // 消し始めるまでの秒
};

/// <summary>
/// 開いている画面1つ
/// </summary>
struct OpenUIScreen {
	std::string name;
	Handle<Entity> root;                 // 一番上の Entity（UI Screen Component が付いている）
	UIScreenPhase phase = UIScreenPhase::Waiting;
	float time = 0.0f;                   // 出し始めて・消し始めてからの秒（止めない時間）
	bool pauseGame = false;              // 出している間、ゲームを止めるか
	std::vector<UIScreenElement> elements; // 動く要素（Hierarchy の上から順）
	float showEnd = 0.0f;                // 全部の要素が出きる秒
	float hideEnd = 0.0f;                // 全部の要素が消えきる秒
};

/// <summary>
/// UIScreen が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct UIScreenState {
	std::vector<OpenUIScreen> screens;
	bool isPausing = false; // 画面がゲームを止めているか（止めた・動かしたを1回だけ Time に書く）
};
std::unique_ptr<UIScreenState> state;

//=============================================================================
// 小さな道具
//=============================================================================
// 名前から開いている画面（無ければ end）
std::vector<OpenUIScreen>::iterator FindScreen(const std::string& name) { return std::ranges::find(state->screens, name, &OpenUIScreen::name); }

// 滑り込む向き（画面のどちら側か → ピクセルの向き。Y は上が＋）
Vector3 SideDirection(UIScreenSide side) {
	switch (side) {
	case UIScreenSide::Left:
		return {-1.0f, 0.0f, 0.0f};
	case UIScreenSide::Right:
		return {1.0f, 0.0f, 0.0f};
	case UIScreenSide::Top:
		return {0.0f, 1.0f, 0.0f};
	case UIScreenSide::Bottom:
		return {0.0f, -1.0f, 0.0f};
	}
	return {};
}

// どれだけ見えているか（0：出す前・消えきった ～ 1：出きった）。OutBack などは途中で 1 を少し超える
float CalcVisible(const UIScreenMotion& motion, float time, float delay, bool isShowing) {
	float t = 1.0f;
	if (motion.duration > 0.0f) {
		t = std::clamp((time - delay) / motion.duration, 0.0f, 1.0f);
	} else if (time < delay) {
		t = 0.0f; // 0 秒なら、遅れの分を待ってから一度に変える
	}
	const float eased = Ease::Apply(t, motion.ease);
	return isShowing ? eased : 1.0f - eased;
}

// 要素1つに、見えている量 visible の形を当てる（Translation・Scale・Alpha Group の Alpha）
void ApplyElement(const UIScreenElement& element, const UIScreenMotion& motion, float visible) {
	TransformComponent* transform = EntityManager::Get<TransformComponent>(element.entity);
	AlphaGroupComponent* group = EntityManager::Get<AlphaGroupComponent>(element.entity);
	if (transform == nullptr || group == nullptr) {
		return;
	}
	Vector3 translation = element.baseTranslation;
	Vector3 scale = element.baseScale;
	float alpha = element.baseAlpha;
	switch (motion.type) {
	case UIScreenMotionType::None:
		break; // 動かさない
	case UIScreenMotionType::Fade:
		alpha *= visible;
		break;
	case UIScreenMotionType::Slide:
		// 見えていない分だけ、画面の外の方へずらす（visible が 1 を超えると、少し行き過ぎて戻る）
		translation += SideDirection(motion.side) * (motion.distance * (1.0f - visible));
		alpha *= motion.withFade ? visible : 1.0f;
		break;
	case UIScreenMotionType::Scale:
		scale *= Ease::Lerp(motion.fromScale, 1.0f, visible);
		alpha *= motion.withFade ? visible : 1.0f;
		break;
	}
	transform->translation = translation;
	transform->scale = scale;
	group->alpha = std::clamp(alpha, 0.0f, 1.0f);
}

// 画面の全部の要素に、今の時間の形を当てる
void ApplyScreen(const OpenUIScreen& screen) {
	const bool isShowing = (screen.phase != UIScreenPhase::Hiding);
	for (const UIScreenElement& element : screen.elements) {
		const UIScreenMotion& motion = isShowing ? element.show : element.hide;
		const float delay = isShowing ? element.showDelay : element.hideDelay;
		ApplyElement(element, motion, CalcVisible(motion, screen.time, delay, isShowing));
	}
}

// Component が付いた画面の要素を集めて、出し始める（フレームの頭で呼ぶ。Alpha Group をその場で付けるので）
void BeginShow(OpenUIScreen& screen) {
	// --- 出し方・消し方（UI Screen Component が無ければ、動かさずに出す）---
	UIScreenComponent settings;
	if (const UIScreenComponent* component = EntityManager::Get<UIScreenComponent>(screen.root)) {
		settings = *component;
	} else {
		LogManager::Warning(std::format("画面 \"{}\" の一番上の Entity に UI Screen Component がありません（動かさずに出します）", screen.name));
		settings.pauseGame = false;
		settings.show.type = UIScreenMotionType::None;
		settings.hide.type = UIScreenMotionType::None;
	}
	screen.pauseGame = settings.pauseGame;

	// --- 要素（直下の子。Hierarchy の上から順）---
	std::vector<Handle<Entity>> children;
	EntityManager::GetChildren(screen.root, children);
	screen.elements.clear();
	for (size_t i = 0; i < children.size(); ++i) {
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(children[i]);
		if (transform == nullptr) {
			continue;
		}
		// フェードは Alpha Group で掛ける。無ければ今付ける（フレームの頭なので、予約せずにその場で付けてよい）
		if (EntityManager::Get<AlphaGroupComponent>(children[i]) == nullptr) {
			EntityManager::RestoreComponents(children[i], {ComponentSnapshot::Make(AlphaGroupComponent{})});
		}
		AlphaGroupComponent* group = EntityManager::Get<AlphaGroupComponent>(children[i]);
		UIScreenElement element;
		element.entity = children[i];
		element.baseTranslation = transform->translation;
		element.baseScale = transform->scale;
		element.baseAlpha = group->enabled ? group->alpha : 1.0f; // 切ってあった Alpha Group は 1 と同じ
		group->enabled = true;
		element.show = settings.show;
		element.hide = settings.hide;
		float extraDelay = 0.0f;
		if (const UIScreenElementComponent* own = EntityManager::Get<UIScreenElementComponent>(children[i])) {
			element.show = own->overrideShow ? own->show : element.show;
			element.hide = own->overrideHide ? own->hide : element.hide;
			extraDelay = own->delay;
		}
		element.showDelay = settings.showStagger * static_cast<float>(i) + extraDelay;
		element.hideDelay = settings.hideStagger * static_cast<float>(i) + extraDelay;
		screen.showEnd = (std::max)(screen.showEnd, element.showDelay + element.show.duration);
		screen.hideEnd = (std::max)(screen.hideEnd, element.hideDelay + element.hide.duration);
		screen.elements.push_back(element);
	}
	screen.phase = UIScreenPhase::Showing;
	screen.time = 0.0f;
	ApplyScreen(screen); // 描く前に、出し始めの形にしておく
}

// 画面がゲームを止めるか（止める画面が1つでも出ていれば止める。消している途中の画面は数えない）
void UpdatePause() {
	const bool wantsPause = std::ranges::any_of(state->screens, [](const OpenUIScreen& screen) { return screen.pauseGame && screen.phase != UIScreenPhase::Hiding; });
	if (wantsPause != state->isPausing) {
		state->isPausing = wantsPause;
		Time::SetGamePaused(wantsPause);
	}
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// 出し方・消し方1つ分（id は保存の名前の頭。"show" → "showMotion"・"showDuration" …）
// 選んだ動きの値だけ見せる（保存も、使っている方だけが書かれる）
void DescribeMotion(ComponentUI& ui, UIScreenMotion& motion, const char* id) {
	ui.Field(std::format("Motion###{}Motion", id).c_str(), motion.type, Tip("None：動かない / Fade：だんだん見える・消える / Slide：画面の外の方から滑り込む / Scale：小さい所から大きくなる"));
	switch (motion.type) {
	case UIScreenMotionType::None:
		return; // 秒数もイージングも使わない
	case UIScreenMotionType::Fade:
		break;
	case UIScreenMotionType::Slide:
		ui.Field(std::format("Side###{}Side", id).c_str(), motion.side, Tip("出すときは、どちら側から来るか。消すときは、どちら側へ行くか"));
		ui.Field(std::format("Distance###{}Distance", id).c_str(), motion.distance, AtLeast(0.0f).Tip("動く長さ（ピクセル。1280×720 の画面のつもりで。画面の外から入れるなら 1280・720）"));
		ui.Field(std::format("With Fade###{}WithFade", id).c_str(), motion.withFade, Tip("動きながらフェードもする"));
		break;
	case UIScreenMotionType::Scale:
		ui.Field(std::format("From Scale###{}FromScale", id).c_str(), motion.fromScale, AtLeast(0.0f).Tip("一番小さいときの大きさ（元の大きさが 1）"));
		ui.Field(std::format("With Fade###{}WithFade", id).c_str(), motion.withFade, Tip("大きさを変えながらフェードもする"));
		break;
	}
	ui.Field(std::format("Duration###{}Duration", id).c_str(), motion.duration, AtLeast(0.0f).Tip("1つの要素が動ききるまでの秒数（ゲームを止めていても進む）"));
	ui.Field(std::format("Ease###{}Ease", id).c_str(), motion.ease, Tip("動きの速さの変え方。出すときは Out〜（速く入って、ゆっくり止まる）、消すときは In〜（ゆっくり動き出して、速く抜ける）が自然。OutBack は少し行き過ぎて戻る"));
}

void DescribeUIScreen(ComponentUI& ui, UIScreenComponent& value) {
	ui.Label("画面のファイルの一番上の Entity に付ける。直下の子が、上から順に1つずつ動く");
	ui.Field("Pause Game###pauseGame", value.pauseGame, Tip("出している間、ゲームを止める（時間が0・Script を呼ばない）。この画面の中の Script は動く"));
	ui.Separator("Show");
	DescribeMotion(ui, value.show, "show");
	ui.Field("Stagger###showStagger", value.showStagger, AtLeast(0.0f).Tip("要素1つごとに遅らせる秒数（上から2つ目はこの秒、3つ目は2倍遅れて動き出す）"));
	ui.Separator("Hide");
	DescribeMotion(ui, value.hide, "hide");
	ui.Field("Stagger###hideStagger", value.hideStagger, AtLeast(0.0f).Tip("要素1つごとに遅らせる秒数"));
}

void DescribeUIScreenElement(ComponentUI& ui, UIScreenElementComponent& value) {
	ui.Label("画面の一番上の Entity の直下の子に付ける。この要素だけ動き方を変える");
	ui.Field("Override Show###overrideShow", value.overrideShow, Tip("出し方を、ここで決めた物にする"));
	if (value.overrideShow) {
		DescribeMotion(ui, value.show, "show");
	}
	ui.Field("Override Hide###overrideHide", value.overrideHide, Tip("消し方を、ここで決めた物にする"));
	if (value.overrideHide) {
		DescribeMotion(ui, value.hide, "hide");
	}
	ui.Field("Delay###delay", value.delay, AtLeast(0.0f).Tip("Stagger の遅れに足す秒数（出す・消すの両方）"));
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void UIScreen::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<UIScreenState>();
	// UI Screen・UI Screen Element の置き場所・保存・Inspector を登録する（見せ方は Describe〜 1つで、保存と Inspector の両方に使う）
	EntityManager::RegisterComponent<UIScreenComponent>();
	ComponentSerializer::Register<UIScreenComponent>("UI Screen", &DescribeUIScreen);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<UIScreenComponent>>("UI Screen", "UI", &DescribeUIScreen));
	EntityManager::RegisterComponent<UIScreenElementComponent>();
	ComponentSerializer::Register<UIScreenElementComponent>("UI Screen Element", &DescribeUIScreenElement);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<UIScreenElementComponent>>("UI Screen Element", "UI", &DescribeUIScreenElement));
}

void UIScreen::Release() { state.reset(); }

//=============================================================================
// ゲームから使う
//=============================================================================
bool UIScreen::Open(const std::string& name) {
	MY_ASSERT_MSG(state != nullptr, "UIScreen::Initialize()を先に呼んでください");
	// 停止中はゲームのコードが動かないので、ふつうは来ない（エディタで開いているシーンに、勝手に Entity を足さないように）
	const SceneManager* main = SceneManager::GetMain();
	if (main == nullptr || main->GetPlayState() == PlayState::Editing) {
		LogManager::Log(std::format("UIScreen::Open(\"{}\")：停止中なので出しません（Play 中だけ効きます）", name));
		return false;
	}
	// もう出ている・消している途中なら出さない（Esc を続けて押したときに、2枚重ならないように）
	if (FindScreen(name) != state->screens.end()) {
		return false;
	}
	// 同じファイルは1回だけ読む（2回目からは同じ番号。エディタで保存し直すと、中身も差し替わる）
	const uint32_t prefab = PrefabManager::Load(ToPath(MakeUIScreenFilePath(name)));
	if (prefab == 0) {
		LogManager::Warning(std::format("UIScreen::Open(\"{}\")：{} が読めません", name, MakeUIScreenFilePath(name)));
		return false;
	}
	OpenUIScreen screen;
	screen.name = name;
	screen.root = PrefabManager::Instantiate(prefab, {0.0f, 0.0f, 0.0f}); // 一番上の Entity は原点（UI の位置は子に書く）
	state->screens.push_back(std::move(screen));
	return true;
}

bool UIScreen::Close(const std::string& name) {
	MY_ASSERT_MSG(state != nullptr, "UIScreen::Initialize()を先に呼んでください");
	const auto it = FindScreen(name);
	if (it == state->screens.end() || it->phase == UIScreenPhase::Hiding) {
		return false;
	}
	// まだ Component が付いていない（Open と同じフレームに Close した）なら、動かさずに消す
	if (it->phase == UIScreenPhase::Waiting) {
		EntityManager::Destroy(it->root);
		state->screens.erase(it);
		return true;
	}
	it->phase = UIScreenPhase::Hiding;
	it->time = 0.0f;
	UpdatePause(); // 消し始めたら、すぐゲームを動かす（消える動きを待たずに遊べるように）
	return true;
}

bool UIScreen::IsOpen(const std::string& name) {
	MY_ASSERT_MSG(state != nullptr, "UIScreen::Initialize()を先に呼んでください");
	const auto it = FindScreen(name);
	return it != state->screens.end() && it->phase != UIScreenPhase::Hiding;
}

//=============================================================================
// エンジンから使う
//=============================================================================
bool UIScreen::IsInsideOpenScreen(Handle<Entity> entity) {
	return std::ranges::any_of(state->screens, [entity](const OpenUIScreen& screen) { return entity == screen.root || EntityManager::IsDescendantOf(entity, screen.root); });
}

void UIScreen::Flush() {
	MY_ASSERT_MSG(state != nullptr, "UIScreen::Initialize()を先に呼んでください");
	for (OpenUIScreen& screen : state->screens) {
		// Open した次のフレームの頭：PrefabManager::Flush で Component が付いたので、出し始める
		if (screen.phase == UIScreenPhase::Waiting && EntityManager::IsAlive(screen.root)) {
			BeginShow(screen);
		}
	}
	UpdatePause();
}

void UIScreen::Update(bool isGameRunning) {
	MY_ASSERT_MSG(state != nullptr, "UIScreen::Initialize()を先に呼んでください");
	// 停止中：全部忘れる（Stop でシーンが Play を押した瞬間に戻るので、画面の Entity も無くなる）
	if (!isGameRunning) {
		state->screens.clear();
		state->isPausing = false;
		return;
	}
	// エディタの一時停止中は、画面の動きも止める
	const SceneManager* main = SceneManager::GetMain();
	if (main != nullptr && main->GetPlayState() == PlayState::Paused) {
		return;
	}
	const float deltaTime = Time::GetUnscaleDeltaTime(); // 止めない時間（ポーズ中も動く）
	// erase_if：条件に合う物を配列から全部消す（C++20）。true を返した画面は、もう忘れる
	std::erase_if(state->screens, [deltaTime](OpenUIScreen& screen) {
		// シーンが変わった・誰かが Destroy した
		if (!EntityManager::IsAlive(screen.root)) {
			return true;
		}
		switch (screen.phase) {
		case UIScreenPhase::Waiting:
		case UIScreenPhase::Shown:
			return false;
		case UIScreenPhase::Showing:
			screen.time += deltaTime;
			ApplyScreen(screen);
			if (screen.time >= screen.showEnd) {
				screen.phase = UIScreenPhase::Shown;
			}
			return false;
		case UIScreenPhase::Hiding:
			screen.time += deltaTime;
			ApplyScreen(screen);
			if (screen.time >= screen.hideEnd) {
				EntityManager::Destroy(screen.root); // 子も一緒に消える
				return true;
			}
			return false;
		}
		return false;
	});
	UpdatePause();
}

//=============================================================================
// エディタから使う
//=============================================================================
bool UIScreen::CreateNewFile(const std::filesystem::path& path) {
	// 一番上の Entity 1つ（名前はファイルの名前、UI Screen Component だけ）。書いていない値は、読むときに初期値になる
	SceneJson item = SceneJson::object();
	item["id"] = EntityId{1};
	item["name"] = ToUtf8(path.stem());
	item["parent"] = EntityId{0};
	item["active"] = true;
	item["components"] = {{"UI Screen", SceneJson::object()}};
	SceneJson root = SceneJson::object();
	root["version"] = SceneSerializer::kVersion;
	root["entities"] = SceneJson::array({item});
	if (!SceneSerializer::WriteFile(ToUtf8(path), root)) {
		return false;
	}
	LogManager::Log(std::format("画面を作りました: {}", ToUtf8(path)));
	return true;
}
```
- `std::erase_if(配列, 条件)`：条件が true の要素を全部消す（C++20）。`Update` では「もう居ない・消しきった画面」を、回しながら消している（添字の付け替えを自分で書かなくてよい）。
- `CalcVisible` は「どれだけ見えているか」（0〜1）。出すときは `Ease::Apply(t)`、消すときは `1 - Ease::Apply(t)`。OutBack は途中で 1 を少し超えるので、Slide は行き過ぎて戻る・Scale は少し大きくなって戻る（Alpha は 0〜1 に収める）。
- `BeginShow` の `UIScreenElement`：出しきったときの形（`base〜`）を覚えておき、毎フレーム「元 ＋ ずれ」を書く。今の値に足していくと、ずれがたまっていく。
- `Close` で `UpdatePause` をすぐ呼ぶ：消える動きを待たずに、ゲームが動き出す（Enter を押したらすぐ遊べる）。
- `CreateNewFile` の JSON は、UI Screen Component の中身を空の `{}` にしている（書いていない値は、読むときに初期値になる）。

### `MyEngine/Systems/ScriptSystem.h`（毎フレームの2つ）
```cpp
	// ===== 毎フレーム（SceneManager が再生中だけ呼ぶ）=====
	// 呼ぶ Script を選ぶ関数（Script が付いている Entity を受け取って、呼ぶなら true）
	using EntityFilter = bool (*)(Handle<Entity> entity);
	// Awake・OnEnable → Start → 当たり（OnTrigger〜・OnCollision〜）→ Update。SYSTEM(...) より前
	// filter を渡すと、選んだ Script だけ呼ぶ（ゲームのポーズ中に、開いている画面の中の Script だけ動かす）。nullptr なら全部
	static void Update(EntityFilter filter = nullptr);
	// LateUpdate。SYSTEM(...) の後（filter は Update と同じ）
	static void LateUpdate(EntityFilter filter = nullptr);
```

### `MyEngine/Systems/ScriptSystem.cpp`（`Update`・`LateUpdate`）
`Update` の頭（`const size_t count = scripts.size();` の下に1つ足す）：
```cpp
void ScriptSystem::Update(EntityFilter filter) {
	// 途中で Script が増えても大丈夫なように、添字で回す。数は最初に決める（このフレームに付いた物は、次のフレームから）
	// 回している間に配列から消すことはない（消すのは FlushDestroy・Entity の破棄＝フレームの終わりか境目だけ）
	const size_t count = scripts.size();
	// filter で選ばなかった Script は、このフレームは何も呼ばない（ポーズ中に、開いている画面の中の Script だけ動かす）
	const auto isSkipped = [filter](const Script& script) { return filter != nullptr && !filter(script.gameObject_.handle); };
```
4つの for の if に `isSkipped` を足す（1. 起こす / 2. Start / 3. 当たり / 4. Update の順）：
```cpp
		if (script.isDestroyRequested_ || isSkipped(script)) {
```
```cpp
		if (script.isEnableCalled_ && !script.isStarted_ && !script.isDestroyRequested_ && !isSkipped(script)) {
```
```cpp
		if (!IsRunning(script) || isSkipped(script)) {
```
```cpp
		if (IsRunning(script) && !isSkipped(script)) {
			script.Update();
```
`LateUpdate`：
```cpp
void ScriptSystem::LateUpdate(EntityFilter filter) {
	const size_t count = scripts.size();
	for (size_t i = 0; i < count; ++i) {
		Script& script = *scripts[i];
		if (IsRunning(script) && (filter == nullptr || filter(script.gameObject_.handle))) {
			script.LateUpdate();
		}
	}
}
```
- ラムダ（`isSkipped`）は `ScriptSystem::Update` の中で作ったので、`Script` の private（`gameObject_`）も読める（ScriptSystem は Script の friend。メンバ関数の中のラムダは、その関数と同じだけ読める）。
- 呼ばなかった Script は、Awake・Start も待つ（ポーズ中にプレイヤーの Start が走らない）。

### `MyEngine/Scene/SceneManager.cpp`（2か所）
include（`SceneSerializer.h` の下）：
```cpp
#include "MyEngine/UI/UIScreen.h"
```
`Update` の Script を呼ぶ所を：
```cpp
		// Script は一時停止中は呼ばない（SYSTEM は時間0で回る）
		// ゲームのポーズ中は、開いている画面（UI Screen）の中の Script だけ呼ぶ
		// 全部呼ぶと、ポーズ画面を選ぶキー（Enter・↑↓）でプレイヤーまで動いてしまうので
		const bool isRunning = (playState_ == PlayState::Playing);
		const ScriptSystem::EntityFilter filter = Time::IsGamePaused() ? &UIScreen::IsInsideOpenScreen : nullptr;
		if (isRunning) {
			ScriptSystem::Update(filter); // Script の Awake・Start・当たり・Update（シーンのUpdateの後、SYSTEMの前）
		}
		GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）
		if (isRunning) {
			ScriptSystem::LateUpdate(filter); // Script の LateUpdate（SYSTEMの後）
		}
```

### `MyEngine/Window/WindowManager.cpp`（3か所）
include（`SceneTransition.h` の下）：
```cpp
#include "MyEngine/UI/UIScreen.h"
```
`UpdateAll` の頭の `PrefabManager::Flush();` の下に：
```cpp
	UIScreen::Flush();        // 前フレームに UIScreen::Open で作った画面に、出し始めの形を当てる（Component が付いた直後・描く前）
```
`TriggerActivatorSystem::Update(...)` の下（`SceneTransition::Update` の上）に：
```cpp
	UIScreen::Update(isGameRunning);                                       // 画面（ポーズ画面など）の出す・消す動きを進めて、ゲームを止める・動かす（止めない時間で動く）
```

### `MyEngine/Engine.cpp`（3か所）
include（`TriggerActivatorSystem.h` の下）：
```cpp
#include "MyEngine/UI/UIScreen.h"
```
`Initialize` の `TriggerActivatorSystem::Initialize();` の下に：
```cpp
	UIScreen::Initialize();                 // UI Screen・UI Screen ElementをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```
`Finalize` の `TriggerActivatorSystem::Release();` の下に：
```cpp
	UIScreen::Release();                 // シーン（ウィンドウ）の破棄より後にする
```

### `MyEngine/ECS/GameComponent.h`（1つ足す）
`SceneTransition.h` の下に（Script から include を足さずに `UIScreen::Open` を使えるように）：
```cpp
#include "MyEngine/UI/UIScreen.h"
```

### `MyEngine_v1.vcxproj`
```
    <ClCompile Include="MyEngine\UI\UIScreen.cpp" />
    <ClInclude Include="MyEngine\Components\UIScreenComponent.h" />
    <ClInclude Include="MyEngine\UI\UIScreen.h" />
```

---

## 4. パートC：編集モード（エディタ）

### `MyEngine/Editor/History/EditorHistory.h`（`Clear` の下）
```cpp
	// 今の履歴を脇へよけて、空の履歴から始める（画面（UI Screen）の編集に入るとき。シーンの操作を Ctrl+Z で戻さないように）
	static void Suspend();
	// 脇へよけた履歴に戻す（今の履歴は捨てる。画面の編集を終えたとき。よけていなければ何もしない）
	static void Resume();
```

### `MyEngine/Editor/History/EditorHistory.cpp`（3か所）
無名 namespace の `changeCount` の下に：
```cpp

// Suspend で脇へよけた履歴（Resume で戻す）
std::vector<Entry> suspendedHistory;
size_t suspendedCursor = 0;
bool isSuspended = false;
```
`Clear` の最後（`++changeCount;` の前）に：
```cpp
	suspendedHistory.clear(); // 脇へよけていた物も、もう戻せない（相手が作り直される）
	isSuspended = false;
```
`Clear` の下に：
```cpp

void EditorHistory::Suspend() {
	if (isSuspended) {
		return;
	}
	// std::exchange：1つ目の変数に2つ目の値を入れて、前の値を返す（よけて、空にする）
	suspendedHistory = std::exchange(history, {});
	suspendedCursor = std::exchange(cursor, size_t{0});
	isSuspended = true;
	pendingEdits.clear();
	++changeCount;
}

void EditorHistory::Resume() {
	if (!isSuspended) {
		return;
	}
	// よけている間の履歴（画面の編集）は捨てて、よける前の履歴に戻す
	history = std::exchange(suspendedHistory, {});
	cursor = suspendedCursor;
	isSuspended = false;
	pendingEdits.clear();
	++changeCount;
}
```
- 画面の編集の履歴は、画面の Entity（もう消えた）を指しているので捨てる。シーンの履歴は、シーンの Entity が消えていないので、そのまま戻せる。

### `MyEngine/Editor/Viewport/SceneVisibility.h`（`Clear` の下）
```cpp

// ===== クリックで選べる物を絞る（画面（UI Screen）の編集中。絵はそのまま描く）=====
// root と、その子孫だけ選べるようにする（無効な Handle なら、絞らない）
void SetPickRoot(Handle<Entity> root);
// Scene ビューでクリックして選べるか（隠していない・絞っていればその中の物）
bool IsPickable(Handle<Entity> entity);
```

### `MyEngine/Editor/Viewport/SceneVisibility.cpp`（2か所）
無名 namespace の `hiddenIds` の下に：
```cpp
Handle<Entity> pickRoot; // 選べる物の一番上（無効なら絞らない）
```
ファイルの最後に：
```cpp

//=============================================================================
// 選べる物を絞る
//=============================================================================
void SceneVisibility::SetPickRoot(Handle<Entity> root) { pickRoot = root; }

bool SceneVisibility::IsPickable(Handle<Entity> entity) {
	if (IsHidden(entity)) {
		return false;
	}
	return !pickRoot.IsValid() || entity == pickRoot || EntityManager::IsDescendantOf(entity, pickRoot);
}
```

### `MyEngine/Editor/Viewport/ScenePicking.cpp`（`ClosestHit::Consider`）
```cpp
	// もっと手前なら入れ替える。Hierarchy の目で隠した物は選ばない（見えない物をクリックで選ばないように）
	// 画面（UI Screen）の編集中は、画面の中の物だけ選ぶ（後ろのシーンは見えているが、触らない）
	void Consider(Handle<Entity> candidate, float candidateDistance) {
		if (!SceneVisibility::IsPickable(candidate)) {
			return;
		}
```

### `MyEngine/Editor/Gizmos/UIGizmo.cpp`（`CalcShape` の上のコメントと最初の if）
```cpp
// どちらも無い・描いていない（無効・目で隠した）・選べない（画面の編集中の、画面の外の物）物は false
bool CalcShape(Handle<Entity> entity, UIShape& shape) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	if (transform == nullptr || !EntityManager::IsActiveInHierarchy(entity) || !SceneVisibility::IsPickable(entity)) {
```

### `MyEngine/Editor/Windows/UIScreenEditor.h`（新しく作る）
```cpp
#pragma once
#include <filesystem>

#include <externals/imgui/imgui.h>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"


/// <summary>
/// 画面（UI Screen。resources/ui/〇〇.uiscreen）の編集モード（Polygon Shape の Edit Shape と同じ考えの、画面版）
/// <para>入る：Project ウィンドウでダブルクリック・Inspector の Edit UI Screen（停止中だけ）。画面の中身を、今のシーンに置く</para>
/// <para>編集中：シーンはそのまま見える（後ろにどう重なるか見ながら置ける）が、クリックで選べるのは画面の中の物だけ。
/// Hierarchy には画面の中身だけを出す。中身には「シーンに保存しない」印（Entity::isDontSave）を付ける。Undo は画面の編集の分だけ</para>
/// <para>出る：Done（ファイルに保存して閉じる）・Cancel（保存せずに閉じる）。Play・シーンを開くときは、先に Done と同じことをする</para>
/// </summary>
class UIScreenEditor {
public:
	// .uiscreen のファイルを Inspector に出す係（Edit UI Screen のボタン）を登録する
	static void Initialize();

	// ===== 入る・出る（Project・Inspector・Hierarchy から。実際に入る・出るのは次のフレームの頭）=====
	static void RequestOpen(const std::filesystem::path& filePath);
	// save が true なら Done（保存して閉じる）、false なら Cancel（保存せずに閉じる）
	static void RequestClose(bool save);

	// ===== エンジンから =====
	// フレームの頭（WindowManager::UpdateAll の EditorHistory::Flush の後）。頼まれていれば入る・出る
	static void Flush();
	// 今すぐ閉じる（SceneManager がシーンを作り直す前。フレームの境目で呼ぶ）。編集していなければ何もしない
	static void EndNow(bool save);
	// 編集中なら、画面のファイルに書く（Ctrl+S。シーンの保存と一緒に）
	static void Save();

	// ===== 調べる =====
	static bool IsEditing();
	// 編集中の画面の一番上の Entity（編集していなければ無効な Handle）
	static Handle<Entity> GetRoot();

	// ===== 描く（エディタのウィンドウから）=====
	// Hierarchy の一番上の帯（画面の名前・Done・Cancel）。編集していなければ何も描かない
	static void DrawBar();
	// Scene ビューの絵の上に、画面の枠（1280×720 の画面＝絵の縁）と名前を描く
	static void DrawFrame(const ImVec2& imageMin, const ImVec2& imageSize);
};
```

### `MyEngine/Editor/Windows/UIScreenEditor.cpp`（新しく作る）
```cpp
#include "UIScreenEditor.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/AssetInspector.h"
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Scene/PrefabManager.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/UI/UIScreen.h"
#include "MyEngine/String/ConvertString.h"

namespace {
constexpr ImU32 kFrameColor = IM_COL32(90, 200, 255, 255);  // 画面の枠（選んだ物の橙と分けて、水色）
constexpr ImU32 kGuideColor = IM_COL32(90, 200, 255, 70);   // 真ん中の線（うすく）
constexpr ImU32 kLabelBackColor = IM_COL32(0, 0, 0, 160);   // 名前の後ろ（シーンが明るくても読めるように）
constexpr float kFrameThickness = 2.0f;                     // 枠の太さ（画面のピクセル）

/// <summary>
/// 編集の状態（1つの画面だけ編集できる）
/// </summary>
struct EditState {
	bool isEditing = false;
	std::filesystem::path filePath;           // 編集中の画面のファイル
	Handle<Entity> root;                      // 編集中の画面の一番上の Entity
	std::filesystem::path openRequest;        // 次のフレームの頭で開く画面（空なら頼まれていない）
	std::optional<bool> closeRequest;         // 次のフレームの頭で閉じる（true：保存する / false：保存しない。無ければ頼まれていない）
};
EditState edit;

// 画面の名前（ファイルの名前から拡張子を外した物。"resources/ui/Pause.uiscreen" → "Pause"）
std::string NameOf(const std::filesystem::path& filePath) { return ToUtf8(filePath.stem()); }

// 入る（フレームの頭で）。中身を今のシーンに置いて、選べる物・Hierarchy・履歴を画面だけにする
void Begin(const std::filesystem::path& filePath) {
	const SceneManager* main = SceneManager::GetMain();
	if (main == nullptr || main->GetPlayState() != PlayState::Editing) {
		LogManager::Warning("画面の編集は、停止中だけできます（Stop してから開いてください）");
		return;
	}
	// 同じファイルは1回だけ読む（保存し直すと、PrefabManager::Save が中身を差し替える）
	const uint32_t prefab = PrefabManager::Load(filePath);
	if (prefab == 0) {
		return; // 読めなかった（PrefabManager がログに出している）
	}
	EditorHistory::Suspend(); // シーンの履歴はよけておく（Ctrl+Z でシーンの操作を戻さないように）
	edit.root = PrefabManager::Instantiate(prefab, {0.0f, 0.0f, 0.0f}); // Component は次のフレームの頭に付く
	EntityManager::Get(edit.root)->isDontSave = true; // 子も一緒に、シーンに保存しない
	edit.filePath = filePath;
	edit.isEditing = true;
	SceneVisibility::SetPickRoot(edit.root);
	HierarchyWindow::SetSelected(edit.root);
	LogManager::Log(std::format("画面の編集を始めました: {}", ToUtf8(filePath)));
}

/// <summary>
/// .uiscreen のファイルを Inspector に出す係（中身は Edit UI Screen で直す。ここでは開くボタンだけ）
/// </summary>
class UIScreenAssetEditor final : public AssetEditor {
public:
	std::string_view GetExtension() const override { return kUIScreenFileExtension; }
	std::string_view GetTypeName() const override { return "UI Screen"; }
	bool Draw(const std::filesystem::path& filePath, ComponentUI& ui) override {
		ui.Label(std::format("ゲームからは UIScreen::Open(\"{}\") で出す・UIScreen::Close(\"{}\") で消す", NameOf(filePath), NameOf(filePath)).c_str());
		if (ui.Button("Edit UI Screen")) {
			UIScreenEditor::RequestOpen(filePath);
		}
		ui.Label("Project ウィンドウでダブルクリックしても開ける（停止中だけ）");
		return false; // ここでは中身を変えない
	}
	void Save(const std::filesystem::path&) override {} // 保存は Done（UIScreenEditor）
};
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void UIScreenEditor::Initialize() { AssetInspector::Register(std::make_unique<UIScreenAssetEditor>()); }

//=============================================================================
// 入る・出る
//=============================================================================
void UIScreenEditor::RequestOpen(const std::filesystem::path& filePath) { edit.openRequest = filePath; }

void UIScreenEditor::RequestClose(bool save) { edit.closeRequest = save; }

void UIScreenEditor::Flush() {
	// --- Hierarchy で一番上を消した・Undo で消えた：閉じる（中身がもう無いので、保存はしない）---
	if (edit.isEditing && !EntityManager::IsAlive(edit.root)) {
		LogManager::Warning("編集中の画面の一番上の Entity が無くなったので、保存せずに閉じました");
		EndNow(false);
	}
	// --- 閉じる（Done・Cancel）---
	if (edit.closeRequest.has_value()) {
		EndNow(*std::exchange(edit.closeRequest, std::nullopt));
	}
	// --- 開く（ほかの画面を編集中なら、それを保存して閉じてから）---
	if (!edit.openRequest.empty()) {
		const std::filesystem::path filePath = std::exchange(edit.openRequest, {});
		if (edit.isEditing && filePath == edit.filePath) {
			return; // もう開いている
		}
		EndNow(true);
		Begin(filePath);
	}
}

void UIScreenEditor::EndNow(bool save) {
	if (!edit.isEditing) {
		return;
	}
	if (save) {
		Save();
	}
	// 中身を今すぐ消す（フレームの境目なので、予約をその場で反映してよい）
	if (EntityManager::IsAlive(edit.root)) {
		EntityManager::Destroy(edit.root); // 子も一緒に消える
		EntityManager::FlushDestroy();
	}
	EditorHistory::Resume(); // シーンの履歴に戻す（画面の編集の履歴は捨てる。消えた Entity を指しているので）
	SceneVisibility::SetPickRoot({});
	HierarchyWindow::SetSelected({});
	LogManager::Log(std::format("画面の編集を終わりました（{}）: {}", save ? "保存した" : "保存していない", ToUtf8(edit.filePath)));
	edit.isEditing = false;
	edit.root = {};
	edit.filePath.clear();
}

void UIScreenEditor::Save() {
	if (!edit.isEditing || !EntityManager::IsAlive(edit.root)) {
		return;
	}
	// 中身は Prefab と同じ形。読み込み済みなら中身も差し替わるので、次の UIScreen::Open から新しい中身になる
	PrefabManager::Save(edit.root, ToUtf8(edit.filePath));
}

//=============================================================================
// 調べる
//=============================================================================
bool UIScreenEditor::IsEditing() { return edit.isEditing; }

Handle<Entity> UIScreenEditor::GetRoot() { return edit.isEditing ? edit.root : Handle<Entity>{}; }

//=============================================================================
// 描く
//=============================================================================
void UIScreenEditor::DrawBar() {
	if (!edit.isEditing) {
		return;
	}
	ImGui::AlignTextToFramePadding();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kFrameColor), "UI Screen: %s", NameOf(edit.filePath).c_str());
	// --- 右端に Done・Cancel ---
	const ImGuiStyle& style = ImGui::GetStyle();
	const float doneWidth = ImGui::CalcTextSize("Done").x + style.FramePadding.x * 2.0f;
	const float cancelWidth = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.0f;
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端（ウィンドウの中での位置）
	ImGui::SameLine(right - doneWidth - cancelWidth - style.ItemSpacing.x);
	if (ImGui::Button("Done")) {
		RequestClose(true);
	}
	ImGui::SetItemTooltip("画面のファイルに保存して、編集を終わる");
	ImGui::SameLine();
	if (ImGui::Button("Cancel")) {
		RequestClose(false);
	}
	ImGui::SetItemTooltip("保存せずに、編集を終わる（開いてから直した所は消える）");
	// 説明は灰色で、ウィンドウの幅で折り返す（TextDisabled は折り返さないので、色を変えて TextWrapped）
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("Scene ビューの絵の縁が、1280×720 の画面。後ろのシーンは見えるだけで、選べない");
	ImGui::PopStyleColor();
}

void UIScreenEditor::DrawFrame(const ImVec2& imageMin, const ImVec2& imageSize) {
	if (!edit.isEditing) {
		return;
	}
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
	const ImVec2 center(imageMin.x + imageSize.x * 0.5f, imageMin.y + imageSize.y * 0.5f);
	// --- 真ん中の十字（左右・上下の真ん中にそろえるときの目安）---
	drawList->AddLine(ImVec2(center.x, imageMin.y), ImVec2(center.x, imageMax.y), kGuideColor);
	drawList->AddLine(ImVec2(imageMin.x, center.y), ImVec2(imageMax.x, center.y), kGuideColor);
	// --- 枠：UI は 1280×720 を絵の全体に引き伸ばして描くので、絵の縁＝画面の縁（線の太さの半分だけ内側に描く）---
	const float half = kFrameThickness * 0.5f;
	drawList->AddRect(ImVec2(imageMin.x + half, imageMin.y + half), ImVec2(imageMax.x - half, imageMax.y - half), kFrameColor, 0.0f, 0, kFrameThickness);
	// --- 左上に名前 ---
	const std::string label = std::format("UI Screen: {}  (1280×720)", NameOf(edit.filePath));
	const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
	const ImVec2 textMin(imageMin.x + 8.0f, imageMin.y + 8.0f);
	drawList->AddRectFilled(ImVec2(textMin.x - 4.0f, textMin.y - 2.0f), ImVec2(textMin.x + textSize.x + 4.0f, textMin.y + textSize.y + 2.0f), kLabelBackColor, 3.0f);
	drawList->AddText(textMin, kFrameColor, label.c_str());
}
```
- `std::optional<bool>`：bool が「入っている・入っていない」のどちらかを持てる入れ物。`closeRequest` は「頼まれていない（空）・保存して閉じる（true）・保存せずに閉じる（false）」の3つを1つで表す。`has_value()` で入っているか、`*` で中身。
- `std::exchange(edit.closeRequest, std::nullopt)`：頼まれた値を取り出しながら、空に戻す（1回だけ実行する）。
- `UIScreenAssetEditor` は Physic Material と同じ「Inspector に出す係」（`AssetEditor`）。中身は Edit UI Screen のボタンだけ（中身を直すのは編集モード）。
- 中身を消すのは `EntityManager::Destroy` ＋ `FlushDestroy` をその場で（フレームの頭なので、予約をその場で反映してよい。Undo と同じ使い方）。

### `MyEngine/Editor/Windows/HierarchyWindow.cpp`（9か所）
include（`SceneVisibility.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
無名 namespace の `ParentOf` を、3つの関数に（上の2つは新しく足す）：
```cpp
// 一番上の親（ふだんは無効なHandle＝root。画面（UI Screen）の編集中は、画面の一番上の Entity。作る・貼る・ドロップする物を、この外へ出さない）
Handle<Entity> TopParent() { return UIScreenEditor::GetRoot(); }

// 画面の編集中の、画面の一番上の Entity か（消す・複製すると、画面の外にできてしまうので、させない）
bool IsUIScreenRoot(Handle<Entity> handle) { return UIScreenEditor::IsEditing() && handle == UIScreenEditor::GetRoot(); }

// 親のHandle（いなければ一番上の親）。画面の一番上の Entity なら、それ自身（兄弟に貼ると画面の外になるので、子に貼る）
Handle<Entity> ParentOf(Handle<Entity> handle) {
	if (IsUIScreenRoot(handle)) {
		return handle;
	}
	const Entity* entity = EntityManager::Get(handle);
	return (entity != nullptr && entity->parent.IsValid()) ? entity->parent : TopParent();
}
```
`AcceptEntityDrop` のフォルダの if（画面の一番上へは、フォルダも入れてよい）：
```cpp
		if (IsFolder(dropped) && newParent.IsValid() && !IsFolder(newParent) && newParent != TopParent()) {
```
`Draw` の `DrawToolbar();` の下に：
```cpp
		UIScreenEditor::DrawBar(); // 画面（UI Screen）の編集中だけ、名前と Done・Cancel
```
`Draw` の一覧の for を：
```cpp
		if (UIScreenEditor::IsEditing()) {
			DrawEntityNode(UIScreenEditor::GetRoot()); // 画面の編集中は、画面の中身だけ（シーンの物は出さない）
		} else {
			for (const Entity& entity : EntityManager::GetAll()) {
				if (!entity.parent.IsValid()) {
					DrawEntityNode(entity.self);
				}
			}
		}
```
`DrawEmptySpace` のドロップ：
```cpp
	AcceptEntityDrop(TopParent()); // ここへ落としたら一番上へ移す（画面の編集中は、画面の一番上の子へ）
```
`DrawEntityMenu` の Duplicate と Delete（画面の一番上は押せない）：
```cpp
	if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !IsUIScreenRoot(handle))) {
```
```cpp
	if (ImGui::MenuItem("Delete", "Del", false, !IsUIScreenRoot(handle))) {
```
`DrawCreateMenu`：頭に `top` を作って、**`RequestCreate("…", {}, …)` の `{}` を全部 `top` に**（Create Entity・Folder・Light 3つ・3D Object 4つ・Camera・Sprite・Image・Text の 13 か所）。最後の Paste も：
```cpp
void HierarchyWindow::DrawCreateMenu() {
	// 作る物は一番上の親の下（ふだんは root。画面の編集中は、画面の一番上の子）
	const Handle<Entity> top = TopParent();
	if (ImGui::MenuItem("Create Entity")) {
		EditorHistory::RequestCreate("Entity", top);
	}
```
```cpp
		EditorHistory::RequestCreate("Text", top, {ComponentSnapshot::Make(TextComponent{})});
```
```cpp
	// 何も無い所から貼るので、一番上の親の下に貼る
	if (ImGui::MenuItem("Paste", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste(top);
	}
```
`HandleShortcuts` の Delete・Ctrl+D：
```cpp
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && selected_.IsValid() && !IsUIScreenRoot(selected_)) {
```
```cpp
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D) && selected_.IsValid() && !IsUIScreenRoot(selected_)) {
```
- ふだんは `TopParent()` が無効な Handle なので、今までと同じ動き（root に作る）。

### `MyEngine/Editor/Windows/ProjectWindow.h`（`CreatePhysicMaterial` の下）
```cpp
	static void CreateUIScreen();         // resources/ui/ に、一番上の Entity だけの .uiscreen を作る
```

### `MyEngine/Editor/Windows/ProjectWindow.cpp`（5か所）
include（`HierarchyWindow.h` の下・`SceneManager.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
```cpp
#include "MyEngine/UI/UIScreen.h"
```
`DrawItemMenu` の Prefab の `else if` の後に、画面の `else if` を足す：
```cpp
		ImGui::SetItemTooltip("原点に置く。直して Hierarchy の右クリック → Save As Prefab で、Prefab を上書きできる");
	} else if (ToUtf8(item.path).ends_with(kUIScreenFileExtension)) {
		// 画面は編集モードで開く（ダブルクリックと同じ。停止中だけ）
		const SceneManager* sceneManager = SceneManager::GetMain();
		const bool canEdit = sceneManager && sceneManager->GetPlayState() == PlayState::Editing;
		if (ImGui::MenuItem("Edit UI Screen", nullptr, false, canEdit)) {
			UIScreenEditor::RequestOpen(item.path);
		}
		ImGui::SetItemTooltip("停止中に、Scene ビューで画面を編集する（Hierarchy の Done で保存して閉じる）");
	}
```
`DrawCreateMenu` の Physic Material の Tooltip の下に：
```cpp
		if (ImGui::MenuItem("UI Screen")) {
			CreateUIScreen();
		}
		ImGui::SetItemTooltip("画面（ポーズ画面など）は resources/ui/ に作ります（UIScreen::Open(名前) で出すので、置き場所は1つ）");
```
`Open` のコードの if の下に：
```cpp
	// 画面は編集モードで開く（Scene ビューで置く。Hierarchy の Done で保存して閉じる）
	if (ToUtf8(item.path).ends_with(kUIScreenFileExtension)) {
		UIScreenEditor::RequestOpen(item.path);
	}
```
`CreateScene` の下に：
```cpp
void ProjectWindow::CreateUIScreen() {
	// まだ使われていない名前（NewUIScreen.uiscreen・NewUIScreen1.uiscreen …）
	std::filesystem::path path;
	std::error_code error;
	for (int number = 0;; ++number) {
		const std::string name = "NewUIScreen" + (number == 0 ? std::string() : std::to_string(number));
		path = ToPath(MakeUIScreenFilePath(name));
		if (!std::filesystem::exists(path, error)) {
			break;
		}
	}
	if (!UIScreen::CreateNewFile(path)) {
		return;
	}
	// resources/ui/ を開いて、作った画面の名前をすぐに打てるようにする
	OpenFolder(path.parent_path());
	selected_ = path;
	AssetInspector::Select(path);
	HierarchyWindow::SetSelected({});
	renaming_ = path;
	renameBuffer_ = EditableName(path, IconOf(ToUtf8(path.filename())), false); // 拡張子の前だけ打つ（StartRename と同じ）
	renameFocusRequest_ = true;
}
```

### `MyEngine/Editor/Gizmos/SceneGizmo.cpp`（2か所）
include（`HierarchyWindow.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
`SceneGizmo::Draw` の `HandleShortcuts(camera, isHovered);` の下に：
```cpp
	UIScreenEditor::DrawFrame(imageMin, imageSize); // 画面（UI Screen）の編集中は、画面の枠（一番下に描く。選んだ物の枠・ギズモが上に重なる）
```

### `MyEngine/Scene/SceneManager.cpp`（3か所）
include（`#ifdef USE_IMGUI` の中の `HierarchyWindow.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
`ChangeScene` の頭（`#ifdef USE_IMGUI` のすぐ下、`EditorHistory::Clear();` の上）に：
```cpp
	// 画面（UI Screen）を編集中なら、保存して閉じる（Play・シーンを開く・作り直すの前。画面の中身はシーンに入っていないので、このままだと消えてしまう）
	UIScreenEditor::EndNow(true);
```
`SaveImmediate` の `MarkSaved();` の上に：
```cpp
#ifdef USE_IMGUI
	UIScreenEditor::Save(); // 画面（UI Screen）を編集中なら、その画面のファイルも保存する（Ctrl+S で両方）
#endif
```
- Play は「退避（`SaveToText`）→ `ChangeScene`」の順。退避には画面の中身が入らない（パートA）ので、Play 中のシーンに画面の Entity は残らない。

### `MyEngine/Window/WindowManager.cpp`（2か所）
include（`#ifdef USE_IMGUI` の中の `EditorHistory.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
`UpdateAll` の頭の `EditorHistory::Flush();` の下に：
```cpp
	UIScreenEditor::Flush(); // 画面（UI Screen）の編集に入る・出る（Undo・Delete などの予約を済ませた後。中身の Component は次のフレームの頭に付く）
```

### `MyEngine/Engine.cpp`（2か所）
include（`Editor/Profiler.h` の下）：
```cpp
#include "MyEngine/Editor/Windows/UIScreenEditor.h"
```
`Initialize` の `UIScreen::Initialize();` の下に：
```cpp
#ifdef USE_IMGUI
	UIScreenEditor::Initialize(); // .uiscreen のファイルを Inspector に出す（Edit UI Screen のボタン）
#endif
```

### `MyEngine_v1.vcxproj`
```
    <ClCompile Include="MyEngine\Editor\Windows\UIScreenEditor.cpp" />
    <ClInclude Include="MyEngine\Editor\Windows\UIScreenEditor.h" />
```

---

## 5. パートD：ゲーム（AL4_01）のポーズ画面の Script

### `Source/Scripts/UI/PauseMenu.h`（新しく作る）
```cpp
#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector4.h>


/// <summary>
/// ポーズ画面の項目を選ぶ・決める。ポーズ画面（resources/ui/Pause.uiscreen）の一番上の Entity に付ける
/// <para>項目は、子の Items の子の Text（Hierarchy の上から順）。名前で何をするか決める（Resume：つづける / Restart：やりなおす）</para>
/// <para>ポーズ中（ゲームが止まっている間）も、開いている画面の中の Script なので呼ばれる</para>
/// </summary>
class PauseMenu : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// 選んでいる項目を、見た目（色・大きさ）に出す
	void ApplyLook() const;
	// 選んでいる項目を決める（名前で何をするか決める）
	void Decide(const std::string& itemName);

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Pause";              // この画面の名前（閉じるときに UIScreen::Close に渡す）
	Vector4 normalColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 選んでいない項目の文字の色
	Vector4 selectedColor_ = {1.0f, 0.85f, 0.3f, 1.0f}; // 選んでいる項目の文字の色
	float selectedScale_ = 1.15f;                   // 選んでいる項目の大きさ

	// ===== 実行中だけの値 =====
	int selected_ = 0;        // 選んでいる項目（上から 0, 1, …）
	bool isDecided_ = false;  // もう決めた（やりなおすの幕が閉じるまでに、もう一度決めないように）
};
```

### `Source/Scripts/UI/PauseMenu.cpp`（新しく作る）
```cpp
#include "PauseMenu.h"

#include <vector>

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Input/InputManager.h>

SCRIPT(PauseMenu, "UI");

namespace {
// 項目の並び（子の Items の子。Hierarchy の上から順）
std::vector<Handle<Entity>> CollectItems(const GameObject& owner) {
	std::vector<Handle<Entity>> items;
	if (const GameObject list = owner.FindChild("Items")) {
		EntityManager::GetChildren(list.handle, items);
	}
	return items;
}

// 上へ・下へ・決める・閉じるのキー（キーボードとパッドのどちらでも）
bool IsUpTriggered() { return InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsButtonTriggered(GamepadButton::DpadUp); }
bool IsDownTriggered() { return InputManager::IsKeyTriggered(DIK_DOWN) || InputManager::IsKeyTriggered(DIK_S) || InputManager::IsButtonTriggered(GamepadButton::DpadDown); }
bool IsDecideTriggered() { return InputManager::IsKeyTriggered(DIK_RETURN) || InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A); }
bool IsCloseTriggered() { return InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start) || InputManager::IsButtonTriggered(GamepadButton::B); }
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	ui.Label("子の Items の子の Text が項目（上から順）。名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）");
	ui.Label("↑↓・W S・十字で選ぶ / Enter・Space・A で決める / Esc・Start・B で閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.ColorField("Normal Color", normalColor_);
	ui.ColorField("Selected Color", selectedColor_);
	ui.Field("Selected Scale", selectedScale_, AtLeast(0.0f).Tip("選んでいる項目の大きさ（項目の Scale は 1 のままにしておく）"));
}

//=============================================================================
// 毎フレーム（ゲームが止まっていても呼ばれる）
//=============================================================================
void PauseMenu::Update() {
	const std::vector<Handle<Entity>> items = CollectItems(gameObject);
	if (items.empty() || isDecided_) {
		return;
	}
	const int count = static_cast<int>(items.size());
	// --- 閉じる ---
	if (IsCloseTriggered()) {
		UIScreen::Close(screenName_);
		isDecided_ = true;
		return;
	}
	// --- 選ぶ（端から反対の端へ回る。% は負の数だと負になるので、count を足してから割る）---
	if (IsUpTriggered()) {
		selected_ = (selected_ - 1 + count) % count;
	}
	if (IsDownTriggered()) {
		selected_ = (selected_ + 1) % count;
	}
	selected_ %= count; // 項目が減ったとき
	ApplyLook();
	// --- 決める ---
	if (IsDecideTriggered()) {
		const Entity* item = EntityManager::Get(items[selected_]);
		if (item != nullptr) {
			Decide(item->name);
		}
	}
}

//=============================================================================
// 見た目
//=============================================================================
void PauseMenu::ApplyLook() const {
	const std::vector<Handle<Entity>> items = CollectItems(gameObject);
	for (int i = 0; i < static_cast<int>(items.size()); ++i) {
		const bool isSelected = (i == selected_);
		if (TextComponent* text = EntityManager::Get<TextComponent>(items[i])) {
			text->color = isSelected ? selectedColor_ : normalColor_;
		}
		if (TransformComponent* itemTransform = EntityManager::Get<TransformComponent>(items[i])) {
			const float scale = isSelected ? selectedScale_ : 1.0f;
			itemTransform->scale = {scale, scale, 1.0f};
		}
	}
}

//=============================================================================
// 決める
//=============================================================================
void PauseMenu::Decide(const std::string& itemName) {
	if (itemName == "Resume") {
		UIScreen::Close(screenName_); // 消し始めたら、すぐゲームが動く
		isDecided_ = true;
	} else if (itemName == "Restart") {
		// ポーズのまま幕を閉じて、最初から（作り直すとポーズも画面も消える）
		isDecided_ = SceneTransition::ReloadScene();
	}
}
```
- 項目の並びは、子の **Items** の子（Hierarchy の上から順）。何をするかは**名前**で決める（`Resume`・`Restart`）。項目を増やすときは、`Decide` に名前を足す。
- 閉じる・決めたら `isDecided_`：消える動き・幕が閉じる間に、もう一度押しても何もしない。
- 選んだ項目の大きさは Scale に直接書く（項目の Scale は 1 のまま置く）。項目は Items の子なので、UI Screen が動かす Items の Scale とはぶつからない。

### `Source/Scripts/LevelInfo.h`（2か所）
クラスの説明の最後の行：
```cpp
/// <para>R キー / パッドの Back で、いつでもレベルの最初からやり直せる。Esc / パッドの Start でポーズ画面（Pause Screen）を出す</para>
```
メンバ変数の最後（`nextScene_` の下）に：
```cpp
	std::string pauseScreen_ = "Pause"; // Esc で出す画面の名前（resources/ui/<名前>.uiscreen。空ならポーズしない）
```

### `Source/Scripts/LevelInfo.cpp`（3か所）
`Fields` の最初の Label と、最後に1つ：
```cpp
	ui.Label("R / パッドの Back：レベルの最初からやり直す。Esc / パッドの Start：ポーズ画面を出す");
```
```cpp
	ui.Field("Pause Screen", pauseScreen_, Tip("Esc で出す画面の名前（resources/ui の .uiscreen の名前）。空なら、ポーズしない"));
```
`Update` の最後（やり直しの if の下）に：
```cpp
	// --- ポーズ：画面を出す（閉じるのは、画面の中の PauseMenu。ポーズ中はこの Update が呼ばれないので）---
	if (!pauseScreen_.empty() && (InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start))) {
		UIScreen::Open(pauseScreen_);
	}
```
- 前に保存したシーンの LevelInfo には `Pause Screen` が無いが、読むと初期値の "Pause" になる（シーンを直さなくてよい）。

### `AL4_01.vcxproj`
```
    <ClCompile Include="Source\Scripts\UI\PauseMenu.cpp" />
    <ClInclude Include="Source\Scripts\UI\PauseMenu.h" />
```

---

## 6. パートE：エディタでポーズ画面を作る
1. Project ウィンドウの何も無い所を右クリック → **Create → UI Screen**。`resources/ui/NewUIScreen.uiscreen` ができて、名前を打てるので **`Pause`** にする。
2. Pause.uiscreen を**ダブルクリック**（または Inspector の **Edit UI Screen**）。Hierarchy が `UI Screen: Pause ［Done］［Cancel］` になり、Scene ビューに水色の枠が出る。
3. 一番上の **Pause** を選んで、Inspector の **UI Screen**：Pause Game ✓／Show：Motion **Slide**・Side **Bottom**・Distance **60**・With Fade ✓・Duration **0.3**・Ease **OutCubic**・Stagger **0.06**／Hide：Motion **Fade**・Duration **0.2**・Ease **Linear**。**Add Component → Gameplay → UI → PauseMenu**（パートD の Script）。
4. ＋ → **Image**（Pause の子にできる）。名前 `Dim`、Color 黒・A **153**、Size **1280×720**、Order **100**。**Add Component → UI Screen Element**：Override Show ✓・Motion **Fade**・Duration **0.25**・Ease **OutQuad**（後ろを暗くする幕は、滑らずにフェードだけ）。
5. ＋ → **Text**。名前 `Title`、Text「ポーズ」、Font は Level01 と同じ物、Font Size **72**、Order **101**。Scene ビューでドラッグして上の方へ（Y 160 くらい）。
6. ＋ → **Create Entity**。名前 `Items`。
7. ＋ → **Text** を2つ、**つづける → やりなおす の順に**作る（並びは作った順。1.5）。名前 `Resume`・`Restart`、Text「つづける」・「やりなおす」、Font Size **48**、Order **101**。Hierarchy で2つとも **Items の上へドラッグ**して子にする（Resume を先に）。Scene ビューで Restart を下へ（Y −80 くらい）。
8. **Done**（保存して閉じる）。Hierarchy がシーンに戻る。
9. **Play** → Esc で出る（下から滑り込む・後ろが暗くなる・キューブが止まる）→ ↑↓で選ぶ → Enter。つづける＝閉じてすぐ動ける、やりなおす＝幕で最初から。Esc でも閉じる。

![編集モード（Scene ビューに水色の枠・Hierarchy は画面の中身だけ）](images/g9d_edit.png)

![ポーズ画面（Release）](images/g9d_pause.png)

---

## 7. 確かめたこと
**Release**（エディタ無し）：手で書いた Pause.uiscreen（6章と同じ形）を置いた Level01 のコピーで、キーを時間で流して（テストのときだけ InputManager に入れた仕掛け。外した）、画面を撮り、ログを出した（一時的なログ。外した）。

| キー | 結果 |
|---|---|
| Esc | ポーズになる。要素が下から 0.06 秒ずつずれて滑り込み、**0.43 秒で出きる**（計算：Items が 0.12＋0.3＝0.42 秒）。Dim はフェードだけ |
| ↓・↑ | 「やりなおす」・「つづける」が黄色・少し大きく |
| Enter（つづける） | すぐポーズが外れ、**0.20 秒**でフェードして消える |
| ポーズ中 | LevelInfo の Update が呼ばれない（ポーズの5秒間、LevelInfo の時間が進まなかった）。PauseMenu は呼ばれる |
| Esc → ↓ → Enter（やりなおす） | ポーズのまま幕が閉じて、最初から（画面とポーズが一緒に消える） |
| 作り直した後に Esc・Esc | また出る・Esc で閉じる |

**Debug のエディタ**（ImGui に入力を流すテスト用の仕掛け UiTestDriver。外した）：

| 操作 | 結果 |
|---|---|
| Project の ui → Pause.uiscreen を1回クリック | Inspector に「UI Screen」と Edit UI Screen のボタン |
| ダブルクリック | 編集モード（Hierarchy は画面の中身だけ・Done／Cancel・水色の枠と名前・後ろのシーンは見える） |
| Scene ビューで「つづける」をドラッグ | (142, 68) へ動く。Ctrl+Z で (0, 0) に戻る |
| 何も無い所をクリック | 画面の Dim が選ばれる（後ろのシーンの物は選ばれない） |
| ＋ → Text | Pause の子にできる |
| Save | シーンファイルは 35 個のまま（画面の中身は入らない）。Pause.uiscreen に足した Text が入る |
| Pause（一番上）を選んで Delete | 消えない |
| Title を Delete | 消える。**Items の中の並び（Resume → Restart）は変わらない**（パートA の SlotMap の直しの前は、入れ替わっていた） |
| Cancel | ファイルは変わらない。Hierarchy がシーンに戻る |
| Done | 保存して、Hierarchy がシーンに戻る |
| 編集中に Play | 先に保存して閉じ、シーンで再生が始まる |
| Inspector の Edit UI Screen | 編集モードに入る |

ログに警告・エラー無し。素の Release も起動・終了でエラー無し。

名前を `UIScreen` に変えた後も、Release でポーズ → ↓ → やりなおす（`resources/ui/Pause.uiscreen` から読む・「UI Screen」の Component）と、エディタで Pause.uiscreen のダブルクリック → 編集モードを確かめ直した。

確かめていないこと：
- エディタの Play 中の Esc（Release と同じ道を通る）・パッド。
- Create → UI Screen（新しいファイル）と、そこから 6章の手順を全部エディタで作ること（テストは手で書いたファイル）。
- UI Screen Element の Override Hide・Delay、Motion の Scale と Side の Left/Right/Top（コードは同じ道）。

---

## 8. 決めたこと・後でやること
- **シーンをまたいで開いたままにする**のは提出の後（今はシーンが変わると閉じる。クリア画面 → 次へは、幕が隠すので困らない）。
- **画面の中に Model（プレイヤー）**を置くのは提出の後（Culling Mask と、RenderTexture を Image に貼る形が要る）。
- Hierarchy で**並び替える**機能は無い（並びは作った順）。間違えたら、前に来てほしくない方を Copy → Paste（一番後ろにできる）→ 元を Delete で後ろへ回す。Undo で消した物を戻すと、一番後ろに戻る。
- 編集中に Light・3D Object も作れる（画面の子になる）が、画面の中で使うことは考えていない。
- 画面を閉じた後も、Esc を押すたびに Prefab の読み込みはしない（1回読んだ物を使い回す。エディタで保存し直すと中身も差し替わる）。
- 選んだときの音・決めたときの音は無い（要るなら PauseMenu に Audio Source を足す）。
