# Editor系の作業

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
| エディタで文字列を編集するときは `EditorWidgets::InputText`（`std::string` を直接編集）を使う（2026-09-18） | `char[64]` のバッファだと日本語は21文字で打てなくなり、コピーのときに文字の途中で切れることもある。`ImGuiManager` は `USE_IMGUI` の中にしか無いので、Releaseでもコンパイルされるウィンドウから使えるように、共通部品は `EditorWidgets.h`（ヘッダだけ）に置く |
| ImGuiのフォントは 1.92 の「使われた文字をその場で足す」方式にする（2026-09-18） | 前の初期化（SRV 1個だけ渡す古い形）だと、最初に決めた範囲（常用漢字＋かな）の文字しか出ず、珍しい漢字や ★♪① が「?」になる。新しい初期化でSRVをコールバックで渡すと、どの文字も出る |
| ゲームのシーンが作ったEntityは、シーンの親Entity（`sceneRoot_`）の下に入れ、`Finalize` で親ごと消す（2026-09-18） | エンジンの Stop は「`Finalize` → シーンを作り直して `Initialize`」（`SceneManager::ReloadImmediate`）。消さないと Stop のたびに同じEntityが増える。親にまとめておけば1回の `Destroy` で済み、Hierarchyでもまとまって見える |
| IBL（`IBLEnvironment`）はシーンが1つ持ち、`ModelRenderSystem::Draw` に渡す（2026-09-18） | Unityの「Lighting設定のEnvironment」がシーンごとにあるのと同じ形。Componentにポインタを持たせないので原則2も守れる |

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
| **3** | **`RenderComponent`（Entityがモデルを描く）＋ モデル選択（最小のアセット一覧）。この後 `Model.md` Step 2でモデルのノード階層をEntityとして取り込む** | **進行中：3a実行確認済み、3b写経手順作成** |
| 4 | ライトのComponentをEntityに載せる（`Light.md` Step 6） | 3の後 |
| **3+** | **使い勝手の改善：Hierarchyでその場で名前の変更（ダブルクリック / F2）、日本語の名前（長さの上限なし・どの文字も出るフォント）、縦長のAdd Component（カテゴリを押すと下に開く＋検索）** | **作業中**（3fと一緒に「まとめて」） |
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

### 次のStepでやること（ここではやらない）
- Step 3：`RenderComponent`。`Add Component` の最初の項目になる。ここで初めてヒエラルキーで作ったEntityが画面に見える。
- Step 4：ライトのComponentをEntityに載せる（`Light.md` Step 6）。

---

## Step 3：RenderComponentの写経手順

## 作業ルール

- コードはこのMarkdownに書き、ユーザーが実装ファイルへ写経する。
- 作業ブランチは `codex/component-render-guide`。Pushは禁止。
- 実装済み・ビルド済み・実行確認済みを分けて記録する。
- 既存の作業途中の変更は残す。完了した手順や設計理由も残す。

## 進める順番

| 区切り | 内容 | 状態 |
|---|---|---|
| 3a | データ、追加予約、取得、Entityと一緒の破棄 | 完了（ユーザーより実行確認OKの報告。2026-09-18） |
| 3b | 描画処理（`RenderSystem`）、カメラ・IBLの受け渡し | コードは反映済み（`RenderSystem.h/.cpp` あり）。**まだどこからも呼ばれていないので画面には出ない** |
| 3c | Inspector、Add Component、モデル選択、ゲーム側から `RenderSystem::Draw` を呼ぶ | 反映済み（2026-09-18。エンジン・ゲームとも） |
| 3d | ビルボード（`BillboardMode`：None / Full / AxisY）。3Dモデルでも潰れない形にVSを直す | 反映済み（2026-09-18） |
| 3e | 名前をカテゴリ前提に直す（`RenderComponent` → `ModelRendererComponent`）＋ Add Componentをカテゴリのメニューに ＋ ゲーム側に残った古いヘッダの掃除 | 完了（2026-09-18 実行確認OK。`Entity::render` のメンバ名はそのまま） |
| **3f** | **ゲーム側のMonsterBallをEntityへ移行（初期値付きの追加、IBLはシーンが持つ）** | **作業中**（下の「まとめて」のパートE） |

3a〜3cで「Create Entity → Add Component → モデルを選ぶ → 画面に出る」が通った。
`Editor.md` Step 3全体の完了は3fまで確認してからとする。

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| 実体はEntityManagerの `SlotMap<RenderComponent>` が所有し、EntityはHandleを持つ | 既存のTransformと同じ方式。Componentのデータを型別にまとめる |
| `Renderer::ModelConfig` をComponentに丸ごと持たせない | Camera・IBL・行列のポインタと `std::wstring` が入っている。Componentは値とIDを持ち、描画処理が一時的なConfigへ変換する |
| 追加は予約し、次のUpdate冒頭にまとめて反映する | InspectorやSystemの処理中にSlotMapを増やして、取得済みポインタを無効化しないため |
| 1 EntityにつきRenderComponentは1個 | 複数メッシュは既存のモデルアセットが持つ。ノードのEntity化は `Model.md` Step 2で扱う |
| IBLの所有方法は3bで整理する。今回はIBLポインタを足さない | 現在のIBLEnvironmentはGPUリソースを所有し、Handle管理されていない。先に生ポインタを保存して済ませると設計原則に反する |
| ビルボードは `bool` ではなく `BillboardMode`（None / Full / AxisY）で持つ（3d） | 木やキャラの立ち絵は「Y軸だけ回す」が欲しくなる。`bool` で作ると後で2つ目の `bool` が生える。enumならInspectorの選択肢もmagic_enumで勝手に増える |
| ビルボードの計算はVS（シェーダー）でやる。CPUでやらない（3d） | カメラの軸（`gCamera.right/up`）はビューごとに入る定数なので、**Sceneビューではデバッグカメラ、Gameビューではゲームのカメラを向く**。CPUでやると `RenderSystem` が受け取る1つのカメラ（ゲームのカメラ）しか向けない |
| ビルボードのVSは頂点のzも使い、立体のモデルを「剛体として」回す（3d） | 前のVSはzを捨てて板に潰していた（XY平面の四角形専用）。RenderComponentは何のモデルでも選べるので、立体を選ぶと潰れてしまう。XY平面の板（`plane.obj`、ギズモのアイコン）は前と同じ結果になる |
| `ModelConfig` だけ `BillboardMode`、他の図形のConfigは `bool isBillboard` のまま（3d） | 図形（Rect3dなど）は板として使うのでFullだけで足りる。全部変えると触るファイルが増えるだけ。`PushMesh` で `true → Full` に読み替える |
| Componentの名前は「何を・どうする」で付ける：`ModelRendererComponent`（3e） | `Render` だけだと2D（Sprite）・パーティクル・スキンメッシュが来たときに区別できない。このエンジンの素材の呼び名は「Model」（`ModelManager` / `ModelConfig` / `DrawModel`）なので `Model` + `Renderer`。Unityの `MeshRenderer` は「MeshFilter＋MeshRenderer」の2つ組の片方なので、1つで両方持つこのComponentには合わない |
| 3D / 2D は**型の名前ではなくカテゴリ（Add Componentのメニュー）**で分ける（3e） | `Render3DComponent` のように名前に入れると、「何を描くか」が名前から消える。`ModelRenderer`（3D）と `SpriteRenderer`（2D）は名前だけで区別できるので、3D / 2D はメニューの分類で十分。Unityも同じ（Add Component → Rendering / Mesh / UI …） |
| カテゴリは `enum class ComponentCategory` で持ち、表示名はmagic_enumで作る（3e） | 文字列で持つと打ち間違いで別カテゴリができる。enumならメニューの並び順もenumの順に決まる |
| Systemの名前は `〇〇RenderSystem`（Componentの `Renderer` と揃えない）（3e） | Component＝「〇〇を描く設定を持つもの（Renderer）」、System＝「〇〇の描画をまとめてやる処理（RenderSystem）」。役割が違うので語尾も変える |
| ファイルの置き場所（カテゴリごとのフォルダ）は、カテゴリに2つ目のComponentが来たときに決める（3e） | 今フォルダを作ると中身が1つだけのフォルダが並ぶ。`Graphics/` の下に置くと、下の層（Graphics）が上の層（Entity）を知ることになり依存の向きが逆になる。当面は `MyEngine/Entity/` に置く |

## Step 3a：データと寿命の管理

### 仕組みの説明

追加ボタンを押したフレームはHandleだけを予約し、次のUpdate冒頭で実体を作る。
そのため、`RequestAddRender` の直後の `GetRender` は、初回追加ならまだ `nullptr`。
3cのInspectorでは予約中を表示し、次のフレームから編集欄を出す。

EntityのSlotMapとRenderComponentのSlotMapは別の配列なので、
`renders_.Create()` はこの関数内の `Entity*` を無効にしない。
一方、以前取得した `RenderComponent*` は無効になる可能性があるため、フレームをまたいで保持しない。

予約した後でEntityが破棄された場合、世代付きHandleの検査で追加を飛ばす。
同じスロット番号で別Entityが作られても、世代が異なるため新しいEntityに誤って追加されない。
破棄予約されたEntityがまだ生存中なら一旦追加される場合があるが、既存の `FlushDestroy` で一緒に消える。

モデルやテクスチャのHandleは共有アセットへの参照。
RenderComponentを消す際に、ModelManagerやTextureManagerの共有アセットまで解放してはいけない。

## Step 3b：Entityのモデルを描く

### このStepでできること

RenderComponentとTransformComponentから一時的なModelConfigを作り、既存のRendererへ渡す。
モデル選択UIとAdd Componentは3c。今回はゲーム側でモデル番号を設定して描画を確認する。
MonsterBallの削除はまだ行わない。

