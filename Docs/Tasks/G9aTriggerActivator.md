# G9a：チュートリアルの説明（Trigger Activator・Alpha Group）

参考：
- Unity の Canvas Group（子の UI をまとめて透明にする）：https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/class-CanvasGroup.html
- Unity の OnTriggerEnter（Is Trigger の Collider に入った知らせ）：https://docs.unity3d.com/ScriptReference/Collider.OnTriggerEnter.html
- Unity の GameObject.SetActive（有効・無効）：https://docs.unity3d.com/ScriptReference/GameObject.SetActive.html
- cpprefjp `std::ranges::any_of`：https://cpprefjp.github.io/reference/algorithm/ranges_any_of.html
- cpprefjp 初期化式をともなう if 文（`if (初期化; 条件)`）：https://cpprefjp.github.io/lang/cpp17/selection_statement_with_initializer.html

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の 13章の **G9a**（G9 のレベル・UI の前に、チュートリアルの説明を出す仕組み）。
- やりたいこと：**ある場所に近づいたら、説明（Text・Image）を出す**。離れたら消す・1回だけ出す・フェードの秒数を選べる。
- 3D 空間の Text と比べて、こちらにした（ユーザーの判断）：
  - 3D の Text は、文字を 3D の板に描く新しい描き方（深度・霧・DOF との重なり）が要る。今の Text は画面の UI（Anchor からのピクセル）なので、置いた場所に関係なく画面の同じ所に出る。
  - 「範囲に入ったら出す」は、新しい描き方が要らない（Collider の当たり＋有効・無効＋透明度だけ）。Script を書かずにエディタだけで置ける。
- 作る物は2つ（どちらもエンジン。ほかのゲームでも使える）：
  - **Alpha Group**（`AlphaGroupComponent`）：自分と子の Image・Text・Sprite Renderer を、まとめて透明にする（Unity の Canvas Group の、透明度だけの版）。
  - **Trigger Activator**（`TriggerActivatorComponent`・`TriggerActivatorSystem`）：同じ Entity の Collider の範囲に入ったら・出たら、Target を出す・消す（フェードは Target の Alpha Group を動かす）。
- **パートの順番**：A（Alpha Group）→ B（Trigger Activator）→ C（エディタで置く）。A だけでビルドが通る。B は A の `AlphaGroupComponent.h` を読むので、A の後に写す。ゲーム（AL4_01）のコードは変えない。
- 確かめた環境：本体の master（966b734。G11 まで）＋ このファイルのエンジン、AL4_01 のコピー（G11 までの写経のまま）。Debug・Release でビルドし、Release でテスト用のシーンを動かしてログと画面で確かめた（5章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Components/AlphaGroupComponent.h`（新しく作る）・`Math/Color.h`・`Systems/SpriteRenderSystem.h/.cpp`・`Systems/TextRenderSystem.cpp`・`MyEngine_v1.vcxproj` | まとめて透明にする。Image・Text・Sprite Renderer が親までの Alpha を掛けて描く |
| B | `Components/TriggerActivatorComponent.h`・`Systems/TriggerActivatorSystem.h/.cpp`（新しく作る）・`Systems/CameraSystem.h/.cpp`・`Engine.cpp`・`WindowManager.cpp`・`HierarchyWindow.cpp`・`MyEngine_v1.vcxproj` | 入ったら・出たら Target を出す・消す・フェード。＋ → 3D Object → Trigger Activator |
| C | エディタで | チュートリアルの説明を置く |

---

## 0. 前の写経の答え合わせ（G11）
G11（c0c9d73）は前回の答え合わせで写し違いが無かった（空白・コメントだけ）ので、今回は無し。

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 どう動くか
```
   ┌───────── Collider（Is Trigger の箱）─────────┐       画面（Game ビュー）
   │                                              │      ┌──────────────────────────┐
   │   ●→  プレイヤーが入る                         │ ───→ │   ← → で うごく          │ ← Target（Text と Image の親）
   │      （Source。初めは FollowTarget）           │      │   （0.3 秒でだんだん出る）│    を有効にして、Alpha Group を 0→1
   │                                              │      └──────────────────────────┘
   └──────────────────────────────────────────────┘
   出たら：Alpha Group を 1→0（0.3 秒）→ 消えきったら Target を無効にする
