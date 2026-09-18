# Editor：現在の作業と設計記録

## 読む順番

1. **[まとめて：変換中の文字の表示・Undoの作り直し・コピー・＋ボタン・フォルダ分け（2026-09-18）](#まとめて変換中の文字の表示undoの作り直しコピーボタンフォルダ分け2026-09-18)** ← 今回の作業
2. **[Undoの仕組み（解説）](#undoの仕組み解説)** ← 今回のコードの読み方。残しておく解説
3. 「1. 日本語入力の修正」〜「4. 確認と次の作業」は、Codexが書いた前回の手順（反映済み）。今回の作業で `EditorHistory`・Inspector・`Win32Window` のコードは置き換わるので、読み返すのは経緯を知りたいときだけでよい
4. [残す設計記録](#残す設計記録)

コードはMarkdownから写経する。作業ブランチは `codex/component-render-guide`、Push禁止。
前回までの手順はコミット `936cdcf` にある（Codexが `Editor.md`・`Light.md`・`FrameLoop.md` を短くしたので、それより前の説明はこのコミットの中）。

| 現在の状態 | 内容 |
|---|---|
| 動作確認済み | Entity作成時の自動Rename停止、HierarchyのUndo/Redo・コピー、日本語の入力（Codexの版：入力欄の上に別の箱で表示） |
| 今回 | 変換中の文字を入力欄の中に表示、Undoの作り直し（Componentを増やしてもUndo側を直さなくてよい形）、(Copy1)(Copy2)、右クリックで貼り付け、＋ボタン、Editorフォルダの整理 |
| 今回は対象外 | ゲーム固有のComponentの「実体の置き場所」（段階3で作る）、ゲーム実行の巻き戻し、シーン保存/復元 |

---

## まとめて：変換中の文字の表示・Undoの作り直し・コピー・＋ボタン・フォルダ分け（2026-09-18）

### 0. 今回やること

| 要望 | やること | パート |
|---|---|---|
| 変換中の文字を、画像のように入力欄の中に出したい | Windowsの小さな変換窓は出さず、**ImGuiの入力欄のカーソルの所に自分で描く**。候補の一覧はWindowsのまま、行のすぐ下に出す。ついでに、変換窓が出なかった原因らしき所（メッセージの取り出し方）も直す | G |
| 必要のない変数・関数を消したい | 下の「消すもの」の表。Codexの版で増えた物は、作り直しでほぼ全部いらなくなる | 全部 |
| `MyEngine/Editor` のファイルが多い | 役割ごとに5つのフォルダに分ける | A |
| `TrackInspectorValues` がComponentごとに手直しが要る | **Componentの中身を「描く前」と「描いた後」でバイトごとに比べる**。項目を1つずつ書き並べるのをやめるので、Componentを増やしてもUndo側は1行も変えない | C・D・E |
| Undoの仕組みを知りたい | 「[Undoの仕組み（解説）](#undoの仕組み解説)」の節 | ― |
| コピーの名前を (Copy1)(Copy2) にしたい | 同じ親の下で空いている一番小さい番号を付ける。コピーのコピーも `(Copy1) (Copy1)` にならない | D |
| 右クリックで貼り付けたい | Entityの右クリックに **Paste（兄弟として）/ Paste As Child（子として）**、一覧の何も無い所の右クリックに **Create / Paste** | F |
| Add・Createに大きな＋を付けたい | 線で＋を描くボタン `PlusButton` を作り、**Hierarchyの右上（作成メニュー）** と **Add Component** に付ける。Add ComponentはComponentの見出しと同じ色の帯にする | E・F・G |

ついでに変わること（小さいもの）：
- **Undo / Redo のボタンは、上のメニューの Edit へ移す**。Ctrl+Z / Ctrl+Y（Ctrl+Shift+Z）はHierarchyだけでなく、どのウィンドウを触っていても効く（文字の入力中とドラッグ中を除く）
- **Destroy Selected ボタンは無くし、Deleteキーと右クリックの Delete にする**
- Componentの見出しを右クリックすると **Remove Component**（Undoで値ごと戻る）
- Hierarchyの何も無い所をクリックすると、選択が外れる
- Inspectorに **ID**（下で説明する EntityId）を出す

#### 順番

1. **パートAのフォルダ分けを最初にやる**（ほかのパートのコードは、新しい場所のinclude名で書いてある）
2. B〜Hを写す。**B〜Hはお互いを呼び合うので、全部そろうまでビルドは通らない**
3. ゲーム側に配ったヘッダーの古いフォルダを消してから（A-4）、エンジン → ゲームの順にビルド

#### 新しく作るファイル（すべてエンジン側）

| ファイル | 中身 |
|---|---|
| `Editor/History/EditorHistory.cpp` | Undoの本体（今は `.h` に全部入っているのを、`.h` は窓口だけ・中身は `.cpp` に分ける） |
| `Editor/Inspector/ComponentEditor.h` / `.cpp` | Componentごとの「エディタでの扱い方」の土台と、その登録表 |
| `Editor/Inspector/TransformEditor.h` / `.cpp` | TransformのInspector（今の `InspectorWindow::DrawTransform` の引っ越し） |
| `Editor/Inspector/ModelRendererEditor.h` / `.cpp` | Model RendererのInspector（今の `DrawModelRenderer` と `DrawModelPicker` の引っ越し） |
| `Editor/Widgets/ImeInput.h` / `.cpp` | 日本語の変換中の文字を入力欄の中に描く |

#### 消すもの（リファクタリング）

| 場所 | 消すもの | 理由 |
|---|---|---|
| `EditorWidgets.h` | `GetImeComposition`・`DrawImeCompositionPreview`・`<Windows.h>`・`<imm.h>` | 入力欄の上の別の箱をやめる。IMEを読む処理は `ImeInput.cpp` に移す（Windows.hを、あちこちから読むヘッダーに入れない） |
| `InspectorWindow.cpp` | `InspectorValues`・`ReadInspectorValues`・`TrackInspectorValues`・`ComponentMenuItem`・`kComponentMenu`・`ComponentCategory`（`ComponentEditor.h` へ）・モデル一覧の走査（`ModelRendererEditor.cpp` へ）・Ctrl+Zの処理 | バイトの比較と `ComponentEditor` の登録表に置き換わる |
| `InspectorWindow.h` | `DrawTransform`・`DrawModelRenderer`・`DrawModelPicker` | `ComponentEditor` 側へ |
| `EditorHistory.h` | `Detail` の中身ぜんぶ（`Identity`・`Ref`・`Find`・`PropertyValue`・`PropertyEdit`・`TrackValue`・`Member`・`BeginInspector`・`FinishEdit`・`batchHasChange` など） | 相手を `EntityId` で覚えるようになり、`Identity` の共有ポインタが要らなくなる。値の記録はバイトの比較になる |
| `HierarchyWindow.h` / `.cpp` | `createChildOf_`・`destroyRequest_`・`reparentChild_`・`reparentParent_`・`createRootRequest_`・`reparentRequest_`・`ApplyRequests` | `EditorHistory` のRequestがもう「予約して次のフレームで実行」なので、Hierarchyでもう1回予約し直す必要がない |
| `Win32Window.h` | `imeComposing_`・`imeEnter_`・`imeEscape_` | `ImeInput` へ移す |
| `Win32Window.cpp` | `case` ごとに `self` を取り直している4か所 | 関数の頭で1回取れば足りる（`self` の隠蔽の警告C4456も消える） |
| `EntityManager` | `RemoveModelRendererAtBoundary` | `RequestRemoveModelRenderer`（予約）に変える。追加と同じく `FlushComponentChanges` で反映する形にそろえる |
| `ImGuiManager.cpp` | `DrawProfilerBar` | どこからも呼ばれていない |
| `ImGuiManager.h` | `hwnd_`・`isShowStyleEditor_` | 代入だけで、読んでいる所が無い |

---

### 日本語：変換中の文字を入力欄の中に出す

#### 画像のエンジンは何をしているか

画像の白い箱（「あいうえ」に点線の下線）は、**Windows（IME）が自分で描いている変換窓**。ImGuiは入力欄のカーソルの位置をIMEに教えていて（`ImmSetCompositionWindow`）、IMEはそこにぴったり重ねて変換窓を出す。なので、本当はこのエンジンでも最初から同じ見た目になるはずだった。

#### このエンジンで出なかった原因（たぶん）

`Win32Window::ProcessMessage` がメッセージを取り出すときに、**自分のウィンドウを指定している**。

```cpp
while (PeekMessage(&msg, hwnd_, 0, 0, PM_REMOVE)) { // hwnd_宛てのメッセージしか取り出さない
```

IMEの変換窓は、Windowsが同じスレッドに作る**別のウィンドウ**。ウィンドウを指定して取り出すと、それ以外のウィンドウ宛てのメッセージ（描き直しの合図など）が処理されないまま残り続けるので、変換窓は表示されない。Windows 11の候補の一覧は別のプログラムが描いているので、こちらは出る——という今の症状と合う。

ただし実機で確かめたわけではないので「たぶん」。**どちらにしても今回は変換中の文字を自分で描くので、結果は同じになる**。取り出し方の修正は、IME以外（Windowsが裏で作るほかのウィンドウ）にとっても正しい書き方なので入れておく。

#### 自分で描く理由

原因を直してWindowsの変換窓に任せる手もあるが、今回は自分で描く。
- 入力欄と同じ色・同じフォントで出せる（画像の白い箱はWindowsの見た目なので、暗いエディタの中では浮く）
- IMEの種類（新しいMicrosoft IME・以前のバージョン・Google日本語入力）や設定で見た目が変わらない
- UnityやUnreal Engineも、変換中の文字は自分で描いている（Windowsからは文字と状態だけもらう）

#### 仕組み

```
Windows（IME）                        エンジン
──────────────                        ─────────────────────────────
キーを押す → 変換中の文字が変わる
                                      ImeInput::NewFrame
                                        ImmGetCompositionStringW で
                                        「変換中の文字・カーソル・変換の対象の文節」を読む
                                      ImGuiが入力欄を描く（カーソルの位置が分かる）
                                      ImeInput::DrawComposition
                                        カーソルの所に、入力欄と同じ色の箱を置いて文字を描く
候補の一覧を描く（行のすぐ下）         ← ImeInput の SetImeData で「この行を避けて」と頼む
Enterで確定 → 確定した文字が WM_CHAR  → ImGuiの入力欄に入る（今までどおり）
```

```
  ┌─────────────────────────┐
  │ あいうえ|               │   ← 入力欄の中（ImGuiで描く）。変換前は細い下線、変換中の文節は色付き＋太い下線
  └─────────────────────────┘
  ┌───────────────────┐
  │ 1 あいうえお       │       ← 候補の一覧（Windowsが描く）
  │ 2 あいうえおかきくけこ │
  └───────────────────┘
```

- Windowsの変換窓は `WM_IME_SETCONTEXT` の `ISC_SHOWUICOMPOSITIONWINDOW` を外して出さない（候補の一覧の印は残すので、一覧は出る）
- 変換が始まったとき入力欄の文字が選択されていたら（名前の変更は最初に全部選択する）、**先に消す**。メモ帳などと同じ動きで、画像のように変換中の文字が欄の先頭に出る
- 入力欄の外をクリックしたのに変換中の文字が残っていたら、捨てる（ほかの入力欄やSceneビューのカメラ操作に持ち越さない）
- Codexが入れた「IMEが確定に使ったEnter/EscをImGuiに渡さない」処理はそのまま残し、`ImeInput::HandleMessage` へ移す。`WM_CHAR` の `'\r'` と `Esc` を捨てる部分は消す（ImGuiは1行の入力欄に制御文字を入れないので、元から何も起きない）
- **全部の入力欄に効く**（Hierarchyの名前・Add Componentの検索・数値の直接入力など）。`EditorWidgets::InputText` を使っている欄だけは、選択を先に消す動きも付く

---

### A. フォルダ分け

#### A-1. 分け方

```
MyEngine/Editor/
├─ ImGuiManager.h / .cpp        ImGuiの初期化・毎フレームの入口（ゲーム側もincludeするので動かさない）
├─ Profiler.h / .cpp            Collisionなどエディタ以外からも使うので動かさない
├─ History/
│   └─ EditorHistory.h / .cpp   Undo・Redo・コピー
├─ Inspector/                   Componentごとの「Inspectorでの扱い方」（Componentが増えるとここが増える）
│   ├─ ComponentEditor.h / .cpp
│   ├─ TransformEditor.h / .cpp
│   └─ ModelRendererEditor.h / .cpp
├─ Viewport/                    Game / Scene の2画面とその上に描く物
│   ├─ EditorViewport.h / .cpp
│   ├─ ViewportWindow.h / .cpp
│   ├─ EditorGrid.h / .cpp
│   └─ EditorOverlay.h / .cpp
├─ Widgets/                     ウィンドウをまたいで使う部品
│   ├─ EditorWidgets.h
│   └─ ImeInput.h / .cpp
└─ Windows/                     ドッキングするウィンドウ
    ├─ HierarchyWindow.h / .cpp
    └─ InspectorWindow.h / .cpp
```

「Componentが増えると増えるファイル」（`Inspector/`）と、「一度作ったらあまり増えないファイル」を分けておくのが目的。今後、Project（アセット）ウィンドウやConsoleを作ったら `Windows/` に足す。

#### A-2. 動かし方（Visual Studio）

1. ソリューションエクスプローラーの上の「すべてのファイルを表示」を押す（フォルダがディスクどおりに見える）
2. `MyEngine/Editor` を右クリック → 追加 → 新しいフォルダー で `History`・`Inspector`・`Viewport`・`Widgets`・`Windows` を作る
3. ファイルを新しいフォルダへドラッグする（ディスク上のファイルも一緒に動き、`.vcxproj` も書き換わる）

| 今の場所 | 移動先 |
|---|---|
| `Editor/EditorHistory.h` | `Editor/History/` |
| `Editor/EditorWidgets.h` | `Editor/Widgets/` |
| `Editor/HierarchyWindow.h` `.cpp`・`Editor/InspectorWindow.h` `.cpp` | `Editor/Windows/` |
| `Editor/EditorViewport.h` `.cpp`・`Editor/ViewportWindow.h` `.cpp`・`Editor/EditorGrid.h` `.cpp`・`Editor/EditorOverlay.h` `.cpp` | `Editor/Viewport/` |

新しく作るファイル（上の表）は、最初から新しいフォルダに作る。

うまく動かせなかったときは、エクスプローラーで動かしてから、VS側で黄色い「！」が付いた古い項目を「プロジェクトから除外」→ 新しい場所のファイルを「プロジェクトに含める」でもよい。

#### A-3. includeの書き換え

動かしたファイルを読んでいる所。`Ctrl+Shift+H`（フォルダーを指定して置換）で `MyEngine/Editor/EditorOverlay.h` → `MyEngine/Editor/Viewport/EditorOverlay.h` のように1つずつ置き換えると早い。

| ファイル | 変える行 |
|---|---|
| `Editor/Viewport/EditorViewport.h` | `EditorGrid.h`・`ViewportWindow.h` → `Viewport/` 付き |
| `Editor/Viewport/EditorViewport.cpp` | `EditorOverlay.h` → `Viewport/` 付き |
| `Editor/Viewport/EditorOverlay.cpp` | 自分の `EditorOverlay.h` → `Viewport/` 付き |
| `Engine.cpp` | `EditorOverlay.h` → `Viewport/` 付き |
| `Window/WindowManager.h` | `EditorViewport.h` → `Viewport/` 付き |
| `Window/WindowManager.cpp` | `EditorOverlay.h`・`ViewportWindow.h` → `Viewport/` 付き、`EditorHistory.h` → `History/` 付き |
| `Scene/SceneManager.cpp` | `EditorHistory.h` → `History/` 付き |
| `Editor/ImGuiManager.cpp` | パートHでまとめて直す |
| `Editor/Windows/*`・`Editor/History/*` | パートD・E・Fで全体を差し替えるので、ここでは直さなくてよい |
| **ゲーム** `GameScene.cpp` | `#include <MyEngine/Editor/EditorOverlay.h>` → `#include <MyEngine/Editor/Viewport/EditorOverlay.h>` |

`#include "EditorGrid.h"` のように、同じフォルダのファイルを名前だけで読んでいる所（`EditorGrid.cpp`・`ViewportWindow.cpp`・`EditorViewport.cpp` の1行目など）は、一緒に動くのでそのままでよい。

#### A-4. ゲーム側に配った古いヘッダーを消す

エンジンのビルド後の `xcopy` は、**消した・動かしたヘッダーを配り先から消さない**。古い `CG3_Project/MyEngine/include/MyEngine/Editor/HierarchyWindow.h` などが残ると、古い場所をincludeしてもビルドが通ってしまい、間違いに気づけない。

エンジンをビルドする前に、**`CG3_Project/MyEngine/include/MyEngine/Editor` フォルダを丸ごと消す**（ビルドで新しい物が配られ直す）。

毎回消すのが面倒なら、3eで書いた置き換えをしておくと、今後は自動で消える。エンジンのプロパティ →「ビルドイベント」→「ビルド後イベント」→「コマンドライン」（**構成：すべての構成**）の、ヘッダーをコピーしている `xcopy` の1行を次に置き換える（説明はコミット `936cdcf` の `Editor.md` の771行目あたり）。

```bat
robocopy "$(ProjectDir)MyEngine" "$(SolutionDir)MyEngine\include\MyEngine" *.h /S /PURGE /NJH /NJS /NDL /NP /NFL & if errorlevel 8 (exit /b 1) else (cmd /c "exit /b 0")
```

`/PURGE` は「コピー先にあってコピー元に無い `.h`」を消す。robocopyは成功でも終了コードが1〜7になるので、8未満を0（成功）に直している。

---

### B. EntityId と、Componentを外す予約（Entity・EntityManager）

**なぜ要るか**：Undoで消したEntityを戻すと、SlotMapの世代が進むので**Handleは別の値になる**。今の `EditorHistory` はそのために `Identity`（Handleを入れておく共有の箱）を作り、戻すたびに中身を差し替えていた。代わりに、**Entityに「消して作り直しても変わらない番号」を持たせる**と、履歴は番号で相手を覚えるだけでよくなる。将来のシーン保存でも、親子を番号で書くのに使う（UnityのGUID / fileIDと同じ役割）。

#### B-1. `Entity/Entity.h`（全体を差し替え）

```cpp
#pragma once
#include <cstdint>
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/TransformComponent.h"
#include "MyEngine/Entity/ModelRendererComponent.h"


// Entityに1つずつ配る、消して作り直しても変わらない番号（0は「無し」）
// Handleは消すと無効になり、作り直すと別の値になる。Undoや将来のシーン保存では、相手をこの番号で覚える
using EntityId = uint64_t;


/// <summary>
/// シーンに置くもの1個
/// <para>Componentそのものは持たず、Handleで指す（実体はそれぞれのManagerが型別に持つ）</para>
/// <para>ARCHITECTURE.md 段階3でEntityが「ただのID」になったら、name はEditor用の別の表に移す</para>
/// </summary>
struct Entity {
	std::string name = "Entity";           // ヒエラルキーに出す名前
	EntityId id = 0;                       // 消して作り直しても変わらない番号（Undo・保存用）
	Handle<Entity> self;                   // 自分を指すHandle（一覧から選ぶときに使う）
	Handle<Entity> parent;                 // 親。無効なら一番上（root）
	Handle<TransformComponent> transform;  // 全Entityが必ず1つ持つ
	Handle<ModelRendererComponent> render; // 未追加なら無効なHandle
	bool isActive = true;                  // falseで更新・描画の対象から外す（使うのは後のStep）
};
```

#### B-2. `Entity/EntityManager.h`（全体を差し替え）

変わった所：`CreateWithId`・`NewId`・`FindById` を追加、`RequestRemoveModelRenderer` を追加して `RemoveModelRendererAtBoundary` を削除、メンバに `idToHandle_`・`nextId_`・`pendingRemoveModelRenderer_` を追加。

```cpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// Entityの管理。実体はここだけが持ち、外にはHandleを渡す（LightManagerと同じ形）
/// <para>ARCHITECTURE.md の更新順序のうち、2（ワールド行列）と6（破棄）を担当する</para>
/// </summary>
class EntityManager {
public:
	static void Initialize();
	static void Release();

	// ===== 生成・破棄 =====
	/// <summary>
	/// Entityを作る。TransformComponentも一緒に作られる
	/// </summary>
	/// <param name="name">ヒエラルキーに出す名前</param>
	/// <param name="parent">親。省略すると一番上に置く</param>
	static Handle<Entity> Create(const std::string& name = "Entity", Handle<Entity> parent = {});

	/// <summary>
	/// 番号（EntityId）を指定して作る。Undoで消したEntityを戻すとき・将来のシーン読み込み用
	/// </summary>
	static Handle<Entity> CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent);

	/// <summary>
	/// まだ誰も使っていない番号を1つもらう（貼り付けで、作る前に番号を決めておきたいとき用）
	/// </summary>
	static EntityId NewId();

	/// <summary>
	/// 破棄を予約する。実際に消えるのはフレームの最後（子も一緒に消える）
	/// </summary>
	static void Destroy(Handle<Entity> handle);

	// ===== 取得（受け取ったポインタは使い捨てにする。ARCHITECTURE.md 原則1） =====
	static Entity* Get(Handle<Entity> handle);
	static TransformComponent* GetTransform(Handle<Entity> handle);
	static bool IsAlive(Handle<Entity> handle);
	// 番号からHandleを探す。無ければ無効なHandle（0を渡しても無効なHandle＝root扱いにできる）
	static Handle<Entity> FindById(EntityId id);

	// ===== 親子 =====
	/// <summary>
	/// 親を付け替える。自分の子孫を親にしようとしたときは何もしない（輪になるのを防ぐ）
	/// </summary>
	static void SetParent(Handle<Entity> child, Handle<Entity> parent);

	/// <summary>
	/// 親をたどって、handleがancestorの子孫かを調べる
	/// </summary>
	static bool IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor);

	/// <summary>
	/// 子のHandleを out に詰める（毎フレーム呼ぶので、outは使い回す）
	/// </summary>
	static void GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out);

	/// <summary>
	/// 親を持たないEntity（一番上）のHandleを out に詰める
	/// </summary>
	static void GetRoots(std::vector<Handle<Entity>>& out);

	// ===== ModelRendererComponent =====
	// 追加・取り外しは予約だけ。実際に変わるのは次のFlushComponentChanges（UIの一覧を回している途中で配列を動かさないため）
	// initialを渡すと、その値で作られる（コードからモデル付きのEntityを作るとき・Undoで戻すとき用）
	static void RequestAddModelRenderer(Handle<Entity> handle, const ModelRendererComponent& initial = {});
	static void RequestRemoveModelRenderer(Handle<Entity> handle);
	static bool IsModelRendererAddPending(Handle<Entity> handle);
	static ModelRendererComponent* GetModelRenderer(Handle<Entity> handle);

	// Update冒頭で1回呼ぶ。Componentの追加・取り外しをここでまとめて反映する
	static void FlushComponentChanges();


	// ===== フレーム更新 =====
	/// <summary>
	/// 更新順序2：ワールド行列を親→子の順で計算する。シーンのUpdateの後に呼ぶ
	/// </summary>
	static void UpdateTransforms();

	/// <summary>
	/// 更新順序6：破棄予約をまとめて反映する。フレームの最後に呼ぶ
	/// </summary>
	static void FlushDestroy();

	// ===== 一覧（ヒエラルキー用） =====
	static size_t GetCount();
	static SlotMap<Entity>& GetAll() { return instance_->entities_; }


private:
	static constexpr uint32_t kMaxParentDepth = 64; // 親をたどる回数の上限（万一輪になっても止まるように）

	static EntityManager* instance_;

	// 破棄予約されたEntityと、その子孫を outに集める
	void CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
	// 親を何回たどるとrootに着くか
	uint32_t CalcDepth(Handle<Entity> handle);

	SlotMap<Entity> entities_;
	SlotMap<TransformComponent> transforms_;
	SlotMap<ModelRendererComponent> modelRenderers_;
	std::unordered_map<EntityId, Handle<Entity>> idToHandle_; // 番号 → Handle（生きているEntityだけ）
	EntityId nextId_ = 1;                                      // 次に配る番号（0は「無し」なので1から）
	std::vector<std::pair<Handle<Entity>, ModelRendererComponent>> pendingAddModelRenderer_; // (追加先, 初期値)
	std::vector<Handle<Entity>> pendingRemoveModelRenderer_;                                // 取り外し予約
	std::vector<Handle<Entity>> pendingDestroy_; // 破棄予約
	// 毎フレーム使う作業用の配列（確保し直さないようにメンバで持つ）
	std::vector<std::pair<uint32_t, Handle<Entity>>> updateOrder_; // (深さ, Handle)
	std::vector<Handle<Entity>> destroyWork_;
};
```

#### B-3. `Entity/EntityManager.cpp`

`Create` と `Destroy` の所を差し替える（`Create` は番号を配って `CreateWithId` を呼ぶだけになる）。

```cpp
Handle<Entity> EntityManager::Create(const std::string& name, Handle<Entity> parent) {
	return CreateWithId(NewId(), name, parent); // 新しい番号を配って作る
}

Handle<Entity> EntityManager::CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent) {
	MY_ASSERT_MSG(id != 0 && !FindById(id).IsValid(), "EntityIdが0か、もう使われています");
	// Transformは全Entityが必ず持つので、一緒に作る
	Handle<TransformComponent> transform = instance_->transforms_.Create();
	Handle<Entity> handle = instance_->entities_.Create();

	Entity* entity = instance_->entities_.Get(handle);
	MY_ASSERT_MSG(entity, "作った直後のEntityが取れませんでした");
	entity->name = name;
	entity->id = id;
	entity->self = handle; // 一覧から選ぶときに使う
	entity->transform = transform;
	instance_->idToHandle_[id] = handle;
	// 番号を指定して作ったときも、この先配る番号とぶつからないようにする
	instance_->nextId_ = (std::max)(instance_->nextId_, id + 1);
	// 親は SetParent を通す（輪になっていないかを見るため）
	if (parent.IsValid()) {
		SetParent(handle, parent);
	}
	return handle;
}

EntityId EntityManager::NewId() { return instance_->nextId_++; }

void EntityManager::Destroy(Handle<Entity> handle) { instance_->pendingDestroy_.push_back(handle); }
```

`IsAlive` の下に追加。

```cpp
Handle<Entity> EntityManager::FindById(EntityId id) {
	auto it = instance_->idToHandle_.find(id);
	return (it != instance_->idToHandle_.end()) ? it->second : Handle<Entity>{};
}
```

`RequestAddModelRenderer` の下に追加。

```cpp
void EntityManager::RequestRemoveModelRenderer(Handle<Entity> handle) {
	// まだ追加の予約だけの段階なら、予約を取り消すだけでよい
	std::erase_if(instance_->pendingAddModelRenderer_, [handle](const auto& pending) { return pending.first == handle; });
	instance_->pendingRemoveModelRenderer_.push_back(handle);
}
```

`FlushComponentChanges` の頭に「取り外し」を足す（追加の `for` はそのまま）。

```cpp
void EntityManager::FlushComponentChanges() {
	EntityManager& self = *instance_;
	// --- 取り外し ---
	for (Handle<Entity> handle : self.pendingRemoveModelRenderer_) {
		if (Entity* entity = self.entities_.Get(handle)) {
			self.modelRenderers_.Destroy(entity->render); // 持っていなければ何もしない
			entity->render = {};
		}
	}
	self.pendingRemoveModelRenderer_.clear();

	// --- 追加 ---
	for (const auto& [handle, initial] : self.pendingAddModelRenderer_) {
```

`FlushDestroy` の、Componentを消している所の上に1行足す。

```cpp
		self.idToHandle_.erase(entity->id); // 番号の表からも外す
		self.modelRenderers_.Destroy(entity->render);
		self.transforms_.Destroy(entity->transform); // Componentも一緒に消す
		self.entities_.Destroy(handle);
```

ファイルの最後の `RemoveModelRendererAtBoundary` は関数ごと消す。

---

### C. ComponentEditor（Componentごとの「エディタでの扱い方」）

Unityの `CustomEditor` と同じ考え方。**Componentの種類ごとに1つクラスを作り、「名前・カテゴリ・実体の取り方・足し方・外し方・Inspectorの中身」をまとめて持たせる**。Inspector・Add Component・Undo・コピーは、Componentの型を知らずにこの窓口だけを使う。

```
          ┌───────── ComponentEditor（窓口：void* で扱う）─────────┐
          │ GetName / GetCategory / GetSize / Get / RequestAdd /   │
          │ RequestRemove / Draw                                    │
          └────────────────────────────────────────────────────────┘
                              ▲ 継承
          ┌───────── TypedComponentEditor<T>（void* ⇔ T を変換）───┐
          └────────────────────────────────────────────────────────┘
                 ▲                                  ▲
        TransformEditor                   ModelRendererEditor        ← Componentごとに書くのはここだけ
```

`TypedComponentEditor<T>` を挟むのは、継承したクラスが `void*` のキャストを書かなくて済むようにするため（`GetSize` と `Get` の型の食い違いも起きない）。
また `static_assert(std::is_trivially_copyable_v<T>)` で、**memcpyで写せない型（`std::string` やポインタを持つ型）をComponentにするとコンパイルエラーになる**。ARCHITECTURE.md 原則2（Componentはデータだけ）を、コンパイラが守らせてくれる。

#### C-1. `Editor/Inspector/ComponentEditor.h`（新規）

```cpp
#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// Add Componentのメニューの分け方（並びはこの順）
/// </summary>
enum class ComponentCategory {
	Core,        // Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
	Rendering3D, // 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
	Rendering2D, // 2D（将来のSpriteRenderer）
	Lighting,    // ライト（Light.md Step 6）
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
};


/// <summary>
/// Componentの種類ごとに1つ作る「エディタでの扱い方」（UnityのCustomEditorと同じ考え方）
/// <para>Inspectorの表示・Add Component・Undo・コピーは、全部この窓口を通してComponentを触る</para>
/// <para>新しいComponentを作ったら、TypedComponentEditorを継承したクラスを作って ComponentEditorRegistry::Register するだけでよい</para>
/// </summary>
class ComponentEditor {
public:
	virtual ~ComponentEditor() = default;

	// ===== 種類の情報 =====
	virtual const char* GetName() const = 0;           // Inspectorの見出し・Add Componentの表示名
	virtual ComponentCategory GetCategory() const = 0; // Add Componentでどのカテゴリに出すか
	virtual bool IsOptional() const { return true; }   // Add・Removeの対象か（Transformのように必ず持つものはfalse）
	virtual size_t GetSize() const = 0;                // Componentの大きさ（Undo・コピーで中身を丸ごと写すのに使う）

	// ===== 実体の出し入れ（実体を持っているManagerへの窓口）=====
	virtual void* Get(Handle<Entity> handle) const = 0;                   // 持っていなければnullptr。ポインタは使い捨て
	virtual bool IsAddPending(Handle<Entity>) const { return false; }     // 追加を予約済みか
	virtual void RequestAdd(Handle<Entity>, const void* /*initial*/) const {} // 追加を予約（initialがnullptrなら初期値）
	virtual void RequestRemove(Handle<Entity>) const {}                   // 取り外しを予約

	// ===== Inspectorの中身 =====
	virtual void Draw(void* component) const = 0;
};


/// <summary>
/// ComponentEditorの void* を、決まった型 T に直してくれる土台
/// <para>継承したクラスは T* / T& で書けるので、キャストの書き間違いが起きない</para>
/// </summary>
template<class T>
class TypedComponentEditor : public ComponentEditor {
	// Undo・コピーは中身をバイト列として写す。ポインタや std::string を持つと写した先で壊れるので、持てないようにする
	static_assert(std::is_trivially_copyable_v<T>, "Componentはmemcpyで写せる型にしてください（ARCHITECTURE.md 原則2）");

public:
	size_t GetSize() const final { return sizeof(T); }
	void* Get(Handle<Entity> handle) const final { return GetComponent(handle); }
	void RequestAdd(Handle<Entity> handle, const void* initial) const final {
		T value{};
		if (initial) {
			std::memcpy(&value, initial, sizeof(T)); // バイト列からTへ戻す
		}
		RequestAddComponent(handle, value);
	}
	void Draw(void* component) const final { DrawComponent(*static_cast<T*>(component)); }

protected:
	virtual T* GetComponent(Handle<Entity> handle) const = 0;
	virtual void RequestAddComponent(Handle<Entity>, const T&) const {} // 外せないComponentは書かなくてよい
	virtual void DrawComponent(T& component) const = 0;
};


/// <summary>
/// ComponentEditorの登録表。Inspectorは登録した順に区画を並べる
/// </summary>
class ComponentEditorRegistry {
public:
	// 登録する。同じ名前のものが登録済みなら何もしない（シーンを作り直すたびに増えないように）
	static void Register(std::unique_ptr<ComponentEditor> editor);
	static const std::vector<std::unique_ptr<ComponentEditor>>& GetAll();
};
```

#### C-2. `Editor/Inspector/ComponentEditor.cpp`（新規）

```cpp
#include "ComponentEditor.h"

#include <string_view>

namespace {
std::vector<std::unique_ptr<ComponentEditor>> editors; // 登録された順
}


void ComponentEditorRegistry::Register(std::unique_ptr<ComponentEditor> editor) {
	for (const std::unique_ptr<ComponentEditor>& registered : editors) {
		if (std::string_view(registered->GetName()) == editor->GetName()) {
			return;
		}
	}
	editors.push_back(std::move(editor));
}

const std::vector<std::unique_ptr<ComponentEditor>>& ComponentEditorRegistry::GetAll() { return editors; }
```

#### C-3. `Editor/Inspector/TransformEditor.h`（新規）

```cpp
#pragma once
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// TransformComponentのInspector（全Entityが必ず持つので、外せない）
/// </summary>
class TransformEditor : public TypedComponentEditor<TransformComponent> {
public:
	const char* GetName() const override { return "Transform"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Core; }
	bool IsOptional() const override { return false; }

protected:
	TransformComponent* GetComponent(Handle<Entity> handle) const override;
	void DrawComponent(TransformComponent& transform) const override;
};
```

#### C-4. `Editor/Inspector/TransformEditor.cpp`（新規）

中身は今の `InspectorWindow::DrawTransform` と同じ。1か所だけ、**Rotationは動かした軸だけ書き戻す**ように変えた（今は3軸とも度→ラジアンを往復させて書き戻すので、触っていない軸の値もわずかに変わり、Undoの対象に入ってしまう）。

```cpp
#include "TransformEditor.h"

#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Entity/EntityManager.h"

namespace {
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace


TransformComponent* TransformEditor::GetComponent(Handle<Entity> handle) const { return EntityManager::GetTransform(handle); }


void TransformEditor::DrawComponent(TransformComponent& transform) const {
	ImGui::DragFloat3("Position", &transform.translation.x, 0.05f);

	// 回転は中ではラジアンで持っているので、見せるときだけ度に直す。
	// 動かした軸だけ書き戻す（触っていない軸まで度↔ラジアンを往復させると、値がわずかにずれてUndoの対象にも入ってしまう）
	float* radians = &transform.rotation.x;
	const float degrees[3] = {radians[0] * kRadToDeg, radians[1] * kRadToDeg, radians[2] * kRadToDeg};
	float edited[3] = {degrees[0], degrees[1], degrees[2]};
	if (ImGui::DragFloat3("Rotation", edited, 0.5f)) {
		for (int axis = 0; axis < 3; ++axis) {
			if (edited[axis] != degrees[axis]) {
				radians[axis] = edited[axis] * kDegToRad;
			}
		}
	}

	ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform.translation = {0.0f, 0.0f, 0.0f};
		transform.rotation = {0.0f, 0.0f, 0.0f};
		transform.scale = {1.0f, 1.0f, 1.0f};
	}

	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform.worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}
}
```

#### C-5. `Editor/Inspector/ModelRendererEditor.h`（新規）

```cpp
#pragma once
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/ModelRendererComponent.h"


/// <summary>
/// ModelRendererComponentのInspector（どのモデルを描くか・描画設定・マテリアル）
/// </summary>
class ModelRendererEditor : public TypedComponentEditor<ModelRendererComponent> {
public:
	const char* GetName() const override { return "Model Renderer"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Rendering3D; }
	bool IsAddPending(Handle<Entity> handle) const override;
	void RequestRemove(Handle<Entity> handle) const override;

protected:
	ModelRendererComponent* GetComponent(Handle<Entity> handle) const override;
	void RequestAddComponent(Handle<Entity> handle, const ModelRendererComponent& initial) const override;
	void DrawComponent(ModelRendererComponent& render) const override;

private:
	// モデルを選ぶコンボ（resources以下を走査した一覧から選ぶ）
	void DrawModelPicker(ModelRendererComponent& render) const;
};
```

#### C-6. `Editor/Inspector/ModelRendererEditor.cpp`（新規）

中身は今の `DrawModelRenderer`（見出しと `PushID` を除く）と `DrawModelPicker`、モデル一覧の走査の引っ越し。見出しの名前は「Render」から「Model Renderer」（Add Componentと同じ名前）になる。

```cpp
#include "ModelRendererEditor.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"

namespace {
constexpr const char* kModelSearchRoots[] = {"resources", "MyEngine/Resources"}; // モデルを探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr const char* kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};    // モデルとして扱う拡張子

std::vector<std::string> modelFiles; // 見つかったモデルのパス
bool modelFilesScanned = false;      // 1回でも走査したか

// 拡張子がモデルのものか（大文字でも通るように小文字へ直して比べる）
bool IsModelFile(const std::filesystem::path& path) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	for (const char* candidate : kModelExtensions) {
		if (extension == candidate) {
			return true;
		}
	}
	return false;
}

// フォルダを掘って、モデルファイルのパスを集める。毎フレームやるとディスクを叩き続けるので、1回だけ
void ScanModelFiles() {
	modelFiles.clear();
	for (const char* root : kModelSearchRoots) {
		std::error_code error; // 例外ではなくエラーコードで受ける（フォルダが無くても止まらない）
		if (!std::filesystem::exists(root, error)) {
			continue;
		}
		auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsModelFile(entry.path())) {
				modelFiles.push_back(entry.path().generic_string()); // 区切りを / に統一する
			}
		}
	}
	std::sort(modelFiles.begin(), modelFiles.end());
	modelFilesScanned = true;
}
} // namespace


//=============================================================================
// 実体の出し入れ（EntityManagerへ取り次ぐだけ）
//=============================================================================
ModelRendererComponent* ModelRendererEditor::GetComponent(Handle<Entity> handle) const { return EntityManager::GetModelRenderer(handle); }

bool ModelRendererEditor::IsAddPending(Handle<Entity> handle) const { return EntityManager::IsModelRendererAddPending(handle); }

void ModelRendererEditor::RequestAddComponent(Handle<Entity> handle, const ModelRendererComponent& initial) const { EntityManager::RequestAddModelRenderer(handle, initial); }

void ModelRendererEditor::RequestRemove(Handle<Entity> handle) const { EntityManager::RequestRemoveModelRenderer(handle); }


//=============================================================================
// Inspectorの中身
//=============================================================================
void ModelRendererEditor::DrawComponent(ModelRendererComponent& render) const {
	ImGui::Checkbox("Enabled", &render.enabled);

	// --- どのモデルを描くか ---
	DrawModelPicker(render);

	// --- 描画設定（enumはmagic_enumが名前を作る）---
	EditorWidgets::EnumCombo("Shading", render.shadingType);
	EditorWidgets::EnumCombo("Blend", render.blendMode);
	EditorWidgets::EnumCombo("Rasterizer", render.rasterizerType);
	EditorWidgets::EnumCombo("Depth", render.depthMode);
	EditorWidgets::EnumCombo("Billboard", render.billboard);

	// --- 色（0xRRGGBBAAで持っているので、編集のときだけ0〜1のfloatに直す）---
	float rgba[4] = {
	    static_cast<float>((render.color >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((render.color >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((render.color >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(render.color & 0xFF) / 255.0f,
	};
	if (ImGui::ColorEdit4("Color", rgba)) {
		render.color = (static_cast<uint32_t>(rgba[0] * 255.0f + 0.5f) << 24) | (static_cast<uint32_t>(rgba[1] * 255.0f + 0.5f) << 16) |
		               (static_cast<uint32_t>(rgba[2] * 255.0f + 0.5f) << 8) | static_cast<uint32_t>(rgba[3] * 255.0f + 0.5f);
	}

	// --- マテリアルの調整（モデルのmtl / glTFの値を上書きする）---
	if (ImGui::TreeNode("Material")) {
		ImGui::ColorEdit3("Ambient", &render.material.ambient.x);
		ImGui::ColorEdit3("Diffuse", &render.material.diffuse.x);
		ImGui::ColorEdit3("Specular", &render.material.specular.x);
		ImGui::ColorEdit3("Emissive", &render.material.emissive.x);
		ImGui::DragFloat("Shininess", &render.material.shininess, 0.5f, 0.0f, 256.0f);
		ImGui::SliderFloat("Metallic", &render.material.metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &render.material.roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha Cutoff", &render.material.alphaCutoff, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	// --- テクスチャのUVをずらす・回す（Vector3の先頭アドレスを渡してx,yだけ触る）---
	if (ImGui::TreeNode("UV Transform")) {
		ImGui::DragFloat2("Offset", &render.uvTransform.translation.x, 0.01f);
		ImGui::DragFloat2("Tiling", &render.uvTransform.scale.x, 0.01f);
		ImGui::DragFloat("Rotate", &render.uvTransform.rotation.z, 0.01f);
		ImGui::TreePop();
	}
}


//=============================================================================
// モデルを選ぶコンボ
//=============================================================================
void ModelRendererEditor::DrawModelPicker(ModelRendererComponent& render) const {
	// 一覧は最初に開いたときだけ作る
	if (!modelFilesScanned) {
		ScanModelFiles();
	}

	// 今選ばれているモデルの名前（未選択なら (none)）
	const std::string& currentName = ModelManager::GetModelName(render.modelHandle);
	const char* label = currentName.empty() ? "(none)" : currentName.c_str();

	if (ImGui::BeginCombo("Model", label)) {
		for (const std::string& path : modelFiles) {
			if (ImGui::Selectable(path.c_str())) {
				// 同じパスならキャッシュが返るので、選び直しても読み込み直さない
				render.modelHandle = ModelManager::Load(path);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		ScanModelFiles(); // フォルダにモデルを増やしたとき用
	}
	ImGui::TextDisabled("%zu files / handle=%u", modelFiles.size(), render.modelHandle);
}
```

#### C-7. 新しいComponentを作ったときにやること

1. Componentの構造体を作る（データだけ。`std::string` やポインタは持てない）
2. 実体の置き場所を作る（今は `EntityManager` に型ごとに手書き＝段階2。**ゲーム固有のComponentは、段階3で `EntityManager` を型で引ける形にしたら置けるようになる**）
3. `TypedComponentEditor<その型>` を継承したクラスを1つ作る
4. 起動時に1回 `ComponentEditorRegistry::Register(std::make_unique<そのクラス>());`

**Inspector・Add Component・Undo・コピー・削除のUndoのコードは1行も変えない**。これが今回の作り直しの一番の目的。

```cpp
// 例（ゲーム側）：回り続けるComponent。実体の置き場所は段階3の後で用意する
struct SpinnerComponent {
	float speed = 1.0f; // 1秒に何ラジアン回るか
};

class SpinnerEditor : public TypedComponentEditor<SpinnerComponent> {
public:
	const char* GetName() const override { return "Spinner"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Effects; }

protected:
	SpinnerComponent* GetComponent(Handle<Entity> handle) const override { /* 実体を持っている所から取る */ }
	void RequestAddComponent(Handle<Entity> handle, const SpinnerComponent& initial) const override { /* 追加を予約 */ }
	void DrawComponent(SpinnerComponent& spinner) const override { ImGui::DragFloat("Speed", &spinner.speed, 0.01f); }
};
```

---

### D. EditorHistory（Undoの本体を作り直す）

仕組みは「[Undoの仕組み（解説）](#undoの仕組み解説)」に書いた。ここはコードだけ。

#### D-1. `Editor/History/EditorHistory.h`（全体を差し替え）

`.h` は「外から呼ぶ関数の一覧」だけにした。中で使う型（履歴1件・Entityの写し・編集中のComponent）は全部 `.cpp` の無名namespaceに隠す。

```cpp
#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"

class ComponentEditor;


/// <summary>
/// エディタの操作の履歴（Undo・Redo）と、コピー・貼り付け
/// <para>操作はRequestで予約だけして、次のフレームの最初（Flush）に順番どおり実行する（UIを描いている途中でEntityを増減させないため）</para>
/// <para>相手はHandleではなくEntityIdで覚える（消して戻すとHandleは変わるが、EntityIdは変わらない）</para>
/// <para>仕組みの解説は Docs/Tasks/Editor.md の「Undoの仕組み」</para>
/// </summary>
class EditorHistory {
public:
	// ===== 毎フレーム =====
	// フレームの最初に1回呼ぶ。予約された操作を、頼まれた順に実行する
	static void Flush();
	// シーンの切り替え・終了で呼ぶ。履歴を捨てる（コピーした物は残すので、Stopの後にも貼れる）
	static void Clear();

	// ===== Undo / Redo =====
	static bool CanUndo();
	static bool CanRedo();
	static void RequestUndo();
	static void RequestRedo();

	// ===== Entityの操作 =====
	static void RequestCreate(const std::string& name, Handle<Entity> parent); // parentが無効ならroot
	static void RequestDestroy(Handle<Entity> handle);                         // 子も一緒に消える
	static void RequestRename(Handle<Entity> handle, const std::string& name); // 空の名前にはしない
	static void RequestSetActive(Handle<Entity> handle, bool isActive);
	static void RequestReparent(Handle<Entity> child, Handle<Entity> parent); // parentが無効ならrootへ

	// ===== コピー・貼り付け =====
	static bool CanPaste();
	static void Copy(Handle<Entity> handle);             // その場で写す（何も変えないので予約しない）
	static void RequestPaste(Handle<Entity> parent);     // parentの子として貼る（無効ならroot）
	static void RequestDuplicate(Handle<Entity> handle); // 同じ親の下に複製する

	// ===== Component =====
	static void RequestAddComponent(Handle<Entity> handle, const ComponentEditor& editor);
	static void RequestRemoveComponent(Handle<Entity> handle, const ComponentEditor& editor);
	// Inspectorが描く前と描いた後の中身を渡す。変わったバイトだけを覚えておく
	static void RecordComponentChange(Handle<Entity> handle, const ComponentEditor& editor, const void* before, const void* after);
	// 操作の区切り（マウスを離した・入力を確定した）で呼ぶ。ここまでの変更を1件の履歴にまとめる
	static void CommitComponentChanges();
};
```

#### D-2. `Editor/History/EditorHistory.cpp`（新規）

```cpp
#include "EditorHistory.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
//=============================================================================
// 履歴
//=============================================================================
// 履歴1件＝「戻す処理」と「やり直す処理」の組。対象が見つからないなどで失敗したらfalseを返す
struct Entry {
	std::function<bool()> undo;
	std::function<bool()> redo;
};

constexpr size_t kMaxHistory = 128; // 覚えておく操作の数（超えたら古いものから捨てる）

std::vector<Entry> history;                  // 古い順に並ぶ
size_t cursor = 0;                           // 適用済みの件数。[0, cursor) がUndoでき、[cursor, size) がRedoできる
std::vector<std::function<void()>> requests; // 次のFlushで実行する操作（頼まれた順）


//=============================================================================
// Entityの写し（削除のUndo・コピー・貼り付け用）
//=============================================================================
struct ComponentData {
	const ComponentEditor* editor = nullptr; // どの種類のComponentか
	std::vector<std::byte> bytes;            // 中身をそのまま写したもの
};
struct EntityData {
	EntityId id = 0;
	EntityId parent = 0; // 0ならroot
	std::string name;
	bool isActive = true;
	std::vector<ComponentData> components;
};
using EntityTree = std::vector<EntityData>; // [0]が根。親は必ず子より前に並ぶ

EntityTree clipboard; // コピーした物（空なら何も無い）


//=============================================================================
// Inspectorで編集中のComponent
//=============================================================================
// マウスを離すまでの変更を、Component1つにつき1個にまとめて持つ
struct ComponentEdit {
	EntityId entity = 0;
	const ComponentEditor* editor = nullptr;
	std::vector<std::byte> before; // 編集前の値（changedの所だけ意味がある）
	std::vector<std::byte> after;  // 編集後の値（同上）
	std::vector<bool> changed;     // Inspectorで書き換えたバイトか
};
std::vector<ComponentEdit> pendingEdits;


//=============================================================================
// 履歴の出し入れ
//=============================================================================
EntityId IdOf(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	return entity ? entity->id : 0;
}

// もう適用してある操作を、履歴の末尾に足す（Redoできる分は捨てる）
void Append(Entry entry) {
	history.erase(history.begin() + static_cast<std::ptrdiff_t>(cursor), history.end());
	history.push_back(std::move(entry));
	if (history.size() > kMaxHistory) {
		history.erase(history.begin());
	}
	cursor = history.size();
}

// まだ適用していない操作を、実行してから履歴に足す（実行できなければ何もしない）
void Execute(Entry entry) {
	if (entry.redo()) {
		Append(std::move(entry));
	}
}

// Undo・Redoが失敗した＝履歴と今のシーンが食い違った。続けると関係ない物を書き換えかねないので、履歴を捨てる
void Failed() {
	history.clear();
	cursor = 0;
	LogManager::Warning("Undo/Redoの対象が見つからなかったので、履歴を消しました");
}


//=============================================================================
// Entityの写しを作る・戻す・消す
//=============================================================================
// Entity1つ分。登録されている全種類のComponentを見て、持っている物を写す
EntityData CaptureOne(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	EntityData data;
	data.id = entity->id;
	data.parent = IdOf(entity->parent);
	data.name = entity->name;
	data.isActive = entity->isActive;
	for (const std::unique_ptr<ComponentEditor>& editor : ComponentEditorRegistry::GetAll()) {
		if (const void* component = editor->Get(handle)) {
			const auto* bytes = static_cast<const std::byte*>(component);
			data.components.push_back({editor.get(), std::vector<std::byte>(bytes, bytes + editor->GetSize())});
		}
	}
	return data;
}

// rootと、その子孫をまとめて写す（幅優先なので、親が必ず子より前に並ぶ）
EntityTree Capture(Handle<Entity> root) {
	EntityTree tree;
	if (!EntityManager::IsAlive(root)) {
		return tree;
	}
	std::vector<Handle<Entity>> queue = {root};
	std::vector<Handle<Entity>> children;
	for (size_t i = 0; i < queue.size(); ++i) {
		tree.push_back(CaptureOne(queue[i]));
		EntityManager::GetChildren(queue[i], children);
		queue.insert(queue.end(), children.begin(), children.end());
	}
	return tree;
}

// 写しからEntityを作り直す。同じEntityIdで作るので、ほかの履歴からも同じ相手として見つかる
bool Restore(const EntityTree& tree) {
	if (tree.empty()) {
		return false;
	}
	const EntityData& root = tree.front();
	if (root.parent != 0 && !EntityManager::FindById(root.parent).IsValid()) {
		return false; // 戻す先の親がもう無い
	}
	for (const EntityData& data : tree) {
		if (EntityManager::FindById(data.id).IsValid()) {
			return false; // すでに居る（二重に戻そうとしている）
		}
	}

	for (const EntityData& data : tree) {
		Handle<Entity> handle = EntityManager::CreateWithId(data.id, data.name, EntityManager::FindById(data.parent));
		EntityManager::Get(handle)->isActive = data.isActive;
		for (const ComponentData& component : data.components) {
			if (void* existing = component.editor->Get(handle)) {
				std::memcpy(existing, component.bytes.data(), component.bytes.size()); // Transformのように、作った時点で持っている物は上書き
			} else {
				component.editor->RequestAdd(handle, component.bytes.data());
			}
		}
	}
	EntityManager::FlushComponentChanges(); // 予約した追加をすぐ反映する（Flushの中＝フレームの境目なので安全）
	HierarchyWindow::SetSelected(EntityManager::FindById(root.id));
	return true;
}

// 自分と子孫の数
size_t CountTree(Handle<Entity> handle) {
	size_t count = 1;
	std::vector<Handle<Entity>> children;
	EntityManager::GetChildren(handle, children);
	for (Handle<Entity> child : children) {
		count += CountTree(child);
	}
	return count;
}

// 写しのEntityを消す（作成・貼り付けのUndo、削除のRedo）
bool Remove(const EntityTree& tree) {
	if (tree.empty()) {
		return false;
	}
	const Handle<Entity> root = EntityManager::FindById(tree.front().id);
	// 写した後でゲームが子を足していたら、その子まで消してしまうので戻さない
	if (!root.IsValid() || CountTree(root) != tree.size()) {
		return false;
	}
	EntityManager::Destroy(root);
	EntityManager::FlushDestroy(); // Flushの中＝フレームの境目なので、すぐ消してよい
	return true;
}

// Entityを増やす操作（作成・貼り付け）を実行して、履歴に積む
void AddTree(EntityTree tree) {
	Execute({[tree] { return Remove(tree); }, [tree] { return Restore(tree); }});
}


//=============================================================================
// 貼り付け
//=============================================================================
// 同じ親の下に、その名前のEntityがいるか
bool HasSibling(EntityId parent, const std::string& name) {
	const Handle<Entity> parentHandle = EntityManager::FindById(parent); // 0なら無効なHandle＝root
	for (const Entity& entity : EntityManager::GetAll()) {
		if (entity.parent == parentHandle && entity.name == name) {
			return true;
		}
	}
	return false;
}

// 末尾の " (Copy3)" を外す。コピーのコピーが "Cube (Copy1) (Copy1)" のように伸びないように
std::string RemoveCopySuffix(const std::string& name) {
	const std::string mark = " (Copy";
	const size_t position = name.rfind(mark);
	if (position == std::string::npos || name.back() != ')') {
		return name;
	}
	const size_t digitsBegin = position + mark.size();
	const size_t digitsEnd = name.size() - 1; // 最後の ')' の位置
	if (digitsBegin >= digitsEnd) {
		return name; // "Cube (Copy)" のように番号が無い
	}
	for (size_t i = digitsBegin; i < digitsEnd; ++i) {
		if (name[i] < '0' || name[i] > '9') {
			return name;
		}
	}
	return name.substr(0, position);
}

// 貼り付けた物の名前。同じ親の下で空いている一番小さい番号を付ける："Cube (Copy1)" "Cube (Copy2)" …
std::string MakeCopyName(const std::string& name, EntityId parent) {
	const std::string base = RemoveCopySuffix(name);
	for (int number = 1;; ++number) {
		std::string candidate = base + " (Copy" + std::to_string(number) + ")";
		if (!HasSibling(parent, candidate)) {
			return candidate;
		}
	}
}

// 写しに新しい番号を振り直して、parentの子として作る
void Paste(EntityTree tree, EntityId parent) {
	if (tree.empty() || (parent != 0 && !EntityManager::FindById(parent).IsValid())) {
		return;
	}
	// 番号を振り直す。子が覚えている親の番号も、古い番号→新しい番号の表で付け替える
	std::unordered_map<EntityId, EntityId> newIds;
	for (EntityData& data : tree) {
		const EntityId newId = EntityManager::NewId();
		newIds[data.id] = newId;
		data.id = newId;
	}
	for (size_t i = 1; i < tree.size(); ++i) {
		tree[i].parent = newIds.at(tree[i].parent);
	}
	tree.front().parent = parent;
	tree.front().name = MakeCopyName(tree.front().name, parent);
	AddTree(std::move(tree));
}


//=============================================================================
// Entityの項目・Component
//=============================================================================
// Entityの項目（名前・有効など）を書き換える操作。memberは「Entityのどの項目か」（メンバへのポインタ）
template<class T> void RequestSetEntityValue(Handle<Entity> handle, T Entity::* member, T value) {
	requests.push_back([id = IdOf(handle), member, value] {
		const Entity* entity = EntityManager::Get(EntityManager::FindById(id));
		if (!entity || entity->*member == value) {
			return;
		}
		const T oldValue = entity->*member;
		auto set = [id, member](const T& newValue) {
			Entity* target = EntityManager::Get(EntityManager::FindById(id));
			if (!target) {
				return false;
			}
			target->*member = newValue;
			return true;
		};
		Execute({[set, oldValue] { return set(oldValue); }, [set, value] { return set(value); }});
	});
}

// Componentを今すぐ足す・外す（Flushの中からだけ呼ぶ）
bool AddComponentNow(EntityId id, const ComponentEditor& editor, const void* initial) {
	const Handle<Entity> handle = EntityManager::FindById(id);
	if (!handle.IsValid() || editor.Get(handle)) {
		return false;
	}
	editor.RequestAdd(handle, initial);
	EntityManager::FlushComponentChanges();
	return editor.Get(handle) != nullptr;
}

bool RemoveComponentNow(EntityId id, const ComponentEditor& editor) {
	const Handle<Entity> handle = EntityManager::FindById(id);
	if (!editor.Get(handle)) {
		return false;
	}
	editor.RequestRemove(handle);
	EntityManager::FlushComponentChanges();
	return editor.Get(handle) == nullptr;
}

// 覚えたバイトだけを書き戻す（undoならbefore、redoならafter）
bool ApplyEdits(const std::vector<ComponentEdit>& edits, bool undo) {
	// 先に全部そろっているか調べる（途中で失敗して一部だけ戻るのを防ぐ）
	for (const ComponentEdit& edit : edits) {
		if (!edit.editor->Get(EntityManager::FindById(edit.entity))) {
			return false;
		}
	}
	for (const ComponentEdit& edit : edits) {
		auto* bytes = static_cast<std::byte*>(edit.editor->Get(EntityManager::FindById(edit.entity)));
		const std::vector<std::byte>& source = undo ? edit.before : edit.after;
		for (size_t i = 0; i < source.size(); ++i) {
			if (edit.changed[i]) {
				bytes[i] = source[i];
			}
		}
	}
	return true;
}
} // namespace


//=============================================================================
// 毎フレーム
//=============================================================================
void EditorHistory::Flush() {
	// 実行中に新しい予約が足されても壊れないように、取り出してから回す
	std::vector<std::function<void()>> current = std::move(requests);
	requests.clear();
	for (const std::function<void()>& request : current) {
		request();
	}
}

void EditorHistory::Clear() {
	history.clear();
	cursor = 0;
	requests.clear();
	pendingEdits.clear();
}


//=============================================================================
// Undo / Redo
//=============================================================================
bool EditorHistory::CanUndo() { return cursor > 0; }
bool EditorHistory::CanRedo() { return cursor < history.size(); }

void EditorHistory::RequestUndo() {
	CommitComponentChanges(); // 編集中の物があれば、先に履歴へ入れてから戻す
	requests.push_back([] {
		if (cursor == 0) {
			return;
		}
		// Failedでhistoryを消しても壊れないように、呼ぶ前に関数をコピーしておく
		const std::function<bool()> undo = history[cursor - 1].undo;
		if (undo()) {
			--cursor;
		} else {
			Failed();
		}
	});
}

void EditorHistory::RequestRedo() {
	CommitComponentChanges();
	requests.push_back([] {
		if (cursor >= history.size()) {
			return;
		}
		const std::function<bool()> redo = history[cursor].redo;
		if (redo()) {
			++cursor;
		} else {
			Failed();
		}
	});
}


//=============================================================================
// Entityの操作
//=============================================================================
void EditorHistory::RequestCreate(const std::string& name, Handle<Entity> parent) {
	if (parent.IsValid() && !EntityManager::IsAlive(parent)) {
		return;
	}
	requests.push_back([name, parentId = IdOf(parent)] {
		EntityData data;
		data.id = EntityManager::NewId();
		data.parent = parentId;
		data.name = name;
		AddTree({data}); // Componentは空＝作った時点のTransform（初期値）のまま
	});
}

void EditorHistory::RequestDestroy(Handle<Entity> handle) {
	requests.push_back([id = IdOf(handle)] {
		const EntityTree tree = Capture(EntityManager::FindById(id));
		if (tree.empty()) {
			return;
		}
		Execute({[tree] { return Restore(tree); }, [tree] { return Remove(tree); }});
	});
}

void EditorHistory::RequestRename(Handle<Entity> handle, const std::string& name) {
	if (name.empty()) {
		return; // 空の名前にはしない（元の名前のまま）
	}
	RequestSetEntityValue(handle, &Entity::name, name);
}

void EditorHistory::RequestSetActive(Handle<Entity> handle, bool isActive) { RequestSetEntityValue(handle, &Entity::isActive, isActive); }

void EditorHistory::RequestReparent(Handle<Entity> child, Handle<Entity> parent) {
	if (parent.IsValid() && !EntityManager::IsAlive(parent)) {
		return;
	}
	requests.push_back([childId = IdOf(child), parentId = IdOf(parent)] {
		const Entity* entity = EntityManager::Get(EntityManager::FindById(childId));
		if (!entity) {
			return;
		}
		const EntityId oldParentId = IdOf(entity->parent);
		if (oldParentId == parentId) {
			return;
		}
		auto set = [childId](EntityId newParentId) {
			const Handle<Entity> childHandle = EntityManager::FindById(childId);
			const Handle<Entity> parentHandle = EntityManager::FindById(newParentId);
			if (!childHandle.IsValid() || (newParentId != 0 && !parentHandle.IsValid())) {
				return false;
			}
			EntityManager::SetParent(childHandle, parentHandle);
			return EntityManager::Get(childHandle)->parent == parentHandle; // 輪になる付け替えはSetParentが断る
		};
		Execute({[set, oldParentId] { return set(oldParentId); }, [set, parentId] { return set(parentId); }});
	});
}


//=============================================================================
// コピー・貼り付け
//=============================================================================
bool EditorHistory::CanPaste() { return !clipboard.empty(); }

void EditorHistory::Copy(Handle<Entity> handle) {
	EntityTree tree = Capture(handle);
	if (!tree.empty()) {
		clipboard = std::move(tree);
	}
}

void EditorHistory::RequestPaste(Handle<Entity> parent) {
	if (parent.IsValid() && !EntityManager::IsAlive(parent)) {
		return;
	}
	requests.push_back([parentId = IdOf(parent)] { Paste(clipboard, parentId); });
}

void EditorHistory::RequestDuplicate(Handle<Entity> handle) {
	requests.push_back([id = IdOf(handle)] {
		const Entity* entity = EntityManager::Get(EntityManager::FindById(id));
		if (!entity) {
			return;
		}
		// Pasteの中でEntityが増えるとentityのポインタが使えなくなるので、先に値を取り出しておく
		EntityTree tree = Capture(entity->self);
		const EntityId parent = IdOf(entity->parent);
		Paste(std::move(tree), parent);
	});
}


//=============================================================================
// Component
//=============================================================================
void EditorHistory::RequestAddComponent(Handle<Entity> handle, const ComponentEditor& editor) {
	CommitComponentChanges();
	requests.push_back([id = IdOf(handle), editor = &editor] {
		Execute({[id, editor] { return RemoveComponentNow(id, *editor); }, [id, editor] { return AddComponentNow(id, *editor, nullptr); }});
	});
}

void EditorHistory::RequestRemoveComponent(Handle<Entity> handle, const ComponentEditor& editor) {
	CommitComponentChanges();
	requests.push_back([id = IdOf(handle), editor = &editor] {
		const void* component = editor->Get(EntityManager::FindById(id));
		if (!component) {
			return;
		}
		// 外す前の中身を写しておき、Undoではその値のまま戻す
		const auto* bytes = static_cast<const std::byte*>(component);
		const std::vector<std::byte> saved(bytes, bytes + editor->GetSize());
		Execute({[id, editor, saved] { return AddComponentNow(id, *editor, saved.data()); }, [id, editor] { return RemoveComponentNow(id, *editor); }});
	});
}

void EditorHistory::RecordComponentChange(Handle<Entity> handle, const ComponentEditor& editor, const void* before, const void* after) {
	const size_t size = editor.GetSize();
	if (std::memcmp(before, after, size) == 0) {
		return; // 何も変わっていない
	}
	const EntityId id = IdOf(handle);

	// このEntityのこのComponentを編集中なら、その続きとして足す。無ければ新しく始める
	auto edit = std::find_if(pendingEdits.begin(), pendingEdits.end(), [&](const ComponentEdit& e) { return e.entity == id && e.editor == &editor; });
	if (edit == pendingEdits.end()) {
		pendingEdits.push_back({id, &editor, std::vector<std::byte>(size), std::vector<std::byte>(size), std::vector<bool>(size, false)});
		edit = pendingEdits.end() - 1;
	}

	const auto* oldBytes = static_cast<const std::byte*>(before);
	const auto* newBytes = static_cast<const std::byte*>(after);
	for (size_t i = 0; i < size; ++i) {
		if (oldBytes[i] == newBytes[i]) {
			continue;
		}
		// 4バイト（float1個分）ごとに印を付ける。floatの一部のバイトだけ戻すと、でたらめな値になるため
		const size_t wordBegin = i / 4 * 4;
		const size_t wordEnd = (std::min)(wordBegin + 4, size);
		for (size_t j = wordBegin; j < wordEnd; ++j) {
			if (!edit->changed[j]) {
				edit->changed[j] = true;
				edit->before[j] = oldBytes[j]; // 最初に変わったときの値が「編集前」
			}
		}
	}
	for (size_t j = 0; j < size; ++j) {
		if (edit->changed[j]) {
			edit->after[j] = newBytes[j]; // 「編集後」は毎回最新にする
		}
	}
}

void EditorHistory::CommitComponentChanges() {
	if (pendingEdits.empty()) {
		return;
	}
	std::vector<ComponentEdit> edits = std::move(pendingEdits);
	pendingEdits.clear();
	// ドラッグして元の値に戻しただけなら履歴にしない（Undoしても何も変わらない1件ができてしまう）
	std::erase_if(edits, [](const ComponentEdit& edit) {
		for (size_t i = 0; i < edit.changed.size(); ++i) {
			if (edit.changed[i] && edit.before[i] != edit.after[i]) {
				return false;
			}
		}
		return true;
	});
	if (edits.empty()) {
		return;
	}
	// 履歴に入れるのもFlushの中で行う（同じフレームに頼まれたUndoより先に入るように、順番をそろえる）
	requests.push_back([edits] { Append({[edits] { return ApplyEdits(edits, true); }, [edits] { return ApplyEdits(edits, false); }}); });
}
```

---

### E. InspectorWindow

Componentの区画を `ComponentEditorRegistry` の順に並べるだけになる。**Componentが増えてもこのファイルは変えない**。

#### E-1. `Editor/Windows/InspectorWindow.h`（全体を差し替え）

```cpp
#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"

class ComponentEditor;


/// <summary>
/// インスペクターウィンドウ（HierarchyWindowで選んでいるEntityの中身を編集する）
/// <para>Componentごとの中身は ComponentEditor が描く。ここは並べるだけなので、Componentが増えてもこのクラスは変えない</para>
/// </summary>
class InspectorWindow {
public:
	/// <summary>
	/// エンジンのComponentEditorを登録する（ImGuiManager::Initializeから1回だけ）
	/// </summary>
	static void Initialize();

	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ（HierarchyWindow::Drawより後）
	/// </summary>
	static void Draw();


private:
	// Entityそのものの情報（有効無効・名前・番号）
	static void DrawHeader(Handle<Entity> handle);
	// Component1つ分の区画（持っていなければ何も出さない）
	static void DrawComponent(Handle<Entity> handle, const ComponentEditor& editor);
	// Componentを足すボタン（カテゴリごとに縦に並べたポップアップ）
	static void DrawAddComponent(Handle<Entity> handle);
};
```

#### E-2. `Editor/Windows/InspectorWindow.cpp`（全体を差し替え）

```cpp
#include "InspectorWindow.h"

#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Inspector/ModelRendererEditor.h"
#include "MyEngine/Editor/Inspector/TransformEditor.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）

// Add Componentに出すか（外せる種類で、カテゴリが合っていて、検索にも合う）
bool IsListed(const ComponentEditor& editor, ComponentCategory category, const ImGuiTextFilter& filter) {
	return editor.IsOptional() && editor.GetCategory() == category && filter.PassFilter(editor.GetName());
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void InspectorWindow::Initialize() {
	// エンジンのComponent。登録した順がInspectorの区画の順になる
	ComponentEditorRegistry::Register(std::make_unique<TransformEditor>());
	ComponentEditorRegistry::Register(std::make_unique<ModelRendererEditor>());
}


//=============================================================================
// 描画
//=============================================================================
void InspectorWindow::Draw() {
	if (ImGui::Begin("Inspector")) {
		const Handle<Entity> handle = HierarchyWindow::GetSelected();
		if (EntityManager::IsAlive(handle)) {
			DrawHeader(handle);
			for (const std::unique_ptr<ComponentEditor>& editor : ComponentEditorRegistry::GetAll()) {
				DrawComponent(handle, *editor);
			}
			DrawAddComponent(handle);
		} else {
			ImGui::TextDisabled("No entity selected");
		}
	}
	ImGui::End();

	// どの項目も操作していない（マウスを離した・入力を確定した）なら、ここまでの編集を1件の履歴にまとめる
	if (!ImGui::IsAnyItemActive()) {
		EditorHistory::CommitComponentChanges();
	}
}


//=============================================================================
// Entityそのものの情報
//=============================================================================
void InspectorWindow::DrawHeader(Handle<Entity> handle) {
	// ポインタは使い捨てにする（ARCHITECTURE.md 原則1）。持ち越すのはHandleだけ
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// --- 有効・無効（直接書き換えず、履歴を通す。反映は次のフレームの頭）---
	bool isActive = entity->isActive;
	if (ImGui::Checkbox("##isActive", &isActive)) {
		EditorHistory::RequestSetActive(handle, isActive);
	}
	ImGui::SameLine();

	// --- 名前（変更はHierarchyのダブルクリック・F2から）---
	ImGui::TextUnformatted(entity->name.c_str());

	// --- 参考情報（読み取り専用）---
	ImGui::TextDisabled("ID: %llu  Handle: %u / %u", entity->id, handle.index, handle.generation);
	if (const Entity* parent = EntityManager::Get(entity->parent)) {
		ImGui::TextDisabled("Parent: %s", parent->name.c_str());
	} else {
		ImGui::TextDisabled("Parent: (root)");
	}
	ImGui::Separator();
}


//=============================================================================
// Component1つ分の区画
//=============================================================================
void InspectorWindow::DrawComponent(Handle<Entity> handle, const ComponentEditor& editor) {
	void* component = editor.Get(handle);
	if (!component) {
		return; // このEntityは持っていない
	}

	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（"Position" などの項目名がComponent同士でぶつからないように）
	ImGui::PushID(editor.GetName());
	const bool isOpen = ImGui::CollapsingHeader(editor.GetName(), ImGuiTreeNodeFlags_DefaultOpen);

	// --- 見出しの右クリック ---
	if (ImGui::BeginPopupContextItem("componentMenu")) {
		if (ImGui::MenuItem("Remove Component", nullptr, false, editor.IsOptional())) {
			EditorHistory::RequestRemoveComponent(handle, editor);
		}
		ImGui::EndPopup();
	}

	if (isOpen) {
		// 描く前の中身を写しておき、描いた後と比べる。変わった所だけがUndoの対象になる
		// （Componentごとの項目を1つずつ書き並べなくてよいので、Componentが増えてもここは変えない）
		const auto* bytes = static_cast<const std::byte*>(component);
		const std::vector<std::byte> before(bytes, bytes + editor.GetSize());
		editor.Draw(component);
		EditorHistory::RecordComponentChange(handle, editor, before.data(), component);
	}
	ImGui::PopID();
}


//=============================================================================
// Componentを足すボタン
//=============================================================================
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	ImGui::Spacing();

	// Componentの見出し（CollapsingHeader）と同じ色の、横幅いっぱいのボタンにする
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
	const bool clicked = EditorWidgets::PlusButton("Add Component", ImVec2(-FLT_MIN, 0.0f));
	ImGui::PopStyleColor(3);
	if (clicked) {
		ImGui::OpenPopup("addComponent");
	}

	// --- ボタンの真下に、ボタンと同じ幅の縦長のポップアップを出す ---
	const ImVec2 buttonMin = ImGui::GetItemRectMin();
	const ImVec2 buttonMax = ImGui::GetItemRectMax();
	const float width = buttonMax.x - buttonMin.x;
	ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y));
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, kAddComponentMaxHeight));
	if (!ImGui::BeginPopup("addComponent")) {
		return;
	}

	// --- 検索欄（開いた瞬間に文字を打てるようにする）---
	static ImGuiTextFilter filter; // ポップアップは同時に1つしか開かないので、関数の中のstaticで持つ
	if (ImGui::IsWindowAppearing()) {
		filter.Clear();
		ImGui::SetKeyboardFocusHere();
	}
	filter.Draw("##search", -FLT_MIN);
	ImGui::Separator();

	// --- カテゴリ（押すと下に開く）→ その中のComponent ---
	const std::vector<std::unique_ptr<ComponentEditor>>& editors = ComponentEditorRegistry::GetAll();
	for (ComponentCategory category : magic_enum::enum_values<ComponentCategory>()) {
		// このカテゴリに出す物が1つも無ければ、カテゴリごと出さない
		bool hasItem = false;
		for (const std::unique_ptr<ComponentEditor>& editor : editors) {
			hasItem |= IsListed(*editor, category, filter);
		}
		if (!hasItem) {
			continue;
		}

		// 検索中は、見つかったカテゴリを全部開いて見せる
		if (filter.IsActive()) {
			ImGui::SetNextItemOpen(true);
		}
		const std::string categoryName(magic_enum::enum_name(category));
		if (!ImGui::TreeNodeEx(categoryName.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			continue;
		}
		for (const std::unique_ptr<ComponentEditor>& editor : editors) {
			if (!IsListed(*editor, category, filter)) {
				continue;
			}
			// すでに持っている・追加を予約済みなら、灰色にして押せなくする
			ImGui::BeginDisabled(editor->Get(handle) || editor->IsAddPending(handle));
			if (ImGui::Selectable(editor->GetName())) { // Selectableを押すとポップアップは自動で閉じる
				EditorHistory::RequestAddComponent(handle, *editor);
			}
			ImGui::EndDisabled();
		}
		ImGui::TreePop();
	}

	ImGui::EndPopup();
}
```

- `DrawComponent` の「描く前に写す → `Draw` → 描いた後と比べる」の3行が、`TrackInspectorValues` の代わり
- `Draw` の最後の `if (!ImGui::IsAnyItemActive())` が「マウスを離したら1件にまとめる」。Codexの版にあった「カラーピッカーのポップアップが開いている間は1件」は無くし、**ピッカーの中のドラッグ1回ごとに1件**にした（こちらの方がUndoで少しずつ戻せる）
- 有効・無効のチェックは、直接書き換えずに `RequestSetActive` を通す（名前と同じ扱い）

---

### F. HierarchyWindow

#### F-1. `Editor/Windows/HierarchyWindow.h`（全体を差し替え）

```cpp
#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// ヒエラルキーウィンドウ（Entityの一覧・作成・削除・選択・親子の付け替え・名前の変更・コピー）
/// <para>選択中のEntityはここが持つ。InspectorWindowはこれを見る</para>
/// <para>作成・削除などは全部EditorHistoryに予約するだけ（Undoでき、描いている途中で一覧も変わらない）</para>
/// </summary>
class HierarchyWindow {
public:
	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ
	/// </summary>
	static void Draw();

	// 選択中のEntity（何も選んでいなければ無効なHandle）
	static Handle<Entity> GetSelected() { return selected_; }
	static void SetSelected(Handle<Entity> handle) { selected_ = handle; }


private:
	// 見出し（Entityの数と、作成メニューを開く＋ボタン）
	static void DrawToolbar();
	// Entity1つ分を描く（子がいれば入れ子で描く）
	static void DrawEntityNode(Handle<Entity> handle);
	// 一覧の下の何も無い所（クリックで選択解除・rootへのドロップ・右クリックで作成メニュー）
	static void DrawEmptySpace();
	// Entityの右クリックメニュー
	static void DrawEntityMenu(Handle<Entity> handle);
	// 作成メニュー（＋ボタンと、何も無い所の右クリックで共通）
	static void DrawCreateMenu();
	// F2・Delete・Ctrl+C / V / D
	static void HandleShortcuts();
	// 名前の変更を始める（そのEntityの行を入力欄にする）
	static void StartRename(Handle<Entity> handle);
	// 名前の入力欄（DrawEntityNodeの中、ツリーの矢印の横に出す）
	static void DrawRenameField(Handle<Entity> handle);

	static Handle<Entity> selected_;  // 選択中
	static Handle<Entity> renaming_;  // 名前を変更中のEntity（無効なら変更中ではない）
	static std::string renameBuffer_; // 入力中の名前（確定するまでEntityの名前は変えない）
	static bool renameFocusRequest_;  // 入力欄を出した最初のフレームだけtrue（フォーカスを移す）
};
```

#### F-2. `Editor/Windows/HierarchyWindow.cpp`（全体を差し替え）

```cpp
#include "HierarchyWindow.h"

#include <algorithm>
#include <cfloat>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::renaming_;
std::string HierarchyWindow::renameBuffer_;
bool HierarchyWindow::renameFocusRequest_ = false;

namespace {
constexpr const char* kDragDropType = "ENTITY_HANDLE"; // ドラッグで運ぶものの種類の名前

// 親のHandle（いなければ無効なHandle＝root）
Handle<Entity> ParentOf(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	return entity ? entity->parent : Handle<Entity>{};
}

// 子がいるか（いなければ矢印を出さない）
bool HasChild(Handle<Entity> handle) {
	for (const Entity& entity : EntityManager::GetAll()) {
		if (entity.parent == handle) {
			return true;
		}
	}
	return false;
}

// ドロップされたEntityを受け取る。newParentの子にする（無効ならrootへ）
void AcceptEntityDrop(Handle<Entity> newParent) {
	if (!ImGui::BeginDragDropTarget()) {
		return;
	}
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropType)) {
		EditorHistory::RequestReparent(*static_cast<const Handle<Entity>*>(payload->Data), newParent);
	}
	ImGui::EndDragDropTarget();
}
} // namespace


//=============================================================================
// 描画
//=============================================================================
void HierarchyWindow::Draw() {
	// Undoなどで消えたEntityを持ち越さない
	if (!EntityManager::IsAlive(selected_)) {
		selected_ = {};
	}
	if (!EntityManager::IsAlive(renaming_)) {
		renaming_ = {};
		renameFocusRequest_ = false;
	}

	if (ImGui::Begin("Hierarchy")) {
		DrawToolbar();
		ImGui::Separator();

		// --- 一覧（rootから入れ子で描く）---
		// 作成・削除などはEditorHistoryに予約するだけなので、描いている途中で一覧が変わることはない
		for (const Entity& entity : EntityManager::GetAll()) {
			if (!entity.parent.IsValid()) {
				DrawEntityNode(entity.self);
			}
		}
		DrawEmptySpace();
		HandleShortcuts();
	}
	ImGui::End();
}


//=============================================================================
// 見出し
//=============================================================================
void HierarchyWindow::DrawToolbar() {
	const float buttonSize = ImGui::GetFrameHeight();
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端（ウィンドウの中での位置）

	ImGui::AlignTextToFramePadding(); // 文字の高さをボタンに合わせる
	ImGui::Text("Entities (%zu)", EntityManager::GetCount());

	// --- 右端に＋ボタン。押すと作成メニュー ---
	ImGui::SameLine(right - buttonSize);
	if (EditorWidgets::PlusButton("##create", ImVec2(buttonSize, buttonSize))) {
		ImGui::OpenPopup("createMenu");
	}
	ImGui::SetItemTooltip("Create");
	if (ImGui::BeginPopup("createMenu")) {
		DrawCreateMenu();
		ImGui::EndPopup();
	}
}


//=============================================================================
// Entity1つ分
//=============================================================================
void HierarchyWindow::DrawEntityNode(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	const bool isRenaming = (handle == renaming_);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!isRenaming) {
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth; // 名前の変更中は、矢印の横に入力欄を置くので横幅いっぱいにしない
	}
	if (!HasChild(handle)) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (handle == selected_) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// Handleの中身をIDにする（名前が同じEntityがあってもぶつからない）
	ImGui::PushID(static_cast<int>(handle.index));
	const bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", isRenaming ? "" : entity->name.c_str());

	if (isRenaming) {
		ImGui::SameLine();
		DrawRenameField(handle);
	} else {
		// --- クリックで選択（右クリックでも選ぶ）、ダブルクリックで名前の変更 ---
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right);
		if (clicked && !ImGui::IsItemToggledOpen()) {
			selected_ = handle;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			StartRename(handle);
		}

		// --- ドラッグ（このEntityを運ぶ）・ドロップ（このEntityを親にする）---
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(kDragDropType, &handle, sizeof(handle));
			ImGui::TextUnformatted(entity->name.c_str());
			ImGui::EndDragDropSource();
		}
		AcceptEntityDrop(handle);

		// --- 右クリックのメニュー ---
		if (ImGui::BeginPopupContextItem("entityMenu")) {
			DrawEntityMenu(handle);
			ImGui::EndPopup();
		}
	}

	// --- 子を描く ---
	if (isOpen) {
		for (const Entity& other : EntityManager::GetAll()) {
			if (other.parent == handle) {
				DrawEntityNode(other.self);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}


//=============================================================================
// 一覧の下の何も無い所
//=============================================================================
void HierarchyWindow::DrawEmptySpace() {
	// 残りの高さいっぱいに透明なボタンを置いて、クリック・ドロップ・右クリックを受ける
	const ImVec2 space = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##emptySpace", ImVec2((std::max)(space.x, 1.0f), (std::max)(space.y, ImGui::GetFrameHeight())));
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		selected_ = {}; // 何も無い所をクリックしたら選択を外す
	}
	AcceptEntityDrop({}); // ここへ落としたらrootへ移す
	if (ImGui::BeginPopupContextItem("emptySpaceMenu")) {
		DrawCreateMenu();
		ImGui::EndPopup();
	}
}


//=============================================================================
// メニュー
//=============================================================================
void HierarchyWindow::DrawEntityMenu(Handle<Entity> handle) {
	if (ImGui::MenuItem("Rename", "F2")) {
		StartRename(handle);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Copy", "Ctrl+C")) {
		EditorHistory::Copy(handle);
	}
	// Paste：このEntityと同じ親の下（兄弟）に貼る　Paste As Child：このEntityの子として貼る
	if (ImGui::MenuItem("Paste", "Ctrl+V", false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste(ParentOf(handle));
	}
	if (ImGui::MenuItem("Paste As Child", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste(handle);
	}
	if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
		EditorHistory::RequestDuplicate(handle);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Create Child")) {
		EditorHistory::RequestCreate("Child", handle);
	}
	if (ImGui::MenuItem("Delete", "Del")) {
		EditorHistory::RequestDestroy(handle);
	}
}

void HierarchyWindow::DrawCreateMenu() {
	if (ImGui::MenuItem("Create Entity")) {
		EditorHistory::RequestCreate("Entity", {});
	}
	if (ImGui::MenuItem("Create Child", nullptr, false, selected_.IsValid())) {
		EditorHistory::RequestCreate("Child", selected_);
	}
	ImGui::Separator();
	// 何も無い所から貼るので、一番上（root）に貼る
	if (ImGui::MenuItem("Paste", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste({});
	}
}


//=============================================================================
// ショートカット（Hierarchyを触っているときだけ）
//=============================================================================
void HierarchyWindow::HandleShortcuts() {
	// 名前や数値を入力中なら、入力欄のほうの操作を優先する
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || renaming_.IsValid() || ImGui::GetIO().WantTextInput) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
		StartRename(selected_); // 選んでいなければ何もしない
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && selected_.IsValid()) {
		EditorHistory::RequestDestroy(selected_);
	}
	// IsKeyChordPressed：修飾キーまで完全に一致したときだけ（Ctrl+Shift+Cでは反応しない）
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C) && selected_.IsValid()) {
		EditorHistory::Copy(selected_);
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
		EditorHistory::RequestPaste(ParentOf(selected_)); // 選んでいるEntityの兄弟に貼る（選んでいなければroot）
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D) && selected_.IsValid()) {
		EditorHistory::RequestDuplicate(selected_);
	}
}


//=============================================================================
// 名前の変更を始める
//=============================================================================
void HierarchyWindow::StartRename(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}
	renaming_ = handle;
	renameBuffer_ = entity->name; // 今の名前から編集を始める
	renameFocusRequest_ = true;
	selected_ = handle;
}


//=============================================================================
// 名前の入力欄
//=============================================================================
void HierarchyWindow::DrawRenameField(Handle<Entity> handle) {
	// 出した最初のフレームだけ、入力欄にフォーカスを移す（すぐに文字を打てるように）
	const bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全選択（そのまま打つと置き換わる）
	const bool entered = EditorWidgets::InputText("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	// --- 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）---
	// Escで抜けたときはImGuiが中身を元に戻すので、名前は変わらない＝取り消しになる
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		EditorHistory::RequestRename(handle, renameBuffer_); // 空・同じ名前なら何もしない
	}

	// --- 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる ---
	bool finished = entered || ImGui::IsItemDeactivated();
	// 入力欄が有効にならないまま別の所をクリックされたときも閉じる（出しっぱなしにしない）
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_ = {};
	}
}
```

| 操作 | どこ |
|---|---|
| 作成 | 右上の＋ → Create Entity / Create Child、何も無い所を右クリック → 同じメニュー |
| 貼り付け | Entityを右クリック → **Paste**（そのEntityの兄弟）/ **Paste As Child**（そのEntityの子）、何も無い所を右クリック → Paste（一番上）、Ctrl+V（選んでいるEntityの兄弟） |
| 削除 | Deleteキー、右クリック → Delete |
| 名前の変更 | ダブルクリック、F2、右クリック → Rename（今までどおり） |
| 選択を外す | 何も無い所をクリック |
| 右クリックしたEntity | 選択される（メニューの対象と選択がずれないように） |

---

### G. 日本語入力と部品（ImeInput・EditorWidgets・Win32Window）

#### G-1. `Editor/Widgets/ImeInput.h`（新規）

```cpp
#pragma once
#include <cstdint>


/// <summary>
/// 日本語入力（IME）の、変換中の文字を扱う
/// <para>Windowsが出す小さな変換窓の代わりに、ImGuiの入力欄のカーソルの所へ変換中の文字を描く（候補の一覧はWindowsのまま）</para>
/// <para>全部の入力欄に効く（Hierarchyの名前・Add Componentの検索・数値の直接入力など）</para>
/// </summary>
class ImeInput {
public:
	/// <summary>
	/// ImGuiを作った後に1回呼ぶ。候補の一覧を出す位置の決め方を、ImGuiの標準から差し替える
	/// </summary>
	static void Initialize();

	/// <summary>
	/// ウィンドウのメッセージを見て、IMEが変換中かどうかを覚える（Win32Window::WindowProcから呼ぶ）
	/// </summary>
	/// <returns>trueならImGuiに渡さない（IMEが変換の確定・取り消しに使ったEnter / Esc）</returns>
	static bool HandleMessage(unsigned int message, std::uintptr_t wparam);

	/// <summary>
	/// ImGui::NewFrameの後に1回呼ぶ。変換中の文字をIMEから読む
	/// </summary>
	static void NewFrame();

	/// <summary>
	/// 全部のウィンドウを描いた後、ImGui::Renderの前に1回呼ぶ。変換中の文字を入力欄の中に描く
	/// </summary>
	static void DrawComposition();

	// 変換中の文字があるか（入力欄が、選んでいた文字を先に消すのに使う）
	static bool IsComposing();
};
```

#### G-2. `Editor/Widgets/ImeInput.cpp`（新規）

```cpp
#include "ImeInput.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include <Windows.h>
#include <imm.h>

#include <externals/imgui/imgui.h>
#include <externals/imgui/imgui_internal.h>

#pragma comment(lib, "imm32.lib")

namespace {
// 変換中の文字
struct Composition {
	std::string text;    // 変換中の文字（ImGuiに合わせてUTF-8）
	int cursor = 0;      // IMEのカーソルの位置（textの何バイト目か）
	int targetBegin = 0; // 変換の対象になっている文節（textの何バイト目から何バイト目か。無ければ同じ値）
	int targetEnd = 0;
};

Composition composition;
bool isComposing = false;     // IMEが変換中（WM_IME_STARTCOMPOSITION 〜 WM_IME_ENDCOMPOSITION）
bool enterUsedByIme = false;  // IMEが使ったEnter（離すまでImGuiに渡さない）
bool escapeUsedByIme = false; // IMEが使ったEsc（同上）


// UTF-16の先頭count文字が、UTF-8で何バイトになるか（IMEは位置をUTF-16の文字数で教えてくる）
int Utf8Length(const std::wstring& text, int count) {
	count = std::clamp(count, 0, static_cast<int>(text.size()));
	if (count == 0) {
		return 0;
	}
	return WideCharToMultiByte(CP_UTF8, 0, text.data(), count, nullptr, 0, nullptr, nullptr);
}

// IMEから変換中の文字を読む
Composition ReadComposition(HWND hwnd) {
	Composition result;
	HIMC context = ImmGetContext(hwnd);
	if (!context) {
		return result;
	}

	// --- 文字（UTF-16）。大きさはバイト数で返ってくる ---
	const LONG bytes = ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
	if (bytes > 0) {
		std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
		ImmGetCompositionStringW(context, GCS_COMPSTR, wide.data(), static_cast<DWORD>(bytes));
		const int length = static_cast<int>(wide.size());

		// --- 1文字ごとの状態（入力中・変換の対象・変換済み など）---
		std::vector<BYTE> attributes(wide.size(), ATTR_INPUT);
		ImmGetCompositionStringW(context, GCS_COMPATTR, attributes.data(), static_cast<DWORD>(attributes.size()));
		// --- カーソルの位置（何文字目か。戻り値がそのまま位置）---
		const LONG cursor = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);

		// --- UTF-8に直す ---
		result.text.resize(static_cast<size_t>(Utf8Length(wide, length)));
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, result.text.data(), static_cast<int>(result.text.size()), nullptr, nullptr);
		result.cursor = Utf8Length(wide, static_cast<int>(cursor));

		// --- 変換の対象の文節（スペースで変換したときに、今選んでいる部分）---
		int begin = -1;
		int end = -1;
		for (int i = 0; i < length; ++i) {
			if (attributes[i] == ATTR_TARGET_CONVERTED || attributes[i] == ATTR_TARGET_NOTCONVERTED) {
				if (begin < 0) {
					begin = i;
				}
				end = i + 1;
			}
		}
		if (begin >= 0) {
			result.targetBegin = Utf8Length(wide, begin);
			result.targetEnd = Utf8Length(wide, end);
		}
	}
	ImmReleaseContext(hwnd, context);
	return result;
}

// 変換中の文字を捨てる
void CancelComposition(HWND hwnd) {
	if (HIMC context = ImmGetContext(hwnd)) {
		ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
		ImmReleaseContext(hwnd, context);
	}
}

// ImGuiが入力欄のカーソルの位置をIMEに教えるときに呼ぶ関数（入力欄が変わった・カーソルが動いたときだけ呼ばれる）
// ImGuiの標準は候補の一覧の「左上」をカーソルの行の上端に合わせるので、変換中の文字に重なるおそれがある。
// 行の範囲（rcArea）を避けて、その下に出すように頼む
void SetImeData(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
	HWND hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);
	if (!hwnd || !data->WantVisible) {
		return;
	}
	HIMC context = ImmGetContext(hwnd);
	if (!context) {
		return;
	}
	const LONG x = static_cast<LONG>(data->InputPos.x - viewport->Pos.x);
	const LONG top = static_cast<LONG>(data->InputPos.y - viewport->Pos.y);
	const LONG bottom = top + static_cast<LONG>(data->InputLineHeight);

	// Windowsの変換窓（出さないようにしているが、出してしまうIMEのために場所だけ合わせておく）
	COMPOSITIONFORM compositionForm = {};
	compositionForm.dwStyle = CFS_FORCE_POSITION;
	compositionForm.ptCurrentPos = {x, top};
	ImmSetCompositionWindow(context, &compositionForm);

	// 候補の一覧：カーソルの行（top〜bottom）を避けて、行のすぐ下に出す
	CANDIDATEFORM candidateForm = {};
	candidateForm.dwIndex = 0;
	candidateForm.dwStyle = CFS_EXCLUDE;
	candidateForm.ptCurrentPos = {x, bottom};
	candidateForm.rcArea = {x, top, x + 1, bottom};
	ImmSetCandidateWindow(context, &candidateForm);

	ImmReleaseContext(hwnd, context);
}

// 入力欄の背景の色。FrameBgは半透明なので、ウィンドウの背景に重ねた色を作って不透明にする（下の文字を隠すため）
ImU32 GetFieldBackgroundColor() {
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec4& frame = style.Colors[ImGuiCol_FrameBg];
	const ImVec4& window = style.Colors[ImGuiCol_WindowBg];
	const float alpha = frame.w;
	return ImGui::GetColorU32(ImVec4(window.x + (frame.x - window.x) * alpha, window.y + (frame.y - window.y) * alpha, window.z + (frame.z - window.z) * alpha, 1.0f));
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void ImeInput::Initialize() { ImGui::GetPlatformIO().Platform_SetImeDataFn = SetImeData; }


//=============================================================================
// ウィンドウのメッセージ
//=============================================================================
bool ImeInput::HandleMessage(unsigned int message, std::uintptr_t wparam) {
	switch (message) {
	case WM_IME_STARTCOMPOSITION:
		isComposing = true;
		break;

	case WM_IME_ENDCOMPOSITION:
		isComposing = false;
		// IMEによっては、確定に使ったEnterのWM_KEYDOWNが、変換の終わりより後に届く。
		// そのとき押されたままのキーは「IMEが使ったキー」として、離すまでImGuiに渡さない
		enterUsedByIme |= (GetKeyState(VK_RETURN) & 0x8000) != 0;
		escapeUsedByIme |= (GetKeyState(VK_ESCAPE) & 0x8000) != 0;
		break;

	case WM_KILLFOCUS:
		isComposing = false;
		enterUsedByIme = false;
		escapeUsedByIme = false;
		break;

	case WM_KEYDOWN:
	case WM_KEYUP: {
		if (wparam != VK_RETURN && wparam != VK_ESCAPE) {
			break;
		}
		bool& usedByIme = (wparam == VK_RETURN) ? enterUsedByIme : escapeUsedByIme;
		if (message == WM_KEYDOWN && isComposing) {
			usedByIme = true; // 変換中に押された＝IMEの確定・取り消しに使うキー
		}
		if (usedByIme) {
			if (message == WM_KEYUP) {
				usedByIme = false; // 離したら、次からは普通にImGuiへ渡す
			}
			return true;
		}
		break;
	}
	}
	return false;
}


//=============================================================================
// 毎フレーム
//=============================================================================
void ImeInput::NewFrame() {
	composition = {};
	HWND hwnd = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
	if (!hwnd) {
		return;
	}
	if (ImGui::GetIO().WantTextInput) {
		composition = ReadComposition(hwnd);
	} else if (isComposing) {
		// 入力欄から外れたのに変換中の文字が残っていたら捨てる（ほかの入力欄やカメラ操作に持ち越さない）
		CancelComposition(hwnd);
	}
}

void ImeInput::DrawComposition() {
	if (composition.text.empty()) {
		return;
	}
	// 入力欄のカーソルの位置。ImGuiはIMEに教えるために、この値を毎フレーム作っている（imgui_internal.h）
	const ImGuiPlatformImeData& ime = ImGui::GetCurrentContext()->PlatformImeData;
	if (!ime.WantVisible) {
		return; // カーソルを出している入力欄が無い
	}

	ImFont* font = ImGui::GetFont();
	const float fontSize = ime.InputLineHeight;
	const char* text = composition.text.c_str();
	auto widthTo = [&](int end) { return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text, text + end).x; }; // 先頭からendバイト目までの幅
	const float width = widthTo(static_cast<int>(composition.text.size()));
	const ImVec2 origin(ime.InputPos.x + 1.0f, ime.InputPos.y); // InputPosは「カーソルの1ピクセル左・行の上端」
	const float bottom = origin.y + fontSize;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport()); // 全ウィンドウより手前に描く

	// --- 背景：入力欄と同じ色で塗って、カーソルより後ろの文字とカーソルを隠す ---
	drawList->AddRectFilled(ImVec2(origin.x - 1.0f, origin.y), ImVec2(origin.x + width + 1.0f, bottom), GetFieldBackgroundColor());

	// --- 変換の対象の文節は、選択の色で塗る ---
	const bool hasTarget = composition.targetEnd > composition.targetBegin;
	const float targetLeft = origin.x + widthTo(composition.targetBegin);
	const float targetRight = origin.x + widthTo(composition.targetEnd);
	if (hasTarget) {
		drawList->AddRectFilled(ImVec2(targetLeft, origin.y), ImVec2(targetRight, bottom), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
	}

	// --- 文字と下線（全体は細い線、変換の対象は太い線。WindowsのIMEと同じ見せ方）---
	const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
	drawList->AddText(font, fontSize, origin, textColor, text, text + composition.text.size());
	drawList->AddLine(ImVec2(origin.x, bottom - 1.0f), ImVec2(origin.x + width, bottom - 1.0f), textColor, 1.0f);
	if (hasTarget) {
		drawList->AddLine(ImVec2(targetLeft, bottom - 1.0f), ImVec2(targetRight, bottom - 1.0f), textColor, 2.0f);
	} else {
		// 打っている途中なら、変換中の文字の中にカーソルを出す
		const float cursorX = origin.x + widthTo(composition.cursor);
		drawList->AddLine(ImVec2(cursorX, origin.y), ImVec2(cursorX, bottom), ImGui::GetColorU32(ImGuiCol_InputTextCursor), 1.0f);
	}
}

bool ImeInput::IsComposing() { return !composition.text.empty(); }
```

- `ImmGetCompositionStringW` が返す位置は **UTF-16の文字数**、ImGuiが扱う文字列は **UTF-8のバイト列**。`Utf8Length` で「先頭から何文字分が、UTF-8で何バイトか」に直してから、`CalcTextSizeA` で幅を測る
- 入力欄のカーソルの位置は、ImGuiがIMEに教えるために毎フレーム作っている `PlatformImeData` から読む（`imgui_internal.h` の中身なので、ImGuiを更新したときに名前が変わることはある）
- 箱の色は `GetFieldBackgroundColor` の1か所。画像のような白い箱にしたければ、ここで白を返し、`DrawComposition` の文字の色を黒にするだけ

#### G-3. `Editor/Widgets/EditorWidgets.h`（全体を差し替え）

`InputText` にIMEの対応（選択を先に消す）、新しく `PlusButton` を追加。IMEを直接読む関数は `ImeInput.cpp` へ移したので、`<Windows.h>` はもう読まない。

```cpp
#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/Widgets/ImeInput.h"


/// <summary>
/// Hierarchy・Inspectorなど、エディタのウィンドウで共通に使うImGuiの部品
/// <para>ImGuiManagerは USE_IMGUI の中にしか無いので、Releaseでもコンパイルされるウィンドウから使える場所に置く</para>
/// </summary>
namespace EditorWidgets {

// std::string版InputTextのコールバック
inline int InputTextCallback(ImGuiInputTextCallbackData* data) {
	// 文字が増えて入れ物が足りなくなったら、std::stringを伸ばして付け替える（ImGui公式の misc/cpp/imgui_stdlib と同じ仕組み）
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		auto* text = static_cast<std::string*>(data->UserData);
		text->resize(static_cast<size_t>(data->BufTextLen));
		data->Buf = text->data();
	}
	// 日本語の変換が始まったら、選んでいた文字を先に消す（メモ帳などと同じ。変換中の文字がその場所に出る）
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
		if (ImeInput::IsComposing() && data->HasSelection()) {
			const int start = (std::min)(data->SelectionStart, data->SelectionEnd);
			data->DeleteChars(start, std::abs(data->SelectionEnd - data->SelectionStart));
		}
	}
	return 0;
}

/// <summary>
/// std::string をそのまま編集する入力欄。長さの上限が無いので、日本語でも途中で切れない
/// </summary>
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways;
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, InputTextCallback, &text);
}

/// <summary>
/// 大きな「＋」付きのボタン。labelの「##」より前が＋の右に出る文字（"##create" なら＋だけ）
/// <para>sizeの考え方はImGui::Buttonと同じ（0なら中身に合わせる、-FLT_MINなら横幅いっぱい）</para>
/// </summary>
inline bool PlusButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f)) {
	const ImGuiStyle& style = ImGui::GetStyle();
	const char* textEnd = std::strstr(label, "##");
	if (!textEnd) {
		textEnd = label + std::strlen(label);
	}
	const bool hasText = (textEnd != label);
	const float iconSize = ImGui::GetFontSize() * 0.7f; // ＋の縦横の長さ
	if (size.x == 0.0f) {
		const float textWidth = hasText ? style.ItemInnerSpacing.x + ImGui::CalcTextSize(label, textEnd).x : 0.0f;
		size.x = style.FramePadding.x * 2.0f + iconSize + textWidth;
	}

	// ボタンそのものは文字無しで作り、上に＋と文字を自分で描く
	ImGui::PushID(label);
	const bool pressed = ImGui::Button("##plusButton", size);
	ImGui::PopID();

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
	const float half = iconSize * 0.5f;
	const float thickness = (std::max)(2.0f, ImGui::GetFontSize() * 0.12f); // 線の太さ（文字より太くして目立たせる）
	// 文字があれば左寄せ、無ければ真ん中に＋を描く
	const float centerX = hasText ? min.x + style.FramePadding.x + half : (min.x + max.x) * 0.5f;
	const float centerY = (min.y + max.y) * 0.5f;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(centerX - half, centerY), ImVec2(centerX + half, centerY), color, thickness);
	drawList->AddLine(ImVec2(centerX, centerY - half), ImVec2(centerX, centerY + half), color, thickness);
	if (hasText) {
		const ImVec2 textPos(centerX + half + style.ItemInnerSpacing.x, centerY - ImGui::GetFontSize() * 0.5f);
		drawList->AddText(textPos, color, label, textEnd);
	}
	return pressed;
}

/// <summary>
/// enumをコンボボックスで選ばせる。選択肢の名前はmagic_enumが型から作る
/// </summary>
template<typename E> bool EnumCombo(const char* label, E& value) {
	static_assert(std::is_enum_v<E>, "EnumCombo は enum 専用です");
	constexpr auto names = magic_enum::enum_names<E>();
	size_t current = magic_enum::enum_index(value).value_or(0);
	bool changed = false;
	std::string currentName(names[current]);
	if (ImGui::BeginCombo(label, currentName.c_str())) {
		for (size_t i = 0; i < names.size(); ++i) {
			std::string name(names[i]);
			bool isSelected = (i == current);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
				value = magic_enum::enum_value<E>(i);
				changed = true;
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus(); // 開いたときに今の選択へスクロールする
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

} // namespace EditorWidgets
```

`PlusButton` はフォントの文字の「＋」ではなく線で描いている。フォントの＋は細くて小さいので、線の太さを文字より太くして目立たせた。

#### G-4. `Window/Win32Window.h`

`private` の次の3行を消す。

```cpp
	bool imeComposing_ = false; // IMEが未確定文字を変換中
	bool imeEnter_ = false;     // IMEに使ったEnterのキーアップ待ち
	bool imeEscape_ = false;    // IMEに使ったEscのキーアップ待ち
```

#### G-5. `Window/Win32Window.cpp`

`#include "MyEngine/Diagnostics/LogManager.h"` の下に追加。

```cpp
#include "MyEngine/Editor/Widgets/ImeInput.h"
```

`ProcessMessage` を差し替え（`hwnd_` → `nullptr`）。

```cpp
bool Win32Window::ProcessMessage() {
	MSG msg{};
	// このスレッドに届いたメッセージを全部処理する。
	// hwnd_を指定すると自分宛てしか取り出さず、IMEの変換窓などWindowsが作ったウィンドウ宛てのメッセージが処理されないまま残る
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return IsWindow(hwnd_) != 0;
}
```

`WindowProc` を関数ごと差し替え。IMEの所のほか、`case` ごとに `self` を取り直していたのを頭の1回にまとめた（`WM_SYSCOMMAND` だけ `self` のnullチェックが抜けていたのも直した）。

```cpp
LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// SetWindowLongPtrで保存した値を取り出す（ウィンドウを作っている途中はまだnullptr）
	Win32Window* self = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

#ifdef USE_IMGUI
	// 変換中の文字はImGuiの入力欄の中に自分で描く（ImeInput）ので、Windowsの小さな変換窓は出さない。
	// 最初のこのメッセージは、ウィンドウを表示した瞬間（SetImGuiTargetより前）に届くので、isImGuiTarget_を見ずに外す
	if (msg == WM_IME_SETCONTEXT) {
		lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
	}
	if (self && self->isImGuiTarget_) {
		// IMEが変換の確定・取り消しに使ったEnter / Escは、ImGuiに渡さない（渡すと名前の確定・取り消しまで一緒に起きる）
		if (ImeInput::HandleMessage(msg, wparam)) {
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		const LRESULT result = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
		// WM_IME_COMPOSITIONは、ImGuiの中でDefWindowProcWまで済ませている（下のDefWindowProcで2回目を呼ばない）
		if (result != 0 || msg == WM_IME_COMPOSITION) {
			return result;
		}
	}
#endif

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {

	// Alt + Enter でフルスクリーン切り替え
	case WM_SYSKEYDOWN: {
		// lparam の bit29 = Altが押されている、bit30 = 直前も押されていた（キーリピート）
		bool isAlt = (lparam & (1 << 29)) != 0;
		bool isRepeat = (lparam & (1 << 30)) != 0;
		if (self && wparam == VK_RETURN && isAlt && !isRepeat) {
			self->ToggleFullscreen();
			return 0; // DefWindowProc に渡すとシステムメニューが開いてしまう
		}
		break;
	}

	// ウィンドウのサイズが変わったとき
	case WM_SIZE: {
		if (wparam == SIZE_MINIMIZED || !self) {
			break;
		}
		int w = static_cast<int>(LOWORD(lparam));
		int h = static_cast<int>(HIWORD(lparam));
		// 最大化・復元時は即時リサイズ
		if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
			if (self->onResize_) {
				self->onResize_(w, h);
			}
		} else {
			// ドラッグ中などは保留
			self->pendingResize_ = true;
			self->pendingWidth_ = w;
			self->pendingHeight_ = h;
		}
		break;
	}

#ifdef USE_IMGUI
	// ウィンドウのサイズ制限
	case WM_GETMINMAXINFO: {
		if (self && self->isImGuiTarget_) {
			MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lparam);
			info->ptMinTrackSize.x = 800;
			info->ptMinTrackSize.y = 500;
			info->ptMaxTrackSize.x = 1920;
			info->ptMaxTrackSize.y = 1080;
		}
		break;
	}
#endif

	case WM_SYSCOMMAND: {
		// ウィンドウの移動制限がかかっているとき
		UINT command = static_cast<UINT>(wparam & 0xFFF0);
		if (self && self->isPositionLocked_ && (command == SC_MOVE || command == SC_SIZE)) {
			return 0;
		}
		break;
	}

	// ウィンドウの×ボタン
	case WM_CLOSE: {
		if (self && self->onCanClose_) {
			if (!self->onCanClose_()) {
				// プロジェクト側で閉じれなかった処理を書いてから呼ぶ
				if (self->onTryClose_) {
					self->onTryClose_();
				}
				return 0;
			}
		}
		DestroyWindow(hwnd);
		return 0;
	}

	// ウィンドウが破棄された
	case WM_DESTROY: {
		return 0;
	}
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
```

`WM_IME_SETCONTEXT` だけ `isImGuiTarget_` を見ていないのは、**最初のこのメッセージが、ウィンドウを表示した瞬間（`Init` の `ShowWindow` の中）に届く**ため。`SetImGuiTarget(true)` はその後なので、`isImGuiTarget_` を見るとその1回を取りこぼし、次にウィンドウを選び直すまでWindowsの変換窓が出てしまう。

---

### H. ImGuiManager

#### H-1. `Editor/ImGuiManager.cpp`

includeを直す（3行を差し替えて、1行足す）。

```cpp
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Editor/Windows/InspectorWindow.h"
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Widgets/ImeInput.h"
```

無名namespaceの `DrawProfilerBar` を消して、代わりに次を入れる。

```cpp
	// エディタ全体で効くショートカット（どのウィンドウを触っていても効く）
	// 文字の入力中・ドラッグ中は何もしない（入力欄の中のCtrl+Zは、ImGuiが文字のUndoに使う）
	void HandleGlobalShortcuts() {
		if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive()) {
			return;
		}
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) {
			EditorHistory::RequestUndo();
		}
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
			EditorHistory::RequestRedo();
		}
	}
```

`IsKeyChordPressed` は「修飾キーまで完全に一致したときだけ」true。`IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl` だと、Ctrl+Shift+Z でもUndoとRedoが両方動いてしまう。

`Initialize`：`instance_->hwnd_ = window->GetHWND();` の行を消す。`ImGui_ImplDX12_Init(&initInfo);` の下と、`LoadStyle` の下にそれぞれ追加。

```cpp
	ImGui_ImplDX12_Init(&initInfo);
	// 日本語入力：候補の一覧を、入力欄の行を隠さない位置に出す
	ImeInput::Initialize();

	// 前回保存したスタイルを自動で読み込む（ファイルがなければスキップ）
	instance_->LoadStyle("imgui_style.ini");

	// InspectorにエンジンのComponent（Transform・ModelRenderer）を登録する
	InspectorWindow::Initialize();
```

`Begin`：`ImGui::NewFrame();` の下に1行。

```cpp
	ImGui::NewFrame();
	ImeInput::NewFrame(); // 日本語の変換中の文字を読む
```

`Begin`：メニューバーの `Edit` の中身。

```cpp
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, EditorHistory::CanUndo())) {
				EditorHistory::RequestUndo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, EditorHistory::CanRedo())) {
				EditorHistory::RequestRedo();
			}
			ImGui::EndMenu();
		}
```

`Begin`：一番最後（`InspectorWindow::Draw();` の下）。

```cpp
	// ===== Undo・Redoのショートカット =====
	HandleGlobalShortcuts();
```

`Render`：`ImGui::Render();` の上に1行。

```cpp
	ImeInput::DrawComposition(); // 全部のウィンドウを描き終わってから、変換中の文字を入力欄の上に描く
	ImGui::Render();
```

#### H-2. `Editor/ImGuiManager.h`

`private` の最後の、次の4行を消す（`requests_` は残す）。

```cpp
	// メインウィンドウのハンドル
	HWND hwnd_ = nullptr;

	// Setting メニューの表示状態フラグ
	bool isShowStyleEditor_ = false;
```

---

### 確認したこと（写す前に、こちらで）

- 変更後のエンジンを一時フォルダに組み立て、**全76ファイル**（今の71＋新しい5）を `/W4` の **Debug（USE_IMGUI）/ Release** でコンパイル：エラー0。今回触ったファイルに新しい警告は無い（`Win32Window.cpp` の `self` の警告4件は消えた。残る警告はspdlogと、元からある `WindowManager.cpp` の `scene` だけ）
- 古い場所のヘッダー（`Editor/EditorOverlay.h` など）を「読んだらエラーになるファイル」に置き換えてコンパイルし、**古いincludeが1か所も残っていない**ことを確認
- ゲーム側 `GameScene.cpp`・`Stage.cpp`・`Particles.cpp`・`main.cpp` を新しいヘッダーで Debug / Release コンパイル：エラー0
- **本物の `EntityManager`・`SlotMap`・`EditorHistory` を動かすテスト**（ImGuiだけ外したもの）で次を確認：全部通過
  - 作成 → Undo → Redo で、同じEntityIdで戻る
  - 100フレームのドラッグが1件になる。**ドラッグ中にゲームがRotationを変えても、Position のUndoでRotationは戻らない**
  - ドラッグして元の値に戻しただけなら、履歴は増えない
  - Add Component → 色の編集 → Undo×2で外れる → Redo×2で色まで戻る
  - 子とModel Rendererを持つEntityを削除 → Undoで子・Component・値・IDまで戻る。**削除より前の履歴（色の編集）も、戻ったEntityにそのまま効く**
  - `Cube (Copy1)` → `Cube (Copy2)`、`Cube (Copy1)` をコピーして貼ると `Cube (Copy3)`（`(Copy1) (Copy1)` にならない）。子も一緒に貼られ、IDは新しくなる。Paste As Child・Duplicate
  - Remove Component → Undoで同じ値のまま戻る。Transformは外せない
  - 名前（日本語）・有効・親の付け替えのUndo、輪になる付け替えは無視、Undo後に新しい操作をするとRedoが消える
  - 編集した直後の同じフレームにUndoを押しても、「編集を記録 → Undo」の順になる
  - ゲームが勝手に子を足したEntityの「作成のUndo」は断る（その子まで消さない）→ 警告を出して履歴を消す
- **確かめられていないこと**：画面の見た目と、WindowsのIMEの実際の動き（変換中の文字の位置・候補の一覧の位置・Enter/Escの扱い）

### 写した後に確認すること

1. `CG3_Project/MyEngine/include/MyEngine/Editor` を消す → エンジンDebug / Release → ゲームDebug / Release
2. **日本語**（HierarchyでF2かダブルクリック）
   - 「あいうえ」と打つと、**名前の欄の先頭に**下線付きで出る（元の名前は消える）
   - スペースで変換すると、変換中の文節に色が付いて太い下線になる。候補の一覧が**欄のすぐ下**に出る
   - Enter 1回目は変換の確定だけ（欄は閉じない）、2回目で名前の確定。Esc 1回目は変換の取り消し、2回目で名前の変更の取り消し
   - 変換中に欄の外をクリックすると、変換中の文字は捨てられ、名前は変わらない
   - Add Componentの検索欄、数値の直接入力（Ctrl+クリック）でも同じように出る
   - **Windowsの白い変換窓が別に出ないこと**
3. **Undo**：Positionを数秒ドラッグ → Ctrl+Z 1回で元の位置。Reset → 1回で戻る。色のピッカーはドラッグ1回ごとに1件。Model・Shadingなどのコンボ、Enabled、有効のチェック。Add Component → Ctrl+Zで外れる。見出しを右クリック → Remove Component → Ctrl+Zで値ごと戻る。子のあるEntityを削除 → Ctrl+Zで戻り、Inspectorの **ID** が同じ
4. **コピー**：Ctrl+C → Ctrl+V で `(Copy1)`、もう1回で `(Copy2)`。右クリック → Paste / Paste As Child。何も無い所の右クリック → Paste。Ctrl+D。コピーしてから Stop → 貼れる（コピーした物はシーンを作り直しても残す）
5. **＋ボタン**：Hierarchyの右上の＋ → メニュー、Add Componentの＋
6. **ショートカット**：Edit → Undo / Redo。Inspectorを触っているときもCtrl+Zが効く。名前の入力中のCtrl+Zは文字のUndoになる

#### 日本語の表示がおかしいとき

- **変換中の文字が出ない**：`Win32Window.cpp` の `lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;` を一時的にコメントにする。Windowsの白い変換窓が**入力欄の上に**出れば、「メッセージの取り出し方」が原因だったことの確認になり、`ImeInput` の読み取りか描画の方に問題がある。白い窓も出なければ、IMEの種類（設定 → 時刻と言語 → 言語 → 日本語 → Microsoft IME → 全般 →「以前のバージョンの Microsoft IME を使う」）を教えてほしい
- **候補の一覧が文字に重なる / 離れすぎる**：`SetImeData` の `candidateForm.ptCurrentPos` の y（`bottom`）を調整する
- **Enterで名前まで確定してしまう**：`ImeInput::HandleMessage` の `return true;` の直前に `OutputDebugStringA` を入れて、Enterが「IMEが使ったキー」と判定されているかを見る

---

## Undoの仕組み（解説）

今回の `EditorHistory` の読み方。コードは `Editor/History/EditorHistory.cpp` の1ファイルにまとまっている。

### 1. 全体の流れ

```
 UI（Hierarchy・Inspector・メニュー）        EditorHistory                          シーン（EntityManager）
 ─────────────────────────────           ───────────────────────────────        ─────────────────────
 「消して」RequestDestroy(handle)  ──▶   requests に「消す処理」を積むだけ
 「戻して」RequestUndo()           ──▶   requests に「戻す処理」を積むだけ
                                          （このフレームはここまで。UIは描き続ける）

 ── 次のフレームの最初：WindowManager::UpdateAll → EditorHistory::Flush ──

                                          requests を頼まれた順に実行
                                            「消す処理」：写しを取って、消して、履歴に1件足す  ──▶ Entityが消える
                                            「戻す処理」：履歴の1件の undo を呼ぶ            ──▶ 写しから作り直す
```

**なぜすぐ実行せず、次のフレームまで待つのか**：Hierarchyは `EntityManager::GetAll()` の配列を回しながら描いている。その途中でEntityを消したり足したりすると、配列の中身が動いて（SlotMapは末尾と入れ替えて消す）、回している途中のループが壊れる。なので「頼む（Request）」と「やる（Flush）」を分け、やるのは誰も配列を回していない**フレームの境目**だけにしている。

Undo・Redoも同じ `requests` に積むので、**頼まれた順番が必ず守られる**（「編集を記録する」→「Undoする」の順が入れ替わらない）。

### 2. 履歴の持ち方

```cpp
struct Entry {
	std::function<bool()> undo; // 戻す
	std::function<bool()> redo; // やり直す
};
std::vector<Entry> history; // 古い順
size_t cursor;              // 適用済みの件数
```

```
 history:  [作成] [移動] [色] [削除] [名前]
                               ▲
                            cursor = 3
 ・Undoできる：cursorより左（[色] から順に戻す）
 ・Redoできる：cursorより右（[削除] から順にやり直す）
 ・Undoした後に新しい操作をすると、cursorより右は捨てる（Redoできなくなる）
```

1件の操作を「戻す関数」と「やり直す関数」の組で持つのは、**コマンドパターン**と呼ばれる形。何を戻すかは関数の中に閉じ込めてあるので、`history` 自体は「作成」も「色の変更」も区別しない。
どちらの関数も、相手が見つからないなどで失敗したら `false` を返す。Undo・Redoが失敗したら、履歴と今のシーンが食い違っている（ゲームのコードが勝手に消した等）ので、続けると関係ない物を書き換えかねない。そのときは**履歴を全部捨てる**（`Failed`）。

### 3. 相手は EntityId で覚える

「削除 → Undo」でEntityを作り直すと、SlotMapの世代が進むので**Handleは前と違う値になる**。履歴が相手をHandleで覚えていると、削除より前の履歴（例：その前にした色の変更）が、もう相手を見つけられない。

```
 [色の変更 (Handle 3/1)] [削除 (Handle 3/1)]
        Undo → 削除を戻す → 作り直したEntityは Handle 3/2
        Undo → 色の変更を戻したいが、Handle 3/1 はもう無い ✕
```

そこで、Entityに**消して作り直しても変わらない番号 `EntityId`** を持たせ、履歴は全部これで相手を覚える。戻すときは `CreateWithId` で**同じ番号**のまま作り直すので、`FindById` で誰でも同じ相手を見つけられる。

Codexの版は、`Identity`（Handleを入れる共有の箱）を `shared_ptr` で配り、作り直したら箱の中身を差し替える方法だった。考え方は同じだが、番号の方が「箱の配り方」を気にしなくてよく、将来のシーン保存（親子関係をファイルに書く）にもそのまま使える。

### 4. 操作ごとの記録の仕方（3種類）

| 種類 | 例 | 何を覚えるか | 戻し方 |
|---|---|---|---|
| **Entityを増やす・減らす** | 作成・削除・貼り付け・複製 | Entityとその子孫の**写し**（`EntityTree`：名前・有効・親の番号・Componentの中身のバイト列） | 写しから同じ番号で作り直す（`Restore`）／写しの番号のEntityを消す（`Remove`） |
| **Entityの項目** | 名前・有効・親 | 変える前の値と変えた後の値 | 値を書き戻す |
| **Componentの中身** | Position・色・Material | Inspectorが**描く前と描いた後で変わったバイト**の、前後の値 | 変わったバイトだけ書き戻す |

- 作成は「Undo＝消す、Redo＝写しから作る」、削除はその逆。**同じ `Restore` と `Remove` を逆向きに使っているだけ**
- 貼り付けは、コピーした写しに**新しい番号を振り直して**から作成と同じ扱いにする。子が覚えている親の番号も、古い番号→新しい番号の表（`newIds`）で付け替える
- 名前・有効は `RequestSetEntityValue(handle, &Entity::name, 新しい名前)` のように、「Entityのどの項目か」を**メンバへのポインタ**で渡す1つの関数で書いている

### 5. Componentの中身：バイトで比べる

Inspectorの `DrawComponent` はこの3行だけ。

```cpp
const std::vector<std::byte> before(bytes, bytes + editor.GetSize()); // 描く前に中身を丸ごと写す
editor.Draw(component);                                                // ImGuiで描く（ここで値が変わるかもしれない）
EditorHistory::RecordComponentChange(handle, editor, before.data(), component); // 描いた後と比べる
```

Componentは「データだけ」（ポインタも `std::string` も持たない）なので、**中身はただのバイトの並び**として比べられる。項目の名前を知らなくても、「何バイト目が変わったか」で「どの項目が変わったか」が分かる。

```
 TransformComponent（バイトの並び）
 ┌ translation.x ┬ translation.y ┬ ... ┬ rotation.x ┬ ... ┬ scale.z ┬ worldMatrix ... ┐
 │    4バイト     │    4バイト     │     │   4バイト   │     │  4バイト │               │
 └───────────────┴───────────────┴─────┴────────────┴─────┴─────────┴───────────────┘
   ↑ Positionを動かしたら、この4バイトだけが変わる → この4バイトだけを覚える
```

- **4バイトずつ印を付ける**：floatの4バイトのうち一部だけが変わることがある。その一部だけ戻すと、ゲームが同じ値を途中で変えていたときに、2つの値が混ざったでたらめなfloatになる。floatやuint32_tは4バイトなので、4バイトまとめて戻す
- **マウスを離すまでを1件にする**：ドラッグ中は毎フレーム変わる。同じEntityの同じComponentの変更は `pendingEdits` の1つにまとめ、「編集前」は最初に変わったときの値のまま、「編集後」だけ毎フレーム最新にする。どの項目も操作していない（`!ImGui::IsAnyItemActive()`）になったら、`CommitComponentChanges` で1件の履歴にする
- **Inspectorの外の変化は入らない**：比べるのは「このフレームのInspectorが描く前と後」だけ。ゲームの `Update` が変えた値（アニメーションで回っている角度など）は、Inspectorの外で変わるので記録されない。なのでPositionのUndoで、ゲームが動かしたRotationまで戻ることはない
- ドラッグして元の値に戻しただけ（変わったバイトの前後がすべて同じ）なら、1件にまとめる時に捨てる。Undoしても何も起きない1件が残らないように

### 6. Codexの版の `TrackInspectorValues` との違い

Codexの版は、**項目を1つずつ名前と取り出し方で書き並べていた**。

```cpp
modelField("Model.Color", &ModelRendererComponent::color);
materialField("Material.Roughness", &MaterialParams::roughness);
// ……Componentに項目を足すたびに、ここにも1行足す。Componentを増やしたら区画ごと足す
```

さらに、値を入れる `std::variant<bool, uint32_t, float, std::string, Vector3, ShadingType, ...>` にも、使う型を全部並べる必要があった。ゲーム側でComponentを作ったら、エンジンの `EditorHistory.h` を書き換えないとUndoできない。

今回の形では、**Componentの種類が増えても `EditorHistory`・`InspectorWindow` は変えない**。Componentごとに書くのは `ComponentEditor` の1クラス（表示と実体の取り方）だけで、Undo・コピー・削除のUndo・Add Component・Remove Componentはそこから自動で効く。

| | Codexの版 | 今回 |
|---|---|---|
| 項目の記録 | 項目ごとに名前と取り出し方を手書き | 描く前後のバイトを比べる |
| 値の型 | `std::variant` に全部並べる | バイト列なので型を知らなくてよい |
| 削除の写し | `std::optional<ModelRendererComponent>` を直に持つ | 登録されている全Componentを `ComponentEditor` 経由で写す |
| 相手の覚え方 | `shared_ptr<Identity>` の中のHandle | `EntityId` |
| Componentを増やしたとき | `TrackInspectorValues`・`PropertyValue`・`Node`・`Capture`・`Restore` を直す | `ComponentEditor` を1つ作って登録するだけ |

### 7. できないこと・注意

- **ゲームが持っている古いHandleは戻らない**：削除をUndoすると、ゲームのメンバ変数が持っていたHandle（例：`GameScene::sceneRoot_`）は無効のまま。ゲームが相手を覚え続けたいなら、将来は `EntityId` で覚えるようにする
- **写したComponentの中の番号（モデル・テクスチャのHandle）は、そのまま共有する**：モデルそのものはコピーしない（Unityでも同じで、同じメッシュを指す）
- **コピーした物はシーンを作り直しても残す**（`Clear` で消さない）。PlayしながらCopyして、Stopした後に貼る、という使い方ができる
- 履歴は128件まで。超えたら古いものから捨てる
- 今の比べ方は「Inspectorの中で変わった物」だけ。将来ギズモ（ImGuizmo）でSceneビューから動かすときも、ギズモを描く前後で同じ `RecordComponentChange` を呼べばUndoできる

---

## 1. 日本語入力の修正

**確認できた問題**：同梱の `imgui_impl_win32.cpp` は `WM_IME_COMPOSITION` で `DefWindowProcW` を呼ぶ。
戻り値0の場合、今の `Win32Window::WindowProc` は末尾でも `DefWindowProc` を呼び、同じメッセージを二重処理する。
また、HierarchyはEnterで名前編集を終えるため、IMEの確定に使ったEnterが届くと確定文字を受け取る前に編集を終える可能性がある。
**この2点を修正するが、入力不能の原因がこれだけかは未確定。実機確認前に「日本語が直った」とは扱わない。**

Windows標準処理はIMEへメッセージを渡す役割を持つ。[Microsoftの説明](https://learn.microsoft.com/en-us/windows/win32/intl/wm-ime-composition)参照。
フォント変更や未確定文字の再挿入は行わず、文字の入力経路は既存の `WM_CHAR` に一本化する。

### 1-A. `Win32Window.h`

privateの `isImGuiTarget_` の下に追加する。**メンバをUSE_IMGUIで囲まない**（エンジンとゲームでクラスのサイズを揃える）。

```cpp
	bool imeComposing_ = false; // IMEが未確定文字を変換中
	bool imeEnter_ = false;     // IMEに使ったEnterのキーアップ待ち
	bool imeEscape_ = false;    // IMEに使ったEscのキーアップ待ち
```

### 1-B. `Win32Window.cpp`

`WindowProc` 冒頭の `#ifdef USE_IMGUI` 内にある `if (self && self->isImGuiTarget_)` **だけ**を差し替える。
外側の `self` の取得・ifdefと、後ろのゲーム用switchは残す。

```cpp
	if (self && self->isImGuiTarget_) {
		// IMEが使ったEnter/Escを、名前欄の確定/取消へ重ねて渡さない。
		if (msg == WM_IME_STARTCOMPOSITION) {
			self->imeComposing_ = true;
		} else if (msg == WM_IME_ENDCOMPOSITION) {
			self->imeComposing_ = false;
			// IMEによってはENDがキーイベントより先に届く。
			self->imeEnter_ |= (GetKeyState(VK_RETURN) & 0x8000) != 0;
			self->imeEscape_ |= (GetKeyState(VK_ESCAPE) & 0x8000) != 0;
		} else if (msg == WM_KILLFOCUS) {
			self->imeComposing_ = false;
			self->imeEnter_ = self->imeEscape_ = false;
		}
		const bool keyDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
		const bool keyUp = msg == WM_KEYUP || msg == WM_SYSKEYUP;
		if ((keyDown || keyUp) && (wparam == VK_RETURN || wparam == VK_ESCAPE)) {
			bool& imeKey = wparam == VK_RETURN ? self->imeEnter_ : self->imeEscape_;
			if (keyDown && self->imeComposing_) {
				imeKey = true;
			}
			if (imeKey) {
				if (keyUp) {
					imeKey = false;
				}
				return DefWindowProc(hwnd, msg, wparam, lparam);
			}
		}
		if (msg == WM_CHAR && ((wparam == L'\r' && self->imeEnter_) ||
			(wparam == 27 && self->imeEscape_))) {
			return 0; // IME操作に使った制御文字。日本語のWM_CHARは下へ通す。
		}
		const LRESULT handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
		// 同梱バックエンドはCOMPOSITION内でDefWindowProcWを呼ぶ。
		// 戻り値0でも処理済みなので、このWndProc末尾で二重に呼ばない。
		if (msg == WM_IME_COMPOSITION) {
			return 0;
		}
		if (handled != 0) {
			return handled; // カーソル等、IME以外の戻り値は従来どおり
		}
	}
```

`EditorWidgets::InputText`、未確定文字プレビュー、`RequestRename` はそのまま。
IMEに使った最初のEnterは変換確定、次のEnterは名前確定になることを確認する。
先にF2で入力欄へ入り、英数字→日本語→変換→確定→名前確定を試す。症状が残る場合は「日本語へ切替不可」「未確定表示のみ不可」「確定で消える」のどこで止まるかを分ける。

## 2. EditorHistoryの説明とInspector用の拡張

### 2-A. 既存部分のコメントを補う

`EditorHistory.h` の `namespace EditorHistory` の直前へ追加する。既存のCreate/Delete/Copyの本体は書き直さない。

```cpp
// EditorHistoryの流れ：UIでRequest → Update冒頭のFlushで適用 → historyへ登録。
// UIの一覧走査中にSlotMapを増減させないため、構造を変える処理は予約する。
//
// Identity : 同じ編集対象を指す参照票。Entityそのものは所有しない。
//            削除Undoで新しいHandleができるとcurrentだけを更新する。
// Node/Tree: コピー・削除Undoに必要な値と親子構造。GPUアセットは番号を共有する。
// Entry    : undo（戻す処理）とredo（進める処理）の組。
// cursor   : 適用済みの件数。[0,cursor)がUndo可能、[cursor,size)がRedo可能。
//
// Find     : 生存Handleから共通のIdentityを取得する。
// Capture  : 親から子の順に値を保存。worldMatrixは保存せず再計算する。
// Restore  : Entityを新規作成しIdentityを更新。SlotMapの世代は巻き戻さない。
// Remove   : 対象の子孫が変わっていないか確認してから削除する。
// Execute  : 未適用の操作をredoして登録。Inspectorには再適用しないAppendを使う。
// Clear    : シーン変更・終了時に履歴と編集中の操作を破棄する。
//
// 生のComponentポインタをラムダへ保存しない。適用時にHandleから取得し直す。
```

### 2-B. 履歴へ「適用済みの値変更」を登録する

標準includeへ `<variant>` を追加。
`namespace Detail` の既存 `clipboard` 定義の下へ追加する（Ref / Entry / history / cursor定義より後）。

```cpp
// Inspectorの編集前後の値。GPUリソースやComponentのポインタは保存しない。
using PropertyValue = std::variant<bool, uint32_t, float, std::string, Vector3,
	ShadingType, BlendMode, RasterizerType, DepthMode, BillboardMode>;
struct PropertyEdit {
	Ref target;                         // 削除UndoでHandleが変わっても同じ対象を追う
	std::string key;                    // "Transform.Position"など、項目ごとの識別子
	PropertyValue before, after;        // ドラッグ開始前と、最後に編集した値
	std::function<bool()> available;    // EntityとComponentが今も存在するか
	std::function<void(const PropertyValue&)> apply;
};
inline std::vector<PropertyEdit> pendingEdit; // まだ終了していない1操作
inline bool batchHasChange = false;          // このInspector描画で変更を既に記録したか

// 既に画面で適用した操作は、もう一度redoせず履歴だけに登録する。
inline void Append(Entry entry) {
	history.erase(history.begin() + static_cast<std::ptrdiff_t>(cursor), history.end());
	history.push_back(std::move(entry));
	if (history.size() > 128) {
		history.erase(history.begin());
	}
	cursor = history.size();
}
```

既存の `Detail::Execute` だけ次へ差し替える。

```cpp
// Create/Delete等は、まだ実行していないので適用してから履歴へ登録する。
inline void Execute(Entry entry) {
	if (!entry.redo()) {
		Failed();
		return;
	}
	Append(std::move(entry));
}
```

次を `} // namespace Detail` の直後、`Clear()` **より前**に追加する。
ドラッグ中はbeforeを保持しafterを更新する。最後に1件へまとめ、既にUIへ適用した値を二重適用しない。

```cpp
// ドラッグや文字入力が終わった時点で、まとめて1件のUndoにする。
// RequestUndo等からも最初に呼ぶことで「編集の記録→Undo」の順番を保証する。
inline void FinishEdit() {
	if (Detail::pendingEdit.empty()) {
		return;
	}
	auto edits = std::move(Detail::pendingEdit);
	Detail::pendingEdit.clear();
	std::erase_if(edits, [](const auto& edit) { return edit.before == edit.after; });
	if (edits.empty()) {
		return; // 途中で動かしても元の値へ戻したなら履歴を作らない
	}
	auto apply = [edits](bool undo) {
		// 全項目を確認してから書く。途中でComponentが無いと一部だけ戻ってしまう。
		for (const auto& edit : edits) {
			if (!edit.available()) {
				return false;
			}
		}
		for (const auto& edit : edits) {
			edit.apply(undo ? edit.before : edit.after);
		}
		return true;
	};
	Detail::requests.push_back([apply] {
		Detail::Append({[apply] { return apply(true); }, [apply] { return apply(false); }});
	});
}

// Inspectorの選択変更では、前のEntityの操作を先に区切る。
inline void BeginInspector(Handle<Entity> handle) {
	Detail::batchHasChange = false;
	if (!Detail::pendingEdit.empty() && Detail::pendingEdit.front().target->current != handle) {
		FinishEdit();
	}
}

// 1フレームの編集前後を比較する。getterは呼ぶたびにHandleから項目を取り直す。
// 同じ項目をドラッグ中はbeforeを保持し、afterだけ更新する。
template<class T, class Getter>
void TrackValue(Handle<Entity> handle, const char* key, const T& before, const T& after, Getter getter) {
	if (before == after) {
		return;
	}
	if (!Detail::pendingEdit.empty() && Detail::pendingEdit.front().target->current != handle) {
		FinishEdit();
	}
	if (!Detail::batchHasChange) {
		// ポップアップを閉じたクリックで別項目を編集した場合も、前の操作と分ける。
		const bool sameProperty = std::any_of(Detail::pendingEdit.begin(), Detail::pendingEdit.end(),
			[key](const auto& edit) { return edit.key == key; });
		if (!sameProperty) {
			FinishEdit();
		}
		Detail::batchHasChange = true;
	}
	auto id = Detail::Find(handle);
	if (!id) {
		return;
	}
	for (auto& edit : Detail::pendingEdit) {
		if (edit.target == id && edit.key == key) {
			edit.after = after;
			return;
		}
	}
	Detail::pendingEdit.push_back({id, key, before, after,
		[id, getter] { return Detail::Alive(id) && getter(id->current) != nullptr; },
		[id, getter](const Detail::PropertyValue& value) {
			// availableで検証済み。同じフレーム境界内なのでここで寿命は変わらない。
			*getter(id->current) = std::get<T>(value);
		}});
}

// getterにメンバの位置を組み合わせる。生ポインタの保存はしない。
// Member(Member(GetModelRenderer, &ModelRendererComponent::material), &MaterialParams::roughness)
// のように組み合わせると、ネストした項目も同じ仕組みで扱える。
template<class Getter, class Owner, class T>
auto Member(Getter getter, T Owner::* member) {
	return [getter, member](Handle<Entity> handle) -> T* {
		Owner* object = getter(handle);
		return object ? &(object->*member) : nullptr;
	};
}

// Add Componentも1操作として登録する。実体の変更は既存のFlush内で行う。
inline void RequestAddModelRenderer(Handle<Entity> handle) {
	FinishEdit();
	Detail::requests.push_back([handle] {
		auto id = Detail::Find(handle);
		if (!id || EntityManager::GetModelRenderer(handle) || EntityManager::IsModelRendererAddPending(handle)) {
			return;
		}
		Detail::Execute({
			[id] {
				return Detail::Alive(id) && EntityManager::RemoveModelRendererAtBoundary(id->current);
			},
			[id] {
				if (!Detail::Alive(id) || EntityManager::GetModelRenderer(id->current)) {
					return false;
				}
				EntityManager::RequestAddModelRenderer(id->current);
				EntityManager::FlushComponentChanges();
				return EntityManager::GetModelRenderer(id->current) != nullptr;
			}});
	});
}
```

既存関数への変更は次の3つ。

- `Clear()` の先頭に `Detail::pendingEdit.clear();` を追加。
- `Detail::Failed()` の先頭に `pendingEdit.clear();` を追加。
- `RequestUndo / RequestRedo / RequestCreate / RequestDestroy / RequestRename / RequestReparent / RequestCopy / RequestPaste / RequestDuplicate` の**各関数本体の先頭**に `FinishEdit();` を追加（予約ラムダの中ではなく、その前）。

これが無いと、Inspectorで編集を終えたクリックでUndoを押したとき、「直前の編集を記録する前にUndoする」順序になる。
`WindowManager::UpdateAll()` の既存 `EditorHistory::Flush()` と、シーン切替時の `Clear()` はそのまま。

### 2-C. Add ComponentをUndoで取り外せるようにする

`EntityManager.h` のpublicに宣言を追加する。

```cpp
	// 配列の要素を移動するので、Update境界からのみ呼ぶ。存在しなければfalse。
	static bool RemoveModelRendererAtBoundary(Handle<Entity> handle);
```

`EntityManager.cpp` に定義を追加する。

```cpp
// Componentの配列が動くので、UIの走査中には呼ばない。
// EditorHistory::Flush（Update冒頭）からのUndoだけが使用する。
bool EntityManager::RemoveModelRendererAtBoundary(Handle<Entity> handle) {
	Entity* entity = Get(handle);
	if (!entity || IsModelRendererAddPending(handle) || !GetModelRenderer(handle)) {
		return false;
	}
	instance_->modelRenderers_.Destroy(entity->render);
	entity->render = {};
	return true;
}
```

## 3. Inspectorへの接続

### 3-A. 編集前後の値を比較する

`InspectorWindow.cpp` の標準includeに `<optional>`、自作includeに `MyEngine/Editor/EditorHistory.h` を追加。
既存の無名namespace内へ以下を追加する。

行列やEntity全体のスナップショットをUndoで書き戻す方法は採らない。
Positionを変更したらPositionだけを戻すので、他の処理で変わったMaterial等を巻き戻さない。

```cpp
// 1回のInspector描画の直前の値だけ保存する。worldMatrixは履歴へ入れない。
struct InspectorValues {
	bool active;
	TransformComponent transform;
	std::optional<ModelRendererComponent> model;
};

InspectorValues ReadInspectorValues(Handle<Entity> handle) {
	InspectorValues values{EntityManager::Get(handle)->isActive, *EntityManager::GetTransform(handle), {}};
	if (const auto* model = EntityManager::GetModelRenderer(handle)) {
		values.model = *model;
	}
	return values;
}

// 編集された項目だけを登録する。例えばPositionのUndoでMaterialを戻さない。
void TrackInspectorValues(Handle<Entity> handle, const InspectorValues& before, const InspectorValues& after) {
	using EditorHistory::Member;
	using EditorHistory::TrackValue;
	TrackValue(handle, "Entity.Active", before.active, after.active,
		Member(EntityManager::Get, &Entity::isActive));

	auto transformField = [&](const char* key, auto member) {
		TrackValue(handle, key, before.transform.*member, after.transform.*member,
			Member(EntityManager::GetTransform, member));
	};
	transformField("Transform.Position", &TransformComponent::translation);
	transformField("Transform.Rotation", &TransformComponent::rotation);
	transformField("Transform.Scale", &TransformComponent::scale);

	if (!before.model || !after.model) {
		return; // 追加はRequestAddModelRendererが別の操作として担当する
	}
	const auto& oldModel = *before.model;
	const auto& newModel = *after.model;
	auto modelField = [&](const char* key, auto member) {
		TrackValue(handle, key, oldModel.*member, newModel.*member,
			Member(EntityManager::GetModelRenderer, member));
	};
	modelField("Model.Enabled", &ModelRendererComponent::enabled);
	modelField("Model.Asset", &ModelRendererComponent::modelHandle);
	modelField("Model.Texture", &ModelRendererComponent::textureHandle);
	modelField("Model.Color", &ModelRendererComponent::color);
	modelField("Model.Shading", &ModelRendererComponent::shadingType);
	modelField("Model.Blend", &ModelRendererComponent::blendMode);
	modelField("Model.Rasterizer", &ModelRendererComponent::rasterizerType);
	modelField("Model.Depth", &ModelRendererComponent::depthMode);
	modelField("Model.Billboard", &ModelRendererComponent::billboard);

	auto material = Member(EntityManager::GetModelRenderer, &ModelRendererComponent::material);
	auto materialField = [&](const char* key, auto member) {
		TrackValue(handle, key, oldModel.material.*member, newModel.material.*member, Member(material, member));
	};
	materialField("Material.Ambient", &MaterialParams::ambient);
	materialField("Material.Diffuse", &MaterialParams::diffuse);
	materialField("Material.Specular", &MaterialParams::specular);
	materialField("Material.Emissive", &MaterialParams::emissive);
	materialField("Material.Shininess", &MaterialParams::shininess);
	materialField("Material.Metallic", &MaterialParams::metallic);
	materialField("Material.Roughness", &MaterialParams::roughness);
	materialField("Material.AlphaCutoff", &MaterialParams::alphaCutoff);

	auto uv = Member(EntityManager::GetModelRenderer, &ModelRendererComponent::uvTransform);
	auto uvField = [&](const char* key, auto member) {
		TrackValue(handle, key, oldModel.uvTransform.*member, newModel.uvTransform.*member, Member(uv, member));
	};
	uvField("UV.Offset", &Transform::translation);
	uvField("UV.Rotation", &Transform::rotation);
	uvField("UV.Tiling", &Transform::scale);
}
```

### 3-B. `InspectorWindow::Draw()` を差し替える

既存の `DrawHeader / DrawTransform / DrawModelRenderer / DrawModelPicker / DrawAddComponent` の本体は、次の3-C以外はそのまま。

```cpp
void InspectorWindow::Draw() {
	if (!ImGui::Begin("Inspector")) {
		EditorHistory::FinishEdit(); // 折り畳んだときも編集中の履歴を確定する
		ImGui::End();
		return;
	}
	const Handle<Entity> handle = HierarchyWindow::GetSelected();
	EditorHistory::BeginInspector(handle);
	if (!EntityManager::IsAlive(handle)) {
		EditorHistory::FinishEdit();
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	// このDrawの中のUI変更だけを比べる。ゲームのUpdateの変化は取り込まない。
	const InspectorValues before = ReadInspectorValues(handle);
	DrawHeader(handle);
	ImGui::Separator();
	DrawTransform(handle);
	ImGui::Separator();
	DrawModelRenderer(handle);
	DrawAddComponent(handle);
	TrackInspectorValues(handle, before, ReadInspectorValues(handle));

	// カラーピッカーはマウスを離しても開いている。閉じるまでを1操作にする。
	const bool popupOpen = ImGui::IsPopupOpen(nullptr,
		ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (!ImGui::IsAnyItemActive() && !popupOpen) {
		EditorHistory::FinishEdit();
	}

	// Hierarchyへ移動しなくてもUndoできる。文字/数値の直接入力中はImGuiへ任せる。
	const ImGuiIO& io = ImGui::GetIO();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		!io.WantTextInput && !ImGui::IsAnyItemActive() && !popupOpen && io.KeyCtrl && !io.KeyAlt) {
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			if (io.KeyShift) {
				EditorHistory::RequestRedo();
			} else {
				EditorHistory::RequestUndo();
			}
		} else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			EditorHistory::RequestRedo();
		}
	}
	ImGui::End();
}
```

### 3-C. 既存の小さな変更

1. 同じcppの `kComponentMenu` のModel Renderer追加ラムダ内で、`EntityManager::RequestAddModelRenderer(handle);` を `EditorHistory::RequestAddModelRenderer(handle);` に変更する。履歴を通さないと追加だけUndo対象から漏れる。
2. `DrawHeader` の名前入力コメントだけが残っている箇所には `ImGui::TextUnformatted(entity->name.c_str());` を追加して名前を表示する。名前の変更入口はHierarchyに統一し、今回Inspectorへ別の入力状態を増やさない。
3. ショートカットは今のHierarchy側も残す。フォーカスされたウィンドウだけが受けるので二重実行しない。

## 4. 確認と次の作業

### 既に行った検証

- 一時ディレクトリに変更後コードを組み立て、実物のEntityManagerとSlotMapで履歴を実行テスト。
- 100回のドラッグ更新が1件になる／元の値へ戻すと0件／別項目を巻き戻さない／Resetを1件にする／選択変更で分ける／編集直後のUndoの順序を確認。
- Add Component→値編集→Undo×2→Redo×2、複製→Inspector編集→Undo×2→Redo×2を確認（世代が変わったHandleに追従）。
- Inspectorの変更はMSVC `/W4 /Zs` で通過。Win32変更も通過（既存のself変数隠蔽警告あり）。
- テストはログと行列計算を代替しており、描画・Windows IMEの実入力・エンジン全体のリンクは未検証。**本体の実装ファイルは変更していない。**

### 写経後に確認する順番

1. エンジンDebug/Release → ゲームをビルド。ヘッダー配布先も更新する（クラスにメンバを追加したため両方必要）。
2. F2で英数、日本語の未確定入力、変換、Enterで変換確定、次のEnterで名前確定。Escは変換取消と名前取消を分けて確認。候補のクリックと別ウィンドウへの移動も確認。
3. Positionを数秒ドラッグ→Ctrl+Z 1回で開始位置→Redoで終了位置。回転の度表示、Scale、Resetも確認。
4. Model、各enum、Enabled、isActive、色、Material、UVを編集してUndo。色のポップアップは閉じるまで1操作。
5. Add Componentと複製を交えたUndo/Redo。削除復元後もInspectorの履歴が同じ対象を指すことを確認。
6. Undo後の新規編集でRedoが消える。数値を手入力中のCtrl+Zは文字編集を優先。Stop/Restartで履歴をクリアする。

次は、この入力・Inspector操作を確認してから **Model.md Step 2（モデルのノード階層をEntityへ）→ Light.md Step 6（Entityとライトの連携）**へ進む。

## 残す設計記録

| 方針 | 理由・現在の仕様 |
|---|---|
| EntityはHandle、Component実体は型別SlotMap | 将来の連続配置へつなげる。取得ポインタは同フレーム内で使い捨て |
| 全EntityがTransformを持つ | 親子行列の処理を共通化。UIは将来UITransformを追加 |
| データはModelRendererComponent、処理はModelRenderSystem | Renderer::ModelConfigのポインタや文字列をComponentへ保存しない |
| 追加はフレーム境界、1EntityにModelRendererは1個 | UI走査中の再確保を避ける。モデル内のノード分割はModel.mdで扱う |
| IBLはシーン所有、Drawへ一時的に渡す | GPUリソースはGPU使用完了まで生存。PBRはIBL無しなら描画しない |
| シーンのEntityはsceneRoot配下、Finalizeで親を破棄 | 再起動で増殖させない。Hierarchyのroot直下のEntityは現在は別管理 |
| 現行Drawのroot={}は全Entity対象 | 複数シーンへ同じEntityを重複描画しない所属管理は未解決 |
| BillboardはNone/Full/AxisY、VSで頂点zも含めて回す | Scene/Gameそれぞれのカメラへ向け、3Dモデルを板に潰さない。Primitiveのbool指定はFullへ変換 |
| Add Componentはカテゴリenum＋検索、モデル一覧はModel RendererのInspector内 | 最小のアセット選択を優先。メニューは `ComponentEditorRegistry` から作る（2026-09-18） |
| 日本語はUTF-8のstd::string＋Meiryo動的フォント | 固定長バッファで切らない。未収録の字形まで保証しない |
| 変換中の文字はImGuiの入力欄の中に自分で描く（`ImeInput`）。候補の一覧はWindows | 入力欄と同じ見た目で、IMEの種類に左右されない。Unity・Unrealも同じ分担。Windowsの変換窓は `ISC_SHOWUICOMPOSITIONWINDOW` を外して出さない（2026-09-18） |
| メッセージは `PeekMessage(&msg, nullptr, …)` でスレッドの分を全部取り出す | ウィンドウを指定すると、IMEなどWindowsが作ったウィンドウ宛てのメッセージが残り続ける。変換窓が出なかった原因の有力候補（2026-09-18） |
| Componentごとのエディタの扱いは `ComponentEditor`（UnityのCustomEditor）。`TypedComponentEditor<T>` を継承して登録するだけ | Inspector・Add Component・Undo・コピーがComponentの型を知らずに済む。Componentを増やしても `EditorHistory`・`InspectorWindow` は変えない（2026-09-18） |
| Componentの編集の記録は「Inspectorが描く前と後のバイト比較」、4バイト単位 | 項目の手書き（旧 `TrackInspectorValues`）をやめる。Inspectorの外（ゲームの `Update`）の変化は記録しない。floatを半分だけ戻さない（2026-09-18） |
| Componentは `std::is_trivially_copyable_v` を満たす | バイト列で写してUndo・コピーするため。`TypedComponentEditor` の `static_assert` で守らせる |
| Entityは `EntityId`（消して作り直しても変わらない番号）を持つ。履歴は番号で相手を覚える | 削除のUndoでHandleが変わっても、前の履歴が同じ相手を見つけられる。将来のシーン保存の親子の書き出しにも使う（2026-09-18） |
| Componentの追加も取り外しも「予約 → `FlushComponentChanges`」 | UIの一覧を回している途中で配列を動かさない。Undoからは予約の直後にFlushを呼ぶ（フレームの境目なので安全） |
| 履歴の変更はすべて `EditorHistory::Flush`（フレームの最初）で、頼まれた順に行う | 描画中に構造を変えない。「編集の記録 → Undo」の順番が入れ替わらない |
| コピーした物は `EditorHistory::Clear`（シーンの作り直し）で消さない | Play中にコピーして、Stopの後に貼れる（Unityと同じ） |
| コピーの名前は `名前 (CopyN)`、同じ親の下で空いている一番小さいN | コピーのコピーで `(Copy1) (Copy1)` と伸びない |
| Undo/RedoはEditメニューとCtrl+Z / Ctrl+Y（どのウィンドウでも）。コピー・貼り付け・削除のキーはHierarchyを触っているときだけ | 文字の入力中とドラッグ中は、入力欄のUndoを優先する |
| Editorのフォルダは History / Inspector / Viewport / Widgets / Windows | Componentが増えると増えるファイル（Inspector）と、あまり増えないファイルを分ける（2026-09-18） |
| Gameは製品の絵、Sceneは編集用 | Gameはポストエフェクトあり・ギズモなしが目標。ギズモ混入とビュー別の半透明ソートは未解決 |
| ライトは種類別Component、ギズモは全体AND個別 | 詳細と進捗はLight.mdへ集約 |
| Pauseとシーン再生成は実装あり | 編集内容を保存してPlay後に元へ戻す機能は、シリアライズができてから |
| Undoはゲームの時間を戻す機能ではない | 編集した値だけを記録。削除前のゲーム側Handleは無効のまま（ゲームが覚え続けたいなら `EntityId` で覚える） |
| ファイルを増やすのは大きな話題の区切りだけ | 完了した写経コードは実装へ、理由はこの表へ。古い「追加する／消す」の抜け殻は残さない |

未完了：Sceneギズモ分離、半透明のビュー別ソート、ライト連携、シリアライズ、ImGuizmo、シーン所属。
ゲーム側MonsterBall移行を確認する場合は、シーン所有IBL・sceneRootの破棄・ModelRenderSystem::DrawへのIBL引数を確認する。旧GameScene全文は再掲しない。
補足：imgui_style.iniはSaveStyle/LoadStyleにある項目だけを保存し、色はリニア。ヘッダー配布のxcopyは削除済みヘッダーを消さないため、改名時の古い配布ファイルに注意（`robocopy /PURGE` への置き換えは「まとめて」のA-4）。

次の候補（2026-09-18時点）：
- **段階3：`EntityManager` を型で引ける形にする**（`Get<T>` / `RequestAdd<T>`）。ゲーム固有のComponentの実体を置けるようになり、今回の `ComponentEditor` と組み合わせて、ゲーム側でComponentを作ってInspectorで触れるようになる。Light.md Step 6（ライトのComponent化）の前にやると、ライトもこの形に乗せられる
- Model.md Step 2（モデルのノード階層をEntityへ）
- Compute.md CS-1（CSの土台＋BRDF LUT）
