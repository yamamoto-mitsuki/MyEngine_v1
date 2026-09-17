# Editor系の作業

> Step 3の写経手順は [RenderComponent.md](RenderComponent.md) に分けて記載する。
> まず3a（データ・追加予約・破棄）から進める。2026-09-18時点では手順作成のみで、実装・ビルド・実行確認は未完了。
> Codexでの作業はMarkdownへのコード記載方式。作業ブランチは `codex/component-render-guide`、Pushは禁止。

## このファイルについて
- エディタ（ヒエラルキー、Inspector、ギズモの表示など）の設計と作業手順を書く。
- 決まったことは「決めたこと」の表に理由と一緒に追記する。消さずに更新し続ける。

---

## 目指す操作（ユーザーの設計案）
1. ヒエラルキーでEntityを選ぶ（例：「Light」。作った直後はComponentが何も付いていない）
2. Inspectorの **Add Component** から `PointLight` / `SpotLight` などを選んで付ける
3. 付けたComponentの欄で、次の3つを編集する
   - Componentの値（色、強さ、半径など）
   - 線（光の範囲のワイヤー）の表示
   - アイコンの表示

## 前提として必要なもの（まだ無い）
| 必要なもの | 内容 |
|---|---|
| Entity（GameObject） | ARCHITECTURE.md 段階1：Componentを所有する入れ物 |
| TransformComponent | 位置・回転・拡縮。ライトの位置はここから取るようにする |
| ヒエラルキーウィンドウ | Entityの一覧、作成、選択 |
| Inspectorウィンドウ | 選択中のEntityのComponent一覧と編集 |
| Componentの種類の登録 | Add Componentの選択肢を出すための一覧 |

## 他の作業との順番
`Light.md` のStep 2〜5.5 → **この作業** → `Light.md` のStep 6（ライトのComponentをInspector / Add Componentに対応）

---

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| ライトの種類ごとに別のComponentにする | Add Componentで種類を選ぶ操作に合う。詳細は `Light.md` |
| ギズモの表示設定はComponentの中に持ち、「全体 AND 個別」で判定する | 詳細は `Light.md` の「決めたこと」 |
| **Gameビュー＝製品版で見える絵**（ゲームのカメラ、ポストエフェクトあり、ギズモなし） | 製品版と同じ見た目をエディタ上で確認するため。Bloomなどの演出はここで確認する |
| **Sceneビュー＝編集用**（デバッグカメラ、ギズモあり） | 配置や範囲を見ながら編集するため。今はポストエフェクトなし（Unityのように切り替えられるようにしてもよい） |
| 全EntityがTransformComponentを必ず持つ（2026-09-18） | Unityも全GameObjectがTransformを持つ形。位置が要らないUIやマネージャ的なEntityでも、持っているだけなら112バイトで害が無い。逆に「無いかもしれない」形にすると、親子の行列計算に毎回「無かったとき」の分岐が要る |
| UI（Sprite）は、後で `UITransformComponent`（画面座標・アンカー・サイズ）を別のComponentとして足す（2026-09-18） | 3DのTransformとUIの位置決めはルールが違う（UnityもRectTransformを分けている）。SpriteのComponentは「UITransformがあればそれを見て、無ければTransformを見る」形にできる。TransformComponentを共用して無理に画面座標を入れない |
| 独立した「アセットブラウザ」は作らず、`RenderComponent`（Step 3）の中に最小の「モデル選択」を入れる（2026-09-18） | 本当に必要なのは「再ビルドせずにアセットを選べること」だけ。サムネイル・フォルダツリー・D&D・インポート設定は後から育てられる。詳細は下の「アセットブラウザ」 |
| Play / Stop / Reset は、シーンの保存・復元ができてから作る（2026-09-18） | Unityで「Play中に変えた値が戻る」のは保存・復元をしているから。ボタン自体は10行だが、Componentが揃う前にシリアライズを書くと、Componentを足すたびに書き直しになる。Pause / Step / 速度は `Time::SetTimeScale` があるので先に入れてよい |
| ゲーム側の `MonsterBall` クラスは、Step 3（`RenderComponent`）ができたら Entity に置き換えて消す（2026-09-18） | 今はIBL（`IBLEnvironment`）を試している唯一の場所なので、先に消すとPBR / IBLの確認手段が無くなる。`RenderComponent` が `env` を持てるようになってから移す |

## 未解決
- **Gameビューにもギズモ（ライトのアイコン、範囲の線、カメラの視錐台）が出る。さらにギズモ自体にBloom / Lensがかかる**
  - 原因1：`EditorViewport::Render` がSceneビューとGameビューで**同じRenderQueueを2回描いている**。キューに「Sceneビューだけ」という区別が無い。
  - 原因2：ギズモは3Dの描画（`SceneRenderer::RenderWorld`）の中で描かれるので、**ポストエフェクトより前**に入る。そのため、ギズモの線やアイコンが光ったり（オレンジの線はしきい値0.75を超えるので光る）、Lensで歪んだりする。
  - GameビューにBloomがかかること自体は正しい。問題なのは「ギズモ」が「ゲームの演出」の影響を受けていること。
  - 方向性：
    1. 描画リクエストに「エディタ専用」の印を付け、Gameビューを描くときは飛ばす
    2. ギズモはUI（`RenderUI`）と同じく、ポストエフェクトの**後**に描く。ただし奥の物体に隠れるようにするには、3Dを描いたときの深度バッファ（`postRT_` の深度）を、出力先に描くときにも使えるようにする必要がある
    3. Unityのように、Gameビューでもギズモを出すかをトグルで選べるようにする場合も、2の順番なら演出の影響を受けない
- **Sceneビューで、半透明の前後の並びがおかしくなることがある**（2026-09-17、`Light.md` Step 5.1a で発見）
  - 原因：奥から描くための距離（`MeshRequest::cameraDistanceSq`）を、リクエストを作るときに `config.camera` で1回だけ計算している。キューはSceneビューとGameビューで共有なので、Sceneビュー（デバッグカメラ）でもゲームのカメラからの距離で並ぶ。
  - 方向性：距離ではなく位置をリクエストに持たせ、`FlushTransparentMesh` でそのビューのカメラから距離を計算して並べ替える。上の「ビューの区別」と一緒にやる。

---

## 予定