```

- 範囲は **同じ Entity の Collider**（Is Trigger の箱）。ギズモは Collider の線がそのまま出るので、Scene ビューで範囲が見える。
- 「出す」＝ Target を**有効**にする（Inspector の名前の左のチェック。`Entity::isActive`）。「消す」＝**無効**にする。子も一緒に出る・消える（`IsActiveInHierarchy`）。
- 文字と背景の板を1つの親（例：`Tutorial_Move`）にまとめて、その親を Target に入れる。

### 1.2 なぜ Component（エンジン）にしたか
| やり方 | | |
|---|---|---|
| Script（`OnTriggerEnter` で `SetActive`）を説明ごとに書く | 説明を足すたびに Script が増える・フェードを毎回書く | × |
| Script で文字を 3D の位置に追わせる（`WorldToScreenPoint`） | エンジンが育たない・カメラが動くと文字が揺れる | × |
| 画像の看板（Sprite Renderer に文字を描いた画像） | 文字を直すたびに画像を作り直す | × |
| **Trigger Activator（エンジンの Component）** | Script 無し・エディタだけで置ける・どのゲームでも使える | ○ |

Unity でも、この形（`OnTriggerEnter` で別の物を `SetActive`）は Script を書くのが普通だが、このゲームではチュートリアルの数だけ同じ Script を書くことになるので、決まった動きは Component にした（Wind Zone と同じ考え）。

### 1.3 入った・出たの決め方：毎フレーム「今、中にいるか」を数え直す
`CollisionSystem::GetHits(entity)` には、その Entity の当たりが Enter・Stay・Exit の3つで入っている。Enter が来たら出す・Exit が来たら消す、と書くと、次の2つで困る：

| 困ること | なぜ |
|---|---|
| 中にいた相手が**消えた**（やられて Destroy された） | Exit は来るが、`hit.other` がもう居ないので「プレイヤーか」を調べられない（`EntityManager::Get` が nullptr） |
| プレイヤーに Collider が**2つ**ある（自分と子） | 1つ目が出たときに Exit が来て消えるが、もう1つはまだ中にいる |

なので、**Enter・Stay の当たりの中に、反応する相手が1つでもいるか**を毎フレーム調べて（`isInside`）、前のフレームの値と比べる：

| 前のフレーム | 今 | すること |
|---|---|---|
| 外 | 中 | On Enter（入った） |
| 中 | 外 | On Exit（出た） |
| 同じ | 同じ | 何もしない |

相手が消えると、次のフレームには当たりが無くなるので「外」になる（Exit を待たない）。

### 1.4 フェードは Alpha Group（色を直接いじらない）
Target の Text・Image の `color.a` を直接動かす形にすると：
- 子が何個もある（文字・背景・アイコン）→ 全部の子をたどって書き換える。
- 元の a（背景の板は 0.6 など）を覚えておいて、そこまで戻す必要がある。
- Text は色が5つ（文字・縁取り・影・光・斜めの光）。

そこで、**Alpha Group**（透明度の倍率を持つだけの Component）を作り、**描く側**（Image・Text・Sprite Renderer）が「自分と親の Alpha Group を全部掛けた値」を色の a に掛けるようにした（Unity の Canvas Group と同じ考え）。Trigger Activator は Target の Alpha Group の Alpha 1つを動かすだけでよい。

```
Tutorial_Move（Alpha Group：Alpha 0.5）      ← Trigger Activator が動かす
 ├ Text（color.a 1.0）         → 描く a ＝ 1.0 × 0.5 ＝ 0.5
 └ Back（Image。color.a 0.6）  → 描く a ＝ 0.6 × 0.5 ＝ 0.3（元の濃さの比はそのまま）
```

- Target に Alpha Group が無ければ、**再生を始めたときに付ける**（`RequestAdd`。付くのは次のフレームの頭。Stop でシーンを Play を押した瞬間に戻すので、外れる）。自分で付けておいてもよい（Alpha を 0.8 にしておくと、0.8 まで出す＝`baseAlpha`）。
- 付く前のフレームに出すと、Alpha Group がまだ無いので1フレームだけ濃く出る。なので、付くまでの1フレームは入った・出たを見ない。
- Alpha Group が効くのは Image・Text・Sprite Renderer だけ（モデル・Primitive Renderer には効かない。今は要らないので）。Sprite Renderer の **Alpha Clip** は、半分より透明な所を捨てるので、フェードの途中で急に消える。

### 1.5 再生中だけ動く・Hide On Start
- 動かすのは再生中だけ（Wind Zone・Camera Follow と同じ）。停止中は何もしないので、エディタでは Target が見えたまま、位置・文字を直せる。
- **Hide On Start**（初期値 オン）：再生を始めたら、すぐ Target を消す。エディタで Target を無効にしておかなくてよい（無効にすると、エディタで見えなくなって直しにくい）。
- Play 中に有効・無効・Alpha を変えても、**Stop で Play を押した瞬間の状態に戻る**（`SceneManager` がシーンを写しから作り直すので）。実行中だけの値（`isInside`・`fadeAlpha` など）は保存しない（`Describe` に書かない＝Camera Follow の `isFollowing` と同じ）。

### 1.6 組み合わせ（よく使う形）
| したいこと | On Enter | On Exit | Once | Hide On Start |
|---|---|---|---|---|
| 近づいたら出す・離れたら消す（初期値） | Show | Hide | オフ | オン |
| 一度読んだら、もう出さない | Show | Hide | **オン** | オン |
| 入ったら出したままにする（看板を見つけた） | Show | None | オフ | オン |
| 最初から出しておいて、そこを離れたら消す（スタート地点の「← → で うごく」） | None | Hide | オン | **オフ** |
| ある所に着いたら、前の説明を消す | Hide | None | オフ | オフ |

- **Once**：入ったとき・出たときの動きを、**それぞれ1回だけ**にする（`isEnterUsed`・`isExitUsed`）。Show・Hide なら「1回入って出たら、もう出ない」。
- **Source**（誰が入ったら反応するか）：
  - `FollowTarget`（初期値）：メインカメラの Camera Follow の Target＝プレイヤー。何も選ばなくてよい。
  - `Entity`：Source Entity に入れた物（子に付けた Collider も、その Entity として数える）。
  - `AnyRigidbody`：Rigidbody を付けた物なら、どれでも。

### 1.7 1フレームの中の順番
`TriggerActivatorSystem::Update` は **`CollisionSystem::Update` の直後**（Reference.md の 1フレームの順番の 9.5）。そのフレームの当たりで決めるので、入ったフレームのうちに出し始める。出した Target は、このフレームの描画（11）から見える。

---

## 2. パートA：Alpha Group（エンジン）

### `MyEngine/Components/AlphaGroupComponent.h`（新しく作る）
```cpp
#pragma once
#include <type_traits>

// 自分と子の Image・Text・Sprite Renderer を、まとめて透明にする。Entityに付けるComponent（カテゴリ：UI）
// 描くときに、自分と親を全部たどって、見つかった Alpha Group の Alpha を全部掛ける（親の親に付けた物も効く）
// 説明の文字と背景の板を1つの親にまとめて、親の Alpha を動かせば、全部いっしょにフェードする（Unity の Canvas Group と同じ考え）
// Trigger Activator のフェードは、Target の Alpha Group の Alpha を動かす（Target に無ければ、再生を始めたときに付ける）