### 今回の設計と理由

| 決めたこと | 理由 |
|---|---|
| 描画処理は `RenderSystem` に置く | データを持つComponent、寿命を管理するEntityManager、描画するSystemを分ける |
| シーンは描画する親EntityのHandleを渡す。親自身とその子孫だけ描く | EntityManagerが全シーン共通でも、別シーンのEntityを混ぜない。WindowManagerから全Entityをウィンドウの数だけ描く方法は採らない |
| CameraとIBLは毎回引数で渡し、Systemは保存しない | ARCHITECTURE.mdが許可する一時的な参照。IBLの所有者は呼び出し側に残す |
| この段階では1回の呼び出しでIBLを共用する | IBLアセットのHandle管理をまだ実装していないため。ComponentごとのIBL選択は未対応として残す |
| 親の `isActive == false` は子孫にも反映する | 親を非表示にしたとき、子だけ残らないようにする。Componentの `enabled` はそのEntityの描画だけに効く |
| PBRでIBLが未準備なら描画を飛ばす | 現在のRenderContextはPBRに非ゼロのIBL定数バッファを要求する。無効なGPUアドレスを送らない |

この「描画する親」はシーンIDの本実装ではなく、現在あるEntityの親子構造を使った範囲指定。
3cではHierarchyで作ったモデルもその親の子として配置する。親の外にあるEntityはこの呼び出しでは描かれない。
同じ親を1フレームに複数回指定すると重複描画になるので、シーンのDrawで1回だけ呼ぶ。
---

## Step 3c：InspectorのRender区画・Add Component・モデル選択

### 目的
3bまでで「データ（`RenderComponent`）」と「描く処理（`RenderSystem`）」は揃った。足りないのは**入口と出口**の2つ。

1. **入口**：`modelHandle` を入れる手段が無い。`Add Component` は `(none yet)` のままで、Renderを足すこともできない
2. **出口**：`RenderSystem::Draw` を呼ぶ場所が無い（エンジン内のどこからも呼ばれていない）

この2つを繋ぐと、**「Create Entity → Add Component → モデルを選ぶ → 画面に出る」**が通る。ここがStep 3の山。

---

### 解説

**なぜ `root` を省略できるようにするのか**
- `RenderSystem::Draw` は「rootの配下のEntityだけ描く」作りになっている。複数ウィンドウ・複数シーンのときに要る仕組みなので、これ自体は残す。
- ただしヒエラルキーの `Create Entity` は**親を持たないEntity（root扱い）**を作る。なので `Scene` という空Entityを作ってそれをrootに渡すと、**新しく作ったEntityは毎回ドラッグでScene配下に入れないと描かれない**。
- 「Entityを作る → モデルを選ぶ → 出る」が1回で通らないのは、このStepの目的に反する。なので**rootを省略したら全部対象**にした。これは `bool belongsToRoot = !root.IsValid();` の1行で済む。

**`Add Component` が押した瞬間に反映されない理由**
- `RequestAddRender` は**予約だけ**で、実際に足されるのは次のフレームの頭（`WindowManager` が `EntityManager::FlushComponentChanges()` を呼ぶところ）。
- Inspectorを描いている最中に `SlotMap<RenderComponent>` を増やすと、`std::vector` が再確保されて、いま持っているポインタが別の場所を指す。それを避けるため（ヒエラルキーの `ApplyRequests` と同じ理由）。
- 見た目には1フレーム遅れるだけなので気づかない。ただし**押した直後の同じフレームでは `GetRender` がまだ `nullptr`** なので、`IsRenderAddPending` で二重に予約されないようにしている。

**`magic_enum` で選択肢を作る利点**
- `ShadingType` に種類を足したとき、Inspectorを直さなくても選択肢が増える。
- `GlobalVariables::MakeEnumCombo` が既に同じやり方をしている（`magic_enum::enum_names<E>()`）ので、エンジンの中で書き方がそろう。
- `string_view` から `std::string` を作っているのは、ImGuiが `const char*`（ヌル終端）を要求するから。エディタのコンボなので毎フレームの確保は気にしなくてよい。

**モデル一覧を毎フレーム作らない理由**
- `recursive_directory_iterator` はディスクを触る処理。毎フレームやると、フォルダの数だけファイルシステムに問い合わせが飛ぶ。
- 最初に開いたときだけ走査して覚えておき、フォルダにモデルを足したときは `Refresh` を押す。

**モデルを選んだ瞬間に一瞬止まる**
- `ModelManager::Load` は中で `UploadContext::Flush()` を呼び、**GPUの転送完了を待つ**。大きいモデルだと目に見えて止まる。
- `UploadContext` は専用のCommandListとFenceを持っていてフレーム描画の同期とは独立しているので、ImGuiを組んでいる途中に呼んでも壊れない（ヘッダのコメントにもそう書いてある）。
- 同じパスは `pathToHandle_` のキャッシュが返るので、選び直しても2回目は止まらない。

**なぜ `ModelConfig` をそのままComponentに持たせないのか**（3aの決定のおさらい）
- `ModelConfig` には `Camera*`、`IBLEnvironment*`、`const Matrix4x4*`、`std::wstring` が入っている。ARCHITECTURE.md 原則2（Componentはポインタ・可変長・GPUリソースを持たない）に反する。
- なので `RenderComponent` は値とIDだけを持ち、`RenderSystem` が毎フレーム一時的な `ModelConfig` に詰め替える。詰め替えの場所が1か所なので、増えた項目の入れ忘れも起きにくい。

---

## Step 3d：RenderComponentのビルボード

### 目的
`RenderComponent` に「カメラの方を向かせる」設定を足す。種類は3つ。

| `BillboardMode` | 動き | 使いどころ |
|---|---|---|
| `None` | Transformの回転どおり（今まで通り） | 普通のモデル |
| `Full` | カメラの向きに**完全に**合わせる。上下にも傾く | アイコン、エフェクト、名札、HPバー |
| `AxisY` | **Y軸まわりだけ**回す。上はワールドの上のまま | 木、草、キャラの立ち絵（上から見ても傾かない） |

### 今のビルボードの問題（エンジンにはもう `isBillboard` がある）
`ModelConfig::isBillboard` と、それを受けるVSの処理は既にある。ただし今のVSは

```hlsl
float3 p = center + gCamera.right * (input.position.x * sx)
                  + gCamera.up * (input.position.y * sy);   // ← z を使っていない
```

と、**頂点のzを捨てて、カメラに平行な板に押し潰している**。XY平面の四角形（ギズモのアイコン、`plane.obj`）専用の作り。
RenderComponentは何のモデルでも選べるので、立体（`teapot`、`bunny`）を選ぶと**ぺちゃんこの切り絵**になる。さらに法線が全部同じ向きになるので陰影も消える。

→ **zも使って、モデル全体を剛体としてカメラの方へ回す**形に直す。XY平面の板は、z=0なので**前と全く同じ結果**になる。

### 仕組み（VSでやること）
1. カメラの軸を3本作る：右（`gCamera.right`）・上（`gCamera.up`）・奥（`右 × 上`）
   - `AxisY` のときは、上を `(0,1,0)` に固定し、右はカメラの右を水平にしたもの
2. `world` から**位置と大きさだけ**取り出す（回転は捨てて、1の軸で置き換える）
3. 頂点を `中心 + 右 * x + 上 * y + 奥 * z`（x,y,zは大きさを掛けた後）で組み立てる
4. 法線も同じ軸で回す（大きさで割ってから。`Light.md` Step 5.7 と同じ理由）

**なぜCPUでなくVSでやるのか**：`gCamera.right / up` は**ビューごと**に入る定数（`SceneRenderer::UploadCameraCB` がビューごとに書いている）。なのでSceneビューでは**デバッグカメラ**、Gameビューでは**ゲームのカメラ**を向く。CPU（`RenderSystem`）でやると、受け取った1つのカメラ（ゲームのカメラ）しか向けず、Sceneビューで見るとそっぽを向く。

### 解説

**「左手系なので 右 × 上 = 奥」の確認**
- カメラが回転していないとき：右 `(1,0,0)`、上 `(0,1,0)`。`cross` = `(0,0,1)` = +Z。このエンジン（左手系）のカメラは+Zを見るので「奥」で合っている。
- `plane.obj` の見える面は法線 `(0,0,-1)`（エンジンで読んだ後）。ビルボードで `-奥` ＝ **カメラの方**を向くので、表が見える。

**数値で確かめた結果**（エンジンの `MakeAffineMatrix` / `Inverse` と、`SceneRenderer` と同じ `right / up` の取り出し方を使い、カメラの向きを117通り試した）
```
Full  : 法線がカメラ側・表面が見える = 117 / 117
AxisY : 法線がカメラ側・表面が見える・法線が水平 = 117 / 117
```
- Entityの回転 `(0.4, 1.1, 0.3)` と非均一スケール `(2,1,1)` を入れた状態で試している（回転は無視され、大きさは効く）。
- 「表面が見える」＝画面上で時計回り。`SolidBack`（背面カリング）でも消えない。

**`AxisY` で `flatLength` を調べている理由**
- カメラの右が真上・真下を向くと（カメラを90度横に倒したとき）、水平にした右の長さが0になり、`normalize` でNaNになる。そのときだけカメラの右をそのまま使う。
- 普通のカメラ（`DebugCamera` など）は横に倒れないので、実際にはほぼ通らない。

