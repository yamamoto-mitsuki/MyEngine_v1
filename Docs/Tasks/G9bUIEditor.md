# G9b：UI を置きやすくする（Hierarchy の目・UI をマウスで動かす）

参考：
- Unity の Scene Visibility（Hierarchy の目。Scene ビューだけで隠す）：https://docs.unity3d.com/Manual/SceneVisibility.html
- Unity の Rect Tool（UI をマウスで動かす道具）：https://docs.unity3d.com/Manual/PositioningGameObjects.html
- cpprefjp `std::unordered_set`：https://cpprefjp.github.io/reference/unordered_set/unordered_set.html
- cpprefjp `std::visit`：https://cpprefjp.github.io/reference/variant/visit.html
- cpprefjp `std::tie`（並びを辞書順に比べる）：https://cpprefjp.github.io/reference/tuple/tie.html
- Dear ImGui `imgui.h`（`InvisibleButton`・`IsMouseDragging`・`ImDrawList::AddBezierQuadratic`）：https://github.com/ocornut/imgui/blob/master/imgui.h

## このファイルについて
- [AL4_01Game.md](AL4_01Game.md) の 13章の **G9b**（G9a のチュートリアルの説明を置いてみて困ったことを直す。G9 のレベル・UI の前）。
- 困っていたこと（ユーザー）：
  - 「移動」の説明を作った後に「ジャンプ」の説明を作ろうとしたら、**「移動」の文字が Scene ビューに出続けていて**、重なって作りにくい。
  - **UI（Image・Text）を動かすのが大変**（Inspector の数字を打つしかない）。マウスで動かしたい。
- 作る物は2つ（どちらもエンジンのエディタ。ほかのゲームでも使える）：
  - **Hierarchy の目**（`SceneVisibility`）：押すと、その Entity（と子）を **Scene ビューだけで隠す**。Game ビューの絵・ゲームの動きは変わらない・保存しない（Unity の Scene Visibility と同じ）。
  - **UI をマウスで動かす**（`UIGizmo`）：Scene ビューで Image・Text を**クリックで選び、ドラッグで動かす**（Unity の Rect Tool の、動かすだけの版）。1回のドラッグで1件の履歴（Ctrl+Z で戻せる）。
- **パートの順番**：A（描き分けの情報を `DrawTarget` にまとめる）→ B（目）→ C（UI をマウスで）→ D（使い方）。A だけでビルドが通る（動きは今までと同じ）。B は A の `DrawTarget::isSceneHidden` を使う。C は B の `SceneVisibility` を使う（隠した UI は選ばない）。ゲーム（AL4_01）のコードは変えない。
- 確かめた環境：本体の master（54a8a07。G9a まで）＋ このファイルのエンジン、AL4_01 のコピー（G9a までの写経のまま）。Debug・Release でビルドし、Debug のエディタで動かして画面で確かめた（6章）。
- 次の [G9cSceneTransition.md](G9cSceneTransition.md)（シーンの切り替えの幕・ポーズ）は、この G9b の `DrawTarget::isSceneHidden` を使う。**G9b を先に写す**。

| パート | ファイル | 何をするか |
|---|---|---|
| A | `Graphics/Pipeline/RenderStates.h`・`Graphics/Renderer/DrawRequest.h`・`Renderer.h/.cpp`・`RenderQueue.h/.cpp`・`SceneRenderer.h/.cpp`・`Editor/Viewport/EditorViewport.cpp` | どの Config・リクエストにもある「どのウィンドウ・どの層に描くか」を `DrawTarget` にまとめ、「目で隠した」（`isSceneHidden`）を足す。どのビューに描くかを `RenderQueue::IsDrawnInView` 1つで決める |
| B | `Editor/Viewport/SceneVisibility.h/.cpp`（新しく作る）・描く System 6つ・`ScenePicking.cpp`・`HierarchyWindow.h/.cpp`・`SceneManager.cpp`・`MyEngine_v1.vcxproj` | Hierarchy の目。隠した物を Scene ビューで描かない・選ばない |
| C | `Systems/SpriteRenderSystem.h/.cpp`・`Editor/Gizmos/UIGizmo.h/.cpp`（新しく作る）・`SceneGizmo.cpp`・`EditorViewport.h`・`ImGuiManager.cpp`・`MyEngine_v1.vcxproj` | Scene ビューで UI をクリックで選ぶ・ドラッグで動かす。View → Scene View → Pick UI |
| D | エディタで | 使い方 |

---

## 0. 前の写経の答え合わせ（G9a）
G9a（54a8a07）を答えと比べた。**写し違いは無し**（動きは答えと同じ）。違いは次の2つだけで、どちらも直さなくてよい：

| ファイル | 違い |
|---|---|
| `TriggerActivatorSystem.cpp` | `ui.Field("Source###source", ...)` の行を3行に折り返している（中身は同じ） |
| `WindowManager.cpp` | `TriggerActivatorSystem::Update` の説明のコメントを、上の行から行の右へ移している（中身は同じ） |

---

## 1. 何をするか（なぜこう作ったか）

### 1.1 なぜ「移動」の文字が出続けるか
Text・Image は**画面に重ねる UI**（位置は 1280×720 の画面のピクセル）なので、Scene ビューでも、カメラをどこに向けても**画面の同じ所**に重なる。Trigger Activator の Hide On Start は**再生中だけ**動くので、停止中のエディタでは、置いた説明が全部出たまま重なる。

```
 Scene ビュー（停止中）               再生中（Game ビュー）
 ┌──────────────────────┐            ┌──────────────────────┐
 │    移動  ジャンプ      │ ← 重なる   │    移動                │ ← 範囲に入った物だけ出る
 │    A D   Space        │            │    A D                 │
 └──────────────────────┘            └──────────────────────┘
```

**コードを書かずに今すぐできる手**：作り終わった説明（`Text_Move`）の Inspector の名前の左のチェックを外して**無効**にする。Trigger Activator の Show は、無効の Target を有効にしてから出す（`Show` の `if (!target.isActive) { target.isActive = true; ...`）ので、再生中は今までどおり出る。
ただし、無効にすると **Game ビューでも消える**・**シーンファイルに書かれる**（戻し忘れると、Hide On Start を外した説明が出なくなる）。そこで、Unity と同じ「Scene ビューだけで隠す目」を作る。

| | 有効のチェック（今ある） | 目（今回作る） |
|---|---|---|
| Scene ビュー | 消える | 消える |
| Game ビュー | 消える | **出る** |
| 当たり・Script | 動かない | **動く**（ゲームは何も変わらない） |
| シーンファイル | 書かれる | **書かれない**（エディタを閉じる・別のシーンを開くと戻る） |
| Play / Stop | そのまま | そのまま（EntityId で覚える） |

### 1.2 Scene ビューだけ描かない仕組み：描画の層（DrawLayer）
エディタは、描く物を**1回だけ積んで**、Scene ビュー（エディタのカメラ）と Game ビュー（ゲームのカメラ）の**2回描く**（`EditorViewport::Render`）。G11 で、グリッドを Scene・Game で別々に切れるように、**層**（`DrawLayer::EditorGrid`）を作った：

```
 積む（1回）                     描く（ビューごとに）
 MeshRequest { layer = EditorGrid } ─┬→ Scene ビュー：SetLayerVisible(EditorGrid, true)  → 描く
                                      └→ Game ビュー ：SetLayerVisible(EditorGrid, false) → 飛ばす
```

同じように、「目で隠した物」を Scene ビューだけ飛ばしたい。Scene ビューは `SceneViewOptions::sceneHidden = false`、Game ビューと製品版は `true`（いつも描く）。

#### 困ること1：層を持っているのが `MeshRequest`・`ModelConfig` だけ
Renderer の Config は14種類（`SphereConfig`・`MeshConfig`・`Quad2dConfig`・`Text2dConfig`…）あり、どれも**同じ `windowTitle` の行をコピペで持っている**。層（`layer`）を持っているのは `ModelConfig` だけ。リクエスト（`MeshRequest`・`SpriteRequest`…）も同じで、`windowTitle` は全部にあり、`layer` は `MeshRequest` だけ。RenderQueue の Flush も、「別のウィンドウの物なら飛ばす」という同じ if を5か所に持っている。

#### 困ること2：「目で隠した」を層の1つにすると、ほかの層と一緒に持てない
`DrawLayer` に `SceneHidden` を足すと、層は1つしか持てないので、「グリッドで、しかも隠した物」「（将来）背景の層で、しかも隠した物」を表せない。**層（ゲームが決める描き分け）と、目（エディタだけの見せ方）は別の物**なので、別の値にする。