/// <summary>
/// まとめて透明にする（Unity の Canvas Group の、透明度だけの版）
/// </summary>
struct AlphaGroupComponent {
	bool enabled = true; // false の間は掛けない（Alpha が 1 と同じ）
	float alpha = 1.0f;  // 透明度の倍率（0〜1。0 なら見えない）。自分と子の Image・Text・Sprite Renderer の色の a に掛かる
};

static_assert(std::is_trivially_copyable_v<AlphaGroupComponent>);
```

### `MyEngine/Math/Color.h`（1つ足す）
`FromColorCode` の下に：
```cpp
// A（透け方）にだけ倍率を掛ける（Alpha Group でまとめて透明にする。RGB はそのまま）
inline Vector4 MultiplyAlpha(const Vector4& color, float alpha) { return {color.x, color.y, color.z, color.w * alpha}; }
```

### `MyEngine/Systems/SpriteRenderSystem.h`（1つ足す）
`CalcScreenPoint` の下（「エディタから使う」の前）に：
```cpp
	/// <summary>
	/// 自分と親を全部たどって、有効な Alpha Group の Alpha を掛けた値（0〜1。どこにも無ければ 1）
	/// <para>Image・Text・Sprite Renderer が、描く色の a に掛ける（親の Alpha Group を動かすと、子がまとめてフェードする）</para>
	/// </summary>
	static float CalcGroupAlpha(Handle<Entity> entity);
```

### `MyEngine/Systems/SpriteRenderSystem.cpp`（6か所）
include に（Components の並びの頭）：
```cpp
#include "MyEngine/Components/AlphaGroupComponent.h"
```
`DrawSprites` の中、`const FrameUV uv = CalcSourceUV(sprite);` の前に：
```cpp
		// 親までの Alpha Group を掛ける（0 なら描かない）
		const float groupAlpha = SpriteRenderSystem::CalcGroupAlpha(entity);
		if (groupAlpha <= 0.0f) {
			return;
		}
```
同じ所の色を：
```cpp
		config.color = ToColorCode(MultiplyAlpha(sprite.color, groupAlpha));
```
`DrawImages` の for の中、`const ImageQuad quad = CalcImageQuad(*image, *transform);` の前に：
```cpp
		// 親までの Alpha Group を掛ける（0 なら描かない）
		const float groupAlpha = SpriteRenderSystem::CalcGroupAlpha(item.entity);
		if (groupAlpha <= 0.0f) {
			continue;
		}
```
同じ所の色を：
```cpp
		config.color = ToColorCode(MultiplyAlpha(image->color, groupAlpha));
```
`DescribeImage` の下（`} // namespace` の前）に：
```cpp
// Alpha Group の見せ方
void DescribeAlphaGroup(ComponentUI& ui, AlphaGroupComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと掛けない"));
	ui.Field("Alpha###alpha", value.alpha, Range(0.0f, 1.0f).Tip("自分と子の Image・Text・Sprite Renderer の透明度の倍率（0 なら見えない）。親の Alpha Group とも掛け合わせる"));
	ui.Label("Trigger Activator のフェードは、ここを動かす（再生中は、ここで直しても上書きされる）");
}
```
`Initialize` の最後（Image の登録の下）に。上のコメントの「2つの Component」は「3つの Component」に：
```cpp
	// Alpha Group は描く物ではないが、Image・Text・Sprite Renderer の3つが読むので、ここで登録する
	EntityManager::RegisterComponent<AlphaGroupComponent>();
	ComponentSerializer::Register<AlphaGroupComponent>("Alpha Group", &DescribeAlphaGroup);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<AlphaGroupComponent>>("Alpha Group", "UI", &DescribeAlphaGroup));
```
「ゲームから使う」の `CalcScreenPoint` の前に：
```cpp
float SpriteRenderSystem::CalcGroupAlpha(Handle<Entity> entity) {
	// 自分 → 親 → 親の親 … と一番上までたどって、有効な Alpha Group の Alpha を掛けていく（親子は輪にならない。SetParent が断る）
	float alpha = 1.0f;
	for (Handle<Entity> current = entity; current.IsValid();) {
		if (const AlphaGroupComponent* group = EntityManager::Get<AlphaGroupComponent>(current); group != nullptr && group->enabled) {
			alpha *= std::clamp(group->alpha, 0.0f, 1.0f);
		}
		const Entity* owner = EntityManager::Get(current);
		current = (owner != nullptr) ? owner->parent : Handle<Entity>{};
	}
	return alpha;
}
```

> **`if (初期化; 条件)`**（C++17）：`if` の中で変数を作って、そのまま条件に使える。作った `group` は if の中（else も）だけで使え、外へは出ない。`const auto* group = Get(...); if (group != nullptr && group->enabled)` の2行を1つにした形。