**前のVSとの違い（見た目が変わるもの）**
- XY平面の板（ギズモのアイコン、`Rect3d` の `isBillboard`）：**位置は同じ**。
- ただし法線は**逆向きに直る**。前は `cross(right, up)` ＝カメラと反対向き（奥）を入れていた。今は板の法線 `(0,0,-1)` を回すのでカメラ側を向く。アイコンは `Unlit` なので見た目は変わらない。ライトありで板をビルボードにしていた場合は、**前は裏から照らされたように暗かったのが、正しく明るくなる**。
- 立体（`Sphere` / `AABB` の `isBillboard` など）：前は潰れて板になっていた。今は形を保ったまま回る。今このエンジンとゲームで使っている所は無い（`grep` で確認済み）。

**弱いところ（今は直さない）**
- **ノードが複数あるglTF**（Goblets のように、ノードごとに位置がずれているもの）は、**ノードごとに**カメラの方を向く。ノード同士の位置関係はカメラと一緒に回らないので、横から見ると並びが崩れる。
  - 原因：VSには「ノードの行列 × Entityの行列」を掛けた1つの行列しか届かないので、どこまでがEntityの回転か区別できない。
  - OBJ（ノードの行列が全部単位行列）と、1ノードのglTF（`monsterBall`、`plane`）は正しく動く。
  - 直すなら、Entityの行列とノードの行列を分けてVSに渡し、ビルボードはEntityの方だけにかける。`Model.md` Step 2（ノードをEntityにする）と一緒に考える。
- `Scale` を0にすると法線が0除算でNaNになる（`Light.md` Step 5.7 と同じ）。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
   - `MakeObjectTransform` に `bool` を渡している所が残っていれば、ここでエラーになる
2. **ゲームのビルド**が通る
3. **実行して確認**
   - ライトのアイコン（ギズモ）が**前と同じように**カメラを向く
   - Entityに `resources/plane.obj` を選び（`plane.mtl` が `uvChecker.png` を貼っているので、テクスチャの指定は要らない）、`Billboard = Full` にすると、どこから見ても**正面**が見える。uvCheckerの文字が左右反転しない
   - `Billboard = AxisY` にしてカメラを上に回すと、板は**垂直のまま**（上に傾かない）
   - `teapot` / `bunny` を `Full` にすると、**潰れずに**立体のままカメラの方を向く（前のVSなら切り絵になる）
   - Sceneビューではデバッグカメラ、Gameビューではゲームのカメラの方を向く（同じEntityが2つのビューで別の方を向く）
   - Transformの `Rotation` を変えても向きは変わらない（ビルボードが上書きする）。`Scale` と `Position` は効く
   - `Shading = Lambert` にして、ビルボードの板が**ライトの側から見たとき明るい**

### 次のStepでやること（ここではやらない）
- 3e：ゲーム側の `MonsterBall` をEntityへ移す（下の「3eの見通し」）。→ **名前の整理を3eに入れたので、これは3fへ移動**（中身は下のまま）

### 3eの見通し（コードは次で書く）→ 3fの見通し
- **IBLはシーンが持つ**：`GameScene` が `IBLEnvironment` を1つ持ち、`RenderSystem::Draw(..., &environment_, ...)` に渡す。Unityの「Lighting設定のEnvironment」がシーンごとにあるのと同じ形。RenderComponentにポインタを持たせないので、原則2（Componentはポインタを持たない）も守れる。
- **コードから「モデル付きのEntity」を作る手段が要る**：今の `RequestAddRender(handle)` は空のRenderComponentを予約するだけで、**次のフレームにならないと `GetRender` が取れない**。`Initialize` の中で「このEntityは `monsterBall.gltf` を `PBR` で描く」と書けない。
  - → `RequestAddRender(handle, const RenderComponent& initial)` のように**初期値付きで予約**できるようにする（予約の配列にHandleと初期値を一緒に積む）。安全性（処理中にSlotMapを増やさない）はそのまま。
- `MonsterBall` クラスは、IBLの確認用ウィンドウ（EnvCube / Irradiance / Prefilter / BRDF LUT）を `GameScene` 側へ移してから消す。
- ※3eで名前を変えるので、3fでは `RequestAddModelRenderer(handle, const ModelRendererComponent& initial)`、`ModelRenderSystem::Draw` になる。

---

## Step 3e：名前をカテゴリ前提に直す

### 目的
Componentは今後「カテゴリ」に分かれて増えていく（3D描画、2D描画、ライト、エフェクト、物理、音）。`RenderComponent` / `RenderSystem` という名前は、2D（Sprite）やパーティクルが来た瞬間に「どのRender？」になる。**参照している場所が少ない今のうちに**名前を直し、Add Componentもカテゴリのメニューにする。

ついでに、ゲーム側に**エンジンから消えたヘッダのコピーが残っている**問題を掃除する（下の「古いヘッダが残る問題」）。

### 命名の方針

**型の名前＝「何を」＋「どうする」。3D / 2D はカテゴリ（メニュー）で分ける。**

| カテゴリ（`ComponentCategory`） | Component | System / Manager | いつ |
|---|---|---|---|
| （必須・メニューに出さない） | `TransformComponent` | `EntityManager::UpdateTransforms` | 済み |
| `Rendering3D` | **`ModelRendererComponent`**（旧 `RenderComponent`） | **`ModelRenderSystem`**（旧 `RenderSystem`） | **3e** |
| `Rendering3D` | `SkinnedModelRendererComponent` | `SkinnedModelRenderSystem` | `Model.md` Step 4（スキニング） |
| `Rendering2D` | `SpriteRendererComponent`（＋ `UITransformComponent`） | `SpriteRenderSystem` | UIの作業 |
| `Lighting` | `DirectionalLightComponent` / `PointLightComponent` / `SpotLightComponent` | `LightManager` | `Light.md` Step 6 |
| `Effects` | `ParticleEmitterComponent`、`AccelerationFieldComponent`（風） | `ParticleSystem` | パーティクルの作業 |
| `Physics` | `ColliderComponent` | `CollisionSystem` | 将来 |
| `Audio` | `AudioSourceComponent` | `SoundManager` | 将来 |

**なぜ `Model` なのか（`Mesh` ではなく）**
- このエンジンの素材の呼び名は「Model」（`ModelManager::Load` → `ModelAsset`。中に複数のメッシュ・マテリアル・ノード）。Componentが持っているのも `modelHandle`。
- Unityの `MeshRenderer` は、どのメッシュかを `MeshFilter` という別のComponentが持つ2つ組の片方。このエンジンは1つのComponentが両方持つので、`MeshRenderer` と呼ぶと意味がずれる。

**なぜ名前に `3D` を入れないのか**
- `Render3DComponent` にすると「何を描くか」が名前から消える。`ModelRenderer` と `SpriteRenderer` は、名前だけで3Dと2Dの区別がつく。
- 分類はAdd Componentのメニュー（カテゴリ）でやる。Unityも同じで、型の名前には分類を入れず、メニューを `Rendering` / `Mesh` / `UI` … に分けている。

**フォルダはまだ分けない**
- 今カテゴリごとにフォルダを作ると、中身が1つだけのフォルダが並ぶ。2つ目が来たときに作る。
- `Graphics/Model/`（`ModelManager` の隣）に置くのは**やめる**。`ModelRenderSystem` は `EntityManager` を使うので、下の層（Graphics）が上の層（Entity）を知ることになり、依存の向きが逆になる。
- なので当面は `MyEngine/Entity/` に置いたまま、名前だけ変える。

### 変更するもの

| | 対象 | やること |
|---|---|---|
| ① | 型・関数・変数の名前 | Visual Studioの**名前の変更（`Ctrl+R`, `Ctrl+R`）**で一括 |
| ② | ファイル名 | ソリューションエクスプローラで `F2` |
| ③ | `#include` | 手で直す（名前の変更はincludeの文字列までは直さない） |
| ④ | `InspectorWindow.cpp` | 区画の見出し、Add Componentをカテゴリのメニューに（ここだけ新しいコード） |
| ⑤ | ゲーム側 | `ModelRenderSystem` に直す＋古いヘッダの掃除 |
| ⑥ | `Docs/ARCHITECTURE.md` | 例の名前を1行 |
| ⑦ | `WindowManager.cpp` | ついでに見つけた不具合：パーティクルのカメラを渡す1行が消えていた |

---

### ① 名前の変更（Visual Studioで一括）

名前の上で右クリック →「名前の変更」（`Ctrl+R`, `Ctrl+R`）。**ソリューション全体**が対象になる。

| 今の名前 | 新しい名前 | 場所 |
|---|---|---|
| `RenderComponent` | `ModelRendererComponent` | 型（`RenderComponent.h`） |
| `RenderSystem` | `ModelRenderSystem` | 型（`RenderSystem.h`） |
| `Entity::render` | `Entity::modelRenderer` | `Entity.h` のメンバ |
| `RequestAddRender` | `RequestAddModelRenderer` | `EntityManager` |
| `IsRenderAddPending` | `IsModelRendererAddPending` | `EntityManager` |
| `GetRender` | `GetModelRenderer` | `EntityManager` |
| `renders_` | `modelRenderers_` | `EntityManager` のメンバ |
| `pendingAddRender_` | `pendingAddModelRenderer_` | `EntityManager` のメンバ |
| `DrawRender` | `DrawModelRenderer` | `InspectorWindow` |

- **`Renderer` / `RenderContext` / `RenderQueue` など、似た名前の別物を巻き込まないように**、1つずつ「名前の上で」実行する（文字列の置換ではない）。
- コメント中の `RenderComponent` は名前の変更では直らないことがある。気になれば `Ctrl+Shift+F` で探して直す（動作には関係ない）。

---

### ② ファイル名の変更（ソリューションエクスプローラで `F2`）

| 今 | 新しい名前 |
|---|---|
| `MyEngine/Entity/RenderComponent.h` | `MyEngine/Entity/ModelRendererComponent.h` |
| `MyEngine/Entity/RenderSystem.h` | `MyEngine/Entity/ModelRenderSystem.h` |
| `MyEngine/Entity/RenderSystem.cpp` | `MyEngine/Entity/ModelRenderSystem.cpp` |

