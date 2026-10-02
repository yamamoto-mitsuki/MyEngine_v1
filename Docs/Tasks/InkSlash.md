# InkSlash：見下ろしインクスラッシュに要るエンジンの物（軌跡・時間・レイキャスト）

## このファイルについて

- 2026-10-02 の依頼：AL4_01 の `Game.md`（見下ろしのインクスラッシュ。タイムスロー中にマウスで線を描き、離すとその線に沿って高速で進んで敵を斬る）を作るのに、エンジンに足りない物を足す。「**軌跡の点は、エンジンの Trail Component として作るか？**」「**エンジン側のゲーム案（ActionGame.md）はもう要らない**」。
- 先に結論：
  - **軌跡は Trail Component にした**（0章）。Component には「どう描くか」の設定だけを持たせ、点の列は **TrailSystem** が持つ（AudioSource と鳴っている音の関係と同じ）。
  - ほかに足す物は5つ（G1〜G5）。**一番大事なのは G1**：今は `Time::SetTimeScale(0.1f)` が**次のフレームで 1 に戻される**ので、スローもヒットストップも効かない（1章のバグ1）。
  - 調べている途中で、**バグを3つ**見つけた（1章）。
  - **ActionGame.md は消した。** それを指していた所（Reference.md・SceneFlow.md）も直した。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階。
- ブランチは `feature/inkslash`（`develop` の先）。
- **答え合わせ用のブランチ `claude/inkslash-reference`**（ローカルだけ・push していない）に、このファイルのコードを全部入れた物がある。写し終えたら `git diff claude/inkslash-reference -- MyEngine` で見比べると、写し違いが見つかる。
- 写経用のコードは、**全部を写した状態で** Debug・Release のビルドと実行を確かめた（ゲーム側の例も含む。5章の結果）。Step は「前の Step だけで途中でもビルドが通る」順に並べたが、途中の状態は1つずつは試していない。