| Step | 作業 | 状態 |
|---|---|---|
| 1 | Entity（GameObject）と TransformComponent、EntityManager、ヒエラルキーウィンドウ | 完了（2026-09-18 実行して確認済み） |
| 2 | Inspectorウィンドウ（Transformの編集、Add Componentの置き場所） | 完了（2026-09-18 実行して確認済み） |
| **3** | **`RenderComponent`（Entityがモデルを描く）＋ モデル選択（最小のアセット一覧）。この後 `Model.md` Step 2でモデルのノード階層をEntityとして取り込む** | **次にやる** |
| 4 | ライトのComponentをEntityに載せる（`Light.md` Step 6） | 3の後 |
| 5 | ヒエラルキー・Inspectorのアイコン（Entityの種類、`isActive`）。下の「アイコンと視認性」 | 4の後（Componentが揃ってから） |
| 6 | ツールバーに Pause / Step / 速度（`Time::SetTimeScale`。**Play / Stopより先にできる**） | いつでも |
| 7 | シーンの保存・復元（シリアライズ）→ その上で Play / Stop / Reset | 4の後 |
| 8 | Sceneビューでの操作（ImGuizmoで移動・回転・拡縮。`externals/imgui/ImGuizmo.cpp` が既に入っている） | 将来 |
| 9 | Gameビューにギズモを出さない（下の「未解決」） | 将来 |

---

## ImGuiのスタイル（`imgui_style.ini`）

- 色とサイズは `imgui_style.ini` に保存され、`ImGuiManager::LoadStyle` / `SaveStyle` が読み書きする。**この2つの関数に書いてある項目だけ**が保存・復元される（書いていない項目はImGuiの既定値になる）。
- **色の値はリニア**。ImGuiはsRGBのレンダーターゲット（`DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`）に描くので、書いた数値より明るく表示される。スタイルエディタの見た目と数値が合わないのはこれが理由。カラーコードから作るときは sRGB → リニアに変換する。
- **選択中のタブの上に出る線を太くする**：太さは `ImGuiStyle::TabBarOverlineSize`（既定2.0）。色は `ImGuiCol_TabSelectedOverline`（`Color_38`）。iniの項目として保存できるようにするには、次の2か所を足す。

`MyEngine/Editor/ImGuiManager.cpp` の `SaveStyle`（`TabRounding` の行の下）
```cpp
	fprintf(f, "TabBarOverlineSize=%.3f\n", s.TabBarOverlineSize);
	fprintf(f, "TabBarBorderSize=%.3f\n", s.TabBarBorderSize);
```

同じファイルの `LoadStyle`（`TabRounding` を読む所の下）
```cpp
		if (sscanf_s(line, "TabBarOverlineSize=%f", &v0) == 1) {
			s.TabBarOverlineSize = v0;
		}
		if (sscanf_s(line, "TabBarBorderSize=%f", &v0) == 1) {
			s.TabBarBorderSize = v0;
		}
```

`imgui_style.ini`（`TabRounding=4.000` の下。値を変えれば太さが変わる）
```ini
TabBarOverlineSize=6.000
TabBarBorderSize=2.000
```

---

## アイコンと視認性（2026-09-18の検討）

### ImGuiには標準のアイコンが無い
ImGuiが最初から持っているのは、ツリーの矢印（`▶`/`▼`）とチェックマークくらい。それ以外の絵は自分で用意する。方法は4つ。

| 方法 | 使うもの | 向いているもの | 手間 |
|---|---|---|---|
| **A. `ImDrawList` で自分で描く** | `AddTriangleFilled` / `AddRectFilled` | Play（▲）、Pause（‖）、Stop（■）のような単純な形 | **いちばん軽い。ファイル追加ゼロ** |
| **B. 文字（グリフ）を使う** | フォントのグリフ範囲を広げる | `▶ ■ ● ◆ ↺` など | 小（`ImGuiManager` を1行変えるだけ） |
| **C. アイコンフォントを合成** | Font Awesome 等の `.ttf` ＋ `IconFontCppHeaders` | Componentのアイコン、メニューの絵 | 中（.ttf をリポジトリに追加） |
| **D. テクスチャ** | `ImGui::Image` / `ImGui::ImageButton` | 色付きアイコン、サムネイル | 中（**この機能はもう動いている**） |

**Dはもう使える**：ゲーム側の `MonsterBall.cpp` が `ImGui::Image((ImTextureID)srv.ptr, size)` でキューブマップを表示している。`TextureManager::Load` したPNGを同じやり方でボタンにできる。`LightGizmo` のアイコンPNGもそのまま流用できる。

**Bの注意点**：今のフォント読み込みは
```cpp
io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
```
`GetGlyphRangesJapanese()` が持っている範囲は `0x0020-0x00FF` / `0x2000-0x206F` / `0x3000-0x30FF` / `0x31F0-0x31FF` / `0xFF00-0xFFEF` ＋ 常用漢字。
**`▶`(U+25B6) や `■`(U+25A0) は `0x25A0-0x25FF` なので入っていない**（今書いても豆腐になる）。使うならグリフ範囲を自分で作って足す。

### あったら効くアイコン（優先度つき）

| 優先 | 場所 | アイコン | なぜ効くか | いつやるか |
|---|---|---|---|---|
| **高** | ヒエラルキー | Entityの種類が分かる小さな印（モデル／ライト／カメラ） | 名前が「Entity」「Child」だけだと、何が入っているか開かないと分からない | **`RenderComponent` ができてから**（Component無しだと区別する物が無い） |
| **高** | ヒエラルキー | 非表示（`isActive == false`）のEntityを薄く出す＋目のアイコン | Unityと同じ。今は `isActive` を切っても見た目が変わらない | 同じく後 |
| 中 | Inspector | Component名の前のアイコン | 区画が増えたときに目で探せる | Componentが3種類を超えたら |
| 中 | ツールバー | **Play / Pause / Step / Stop** | 下に別項目 | 後述 |
| 中 | ツールバー | 速度（1x / 0.5x / 0.25x） | **`Time::SetTimeScale` がもうあるので今日できる** | いつでも |
| 低 | 全体 | 保存・読み込み・ゴミ箱 | 文字ボタンでも困らない | 将来 |

### Play / Pause / Reset は「ボタン」ではなく「状態」の話
ボタンを置くのは10行で済むが、**押したときに何が起きるべきか**を先に決める必要がある。