- ソリューションエクスプローラで変えると、ディスク上のファイル名と `.vcxproj` の両方が変わる（エクスプローラで直接変えると `.vcxproj` が古いままになる）。

---

### ③ `#include` を直す

| ファイル | 直す行 |
|---|---|
| `MyEngine/Entity/Entity.h` | `#include "MyEngine/Entity/ModelRendererComponent.h"` |
| `MyEngine/Entity/ModelRenderSystem.cpp` | `#include "ModelRenderSystem.h"`（1行目） |

コメントも合わせておくと後で読みやすい。

`ModelRendererComponent.h` の構造体の上のコメント
```cpp
// 3Dモデルを描くためのデータだけを持つ（カテゴリ：Rendering3D）。処理とGPUリソースは持たない。
```

`ModelRenderSystem.h` のクラスの上のコメント
```cpp
/// ModelRendererComponent を持つEntityを描くシステム（カテゴリ：Rendering3D）
```

---

### ④ `MyEngine/Editor/InspectorWindow.cpp`

**差し替える**：区画の見出しとID（`DrawModelRenderer` の中）
```cpp
	if (!ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（Transformの区画と項目名がぶつからないように）
	ImGui::PushID("ModelRenderer");
```

**追加する**：無名namespaceの一番下（`EnumCombo` の後、`} // namespace` の前）
```cpp

// ===== Add Component のメニュー =====
// Componentの分類。Add Componentではカテゴリごとにサブメニューになる（並びはこの順）
enum class ComponentCategory {
	Rendering3D, // 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
	Rendering2D, // 2D（将来のSpriteRenderer）
	Lighting,    // ライト（Light.md Step 6）
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
};

// Add Componentに出す1項目
struct ComponentMenuItem {
	ComponentCategory category;         // どのサブメニューに出すか
	const char* name;                   // 表示名
	bool (*canAdd)(Handle<Entity>);     // 足せるか（まだ持っていない・予約もしていない）
	void (*requestAdd)(Handle<Entity>); // 追加を予約する（実体ができるのは次のフレームの頭）
};

// 足せるComponentの一覧。Componentを増やしたら、ここに1行足す
// （キャプチャしないラムダは関数ポインタに変換できる）
constexpr ComponentMenuItem kComponentMenu[] = {
    {ComponentCategory::Rendering3D, "Model Renderer",
     [](Handle<Entity> handle) { return !EntityManager::GetModelRenderer(handle) && !EntityManager::IsModelRendererAddPending(handle); },
     [](Handle<Entity> handle) { EntityManager::RequestAddModelRenderer(handle); }},
};
```

**差し替える**：`DrawAddComponent` の中の `if (ImGui::BeginPopup("addComponent")) { ... }` 全体
```cpp
	if (ImGui::BeginPopup("addComponent")) {
		// カテゴリの順に、中身のあるカテゴリだけサブメニューを作る
		for (ComponentCategory category : magic_enum::enum_values<ComponentCategory>()) {
			bool hasItem = false;
			for (const ComponentMenuItem& item : kComponentMenu) {
				if (item.category == category) {
					hasItem = true;
					break;
				}
			}
			if (!hasItem) {
				continue; // まだ1つも無いカテゴリは出さない
			}

			std::string categoryName(magic_enum::enum_name(category));
			if (ImGui::BeginMenu(categoryName.c_str())) {
				for (const ComponentMenuItem& item : kComponentMenu) {
					if (item.category != category) {
						continue;
					}
					// すでに持っている・追加を予約済みなら選べないようにする
					if (ImGui::MenuItem(item.name, nullptr, false, item.canAdd(handle))) {
						item.requestAdd(handle);
					}
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndPopup();
	}
```

---

### ⑤ ゲーム側（CG3_Project）

**差し替える**：`GameScene.cpp`
```cpp
#include <MyEngine/Entity/ModelRenderSystem.h>
```
```cpp
	ModelRenderSystem::Draw({}, camera_, nullptr, GetWindowTitle());
```

**消す**：もう存在しないクラスのinclude・前方宣言（下の「古いヘッダが残る問題」）

| ファイル | 消す行 |
|---|---|
| `GameScene.h` | `#include <MyEngine/Light/PointLight.h>` と `class DirectionalLight;` |
| `Stage.h` | `#include "MyEngine/Light/PointLight.h"` |
| `MonsterBall.h` | `class DirectionalLight;` と `class PointLight;` |

**消す**：ゲーム側にコピーされた古いヘッダ（エクスプローラで削除してよい）
```
CG3_Project/MyEngine/include/MyEngine/Light/PointLight.h
CG3_Project/MyEngine/include/MyEngine/Light/DirectionalLight.h
CG3_Project/MyEngine/include/MyEngine/Entity/RenderComponent.h   ← エンジンをビルドした後に残る
CG3_Project/MyEngine/include/MyEngine/Entity/RenderSystem.h      ← 同上
```

- 消す順番：**エンジンのビルド → 古いヘッダを消す → ゲームのビルド**。先に消しても、エンジンのビルドで作り直されることは無い（エンジンにもう無いファイルなので）。

---

### ⑥ `Docs/ARCHITECTURE.md`

「Editorとの関係」の例（105行目あたり）
```
    ├ RenderComponent
```
を
```
    ├ ModelRendererComponent
```
に直す。

ついでに、118行目あたりの「現在: 段階1」は、実際には**段階2**（Componentを型別の `SlotMap` に分け、Managerがまとめて回している）まで来ている。直しておくとよい。

---

### 古いヘッダが残る問題（今回見つけたもの）

エンジンのビルド後の処理は `xcopy` でヘッダをゲーム側へコピーしている。

```
xcopy /Y /S /I "$(ProjectDir)MyEngine\*.h" "$(SolutionDir)MyEngine\include\MyEngine\"
```

`xcopy` は**上書きと追加しかしない。エンジンから消したファイルは、ゲーム側にずっと残る**。

実際に起きていること：
- `PointLight.h` / `DirectionalLight.h` はエンジンからはライトのコンポーネント化（コミット `bb177b3`）で消えている
- でもゲーム側の `MyEngine/include/MyEngine/Light/` には**古いコピーが残っている**
- `GameScene.h` と `Stage.h` は今もそれをincludeしている → **古いコピーがあるからビルドが通っているだけ**
- 別のPCでcloneしたり、includeフォルダを消したりすると、突然ビルドが通らなくなる

今回 `RenderSystem.h` の名前を変えると、同じことがもう一度起きる。古い `RenderSystem.h` がゲーム側に残るので、⑤の修正を忘れても**コンパイルは通ってしまい、リンクで `RenderSystem::Draw` が見つからない**という分かりにくいエラーになる。

確認済み：古いコピーの無い状態（エンジンのヘッダだけ）で、⑤を直した `GameScene` / `Stage` / `MonsterBall` / `Particles` がコンパイルできる。

**（任意）ビルド後の処理を直して、今後は自動で消えるようにする**

プロジェクトのプロパティ →「ビルドイベント」→「ビルド後イベント」→「コマンドライン」（**構成：すべての構成**）。ヘッダをコピーしている `xcopy` の1行を、次の1行に置き換える。
```
robocopy "$(ProjectDir)MyEngine" "$(SolutionDir)MyEngine\include\MyEngine" *.h /S /PURGE /NJH /NJS /NDL /NP /NFL & if errorlevel 8 (exit /b 1) else (cmd /c "exit /b 0")
```

- `/PURGE`：コピー先にあって、コピー元に無い `.h` を消す（`.h` 以外には触らない）。
- `robocopy` は**成功しても終了コードが1〜7**になる（コピーした＝1、余分を消した＝2、両方＝3…）。VSは0以外を「ビルド失敗」と扱うので、8未満なら0に直している。`%errorlevel%` ではなく `if errorlevel 8` と書くのは、`%errorlevel%` が**その行を実行する前の値**に置き換わってしまうから。
- パスの最後に `\` を付けないこと（`"...\MyEngine\"` のように書くと、robocopyは `\"` を「"という文字」と読んでしまう）。
- 確認済み：scratchで同じ形のフォルダを作って試し、古い `.h` だけが消え、`.cpp` はコピーされず、スクリプト全体の終了コードが0になり、後ろの行（シェーダー・Resourcesのxcopy）も実行されることを確かめた。
- シェーダーとResourcesの `xcopy` も同じ問題を持っている（`.shader` の名前を変えると古いのが残る）。困ったら同じ形にする。

---

### ⑦ ついでに見つけた不具合：パーティクルのカメラが渡っていない

`MyEngine/Window/WindowManager.cpp` の更新処理で、パーティクル用のカメラを**探しているのに渡していない**。

```cpp
	// パーティクルのビルボード用カメラは、現在のシーンから毎フレーム取る。
	// ゲーム側が SetCamera を呼ぶ必要がなくなり、破棄済みカメラを掴む事故も起きない
	Camera* particleCamera = nullptr;
	for (WindowSet& w : windows_) { ... particleCamera = scene->GetCamera(); ... }
	EntityManager::UpdateTransforms();      // ← ここに元々 ParticleManager::SetCamera(particleCamera); があった
	ParticleManager::Update();
```

- コミット `2d0c3da` の差分を見ると、`ParticleManager::SetCamera(particleCamera);` の行が `EntityManager::UpdateTransforms();` に**置き換わって**消えている（`Editor.md` Step 1の「UpdateTransformsを足す」を写したときに、挿入ではなく上書きになったと思われる）。
- 今動いているのは、ゲーム側の `GameScene::Initialize` が自分で `ParticleManager::SetCamera(camera_)` を呼んでいるから。
- 困ること：`SetCamera` を呼ばないシーンを作ると、**パーティクルが止まる**（`ParticleManager::Update` はカメラが無いと何もしない）。シーンを切り替えたときに、**前のシーンの消えたカメラを掴み続ける**おそれもある（コメントに書いてある「事故」そのもの）。