#### そこで：`DrawTarget` にまとめる
「どのウィンドウ・どの層に描くか・目で隠したか」を **`DrawTarget`**（描き分けの情報）という1つの構造体にまとめて、**Config もリクエストも、これを受け継ぐ**（継承する）：

```
DrawTarget { windowTitle; layer; isSceneHidden; }
   ├ Renderer::SphereConfig : DrawTarget   … 14種類の Config 全部（自分の windowTitle の行は消す）
   └ MeshRequest : DrawTarget              … 5種類のリクエスト全部
```

```
 System                         Renderer                                   RenderQueue（ビューごと）
 config.isSceneHidden = 隠したか → static_cast<DrawTarget&>(req) = config; → IsDrawnInView(req, windowTitle)
                                  （3つまとめて写す）                         ・別のウィンドウ → 飛ばす
                                                                              ・切ってある層   → 飛ばす
                                                                              ・目で隠した ＆ Scene ビュー → 飛ばす
```

- **受け継ぐと、今の書き方がそのまま使える**：`config.windowTitle = ...`・`req.layer` は、名前が同じなので、今までどおり書ける（`DrawTarget` の中の値になるだけ）。Config を書く側（ゲームの Script・ほかの System）は何も変えなくてよい。
- **Renderer は3つまとめて写す**：`static_cast<DrawTarget&>(req) = config;`。req を `DrawTarget` として見て代入すると、`DrawTarget` の所（3つの値）だけが写る（ほかの値には触らない）。新しい値を `DrawTarget` に足しても、Renderer は直さなくてよい。
- **どのビューに描くかは `RenderQueue::IsDrawnInView` 1つで決める**：5か所にあった「別のウィンドウなら飛ばす」と層の if を、この関数1つにまとめる。メッシュ・パーティクル・線・2D の全部が同じ決まりになる（パーティクル・線にも層と目が効くようになる）。
- 2D（Image・Text）は `requests2d_` に `std::variant<SpriteRequest, TextRequest>` で入っている。どちらも `DrawTarget` を受け継ぐので、`OrderOf` と同じく **`std::visit`** で `DrawTarget` として取り出す（`std::visit`：variant の中に今入っている型のまま、ラムダを呼んでくれる関数。`auto` の引数にすると、どちらの型でも同じ書き方で読める。戻り値を `-> const DrawTarget&` と書くと、写しを作らずに中の値をそのまま返す）。
- System は、Config を作る所で **`config.isSceneHidden = SceneVisibility::IsHidden(entity);`** を1行書く。

前に考えた別の形（`RenderQueue::LayerScope`。「作ってから消えるまでの間に積んだ物を、その層にする」RAII の入れ物を、System の頭に1行置く）と比べた：

| | LayerScope | **DrawTarget（こちらにした）** |
|---|---|---|
| System に書く量 | 1行 | 1行（同じ） |
| どこで決まるか | 見えにくい（RenderQueue が裏で覚えている値を、積むときに読む） | 見える（`config.isSceneHidden = ...` と書いてある） |
| 描くのを何本かのスレッドに分けたとき | 壊れる（1つの値を取り合う） | そのまま動く |
| 層と目を一緒に持てるか | 持てない（目も層の1つ） | 持てる（別の値） |
| `windowTitle` のコピペ | そのまま | 消える（14か所 → 1か所） |

将来、Unity の Layer とカメラの Culling Mask（カメラごとに描く層を選ぶ。ミニマップのカメラなど）を作るときは、`DrawTarget::layer` を「いくつも選べる形」（ビットの集まり）にして、`IsDrawnInView` の層の if を変えるだけでよい。

### 1.3 目で隠した物の覚え方（SceneVisibility）
- **EntityId** で覚える（`std::unordered_set<EntityId>`）。Handle は Play / Stop でシーンを作り直すたびに変わるが、EntityId は変わらない（EntityRef と同じ考え）。
- `std::unordered_set`：同じ値を2つ持たない入れ物。「入っているか」（`contains`）を、数がいくつあっても一定の速さで調べられる（ハッシュ表）。並び順は無い。
- 親を隠すと、子も隠れる（`IsHidden` が親をたどる。`CalcGroupAlpha` と同じたどり方）。
- 誰も隠していなければ、親をたどらずに `false`（製品版はいつもここで終わる。描く System が毎フレーム呼ぶので）。
- 隠した物は **Scene ビューのクリックでも選ばない**（`ScenePicking` の `ClosestHit::Consider` で断る。見えない物が選ばれると困るので。Unity も同じ）。
- 別のシーンを開いたら、全部見せる（`SceneManager::OpenImmediate` で `Clear`。EntityId はシーンごとに振るので、前のシーンの番号で別のシーンの物を隠さないように）。
- **隠れる物**：Model Renderer・Primitive Renderer・Polygon Renderer・Arrow Strip・Sprite Renderer・Image・Text（描く System が `config.isSceneHidden` を書く物）。**隠れない物**：線のギズモ（Collider・ライト）・パーティクル・Trail・Elastic Surface（今は要らないので。要るときは、その System の Config にも1行足す）。

### 1.4 UI をマウスで動かす（UIGizmo）
今の移動のギズモ（ImGuizmo の3本の矢印）は、**3D の物を3D の長さ（m）で**動かす。UI の Translation は**画面のピクセル**なので、UI を選ぶと 150m 先の所に矢印が出て、使えない。そこで：

- **UI（Image か Text が付いた物）には3D のギズモを出さない**（`Manipulate` で断る。フォルダと同じ）。
- 代わりに、**UI の四角**（`ScreenQuad`）を出して、マウスと比べる：
  - Image：Size・Pivot の四角（Fill は掛けない＝Image の大きさ全部）。
  - Text：文字のかたまりの四角（`TextRenderSystem::CalcTextSize`）。
  - 今の `CalcImageQuad`（Image の描画・`IsMouseOver` に使っている）を、**どちらにも使える `SpriteRenderSystem::CalcScreenQuad`** にする（四角の大きさ・Pivot・Anchor を引数でもらう）。G9d の Screen（マウスで項目を選ぶ）でも使える。
- **クリック**：マウスの下の UI のうち、**一番手前に描いてある物**を選ぶ。描く順と同じく、Order が大きい物、同じなら番号（EntityId）が大きい＝後に作った物（**`std::tie`**：`std::tie(a.order, a.id) < std::tie(b.order, b.id)` で「1つ目を比べて、同じなら2つ目」をまとめて書ける。`DrawImages` の並べ方と同じ）。
- **UI は 3D の物より先に選ぶ**（3D の物より手前に描いてあるので）。UI の後ろの 3D の物を選びたいときは、**View → Scene View → Pick UI** を外す（または、目で UI を隠す）。
- **ドラッグ**：押した所からマウスが動いた量を、UI のピクセルに直して Translation に足す。

```
 Scene ビューの絵（画面の座標。例：横 451 ピクセル）       UI の位置（1280×720 の画面のピクセル。Y は上が＋）
   マウスが右へ 50・上へ 20 動いた        ─────→    x：50 ÷ 451 × 1280 ＝ ＋142
                                                    y：20 ÷ 254 × 720  ＝ ＋57（画面の座標は下が＋なので、符号を逆に）
```
  - 親がいる（`Tutorial` フォルダの子など）ときは、**親から見た量**に直す（親のワールド行列の逆行列で、回転と大きさを戻す）。動かした量は「位置」ではなく「向き」なので、点の変換の差（`TransformPoint(量) − TransformPoint(0)`。移動の分が引かれて消える）を使う。
  - **1 ピクセルずつ**（`std::round`。Inspector の数字が 150.37 のようにならないように）。
  - 押しただけでは動かさない（`ImGui::IsMouseDragging`：押した所から決まった距離＝6 ピクセルより動いたか）。クリックで選ぶだけのときに、ずれないように。
  - 離したら、押す前と後を**1件の履歴**にする（`EditorHistory::RecordComponentChange` → `CommitComponentChanges`。ギズモの `EndDrag` と同じ入口）。
- **順番**（`SceneGizmo::Draw` の中）：移動のギズモ（`Manipulate`）→「ギズモの上か」を決める → UI（`UIGizmo::Draw`）→ 3D の物を選ぶ（`HandleClick`）。ギズモの矢印・右上の軸の上は、UI も 3D の物も選び直さない（前は `HandleClick` の中で調べていた条件を、`SceneGizmo::Draw` で1回だけ決めて、両方に渡す）。

---

## 2. パートA：描き分けの情報を DrawTarget にまとめる（エンジン）

