# G9d 追加：選んだ項目に、見本の Text の飾りを付ける（PauseMenu）

参考：
- cpprefjp 指示付き初期化（`.color = ...`。C++20）：https://cpprefjp.github.io/lang/cpp20/designated_initialization.html

## このファイルについて
- [G9dUIScreen.md](G9dUIScreen.md) のパートD（PauseMenu）の続き。**ゲーム（AL4_01）だけ**直す。エンジンは変えない。
- ユーザーの頼み：選んだ項目は色を変えるだけでなく、エディタで Resum に付けたような飾り（緑の Shine・跳ねる Bounce など）にしたい。**選んでいない項目は何もしない**。
- 作り方：**見本の Text** を1つ画面の中に置き（Text の Enabled を外して、画面には出さない）、PauseMenu が「選んだ項目」にその飾りを写す。外れた項目は、最初に覚えた元の飾りに戻す。凝り方はコードではなく、エディタで見本を直して決める。
- 確かめた環境：G9d を写した本体（SlotMap の直し入り）と AL4_01 のコピー。Release で、キーを時間で流して画面を撮って確かめた（4章）。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Source/Scripts/UI/PauseMenu.h/.cpp`（中身を入れ替える） | 見本の飾りを、選んだ項目に写す |
| B | エディタで | Pause.uiscreen に Items と見本（SelectedStyle）を置く |

---

## 1. 何をするか

### 1.1 写す飾り・写さない物
| 写す（飾り） | 写さない（項目ごとの物） |
|---|---|
| Color・Outline（太さ・色）・Shadow（する・ずれ・色）・Glow（広さ・色）・Shine（する・色・幅・秒・間）・Motion（種類・大きさ・速さ・ずれ） | Text（文字）・Font・Font Size・Align・Anchor・Pivot・Line Spacing・Letter Spacing・Order・Blend・位置・大きさ |

- 飾りの項目だけを持つ入れ物 `TextStyle` を作り、`From`（Text から取り出す）と `ApplyTo`（Text に書く）で写す。全部の項目を1か所にまとめておくと、写し忘れ・戻し忘れが起きない。
- **元の飾り**は、最初のフレームに項目ごとに覚える（`originalStyles_`）。外れた項目はそれに戻す。項目の Text に何も付けていなければ「何もしない」になる。
- 見本が見つからない（名前の打ち間違い・置き忘れ）ときは、選んだ項目も元のまま（落ちない）。

### 1.2 見本の置き場所
```
Pause              ← UI Screen ＋ PauseMenu
 ├ Dim             ← Image（後ろを暗く）
 ├ Pause           ← Text（見出し）
 ├ SelectedStyle   ← Text（見本。Text の Enabled を外す＝画面に出ない）。Resum に付けていた飾りを、ここに付ける
 └ Items           ← 空の Entity（項目の入れ物）
    ├ Resume       ← Text（飾り無し）
    ├ Restart      ← Text（飾り無し）
    └ Title        ← Text（飾り無し。決めても今は何も起きない）
```
- 見本は **Items の外**（一番上の直下）に置く。Items の中に置くと、項目の1つになってしまう。
- 見本の名前は PauseMenu の **Style Name**（初めは `SelectedStyle`）で変えられる。

### 1.3 `TextStyle::From` の書き方（指示付き初期化）
```cpp
return {
    .color = text.color,
    .outlineWidth = text.outlineWidth,
    ...
};
```
- `.名前 = 値` で、構造体の項目に名前を付けて入れる（C++20 の指示付き初期化）。並びは構造体に書いた順と同じにする（違うとコンパイルで止まる）。何番目の値か数えなくてよいので、項目が多い構造体でも読み違えない。

---

## 2. パートA：PauseMenu（ゲーム）
今の `PauseMenu.h`・`PauseMenu.cpp` を、下の中身に入れ替える（G9d の版から、Normal Color・Selected Color・Selected Scale は無くなる。色も見本で決める）。

### `Source/Scripts/UI/PauseMenu.h`
```cpp
#pragma once
#include <string>
#include <vector>

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/Script.h>