**追加する**：`EntityManager::UpdateTransforms();` の上
```cpp
	ParticleManager::SetCamera(particleCamera); // 見つけたカメラを毎フレーム渡す（シーンが替わっても古いカメラを掴まない）
```

- これでゲーム側の `ParticleManager::SetCamera(camera_);`（`GameScene.cpp`）は要らなくなる。消してもよいし、残しても害は無い（毎フレーム上書きされる）。

---

### 解説

**ラムダを関数ポインタにしている理由**
- `EntityManager::RequestAddModelRenderer` を直接表に入れることもできるが、3fで `RequestAddModelRenderer(handle, 初期値)` を足すと、関数の型が変わって表に入らなくなる。
- キャプチャしないラムダで包んでおけば、`EntityManager` 側の引数が増えても表は直さなくてよい。

**今の作りの限界（覚えておくこと）**
- Componentが増えるたびに `EntityManager` に `Get〇〇` / `RequestAdd〇〇` / `Is〇〇AddPending` の3つと、`SlotMap` と予約の配列と `Entity` のHandleが増える。**5種類になると関数だけで15個**。
- これは ARCHITECTURE.md の段階2の限界。段階3へ進むときに、`EntityManager::Get<T>(handle)` / `RequestAdd<T>(handle)` のように**型で引ける形（テンプレート）**に置き換える。そうすると新しいComponentは「データの構造体を作って、表に1行足す」だけになる。
- 目安：`Light.md` Step 6 でライトの3種類を足す前にやると、ライトの分の関数を書かずに済む。

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
   - `RenderComponent` / `RenderSystem` がどこかに残っていればエラーになる
   - 確認済み：エンジン全71ファイルがDebug / Releaseで通る。古い名前のヘッダをincludeしたら `#error` で止まるダミーを置いて試し、取り残しが無いことも確かめた
2. **ゲームのビルド**が通る（古いヘッダを消してから）
3. **実行して確認**
   - Inspectorの区画の見出しが `Model Renderer` になっている
   - `Add Component` → `Rendering3D` → `Model Renderer` の2段のメニューになる
   - 他のカテゴリ（`Lighting` など）は、中身が無いのでまだ出ない
   - すでにModel Rendererを持っているEntityでは、`Model Renderer` がグレーになって選べない
   - 3cと3dの確認項目（モデルを選ぶと出る、ビルボード）がそのまま動く

### 次のStepでやること（ここではやらない）
- 3f：ゲーム側の `MonsterBall` をEntityへ移す（上の「3fの見通し」）。

---

## まとめて：使い勝手の改善 ＋ Step 3f（2026-09-18）

ユーザーの希望で、今回から**1回に出す量を増やす**（小出しだとテンポが悪いので）。パートごとに独立しているので、どの順に写してもよい。**ビルドの確認はパートごとでも、全部写してからでもよい。**

### 今回の中身
| パート | 内容 | 触るファイル |
|---|---|---|
| A | 共通の部品 `EditorWidgets.h`（**新規**）：`std::string` の入力欄、`EnumCombo` | `MyEngine/Editor/EditorWidgets.h` |
| B | Hierarchyで**その場で名前の変更**（ダブルクリック / F2 / 右クリック / 作った直後） | `HierarchyWindow.h` / `.cpp`（**全体を差し替え**） |
| C | **日本語の名前**（長さの上限をなくす＋どの文字も出るフォント） | `InspectorWindow.cpp`、`ImGuiManager.cpp` |
| D | **縦長のAdd Component**（カテゴリを押すと下に開く＋検索欄） | `InspectorWindow.h` / `.cpp` |
| E | **3f**：MonsterBallをEntityへ（初期値付きの追加、IBLはシーンが持つ） | `EntityManager.h` / `.cpp`、ゲーム側 `GameScene.h` / `.cpp`、`MonsterBall.h` / `.cpp` を削除 |

確認済み：エンジン全71ファイルを `/W4` で Debug / Release とも通した（今回触ったファイルに警告なし）。ゲーム側の `GameScene` / `Stage` / `Particles` も Debug / Release で通した。**実行はできないので、画面の確認はお願いします。**

---

### パートA：`MyEngine/Editor/EditorWidgets.h`（新規）

HierarchyとInspectorの両方で使う部品。`ImGuiManager` に置けないのは、`ImGuiManager` が丸ごと `#ifdef USE_IMGUI` の中にあり、Releaseでもコンパイルされる `HierarchyWindow.cpp` / `InspectorWindow.cpp` から呼べないため。**ヘッダだけ**（.cppは無い）。

```cpp
#pragma once
#include <string>
#include <type_traits>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>


/// <summary>
/// Hierarchy・Inspectorなど、エディタのウィンドウで共通に使うImGuiの部品
/// <para>ImGuiManagerは USE_IMGUI の中にしか無いので、Releaseでもコンパイルされるウィンドウから使える場所に置く</para>
/// </summary>
namespace EditorWidgets {

// std::string の長さを、ImGuiが必要な分だけ伸ばす（ImGui公式の misc/cpp/imgui_stdlib と同じ仕組み）
inline int ResizeStringCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		auto* text = static_cast<std::string*>(data->UserData);
		text->resize(static_cast<size_t>(data->BufTextLen));
		data->Buf = text->data();
	}
	return 0;
}

/// <summary>
/// std::string をそのまま編集する入力欄。長さの上限が無いので、日本語でも途中で切れない
/// </summary>
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags | ImGuiInputTextFlags_CallbackResize, ResizeStringCallback, &text);
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

- `EnumCombo` は `InspectorWindow.cpp` の無名namespaceから**引っ越し**（パートD）。ライトのComponentの区画（`Light.md` Step 6）でも使うので、共通の場所へ。
- `InputText` の仕組み：ImGuiは「バッファが足りない」ときにコールバックを呼ぶ。そこで `std::string` を `resize` して、新しい場所を `data->Buf` で教え返す。ImGui公式の `imgui_stdlib`（このエンジンの `externals/imgui` には入っていない）と同じ書き方。

---

### パートB：Hierarchyでその場で名前の変更

**名前の変更を始める方法**（どれでも同じ）
- 名前を**ダブルクリック**
- 選んで **F2**（Windowsのエクスプローラと同じ）
- 右クリック →「**Rename**」
- 「Create Entity」「Create Child」で**作った直後**（Unityと同じ。すぐ名前を打てる）

**終わり方**
| 操作 | 結果 |
|---|---|
| Enter | 確定 |
| 欄の外をクリック | 書き換えていれば確定 |
| Esc | 取り消し（ImGuiが元の文字に戻す） |
| 空のまま確定 | 元の名前のまま（空の名前にはしない） |

変更が多いので、**2つとも全体を差し替え**。

#### `MyEngine/Editor/HierarchyWindow.h`（全体を差し替え）
```cpp
#pragma once
#include <string>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/Entity.h"


/// <summary>
/// ヒエラルキーウィンドウ（Entityの一覧・作成・削除・選択・親子の付け替え・名前の変更）
/// <para>選択中のEntityはここが持つ。InspectorWindowはこれを見る</para>
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
	// Entity1つ分を描く（子がいれば入れ子で描く）
	static void DrawEntityNode(Handle<Entity> handle);
	// 一覧を描いている間に受け付けた操作を、描き終わってから反映する
	static void ApplyRequests();
	// 名前の変更を始める（そのEntityの行を入力欄にする）
	static void StartRename(Handle<Entity> handle);
	// 名前の入力欄（DrawEntityNodeの中、ツリーの矢印の横に出す）
	static void DrawRenameField(Handle<Entity> handle);

	static Handle<Entity> selected_;          // 選択中
	static Handle<Entity> createChildOf_;     // このEntityの子を作る（有効なときだけ）
	static Handle<Entity> destroyRequest_;    // このEntityを消す
	static Handle<Entity> reparentChild_;     // 親を付け替えるEntity
	static Handle<Entity> reparentParent_;    // 新しい親（無効ならrootへ移す）
	static bool createRootRequest_;           // 一番上にEntityを作る
	static bool reparentRequest_;             // 親の付け替えを頼まれた
	static Handle<Entity> renaming_;          // 名前を変更中のEntity（無効なら変更中ではない）
	static std::string renameBuffer_;         // 入力中の名前（確定するまでEntityの名前は変えない）
	static bool renameFocusRequest_;          // 入力欄を出した最初のフレームだけtrue（フォーカスを移す）
};
```

#### `MyEngine/Editor/HierarchyWindow.cpp`（全体を差し替え）
```cpp
#include "HierarchyWindow.h"