ここは「今ある物を、まとめ直す」パート。**動きは今までと同じ**（`isSceneHidden` を書く人がまだいないので）。

### `MyEngine/Graphics/Pipeline/RenderStates.h`（2か所）
include の頭に（`std::wstring` を使うので）：
```cpp
#include <string>
```
`DrawLayer` の下に：
```cpp
/// <summary>
/// 描く物を、どのウィンドウ・どのビューに描くか（Renderer の Config と、RenderQueue のリクエストが受け継ぐ）
/// <para>どの Config にも同じ値があるので、1つにまとめた。System は config の中のこの値を書き、Renderer はリクエストへまとめて写す</para>
/// <para>描くかどうかは RenderQueue::IsDrawnInView が、ビューを描くときに決める（積むのは1回、ビューごとに飛ばす）</para>
/// </summary>
struct DrawTarget {
	std::wstring windowTitle = L"";       // 描画したいウィンドウ名（空なら、どのウィンドウにも描く）
	DrawLayer layer = DrawLayer::Default; // 層（ビューごとに描く・描かないを選ぶ。RenderQueue::SetLayerVisible）
	bool isSceneHidden = false;           // Hierarchy の目で隠した物（Scene ビューだけ描かない。Game ビューにはいつも描く）
};
```

### `MyEngine/Graphics/Renderer/DrawRequest.h`（リクエスト5つ）
include の下に、説明を1行：
```cpp
// どのリクエストも DrawTarget（どのウィンドウ・どの層・目で隠したか）を受け継ぐ。Renderer が Config から写す
```
5つの struct の頭を `: DrawTarget` にして、**自分で持っていた `windowTitle`（と `layer`）の行を消す**：

| struct | 頭 | 消す行 |
|---|---|---|
| `MeshRequest` | `struct MeshRequest : DrawTarget {` | `std::wstring windowTitle; // ウィンドウ名識別時に使用`・`DrawLayer layer = ...` |
| `ParticleRequest` | `struct ParticleRequest : DrawTarget {` | `std::wstring windowTitle = L"";` |
| `SpriteRequest` | `struct SpriteRequest : DrawTarget {` | `// ウィンドウ名識別時に使用` と `std::wstring windowTitle;` |
| `TextRequest` | `struct TextRequest : DrawTarget {` | 同じ（コメントと `windowTitle`） |
| `LineRequest` | `struct LineRequest : DrawTarget {` | 同じ（コメントと `windowTitle`。その下の空行も） |

例（`SpriteRequest`）：
```cpp
struct SpriteRequest : DrawTarget {
	// 重ねる順（大きいほど手前。同じなら積んだ順。文字（TextRequest）とも、この順で混ぜて描く）
	int order = 0;

	// --- ジオメトリ ---
	std::array<Vertex2dData, 4> vertices;

	// --- Shaderとのバインド情報 ---
	Material2dData materialData;

	// --- 描画設定 ---
	BlendMode blendMode = BlendMode::Normal;
	RasterizerType rasterizerType = RasterizerType::SolidBack;
};
```

### `MyEngine/Graphics/Renderer/Renderer.h`（Config 14個）
`explicit Renderer(ConstructorKey) {}` の下に、説明を2行：
```cpp
	// どの Config も DrawTarget を受け継ぐ（windowTitle：描くウィンドウ / layer：層 / isSceneHidden：Hierarchy の目で隠した物）
	// 書かなければ初期値（どのウィンドウにも・Default の層・隠さない）
```
`struct 〇〇Config {` を**全部** `struct 〇〇Config : DrawTarget {` にして（`ModelConfig`・`TriangleConfig`・`SphereConfig`・`Rect3dConfig`・`Sprite3dConfig`・`Quad3dConfig`・`MeshConfig`・`AABBConfig`・`OBBConfig`・`ParticleConfig`・`SpriteConfig`・`Quad2dConfig`・`Text2dConfig`・`LineListConfig` の14個）、それぞれの中の **`std::wstring windowTitle = L"";` の行を消す**。`ModelConfig` は **`DrawLayer layer = DrawLayer::Default;` の行も消す**（どちらも `DrawTarget` から来る）。
- VS の「検索と置換」（Ctrl+H）で、Renderer.h だけを対象に `std::wstring windowTitle = L"";` を探すと、14行が見つかる。

例（`ModelConfig`）：
```cpp
	struct ModelConfig : DrawTarget {
		uint32_t modelHandle = 0;                                  // モデルハンドル
		uint32_t textureHandle = 0;                                // テクスチャハンドル
		Transform transform;                                       // 拡縮、回転、移動
		Transform uvTransform;                                     // UVの拡縮、回転、移動
		const Matrix4x4* worldMatrix = nullptr;                    // 指定するとtransformの代わりに使う（階層・ロール用）
		const std::vector<Matrix4x4>* nodeMatrices = nullptr;      // ノードの行列（AnimationPlayer::GetNodeMatrices）。nullptr・数が合わないときはモデルに入っている行列
		Handle<Entity> entity;                                     // 持ち主のEntity、スキンのあるメッシュを CS で曲げるとき、出力バッファを見分けるのに使う（無効なら曲げない）
		uint32_t color = 0xFFFFFFFF;                               // 色
		ShadingType shadingType = ShadingType::Unlit;              // シェーディング設定
		BlendMode blendMode = BlendMode::Normal;                   // ブレンド設定
		RasterizerType rasterizerType = RasterizerType::SolidBack; // ラスタライザ設定
		DepthMode depthMode = DepthMode::TestWrite;                // 深度設定
		MaterialParams material;                                   // マテリアル調整パラメータ
		bool overrideMaterial = false;                             // true：material をモデルの mtl / glTF の値の代わりに使う（false なら mtl に無いときだけ）
		Camera* camera = nullptr;                                  // カメラ設定
		IBLEnvironment* env = nullptr;                             // 環境光源
		BillboardMode billboard = BillboardMode::None;             // カメラの方を向かせる（Full / AxisY。cameraが必要）
	};
```

### `MyEngine/Graphics/Renderer/Renderer.cpp`（6か所）
`req.windowTitle = config.windowTitle;` を、6か所とも次の行に置き換える（`PushMesh`・`PushSprite`・`DrawModel`・`DrawParticle`・`DrawText2d`・`DrawLines`）。`DrawModel` は、その下の `req.layer = config.layer;` も消す（一緒に写る）：
```cpp
	static_cast<DrawTarget&>(req) = config; // 描き分け（ウィンドウ・層・目で隠したか）をまとめて写す（DrawTarget の所だけ代入する）
```
- `static_cast<DrawTarget&>(req)`：req を「`DrawTarget` の所だけ」として見る。そこへ config（これも `DrawTarget` を受け継いでいる）を代入すると、`DrawTarget` の3つの値だけが写る（Config のほかの値・req のほかの値には触らない）。
- `DrawModel` の中はインデントが深い（for の中）ので、タブを合わせる。

### `MyEngine/Graphics/Renderer/RenderQueue.h`（3か所）
`SetLayerVisible` の下に：
```cpp
	// この後に描くビューで、Hierarchy の目で隠した物（DrawTarget::isSceneHidden）を描くか（Scene ビューだけ false）
	static void SetSceneHiddenVisible(bool isVisible);
```
`private:` の `FlushMeshList` の下に：
```cpp
	// 今描いているビュー（windowTitle のウィンドウ・SetLayerVisible などで決めた見せ方）で描く物か。Flush の全部が使う
	static bool IsDrawnInView(const DrawTarget& target, const std::wstring& windowTitle);
```
メンバ変数の最後（`isLayerVisible_` の下）に：
```cpp
	bool isSceneHiddenVisible_ = true; // 目で隠した物を、今のビューで描くか（Scene ビューだけ false）
```

### `MyEngine/Graphics/Renderer/RenderQueue.cpp`（6か所）
`SetLayerVisible` の下（「リクエスト」の大見出しの前）に：
```cpp
// ===== 目で隠した物を描くか =====
void RenderQueue::SetSceneHiddenVisible(bool isVisible) { instance_->isSceneHiddenVisible_ = isVisible; }

// ===== 今のビューで描く物か =====
bool RenderQueue::IsDrawnInView(const DrawTarget& target, const std::wstring& windowTitle) {
	// 別のウィンドウの物は描かない（windowTitle が空の物は、どのウィンドウにも描く）
	if (!target.windowTitle.empty() && target.windowTitle != windowTitle) {
		return false;
	}
	// このビューで切ってある層の物は描かない（Game ビューのグリッドなど）
	if (!instance_->isLayerVisible_[static_cast<size_t>(target.layer)]) {
		return false;
	}
	// Hierarchy の目で隠した物は、Scene ビューだけ描かない
	return !target.isSceneHidden || instance_->isSceneHiddenVisible_;
}
```