/// <summary>
/// ポーズ画面の項目を選ぶ・決める。ポーズ画面（resources/ui/Pause.uiscreen）の一番上の Entity に付ける
/// <para>項目は、子の Items の子の Text（Hierarchy の上から順）。名前で何をするか決める（Resume：つづける / Restart：やりなおす）</para>
/// <para>選んだ項目には、見本の Text（一番上の直下の子の SelectedStyle）の飾り（色・縁取り・影・光・Shine・Motion）を写す。
/// 選んでいない項目は、元の飾りのまま（何もしない）</para>
/// <para>ポーズ中（ゲームが止まっている間）も、開いている画面の中の Script なので呼ばれる</para>
/// </summary>
class PauseMenu : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	/// <summary>
	/// Text の飾りの部分（文字・フォント・大きさ・位置の合わせ方は入れない。選んだ・外したで入れ替える物だけ）
	/// </summary>
	struct TextStyle {
		Vector4 color;
		float outlineWidth = 0.0f;
		Vector4 outlineColor;
		bool shadow = false;
		Vector2 shadowOffset;
		Vector4 shadowColor;
		float glowWidth = 0.0f;
		Vector4 glowColor;
		bool shine = false;
		Vector4 shineColor;
		float shineWidth = 0.0f;
		float shineDuration = 0.0f;
		float shineInterval = 0.0f;
		TextMotion motion = TextMotion::None;
		float motionAmount = 0.0f;
		float motionSpeed = 0.0f;
		float motionSpread = 0.0f;

		// Text から飾りを取り出す・Text に飾りを書く
		static TextStyle From(const TextComponent& text);
		void ApplyTo(TextComponent& text) const;
	};

	// 項目の並び（子の Items の子。Hierarchy の上から順）
	std::vector<Handle<Entity>> CollectItems() const;
	// 選んでいる項目に見本の飾りを、ほかの項目に元の飾りを書く
	void ApplyLook(const std::vector<Handle<Entity>>& items);
	// 選んでいる項目を決める（名前で何をするか決める）
	void Decide(const std::string& itemName);

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Pause";             // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string styleName_ = "SelectedStyle";      // 選んだ項目の見本の Text の名前（一番上の直下の子。Text の Enabled は外して置く）

	// ===== 実行中だけの値 =====
	int selected_ = 0;                     // 選んでいる項目（上から 0, 1, …）
	bool isDecided_ = false;               // もう決めた（消える動き・幕が閉じる間に、もう一度決めないように）
	std::vector<TextStyle> originalStyles_; // 項目の元の飾り（最初のフレームに覚える。外れた項目をこれに戻す）
};
```

### `Source/Scripts/UI/PauseMenu.cpp`
```cpp
#include "PauseMenu.h"

#include <vector>

#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Input/InputManager.h>

SCRIPT(PauseMenu, "UI");

namespace {
// 上へ・下へ・決める・閉じるのキー（キーボードとパッドのどちらでも）
bool IsUpTriggered() { return InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsButtonTriggered(GamepadButton::DpadUp); }
bool IsDownTriggered() { return InputManager::IsKeyTriggered(DIK_DOWN) || InputManager::IsKeyTriggered(DIK_S) || InputManager::IsButtonTriggered(GamepadButton::DpadDown); }
bool IsDecideTriggered() { return InputManager::IsKeyTriggered(DIK_RETURN) || InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A); }
bool IsCloseTriggered() { return InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start) || InputManager::IsButtonTriggered(GamepadButton::B); }
} // namespace

//=============================================================================
// Text の飾り
//=============================================================================
PauseMenu::TextStyle PauseMenu::TextStyle::From(const TextComponent& text) {
	return {
	    .color = text.color,
	    .outlineWidth = text.outlineWidth,
	    .outlineColor = text.outlineColor,
	    .shadow = text.shadow,
	    .shadowOffset = text.shadowOffset,
	    .shadowColor = text.shadowColor,
	    .glowWidth = text.glowWidth,
	    .glowColor = text.glowColor,
	    .shine = text.shine,
	    .shineColor = text.shineColor,
	    .shineWidth = text.shineWidth,
	    .shineDuration = text.shineDuration,
	    .shineInterval = text.shineInterval,
	    .motion = text.motion,
	    .motionAmount = text.motionAmount,
	    .motionSpeed = text.motionSpeed,
	    .motionSpread = text.motionSpread,
	};
}

void PauseMenu::TextStyle::ApplyTo(TextComponent& text) const {
	text.color = color;
	text.outlineWidth = outlineWidth;
	text.outlineColor = outlineColor;
	text.shadow = shadow;
	text.shadowOffset = shadowOffset;
	text.shadowColor = shadowColor;
	text.glowWidth = glowWidth;
	text.glowColor = glowColor;
	text.shine = shine;
	text.shineColor = shineColor;
	text.shineWidth = shineWidth;
	text.shineDuration = shineDuration;
	text.shineInterval = shineInterval;
	text.motion = motion;
	text.motionAmount = motionAmount;
	text.motionSpeed = motionSpeed;
	text.motionSpread = motionSpread;
}

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	ui.Label("子の Items の子の Text が項目（上から順）。名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）");
	ui.Label("選んだ項目には、見本の Text（Style Name の名前の、一番上の直下の子）の色・縁取り・影・光・Shine・Motion を写す。選んでいない項目は元のまま");
	ui.Label("↑↓・W S・十字で選ぶ / Enter・Space・A で決める / Esc・Start・B で閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Style Name", styleName_, Tip("選んだ項目の見本の Text の名前。見本は Text の Enabled を外して置く（画面には出ない）"));
}