### `MyEngine/Systems/TextRenderSystem.cpp`（3か所）
include に（`FontManager.h` の下）：
```cpp
#include "MyEngine/Math/Color.h"
```
`Draw` の中、`if (transform == nullptr || font == 0) { return; }` の下に：
```cpp
		// 親までの Alpha Group を掛ける（文字・縁取り・影・光の色の a に。0 なら描かない）
		const float groupAlpha = SpriteRenderSystem::CalcGroupAlpha(entity);
		if (groupAlpha <= 0.0f) {
			return;
		}
```
影と文字の色を（5か所。`MultiplyAlpha` で a に掛ける）：
```cpp
		// --- 影：同じ文字を、ずらして影の色で先に描く（縁取りの所も影にする。同じ Order なので、積んだ順＝影が奥）---
		if (text.shadow) {
			config.color = MultiplyAlpha(text.shadowColor, groupAlpha);
			config.outlineColor = config.color;
			Renderer::DrawText2d(config, MakeVertices(layout, text, transform->worldMatrix, text.shadowOffset, time));
		}
		// --- 文字（光・斜めの光は、影には付けない）---
		config.color = MultiplyAlpha(text.color, groupAlpha);
		config.outlineColor = MultiplyAlpha(text.outlineColor, groupAlpha);
		// 光は縁取りの外へ広げるので、縁取りと合わせて上限まで
		config.glowWidth = (std::min)(ToSdfWidth(text.glowWidth, text.fontSize), kMaxOutlineWidth - config.outlineWidth);
		config.glowColor = MultiplyAlpha(text.glowColor, groupAlpha);
		if (text.shine) {
			const float halfWidth = text.shineWidth / (std::max)(layout.size.x, 1.0f); // ピクセル → かたまりの幅に対する割合
			config.shineWidth = halfWidth;
			config.shinePosition = CalcShinePosition(text, halfWidth, time);
			config.shineColor = MultiplyAlpha(text.shineColor, groupAlpha);
		}
		Renderer::DrawText2d(config, MakeVertices(layout, text, transform->worldMatrix, {0.0f, 0.0f}, time));
```

### `MyEngine_v1.vcxproj`
`AlphaGroupComponent.h` をプロジェクトに足す（VS のソリューションエクスプローラーで追加すれば、filters も一緒に書かれる）。

---

## 3. パートB：Trigger Activator（エンジン）

### `MyEngine/Components/TriggerActivatorComponent.h`（新しく作る）
```cpp
#pragma once
#include <type_traits>

#include "MyEngine/ECS/Entity.h"

// 同じ Entity の Collider の範囲に入ったら・出たら、Target（別の Entity）を出す・消す。Entityに付けるComponent（カテゴリ：Gameplay）
// チュートリアルの説明（Text・Image）・看板を、近づいた所でだけ見せるのに使う。Script を書かずに、エディタだけで置ける
// 範囲は同じ Entity の Collider（Is Trigger にする。ギズモは Collider の線）。誰が入ったら反応するかは Source で選ぶ
// 出す＝Target を有効にする（Inspector の名前の左のチェック）。消す＝無効にする。フェードは Target の Alpha Group の Alpha を動かす
// 動かすのは TriggerActivatorSystem（再生中だけ）。停止中は何もしない（Stop で、Play を押した瞬間の状態に戻る）

/// <summary>
/// 誰が入ったら反応するか。Inspector では、Entity を選んだときだけ Source Entity が出る
/// </summary>
enum class TriggerSource {
	FollowTarget, // メインカメラの Camera Follow の Target（横から見るゲームのプレイヤー。何も選ばなくてよい）
	Entity,       // Source Entity に入れた Entity（子に付けた Collider も、その Entity として数える）
	AnyRigidbody, // Rigidbody を付けた物なら、どれでも
};

/// <summary>
/// 入ったとき・出たときに、Target をどうするか
/// </summary>
enum class TriggerAction {
	None, // 何もしない
	Show, // 出す（有効にして、Fade In Time でだんだん見せる）
	Hide, // 消す（Fade Out Time でだんだん消して、消えきったら無効にする）
};

/// <summary>
/// 今のフェード（実行中だけの値）
/// </summary>
enum class TriggerFade {
	None,      // していない（出しきった・消しきった）
	FadingIn,  // だんだん見せている
	FadingOut, // だんだん消している（消えきったら Target を無効にする）
};


/// <summary>
/// 範囲に入ったら・出たら、別の Entity を出す・消す（Unity なら、OnTriggerEnter で SetActive する Script を書く所）
/// </summary>
struct TriggerActivatorComponent {
	bool enabled = true;                                // false の間は反応しない（フェードの途中なら、最後まで続ける）
	EntityRef target;                                   // 出す・消す Entity（Hierarchy からドラッグ。文字と背景をまとめた親を入れると、子も一緒）
	TriggerSource source = TriggerSource::FollowTarget; // 誰が入ったら反応するか
	EntityRef sourceEntity;                             // source が Entity のとき：反応する相手
	TriggerAction onEnter = TriggerAction::Show;        // 入ったとき
	TriggerAction onExit = TriggerAction::Hide;         // 出たとき
	bool once = false;                                  // true：入ったとき・出たときの動きを、それぞれ1回だけにする（2回目に入っても何もしない）
	bool hideOnStart = true;                            // true：再生を始めたら、すぐ Target を消す（エディタでは見えたまま置ける）
	float fadeInTime = 0.3f;                            // 出すときに、見えきるまでの秒数（0 ならすぐ）
	float fadeOutTime = 0.3f;                           // 消すときに、消えきるまでの秒数（0 ならすぐ）

	// --- 実行中だけの値（TriggerActivatorSystem が書く。保存しない）---
	bool isStarted = false;               // 再生を始めたときの準備（Hide On Start・Alpha Group を付ける）をしたか
	bool isInside = false;                // 前のフレームに、Source が範囲の中にいたか（今と比べて、入った・出たを決める）
	bool isEnterUsed = false;             // 入ったときの動きをしたか（Once のとき、2回目をしないため）
	bool isExitUsed = false;              // 出たときの動きをしたか（同じ）
	TriggerFade fade = TriggerFade::None; // 今のフェード
	float fadeAlpha = 1.0f;               // フェードの透明度（0〜1。Target の Alpha Group の Alpha ＝ baseAlpha × これ）
	float baseAlpha = 1.0f;               // 再生を始めたときの、Target の Alpha Group の Alpha（エディタで 0.8 にしておいた物は、0.8 まで出す）
};

static_assert(std::is_trivially_copyable_v<TriggerActivatorComponent>);
```