| ボタン | 必要なもの | 今できるか |
|---|---|---|
| **Pause** | ゲームの時間を止める | **できる**。`Time::SetTimeScale(0.0f)`。ただしゲーム側が `Time::GetDeltaTime()`（スケール付き）を使っていること。`GetUnscaleDeltaTime()` を使っている所は止まらない |
| **Step（1フレーム進める）** | 「次の1フレームだけ `timeScale` を戻す」フラグ | できる |
| **速度変更** | `Time::SetTimeScale(0.5f)` など | **できる** |
| **Play / Stop（Unityと同じ意味）** | 再生前のシーンを保存し、停止時に**元に戻す** | **できない**。シーンの保存・復元（シリアライズ）が必要 |
| **Reset** | 同上（保存した状態へ戻す） | **できない**（同上） |

- Unityで「Play中に変えた値が戻る」のは、まさにこの保存・復元をやっているから。
- シリアライズは**Componentが揃ってから書くべき**（Transformだけの今書くと、`RenderComponent`・ライトを足すたびに書き直しになる）。
- なので順番は **RenderComponent → ライトのComponent → シーンの保存・復元 → Play / Stop**。
- 先に入れて損しないのは **Pause / Step / 速度**。これは今のうちに入れても後で書き直さない。

---

## アセットブラウザ（ファイル表示）は今やるべきか（2026-09-18の検討）

「他の人のエンジンにあるファイル一覧のウィンドウは何のためか」という疑問について。

### 何のためにあるのか
| 目的 | 中身 |
|---|---|
| **① コードを書き換えずにアセットを選ぶ** | いちばん本質。`ModelManager::Load("resources/...")` がコードに直書きだと、モデルを変えるたびに**再ビルド**になる。エディタで選べれば実行中に差し替えられる |
| ② Inspectorの相手役 | `RenderComponent` に「どのモデルか」を入れるとき、パスを手打ちさせるとタイプミスで落ちる。一覧から選ばせれば間違えない |
| ③ ドラッグ＆ドロップで置く | 一覧からSceneビューへドロップしてEntityを作る（ヒエラルキーのD&Dはもう動いているので、仕組みは同じ） |
| ④ 中身の確認 | サムネイル、テクスチャのサイズ、頂点数。「このモデル重すぎない？」が一目で分かる |
| ⑤ アセット＝ファイル＋設定 の置き場 | 「このモデルはFlipUVsする」「このテクスチャはsRGB扱い」といった**インポート設定**を保存する場所。`Model.md` Step 1a で踏んだような形式ごとの違いは、最終的にここに逃がす |

「保存しているモデルやTextureの位置くらいしかメリットが無いのでは」という感覚は半分正しい。**フル機能のブラウザ（サムネイル、フォルダツリー、D&D、インポート設定）は今は要らない。** でも**①②の最小版は `RenderComponent` を作る瞬間に必要になる**。

### 今やるべきこと＝「モデルを選ぶコンボボックス」だけ
```
resources/ 以下を std::filesystem::recursive_directory_iterator で走査
 → 拡張子が .obj / .gltf / .glb のものだけ集める
 → ImGui::Combo で選ばせる → 選ばれたら ModelManager::Load() → ハンドルをRenderComponentへ
```
- これで**再ビルド無しでモデルを差し替えられる**（①）し、パスの手打ちも無くなる（②）。
- 走査は起動時1回＋「Refresh」ボタンでよい（毎フレームやるとディスクを叩き続ける）。
- フォルダツリーもサムネイルも後回し。必要になったらこの一覧を育てる。

**結論：独立した「アセットブラウザ」は今は作らない。`RenderComponent`（Step 3）の中に「モデル選択」として最小版を入れる。**

---

## Step 1：Entity と TransformComponent

### 目的
1. `Entity`（UnityのGameObject）と `TransformComponent` を作る。ARCHITECTURE.md 段階1の「入れ物」を用意する。
2. `EntityManager` が実体を持ち、外にはHandleを渡す（LightManagerと同じ形）。
3. ARCHITECTURE.md の更新順序のうち **2（ワールド行列を親→子で更新）** と **6（破棄の反映）** を実装する。
4. ヒエラルキーウィンドウで、作成・削除・選択・親子の付け替え（ドラッグ）ができるようにする。

### 設計の形
```
EntityManager
 ├ SlotMap<Entity>            … 名前、親のHandle、TransformのHandle
 └ SlotMap<TransformComponent> … 位置・回転・大きさ・ワールド行列

Entity（GameObject）
 ├ name         "Player"
 ├ parent       Handle<Entity>（無効ならroot）
 ├ transform    Handle<TransformComponent>（全Entityが必ず持つ）
 └ self         Handle<Entity>（一覧から選ぶときに使う）
```
- Componentの実体はEntityが持たず、**型別のSlotMapが持つ**。Entityが持つのはHandleだけ。ARCHITECTURE.md 段階2〜3（型別の連続配列）にそのまま進める形にしてある。
- ライトと同じで、**フレームをまたいで持つのはHandleだけ**。`Get` で受け取ったポインタは使い捨てにする。
- 親子は「子が親のHandleを持つ」形（子のリストは持たない）。子を探すときは全Entityを見る。数百個までなら問題にならないので、まずは単純にする。

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Entity/TransformComponent.h` | **新規** |
| ② | `MyEngine/Entity/Entity.h` | **新規** |
| ③ | `MyEngine/Entity/EntityManager.h` | **新規** |
| ④ | `MyEngine/Entity/EntityManager.cpp` | **新規** |
| ⑤ | `MyEngine/Editor/HierarchyWindow.h` | **新規** |
| ⑥ | `MyEngine/Editor/HierarchyWindow.cpp` | **新規** |
| ⑦ | `MyEngine/Engine.cpp` | `EntityManager::Initialize` / `Release` |
| ⑧ | `MyEngine/Window/WindowManager.cpp` | `UpdateTransforms`（更新順序2）と `FlushDestroy`（6） |
| ⑨ | `MyEngine/Editor/ImGuiManager.cpp` | `HierarchyWindow::Draw` を呼ぶ |

- 新規ファイルはVisual Studioのソリューションエクスプローラーに追加する（`Entity` フィルタを新しく作る）。
- ゲーム側は変更なし（まだEntityを使わなくても動く）。

> 確認済み（2026-09-18）：この変更を入れた状態で、エンジンの全69個の `.cpp` をDebug / Releaseでコンパイルして通る（`/W4`。新しい警告なし）。

---

### ① `MyEngine/Entity/TransformComponent.h`（新規）
```cpp
#pragma once
#include "MyEngine/Math/MathIncludes.h"