`FlushMeshList` の for の頭の、ウィンドウの if と層の if（2つ）を、1つに置き換える：
```cpp
	for (const MeshRequest& req : meshes) {
		// このビューで描かない物は飛ばす（別のウィンドウ・切ってある層・目で隠した物）
		if (!IsDrawnInView(req, windowTitle)) {
			continue;
		}
```

`FlushParticle` の for の頭のウィンドウの if を：
```cpp
	for (const ParticleRequest& req : instance_->particleRequests_) {
		// このビューで描かない物は飛ばす（別のウィンドウ・切ってある層・目で隠した物）
		if (!IsDrawnInView(req, windowTitle)) {
			continue;
		}
```

`FlushLine` の for の頭の「ウィンドウ名があっているか確認」のコメントと if を：
```cpp
	for (const LineRequest& req : instance_->lineRequests_) {
		// このビューで描かない物は飛ばす（別のウィンドウ・切ってある層・目で隠した物）
		if (!IsDrawnInView(req, windowTitle)) {
			continue;
		}
```

`OrderOf` の下に：
```cpp
// 2Dのリクエストの描き分けの情報（スプライトも文字も DrawTarget を受け継ぐので、OrderOf と同じく std::visit で1つにする）
const DrawTarget& TargetOf(const RenderQueue::Request2d& request) {
	return std::visit([](const auto& r) -> const DrawTarget& { return r; }, request);
}
```

`Flush2d` の for の中：頭に1つ足して、スプライト・文字の中のウィンドウの if（2つ）を消す：
```cpp
	for (const Request2d& request : instance_->requests2d_) {
		// このビューで描かない物は飛ばす（別のウィンドウ・切ってある層・目で隠した UI）
		if (!IsDrawnInView(TargetOf(request), windowTitle)) {
			continue;
		}
		// --- スプライト ---
		if (const SpriteRequest* sprite = std::get_if<SpriteRequest>(&request)) {
			Bind2d(cmdList, heapStart, current, DrawCategory::Sprite, sprite->blendMode, sprite->rasterizerType);
			RenderContext::DrawSprite(*sprite);
			continue;
		}
		// --- 文字（両面を描く。回して裏返っても消えないように）---
		if (const TextRequest* text = std::get_if<TextRequest>(&request)) {
			Bind2d(cmdList, heapStart, current, DrawCategory::Text, text->blendMode, RasterizerType::SolidNone);
			RenderContext::DrawText2d(*text);
		}
	}
```

### `MyEngine/Graphics/Renderer/SceneRenderer.h`（1つ足す）
`SceneViewOptions` の最後（`editorGrid` の下）に：
```cpp
	bool sceneHidden = true;  // Hierarchy の目で隠した物（DrawTarget::isSceneHidden）を描く（Scene ビューだけ false）
```

### `MyEngine/Graphics/Renderer/SceneRenderer.cpp`（1つ足す）
`ApplyViewOptions` の最後（グリッドの `SetLayerVisible` の下）に：
```cpp
	RenderQueue::SetSceneHiddenVisible(options.sceneHidden);               // 目で隠した物も同じ（積むのは1回、Scene ビューだけ飛ばす）
```

### `MyEngine/Editor/Viewport/EditorViewport.cpp`（`Render` の sceneOptions を置き換え）
```cpp
	// Hierarchy の目で隠した物は、Scene だけ描かない（Game はゲームの見た目のまま）
	const SceneViewOptions sceneOptions = {.fog = flags.sceneFog, .depthOfField = flags.sceneDepthOfField, .editorGrid = flags.sceneGrid, .sceneHidden = false};
```
- 指示付き初期化子（`{.fog = ..., .sceneHidden = false}`）は、**構造体に書いた順**に並べる（順番を変えるとコンパイルエラー）。`sceneHidden` は一番下に足したので、一番後ろに書く。

ここでビルドが通る（まだ何も隠れない。今までと同じ絵）。

---

## 3. パートB：Hierarchy の目（エンジンのエディタ）

### `MyEngine/Editor/Viewport/SceneVisibility.h`（新しく作る）
```cpp
#pragma once
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"


/// <summary>
/// Scene ビューだけで隠す印（Hierarchy の目のアイコン。Unity の Scene Visibility と同じ）
/// <para>隠した物は Scene ビューに描かない・クリックで選べない。Game ビューの絵と、ゲームの動き（当たり・Script）は何も変わらない</para>
/// <para>EntityId で覚えるので、Play / Stop で Entity を作り直しても隠したまま。シーンファイルには書かない（エディタを閉じる・別のシーンを開くと全部見える）</para>
/// <para>描く System が IsHidden を呼んで、Config の isSceneHidden（DrawTarget）に入れる。製品版にも入っている（製品版では誰も隠さないので、いつも false）</para>
/// </summary>
namespace SceneVisibility {

// 自分で隠したか（自分の目を閉じたか）
bool IsHiddenSelf(Handle<Entity> entity);

// 自分か、親・親の親…のどれかを隠しているか（親を隠すと、子も一緒に隠れる）
bool IsHidden(Handle<Entity> entity);

// 目を閉じる・開く（隠していれば見せる、見せていれば隠す）
void Toggle(Handle<Entity> entity);

// 全部見せる（別のシーンを開いたとき。前のシーンの EntityId で、別のシーンの物を隠さないように）
void Clear();

} // namespace SceneVisibility
```

### `MyEngine/Editor/Viewport/SceneVisibility.cpp`（新しく作る）
```cpp
#include "SceneVisibility.h"

#include <unordered_set>

#include "MyEngine/ECS/EntityManager.h"

namespace {
// 目を閉じた物の EntityId（Handle は Play / Stop で作り直すと変わるので、変わらない EntityId で覚える）
// unordered_set：同じ値を2つ持たない入れ物。「入っているか」を、数がいくつあっても1回で調べられる（ハッシュ表）
std::unordered_set<EntityId> hiddenIds;

// Handle → EntityId（居なければ 0）
EntityId IdOf(Handle<Entity> entity) {
	const Entity* owner = EntityManager::Get(entity);
	return (owner != nullptr) ? owner->id : 0;
}
} // namespace


//=============================================================================
// 調べる
//=============================================================================
bool SceneVisibility::IsHiddenSelf(Handle<Entity> entity) { return hiddenIds.contains(IdOf(entity)); }

bool SceneVisibility::IsHidden(Handle<Entity> entity) {
	// 誰も隠していなければ、親をたどらない（製品版・ふだんの描画は、ここで終わる）
	if (hiddenIds.empty()) {
		return false;
	}
	// 自分 → 親 → 親の親 … と一番上までたどる（親子は輪にならない。SetParent が断る）
	for (Handle<Entity> current = entity; current.IsValid();) {
		const Entity* owner = EntityManager::Get(current);
		if (owner == nullptr) {
			return false;
		}
		if (hiddenIds.contains(owner->id)) {
			return true;
		}
		current = owner->parent;
	}
	return false;
}

//=============================================================================
// 変える
//=============================================================================
void SceneVisibility::Toggle(Handle<Entity> entity) {
	const EntityId id = IdOf(entity);
	if (id == 0) {
		return;
	}
	// erase は消した数を返す（0 なら入っていなかった＝見せていたので、隠す）
	if (hiddenIds.erase(id) == 0) {
		hiddenIds.insert(id);
	}
}

void SceneVisibility::Clear() { hiddenIds.clear(); }
```
- `#ifdef USE_IMGUI` で囲まない（描く System が製品版でも呼ぶので）。製品版では誰も `Toggle` しないので、`hiddenIds` はいつも空＝`IsHidden` はすぐ `false` を返す。
- `erase` は消した数を返す（`std::unordered_set` は同じ値を1つしか持たないので、0 か 1）。0 なら入っていなかった＝見せていたので、`insert` で隠す。

### `MyEngine_v1.vcxproj`
`ScenePicking.cpp` の下と `ScenePicking.h` の下に（VS の「既存の項目を追加」でもよい）：
```
    <ClCompile Include="MyEngine\Editor\Viewport\SceneVisibility.cpp" />
    <ClInclude Include="MyEngine\Editor\Viewport\SceneVisibility.h" />
```

### 描く System（6つのファイル。Config を作る所に1行と include）
include に（`Editor` の並びの所）：
```cpp
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
```