#include <cfloat>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::createChildOf_;
Handle<Entity> HierarchyWindow::destroyRequest_;
Handle<Entity> HierarchyWindow::reparentChild_;
Handle<Entity> HierarchyWindow::reparentParent_;
bool HierarchyWindow::createRootRequest_ = false;
bool HierarchyWindow::reparentRequest_ = false;
Handle<Entity> HierarchyWindow::renaming_;
std::string HierarchyWindow::renameBuffer_;
bool HierarchyWindow::renameFocusRequest_ = false;

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

	// --- F2で、選択中のEntityの名前を変更（Windowsのエクスプローラと同じ）---
	bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (isFocused && !renaming_.IsValid() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
		StartRename(selected_);
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

	// 名前を変更中は、ラベルを消して、矢印の横に入力欄を出す
	bool isRenaming = (handle == renaming_);
	if (isRenaming) {
		flags &= ~ImGuiTreeNodeFlags_SpanAvailWidth; // 横幅いっぱいのままだと、入力欄が右端へ押し出される
	}

	// Handleの中身をIDにする（名前が同じEntityがあってもぶつからない）
	ImGui::PushID(static_cast<int>(handle.index));
	bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", isRenaming ? "" : entity->name.c_str());

	if (isRenaming) {
		ImGui::SameLine();
		DrawRenameField(handle);
	} else {
		// --- クリックで選択、ダブルクリックで名前の変更 ---
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			selected_ = handle;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			StartRename(handle);
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
			if (ImGui::MenuItem("Rename", "F2")) {
				StartRename(handle);
			}
			if (ImGui::MenuItem("Create Child")) {
				createChildOf_ = handle;
			}
			if (ImGui::MenuItem("Destroy")) {
				destroyRequest_ = handle;
			}
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
	bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全選択（そのまま打つと置き換わる）
	bool entered = EditorWidgets::InputText("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	// --- 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）---
	// Escで抜けたときはImGuiが中身を元に戻すので、確定しても元の名前のまま＝取り消しになる
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		Entity* entity = EntityManager::Get(handle);
		if (entity && !renameBuffer_.empty()) { // 空の名前にはしない（元の名前のまま）
			entity->name = renameBuffer_;
		}
	}

	// --- 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる ---
	bool finished = entered || ImGui::IsItemDeactivated();
	// 入力欄が有効にならないまま別の所をクリックされたときも閉じる（出しっぱなしにしない）
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_ = Handle<Entity>{};
	}
}


//=============================================================================
// 一覧を描き終わってから操作を反映する
//=============================================================================
void HierarchyWindow::ApplyRequests() {
	// --- 作成（作ったらそのまま名前の変更を始める。Unityと同じ）---
	if (createRootRequest_) {
		StartRename(EntityManager::Create("Entity"));
		createRootRequest_ = false;
	}
	if (createChildOf_.IsValid()) {
		StartRename(EntityManager::Create("Child", createChildOf_));
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
		if (destroyRequest_ == renaming_) {
			renaming_ = Handle<Entity>{};
		}
		EntityManager::Destroy(destroyRequest_);
		destroyRequest_ = Handle<Entity>{};
	}
}
```

**前との違い**：`#include`（`<cfloat>`、`EditorWidgets.h`）、静的メンバ3つ、`Draw` のF2、`DrawEntityNode` の「名前を変更中なら入力欄」の分岐（中身の選択・ドラッグ・右クリックは `else` の中に入っただけ）、右クリックの `Rename`、`StartRename` / `DrawRenameField` の2つの関数、`ApplyRequests` の作成と削除。

**解説**
- **ラベルを消して `SpanAvailWidth` を外す理由**：ツリーの行は横幅いっぱいが当たり判定になっている。そのまま `SameLine` すると、入力欄は「行の後ろ」＝右端に押し出される。名前変更中だけ横幅いっぱいをやめて、矢印のすぐ横に入力欄を置く。
- **確定するまで `Entity::name` を変えない理由**：打っている途中の文字で、Inspectorや他の表示がちらちら変わらないように。`renameBuffer_` に打って、Enter・外をクリックで初めて書き戻す。
- **フォーカスを「出した最初のフレームだけ」移す理由**：`SetKeyboardFocusHere` を毎フレーム呼ぶと、欄の外をクリックしてもフォーカスが戻され、抜けられなくなる。
- **名前を打っている間にゲームが反応しないか**：`InputManager::IsKeyPressed` などは、ImGuiが文字入力を受け取っている間は `false` を返す（`IsKeyboardCapturedByImGui`）。なので名前に「W」や「Space」を打っても、カメラやゲームは動かない（今のエンジンの作りのまま大丈夫）。

---

### パートC：日本語の名前

**実は、日本語の入力の仕組みはもう動く状態だった**（調べた結果）
| 必要なもの | 今のエンジン |
|---|---|
| ウィンドウがUnicode（IMEで確定した文字がUTF-16で届く） | ○（プロジェクトの文字セットが `Unicode`） |
| ImGuiがIMEの変換窓を入力欄の横に出す | ○（ImGuiが標準で `imm32` を使ってやる） |
| DirectInputがIMEを邪魔しない | ○（`DISCL_NONEXCLUSIVE`。`EXCLUSIVE` だとIMEが使えない） |
| 入力中はゲームにキーが渡らない | ○（`IsKeyboardCapturedByImGui`） |

**足りなかったのは2つ**
1. **名前の欄が `char[64]`**：UTF-8で日本語は1文字3バイトなので **21文字まで**。さらに `strncpy_s` でコピーすると、長い名前が**文字の途中で切れて**「?」になることがある。→ パートAの `EditorWidgets::InputText` で `std::string` を直接編集する（上限なし）。
2. **フォントに入っている文字が決め打ち**：今は `GetGlyphRangesJapanese()`（常用漢字など約3000字＋かな）の分だけ、起動時にフォントの画像を作っている。**珍しい漢字（髙・﨑など）や記号（★♪①）は「?」**になる。→ ImGui 1.92の「使われた文字をその場でフォントの画像に足す」方式に切り替える。

#### `MyEngine/Editor/InspectorWindow.cpp`（名前の欄）

**差し替える**：`DrawHeader` の中の「名前」の部分（`nameBufferOwner_` の `if` から `InputText` の `if` まで）
```cpp
	// --- 名前（Entityのstd::stringを直接編集する。長さの上限が無いので日本語でも切れない）---
	ImGui::SetNextItemWidth(-FLT_MIN); // 残りの幅いっぱいに広げる
	EditorWidgets::InputText("##name", entity->name);
```

**消す**：もう使わないもの
- `#include <cstring>`
- 静的メンバの定義2行（`char InspectorWindow::nameBuffer_[...]` と `Handle<Entity> InspectorWindow::nameBufferOwner_;`）と、その上の `// 静的メンバ変数`
- `InspectorWindow.h` の `kNameBufferSize`、`nameBuffer_`、`nameBufferOwner_`（パートDでヘッダも触る）

**追加する**：include（`HierarchyWindow.h` の上）
```cpp
#include "MyEngine/Editor/EditorWidgets.h"
```

#### `MyEngine/Editor/ImGuiManager.cpp`（フォントとDirectX12の初期化）

**追加する**：include
```cpp
#include <vector>
```

**追加する**：無名namespace（`namespace {` のすぐ下。`DrawProfilerBar` より上）
```cpp
	// ===== ImGuiが使うSRV（フォントの画像など）の割り当て =====
	// ImGui 1.92は、必要になった文字をその場でフォントの画像に書き足す（最初に全部の文字を作らない）。
	// そのため画像が作り直されることがあり、SRVをもらったり返したりできる必要がある
	std::vector<uint32_t> imguiFreeSrvSlots = {0}; // 返してもらったスロット（0番は最初からImGui用に空けてある）

	void AllocImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
		uint32_t slot = 0;
		if (!imguiFreeSrvSlots.empty()) {
			slot = imguiFreeSrvSlots.back(); // 返してもらったスロットを使い回す
			imguiFreeSrvSlots.pop_back();
		} else {
			slot = DirectXCommon::AllocateSRVSlot();
		}
		ID3D12DescriptorHeap* heap = DirectXCommon::GetSRVDescriptorHeap();
		*outCpu = DirectXCommon::GetCPUDescriptorHandle(heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, slot);
		*outGpu = DirectXCommon::GetGPUDescriptorHandle(heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, slot);
	}

	void FreeImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
		// CPUハンドルがヒープの先頭から何個目かを逆算して、使い回しの列に戻す
		SIZE_T start = DirectXCommon::GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart().ptr;
		uint32_t slot = static_cast<uint32_t>((cpu.ptr - start) / DirectXCommon::GetDescriptorSizeSRV());
		imguiFreeSrvSlots.push_back(slot);
	}

```

**差し替える**：`Initialize` の「日本語フォントの読み込み」から `io.Fonts->Build();` まで
```cpp
	// 日本語フォントの読み込み
	// 文字の範囲は指定しない。ImGui 1.92は、使われた文字をその場でフォントの画像に書き足す（珍しい漢字や ★♪① も出る）
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f);
	ImGui_ImplWin32_Init(window->GetHWND());
	// DirectX12側の初期化。SRVは必要な分だけコールバックで渡す（フォントの画像が作り直されても大丈夫なように）
	ImGui_ImplDX12_InitInfo initInfo;
	initInfo.Device = DirectXCommon::GetDevice();
	initInfo.CommandQueue = DirectXCommon::GetCommandQueue(); // 書き足したフォントの画像をGPUへ送るのに使う
	initInfo.NumFramesInFlight = DirectXCommon::kSwapChainBufferCount;
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.SrvDescriptorHeap = DirectXCommon::GetSRVDescriptorHeap();
	initInfo.SrvDescriptorAllocFn = AllocImGuiSrv;
	initInfo.SrvDescriptorFreeFn = FreeImGuiSrv;
	ImGui_ImplDX12_Init(&initInfo);
	// io.Fonts->Build() は要らない（1.92では、最初のフレームで必要な分だけ作られる）
```