### `MyEngine/Systems/TriggerActivatorSystem.h`（新しく作る）
```cpp
#pragma once
#include "MyEngine/Components/TriggerActivatorComponent.h"


/// <summary>
/// Trigger Activator を登録し、範囲に入った・出たを見て、Target を出す・消す（フェードも）
/// <para>入ったかどうかは、同じ Entity の Collider の当たり（CollisionSystem::GetHits）で決める。
/// Enter・Exit の知らせではなく「今、中にいるか」を毎フレーム調べて、前のフレームと比べる（相手が消えても、出たことになる）</para>
/// <para>フェードは Target の Alpha Group の Alpha を動かす。描くときに Image・Text・Sprite Renderer が、親まで掛けて使う（SpriteRenderSystem::CalcGroupAlpha）</para>
/// </summary>
class TriggerActivatorSystem {
public:
	// TriggerActivatorComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 入った・出たを見て Target を出す・消し、フェードを進める（再生中だけ。停止中は何もしない）
	/// <para>CollisionSystem::Update の後に呼ぶ（そのフレームの当たりで決めるので、入ったフレームに出始める）</para>
	/// </summary>
	/// <param name="isGameRunning">再生中か（一時停止中も true。そのときは deltaTime が 0 なので、フェードは止まる）</param>
	/// <param name="deltaTime">経過秒（Time::GetDeltaTime）</param>
	static void Update(bool isGameRunning, float deltaTime);
};
```

### `MyEngine/Systems/TriggerActivatorSystem.cpp`（新しく作る）
流れ（`Update` の中、Trigger Activator 1つごと）：
1. Target を探す。無い・自分か親なら何もしない（自分を消すと、範囲の Collider も消えて、もう入れない）。
2. 最初のフレームだけ `Start`：Alpha Group を覚える・無ければ付ける、Hide On Start で消す。
3. Alpha Group を付けている途中（次のフレームに付く）なら、ここで終わり。
4. 今、中にいるか（`IsSourceInside`）を前のフレームと比べて、On Enter・On Exit（`React`）。
5. フェードを進めて（`StepFade`）、Target の Alpha Group へ書く。