// Inspectorで編集するEntityの位置・回転・大きさ
// ARCHITECTURE.md 原則2：ポインタ・可変長の型・仮想関数・GPUリソースを持たない


/// <summary>
/// 位置・回転・大きさ
/// <para>worldMatrix は EntityManager::UpdateTransforms が毎フレーム計算する（親 → 子の順）</para>
/// </summary>
struct TransformComponent {
	Vector3 translation = {0.0f, 0.0f, 0.0f}; // 位置（親から見た位置）
	Vector3 rotation = {0.0f, 0.0f, 0.0f};    // 回転（ラジアン。X→Y→Zの順）
	Vector3 scale = {1.0f, 1.0f, 1.0f};       // 大きさ
	Matrix4x4 worldMatrix = MakeIdentity4x4(); // 計算結果（親の行列まで掛けたもの）
};
```

### ② `MyEngine/Entity/Entity.h`（新規）
```cpp
#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// シーンに置くもの1個（UnityのGameObject）
/// <para>Componentそのものは持たず、Handleで指す（実体はそれぞれのManagerが型別に持つ）</para>
/// <para>ARCHITECTURE.md 段階3でEntityが「ただのID」になったら、name はEditor用の別の表に移す</para>
/// </summary>
struct Entity {
	std::string name = "Entity";              // ヒエラルキーに出す名前
	Handle<Entity> self;                      // 自分を指すHandle（一覧から選ぶときに使う）
	Handle<Entity> parent;                    // 親。無効なら一番上（root）
	Handle<TransformComponent> transform;      // 全Entityが必ず1つ持つ
	bool isActive = true;                     // falseで更新・描画の対象から外す（使うのは後のStep）
};
```

### ③ `MyEngine/Entity/EntityManager.h`（新規）
```cpp
#pragma once
#include <string>
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
	/// 破棄を予約する。実際に消えるのはフレームの最後（子も一緒に消える）
	/// </summary>
	static void Destroy(Handle<Entity> handle);

	// ===== 取得（受け取ったポインタは使い捨てにする。ARCHITECTURE.md 原則1） =====
	static Entity* Get(Handle<Entity> handle);
	static TransformComponent* GetTransform(Handle<Entity> handle);
	static bool IsAlive(Handle<Entity> handle);

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
	std::vector<Handle<Entity>> pendingDestroy_; // 破棄予約
	// 毎フレーム使う作業用の配列（確保し直さないようにメンバで持つ）
	std::vector<std::pair<uint32_t, Handle<Entity>>> updateOrder_; // (深さ, Handle)
	std::vector<Handle<Entity>> destroyWork_;
};
```

### ④ `MyEngine/Entity/EntityManager.cpp`（新規）
```cpp
#include "EntityManager.h"

#include <algorithm>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"

// 静的メンバ変数
EntityManager* EntityManager::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
void EntityManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new EntityManager();
	LogManager::Log("Initialized");
}

void EntityManager::Release() {
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// 生成・破棄
//=============================================================================
Handle<Entity> EntityManager::Create(const std::string& name, Handle<Entity> parent) {
	// Transformは全Entityが必ず持つので、一緒に作る
	Handle<TransformComponent> transform = instance_->transforms_.Create();
	Handle<Entity> handle = instance_->entities_.Create();

	Entity* entity = instance_->entities_.Get(handle);
	MY_ASSERT_MSG(entity, "作った直後のEntityが取れませんでした");
	entity->name = name;
	entity->self = handle;      // 一覧から選ぶときに使う
	entity->transform = transform;
	// 親は SetParent を通す（輪になっていないかを見るため）
	if (parent.IsValid()) {
		SetParent(handle, parent);
	}
	return handle;
}

void EntityManager::Destroy(Handle<Entity> handle) { instance_->pendingDestroy_.push_back(handle); }


//=============================================================================
// 取得
//=============================================================================
Entity* EntityManager::Get(Handle<Entity> handle) { return instance_->entities_.Get(handle); }

TransformComponent* EntityManager::GetTransform(Handle<Entity> handle) {
	Entity* entity = instance_->entities_.Get(handle);
	if (!entity) {
		return nullptr;
	}
	return instance_->transforms_.Get(entity->transform);
}

bool EntityManager::IsAlive(Handle<Entity> handle) { return instance_->entities_.IsAlive(handle); }

size_t EntityManager::GetCount() { return instance_->entities_.Size(); }


//=============================================================================
// 親子
//=============================================================================
void EntityManager::SetParent(Handle<Entity> child, Handle<Entity> parent) {
	Entity* entity = instance_->entities_.Get(child);
	if (!entity) {
		return;
	}
	// 自分自身、または自分の子孫を親にすると、親子が輪になって無限ループする
	if (child == parent || IsDescendantOf(parent, child)) {
		LogManager::Warning("親子が輪になる付け替えは無視しました");
		return;
	}
	entity->parent = parent;
}

bool EntityManager::IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor) {
	if (!ancestor.IsValid()) {
		return false;
	}
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = instance_->entities_.Get(current);
		if (!entity) {
			return false;
		}
		if (entity->parent == ancestor) {
			return true;
		}
		current = entity->parent;
	}
	return false;
}

void EntityManager::GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (entity.parent == handle) {
			out.push_back(entity.self);
		}
	}
}

void EntityManager::GetRoots(std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (!entity.parent.IsValid()) {
			out.push_back(entity.self);
		}
	}
}


//=============================================================================
// 更新順序2：ワールド行列（親 → 子）
//=============================================================================
void EntityManager::UpdateTransforms() {
	EntityManager& self = *instance_;

	// --- 深さ（親をたどる回数）を数えて、浅い順に並べる ---
	// 親の行列が先に確定していないと、子の行列が1フレーム遅れてしまう
	self.updateOrder_.clear();
	for (const Entity& entity : self.entities_) {
		self.updateOrder_.emplace_back(self.CalcDepth(entity.self), entity.self);
	}
	std::sort(self.updateOrder_.begin(), self.updateOrder_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

	// --- 浅い順に、自分の行列を作って親の行列を掛ける ---
	for (const auto& [depth, handle] : self.updateOrder_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity) {
			continue;
		}
		TransformComponent* transform = self.transforms_.Get(entity->transform);
		if (!transform) {
			continue;
		}
		// 自分の分（大きさ → 回転 → 位置）
		transform->worldMatrix = MakeAffineMatrix(transform->scale, transform->rotation, transform->translation);
		// 親がいれば、親のワールド行列を掛ける（親は先に確定している）
		if (const TransformComponent* parentTransform = GetTransform(entity->parent)) {
			transform->worldMatrix = transform->worldMatrix * parentTransform->worldMatrix;
		}
	}
}