| ファイル | 足す所 |
|---|---|
| `Systems/ModelRenderSystem.cpp` | `Draw` の中、`config.windowTitle = windowTitle;` の下 |
| `Systems/PolygonShapeSystem.cpp` | `Draw` の中、`config.camera = camera;` の下 |
| `Systems/ArrowStripSystem.cpp` | `Draw` の中、`config.camera = camera;` の下 |
| `Systems/SpriteRenderSystem.cpp` | `DrawSprites` の中、`config.camera = camera;` の下 |

足す1行：
```cpp
		config.isSceneHidden = SceneVisibility::IsHidden(entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない
```
`Systems/SpriteRenderSystem.cpp` の `DrawImages` は、ループの変数が `item.entity` なので、`config.order = image->order;` の下に：
```cpp
		config.isSceneHidden = SceneVisibility::IsHidden(item.entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない
```
`Systems/TextRenderSystem.cpp` は `Draw` の中、`config.order = text.order;` の下に（影も同じ config で描くので、一緒に隠れる）：
```cpp
		config.isSceneHidden = SceneVisibility::IsHidden(entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない（影も一緒）
```

`Systems/PrimitiveRenderSystem.cpp` は、Config を作るのが `DrawSphere`・`DrawBox`（namespace の中の関数）なので、引数で渡す（4か所）：
- `DrawSphere` と `DrawBox` の引数の最後に `, bool isSceneHidden` を足す：
```cpp
void DrawSphere(const TransformComponent& transform, const PrimitiveRendererComponent& component, Camera* camera, IBLEnvironment* environment, bool isSceneHidden) {
```
```cpp
void DrawBox(const TransformComponent& transform, const PrimitiveRendererComponent& component, Camera* camera, IBLEnvironment* environment, bool isSceneHidden) {
```
- どちらも `config.camera = camera;` の下に：
```cpp
	config.isSceneHidden = isSceneHidden;
```
- `PrimitiveRenderSystem::Draw` の中を：
```cpp
		const bool isSceneHidden = SceneVisibility::IsHidden(entity); // Hierarchy の目で隠した物は、Scene ビューだけ描かない
		if (component.shape == PrimitiveShape::Sphere) {
			DrawSphere(*transform, component, camera, environment, isSceneHidden);
		} else {
			DrawBox(*transform, component, camera, environment, isSceneHidden);
		}
```

### `MyEngine/Editor/Viewport/ScenePicking.cpp`（2か所）
include（`LightGizmo.h` の下）：
```cpp
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
```
`ClosestHit::Consider` を：
```cpp
	// もっと手前なら入れ替える。Hierarchy の目で隠した物は選ばない（見えない物をクリックで選ばないように）
	void Consider(Handle<Entity> candidate, float candidateDistance) {
		if (SceneVisibility::IsHidden(candidate)) {
			return;
		}
		if (candidateDistance >= 0.0f && candidateDistance < distance) {
			entity = candidate;
			distance = candidateDistance;
		}
	}
```

### `MyEngine/Editor/Windows/HierarchyWindow.h`（2か所）
`DrawEntityNode` の下に：
```cpp
	// 行の左の端の目（押すと Scene ビューだけで隠す・見せる）。描いた後は、ツリーの行を描き始める所へ戻す
	static void DrawVisibilityToggle(Handle<Entity> handle);
```
メンバ変数の最後（`revealRequest_` の下）に：
```cpp
	static float eyeColumnX_;         // 目を並べる列の左（ウィンドウの中での位置。親子の深さに関係なく、ここにそろえる）
```

### `MyEngine/Editor/Windows/HierarchyWindow.cpp`（6か所）
include（`EditorHistory.h` の下）：
```cpp
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
```
静的メンバ変数（`revealRequest_` の下）：
```cpp
float HierarchyWindow::eyeColumnX_ = 0.0f;
```
`DrawFolderLabel` の下（`} // namespace` の前）に、目の絵：
```cpp
// 目の絵（見せている：まぶたの線と黒目 / 隠している：まぶたの線と、斜めの線）。min は絵の左上、side は絵の1辺
void DrawEyeIcon(ImDrawList* drawList, const ImVec2& min, float side, bool isOpen, ImU32 color) {
	const ImVec2 center = {min.x + side * 0.5f, min.y + side * 0.5f};
	const float halfWidth = side * 0.42f; // 目の横の長さの半分
	// まぶたのふくらみ：2次ベジェ曲線（AddBezierQuadratic）は、真ん中で制御点までの高さの半分だけふくらむので、ふくらみの2倍の所に制御点を置く
	const float bulge = side * 0.3f;
	const float thickness = (std::max)(1.0f, side * 0.08f);
	const ImVec2 left = {center.x - halfWidth, center.y};
	const ImVec2 right = {center.x + halfWidth, center.y};
	// --- 上と下のまぶた（どちらでも描く）---
	drawList->AddBezierQuadratic(left, ImVec2(center.x, center.y - bulge * 2.0f), right, color, thickness);
	drawList->AddBezierQuadratic(left, ImVec2(center.x, center.y + bulge * 2.0f), right, color, thickness);
	if (isOpen) {
		drawList->AddCircleFilled(center, side * 0.14f, color); // 黒目
	} else {
		// 斜めの線（左上から右下へ。隠している印）
		drawList->AddLine(ImVec2(min.x + side * 0.12f, min.y + side * 0.12f), ImVec2(min.x + side * 0.88f, min.y + side * 0.88f), color, thickness);
	}
}
```
- 絵は画像を使わず、ImGui の線で描く（`AddBezierQuadratic`：2次ベジェ曲線＝始点・制御点・終点の3つで曲がった線。制御点の方へふくらむ）。

`Draw` の一覧の for を、左に目の列を空けるように：
```cpp
		// 左の端に目の列を空けて、ツリーをその分だけ右へずらす（Indent）
		const float eyeColumnWidth = ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.x;
		eyeColumnX_ = ImGui::GetCursorPosX();
		ImGui::Indent(eyeColumnWidth);
		for (const Entity& entity : EntityManager::GetAll()) {
			if (!entity.parent.IsValid()) {
				DrawEntityNode(entity.self);
			}
		}
		ImGui::Unindent(eyeColumnWidth);
```
- `ImGui::Indent`：この後に描く物を、右へずらす（ツリーの子が右へずれるのと同じ仕組み）。一覧の頭で1回ずらして、終わりで戻す。

`DrawEntityNode` の `ImGui::PushID(...)` の下に：
```cpp
	// 目（SetNextItemOpen より先に描く。後に描くと、開く指示を目のボタンが受け取ってしまう）
	DrawVisibilityToggle(handle);
```

`DrawEntityNode` の下（「一覧の下の何も無い所」の大見出しの前）に：
```cpp
//=============================================================================
// 目（Scene ビューだけで隠す）
//=============================================================================
void HierarchyWindow::DrawVisibilityToggle(Handle<Entity> handle) {
	const float rowX = ImGui::GetCursorPosX(); // ツリーの行を描き始める所（親子の深さの分だけ右にある）
	const float side = ImGui::GetTextLineHeight();
	ImGui::SetCursorPosX(eyeColumnX_); // 深さに関係なく、一覧の左の端の列に並べる
	const ImVec2 iconMin = ImGui::GetCursorScreenPos();
	if (ImGui::InvisibleButton("##visibility", ImVec2(side, side))) {
		SceneVisibility::Toggle(handle);
	}
	ImGui::SetItemTooltip("Scene ビューだけで隠す（Game ビューの絵・ゲームの動きは変わらない。保存しない）");
	// --- 絵：隠していれば閉じた目。親を隠しているので隠れている物も閉じた目（自分で隠した物だけ濃く。マウスを乗せても濃く）---
	const bool isHiddenSelf = SceneVisibility::IsHiddenSelf(handle);
	const ImU32 color = ImGui::GetColorU32((isHiddenSelf || ImGui::IsItemHovered()) ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	DrawEyeIcon(ImGui::GetWindowDrawList(), iconMin, side, !SceneVisibility::IsHidden(handle), color);
	// --- ツリーの行を、元の場所から描けるようにする ---
	ImGui::SameLine();
	ImGui::SetCursorPosX(rowX);
}
```
- 行の描き方：ツリーの行は、親子の深さの分だけ右から始まる（`rowX`）。目はいつも左の端の列（`eyeColumnX_`）に描きたいので、カーソルを左の端へ動かして目を描き、`SameLine` で同じ行に戻ってから、カーソルを `rowX` へ戻す。
- 目は `InvisibleButton`（見えないボタン。押せる所だけ作る）の上に、自分で絵を描く。`PushID(entity->id)` の中なので、どの行も同じ `"##visibility"` で、ぶつからない。