```cpp
#include "TriggerActivatorSystem.h"

#include <algorithm>
#include <memory>

#include "MyEngine/Components/AlphaGroupComponent.h"
#include "MyEngine/Components/RigidbodyComponent.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Math/MathUtility.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"

namespace {
//=============================================================================
// 誰が入ったか
//=============================================================================
// other が who 自身か、who の子孫か（プレイヤーの子に付けた Collider も、プレイヤーとして数える）
bool IsSelfOrDescendant(Handle<Entity> other, Handle<Entity> who) { return who.IsValid() && (other == who || EntityManager::IsDescendantOf(other, who)); }

// other が、反応する相手か
bool IsSource(const TriggerActivatorComponent& activator, Handle<Entity> other) {
	switch (activator.source) {
	case TriggerSource::FollowTarget:
		return IsSelfOrDescendant(other, CameraSystem::GetFollowTargetEntity());
	case TriggerSource::Entity:
		return IsSelfOrDescendant(other, EntityManager::Find(activator.sourceEntity));
	case TriggerSource::AnyRigidbody:
		return EntityManager::Get<RigidbodyComponent>(other) != nullptr;
	}
	return false;
}

// 今、反応する相手が範囲の中にいるか（自分の当たりのうち、Enter・Stay の物に、相手が1つでもあれば中）
// Exit の知らせを待たずに毎フレーム数え直すので、中にいた相手が消えても（Exit が来なくても）、次のフレームには「出た」になる
bool IsSourceInside(Handle<Entity> entity, const TriggerActivatorComponent& activator) {
	return std::ranges::any_of(CollisionSystem::GetHits(entity), [&activator](const CollisionHit& hit) { return hit.phase != HitPhase::Exit && IsSource(activator, hit.other); });
}

//=============================================================================
// 出す・消す
//=============================================================================
// フェードを使うか（使うときだけ、Target の Alpha Group を付ける・書く）
bool UsesFade(const TriggerActivatorComponent& activator) { return activator.fadeInTime > 0.0f || activator.fadeOutTime > 0.0f; }

// 再生を始めた最初のフレームに1回：Hide On Start と、フェードに使う Alpha Group の用意
void Start(TriggerActivatorComponent& activator, Handle<Entity> targetHandle, Entity& target) {
	activator.isStarted = true;
	// --- Alpha Group：あれば今の Alpha を覚える（そこまで出す）。フェードするのに無ければ付ける（付くのは次のフレームの頭。Stop で外れる）---
	if (const AlphaGroupComponent* group = EntityManager::Get<AlphaGroupComponent>(targetHandle)) {
		activator.baseAlpha = group->alpha;
	} else if (UsesFade(activator) && !EntityManager::IsAddPending<AlphaGroupComponent>(targetHandle)) {
		EntityManager::RequestAdd<AlphaGroupComponent>(targetHandle); // 同じ Target を指す Trigger Activator が2つあっても、1つだけ付ける
	}
	if (activator.hideOnStart) {
		target.isActive = false;
	}
	activator.fadeAlpha = target.isActive ? 1.0f : 0.0f;
}

// 出す：有効にして、Fade In Time でだんだん見せる（消している途中なら、今の濃さから戻す）
void Show(TriggerActivatorComponent& activator, Entity& target) {
	if (!target.isActive) {
		target.isActive = true;
		activator.fadeAlpha = 0.0f; // 消えていた所から
	}
	if (activator.fadeInTime > 0.0f) {
		activator.fade = TriggerFade::FadingIn;
	} else {
		activator.fadeAlpha = 1.0f;
		activator.fade = TriggerFade::None;
	}
}

// 消す：Fade Out Time でだんだん消して、消えきったら無効にする（StepFade）。0 秒なら、すぐ無効にする
void Hide(TriggerActivatorComponent& activator, Entity& target) {
	if (activator.fadeOutTime > 0.0f && target.isActive) {
		activator.fade = TriggerFade::FadingOut;
		return;
	}
	target.isActive = false;
	activator.fadeAlpha = 0.0f;
	activator.fade = TriggerFade::None;
}

// 入ったとき・出たときの動き。Once なら、それぞれ1回だけ（isUsed に、したことを覚える）
void React(TriggerAction action, bool& isUsed, TriggerActivatorComponent& activator, Entity& target) {
	if (activator.once && isUsed) {
		return;
	}
	isUsed = true;
	switch (action) {
	case TriggerAction::None:
		break;
	case TriggerAction::Show:
		Show(activator, target);
		break;
	case TriggerAction::Hide:
		Hide(activator, target);
		break;
	}
}

// 1フレームに変える濃さ（濃さ 1 を time 秒で変えきる速さ。time が 0 なら、すぐ変えきる）
// フェードの途中で Inspector の秒数を 0 にされても、0 で割らないように
float CalcFadeStep(float time, float deltaTime) { return (time > 0.0f) ? deltaTime / time : 1.0f; }

// フェードを1フレーム進める（消えきったら Target を無効にする）
void StepFade(TriggerActivatorComponent& activator, Entity& target, float deltaTime) {
	switch (activator.fade) {
	case TriggerFade::None:
		break;
	case TriggerFade::FadingIn:
		activator.fadeAlpha = MathUtility::MoveTowards(activator.fadeAlpha, 1.0f, CalcFadeStep(activator.fadeInTime, deltaTime));
		if (activator.fadeAlpha >= 1.0f) {
			activator.fade = TriggerFade::None;
		}
		break;
	case TriggerFade::FadingOut:
		activator.fadeAlpha = MathUtility::MoveTowards(activator.fadeAlpha, 0.0f, CalcFadeStep(activator.fadeOutTime, deltaTime));
		if (activator.fadeAlpha <= 0.0f) {
			target.isActive = false;
			activator.fade = TriggerFade::None;
		}
		break;
	}
}

//=============================================================================
// Inspector と 保存
//=============================================================================
void DescribeTriggerActivator(ComponentUI& ui, TriggerActivatorComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと反応しない（フェードの途中なら、最後まで続ける）"));
	ui.Label("範囲は同じ Entity の Collider（Is Trigger にする）。再生中だけ動く");
	ui.Field("Target###target", value.target, Tip("出す・消す Entity（Hierarchy からドラッグ）。文字と背景をまとめた親を入れると、子も一緒に出る・消える。自分と自分の親は選ばない"));
	// ===== 誰が入ったら =====
	ui.Separator("Source");
	ui.Field("Source###source", value.source, Tip("誰が入ったら反応するか。FollowTarget：カメラが追っている物（プレイヤー） / Entity：Source Entity に入れた物 / AnyRigidbody：Rigidbody を付けた物なら、どれでも"));
	// --- 選んだ相手の値だけを見せる（保存も、使っている方だけが書かれる。Camera Follow の Mode と同じ）---
	switch (value.source) {
	case TriggerSource::FollowTarget:
		break; // 選ぶ物は無い（メインカメラの Camera Follow の Target を使う）
	case TriggerSource::Entity:
		ui.Field("Source Entity###sourceEntity", value.sourceEntity, Tip("反応する相手（Hierarchy からドラッグ）。子に付けた Collider も、この Entity として数える"));
		break;
	case TriggerSource::AnyRigidbody:
		break; // 選ぶ物は無い
	}
	// ===== どうするか =====
	ui.Separator("Action");
	ui.Field("On Enter###onEnter", value.onEnter, Tip("入ったとき。Show：出す / Hide：消す / None：何もしない"));
	ui.Field("On Exit###onExit", value.onExit, Tip("出たとき。Show：出す / Hide：消す / None：何もしない（出したままにする）"));
	ui.Field("Once###once", value.once, Tip("入ったとき・出たときの動きを、それぞれ1回だけにする（一度読んだ説明は、もう出さない）"));
	ui.Field("Hide On Start###hideOnStart", value.hideOnStart, Tip("再生を始めたら、すぐ Target を消す（エディタでは見えたまま置ける）。最初から出しておいて、出たら消すなら外す"));
	// ===== フェード =====
	ui.Separator("Fade");
	ui.Field("Fade In Time###fadeInTime", value.fadeInTime, AtLeast(0.0f).Tip("出すときに、見えきるまでの秒数（0 ならすぐ）"));
	ui.Field("Fade Out Time###fadeOutTime", value.fadeOutTime, AtLeast(0.0f).Tip("消すときに、消えきるまでの秒数（0 ならすぐ）"));
	ui.Label("フェードは Target の Alpha Group の Alpha を動かす（無ければ再生中だけ付く）。効くのは Image・Text・Sprite Renderer");
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void TriggerActivatorSystem::Initialize() {
	// Trigger Activator の置き場所・保存・Inspector を登録する（見せ方は DescribeTriggerActivator 1つで、保存と Inspector の両方に使う）
	EntityManager::RegisterComponent<TriggerActivatorComponent>();
	ComponentSerializer::Register<TriggerActivatorComponent>("Trigger Activator", &DescribeTriggerActivator);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<TriggerActivatorComponent>>("Trigger Activator", "Gameplay", &DescribeTriggerActivator));
}

void TriggerActivatorSystem::Release() {} // 持っている物は無い（実行中の値は、全部 Component の中）

//=============================================================================
// 毎フレーム
//=============================================================================
void TriggerActivatorSystem::Update(bool isGameRunning, float deltaTime) {
	if (!isGameRunning) {
		return; // 停止中は何もしない（Target を消したままにしない。Stop で、Play を押した瞬間の状態に戻る）
	}
	EntityManager::ForEach<TriggerActivatorComponent>([deltaTime](Handle<Entity> entity, TriggerActivatorComponent& activator) {
		const Handle<Entity> targetHandle = EntityManager::Find(activator.target);
		Entity* target = EntityManager::Get(targetHandle);
		if (target == nullptr || IsSelfOrDescendant(entity, targetHandle)) {
			return; // Target が無い・自分か親（消すと、範囲の Collider も一緒に消えて、もう入れない）
		}
		if (!activator.isStarted) {
			Start(activator, targetHandle, *target);
		}
		AlphaGroupComponent* group = EntityManager::Get<AlphaGroupComponent>(targetHandle);
		if (UsesFade(activator) && group == nullptr) {
			return; // Start で付けた Alpha Group が、まだ付いていない（付く前に出すと、1フレームだけ濃く見える。次のフレームから見る）
		}
		// --- 入った・出たを見る（Enabled を外した・Entity が無効の間は見ない。フェードは続ける）---
		if (activator.enabled && EntityManager::IsActiveInHierarchy(entity)) {
			const bool isInside = IsSourceInside(entity, activator);
			if (isInside && !activator.isInside) {
				React(activator.onEnter, activator.isEnterUsed, activator, *target);
			} else if (!isInside && activator.isInside) {
				React(activator.onExit, activator.isExitUsed, activator, *target);
			}
			activator.isInside = isInside;
		}
		// --- フェードを進めて、Target の Alpha Group へ書く（フェードを使わないなら書かない。Script が動かす Alpha を邪魔しない）---
		StepFade(activator, *target, deltaTime);
		if (UsesFade(activator)) {
			group->alpha = activator.baseAlpha * activator.fadeAlpha;
		}
	});
}
```