uint32_t EntityManager::CalcDepth(Handle<Entity> handle) {
	uint32_t depth = 0;
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = entities_.Get(current);
		if (!entity || !entity->parent.IsValid()) {
			break;
		}
		++depth;
		current = entity->parent;
	}
	return depth;
}


//=============================================================================
// 更新順序6：破棄
//=============================================================================
void EntityManager::FlushDestroy() {
	EntityManager& self = *instance_;
	if (self.pendingDestroy_.empty()) {
		return;
	}

	// 親を消したら子も消す。消しながら探すと崩れるので、先に全部集める
	self.destroyWork_.clear();
	for (Handle<Entity> handle : self.pendingDestroy_) {
		self.CollectDescendants(handle, self.destroyWork_);
	}
	self.pendingDestroy_.clear();

	for (Handle<Entity> handle : self.destroyWork_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity) {
			continue; // 親と一緒に2回集まったときは、2回目は消えている
		}
		self.transforms_.Destroy(entity->transform); // Componentも一緒に消す
		self.entities_.Destroy(handle);
	}
}

void EntityManager::CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	if (!entities_.IsAlive(handle)) {
		return;
	}
	out.push_back(handle);
	// 子を探して、その子孫も集める
	for (const Entity& entity : entities_) {
		if (entity.parent == handle) {
			CollectDescendants(entity.self, out);
		}
	}
}
```

### ⑤ `MyEngine/Editor/HierarchyWindow.h`（新規）
```cpp
#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// ヒエラルキーウィンドウ（Entityの一覧・作成・削除・選択・親子の付け替え）
/// <para>選択中のEntityはここが持つ。Inspector（次のStep）はこれを見る</para>
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
	static constexpr int kNameBufferSize = 64; // 名前の編集用バッファの長さ

	// Entity1つ分を描く（子がいれば入れ子で描く）
	static void DrawEntityNode(Handle<Entity> handle);
	// 選択中のEntityの名前を編集する欄
	static void DrawRename();
	// 一覧を描いている間に受け付けた操作を、描き終わってから反映する
	static void ApplyRequests();

	static Handle<Entity> selected_;           // 選択中
	static Handle<Entity> createChildOf_;      // このEntityの子を作る（有効なときだけ）
	static Handle<Entity> destroyRequest_;     // このEntityを消す
	static Handle<Entity> reparentChild_;      // 親を付け替えるEntity
	static Handle<Entity> reparentParent_;     // 新しい親（無効ならrootへ移す）
	static bool createRootRequest_;            // 一番上にEntityを作る
	static bool reparentRequest_;              // 親の付け替えを頼まれた
	static char nameBuffer_[kNameBufferSize];  // 名前の編集用
	static Handle<Entity> nameBufferOwner_;    // nameBuffer_ が今どのEntityのものか
};
```

### ⑥ `MyEngine/Editor/HierarchyWindow.cpp`（新規）
```cpp
#include "HierarchyWindow.h"

#include <cstring>

#include <externals/imgui/imgui.h>

#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::createChildOf_;
Handle<Entity> HierarchyWindow::destroyRequest_;
Handle<Entity> HierarchyWindow::reparentChild_;
Handle<Entity> HierarchyWindow::reparentParent_;
bool HierarchyWindow::createRootRequest_ = false;
bool HierarchyWindow::reparentRequest_ = false;
char HierarchyWindow::nameBuffer_[kNameBufferSize] = {};
Handle<Entity> HierarchyWindow::nameBufferOwner_;

namespace {
constexpr const char* kDragDropType = "ENTITY_HANDLE"; // ドラッグで運ぶものの種類の名前
}


//=============================================================================
// 描画
//=============================================================================
void HierarchyWindow::Draw() {
	ImGui::Begin("Hierarchy");

	// --- 作成・削除 ---
	if (ImGui::Button("Create Entity")) {
		createRootRequest_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Destroy Selected")) {
		destroyRequest_ = selected_;
	}
	ImGui::Text("Count: %zu", EntityManager::GetCount());
	ImGui::Separator();

	// --- 一覧（rootから入れ子で描く） ---
	// 一覧を描いている間にEntityを作ったり消したりすると、並びが変わって崩れる。
	// なので操作は覚えておくだけにして、描き終わってから ApplyRequests で反映する
	for (const Entity& entity : EntityManager::GetAll()) {
		if (!entity.parent.IsValid()) {
			DrawEntityNode(entity.self);
		}
	}

	// --- 何も無い所へドロップしたら、rootへ移す ---
	ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 12.0f)); // ドロップを受ける余白
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropType)) {
			reparentChild_ = *static_cast<const Handle<Entity>*>(payload->Data);
			reparentParent_ = Handle<Entity>{}; // 親なし＝root
			reparentRequest_ = true;
		}
		ImGui::EndDragDropTarget();
	}

	// --- 選択中のEntityの名前 ---
	ImGui::Separator();
	DrawRename();

	ImGui::End();

	ApplyRequests();
}


//=============================================================================
// Entity1つ分
//=============================================================================
void HierarchyWindow::DrawEntityNode(Handle<Entity> handle) {
	Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// 子がいるかを先に調べる（いなければ矢印を出さない）
	bool hasChild = false;
	for (const Entity& other : EntityManager::GetAll()) {
		if (other.parent == handle) {
			hasChild = true;
			break;
		}
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!hasChild) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (handle == selected_) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// Handleの中身をIDにする（名前が同じEntityがあってもぶつからない）
	ImGui::PushID(static_cast<int>(handle.index));
	bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", entity->name.c_str());

	// --- クリックで選択 ---
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		selected_ = handle;
	}

	// --- ドラッグ（このEntityを運ぶ） ---
	if (ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload(kDragDropType, &handle, sizeof(handle));
		ImGui::Text("%s", entity->name.c_str());
		ImGui::EndDragDropSource();
	}
	// --- ドロップ（このEntityを親にする） ---
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropType)) {
			reparentChild_ = *static_cast<const Handle<Entity>*>(payload->Data);
			reparentParent_ = handle;
			reparentRequest_ = true;
		}
		ImGui::EndDragDropTarget();
	}

	// --- 右クリックのメニュー ---
	if (ImGui::BeginPopupContextItem("context")) {
		if (ImGui::MenuItem("Create Child")) {
			createChildOf_ = handle;
		}
		if (ImGui::MenuItem("Destroy")) {
			destroyRequest_ = handle;
		}
		ImGui::EndPopup();
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
// 名前の編集
//=============================================================================
void HierarchyWindow::DrawRename() {
	Entity* entity = EntityManager::Get(selected_);
	if (!entity) {
		ImGui::TextDisabled("No entity selected");
		return;
	}

	// 選択が変わったら、今の名前をバッファへ入れ直す
	if (nameBufferOwner_ != selected_) {
		nameBufferOwner_ = selected_;
		strncpy_s(nameBuffer_, entity->name.c_str(), kNameBufferSize - 1);
	}
	if (ImGui::InputText("Name", nameBuffer_, kNameBufferSize)) {
		entity->name = nameBuffer_;
	}

	// Transformの値（Inspectorができるまでの仮）
	if (TransformComponent* transform = EntityManager::GetTransform(selected_)) {
		ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);
		ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);
	}
}