### `MyEngine/Scene/SceneManager.cpp`（2か所）
include（`#ifdef USE_IMGUI` の中、`EditorHistory.h` の下）：
```cpp
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
```
`OpenImmediate` の `HierarchyWindow::SetSelected({});` の下に：
```cpp
	SceneVisibility::Clear();         // 目で隠した物も同じ（番号で覚えているので、別のシーンの物を隠さないように）
```

ここでビルドすると、Hierarchy の行の左に目が出る。押すと、Scene ビューだけで消える（Game ビューには出たまま）。

---

## 4. パートC：UI をマウスで動かす（エンジンのエディタ）

### `MyEngine/Systems/SpriteRenderSystem.h`（2か所）
`class SpriteRenderSystem` の前（前方宣言の下）に、画面の四角（今まで .cpp の中にあった `ImageQuad` を、名前を変えてここへ出す）：
```cpp
/// <summary>
/// 画面の上の四角の4つの角（正規化座標：画面の真ん中が0、右と上が＋、端が±1）。Image・Text を置いた形
/// </summary>
struct ScreenQuad {
	Vector2 lb; // 左下
	Vector2 lt; // 左上
	Vector2 rb; // 右下
	Vector2 rt; // 右上
};
```
`CalcScreenPoint` の下に：
```cpp
	/// <summary>
	/// 画面に重ねる四角（Image の絵・Text の文字のかたまり）の4つの角
	/// <para>size はピクセル、pivot は四角のどこを Entity の位置に合わせるか（0,0=左下 1,1=右上）。fill は右の端を縮める割合（Image の Fill。1 なら縮めない）</para>
	/// </summary>
	static ScreenQuad CalcScreenQuad(const Matrix4x4& worldMatrix, ScreenAnchor anchor, const Vector2& pivot, const Vector2& size, float fill = 1.0f);

	// 点が四角の中か（point も正規化座標。4つの辺をぐるっと回って、どの辺から見ても同じ側なら中。回っていても・裏返っていても使える）
	static bool IsInsideQuad(const Vector2& point, const ScreenQuad& quad);
```

### `MyEngine/Systems/SpriteRenderSystem.cpp`（5か所）
1. **消す**：namespace の中の `struct ImageQuad { ... };`（ヘッダの `ScreenQuad` に替わった）。
2. **消す**：namespace の中の `IsInsideQuad`（`SideOf` の下の関数。クラスの関数にした）。`SideOf` は残す。
3. `CalcImageQuad` を置き換える（中身は `CalcScreenQuad` へ移した）：
```cpp
// Image の4つの角を、画面の正規化座標で出す（描くのにも、マウスが乗ったかを調べるのにも使う）。Fill の分だけ右の端を縮める
ScreenQuad CalcImageQuad(const ImageComponent& image, const TransformComponent& transform) {
	return SpriteRenderSystem::CalcScreenQuad(transform.worldMatrix, image.anchor, image.pivot, image.size, image.fill);
}
```
4. `DrawImages` の中の `const ImageQuad quad = ...` を：
```cpp
		const ScreenQuad quad = CalcImageQuad(*image, *transform);
```
5. `CalcScreenPoint` の下に：
```cpp
ScreenQuad SpriteRenderSystem::CalcScreenQuad(const Matrix4x4& worldMatrix, ScreenAnchor anchor, const Vector2& pivot, const Vector2& size, float fill) {
	// --- 1. 四角の4つの角（ピクセル。pivot の所が原点、Y が上）。Fill の分だけ右の端を縮める ---
	const float left = -pivot.x * size.x;
	const float right = left + size.x * std::clamp(fill, 0.0f, 1.0f);
	const float bottom = -pivot.y * size.y;
	const float top = bottom + size.y;

	// --- 2. 画面の正規化座標へ（Transform・アンカーの掛け方は CalcScreenPoint。Image・Text・エディタの枠で同じ）---
	const auto toScreen = [&worldMatrix, anchor](float x, float y) { return CalcScreenPoint(worldMatrix, anchor, {x, y}); };
	return {toScreen(left, bottom), toScreen(left, top), toScreen(right, bottom), toScreen(right, top)};
}

bool SpriteRenderSystem::IsInsideQuad(const Vector2& point, const ScreenQuad& quad) {
	// 左下 → 右下 → 右上 → 左上 → 左下 と回る4つの辺
	const std::array<float, 4> sides = {SideOf(quad.lb, quad.rb, point), SideOf(quad.rb, quad.rt, point), SideOf(quad.rt, quad.lt, point), SideOf(quad.lt, quad.lb, point)};
	const bool allLeft = std::ranges::all_of(sides, [](float side) { return side >= 0.0f; });
	const bool allRight = std::ranges::all_of(sides, [](float side) { return side <= 0.0f; });
	return allLeft || allRight;
}
```
- `IsMouseOver` の中の `IsInsideQuad(...)` は、クラスの関数の中から呼んでいるので、そのまま（`SpriteRenderSystem::` を付けなくてよい）。

### `MyEngine/Editor/Gizmos/UIGizmo.h`（新しく作る）
```cpp
#pragma once
#include <externals/imgui/imgui.h>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"


/// <summary>
/// Scene ビューで、画面に重ねる UI（Image・Text）をクリックで選ぶ・ドラッグで動かす（Unity の Rect Tool の、動かすだけの版）
/// <para>UI は 3D の物より手前に描いてあるので、3D の物より先に選ぶ。重なっていたら、手前に描いてある物（Order が大きい・後に作った物）</para>
/// <para>動かすのは Transform の Translation の x・y（画面のピクセル。1 ピクセルずつ）。1回のドラッグで1件の履歴になる（Ctrl+Z で戻せる）</para>
/// <para>View → Scene View → Pick UI を外すと、UI を選ばない（UI の後ろの 3D の物をクリックしたいとき）。Hierarchy の目で隠した UI も選ばない</para>
/// </summary>
class UIGizmo {
public:
	/// <summary>
	/// Scene ビューの絵の上に、UI の枠を描いて、マウスで選ぶ・動かす。SceneGizmo::Draw から毎フレーム1回（移動のギズモの後）
	/// </summary>
	/// <param name="selected">選んでいる Entity（UI なら枠と位置の印を描く）</param>
	/// <param name="imageMin">絵の左上（画面の座標）</param>
	/// <param name="imageSize">絵の大きさ</param>
	/// <param name="canUseMouse">マウスを使ってよいか（マウスが絵の上にあり、ギズモ・右上の軸の上ではない）</param>
	/// <returns>マウスを使ったか（UI をクリックした・ドラッグ中。true なら、SceneGizmo は 3D の物を選び直さない）</returns>
	static bool Draw(Handle<Entity> selected, const ImVec2& imageMin, const ImVec2& imageSize, bool canUseMouse);

	// 画面に重ねる UI か（Image か Text が付いている）。位置がピクセルなので、3D の移動のギズモは出さない
	static bool IsScreenUI(Handle<Entity> entity);
};
```