> **`std::ranges::any_of(範囲, 条件)`**（`<algorithm>`）：範囲の中に、条件が true になる物が**1つでもあれば true**（見つかった所で止まる）。`for` で回して `if (...) { return true; }` と書くのと同じ。`GetHits` が返す `std::span` もそのまま渡せる。

> **フェードの速さ**：`MoveTowards(今, 目標, 1フレームに変える量)`。濃さ 1 を `fadeInTime` 秒で変えきるので、1フレームに `deltaTime / fadeInTime` ずつ近づける（WindZoneSystem の風の線と同じ）。消している途中で入り直したら、今の濃さから戻る（0 からやり直さない）。

### `MyEngine/Systems/CameraSystem.h`（1つ足す）
`GetFollowTarget` の下に：
```cpp
	// 同じ物の Entity（無ければ無効な Handle）。Trigger Activator が「入ったのはプレイヤーか」を見るのに使う
	static Handle<Entity> GetFollowTargetEntity();
```

### `MyEngine/Systems/CameraSystem.cpp`（`GetFollowTarget` を置き換えて、1つ足す）
Camera Follow → Target をたどる所を `GetFollowTargetEntity` にまとめて、`GetFollowTarget` はそれを使う形にした（同じたどり方を2回書かない）：
```cpp
const TransformComponent* CameraSystem::GetFollowTarget() {
	// Target の Entity → Transform（どこかで無ければ nullptr）
	return EntityManager::Get<TransformComponent>(GetFollowTargetEntity());
}

Handle<Entity> CameraSystem::GetFollowTargetEntity() {
	// メインカメラの Camera Follow → その Target の Entity（どこかで無ければ無効な Handle）
	const CameraFollowComponent* follow = EntityManager::Get<CameraFollowComponent>(GetMainCameraEntity());
	return (follow != nullptr) ? EntityManager::Find(follow->target) : Handle<Entity>{};
}
```

### `MyEngine/Engine.cpp`（3か所）
```cpp
#include "MyEngine/Systems/TriggerActivatorSystem.h"
```
`DepthOfFieldSystem::Initialize();` の下に：
```cpp
	TriggerActivatorSystem::Initialize(); // Trigger ActivatorをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```
`DepthOfFieldSystem::Release();` の下に：
```cpp
	TriggerActivatorSystem::Release();    // シーン（ウィンドウ）の破棄より後にする
```

### `MyEngine/Window/WindowManager.cpp`（2か所）
```cpp
#include "MyEngine/Systems/TriggerActivatorSystem.h"
```
`UpdateAll` の `CollisionSystem::Update();` の下に：
```cpp
	// 範囲に入った・出たで、Target を出す・消す（当たり判定の後。入ったフレームのうちに出し始める）
	TriggerActivatorSystem::Update(isGameRunning, Time::GetDeltaTime());
```

### `MyEngine/Editor/Windows/HierarchyWindow.cpp`（2か所）
```cpp
#include "MyEngine/Components/TriggerActivatorComponent.h"
```
＋ → 3D Object の、Wind Zone の下に：
```cpp
		if (ImGui::MenuItem("Trigger Activator")) {
			// 入ったら・出たら Target を出す・消す範囲（Is Trigger の箱。範囲は Collider のギズモで見える）。Target は Inspector でドラッグして入れる
			ColliderComponent collider;
			collider.isTrigger = true;
			collider.size = {4.0f, 4.0f, 4.0f};
			EditorHistory::RequestCreate("Trigger Activator", {}, {ComponentSnapshot::Make(collider), ComponentSnapshot::Make(TriggerActivatorComponent{})});
		}
```