//=============================================================================
// 毎フレーム（ゲームが止まっていても呼ばれる）
//=============================================================================
void PauseMenu::Update() {
	const std::vector<Handle<Entity>> items = CollectItems();
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
	ApplyLook(items);
	// --- 決める ---
	if (IsDecideTriggered()) {
		const Entity* item = EntityManager::Get(items[selected_]);
		if (item != nullptr) {
			Decide(item->name);
		}
	}
}

//=============================================================================
// 項目
//=============================================================================
std::vector<Handle<Entity>> PauseMenu::CollectItems() const {
	std::vector<Handle<Entity>> items;
	if (const GameObject list = gameObject.FindChild("Items")) {
		EntityManager::GetChildren(list.handle, items);
	}
	return items;
}

//=============================================================================
// 見た目
//=============================================================================
void PauseMenu::ApplyLook(const std::vector<Handle<Entity>>& items) {
	// --- 最初のフレームに、項目の元の飾りを覚える（外れた項目を戻すのに使う）---
	if (originalStyles_.size() != items.size()) {
		originalStyles_.clear();
		for (const Handle<Entity> item : items) {
			const TextComponent* text = EntityManager::Get<TextComponent>(item);
			originalStyles_.push_back(text != nullptr ? TextStyle::From(*text) : TextStyle{});
		}
	}
	// --- 見本の飾り（見本が無ければ、選んだ項目も元のまま）---
	const GameObject styleObject = gameObject.FindChild(styleName_);
	const TextComponent* styleText = styleObject ? styleObject.GetComponent<TextComponent>() : nullptr;
	for (size_t i = 0; i < items.size(); ++i) {
		TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (text == nullptr) {
			continue;
		}
		const bool isSelected = (static_cast<int>(i) == selected_);
		if (isSelected && styleText != nullptr) {
			TextStyle::From(*styleText).ApplyTo(*text);
		} else {
			originalStyles_[i].ApplyTo(*text);
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
- `ApplyLook` は毎フレーム全部の項目に書く（選んでいる項目には見本、ほかには元の飾り）。見本を Inspector で直すと、Play 中でもすぐ選んだ項目に出る。
- `originalStyles_.size() != items.size()`：最初のフレーム（まだ空）に覚える。項目の数が変わったときも覚え直す。
- G9d の版にあった `Title` の扱いは同じ（決めても何もしない。タイトル画面を作ったら、`Decide` に足す）。

---

## 3. パートB：エディタで Pause.uiscreen を直す
1. Pause.uiscreen をダブルクリック（編集モード）。
2. **Resum** を選んで Ctrl+D（複製）。複製の名前を **`SelectedStyle`** にして、Inspector の Text の **Enabled を外す**（画面に出なくなる）。飾り（Shine・Motion・色など）は Resum のまま残しておく。
3. 一番上の Pause を選んで ＋ → **Create Entity**。名前を **`Items`** にする。
4. **Resum・Restart・Title** を、この順に Hierarchy で **Items の上へドラッグ**して子にする（並びは作った順なので、Items の中でもこの順になる）。
5. Resum の名前を **`Resume`** に（決めたときに名前で「つづける」と分かるように）。Inspector で **Shine を外し、Motion を None** に（項目は飾り無し。飾りは見本だけに付ける）。
6. 見出しの Pause の Text の最後の改行を消す（`Pause\n` → `Pause`。下に1行のびて、位置がずれるので）。
7. **Done** → Play → Esc。選んだ項目だけ緑の光が通って跳ねる。↑↓で外れた項目は元に戻る。
- 見本の飾りを変えたいときは、SelectedStyle の Text の Enabled を一度入れて見ながら直し、また外す。

---

## 4. 確かめたこと
Release（エディタ無し）で、3章の形に直した Pause.uiscreen（見本は Resum の飾り＝緑の Shine・Bounce 6・速さ 1・ずれ 0.2）を置いた Level01 のコピーに、キーを時間で流して（テストのときだけ InputManager に入れた仕掛け。外した）、0.2 秒ごとに画面を撮った。

| キー | 結果 |
|---|---|
| Esc | 画面が出る。Resume だけ緑の光が左から右へ通り、文字が跳ねる。Restart・Title は白いまま動かない |
| ↓ | Restart に光と跳ねが移る。Resume は元の白い文字に戻る |
| ↓ | Title に移る |
| Enter（Title） | 何も起きない（画面は出たまま、また選べる） |

確かめていないこと：エディタで 3章の手順を通しで行うこと（テストのファイルは手で書いた）。