### `MyEngine/Editor/Gizmos/UIGizmo.cpp`（新しく作る）
```cpp
#ifdef USE_IMGUI
#include "UIGizmo.h"

#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

#include "MyEngine/Components/ImageComponent.h"
#include "MyEngine/Components/TextComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Viewport/EditorViewport.h"
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Systems/SpriteRenderSystem.h"
#include "MyEngine/Systems/TextRenderSystem.h"

namespace {
constexpr ImU32 kSelectedColor = IM_COL32(255, 160, 40, 255); // 選んだ UI の枠（3D の物の枠と同じ色）
constexpr ImU32 kHoveredColor = IM_COL32(255, 160, 40, 110);  // マウスが乗っている UI の枠（うすく。クリックすると選べる物）
constexpr float kOutlineThickness = 1.5f;                     // 枠の線の太さ（画面のピクセル）
constexpr float kPivotRadius = 3.5f;                          // 位置（Pivot の所）の印の半径（画面のピクセル）

/// <summary>
/// 画面に重ねる UI の置いた形（正規化座標）
/// </summary>
struct UIShape {
	ScreenQuad quad;  // 四角の4つの角
	Vector2 position; // Entity の位置（Pivot の所）
};

/// <summary>
/// UI を押してから離すまでの状態（離したときに、押す前と後を1件の履歴にする）
/// </summary>
struct UIDragState {
	bool isPressed = false;        // UI の上で押して、まだ離していないか
	bool isDragging = false;       // 押したまま少し動かしたか（動かすまでは位置を変えない。クリックで選ぶだけのとき、ずれないように）
	Handle<Entity> entity;         // 動かしている物
	std::vector<std::byte> before; // 押したときの Transform（ComponentEditor::Save の写し）
	Vector3 startTranslation;      // 押したときの Translation
	Matrix4x4 parentInverse;       // 親のワールド行列の逆行列（画面で動かした量を、親から見た量に直す）
	ImVec2 startMouse;             // 押したときのマウスの位置（画面の座標）
};
UIDragState drag;

//=============================================================================
// UI の形
//=============================================================================
// 画面に重ねる UI の形（Image があれば Size の四角、無ければ Text の文字のかたまり）
// どちらも無い・描いていない（無効・目で隠した）物は false
bool CalcShape(Handle<Entity> entity, UIShape& shape) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	if (transform == nullptr || !EntityManager::IsActiveInHierarchy(entity) || SceneVisibility::IsHidden(entity)) {
		return false;
	}
	const Matrix4x4& m = transform->worldMatrix;
	if (const ImageComponent* image = EntityManager::Get<ImageComponent>(entity); image != nullptr && image->enabled) {
		shape.quad = SpriteRenderSystem::CalcScreenQuad(m, image->anchor, image->pivot, image->size); // Fill は掛けない（Image の大きさ全部）
		shape.position = SpriteRenderSystem::CalcScreenPoint(m, image->anchor, {0.0f, 0.0f});
		return true;
	}
	if (const TextComponent* text = EntityManager::Get<TextComponent>(entity); text != nullptr && text->enabled) {
		shape.quad = SpriteRenderSystem::CalcScreenQuad(m, text->anchor, text->pivot, TextRenderSystem::CalcTextSize(entity));
		shape.position = SpriteRenderSystem::CalcScreenPoint(m, text->anchor, {0.0f, 0.0f});
		return true;
	}
	return false;
}

// 画面の座標（Scene ビューの絵の上。Y は下向き）→ 正規化座標（真ん中が0、右と上が＋、端が±1）
Vector2 ToNdc(const ImVec2& point, const ImVec2& imageMin, const ImVec2& imageSize) {
	return {(point.x - imageMin.x) / imageSize.x * 2.0f - 1.0f, 1.0f - (point.y - imageMin.y) / imageSize.y * 2.0f};
}

// 正規化座標 → 画面の座標（ToNdc の逆）
ImVec2 ToImage(const Vector2& ndc, const ImVec2& imageMin, const ImVec2& imageSize) {
	return {imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x, imageMin.y + (0.5f - ndc.y * 0.5f) * imageSize.y};
}

//=============================================================================
// クリックで選ぶ
//=============================================================================
// point の下にある UI のうち、一番手前に描いてある物（描く順と同じ：Order が大きい物。同じなら番号が大きい＝後に作った物）
Handle<Entity> PickUI(const Vector2& point) {
	Handle<Entity> best;
	int bestOrder = 0;
	EntityId bestId = 0;
	const auto consider = [&](Handle<Entity> entity, int order) {
		UIShape shape;
		if (!CalcShape(entity, shape) || !SpriteRenderSystem::IsInsideQuad(point, shape.quad)) {
			return;
		}
		const Entity* owner = EntityManager::Get(entity);
		const EntityId id = (owner != nullptr) ? owner->id : 0;
		// std::tie：「1つ目を比べて、同じなら2つ目」をまとめて書ける（描く順の並べ方と同じ）
		if (!best.IsValid() || std::tie(order, id) > std::tie(bestOrder, bestId)) {
			best = entity;
			bestOrder = order;
			bestId = id;
		}
	};
	EntityManager::ForEach<ImageComponent>([&consider](Handle<Entity> entity, const ImageComponent& image) { consider(entity, image.order); });
	EntityManager::ForEach<TextComponent>([&consider](Handle<Entity> entity, const TextComponent& text) { consider(entity, text.order); });
	return best;
}

//=============================================================================
// ドラッグで動かす
//=============================================================================
// 押した：今の Transform を覚える（動かし始めるのは、少し動かしてから）
void BeginPress(Handle<Entity> entity) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	const ComponentEditor* editor = ComponentEditorRegistry::Find<TransformComponent>();
	if (transform == nullptr || editor == nullptr) {
		return;
	}
	drag.isPressed = true;
	drag.isDragging = false;
	drag.entity = entity;
	drag.before = editor->Save(transform);
	drag.startTranslation = transform->translation;
	// 親のワールド行列（親がいなければ単位行列）の逆行列
	const Entity* owner = EntityManager::Get(entity);
	const TransformComponent* parent = (owner != nullptr) ? EntityManager::Get<TransformComponent>(owner->parent) : nullptr;
	drag.parentInverse = (parent != nullptr) ? Inverse(parent->worldMatrix) : MakeIdentity4x4();
	drag.startMouse = ImGui::GetMousePos();
}

// 押している間：押した所からマウスが動いた分だけ、Translation を動かす
void UpdateDrag(const ImVec2& imageSize) {
	TransformComponent* transform = EntityManager::Get<TransformComponent>(drag.entity);
	if (transform == nullptr) {
		drag = {}; // ドラッグ中に消された（Undo など）
		return;
	}
	// 少し動かすまでは動かさない（IsMouseDragging：押した所から決まった距離より動いたか。ImGui の io.MouseDragThreshold）
	if (!drag.isDragging && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		return;
	}
	drag.isDragging = true;
	// --- マウスが動いた量（画面のピクセル）→ 1280×720 の画面のピクセル（UI の位置の単位。Y は上が＋）---
	const ImVec2 mouse = ImGui::GetMousePos();
	const Vector3 worldMove = {
	    (mouse.x - drag.startMouse.x) / imageSize.x * Renderer::kSpriteBaseWidth,
	    -(mouse.y - drag.startMouse.y) / imageSize.y * Renderer::kSpriteBaseHeight,
	    0.0f,
	};
	// --- 親から見た量に直す（親が回っている・大きくしてある物も、マウスについてくるように）---
	// 動かした量は「向き」なので、点の変換の差（移動の分が引かれて消える）で、回転と大きさだけを戻す
	const Vector3 localMove = TransformPoint(worldMove, drag.parentInverse) - TransformPoint({0.0f, 0.0f, 0.0f}, drag.parentInverse);
	// 1 ピクセルずつ（Inspector の数字が 150.37 のようにならないように）。Z はそのまま
	transform->translation = {
	    std::round(drag.startTranslation.x + localMove.x),
	    std::round(drag.startTranslation.y + localMove.y),
	    drag.startTranslation.z,
	};
}

// 離した：動かしていたら、押す前と後を1件の履歴にする（Inspector で値を変えたときと同じ入口）
void EndPress() {
	const ComponentEditor* editor = ComponentEditorRegistry::Find<TransformComponent>();
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(drag.entity);
	if (drag.isDragging && editor != nullptr && transform != nullptr) {
		EditorHistory::RecordComponentChange(drag.entity, *editor, drag.before, editor->Save(transform));
		EditorHistory::CommitComponentChanges();
	}
	drag = {};
}

//=============================================================================
// 枠を描く
//=============================================================================
void DrawShape(ImDrawList* drawList, const UIShape& shape, const ImVec2& imageMin, const ImVec2& imageSize, ImU32 color, bool drawPivot) {
	const auto toImage = [&imageMin, &imageSize](const Vector2& ndc) { return ToImage(ndc, imageMin, imageSize); };
	// 左下 → 右下 → 右上 → 左上 の順に回る四角（回っていても、そのままの形で描ける）
	drawList->AddQuad(toImage(shape.quad.lb), toImage(shape.quad.rb), toImage(shape.quad.rt), toImage(shape.quad.lt), color, kOutlineThickness);
	if (drawPivot) {
		drawList->AddCircle(toImage(shape.position), kPivotRadius, color, 0, kOutlineThickness);
	}
}
} // namespace


//=============================================================================
// 描画
//=============================================================================
bool UIGizmo::Draw(Handle<Entity> selected, const ImVec2& imageMin, const ImVec2& imageSize, bool canUseMouse) {
	// --- 押している間は動かす。離したら履歴に積む（マウスが絵の外へ出ても、離すまで続ける）---
	if (drag.isPressed) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			UpdateDrag(imageSize);
		} else {
			EndPress();
		}
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true); // 絵の外（黒い余白）には描かない
	// --- マウスの下の UI（Pick UI がオンのときだけ。押している間は探さない）---
	const bool canPick = EditorViewport::GetViewFlags().scenePickUI && canUseMouse && !drag.isPressed;
	const Handle<Entity> hovered = canPick ? PickUI(ToNdc(ImGui::GetMousePos(), imageMin, imageSize)) : Handle<Entity>{};
	UIShape shape;
	if (hovered.IsValid() && hovered != selected && CalcShape(hovered, shape)) {
		DrawShape(drawList, shape, imageMin, imageSize, kHoveredColor, false);
	}
	// --- 選んでいる UI の枠と、位置（Pivot の所）の印 ---
	if (CalcShape(selected, shape)) {
		DrawShape(drawList, shape, imageMin, imageSize, kSelectedColor, true);
	}
	drawList->PopClipRect();

	// --- クリック：UI の上なら、それを選んで、ドラッグで動かせるようにする ---
	if (hovered.IsValid() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		HierarchyWindow::SelectAndReveal(hovered); // Hierarchy の一覧でも見えるようにする
		BeginPress(hovered);
		return true;
	}
	return drag.isPressed;
}

bool UIGizmo::IsScreenUI(Handle<Entity> entity) { return EntityManager::Get<ImageComponent>(entity) != nullptr || EntityManager::Get<TextComponent>(entity) != nullptr; }
#endif
```
- `CalcShape` は、Image があれば Image の四角、無ければ Text の四角（両方付いている＝文字の背景の板なら、板の方が大きいので板で選ぶ）。
- `PickUI` は Image・Text の両方を回す。両方付いている Entity は2回調べるが、同じ四角なので結果は同じ。
- 押している間（`drag.isPressed`）は、マウスが絵の外へ出ても続ける（ImGui はボタンを離すまで `IsMouseDown` を返す）。