//=============================================================================
// 一覧を描き終わってから操作を反映する
//=============================================================================
void HierarchyWindow::ApplyRequests() {
	// --- 作成 ---
	if (createRootRequest_) {
		selected_ = EntityManager::Create("Entity");
		createRootRequest_ = false;
	}
	if (createChildOf_.IsValid()) {
		selected_ = EntityManager::Create("Child", createChildOf_);
		createChildOf_ = Handle<Entity>{};
	}
	// --- 親の付け替え ---
	if (reparentRequest_) {
		EntityManager::SetParent(reparentChild_, reparentParent_);
		reparentRequest_ = false;
		reparentChild_ = Handle<Entity>{};
		reparentParent_ = Handle<Entity>{};
	}
	// --- 削除（実際に消えるのはフレームの最後） ---
	if (destroyRequest_.IsValid()) {
		if (destroyRequest_ == selected_) {
			selected_ = Handle<Entity>{};
		}
		EntityManager::Destroy(destroyRequest_);
		destroyRequest_ = Handle<Entity>{};
	}
}
```

### ⑦ `MyEngine/Engine.cpp`

**追加する**：includeの `// Light` の上
```cpp
// Entity
#include "MyEngine/Entity/EntityManager.h"
```

**追加する**：`Initialize` の中、`LightGizmo::Initialize();` の上
```cpp
	EntityManager::Initialize();
```

**追加する**：`Finalize` の中、`LightManager::Release();` の下
```cpp
	EntityManager::Release(); // シーン（ウィンドウ）の破棄より後にする
```

### ⑧ `MyEngine/Window/WindowManager.cpp`

**追加する**：includeの `#include "MyEngine/Light/LightManager.h"` の下
```cpp
#include "MyEngine/Entity/EntityManager.h"
```

**`UpdateAll` の中**：追加する（`ParticleManager::Update();` の上と、`LightManager::Update();` の下）
```cpp
	EntityManager::UpdateTransforms();     // 更新順序2: ワールド行列（親→子）
	ParticleManager::Update();              // パーティクル更新
	LightManager::Update();                 // ライト更新
	EntityManager::FlushDestroy();          // 更新順序6: 破棄予約の反映
```

### ⑨ `MyEngine/Editor/ImGuiManager.cpp`

**追加する**：includeの `#include "MyEngine/Editor/Profiler.h"` の下
```cpp
#include "MyEngine/Editor/HierarchyWindow.h"
```

**追加する**：`Begin` の中、`LightManager::DrawDebugWindow();` の下
```cpp

	// ===== ヒエラルキー =====
	HierarchyWindow::Draw();
```

---

### 解説

**なぜ `Entity` に `self`（自分のHandle）を持たせるのか**
- SlotMapは「Handle → 要素」は引けるが、「要素 → Handle」は引けない（ライトの確認用ウィンドウで困ったのと同じ）。
- ヒエラルキーは一覧を回しながら「このEntityを選択」「このEntityの子を作る」を扱うので、要素からHandleが要る。作るときに自分のHandleを入れておくと、一覧をそのまま回せる。

**ワールド行列を「深さの浅い順」に計算する理由**
- 子の行列は「自分の行列 × 親のワールド行列」。親が先に確定していないと、子が1フレーム遅れてついてくる（親を動かすと子がガクガクする）。
- SlotMapの並び順は作った順・消した順で変わるので、並び順に頼れない。そこで毎フレーム「親を何回たどるとrootに着くか（深さ）」を数えて、浅い順に並べてから計算する。
- 掛ける順番は `自分 * 親` の向き。このエンジンは行ベクトル（シェーダーが `mul(position, world)`）なので、この順になる。

**親子が輪になるのを防ぐ**
- AをBの子にして、さらにBをAの子にすると、親をたどる処理が無限ループする。
- `SetParent` で「新しい親が自分の子孫かどうか」を調べて、輪になるときは何もしない（警告だけ出す）。
- 万一輪ができても止まるように、親をたどる回数に上限（`kMaxParentDepth = 64`）を付けている。

**破棄はフレームの最後にまとめる（ARCHITECTURE.md の規約）**
- `Destroy` は予約だけ。`FlushDestroy` で実際に消す。更新の途中で消すと、同じフレームで使っているポインタが別のEntityを指してしまう（ライトと同じ理由）。
- 親を消したら子も消す。消しながら子を探すと並びが崩れるので、**先に子孫を全部集めてから**まとめて消している。

**ヒエラルキーの操作を「後で反映」する理由**
- 一覧を描いている途中でEntityを作ると、`SlotMap` の中の配列が伸びて、回している最中の場所がずれる（`std::vector` の再確保）。
- なので描画中は「何を頼まれたか」を覚えるだけにして、ウィンドウを描き終わってから `ApplyRequests` で作る・消す・親を変える。
- 削除はもともとフレーム末まで遅れるので安全だが、作成と親の付け替えも同じ形にそろえてある。

**ImGuiのドラッグ＆ドロップ**
- `SetDragDropPayload` でHandleをそのまま運んでいる（Handleは数値2つだけの型なのでコピーできる）。
- 相手のEntityの上でドロップすると子になり、下の余白にドロップするとrootに戻る。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 「Hierarchy」ウィンドウが出て、「Create Entity」でEntityが増える（Countも増える）
   - Entityをクリックすると選択され、下で名前とTransformを編集できる
   - Entityを別のEntityにドラッグすると子になる。下の余白にドラッグするとrootに戻る
   - 親の `Position` を動かすと、子も一緒に動く（子の `Position` は親から見た位置になる）
   - 親を右クリック →「Destroy」で、子も一緒に消える
   - 自分の子を自分の親にしようとすると、ログに警告が出て何も起きない
   - Entityを100個作っても重くならない