### `MyEngine_v1.vcxproj`
`TriggerActivatorComponent.h`・`TriggerActivatorSystem.h`・`TriggerActivatorSystem.cpp` をプロジェクトに足す。

---

## 4. パートC：エディタで置く（チュートリアルの説明）

### 4.1 説明（Target）を作る
1. Hierarchy の ＋ → **Create Entity**。名前を `Tutorial_Move` などに（説明1つにつき1つ）。Transform の Translation の x・y が、画面の中の位置（ピクセル。真ん中が 0,0）になる。
2. `Tutorial_Move` を右クリック → **Create Child** を2回。1つに **Add Component → UI → Text**（文字。Order 1）、もう1つに **UI → Image**（背景の板。Color を黒・a 0.6、Order 0）。Image の Size を文字に合わせる。
3. 置き方は Image・Text と同じ（Anchor・Pivot）。画面の上の方に出すなら、親の Translation の y を 200 くらいに。

### 4.2 範囲（Trigger Activator）を置く
1. Hierarchy の ＋ → **3D Object → Trigger Activator**。Is Trigger の箱（4×4×4）の Collider と Trigger Activator が付いた Entity ができる。
2. 説明を出したい所に動かす。範囲は Collider の線（Scene ビュー。メニューバーの View → Colliders）。大きさは Collider の Size。
3. Inspector の **Target** に、Hierarchy から `Tutorial_Move` をドラッグする。
4. 1.6 の表から選ぶ（初期値のままなら「近づいたら出す・離れたら消す」、フェード 0.3 秒）。
5. Play して、範囲に入ると出る・出ると消えるのを見る。Stop すると元に戻る。

### 4.3 Level01 に置くなら（例）
| 説明 | 範囲 | 設定 |
|---|---|---|
| 「← → で うごく」 | スタートの所 | On Enter None・On Exit Hide・Once・Hide On Start オフ（最初から出ていて、離れたら消える） |
| 「Space で ジャンプ」 | 最初の段の手前 | 初期値（近づいたら出す・離れたら消す） |
| 「水色の円で もう一度 跳べる」 | 円の手前 | Once（一度読んだら出さない） |

- 範囲の Z の大きさは、プレイヤーの奥行き（Z）を含むように（横から見るゲームなので、4 のままでよい）。
- 背景の板は、文字を足しても伸びない（`TextRenderSystem::CalcTextSize` で合わせる Script も書けるが、今は手で合わせる）。

---

## 5. 確かめたこと
Release で、テスト用のシーン（玉が右へ 2m/s で進み、3つの範囲を通る）を動かし、一時的に足したログ（毎フレームの `isInside`・Target の有効・`fadeAlpha`・Alpha Group の Alpha・描くときの `CalcGroupAlpha`）と画面で確かめた（ログは外してビルドし直した）。

| 範囲 | 設定 | 結果 |
|---|---|---|
| A | FollowTarget・Show／Hide・フェード 0.5 秒・Target は Text と Image の親（Alpha Group 無し） | 始めに Alpha Group が付き（2フレーム目）、Target は消えている。入った 1.26 秒に有効になって 0.03 から濃くなり、1.75 秒に 1。出た 2.76 秒から薄くなり、3.25 秒に 0 → 無効。文字と背景の板が一緒にフェードした（下の画像） |
| B | Entity（玉）・Show／Hide・Once・フェード 0 | 入った 3.25 秒にすぐ出て、出た 4.76 秒にすぐ消えた。Alpha Group は付けない（フェードを使わないので） |
| C | AnyRigidbody・Hide／None・Hide On Start オフ・Target に Alpha Group（Alpha 0.5） | 初めから 0.5 の濃さで出ている。入った 5.25 秒から 0.5 → 0 へ 0.5 秒で消えて、無効になった（`baseAlpha` 0.5 から） |

![フェードの様子](images/g9a_fade.png)

- Debug・Release でビルドが通る（警告は前からある `DIRECTINPUT_VERSION` だけ）。
- エディタの Inspector の見た目（Source で Entity を選んだときだけ Source Entity が出る）と、＋ → 3D Object → Trigger Activator は、写経の後にエディタで見てほしい（今回はログと Game の画面だけで確かめた）。
- シーンファイルに、Source が FollowTarget なのに `sourceEntity` が書いてあると「読む所がありません」の警告が出る（Camera Follow の Mode・Depth Of Field の Focus と同じ。エディタで保存し直すと消える）。

---

## 6. 決めたこと・後でやること
| 決めたこと | 理由 |
|---|---|
| チュートリアルは「範囲に入ったら、画面の UI を出す」 | 3D の Text は新しい描き方が要る。こちらは今ある物の組み合わせで済む（ユーザーの判断） |
| 出す・消すは Entity の有効・無効 | 子もまとめて出る・消える。Script の OnEnable・OnDisable も呼ばれる |
| フェードは Alpha Group（親まで掛ける） | 子が何個でも、Alpha 1つで済む。元の色の a を覚えなくてよい |
| 入った・出たは毎フレーム数え直す | 相手が消えても・Collider が2つあっても、正しく「出た」になる |
| Target が Alpha Group を持っていなければ、再生中だけ付ける | エディタで付け忘れてもフェードする。Stop で外れる |
| 名前は Trigger Activator・Alpha Group | 仕組みの名前（ゲームの言葉の「チュートリアル」は入れない。看板・ボスの名前を出すのにも使える） |

- 後で：3D 空間の Text（看板に文字を書く）・Alpha Group をモデル・Primitive Renderer にも効かせる・Image を文字の大きさに合わせる Component。