### `MyEngine/Editor/Gizmos/SceneGizmo.cpp`（4か所）
include（`PolygonShapeGizmo.h` の下）：
```cpp
#include "MyEngine/Editor/Gizmos/UIGizmo.h"
```
`Manipulate` の頭を（UI には3D のギズモを出さない）：
```cpp
	TransformComponent* transform = EntityManager::Get<TransformComponent>(selected);
	// 画面に重ねる UI（Image・Text）は出さない（位置が画面のピクセルなので、3D の矢印では動かせない。UIGizmo がドラッグで動かす）
	if (transform == nullptr || EntityManager::Get<FolderComponent>(selected) != nullptr || UIGizmo::IsScreenUI(selected)) {
		EndDrag(); // ドラッグ中に消された・選ぶのをやめた
		return false;
	}
```
`HandleClick` の頭を（引数を `canUseMouse` 1つにして、ギズモの上かは呼ぶ側で決める）：
```cpp
// canUseMouse：マウスが絵の上にあり、ギズモ・右上の軸の上ではない（SceneGizmo::Draw で決める。UIGizmo と同じ値）
void HandleClick(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, bool canUseMouse) {
	if (!canUseMouse || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}
```
- 前にあった「`// ギズモ・右上の軸の上を押した時は、そちらの操作（選び直さない）`」の if（`isGizmoShown && ImGuizmo::IsOver()` …）は**消す**（`SceneGizmo::Draw` へ移した）。
`SceneGizmo::Draw` の最後の2行を：
```cpp
	const bool isGizmoShown = Manipulate(selected, camera);
	// ギズモ・右上の軸の上は、そちらの操作（UI も 3D の物も選び直さない）。ギズモの上かは、Manipulate の後で分かる
	const bool isOnGizmo = (isGizmoShown && ImGuizmo::IsOver()) || ImGuizmo::IsUsingAny() || ImGuizmo::IsViewManipulateHovered();
	const bool canUseMouse = isHovered && !isOnGizmo;
	// 画面に重ねる UI が先（3D の物より手前に描いてあるので）。UI をクリックした・ドラッグ中なら、3D の物は選ばない
	if (UIGizmo::Draw(selected, imageMin, imageSize, canUseMouse)) {
		return;
	}
	HandleClick(camera, imageMin, imageSize, canUseMouse);
}
```

### `MyEngine/Editor/Viewport/EditorViewport.h`（1つ足す）
`ViewFlags` の `sceneGrid` の下に：
```cpp
		bool scenePickUI = true;       // Scene ビューで、画面に重ねる UI（Image・Text）をクリックで選ぶ（外すと、UI の後ろの 3D の物を選べる）
```

### `MyEngine/Editor/ImGuiManager.cpp`（View メニュー）
`ImGui::MenuItem("Grid###sceneGrid", ...)` の下に：
```cpp
			ImGui::MenuItem("Pick UI", nullptr, &viewFlags.scenePickUI);
			ImGui::SetItemTooltip("Image・Text をクリックで選ぶ・ドラッグで動かす（外すと、UI の後ろの 3D の物を選べる）");
```

### `MyEngine_v1.vcxproj`
`SceneGizmo.cpp` の下と `PolygonShapeGizmo.h` の下に：
```
    <ClCompile Include="MyEngine\Editor\Gizmos\UIGizmo.cpp" />
    <ClInclude Include="MyEngine\Editor\Gizmos\UIGizmo.h" />
```

---

## 5. パートD：使い方
### 5.1 目
- Hierarchy の行の左の目を押すと、その Entity と子が **Scene ビューから消える**（Game ビュー・再生中のゲームはそのまま）。もう一度押すと戻る。
- 隠している物の目は、斜線の入った目。**親を隠したので隠れている子**は、薄い斜線の目（子の目を押しても、親を隠している間は見えない）。
- 隠した物は、Scene ビューのクリックで選ばれない（Hierarchy からは選べる）。
- チュートリアルの説明なら：作り終わった説明（`Text_Move`）の目を閉じてから、次の説明（`Text_Jump`）を作る。

### 5.2 UI をマウスで動かす
- Scene ビューで Image・Text をクリック → 選ばれて（Hierarchy でも見えるように開く）、オレンジの枠と、位置（Pivot の所）の丸が出る。
- そのままドラッグすると動く（1 ピクセルずつ）。離すと1件の履歴になる（Ctrl+Z で戻せる）。
- マウスを乗せた UI には、うすい枠が出る（クリックすると選べる物）。
- UI の後ろの 3D の物（足場など）を選びたいとき：**View → Scene View → Pick UI** を外す。または、UI の目を閉じる。
- 1 ピクセルずつ細かく合わせるのは、今までどおり Inspector の Position。

---

## 6. 確かめたこと
Debug のエディタ（AL4_01 のコピーの Level01）で、ImGui に直接マウスの操作を入れて（確かめた後は外した）、画面を撮って確かめた。

| 確かめたこと | 結果 |
|---|---|
| Hierarchy の目 | どの行にも、左の端の同じ列に出る（深い子も同じ列） |
| Scene ビューで「移動」の文字をクリック | `Text_Move` が選ばれ、Hierarchy の `Tutorial` が開く。文字のかたまりに枠、真ん中に丸 |
| 右へ 50・上へ 20 ドラッグ（絵の横 451 ピクセル） | Position が (0, 150) → **(142, 207)**（計算どおり。小数無し） |
| Ctrl+Z | (0, 150) に戻る |
| `Text_Move` の目を閉じる | Scene ビューから文字が消える。**Game ビューには出たまま** |
| 消えた文字の所をクリック | 文字は選ばれず、後ろの Trigger Activator（3D の箱）が選ばれる |
| 親（PauseMenu。G9c で作る）の目を閉じる | 子の UI が全部 Scene ビューから消える。子の目は薄い斜線 |
| BackWall（奥の大きな壁）・Ground の目を閉じる | Scene ビューから壁が消えて、奥の背景（Sprite Renderer）が見える（3D の物も隠れる） |
| パートA の後（何も隠していない） | 絵は今までと同じ（Release でポーズ・幕も G9c と同じに動いた） |
| Debug・Release のビルド | 通る（警告は前からある DIRECTINPUT_VERSION だけ） |

ドラッグした後（左の Hierarchy の目・Scene ビューの枠と丸・Inspector の Position 142, 207）：

![UI をドラッグした後](images/g9b_drag.png)

PauseMenu の目を閉じた所（子の目は薄い斜線。Scene ビューからメニューが消えた）：

![目を閉じた所](images/g9b_eye.png)

---

## 7. 決めたこと・後でやること
- 目は**保存しない**（Unity はプロジェクトの設定に残すが、今は要らない）。エディタを閉じる・別のシーンを開くと、全部見える。
- 隠れるのは「描く System が `config.isSceneHidden` を書く物」だけ。線のギズモ・パーティクル・Trail・Elastic Surface は隠れない。
- UI のマウス操作は**動かすだけ**。大きさ（Size）・回転は Inspector で。Unity の Rect Tool のように、枠の角をつかんで大きさを変えるのは後で（要るなら）。
- 動かすのはドラッグした1つだけ（複数選んでまとめて動かすのは、エディタが1つしか選べないので無し）。