### 次のStepでやること（ここではやらない）
- Step 2：Inspectorウィンドウ。Transformの編集をこちらに移し、「Add Component」の仕組み（Componentの種類の登録表）を作る。
- Step 3：`RenderComponent`（Entityがモデルを描く）。ここで初めて、ヒエラルキーで作ったEntityが画面に見えるようになる。

---

## Step 2：Inspectorウィンドウ

### 目的
- Step 1 では、選択中のEntityの名前とTransformを**ヒエラルキーの下に仮で**出していた。これを独立した「Inspector」ウィンドウに移す。
- Unityと同じ形にする：**ヒエラルキー＝どのEntityを選ぶか、Inspector＝選んだEntityの中身を編集する**。
- Componentが増えていくので、**「Componentごとに1つの区画」という形を最初に決めておく**。これ以降のStep（RenderComponent、ライト）は、この形に区画を1つ足すだけで済む。

### 区画の形
```
Inspector
 ├ ［☑］［Entityの名前］        … Entityそのもの（isActive・名前）
 │  Handle: index=3 generation=1
 │  Parent: (root)
 ├ ▼ Transform                  … TransformComponentの区画
 │    Position / Rotation / Scale / Reset
 │    World Position: ...       … 計算結果（読み取り専用）
 │    ▶ World Matrix
 └ ［Add Component］             … 区画を足すボタン（中身は次のStepから）
```

**回転は「中はラジアン、表示は度」にする。** `TransformComponent::rotation` はラジアンで持っている（`MakeAffineMatrix` がラジアンを受け取るので、毎フレーム変換したくない）。でも度で見ないと編集しづらいので、**Inspectorで見せるときだけ度に直す**。

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Editor/InspectorWindow.h` | **新規** |
| ② | `MyEngine/Editor/InspectorWindow.cpp` | **新規** |
| ③ | `MyEngine/Editor/HierarchyWindow.h` | 名前編集（`DrawRename`）を消す |
| ④ | `MyEngine/Editor/HierarchyWindow.cpp` | 同上 |
| ⑤ | `MyEngine/Editor/ImGuiManager.cpp` | `InspectorWindow::Draw()` を呼ぶ |

- 新規2ファイルは、Visual Studioの**ソリューションエクスプローラで `MyEngine/Editor` フィルタに追加**する（`.vcxproj` は自分で書かなくてよい）。

---

### ① `MyEngine/Editor/InspectorWindow.h`（新規）
```cpp
#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// インスペクターウィンドウ（HierarchyWindowで選んでいるEntityの中身を編集する）
/// <para>Componentごとに1つの区画（CollapsingHeader）にする。Componentが増えたらDrawXxxを足してDrawから呼ぶ</para>
/// </summary>
class InspectorWindow {
public:
	/// <summary>
	/// ImGuiのフレームの中で毎フレーム呼ぶ（HierarchyWindow::Drawより後）
	/// </summary>
	static void Draw();

private:
	static constexpr int kNameBufferSize = 64; // 名前の編集用バッファの長さ

	// Entityそのものの情報（名前・有効無効・Handleの番号）
	static void DrawHeader(Handle<Entity> handle);
	// TransformComponentの区画
	static void DrawTransform(Handle<Entity> handle);
	// Componentを足すボタン（中身は次のStepから増やす）
	static void DrawAddComponent(Handle<Entity> handle);

	static char nameBuffer_[kNameBufferSize]; // 名前の編集用
	static Handle<Entity> nameBufferOwner_;   // nameBuffer_ が今どのEntityのものか
};
```

---

### ② `MyEngine/Editor/InspectorWindow.cpp`（新規）
```cpp
#include "InspectorWindow.h"

#include <cfloat>
#include <cstring>
#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
char InspectorWindow::nameBuffer_[kNameBufferSize] = {};
Handle<Entity> InspectorWindow::nameBufferOwner_;

namespace {
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace


//=============================================================================
// 描画
//=============================================================================
void InspectorWindow::Draw() {
	ImGui::Begin("Inspector");

	// 選択中のEntityはHierarchyWindowが持っている
	Handle<Entity> handle = HierarchyWindow::GetSelected();
	if (!EntityManager::IsAlive(handle)) {
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	// Componentを1つずつ区画にして描く。Componentが増えたらここに足していく
	DrawHeader(handle);
	ImGui::Separator();
	DrawTransform(handle);
	ImGui::Separator();
	DrawAddComponent(handle);

	ImGui::End();
}


//=============================================================================
// Entityそのものの情報
//=============================================================================
void InspectorWindow::DrawHeader(Handle<Entity> handle) {
	// ポインタは使い捨てにする（ARCHITECTURE.md 原則1）。持ち越すのはHandleだけ
	Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// --- 有効・無効 ---
	ImGui::Checkbox("##isActive", &entity->isActive);
	ImGui::SameLine();

	// --- 名前 ---
	// 選択が変わったら、今の名前をバッファへ入れ直す
	if (nameBufferOwner_ != handle) {
		nameBufferOwner_ = handle;
		strncpy_s(nameBuffer_, entity->name.c_str(), kNameBufferSize - 1);
	}
	ImGui::SetNextItemWidth(-FLT_MIN); // 残りの幅いっぱいに広げる
	if (ImGui::InputText("##name", nameBuffer_, kNameBufferSize)) {
		entity->name = nameBuffer_;
	}

	// --- 参考情報（読み取り専用） ---
	ImGui::TextDisabled("Handle: index=%u generation=%u", handle.index, handle.generation);
	if (const Entity* parent = EntityManager::Get(entity->parent)) {
		ImGui::TextDisabled("Parent: %s", parent->name.c_str());
	} else {
		ImGui::TextDisabled("Parent: (root)");
	}
}


//=============================================================================
// TransformComponent
//=============================================================================
void InspectorWindow::DrawTransform(Handle<Entity> handle) {
	TransformComponent* transform = EntityManager::GetTransform(handle);
	if (!transform) {
		return;
	}

	// 全Entityが必ず持つ区画なので、開いた状態で始める
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（Componentが増えたときに名前がぶつからないように）
	ImGui::PushID("Transform");

	ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);

	// 回転は中ではラジアンで持っているので、見せるときだけ度に直す。
	// 変わったときだけ書き戻す（毎フレーム度↔ラジアンを往復させると、値がじわじわずれる）
	Vector3 degrees = {transform->rotation.x * kRadToDeg, transform->rotation.y * kRadToDeg, transform->rotation.z * kRadToDeg};
	if (ImGui::DragFloat3("Rotation", &degrees.x, 0.5f)) {
		transform->rotation = {degrees.x * kDegToRad, degrees.y * kDegToRad, degrees.z * kDegToRad};
	}

	ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform->translation = {0.0f, 0.0f, 0.0f};
		transform->rotation = {0.0f, 0.0f, 0.0f};
		transform->scale = {1.0f, 1.0f, 1.0f};
	}

	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform->worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}