**解説**
- **なぜ初期化の形を変える必要があるのか**：今の `ImGui_ImplDX12_Init(device, 2, format, heap, cpu, gpu)`（引数6個の古い形）は、SRVを**1個だけ**渡す。ImGuiのバックエンドはこの形で呼ばれると「フォントの画像を作り直せない」と判断して、`ImGuiBackendFlags_RendererHasTextures`（その場で文字を足す機能）を**切る**（`imgui_impl_dx12.cpp` の中で確認済み）。その結果、起動時に決めた範囲の文字しか出ない。
- 新しい形（`ImGui_ImplDX12_InitInfo`）で「SRVをもらう関数・返す関数」を渡すと、ImGuiが必要なときにSRVを増やせるので、その場で文字を足す機能が有効になる。
- **スロットの使い回し**：エンジンの `AllocateSRVSlot` は増やすだけ（返す仕組みが無い）。フォントの画像は作り直されるたびに古いSRVが返ってくるので、ImGui専用の「返してもらったスロットの列」を持って使い回す。0番はもともとImGui用に空けてある（`nextSrvSlot_ = 1` から始まる）ので、最初はそれを渡す。
- 文字の範囲（`GetGlyphRangesJapanese()`）を**消した**のは、新しい方式では使われないから（範囲を見るのは古い方式の「最初に全部作る」処理だけ）。
- **もし起動したら文字が出ない・落ちる場合**：この差し替えを元の3行（`GetGlyphRangesJapanese()` 付きのフォントと、引数6個の `ImGui_ImplDX12_Init`、`io.Fonts->Build()`）に戻せば元通りになる。そのときは教えてほしい。

---

### パートD：縦長のAdd Component

前はカテゴリにマウスを乗せると**横に**サブメニューが出ていた。これを、**Add Componentボタンの真下に、同じ幅の縦長の窓**を出し、その中で**カテゴリを押すと下に開く**形にする（Unityと同じ向き）。一番上に**検索欄**も付ける（開いた瞬間に打てる）。

```
[        Add Component        ]
┌─────────────────────────────┐
│ 🔍 検索                      │
├─────────────────────────────┤
│ ▶ Rendering3D               │  ← 押すと下に開く
│   ・Model Renderer          │
│ ▶ Lighting（Light Step 6で）│
└─────────────────────────────┘
```

#### `MyEngine/Editor/InspectorWindow.h`

**消す**：`kNameBufferSize`、`nameBuffer_`、`nameBufferOwner_`（パートCで使わなくなった）

**差し替える**：`DrawAddComponent` の上のコメント
```cpp
	// Componentを足すボタン（カテゴリごとに縦に並べたポップアップ）
```

#### `MyEngine/Editor/InspectorWindow.cpp`

**消す**：無名namespaceの中の `EnumCombo` のテンプレート全体（パートAの `EditorWidgets.h` へ引っ越した）

**差し替える**：`EnumCombo(` の呼び出し5か所に `EditorWidgets::` を付ける
```cpp
	EditorWidgets::EnumCombo("Shading", render->shadingType);
	EditorWidgets::EnumCombo("Blend", render->blendMode);
	EditorWidgets::EnumCombo("Rasterizer", render->rasterizerType);
	EditorWidgets::EnumCombo("Depth", render->depthMode);
	EditorWidgets::EnumCombo("Billboard", render->billboard);
```

**追加する**：無名namespaceの定数（`kModelExtensions` の下）
```cpp
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）
```

**差し替える**：`DrawAddComponent` の関数全体
```cpp
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	if (!EntityManager::IsAlive(handle)) {
		return;
	}

	if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f))) {
		ImGui::OpenPopup("addComponent");
	}

	// --- ボタンの真下に、ボタンと同じ幅の縦長のポップアップを出す ---
	// （横に出るサブメニューではなく、カテゴリを押すと下に開く形。Unityの Add Component と同じ向き）
	ImVec2 buttonMin = ImGui::GetItemRectMin();
	ImVec2 buttonMax = ImGui::GetItemRectMax();
	float width = buttonMax.x - buttonMin.x;
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
	for (ComponentCategory category : magic_enum::enum_values<ComponentCategory>()) {
		// このカテゴリに、検索に合う項目が1つでもあるか（無いカテゴリは出さない）
		bool hasItem = false;
		for (const ComponentMenuItem& item : kComponentMenu) {
			if (item.category == category && filter.PassFilter(item.name)) {
				hasItem = true;
				break;
			}
		}
		if (!hasItem) {
			continue;
		}

		// 検索中は、見つかったカテゴリを全部開いて見せる
		if (filter.IsActive()) {
			ImGui::SetNextItemOpen(true);
		}
		std::string categoryName(magic_enum::enum_name(category));
		if (!ImGui::TreeNodeEx(categoryName.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			continue;
		}
		for (const ComponentMenuItem& item : kComponentMenu) {
			if (item.category != category || !filter.PassFilter(item.name)) {
				continue;
			}
			// すでに持っている・追加を予約済みなら、灰色にして押せなくする
			ImGui::BeginDisabled(!item.canAdd(handle));
			if (ImGui::Selectable(item.name)) { // Selectableを押すとポップアップは自動で閉じる
				item.requestAdd(handle);
			}
			ImGui::EndDisabled();
		}
		ImGui::TreePop();
	}

	ImGui::EndPopup();
}
```

**解説**
- **`SetNextWindowPos` / `SetNextWindowSizeConstraints` を `BeginPopup` の前に呼ぶ**：ポップアップも中身はウィンドウなので、「次に開くウィンドウ」の位置と大きさを先に決められる。幅を最小・最大とも同じにして**ボタンと同じ幅**に固定し、高さは上限だけ決めて**超えたらスクロール**。ポップアップが開いていないフレームでは、ImGuiがこの指定を捨てるので、他のウィンドウに漏れない。
- **カテゴリは `TreeNodeEx` の `Framed`**：`CollapsingHeader` と同じ見た目で、中身は自動で1段下げて並ぶ。開いた・閉じたはImGuiが覚えているので、次に開いたときもそのまま。
- **検索は `ImGuiTextFilter`**：ImGui標準の部品。大文字小文字を区別せず、`model,light` のように `,` で複数、`-light` で除外もできる。検索中は `SetNextItemOpen(true)` で、合うカテゴリを全部開いて見せる。
- **押せない項目は `BeginDisabled`**：すでに持っているComponentは灰色になる。

---

### パートE：Step 3f（MonsterBallをEntityへ）

#### エンジン：初期値付きでComponentを予約できるようにする

今の `RequestAddModelRenderer(handle)` は**空の**Componentを予約するだけで、実体ができるのは次のフレームの頭。なので、ゲームの `Initialize` で「このEntityは `monsterBall.gltf` を `PBR` で描く」と**書く手段が無かった**。予約のときに初期値も一緒に積めるようにする。

**`MyEngine/Entity/EntityManager.h`**

**差し替える**：予約の関数（コメントと宣言）
```cpp
	// 追加を予約する。実体ができるのは次のFlushComponentChanges。initialを渡すと、その値で作られる（コードからモデル付きのEntityを作るとき用）
	// 戻り値のポインタを即座に使う方式にはしない。
	static void RequestAddModelRenderer(Handle<Entity> handle, const ModelRendererComponent& initial = {});
```

**差し替える**：予約の配列（メンバ）
```cpp
	std::vector<std::pair<Handle<Entity>, ModelRendererComponent>> pendingAddModelRenderer_; // (追加先, 初期値)
```

**`MyEngine/Entity/EntityManager.cpp`**

**差し替える**：`RequestAddModelRenderer`、`IsModelRendererAddPending`、`FlushComponentChanges`（`GetModelRenderer` はそのまま）
```cpp
void EntityManager::RequestAddModelRenderer(Handle<Entity> handle, const ModelRendererComponent& initial) {
	if (!IsAlive(handle) || GetModelRenderer(handle) || IsModelRendererAddPending(handle)) {
		return;
	}
	instance_->pendingAddModelRenderer_.emplace_back(handle, initial); // 初期値も一緒に覚えておく
}

bool EntityManager::IsModelRendererAddPending(Handle<Entity> handle) {
	for (const auto& [pendingHandle, initial] : instance_->pendingAddModelRenderer_) {
		if (pendingHandle == handle) {
			return true;
		}
	}
	return false;
}
```
```cpp
void EntityManager::FlushComponentChanges() {
	EntityManager& self = *instance_;
	for (const auto& [handle, initial] : self.pendingAddModelRenderer_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity || self.modelRenderers_.IsAlive(entity->render)) {
			continue;
		}
		entity->render = self.modelRenderers_.Create();
		*self.modelRenderers_.Get(entity->render) = initial; // 予約したときの値を入れる
	}
	self.pendingAddModelRenderer_.clear();
}
```

- `std::find` は使わなくなった（配列の中身が `pair` になったので、`handle` と直接比べられない）。
- **3eで「Add Componentの表にラムダを入れた」のがここで効く**：`RequestAddModelRenderer` に引数が増えても、表の中のラムダは `RequestAddModelRenderer(handle)` と呼んでいるだけなので（初期値は既定の `{}`）、**Inspectorは直さなくてよい**。関数を直接表に入れていたら、関数の型が変わってコンパイルエラーになっていた。
- `Entity::render` のメンバ名は3eで変えずに残っているので、そのまま `render` を使っている（変えたければ後で `modelRenderer` に。動作は同じ）。

#### ゲーム側（CG3_Project）

**`GameScene.h`**（全体を差し替え）
```cpp
#pragma once
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <MyEngine/Scene/IScene.h>
#include <MyEngine/Core/Handle.h>
#include <MyEngine/Entity/Entity.h>
#include <MyEngine/Graphics/IBL/IBLIncludes.h>
#include <MyEngine/Graphics/Renderer/Renderer.h>
#include "Particles.h"
#include "Stage.h"

// 前方宣言
class Camera;
class DebugCamera;


/// <summary>
/// ゲームシーン
/// </summary>
class GameScene : public IScene {
public:
	// 調整項目
	Vector3 lightDirection_ = {0.0f, 1.0f, 0.0f};
	float lightIntensity_ = 1.0f;

	static constexpr int32_t kPointLightNum = 2;
	
	~GameScene() override;
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;
	Camera* GetCamera() override { return  camera_; }

	void RegisterGV();
	void ApplyGV();

public:
	// エンジン組み込み
	Camera* camera_ = nullptr;
	Particles* particles_;
	Stage* stage_;

	// 環境光（IBL）。シーンが1つ持ち、描画のときに ModelRenderSystem へ渡す
	IBLEnvironment environment_;
	// このシーンが作ったEntityをまとめる親。Finalizeでこれを消すと、子も全部消える
	Handle<Entity> sceneRoot_;
};
```