| 読む順番 | 何が書いてあるか |
|---|---|
| [0. 質問への答え](#0-質問への答え) | 軌跡を Trail Component にするか・ActionGame.md |
| [1. 見つけたバグ](#1-見つけたバグ3つ) | スローが効かない・行列の参照が壊れる・グリッドのモデルが git に無い |
| [2. 作った物](#2-作った物設計) | G1〜G5 と Trail の設計 |
| [3. 手順](#3-手順写経するコード) | 写経するコード（Step 0〜6） |
| [4. ゲーム側の例](#4-ゲーム側の例al4_01) | AL4_01 に置く InkPlayer・InkWall・InkEnemy（描く・進む・斬る・回収） |
| [5. 確認すること](#5-確認すること) | 写したあとに実行して見ること |
| [6. 決めたこと](#6-決めたこと) | 理由の一覧 |
| [7. 未解決](#7-未解決) | やっていないこと |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| 0 | `grid.obj` を git に入れる（`.gitignore` の例外） | 写経待ち |
| 1 | **G1** 時間：ゲームのスロー・ヒットストップを一時停止と分ける | 写経待ち |
| 2 | **G2** カメラ：`ScreenPointToRay`（と `GetViewProjectionMatrix` の直し） | 写経待ち |
| 3 | **G4 の形** カプセルと、線分の一番近い点 | 写経待ち |
| 4 | **G3・G4** CollisionSystem：`Raycast`・`SweepSphere` | 写経待ち |
| 5 | **G5** Renderer：`DrawMesh`（自分で並べた形を描く） | 写経待ち |
| 6 | **Trail**：`TrailComponent`・`TrailSystem` とエンジンへの組み込み | 写経待ち |
| G | ゲーム側（AL4_01）：4章の例を写して、シーンに置く | 写経待ち |

**Step 4 は Step 3 の後**（カプセルを使う）。**Step 6 は Step 5 の後**（帯を `DrawMesh` で描く）。ほかはどれを先にしてもよい。ゲーム側（G）は全部の後。

---

## 0. 質問への答え

### 軌跡の点は、エンジンの Trail Component にするか → する

- **Component に点の列（`std::vector`）は入れられない。** Undo・保存・Play / Stop は Component をバイト列で丸ごと写すので、ヒープを指す物が入っていると壊れる（ARCHITECTURE.md 原則2。`static_assert` で止まる）。
- なので2つに分ける。**AudioSource（Component）と、鳴っている音（AudioSystem が持つ）の関係と同じ形。**

| 置き場所 | 持つ物 | 保存されるか |
|---|---|---|
| `TrailComponent`（Component） | どう描くか：太さ・色・テクスチャ・消えるまでの秒・向き（カメラ向き／地面に寝かせる）・自動で点を足すか | される（Inspector で触れる） |
| `TrailSystem`（System） | 点の列（Entity ごと） | されない（実行中だけ。停止中は持たない） |

- 使い方は2つ（Unity の Trail Renderer と同じ考え＋自分で点を足す使い方）。

| 使い方 | 設定 | Game.md のどこ |
|---|---|---|
| **動いた跡** | `emitting = true`（Entity が動くたびに自動で点が増え、`lifeTime` 秒で消える） | 高速移動の残像 |
| **描いた線** | `emitting = false`、`lifeTime = 0`（消えない）、`alignment = Ground`。点は `TrailSystem::AddPoint` で足す | マウスで描くインクの線・地面に残るインク |

- 描いた線は「**線1本＝Entity 1つ**」。描き始めに `EntityManager::Create` して `RequestAdd<TrailComponent>`、同じフレームから `AddPoint` してよい（付くのは次のフレームだが、TrailSystem は点を捨てずに待つ）。回収したら `Destroy` すれば点も消える。

### ActionGame.md は消した

- 2D 横スクロールの案（E1〜E5）はやめて、課題のゲームは AL4_01 の InkSlash にする。
- ActionGame.md を指していた所を直した：Reference.md（19章の「まだ無い物」・関連）、SceneFlow.md（このファイルについて・進捗の表・4章の表・19.6・21章のやること）。SF3 のフェード・SF4 の「空を消す」を「課題のゲームのために先に作る」としていたのも外した（InkSlash は見下ろしで地面しか映らないので、今は要らない）。

---

## 1. 見つけたバグ（3つ）

### 1.1 `Time::SetTimeScale` が効かない（スロー・ヒットストップ）

`SceneManager::Update` が、**毎フレーム `IScene::Update` の直前に** `Time::SetTimeScale(一時停止なら0、そうでなければ1)` を呼んでいる。一時停止を「時間を0にする」で作っているため、**ゲームが入れた 0.1 は次のフレームの頭で 1 に戻る**。SYSTEM に渡る `deltaTime` は戻した後の値で計算されるので、スローは1フレームも効かない（パーティクルだけ、同じフレームの後ろで1フレーム遅れて効く）。

→ **掛け算を分ける**（Step 1）。一時停止（エンジンが書く）・ゲームのスロー（ゲームが書く）・ヒットストップ（ゲームが頼む）を別々に持ち、`GetDeltaTime` で全部掛ける。`SetTimeScale` は消す（同じ名前を残すと、また「効かない」と迷うので）。

ついでに、**一時停止のまま Stop すると、停止中も時間が0のまま**（停止中は書き直していなかった）だったのも直る（停止中も一時停止の値を書く）。

### 1.2 `Camera::GetViewProjectionMatrix()` が消えた値を指す

```cpp
const Matrix4x4& GetViewProjectionMatrix() const { return Multiply(viewMatrix_, projectionMatrix_); } // ✗
```

`Multiply` の結果（その場で作った値）への**参照**を返しているので、呼んだ直後にその値は消え、読むと壊れた行列になる（未定義動作）。今は誰も呼んでいないので表に出ていなかった。→ **値で返す**（Step 2）。

### 1.3 グリッドのモデル `grid.obj` が git に入っていない

`.gitignore` の `*.obj`（VS の中間ファイル用のルール）に、エディタのグリッドのモデル `Resources/Model/grid.obj` も引っかかっている。**新しくクローンすると、Debug は起動した瞬間に止まる**（`ModelManager.cpp` の assert：`Unable to open file "MyEngine/Resources/Model/grid.obj"`）。作業用のコピーで試していて見つけた。→ `.gitignore` に例外を足す（Step 0）。

---

## 2. 作った物（設計）

| | 作った物 | 使い方（ゲームから） | Game.md のどこ |
|---|---|---|---|
| **G1** | ゲームのスロー・ヒットストップ | `Time::SetGameTimeScale(0.1f)` / `Time::HitStop(0.06f)` | 3-1 ドラッグ中のスロー、ヒットストップ |
| **G2** | 画面の位置 → ワールドのレイ | `CameraSystem::GetMainCamera()->ScreenPointToRay(InputManager::GetMousePositionInGame())` | 4-A スクリーン → ワールド |
| **G3** | Collider へのレイキャスト | `CollisionSystem::Raycast(ray, maxDistance, hit, 絞り込み)` | 3-2 壁で線を止める |
| **G4** | スイープ（動いた跡のカプセルに触れた物） | `CollisionSystem::SweepSphere({start, end, radius}, out, 絞り込み)` | 4-C 高速移動中の斬撃 |
| **G5** | 自分で並べた形を描く | `Renderer::DrawMesh(config, vertices, indices)` | 4-D 帯（Trail が中で使う） |
| **Trail** | 軌跡（動いた跡・描いた線を帯で描く） | `TrailComponent` ＋ `TrailSystem::AddPoint / Clear / GetPoints / CalcLength` | 4-D インクの線・地面のインク |
| 数学 | 線分の上で一番近い点 | `Collision::ClosestPointOnSegment(point, a, b)` | 4-D インクの回収 |

### G1 時間

```
GetDeltaTime() = 前フレームからの秒 × 一時停止（0か1）× ゲームのスロー ×（ヒットストップ中なら 0）
```

- **一時停止**はエンジン（`SceneManager`）が毎フレーム書く。**ゲームのスロー**はゲームが書き、エンジンは触らない。だから書き直されても消えない。
- **ヒットストップ**は「残り秒」を持ち、**何も掛けていない時間で**減らす（スロー中でも同じ長さだけ止まる）。一時停止中は減らない。重ねて呼んだら長い方。
- **Play / Stop / Restart・シーンの切り替えで、スローとヒットストップは戻る**（`ChangeScene` の頭で `Time::ResetGameTime()`）。スロー中に Stop して、エディタまでスローのまま…にならない。

### G2 ScreenPointToRay

画面の同じ位置の「近い面の点（z=0）」と「遠い面の点（z=1）」を、ビュー・プロジェクションの逆行列でワールドに戻し、**w で割って**から差を向きにする（`DrawFrustum` の角の計算と同じやり方）。始点は近クリップ面の上、向きは長さ1。正射影でもそのまま動く。

- 引数は `GetMousePositionInGame()` の値（真ん中0、右と上が+、端が±1）をそのまま渡す。Game ビューも、カメラの縦横比も 16:9 なので、そのまま NDC として使える。

### G3・G4 その場で調べる（Raycast・SweepSphere）

- 今までの `GetHits` は「フレームの最後に1回作った**重なりの一覧**」。こちらは SYSTEM の中で**何回でも、その場で**聞ける。
- 調べる相手の形は、**前のフレームの `CollisionSystem::Update` が集めた物**（`state->colliders`。ギズモの線と同じ物）。動かない壁なら気にしなくてよい。毎回 ForEach で作り直さないのは、作り直してもワールド行列が同じ（前のフレームの値）なので変わらず、遅くなるだけだから。
- そのため、**Play・シーン切り替えの直後の1フレームは何にも当たらない**（作り直す前の Entity の形しか無く、生きていないので飛ばす）。
- **レイヤーは無い**ので、相手は「**持っている Component**」で絞る（`CollisionFilter`。空なら全部）。Reference.md の「相手が何者かは Component で見分ける」と同じ考え。
- `SweepSphere` は「球が start から end まで動いた跡（カプセル）」に触れた物を全部集める。**速く動くと1フレームで敵を飛び越える**ので、フレームの最後に1回だけ重なりを見る Collider ではすり抜ける。動いた跡で調べればすり抜けない。
- カプセルと箱は、**箱を半径ぶん広げた箱と線分**で調べる（本当は辺・角が丸いが、そこだけ少し大きめに当たる。ゲームでは気にならない大きさ）。

### G5 DrawMesh

- `Renderer` の中では、三角形・四角・球・箱がみんな `PushMesh`（頂点と番号の配列を受け取る）を通っている。それを外から呼べるようにしただけ。
- 頂点はワールド座標。**既定は両面描き**（`SolidNone`）にした。帯は並べる向きで裏表が変わるので、気にしなくてよいように。
- 頂点と番号は Config に持たせず、引数で渡す（Config に入れると毎フレーム配列ごと写すことになる。`std::move` で渡せば写さない）。

### Trail

- **毎フレームの流れ**：`TrailSystem::Update`（`FlushDestroy` の後）で、消えた・外した Trail の点を捨て、点を古くして `lifeTime` を過ぎた物を消し、`emitting` なら今の位置を足す（`minVertexDistance` 動いたら1点）。`TrailSystem::Draw`（`DrawAll` の中、パーティクルの後）で帯を作って `DrawMesh`。
- **帯の作り方**：点1つにつき左右2頂点。左右の向きは「**進む向き × 面の向き**」。面の向きは `Ground` なら真上、`View` ならその点からカメラへの向き。太さは新しい方の端が `width`、古い方の端が `width × endWidthRate`。U は古い方 0 → 新しい方 1、V は幅の向き（テクスチャでグラデーション・先の溶け方を作れる）。
- **Emitting の帯の先は、今の Entity の位置にくっつける**（点を足すのは `minVertexDistance` ごとなので、そのままだと先が遅れて見える。描くときだけ今の位置を足す）。
- **停止中は点を持たない**（エディタで Entity を動かしても跡は出ない。Play で最初から）。
- **光の当たり方は使わない**（Unlit 固定）。深度は「テストするが書かない」（重なった帯・半透明が欠けないように）。重ね方は `Normal` と `Add`（光る線）。

---

## 3. 手順（写経するコード）

### Step 0：`grid.obj` を git に入れる

**.gitignore**（一番下に足す）

```gitignore
# エンジンのモデル（上の *.obj は VS の中間ファイル用。モデルの .obj まで消さないように戻す）
!Resources/Model/*.obj
```

足したら `git status` に `Resources/Model/grid.obj` が出るので、コミットに入れる。

### Step 1：時間（G1）

**MyEngine/Time/Time.h**（全部書き換え）

```cpp
#pragma once

// 時間計測クラス
// 経過時間には「一時停止（エンジン）」「ゲームのスロー（ゲーム）」「ヒットストップ（ゲーム）」の3つが掛かる
// 一時停止とゲームのスローを別々に持つのは、SceneManager が毎フレーム一時停止の値を書き直しても、ゲームのスローが消えないようにするため
class Time {
public:
	static void Initialize() { instance_ = new Time(); }
	static void Release() {
		delete instance_;
		instance_ = nullptr;
	};

	// ===== 経過時間 =====
	// 前フレームからの秒（一時停止・ゲームのスロー・ヒットストップを掛けた物）。ゲームの動きは全部これで
	static float GetDeltaTime() { return instance_->deltaTime_ * GetTimeScale(); }
	// 何も掛けていない秒（UIの演出など、スロー・一時停止で止めたくない物に）
	static float GetUnscaleDeltaTime() { return instance_->deltaTime_; }
	// 今掛かっている倍率（一時停止 × ゲームのスロー。ヒットストップ中は0）
	static float GetTimeScale() {
		if (instance_->hitStopRemaining_ > 0.0f) {
			return 0.0f;
		}
		return instance_->pauseScale_ * instance_->gameTimeScale_;
	}

	// ===== ゲームから使う =====
	// ゲームのスロー（1で普通、0.1で10分の1の速さ）。Play / Stop / Restart・シーンの切り替えで1に戻る
	static void SetGameTimeScale(float scale) { instance_->gameTimeScale_ = (scale < 0.0f) ? 0.0f : scale; }
	static float GetGameTimeScale() { return instance_->gameTimeScale_; }
	// ヒットストップ（seconds 秒だけ時間を止める）。止まっている間にもう一度呼んだら、残りの長い方
	// 秒は何も掛けていない時間で数える（スロー中でも同じ長さだけ止まる）。一時停止中は減らない
	static void HitStop(float seconds) {
		if (seconds > instance_->hitStopRemaining_) {
			instance_->hitStopRemaining_ = seconds;
		}
	}
	static bool IsHitStopping() { return instance_->hitStopRemaining_ > 0.0f; }

	// ===== エンジンから使う =====
	// フレームの頭に1回（Engine::BeginFrame）。ヒットストップの残りもここで減らす
	static void SetDeltaTime(float time) {
		instance_->deltaTime_ = time;
		if (instance_->pauseScale_ > 0.0f && instance_->hitStopRemaining_ > 0.0f) {
			instance_->hitStopRemaining_ -= time;
			if (instance_->hitStopRemaining_ < 0.0f) {
				instance_->hitStopRemaining_ = 0.0f;
			}
		}
	}
	// 一時停止（SceneManager が毎フレーム書く。一時停止中0、それ以外1）
	static void SetPauseScale(float scale) { instance_->pauseScale_ = scale; }
	// ゲームのスローとヒットストップを戻す（SceneManager がシーンを作り直すときに呼ぶ）
	static void ResetGameTime() {
		instance_->gameTimeScale_ = 1.0f;
		instance_->hitStopRemaining_ = 0.0f;
	}

private:
	static inline Time* instance_ = nullptr;
	float deltaTime_ = 0.0f;        // 前フレームからの秒（何も掛けていない）
	float pauseScale_ = 1.0f;       // 一時停止（エンジンが書く）
	float gameTimeScale_ = 1.0f;    // ゲームのスロー（ゲームが書く）
	float hitStopRemaining_ = 0.0f; // ヒットストップの残りの秒
};
```

**MyEngine/Scene/SceneManager.cpp**（`Update` の中。一時停止の行を `if` の外へ出し、名前を変える）

```cpp
	// 前
	// 停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ != PlayState::Editing) {
		// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
		Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
		currentScene_->Update();
```

```cpp
	// 後
	// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
	// ゲームのスロー（Time::SetGameTimeScale）は別に持っているので、ここで書き直しても消えない
	// 停止中も書く（一時停止のまま Stop したとき、停止中の時間が0のまま残らないように）
	Time::SetPauseScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);

	// 停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ != PlayState::Editing) {
		currentScene_->Update();
```

**MyEngine/Scene/SceneManager.cpp**（`ChangeScene` の「1. 古いシーンの片付け」の見出しのすぐ下に2行）

```cpp
	// ===== 1. 古いシーンの片付け（カメラ・IBLなど、Entityではない物）=====
	// スロー・ヒットストップの途中で Stop・シーン切り替えをしても、次のシーンへ持ち越さない
	Time::ResetGameTime();
	const bool oldUsesFile = currentScene_ && currentScene_->GetSceneFile() != nullptr;
```

> `Time::SetTimeScale` を呼んでいる所は、エンジンには SceneManager の1か所しか無い（CG3_Project も呼んでいない）。ゲームで呼んでいたら `SetGameTimeScale` に変える。

### Step 2：カメラ（G2）

**MyEngine/Camera/Camera.h**

```cpp
// include を2つ足す
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Transform.h"
#include "MyEngine/Math/Geometry/Ray.h"
```

```cpp
	// CalcWVP の下に足す
	/// <summary>
	/// 画面の位置から、カメラの奥へ向かうレイを作る（マウスで地面・物を指す。Unity の Camera.ScreenPointToRay）
	/// <para>始点は近クリップ面の上、向きは長さ1。最後の Update の行列を使う（Camera Component のカメラは CameraSystem が毎フレーム更新している）</para>
	/// </summary>
	/// <param name="ndc">画面の位置（InputManager::GetMousePositionInGame() の値。真ん中0、右と上が+、端が±1）</param>
	Ray ScreenPointToRay(const Vector2& ndc) const;
```

```cpp
	// 前
	const Matrix4x4& GetViewProjectionMatrix() const { return Multiply(viewMatrix_, projectionMatrix_); }
	// 後（1.2 のバグ）
	// 掛け算した結果を返す（その場で作る値なので、参照ではなく値で返す。参照で返すと、呼んだ直後に消えた値を指す）
	Matrix4x4 GetViewProjectionMatrix() const { return Multiply(viewMatrix_, projectionMatrix_); }
```

**MyEngine/Camera/Camera.cpp**（一番下に足す）

```cpp
//=============================================================================
// 画面の位置からレイを作る
//=============================================================================
Ray Camera::ScreenPointToRay(const Vector2& ndc) const {
	// 画面の同じ位置にある「近い面の点（z=0）」と「遠い面の点（z=1）」を、ビュー・プロジェクションの逆行列でワールドへ戻す
	// DirectX の NDC は x・y が -1〜1（上が+）、z が 0〜1。DrawFrustum の角の計算と同じやり方
	// 戻すときは w で割る（MathUtility::TransformPoint の中で割っている）。透視投影は w で割って縮めた物なので、割らないと位置がずれる
	const Matrix4x4 inverseViewProjection = Inverse(Multiply(viewMatrix_, projectionMatrix_));
	const Vector3 nearPoint = MathUtility::TransformPoint({ndc.x, ndc.y, 0.0f}, inverseViewProjection);
	const Vector3 farPoint = MathUtility::TransformPoint({ndc.x, ndc.y, 1.0f}, inverseViewProjection);
	// 正射影でも同じ式で動く（近い点が画面の位置ごとにずれ、向きはどこでも同じになる）
	return {nearPoint, Normalize(farPoint - nearPoint)};
}
```

### Step 3：カプセルと、線分の一番近い点（G4 の形）

**MyEngine/Math/Geometry/Capsule.h**（新しいファイル。VS で `Math/Geometry` に追加）

```cpp
#pragma once
#include "MyEngine/Math/Vector3.h"

// 当たり判定に使うカプセル（線分 start → end を、半径 radius で太らせた形）
// 「球が start から end まで動いた跡」と同じ形なので、速く動く物のすり抜けを防ぐ判定に使う
struct Capsule {
	Vector3 start; // 線分の始点
	Vector3 end;   // 線分の終点
	float radius;  // 半径（0なら線分そのもの）
};
```

**MyEngine/Collision/Collision.h**

```cpp
// include を1つ足す（AABB の下）
#include "MyEngine/Math/Geometry/Capsule.h"
```

```cpp
// ===== 形状同士 ===== の SphereAABB の下に足す
// カプセル（動いた球の跡）と球・箱。速く動く物が、1フレームで相手を飛び越えても当たる
bool CapsuleSphere(const Capsule& capsule, const Sphere& sphere);
bool CapsuleAABB(const Capsule& capsule, const AABB& aabb); // 箱の角は少し大きめに当たる（角を丸めずに、箱を半径ぶん広げて調べるため）
```

```cpp
// ===== ユーティリティ ===== の ClosestPointOnAABB の下に足す
Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end); // 線分の上で point に一番近い点
```

**MyEngine/Collision/Collision.cpp**（`SphereAABB` の下、「レイキャスト」の見出しの上に足す）

```cpp
// ===== カプセルと球 =====
bool CapsuleSphere(const Capsule& capsule, const Sphere& sphere) {
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	// 線分の上で球の中心に一番近い点を探し、その点と中心の距離が「半径の和」以下なら当たり
	// （カプセル＝線分の全部の点に球を置いた形なので、一番近い点の球が当たるかだけ見ればよい）
	const Vector3 closest = ClosestPointOnSegment(sphere.center, capsule.start, capsule.end);
	const float radiusSum = capsule.radius + sphere.radius;
	return LengthSq(sphere.center - closest) <= radiusSum * radiusSum;
}

// ===== カプセルと箱 =====
bool CapsuleAABB(const Capsule& capsule, const AABB& aabb) {
#ifdef _DEBUG
	COLLISION_PROFILE();
#endif
	// 箱を半径ぶん広げ、線分（start → end）がその箱を通るかで調べる
	// 本当は角が丸い形（角では少し小さい）だが、広げた箱で近似する。角の近くだけ少し大きめに当たる
	const Vector3 grow = {capsule.radius, capsule.radius, capsule.radius};
	const AABB expanded = {aabb.min - grow, aabb.max + grow};
	// 向きを長さ1にしないレイにすると、t が 0〜1 の間＝線分の上で当たったことになる
	// start が箱の中なら RayAABB は t=0 で当たりを返す。start と end が同じ点（動いていない）でも、箱の中なら当たり
	const Ray ray = {capsule.start, capsule.end - capsule.start};
	float t = 0.0f;
	return RayAABB(ray, expanded, &t) && t <= 1.0f;
}
```

**MyEngine/Collision/Collision.cpp**（一番下の `RayPoint` の上に足す）

```cpp
Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end) {
	// 線分の向き ab に point を射影した長さの割合 t（0 で start、1 で end）を、0〜1 に収める
	const Vector3 ab = end - start;
	const float lengthSq = LengthSq(ab);
	if (lengthSq < 1e-12f) {
		return start; // 長さ0の線分（点）
	}
	float t = Dot(point - start, ab) / lengthSq;
	t = std::clamp(t, 0.0f, 1.0f);
	return start + ab * t;
}
```

### Step 4：CollisionSystem の Raycast・SweepSphere（G3・G4）

**MyEngine/Systems/CollisionSystem.h**（頭。include を足し、`RaycastHit` と `CollisionFilter` を足す）

```cpp
#pragma once
#include <functional>
#include <span>
#include <vector>

#include "MyEngine/Components/ColliderComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Geometry/Capsule.h"
#include "MyEngine/Math/Geometry/Ray.h"

// 前方宣言
class Camera;


/// <summary>
/// レイキャストで当たった物
/// </summary>
struct RaycastHit {
	Handle<Entity> entity; // 当たったEntity
	Vector3 point;         // 当たった位置（ワールド座標）
	float distance = 0.0f; // レイの始点から当たった位置までの距離
};

// どのEntityを調べるか（true を返したEntityだけ調べる）。空なら全部
// レイヤーの代わりに「相手が持っている Component」で絞る：[](Handle<Entity> e) { return EntityManager::Get<Wall>(e) != nullptr; }
using CollisionFilter = std::function<bool(Handle<Entity>)>;
```

**MyEngine/Systems/CollisionSystem.h**（`GetAllHits` の下に足す）

```cpp
	// ===== その場で調べる（SYSTEM の中で、何回呼んでもよい）=====
	// 調べる相手の形は、前のフレームの Update で集めた物（1フレームの順番の 9。動かない壁なら気にしなくてよい）
	// Play・シーン切り替えの直後のフレームは、作り直す前の Entity の形しか無いので、何にも当たらない

	/// <summary>
	/// レイを飛ばして、一番近くで当たった Collider を探す（壁で線を止める・地面を指す）
	/// </summary>
	/// <param name="ray">始点と向き（向きの長さは1でなくてよい。中で長さ1にする）</param>
	/// <param name="maxDistance">どこまで調べるか（始点からの距離）</param>
	/// <param name="hit">当たった物が入る（当たらなければ変えない）</param>
	/// <param name="filter">調べる相手を絞る（空なら全部）</param>
	/// <returns>当たったか。始点が Collider の中なら、距離0で当たる</returns>
	static bool Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, const CollisionFilter& filter = {});

	/// <summary>
	/// 球を start から end まで動かしたときに触れた Collider を全部集める（速く動く物の攻撃。1フレームで飛び越えてもすり抜けない）
	/// </summary>
	/// <param name="capsule">動いた跡（start → end を半径 radius で太らせた形）</param>
	/// <param name="out">触れたEntityが入る（最初に空にする。並びは決まっていない）</param>
	/// <param name="filter">調べる相手を絞る（空なら全部）</param>
	static void SweepSphere(const Capsule& capsule, std::vector<Handle<Entity>>& out, const CollisionFilter& filter = {});
```

**MyEngine/Systems/CollisionSystem.cpp**（`GetAllHits` の下、「ギズモ」の見出しの上に足す）

```cpp
//=============================================================================
// その場で調べる（レイキャスト・スイープ）
//=============================================================================
namespace {
// 調べる相手か（生きていて、絞り込みに合う）
// state->colliders は前のフレームに集めた物なので、作り直しで消えたEntityが残っていることがある
bool IsQueryTarget(const WorldCollider& collider, const CollisionFilter& filter) {
	if (!EntityManager::IsAlive(collider.entity)) {
		return false;
	}
	return !filter || filter(collider.entity);
}

// レイ（向きは長さ1）が Collider に当たるか。当たったら、始点からの距離を t に入れる
bool RayCollider(const Ray& ray, const WorldCollider& collider, float& t) {
	if (collider.shape == ColliderShape::Box) {
		return Collision::RayAABB(ray, collider.box, &t); // 始点が箱の中なら t=0
	}
	// 球：RaySphere は「近い方の交点」を返すので、始点が中のとき・球が後ろにあるときは t がマイナスになる
	if (Collision::IsPointInSphere(ray.origin, collider.sphere)) {
		t = 0.0f;
		return true;
	}
	return Collision::RaySphere(ray, collider.sphere, &t) && t >= 0.0f;
}
} // namespace

bool CollisionSystem::Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, const CollisionFilter& filter) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	// 向きを長さ1にする（t がそのまま距離になるように）
	const float length = Length(ray.direction);
	if (length < 1e-6f) {
		return false; // 向きが無い
	}
	const Ray unitRay = {ray.origin, ray.direction / length};

	// 全部の Collider を調べて、一番近い物を残す（総当たり。Update と同じく数百個まではこれで足りる）
	bool isHit = false;
	float nearest = maxDistance;
	for (const WorldCollider& collider : state->colliders) {
		if (!IsQueryTarget(collider, filter)) {
			continue;
		}
		float t = 0.0f;
		if (!RayCollider(unitRay, collider, t) || t > nearest) {
			continue;
		}
		nearest = t;
		isHit = true;
		hit.entity = collider.entity;
	}
	if (isHit) {
		hit.distance = nearest;
		hit.point = Collision::RayPoint(unitRay, nearest);
	}
	return isHit;
}

void CollisionSystem::SweepSphere(const Capsule& capsule, std::vector<Handle<Entity>>& out, const CollisionFilter& filter) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	out.clear();
	for (const WorldCollider& collider : state->colliders) {
		if (!IsQueryTarget(collider, filter)) {
			continue;
		}
		const bool isTouching = (collider.shape == ColliderShape::Sphere) ? Collision::CapsuleSphere(capsule, collider.sphere) : Collision::CapsuleAABB(capsule, collider.box);
		if (isTouching) {
			out.push_back(collider.entity);
		}
	}
}
```

> `WorldCollider` と `state` は、このファイルの上の無名の namespace にある物をそのまま使う（同じファイルの中なので見える）。

### Step 5：DrawMesh（G5）

**MyEngine/Graphics/Renderer/Renderer.h**（`Quad3dConfig` の下、`// --- AABBを生成 ---` の上に足す）

```cpp
	// --- 頂点を自分で並べる形（軌跡の帯など、毎フレーム形が変わる物）---
	// 頂点と番号は DrawMesh の引数で渡す（ここに持たせると、毎フレーム配列ごと写すことになるので）
	struct MeshConfig {
		uint32_t color = 0xFFFFFFFF;                                // 色
		float alphaCutoff = 0.0f;                                   // これ以下のアルファのピクセルを捨てる
		Transform uvTransform;                                      // UVの拡縮、回転、移動
		uint32_t textureHandle = 0;                                 // テクスチャハンドル
		ShadingType shadingType = ShadingType::Unlit;               // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                    // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidNone;  // ラスタライザ設定（既定は両面。並べ方の向きを気にしなくてよいように）
		DepthMode depthMode = DepthMode::TestWrite;                 // 深度設定
		Camera* camera = nullptr;                                   // カメラ設定
		bool isBillboard = false;                                   // 使わない（頂点はワールド座標で渡す）。PushMesh が読むので置いてある
		std::wstring windowTitle = L"";                             // 描画したいウィンドウ名（指定しないとき、メインウィンドウ）
	};
```

**MyEngine/Graphics/Renderer/Renderer.h**（`DrawQuad3d` の宣言の下に足す）

```cpp
	/// <summary>
	/// 頂点と番号を自分で並べた形の描画リクエストを追加する（1回の描画で出る）
	/// <para>頂点はワールド座標。番号は3つで三角形1つ（0, 1, 2, ...）</para>
	/// </summary>
	/// <param name="vertices">頂点（位置の w は 1）。中で動かすので、使い終わった配列を std::move で渡すと写さずに済む</param>
	/// <param name="indices">三角形の頂点の番号</param>
	static void DrawMesh(const MeshConfig& config, std::vector<Vertex3dData> vertices, std::vector<uint32_t> indices);
```

**MyEngine/Graphics/Renderer/Renderer.cpp**（`DrawQuad3d` の下、`// ===== AABB =====` の上に足す）

```cpp
// ===== 自分で並べた形 =====
void Renderer::DrawMesh(const MeshConfig& config, std::vector<Vertex3dData> vertices, std::vector<uint32_t> indices) {
	// 早期リターン（三角形が1つも無い）
	if (vertices.empty() || indices.size() < 3) {
		return;
	}
	// 頂点はワールド座標なので、ワールド行列は単位行列（DrawQuad3d と同じ）
	PushMesh(config, std::move(vertices), std::move(indices), MakeIdentity4x4());
}
```

### Step 6：Trail

#### 6-1. MyEngine/Components/TrailComponent.h（新しいファイル。VS で `Components` に追加）

```cpp
#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Math/Vector4.h"

// 軌跡（通った跡・描いた線を、帯にして描く）。Entityに付けるComponent（カテゴリ：Rendering3D）
// 点の列は TrailSystem が持つ。Component に可変長の配列は入れられない（ARCHITECTURE.md 原則2。Undo・保存がバイト列で写すため）
// ここは「どう描くか」の設定だけ。AudioSource と鳴っている音の関係と同じ


/// <summary>
/// 帯の向き
/// </summary>
enum class TrailAlignment {
	View,   // カメラの方を向く（動いた跡・剣の残像）
	Ground, // 地面に寝かせる（帯の面が真上を向く。上から見る線・地面に残るインク）
};

/// <summary>
/// 重ね方
/// </summary>
enum class TrailBlend {
	Normal, // 普通（透けるのはテクスチャと色のアルファの分）
	Add,    // 足す（光る線。重なるほど明るい）
};

/// <summary>
/// 軌跡の設定
/// </summary>
struct TrailComponent {
	bool enabled = true;                             // false の間は描かない（点は消さずに残す）
	bool emitting = true;                            // true：Entityが動いたら自動で点を足す（動いた跡）／false：TrailSystem::AddPoint で足す（描いた線）
	float minVertexDistance = 0.1f;                  // 自動で足すとき、この距離動くたびに1点足す
	float lifeTime = 0.5f;                           // 点が消えるまでの秒（0 で消えない。Clear か Entityを消すまで残る）
	int maxPoints = 256;                             // 点の上限（超えたら古い方から消す）
	float width = 0.3f;                              // 太さ（新しい方の端）
	float endWidthRate = 0.0f;                       // 古い方の端の太さ（width に掛ける。1 でずっと同じ太さ、0 で先が細くなる）
	TrailAlignment alignment = TrailAlignment::View; // 帯の向き
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};        // 色（RGBA。0〜1）
	uint32_t textureHandle = 0;                      // 0 なら白（色だけ）。U が古い方 0 → 新しい方 1、V が帯の幅の向き
	TrailBlend blend = TrailBlend::Normal;           // 重ね方
};

static_assert(std::is_trivially_copyable_v<TrailComponent>);
```

> `BlendMode`（`RenderStates.h`）を使わず、`TrailBlend` を作ったのは、`RenderStates.h` が `d3d12.h` を読むから。このヘッダは `GameComponent.h` を通ってゲームの全部のファイルに入るので、軽くしておく。

#### 6-2. MyEngine/Systems/TrailSystem.h（新しいファイル。VS で `Systems` に追加）

```cpp
#pragma once
#include <span>

#include "MyEngine/Components/TrailComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Vector3.h"

// 前方宣言
class Camera;


/// <summary>
/// 軌跡の点1つ
/// </summary>
struct TrailPoint {
	Vector3 position; // ワールド座標
	float age = 0.0f; // 足してからの秒（Time::GetDeltaTime で数える。スロー・一時停止で遅くなる・止まる）
};


/// <summary>
/// TrailComponent を持つEntityの点の列を持ち、帯にして描く
/// <para>点の列は実行中だけの状態（保存しない。Play / Stop で作り直すと消える。停止中は持たない）</para>
/// <para>一覧などの中身は TrailSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class TrailSystem {
public:
	// TrailComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::FlushDestroy の後に呼ぶ（ワールド行列も計算済み）
	/// <para>消えた・外した Trail の点を捨て、点を古くして寿命の物を消し、Emitting の Trail に今の位置を足す</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false＝点を全部捨てる）</param>
	/// <param name="deltaTime">経過秒（Time::GetDeltaTime。スロー・一時停止が掛かった物）</param>
	static void Update(bool isGameRunning, float deltaTime);

	// 帯の描画リクエストを積む（WindowManager::DrawAll から）。camera は帯をカメラへ向けるのに使う
	static void Draw(Camera* camera);

	// ===== ゲームから（SYSTEM の中で）使う =====

	// 点を1つ足す（新しい方の端へ）。TrailComponent を RequestAdd したのと同じフレームに呼んでもよい
	static void AddPoint(Handle<Entity> entity, const Vector3& position);
	// 点を全部消す
	static void Clear(Handle<Entity> entity);
	// 点の列（古い方 → 新しい方）。無ければ空。次に点を足す・Update するまで有効（覚えておかない）
	static std::span<const TrailPoint> GetPoints(Handle<Entity> entity);
	// 線の長さ（点と点の距離の合計）。呼ぶたびに数える
	static float CalcLength(Handle<Entity> entity);
};
```

#### 6-3. MyEngine/Systems/TrailSystem.cpp（新しいファイル。VS で `Systems` に追加）

```cpp
#include "TrailSystem.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

namespace {
constexpr int kMinPoints = 2;                          // 帯を描くのに要る点の数（Max Points の下限にも使う）
constexpr float kSamePointDistance = 1e-4f;            // これより近い点は同じ点とみなす（向きが計算できないので）
constexpr Vector3 kGroundNormal = {0.0f, 1.0f, 0.0f};  // Ground の帯の面の向き（真上）

/// <summary>
/// Trail 1つ分の、実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct TrailState {
	Handle<Entity> entity;           // 持ち主
	std::vector<TrailPoint> points;  // 点の列（古い方 → 新しい方）
};

/// <summary>
/// TrailSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct TrailSystemState {
	std::vector<TrailState> trails; // 点を持っている Trail の一覧（Trail の数しか無いので、順に見れば足りる）
	std::vector<Vector3> path;      // 描くときの点の置き場（Draw の中だけで使う。配列のメモリを使い回す）
};
std::unique_ptr<TrailSystemState> state;

// Entityの状態を探す（無ければ nullptr）
TrailState* FindTrail(Handle<Entity> entity) {
	const auto it = std::ranges::find(state->trails, entity, &TrailState::entity);
	return (it != state->trails.end()) ? &*it : nullptr;
}

// Entityの状態を探し、無ければ足す
TrailState& FindOrAddTrail(Handle<Entity> entity) {
	if (TrailState* trail = FindTrail(entity)) {
		return *trail;
	}
	state->trails.push_back({entity, {}});
	return state->trails.back();
}

// 上限を超えた分を、古い方から消す
void TrimPoints(std::vector<TrailPoint>& points, int maxPoints) {
	const size_t limit = static_cast<size_t>((std::max)(maxPoints, kMinPoints));
	if (points.size() > limit) {
		points.erase(points.begin(), points.begin() + static_cast<std::ptrdiff_t>(points.size() - limit));
	}
}

// 0〜1 の色を 0xRRGGBBAA に詰める（Renderer の色の形）
uint32_t PackColor(const Vector4& color) {
	const auto toByte = [](float value) { return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
	return (toByte(color.x) << 24) | (toByte(color.y) << 16) | (toByte(color.z) << 8) | toByte(color.w);
}

//=============================================================================
// 帯を作る
//=============================================================================
// 点の列から帯の頂点と番号を作る（点1つにつき、左右に2頂点。点と点の間を三角形2つでつなぐ）
//
//   古い方                  新しい方
//   0 ── 2 ── 4 ── ...   ← 左（side の向き）
//   │ ＼ │ ＼ │
//   1 ── 3 ── 5 ── ...   ← 右
//
// 左右の向き（side）は「進む向き × 面の向き」。面の向きは Ground なら真上、View ならその点からカメラへの向き
void BuildRibbon(const std::vector<Vector3>& path, const TrailComponent& trail, const Camera* camera, std::vector<Vertex3dData>& vertices, std::vector<uint32_t>& indices) {
	const size_t count = path.size();
	vertices.reserve(count * 2);
	indices.reserve((count - 1) * 6);

	Vector3 previousSide = {1.0f, 0.0f, 0.0f}; // 左右が計算できない点（進む向きと面の向きが同じ）は、1つ前の点と同じにする
	for (size_t i = 0; i < count; ++i) {
		// --- 進む向き（前後の点の差。端は自分と隣） ---
		const Vector3& previous = path[(i == 0) ? 0 : i - 1];
		const Vector3& next = path[(i + 1 < count) ? i + 1 : i];
		const Vector3 tangent = next - previous;

		// --- 面の向き ---
		Vector3 normal = kGroundNormal;
		if (trail.alignment == TrailAlignment::View && camera != nullptr) {
			const Vector3 toCamera = camera->GetTranslation() - path[i];
			if (LengthSq(toCamera) > kSamePointDistance * kSamePointDistance) {
				normal = Normalize(toCamera);
			}
		}

		// --- 左右の向き ---
		Vector3 side = Cross(tangent, normal);
		if (LengthSq(side) < kSamePointDistance * kSamePointDistance) {
			side = previousSide;
		} else {
			side = Normalize(side);
		}
		previousSide = side;

		// --- 太さ（古い方の端 endWidthRate → 新しい方の端 1） ---
		const float rate = static_cast<float>(i) / static_cast<float>(count - 1); // 0 が古い方、1 が新しい方
		const float halfWidth = trail.width * 0.5f * MathUtility::Lerp(trail.endWidthRate, 1.0f, rate);
		const Vector3 left = path[i] + side * halfWidth;
		const Vector3 right = path[i] - side * halfWidth;
		vertices.push_back({{left.x, left.y, left.z, 1.0f}, {rate, 0.0f}, normal});
		vertices.push_back({{right.x, right.y, right.z, 1.0f}, {rate, 1.0f}, normal});
	}

	// --- 点と点の間を三角形2つでつなぐ（両面で描くので、並べる向きは気にしなくてよい） ---
	for (size_t i = 0; i + 1 < count; ++i) {
		const uint32_t base = static_cast<uint32_t>(i * 2);
		indices.insert(indices.end(), {base, base + 1, base + 2, base + 1, base + 3, base + 2});
	}
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// Trail の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Collider・AudioSource と同じ書き方）
void DescribeTrail(ComponentUI& ui, TrailComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと描かない（点は残る）"));
	ui.Field("Emitting###emitting", value.emitting, Tip("動いたら自動で点を足す。外すと TrailSystem::AddPoint で足した点だけ（描いた線）"));
	ui.Field("Min Vertex Distance###minVertexDistance", value.minVertexDistance, AtLeast(0.0f).Tip("自動で足すとき、この距離動くたびに1点"));
	ui.Field("Life Time###lifeTime", value.lifeTime, AtLeast(0.0f).Tip("点が消えるまでの秒。0 で消えない"));
	ui.Field("Max Points###maxPoints", value.maxPoints, AtLeast(static_cast<float>(kMinPoints)).Tip("超えたら古い方から消す"));
	ui.Separator("Appearance");
	ui.Field("Width###width", value.width, AtLeast(0.0f).Tip("新しい方の端の太さ"));
	ui.Field("End Width Rate###endWidthRate", value.endWidthRate, Range(0.0f, 1.0f).Tip("古い方の端の太さ（Width に掛ける）。1 でずっと同じ太さ"));
	ui.Field("Alignment###alignment", value.alignment, Tip("View：カメラの方を向く／Ground：地面に寝かせる"));
	ui.ColorField("Color###color", value.color);
	ui.AssetField("Texture###texture", value.textureHandle, AssetType::Texture, Tip("U が古い方 0 → 新しい方 1、V が幅の向き。無ければ白"));
	ui.Field("Blend###blend", value.blend, Tip("Add で光る線（重なるほど明るい）"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void TrailSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<TrailSystemState>();

	// Trail の置き場所・保存・Inspector を登録する。見せ方は DescribeTrail 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<TrailComponent>();
	ComponentSerializer::Register<TrailComponent>("Trail", &DescribeTrail);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<TrailComponent>>("Trail", "Effects", &DescribeTrail));
}

void TrailSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void TrailSystem::Update(bool isGameRunning, float deltaTime) {
	MY_ASSERT_MSG(state != nullptr, "TrailSystem::Initialize()を先に呼んでください");
	std::vector<TrailState>& trails = state->trails;

	// ===== 停止中は点を持たない（次に Play したときは、何も無いところから）=====
	if (!isGameRunning) {
		trails.clear();
		return;
	}

	// ===== 1. 消えた・外した Trail の点を捨てる =====
	// RequestAdd したばかり（次のフレームに付く）の Trail は残す（同じフレームに AddPoint した点を捨てないように）
	std::erase_if(trails, [](const TrailState& trail) {
		if (!EntityManager::IsAlive(trail.entity)) {
			return true;
		}
		return EntityManager::Get<TrailComponent>(trail.entity) == nullptr && !EntityManager::IsAddPending<TrailComponent>(trail.entity);
	});

	// ===== 2. 点を古くして、寿命が来た物を消す。Emitting なら今の位置を足す =====
	EntityManager::ForEach<TrailComponent>([deltaTime](Handle<Entity> entity, const TrailComponent& component) {
		TrailState* trail = FindTrail(entity);
		// --- 古くする・消す ---
		if (trail != nullptr) {
			for (TrailPoint& point : trail->points) {
				point.age += deltaTime;
			}
			if (component.lifeTime > 0.0f) {
				std::erase_if(trail->points, [&component](const TrailPoint& point) { return point.age >= component.lifeTime; });
			}
		}
		// --- 動いた分だけ足す（無効な Entity は足さない） ---
		if (component.emitting && EntityManager::IsActiveInHierarchy(entity)) {
			const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
			if (transform != nullptr) {
				const Vector3 position = GetWorldPosition(*transform);
				TrailState& emitter = (trail != nullptr) ? *trail : FindOrAddTrail(entity);
				if (emitter.points.empty() || Distance(emitter.points.back().position, position) >= component.minVertexDistance) {
					emitter.points.push_back({position, 0.0f});
				}
				trail = &emitter;
			}
		}
		// --- 上限 ---
		if (trail != nullptr) {
			TrimPoints(trail->points, component.maxPoints);
		}
	});
}

//=============================================================================
// 描画
//=============================================================================
void TrailSystem::Draw(Camera* camera) {
	MY_ASSERT_MSG(state != nullptr, "TrailSystem::Initialize()を先に呼んでください");
	std::vector<Vector3>& path = state->path;
	for (const TrailState& trail : state->trails) {
		const TrailComponent* component = EntityManager::Get<TrailComponent>(trail.entity);
		if (component == nullptr || !component->enabled || !EntityManager::IsActiveInHierarchy(trail.entity)) {
			continue;
		}

		// --- 描く点を並べる（Emitting なら、最後に今の位置を足して、帯の先を Entity にくっつける） ---
		path.clear();
		for (const TrailPoint& point : trail.points) {
			path.push_back(point.position);
		}
		if (component->emitting) {
			if (const TransformComponent* transform = EntityManager::Get<TransformComponent>(trail.entity)) {
				const Vector3 head = GetWorldPosition(*transform);
				if (path.empty() || Distance(path.back(), head) > kSamePointDistance) {
					path.push_back(head);
				}
			}
		}
		if (path.size() < static_cast<size_t>(kMinPoints)) {
			continue; // 点1つでは帯にならない
		}

		// --- 帯を作って描く ---
		std::vector<Vertex3dData> vertices;
		std::vector<uint32_t> indices;
		BuildRibbon(path, *component, camera, vertices, indices);

		Renderer::MeshConfig config;
		config.color = PackColor(component->color);
		config.textureHandle = component->textureHandle;
		config.shadingType = ShadingType::Unlit;   // 軌跡は光の当たり方を使わない（色そのまま）
		config.blendMode = (component->blend == TrailBlend::Add) ? BlendMode::Add : BlendMode::Normal;
		config.depthMode = DepthMode::TestNoWrite; // 奥の物には隠れるが、自分は奥行きを書かない（重なった帯・半透明が欠けないように）
		config.camera = camera;
		Renderer::DrawMesh(config, std::move(vertices), std::move(indices));
	}
}

//=============================================================================
// ゲームから使う
//=============================================================================
void TrailSystem::AddPoint(Handle<Entity> entity, const Vector3& position) {
	MY_ASSERT_MSG(state != nullptr, "TrailSystem::Initialize()を先に呼んでください");
	if (!EntityManager::IsAlive(entity)) {
		return;
	}
	FindOrAddTrail(entity).points.push_back({position, 0.0f});
}

void TrailSystem::Clear(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "TrailSystem::Initialize()を先に呼んでください");
	if (TrailState* trail = FindTrail(entity)) {
		trail->points.clear();
	}
}

std::span<const TrailPoint> TrailSystem::GetPoints(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "TrailSystem::Initialize()を先に呼んでください");
	if (const TrailState* trail = FindTrail(entity)) {
		return trail->points;
	}
	return {};
}

float TrailSystem::CalcLength(Handle<Entity> entity) {
	const std::span<const TrailPoint> points = GetPoints(entity);
	float length = 0.0f;
	for (size_t i = 1; i < points.size(); ++i) {
		length += Distance(points[i - 1].position, points[i].position);
	}
	return length;
}
```

> **ForEach の中で `FindOrAddTrail` を呼んでいるが大丈夫か**：増やしているのは TrailSystem 自分の配列（`state->trails`）で、回している `TrailComponent` の配列ではないので大丈夫（Reference.md 4章の「回している型を増減させない」には当たらない）。ただし `push_back` で配列の場所が変わることがあるので、足した後は `trail` を足した物（`emitter`）に付け替えている。

#### 6-4. MyEngine/Engine.cpp（3か所）

```cpp
// include（Systems の並びの最後）
#include "MyEngine/Systems/LightSystem.h"
#include "MyEngine/Systems/TrailSystem.h"
```

```cpp
	// Initialize の CameraSystem::Initialize() の下
	TrailSystem::Initialize();     // TrailをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```

```cpp
	// Finalize の CameraSystem::Release() の下
	TrailSystem::Release();     // シーン（ウィンドウ）の破棄より後にする
```

#### 6-5. MyEngine/Window/WindowManager.cpp（3か所）

```cpp
// include（CollisionSystem の下）
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/TrailSystem.h"
#include "MyEngine/Time/Time.h"
```

```cpp
	// UpdateAll の AudioSystem::Update の下
	AudioSystem::Update(isGameRunning, gameCamera);
	TrailSystem::Update(isGameRunning, Time::GetDeltaTime()); // 軌跡（消したEntityの点を捨てるので FlushDestroy の後。動いた跡はワールド行列の後の位置で足す）
```

```cpp
	// DrawAll の ParticleManager::Draw() から下（前）
	ParticleManager::Draw(); // パーティクルの描画

#ifdef USE_IMGUI
	// ライトのギズモ（エディタ用）。カメラは最初に見つかったシーンのものを使う
	Camera* gizmoCamera = nullptr;
	for (WindowSet& w : windows_) {
		IScene* scene = w.sceneManager ? w.sceneManager->GetCurrentScene() : nullptr;
		if (scene && scene->GetCamera()) {
			gizmoCamera = scene->GetCamera();
			break;
		}
	}
	LightSystem::DrawGizmos(gizmoCamera);
	CollisionSystem::DrawGizmos(gizmoCamera);
#endif
}
```

```cpp
	// 後（カメラを探すのを #ifdef の外へ出す。軌跡は製品版でも描くので）
	ParticleManager::Draw(); // パーティクルの描画

	// 最初に見つかったシーンのカメラ（軌跡の帯をカメラへ向ける・ギズモに使う）
	Camera* gameCamera = nullptr;
	for (WindowSet& w : windows_) {
		IScene* scene = w.sceneManager ? w.sceneManager->GetCurrentScene() : nullptr;
		if (scene && scene->GetCamera()) {
			gameCamera = scene->GetCamera();
			break;
		}
	}
	TrailSystem::Draw(gameCamera); // 軌跡の描画

#ifdef USE_IMGUI
	// ライト・Collider のギズモ（エディタ用）
	LightSystem::DrawGizmos(gameCamera);
	CollisionSystem::DrawGizmos(gameCamera);
#endif
}
```

#### 6-6. MyEngine/ECS/GameComponent.h（1行。SYSTEM から TrailSystem を使えるように）

```cpp
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/TrailSystem.h"
```

#### 6-7. プロジェクトに入っているか

VS の「既存の項目を追加」で、`Capsule.h`（Step 3）・`TrailComponent.h`・`TrailSystem.h`・`TrailSystem.cpp` の4つがエンジンのプロジェクトに入っていること。**`TrailSystem.cpp` が入っていないと、ゲームのビルドでリンクエラー（`TrailSystem::Initialize` が無い）になる。**

---

## 4. ゲーム側の例（AL4_01）

Game.md の中心（描く → 離すと線に沿って進む → 触れた敵を斬る → 地面の線を拾ってインクが戻る）を、今回の物で書いた例。**これも作業用のコピーでビルドして動かした**（5章）。AL4_01 の `Source/Components/` に置き、VS のプロジェクトに追加する。

| ファイル | 中身 |
|---|---|
| `InkWall.h` / `.cpp` | 線を止める壁の印（中身は空。Collider の Box と一緒に付ける） |
| `InkEnemy.h` / `.cpp` | 斬られる敵（HP。Collider と一緒に付ける） |
| `InkPlayer.h` / `.cpp` | プレイヤー（歩く・描く・進む・斬る・拾う） |

> データをヘッダに分けたのは、ほかのファイル（インクのゲージの表示・敵の数を数えるシーンなど）から型が見えるように。Reference.md 5章の「1ファイル＝1Component」の .cpp に、データの .h が1つ付く形。

**Source/Components/InkWall.h**

```cpp
#pragma once

// 線を止める壁の印（中身は空。Collider を一緒に付ける）
// 相手が壁かどうかは「この Component を持っているか」で見分ける（エンジンにレイヤーは無い）
struct InkWall {};
```

**Source/Components/InkWall.cpp**

```cpp
#include "InkWall.h"

#include <MyEngine/ECS/GameComponent.h>

COMPONENT(InkWall, "InkSlash") { ui.Label("線を止める壁（Collider の Box を一緒に付ける）"); }
```

**Source/Components/InkEnemy.h**

```cpp
#pragma once

// 斬られる敵（Collider を一緒に付ける）
struct InkEnemy {
	int hp = 1; // 斬られるたびに1減り、0で消える
};
```

**Source/Components/InkEnemy.cpp**

```cpp
#include "InkEnemy.h"

#include <MyEngine/ECS/GameComponent.h>

COMPONENT(InkEnemy, "InkSlash") { ui.Field("HP", value.hp, AtLeast(1.0f).Tip("斬られるたびに1減り、0で消える")); }
```

**Source/Components/InkPlayer.h**

```cpp
#pragma once
#include <MyEngine/ECS/Entity.h>

// 今していること
enum class InkPlayerState {
	Move, // WASD で歩く（地面のインクに触れると回収）
	Draw, // 左クリックを押している間：時間がゆっくりになり、線を引く
	Dash, // 離した後：線に沿って高速で進み、触れた敵を斬る
};

// プレイヤー（インクで線を引き、その線に沿って斬る）
// 他のファイル（ゲージの表示など）からも読めるように、データはヘッダに置く
struct InkPlayer {
	// --- 調整する値（Inspector に出して保存する） ---
	float moveSpeed = 6.0f;     // WASD で1秒に進む距離
	float dashSpeed = 40.0f;    // 線に沿って1秒に進む距離
	float slowScale = 0.1f;     // 描いている間の時間の速さ（0.1 で10分の1）
	float maxInk = 20.0f;       // インクの上限（描ける線の長さ）
	float pointSpacing = 0.25f; // マウスがこの距離動くたびに点を足す
	float slashRadius = 0.6f;   // 斬る太さ（動いた跡のカプセルの半径）
	float pickupRadius = 0.5f;  // この距離まで近づいた線を回収する
	float hitStopTime = 0.06f;  // 倒したときに止める秒
	// --- 実行中だけの値（ui.Field に書かないので保存されない） ---
	InkPlayerState state = InkPlayerState::Move;
	float ink = 20.0f;      // 残りのインク
	bool isBlocked = false; // 壁かインク切れで、この線はもう伸ばせない
	EntityRef line;         // 今の線（描いている・進んでいる・最後に引いた線）
	int dashIndex = 0;      // 進んでいるとき、次に向かう点の番号
};
```

**Source/Components/InkPlayer.cpp**

```cpp
#include "InkPlayer.h"

#include <algorithm>
#include <span>
#include <vector>

#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Collision/Collision.h>
#include <MyEngine/Components/TrailComponent.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Time/Time.h>

#include "InkEnemy.h"
#include "InkWall.h"

// ① データは InkPlayer.h

// =============================================================================
// ② Inspector での見せ方
// =============================================================================
COMPONENT(InkPlayer, "InkSlash") {
	ui.Field("Move Speed", value.moveSpeed, AtLeast(0.0f));
	ui.Field("Dash Speed", value.dashSpeed, AtLeast(0.0f));
	ui.Field("Slow Scale", value.slowScale, Range(0.01f, 1.0f).Tip("描いている間の時間の速さ"));
	ui.Field("Max Ink", value.maxInk, AtLeast(0.0f).Tip("描ける線の長さの上限"));
	ui.Field("Point Spacing", value.pointSpacing, AtLeast(0.01f));
	ui.Field("Slash Radius", value.slashRadius, AtLeast(0.0f));
	ui.Field("Pickup Radius", value.pickupRadius, AtLeast(0.0f));
	ui.Field("Hit Stop Time", value.hitStopTime, AtLeast(0.0f));
}

// =============================================================================
// ③ 処理
// =============================================================================
namespace {
constexpr float kInkHeight = 0.01f; // 線を地面から少し浮かせる（地面とちらつかないように）
constexpr float kWallMargin = 0.05f; // 壁の手前で止める距離（壁にめり込まないように）

// 相手の見分け（CollisionSystem の絞り込みに渡す）
bool IsWall(Handle<Entity> entity) { return EntityManager::Get<InkWall>(entity) != nullptr; }
bool IsEnemy(Handle<Entity> entity) { return EntityManager::Get<InkEnemy>(entity) != nullptr; }

// マウスが指している地面（y = 0）の位置。ゲーム画面の外・地面を指していないなら false
bool GetMouseGround(Vector3& ground) {
	const Camera* camera = CameraSystem::GetMainCamera();
	if (camera == nullptr || !InputManager::IsMouseInGame()) {
		return false;
	}
	const Ray ray = camera->ScreenPointToRay(InputManager::GetMousePositionInGame());
	float t = 0.0f;
	if (!Collision::RayPlane(ray, {{0.0f, 1.0f, 0.0f}, 0.0f}, &t)) {
		return false;
	}
	ground = Collision::RayPoint(ray, t);
	ground.y = kInkHeight;
	return true;
}

// 新しい線を作る（地面に寝かせた Trail。点は AddPoint で足す）
Handle<Entity> CreateLine(const Vector3& start) {
	TrailComponent trail;
	trail.emitting = false;  // 動いた跡ではなく、描いた線
	trail.lifeTime = 0.0f;   // 消えない（回収されるまで残る）
	trail.maxPoints = 1024;
	trail.width = 0.4f;
	trail.endWidthRate = 1.0f; // 太さは一定
	trail.alignment = TrailAlignment::Ground;
	trail.color = {1.0f, 0.2f, 0.8f, 1.0f};
	trail.blend = TrailBlend::Add;
	const Handle<Entity> line = EntityManager::Create("InkLine");
	EntityManager::RequestAdd<TrailComponent>(line, trail);
	TrailSystem::AddPoint(line, start); // RequestAdd と同じフレームに足してよい
	return line;
}

// 線から radius 以内にいるか（線分ごとに一番近い点を見る）
bool IsTouchingLine(Handle<Entity> line, const Vector3& position, float radius) {
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	for (size_t i = 1; i < points.size(); ++i) {
		const Vector3 closest = Collision::ClosestPointOnSegment(position, points[i - 1].position, points[i].position);
		if (LengthSq(closest - position) <= radius * radius) {
			return true;
		}
	}
	return false;
}

// ===== 歩く・インクを拾う・描き始める =====
void UpdateMove(InkPlayer& player, TransformComponent& transform, float deltaTime) {
	const Vector2 move = InputManager::GetMoveAxis();
	transform.translation += Vector3{move.x, 0.0f, move.y} * (player.moveSpeed * deltaTime);

	// 地面の線に触れたら回収する（動いた跡と、最後に引いた線は拾わない）
	const Vector3 feet = {transform.translation.x, kInkHeight, transform.translation.z};
	const Handle<Entity> lastLine = EntityManager::Find(player.line);
	EntityManager::ForEach<TrailComponent>([&player, &feet, lastLine](Handle<Entity> line, const TrailComponent& trail) {
		if (trail.emitting || line == lastLine || !IsTouchingLine(line, feet, player.pickupRadius)) {
			return;
		}
		player.ink = (std::min)(player.maxInk, player.ink + TrailSystem::CalcLength(line));
		EntityManager::Destroy(line);
	});

	// 左クリックで描き始める：時間をゆっくりにして、足元から線を引く
	if (InputManager::IsMouseButtonTriggered(0) && player.ink > 0.0f) {
		Time::SetGameTimeScale(player.slowScale);
		player.line = EntityManager::RefOf(CreateLine(feet));
		player.isBlocked = false;
		player.state = InkPlayerState::Draw;
	}
}

// ===== 線を伸ばす（壁とインクの残りで止める）=====
void ExtendLine(InkPlayer& player, Handle<Entity> line) {
	Vector3 target;
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	if (player.isBlocked || points.empty() || !GetMouseGround(target)) {
		return;
	}
	const Vector3 last = points.back().position; // AddPoint すると points は使えなくなるので、先に写す
	const Vector3 toTarget = target - last;
	float distance = Length(toTarget);
	if (distance < player.pointSpacing) {
		return; // まだ短い
	}
	const Vector3 direction = toTarget / distance;
	// 1. 壁：最後の点からマウスの方へレイを飛ばし、当たったら壁の手前まで
	RaycastHit hit;
	if (CollisionSystem::Raycast({last, direction}, distance, hit, IsWall)) {
		distance = (std::max)(hit.distance - kWallMargin, 0.0f);
		player.isBlocked = true;
	}
	// 2. インク：残りより長ければ、残りの分まで
	if (distance >= player.ink) {
		distance = player.ink;
		player.isBlocked = true;
	}
	if (distance <= 0.0f) {
		return;
	}
	TrailSystem::AddPoint(line, last + direction * distance);
	player.ink -= distance;
}

// ===== 線に沿って進み、触れた敵を斬る =====
void UpdateDash(InkPlayer& player, TransformComponent& transform, float deltaTime) {
	const Handle<Entity> line = EntityManager::Find(player.line);
	const std::span<const TrailPoint> points = TrailSystem::GetPoints(line);
	const int pointCount = static_cast<int>(points.size());

	// このフレームに進める距離を、点から点へ使っていく（速いと1フレームで点をいくつも越える）
	const Vector3 start = transform.translation;
	Vector3 position = start;
	float remaining = player.dashSpeed * deltaTime;
	while (remaining > 0.0f && player.dashIndex < pointCount) {
		Vector3 target = points[player.dashIndex].position;
		target.y = position.y; // 高さは今のまま（線は地面、キャラは地面の上）
		const Vector3 toTarget = target - position;
		const float distance = Length(toTarget);
		if (distance <= remaining) {
			position = target;
			remaining -= distance;
			++player.dashIndex;
		} else {
			position += toTarget * (remaining / distance);
			remaining = 0.0f;
		}
	}
	transform.translation = position;

	// 斬る：このフレームに動いた跡（start → position）に触れた敵。速くても飛び越えない
	std::vector<Handle<Entity>> touched;
	CollisionSystem::SweepSphere({start, position, player.slashRadius}, touched, IsEnemy);
	for (const Handle<Entity> enemyEntity : touched) {
		InkEnemy* enemy = EntityManager::Get<InkEnemy>(enemyEntity);
		if (enemy == nullptr || enemy->hp <= 0) {
			continue;
		}
		--enemy->hp;
		if (enemy->hp <= 0) {
			EntityManager::Destroy(enemyEntity);
			Time::HitStop(player.hitStopTime);
		}
	}

	// 線の終わりまで来たら歩きに戻る（線はインクとして地面に残る）
	if (player.dashIndex >= pointCount) {
		player.state = InkPlayerState::Move;
	}
}
} // namespace

void InkPlayerUpdate(InkPlayer& player, TransformComponent& transform, float deltaTime) {
	switch (player.state) {
	case InkPlayerState::Move:
		UpdateMove(player, transform, deltaTime);
		break;
	case InkPlayerState::Draw:
		// 描いている間は歩けない（WASD を読まない）
		ExtendLine(player, EntityManager::Find(player.line));
		if (!InputManager::IsMouseButtonPressed(0)) {
			// 離した：時間を戻して、線の始めから進む
			Time::SetGameTimeScale(1.0f);
			player.dashIndex = 0;
			player.state = InkPlayerState::Dash;
		}
		break;
	case InkPlayerState::Dash:
		UpdateDash(player, transform, deltaTime);
		break;
	}
}
SYSTEM(InkPlayerUpdate);
```

**書き方のメモ：**

- **離したかは `!IsMouseButtonPressed(0)` で見る**（`IsMouseButtonReleased` だと、離した瞬間にマウスが Game ビューの外・ImGui の上にあると取りこぼし、ずっと描いたままになる）。
- **敵の HP が2以上だと、1回の進みで何回も当たる**（フレームごとに動いた跡で調べるので、同じ敵に次のフレームも触れる）。1回にしたいなら、「この線で斬った」印（タグ Component か、線ごとの番号）を敵に付ける。
- **最後に引いた線は、次の線を引くまで拾えない**（進み終わった所が線の上なので、そうしないと引いた瞬間に拾えてしまう）。ここはゲームの決め方しだい。
- 敵を倒したときに地面に飛び散るインク（Game.md 2-②）は、この例には入れていない。同じく `emitting = false` の Trail で放射状の短い線を何本か作るか、`Renderer::DrawQuad3d` に丸いテクスチャを貼る。回収はどちらも「線から近いか」で同じ。

**シーンに置く物（エディタで。置いたら Save）：**

| Entity | Component | メモ |
|---|---|---|
| Main Camera | Camera | 真上から少し傾けて見下ろす（例：位置 (0, 15, -8)、回転 X 60度、Field Of View 30）。G2 は透視でも正射影でも動く |
| Player | InkPlayer ＋ Model Renderer | 位置の Y は 0.5 くらい（線は地面 y=0.01、斬る高さはプレイヤーの高さ） |
| 壁 | Collider（Box）＋ **InkWall** ＋ Model Renderer | 箱は回さない（Collider は軸にそろった箱） |
| 敵 | Collider ＋ **InkEnemy** ＋ Model Renderer | |
| 地面 | Model Renderer | Collider は要らない（マウスは y=0 の平面で取る） |

---

## 5. 確認すること

**作業用のコピーで確かめた結果（Release で実行して、ログに出した値）：**

| 確かめたこと | 結果 |
|---|---|
| `Raycast`：(0, 0.5, 0) から +X、x=6 の箱（面は x=5.5） | 当たり・距離 5.500・位置 (5.50, 0.50, 0.00) |
| `Raycast`：-X 向き／`maxDistance` 5（壁まで届かない） | どちらも当たらない |
| `SweepSphere`：0 → 10（半径 0.2）／0 → 5／0 → 5.4 | 1件／0件／1件（5.4 + 0.2 が 5.5 を越える） |
| `ScreenPointToRay`：(0, 10, -10) から 45度見下ろすカメラの画面の真ん中 | 向き (0, -0.707, 0.707)・地面 y=0 との交点 (0, 0, 0) |
| `SetGameTimeScale(0.5f)` の次のフレーム | `GetDeltaTime / GetUnscaleDeltaTime` = 0.500 |
| `HitStop(0.2f)` の次のフレーム／しばらく後 | 止まっている（0）／止まっていない（スローの 0.5 に戻る） |
| Emitting の Trail（回る物） | 点が増え、1秒で消えていく。帯がカメラを向き、先が細い（画面で確認） |
| AddPoint の Trail（RequestAdd と同じフレームに9点） | 9点が残る・長さ 11.31・地面に寝た太さ一定の帯（画面で確認） |
| InkPlayer：線の端に立たせる | 線を回収して消す |
| InkPlayer：Dash にして、途中に敵（InkEnemy＋Collider） | 線の終わり（x=4.00）で歩きに戻る・敵は消える |
| Debug（エディタ）で起動 | 10秒動いて、エラー・警告なし（`grid.obj` を入れた後） |

**写したあとに見ること：**

1. **Step 0**：`git status` に `Resources/Model/grid.obj` が出る。
2. **Step 1**：今まで通り Play / Pause / Stop が動く（Pause で止まる・パーティクルも止まる）。**一時停止のまま Stop して、停止中にパーティクルが動く**（前は止まったままだった）。
3. **Step 2〜5**：エンジンもゲームもビルドが通る（ここだけでは見た目は変わらない）。
4. **Step 6**：Add Component の **Effects → Trail** が出る。Entity に付けて Play し、Transform を Inspector でつまんで動かすと、跡が出て消えていく。Stop で跡は消える。Save → 開き直すと、Trail の設定が残っている。
5. **ゲーム側（G）**：
   - 左クリックを押している間、敵・パーティクルがゆっくりになり、マウスの所まで線が伸びる。WASD で歩けない。
   - 壁の手前で線が止まる。長く描くと、インクが無くなったところで止まる。
   - 離すと線に沿って高速で進み、途中の敵が消えて一瞬止まる（ヒットストップ）。
   - 線の終わりで歩きに戻る。次の線を引いた後、前の線の上を歩くと線が消える（インクが戻る）。
   - **描いている途中で Stop → Play しても、最初から普通の速さ**（スローが残らない）。

---

## 6. 決めたこと

| 決めたこと | 理由 |
|---|---|
| 軌跡は Component（設定）＋ System（点の列）に分ける | Component に `std::vector` は入れられない（Undo・保存がバイト列で写す）。AudioSource と同じ形なので、覚えることが増えない |
| 描いた線は「線1本＝Entity 1つ」 | 消す（回収する）が `Destroy` だけで済む。点も TrailSystem が一緒に捨てる |
| `Time::SetTimeScale` は残さず消す | 同じ名前を残すと「呼んだのに効かない」がまた起きる。消せばコンパイルエラーで気づける |
| ヒットストップは「何も掛けていない時間」で数える | スロー中に倒したとき、スローの分だけ長く止まると変なので |
| スロー・ヒットストップは `ChangeScene` の頭で戻す | Play / Stop / Restart・シーン切り替えの全部が通る1か所なので、戻し忘れが無い |
| Raycast・Sweep は前のフレームに集めた形を使う | 作り直してもワールド行列は前のフレームの値で同じ。ギズモの線とも同じ形になる |
| 相手の絞り込みは `std::function`（Component で見分ける） | レイヤーが無いので。ラムダで「この Component を持つか」を書ける |
| カプセルと箱は「広げた箱」で近似 | 正しく角を丸めると計算が長くなる。大きめに当たるのは箱の辺・角の近くだけ（辺で半径の4割、角で7割くらい遠くまで）で、斬撃では気にならない |
| `DrawMesh` は両面描きが既定 | 帯は並べる向きで裏表が変わる。片面だと、線を逆向きに描いたときに消える |
| 帯は Unlit・深度を書かない | 軌跡は演出なので光の当たり方は要らない。深度を書くと、重なった帯・半透明のところが欠ける |
| `TrailBlend` を作った（`BlendMode` を使わない） | `BlendMode` のヘッダは `d3d12.h` を読む。ゲームの全部のファイルに入るヘッダを重くしない |
| ActionGame.md は消した | 課題のゲームを InkSlash にしたので。エンジンに足す物（E1〜E5）の案は、要るものだけこのファイルで作った |

---

## 7. 未解決

| 無い物 | 今どうするか | いつ |
|---|---|---|
| 帯の頂点ごとの色・古い方ほど薄くする | `Vertex3dData` に色が無い。テクスチャ（U が古い方 0 → 新しい方 1）でグラデーションを作る | 要るときに（頂点の形とシェーダを足す） |
| 帯の半透明の並べ替え | 帯は位置を原点で数えるので、ほかの半透明との前後がずれることがある。`Add` なら順番は関係ない | 気になったら（`MeshConfig` に並べ替え用の位置を足す） |
| 正射影のカメラ | 透視の視野角を小さく（20〜30度）して、正射影っぽく見せる。G2 は正射影でもそのまま動く | 要るときに（Camera Component に `projection` と `orthoSize`） |
| 回した壁（OBB）・レイヤー | 壁は軸にそろった箱で組む。相手は Component で見分ける | [Collision.md](Collision.md) の「7. 未解決」 |
| 壁の押し戻し（WASD で壁に入らない） | ゲームで書く：プレイヤー（球）と壁（箱）を `Collision::ClosestPointOnAABB` で調べて外へ押し出す（30行くらい） | 要るときに（エンジンに入れるなら Body として） |
| インクのゲージの表示・文字 | `Renderer::DrawSprite` で棒を描く（`InkPlayer.ink / maxInk` で横の長さ）。文字は画像で | UI Image を作るときに |
| Trail の点の数が多いときの速さ | 線を作るたびに頂点を作り直す。線が数十本・点が数千くらいまでは問題ない | 遅くなったら |