//=============================================================================
// Componentを足すボタン
//=============================================================================
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	if (!EntityManager::IsAlive(handle)) {
		return;
	}

	if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f))) {
		ImGui::OpenPopup("addComponent");
	}
	if (ImGui::BeginPopup("addComponent")) {
		// 足せるComponentが増えたら、ここにMenuItemを並べてEntityへ足す
		ImGui::TextDisabled("(none yet)");
		ImGui::EndPopup();
	}
}
```

---

### ③ `MyEngine/Editor/HierarchyWindow.h`

**消す**：`private:` のすぐ下の1行
```cpp
	static constexpr int kNameBufferSize = 64; // 名前の編集用バッファの長さ
```

**消す**：`DrawRename` の宣言（コメント込みで2行）
```cpp
	// 選択中のEntityの名前を編集する欄
	static void DrawRename();
```

**消す**：名前用の静的メンバ2行
```cpp
	static char nameBuffer_[kNameBufferSize]; // 名前の編集用
	static Handle<Entity> nameBufferOwner_;   // nameBuffer_ が今どのEntityのものか
```

**差し替える**：クラスのコメント（Inspectorができたので書き直す）
```cpp
/// <para>選択中のEntityはここが持つ。InspectorWindowはこれを見る</para>
```

---

### ④ `MyEngine/Editor/HierarchyWindow.cpp`

**消す**：includeの1行（`strncpy_s` を使うのをやめるので）
```cpp
#include <cstring>
```

**消す**：静的メンバの定義2行
```cpp
char HierarchyWindow::nameBuffer_[kNameBufferSize] = {};
Handle<Entity> HierarchyWindow::nameBufferOwner_;
```

**消す**：`Draw()` の中、`ImGui::End();` の直前の3行
```cpp
	// --- 選択中のEntityの名前 ---
	ImGui::Separator();
	DrawRename();
```

**消す**：`DrawRename` の関数ごと（`//====` の飾りも含めて）
```cpp
//=============================================================================
// 名前の編集
//=============================================================================
void HierarchyWindow::DrawRename() {
	...（関数の中身全部）...
}
```

---

### ⑤ `MyEngine/Editor/ImGuiManager.cpp`

**追加する**：includeを1行（`HierarchyWindow.h` の下）
```cpp
#include "MyEngine/Editor/InspectorWindow.h"
```

**追加する**：`HierarchyWindow::Draw();` の下
```cpp

	// ===== インスペクター（ヒエラルキーで選んだEntityを編集する）=====
	InspectorWindow::Draw();
```

---

### 解説

**なぜ選択中のEntityを Inspector が持たないのか**
- 選んでいる状態は「ヒエラルキー側の情報」なので、`HierarchyWindow::GetSelected()` を見るだけにしている。Inspectorが自分で持つと、ヒエラルキーで選び直したときに食い違う。
- 将来 Sceneビューのクリックでも選べるようにするときは、`HierarchyWindow::SetSelected()` を呼ぶだけで済む（もう用意してある）。

**Handleを持ち回して、毎回 `Get` し直している理由**
- ARCHITECTURE.md 原則1。`Get` が返すポインタは `SlotMap` の中の配列を指しているので、**Entityが増えると配列が再確保されて無効になる**。
- Inspectorの中ではEntityを作らないので今は壊れないが、`Add Component` からEntityを触るようになるので最初からこの形にしておく。

**`ImGui::PushID("Transform")` が必要な理由**
- `CollapsingHeader` は `TreeNode` と違って**IDの範囲（スコープ）を作らない**。区画が増えたとき、別のComponentにも `Position` という項目があると、ImGuiが同じ項目だと勘違いする（Light.md Step 5.1a と同じ話）。

**`-FLT_MIN` は「残り幅いっぱい」の意味**
- ImGuiの幅の指定は「0＝既定、正＝その幅、負＝右端からその分を残す」。`-FLT_MIN` は「ほぼ0だけ残す」＝実質いっぱい、という書き方（ImGuiの公式のサンプルもこの書き方）。

**度⇔ラジアンを「変わったときだけ」書き戻す理由**
- 毎フレーム `ラジアン→度→ラジアン` と往復させると、floatの丸めで値がじわじわずれていく（触っていないのに回転が変わる）。
- `DragFloat3` は**値が変わったフレームだけ `true`** を返すので、そのときだけ書き戻せばずれない。

**`Add Component` の中身が空な理由**
- 今このエンジンにあるComponentは `TransformComponent`（全Entityが必ず持つ）だけなので、**足せるものがまだ無い**。
- Step 3 の `RenderComponent` で最初の項目が入る。ボタンと popup の置き場所だけ先に作っておく。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 「Inspector」ウィンドウが出る（ドッキングで好きな場所に置ける）
   - 何も選んでいないと `No entity selected` と出る
   - ヒエラルキーでEntityを選ぶと、名前・Transformが出る（ヒエラルキー側の仮のTransform欄は消えている）
   - 名前を書き換えると、ヒエラルキーの表示も変わる
   - `Rotation` が**度**で出る（90と入れると1/4回転）
   - 親の `Position` を動かすと、子の `World Position` が一緒に動く（子の `Position` は変わらない）
   - `Reset` で位置・回転・大きさが初期値に戻る
   - Entityを消した直後も落ちない（`No entity selected` に戻る）

### 次のStepでやること（ここではやらない）
- Step 3：`RenderComponent`。`Add Component` の最初の項目になる。ここで初めてヒエラルキーで作ったEntityが画面に見える。
- Step 4：ライトのComponentをEntityに載せる（`Light.md` Step 6）。