**`GameScene.cpp`**（全体を差し替え）
```cpp
#include "GameScene.h"
#include <MyEngine/Camera/Camera.h>
#include <MyEngine/UI/GlobalVariables.h>
#include <MyEngine/Editor/EditorOverlay.h>
#include <MyEngine/Math/MathUtility.h>
#include <MyEngine/Diagnostics/MyAssert.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Scene/Skybox.h>
#include <MyEngine/Graphics/IBL/IBLBaker.h>
#include <MyEngine/Graphics/Texture/TextureManager.h>
#include <MyEngine/Graphics/Renderer/SceneRenderer.h>
#include <MyEngine/Graphics/RenderTarget/RenderTextureCube.h>
#include <MyEngine/Entity/EntityManager.h>
#include <MyEngine/Entity/ModelRenderSystem.h>
#include <MyEngine/Graphics/Model/ModelManager.h>
#ifdef USE_IMGUI
#include <MyEngine/Editor/ImGuiManager.h>
#endif
#include <externals/imgui/imgui.h>

#ifdef USE_IMGUI
namespace {
// キューブマップの6面を3×2で並べて表示する（IBLの確認用。MonsterBall.cppから移した）
void DrawCubeFacesImGui(const char* label, RenderTextureCube* cube, float size) {
	if (!cube) {
		return;
	}
	const char* names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
	ImGui::Begin(label);
	for (uint32_t face = 0; face < 6; ++face) {
		ImGui::BeginGroup();
		ImGui::Text("%s", names[face]);
		ImGui::Image((ImTextureID)cube->GetFaceSRVGPUHandle(face).ptr, ImVec2(size, size));
		ImGui::EndGroup();
		if (face % 3 != 2) {
			ImGui::SameLine();
		}
	}
	ImGui::End();
}

// 2Dテクスチャを1枚表示する（BRDF LUTの確認用）
void DrawTexture2DImGui(const char* label, RenderTexture* tex, float size) {
	if (!tex) {
		return;
	}
	ImGui::Begin(label);
	ImGui::Image((ImTextureID)tex->GetSRVGPUHandle().ptr, ImVec2(size, size));
	ImGui::End();
}
} // namespace
#endif

//==========================================
// デストラクタ
//==========================================
GameScene::~GameScene() {}

//==========================================
// 終了処理
//==========================================
void GameScene::Finalize() { 
	// このシーンが作ったEntityを消す（子も一緒に消える。実際に消えるのはフレームの最後）
	// 消さないと、Stop（シーンの作り直し）のたびにEntityが増えていく
	EntityManager::Destroy(sceneRoot_);

	delete camera_; 
	delete stage_;
	delete particles_;

	camera_ = nullptr;
	stage_ = nullptr;
	particles_ = nullptr;
}

//==========================================
// 初期化
//==========================================
void GameScene::Initialize() {
	// カメラ
	camera_ = new Camera();
	camera_->Initialize(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
	camera_->SetTranslation({0.0f, 0.0f, -30.0f});

	// 環境光（IBL）。PBRのモデルはこれが無いと描かれない
	environment_.MakeFromHDR(TextureManager::Load("resources/monsterBall/sky.hdr"));

	// このシーンが作るEntityは全部この下に入れる（Hierarchyでもまとまって見える）
	sceneRoot_ = EntityManager::Create("GameScene");

	// モンスターボール（前の MonsterBall クラスの代わり。位置や向きはInspectorで触る）
	ModelRendererComponent monsterBall;
	monsterBall.modelHandle = ModelManager::Load("resources/monsterBall/monsterBall.gltf");
	monsterBall.shadingType = ShadingType::PBR;
	EntityManager::RequestAddModelRenderer(EntityManager::Create("MonsterBall", sceneRoot_), monsterBall);

	// パーティクル
	particles_ = new Particles();
	particles_->Initialize();
	ParticleManager::SetCamera(camera_);
	// ステージ
	stage_ = new Stage();
	stage_->Initialize();
	stage_->SetCamera(camera_);
	// 天球
	SceneRenderer::GetSkybox()->SetIntensity(1.0f);
	SceneRenderer::GetSkybox()->SetRotationY(0.0f); 

	RegisterGV();
	GlobalVariables::GetInstance()->LoadFiles();
}

//==========================================
// 更新
//==========================================
void GameScene::Update() {
	// --- ゲームオブジェクト ---
	particles_->Update();
	stage_->Update();

	if(InputManager::IsKeyPressed(DIK_SPACE)) {
		LogManager::Log("Push!");
	}

#ifdef USE_IMGUI
	// IBLの中身の確認用ウィンドウ（MonsterBallから移した）
	ImGuiManager::AddDrawRequest([this]() {
		DrawCubeFacesImGui("EnvCube", environment_.GetEnvironment(), 128.0f);
		DrawCubeFacesImGui("Irradiance", environment_.GetIrradiance(), 128.0f);
		DrawCubeFacesImGui("Prefilter", environment_.GetPrefilter(), 128.0f);
		DrawTexture2DImGui("BRDF LUT", environment_.GetBrdfLUT(), 256.0f);
	});
#endif

	ApplyGV();
}

//==========================================
// 描画
//==========================================
void GameScene::Draw() {
	// stage_->Draw();

	// Entityを描く（rootを省略＝全Entityが対象。IBLはシーンのものを渡す）
	ModelRenderSystem::Draw({}, camera_, &environment_, GetWindowTitle());
}


//==========================================
// 調整項目
//==========================================
void GameScene::RegisterGV() {
}

void GameScene::ApplyGV() {
}
```

**消す**：`MonsterBall.h` と `MonsterBall.cpp`（ソリューションエクスプローラで右クリック →「削除」→「削除」でファイルごと）。調整項目の `GameScene/MonsterBall` のjsonが残っていても害は無い。

**解説**
- **`sceneRoot_` の下にまとめて、`Finalize` で消す理由**：エンジンの Stop は `SceneManager::ReloadImmediate`（`Finalize` → シーンを作り直して `Initialize`）。Entityは `EntityManager` が持っていてシーンと一緒には消えないので、消さないと**Stopのたびに MonsterBall が1個ずつ増える**。親を1つ作ってその下に入れておけば、`Destroy(sceneRoot_)` 1回で子も全部消える（Step 1で作った「親を消すと子も消える」）。
- **Hierarchyで作ったEntityは消えない**：`sceneRoot_` の外（root）にあるので、Stopしても残る。逆に、手で `GameScene` の下へドラッグしたEntityは、Stopで一緒に消える。
- **パスを `resources/monsterBall/`（小文字のm）にした理由**：前の `MonsterBall.cpp` は `resources/MonsterBall/...` と書いていた。Windowsは大文字小文字を区別しないので読めるが、`ModelManager` はパスの**文字列**でキャッシュしている。Inspectorのモデル一覧（フォルダを走査した結果＝ディスク上の本当の名前）から同じモデルを選ぶと、**別のモデルとして2回読み込まれる**。ディスク上の名前に揃えておく。
- **`ModelRenderSystem` は PBR のモデルを、IBLが無いと描かない**（3bで決めた動き）。`&environment_` を渡すようになったので、Inspectorで `Shading = PBR` にしたEntityも描かれるようになる。
- **`ParticleManager::SetCamera(camera_)` は残していてよい**（3eの⑦でエンジンが毎フレーム渡すようになったので、無くても動く）。

---

### 確認すること（全部まとめて）
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**

**名前の変更（パートB）**
- Hierarchyの名前を**ダブルクリック**すると、その場で入力欄になり、全選択されている
- 打ってEnterで確定。Escで元に戻る。欄の外をクリックしても確定
- 選んで **F2** でも同じ。右クリック →「Rename」でも同じ
- 「Create Entity」を押すと、**作った直後から名前を打てる**
- 名前を打っている間に「W」「Space」などを押しても、カメラやゲームが反応しない
- 名前を全部消してEnterすると、元の名前のまま

**日本語（パートC）**
- Hierarchy・Inspectorの両方で、**日本語の名前**を付けられる（IMEの変換窓が入力欄の近くに出る）
- 30文字以上の日本語でも途中で切れない
- **珍しい漢字（髙、﨑）や記号（★♪①）**も「?」にならない
- ImGuiのほかの文字（ログ、プロファイラなど）が今まで通り出る

**Add Component（パートD）**
- ボタンの**真下に、ボタンと同じ幅**の窓が出る
- `Rendering3D` を押すと**下に**開き、`Model Renderer` が出る。もう一度押すと閉じる
- 開いた瞬間に文字を打てる。`model` と打つと、該当するものだけが出る（カテゴリは自動で開く）
- すでに持っているComponentは灰色で押せない

**3f（パートE）**
- 起動すると、Hierarchyに `GameScene` → `MonsterBall` が出て、**PBRでIBLの映り込みがある**モンスターボールが見える（前の見た目と同じ）
- `MonsterBall` を選ぶと、Inspectorで位置・回転・大きさ・Shadingを変えられる
- **Stop（シーンの作り直し）を何回しても MonsterBall が増えない**
- EnvCube / Irradiance / Prefilter / BRDF LUT の確認用ウィンドウが今まで通り出る
- Hierarchyで作ったEntityに `Shading = PBR` を選んでも**消えなくなる**（IBLが渡るようになったので）
