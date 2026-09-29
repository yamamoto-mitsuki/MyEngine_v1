# SceneFlow：シーンの切り替えを、コードを書かずに（エディタで）

## このファイルについて

- 2026-09-29 の依頼：「シーンの遷移をコードでやるのは面倒なので、なんとかしたい」。
- **今回は設計と順番だけ**（写経用のコードはまだ無い。SF0 から順に、このファイルへ足していく）。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。
- 関連：[Serialize.md](Serialize.md)（シーンファイル・Play / Stop の仕組み、S5 小さなゲーム）、[GameBasics.md](GameBasics.md) の12章（SF0 の元になった質問）、[Animation.md](Animation.md)（もう1つの次の作業）。

| 読む順番 | 何が書いてあるか |
|---|---|
| [1. 今、何が面倒か](#1-今何が面倒か) | タイトル → ゲーム → リザルトを作るときに、今やらないといけないこと |
| [2. どうしたいか](#2-どうしたいかunity-と同じ形) | 目標の形（Unity と同じ） |
| [3. 仕組みの案](#3-仕組みの案) | エンジンの中をどう変えるか |
| [4. 順番](#4-順番) | SF0〜SF4 と、アニメーション・S5 との順番 |
| [5. 決めること](#5-決めること) | 始める前に決めてほしいこと（おすすめ付き） |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| SF0 | 小さな整理（GameBasics.md 12章の 3・5・6 と、ゲームの残り物） | 設計済み・コードはまだ |
| SF1 | シーン＝ファイル。`SceneManager::LoadScene("名前")`。Play は開いているシーンから | 設計済み・コードはまだ |
| SF2 | エディタの File メニュー（New / Open / Save As）・保存していない印 | 設計済み・コードはまだ |
| SF3 | コードを書かない切り替え（Scene Change Component）とフェード | 設計済み・コードはまだ |
| SF4 | IBL・天球を Component に → `GameScene` クラスが要らなくなる | 設計済み・コードはまだ |

---

## 1. 今、何が面倒か

タイトル → ゲーム → リザルト の3つを作るとき、今のエンジンでやらないといけないこと：

| # | 今やること | 面倒な所 |
|---|---|---|
| 1 | シーンごとに C++ のクラスを作る（`TitleScene.h/.cpp` …）。`GetSceneFile()` も書く | 中身がほぼ空でも2ファイル |
| 2 | 切り替えは `NextScene()` を上書きして、キーを見て `return std::make_unique<ResultScene>();` | シーンのクラスが次のシーンのヘッダを読む。遷移を変えるたびにビルド |
| 3 | **エディタで開けるのは、`main.cpp` で最初に作るシーンだけ** | タイトルの配置を直すには、`main.cpp` を書き換えてビルド → 直したら戻す |
| 4 | Play はいつも最初のシーンから | リザルトの見た目を確かめるのに、毎回ゲームを最後まで遊ぶ |
| 5 | 切り替えの演出（フェード）が無い | パッと変わる |
| 6 | 次のシーンへ値（スコア）を渡す方法が決まっていない | 各自でグローバル変数 |

いちばん困るのは **3 と 4**（編集したいシーンを開けない）。1 と 2 は「コードを書く量」の問題。

---

## 2. どうしたいか（Unity と同じ形）

- **シーン＝シーンファイル**（`resources/scenes/Title.scene.json`）。**名前はファイル名**（`"Title"`）。シーンごとの C++ クラスは要らない。
- **エディタ**：メニューの File → New Scene / Open Scene / Save / Save As。Control ウィンドウに開いているシーンの名前（保存していなければ `Title *`）。
- **Play は開いているシーンから始まり、Stop で「そのシーンの、Play を押した瞬間」に戻る**（ゲームの中でリザルトへ進んでいても）。
- **ゲームのコードからは1行**：`SceneManager::LoadScene("Result");`
- **コードを書かずに**：Entity に **Scene Change** を付けて「決定キーを押したら／ぶつかったら／○秒たったら → Result へ（フェード 0.5秒）」を Inspector で選ぶ。
- 製品版で起動したときの最初のシーンは、`main.cpp` に1行（`Engine::Initialize(wc, "Title")`）。

---

## 3. 仕組みの案

### SF0：小さな整理（質問から出た物をまとめて）

| 何を | どう | 元 |
|---|---|---|
| `SoundBus` → `AudioBus` | 型名とファイル名の置き換え。保存データはそのまま読める | GameBasics.md 12章の3 |
| `GetMousePositionX/Y` → `GetMousePositionInWindow()` | `Vector2` を返す1つに | 12章の5 |
| Scene ビューにもポストエフェクト | Scene ビューに「Effects」のチェック（既定 ON）。Lens は Scene では掛けない | 12章の6 |
| ゲームの残り物 | `GameScene` の `RegisterGV`/`ApplyGV`・`lightDirection_`・`lightIntensity_`・`kPointLightNum`、`AudioKeyTest`（確かめ終わった） | 12章の1 |
| Reference.md に名前の決まり | 「Sound＝素材、Audio＝鳴らす仕組み」「Calc＝その場で計算」 | 12章の2・3 |

### SF1：シーン＝ファイル（エンジン）

| 変えること | 中身 |
|---|---|
| `SceneManager::LoadScene(name)` | どこからでも呼べる static 関数。**実際に切り替えるのは次のフレームの頭**（今の `NextScene()` と同じ）。ファイルが無ければ Log に警告して切り替えない。どのウィンドウの SceneManager に頼むかは、「今 Update しているウィンドウ」（`InputManager::SetActiveWindow` と同じ考え方） |
| 名前 → クラスの表 | `REGISTER_SCENE(GameScene)` と書いたクラスは、同じ名前のシーンファイルを開くときに使う（COMPONENT と同じ、書くだけで登録される仕組み）。**表に無い名前は「Entity だけのシーン」**（エンジンの中の、何もしない IScene）。今の `GameScene` はそのまま動く |
| Play / Stop | 「最初のシーンの作り方」（`sceneFactory_`）の代わりに「**開いているシーンの名前**」を覚える。Stop・Restart はその名前のシーンを、Play の瞬間の退避から作り直す（退避の仕組みは今のまま） |
| 最初のシーン | `Engine::Initialize(wc, "Title")`（名前を渡す）。前の書き方（`[] { return std::make_unique<GameScene>(); }`）も当分は使えるようにする |
| `IScene::NextScene()` | 残す（前のシーンのコードが動くように）。新しく書くときは `LoadScene` |
| 値の受け渡し（スコア） | 決まりだけ：**ゲームに1つの値は `inline` のグローバルな構造体**（Reference.md 6章の `gGameState`）。シーンが替わっても消えない。Entity ごと持ち越す仕組み（Unity の DontDestroyOnLoad）は困ってから |
| BGM をまたいで鳴らす | 今のまま：`SoundManager::Play` で直接鳴らした音は、シーンを替えても止まらない（止まるのは Play / Stop / Restart だけ） |

### SF2：エディタの File メニュー

| 変えること | 中身 |
|---|---|
| File → New Scene | 名前を打つ → 空のシーン（Main Camera と Directional Light だけ）を作って保存し、開く |
| File → Open Scene | `resources/scenes/` の `*.scene.json` の一覧から選ぶ |
| File → Save（Ctrl+S）・Save As | Save As は名前を打つ（ファイルを写して、開いているシーンをそちらに） |
| 保存していない印（`*`） | 保存した時点から Entity の変更（履歴）があれば `*`。Undo で保存した時点まで戻したら消える |
| 開き直すときの確認 | `*` のまま別のシーンを開こうとしたら「保存しますか？ Save / Don't Save / Cancel」 |
| 最後に開いていたシーン | エディタの設定ファイルに覚えて、次の起動でそれを開く（`imgui.ini` と同じ考え方） |
| シーンもアセットとして選べる | `AssetType::Scene` を足す（SF3 の「どのシーンへ」を、音やモデルと同じコンボで選ぶため） |

### SF3：コードを書かない切り替え

- **Scene Change Component**（エンジンの Component）：

| 項目 | 中身 |
|---|---|
| Scene | 行き先（`AssetType::Scene` のコンボ） |
| Trigger | 何が起きたら：**決定キー**（Space・Enter・Z・パッドのA・Game ビューのクリックのどれか）／**ぶつかったら**（その Entity の Collider の Enter）／**時間**（Delay 秒たったら） |
| Fade | フェードの秒数（0 ならパッと） |

- **フェード**：`SceneManager::LoadScene(name, fadeSeconds)`。暗くする → 切り替える → 明るくする を SceneManager がやる（画面いっぱいの黒いスプライトの濃さを変えるだけ）。フェードの間はゲームの入力を止める。
- ボタンの UI（「スタート」の文字の上をクリック）は、2D の UI が無いので今は「Game ビューのどこかをクリック」まで。UI を作るときに足す。

### SF4：シーンのクラスを要らなくする

- 今 `GameScene` が持っている物のうち、Entity でない物を Component にする：
  - **IBL・天球 → Environment Component**（HDR のファイル・明るさ・回転）。シーンごとに空を変えられる。IBL の作り直し（重い）は HDR を変えたときだけ。**作り直すとき、前の IBL の SRV スロットを返す**（Collision.md の SRV 漏れと同じ罠）。
  - `Particles` → パーティクルを Component にするとき（学校の Field の課題と一緒に）。`Stage` → Entity に置き換え。
- **Entity の描画をエンジンが自動でやる**（今は各シーンの `Draw()` が `ModelRenderSystem::Draw` を呼んでいる）。シーンのクラスが無くても全部描かれる。
- ここまで来ると `GameScene` クラスを消せて、`main.cpp` は `Engine::Initialize(wc, "Title")` だけになる。

### 後回し

| 何を | いつ |
|---|---|
| Entity を次のシーンへ持ち越す（DontDestroyOnLoad） | BGM 係・プレイヤーの状態を Entity ごと持ち越したくなったら |
| 裏で読み込む（ロード画面） | 読み込みが1秒を超えるようになったら（マルチスレッドの話と一緒に） |
| シーンを重ねる（Additive。UI のシーン＋ゲームのシーン） | 必要になったら |
| プレハブ（Entity のまとまりをファイルに） | Serialize.md の「次」。シーンを開けるようになった後がやりやすい |

---

## 4. 順番

| 順 | 作業 | 大きさ | ひとこと |
|---|---|---|---|
| 0 | [GameBasics.md](GameBasics.md) のパートF（起動できないバグの直し）→ 7章の確認 → コミット | 小 | まずここ |
| 1 | **SF0** 小さな整理 | 小 | 触るファイルは多いが、1つずつは数行 |
| 2 | **SF1** シーン＝ファイル・`LoadScene`・開いているシーンから Play | 中 | いちばん効く（3・4の面倒が無くなる） |
| 3 | **SF2** File メニュー | 中 | タイトル・リザルトをエディタで作れるようになる |
| 4 | **S5 小さなゲームを1本**（タイトル → ゲーム → リザルト） | 中 | ここで SF3（コード無しの切り替え・フェード）が欲しくなったら先に |
| 5 | SF3・SF4 | 中 | |
| 並行 | [Animation.md](Animation.md) | 大 | **学校の進み具合で。締め切りが近ければ 1 より先にやってよい**（SceneFlow とぶつかるファイルはほぼ無い） |

---

## 5. 決めること

| 決めること | おすすめ | 理由 |
|---|---|---|
| シーンの名前＝ファイル名（`resources/scenes/Title.scene.json` → `"Title"`）でよいか | はい | 名前を2か所に書かない。ファイル名を変えたら名前も変わる（Unity と同じ） |
| Play は「開いているシーンから」でよいか | はい | Unity と同じ。「いつも最初のシーンから」が欲しくなったら、Control ウィンドウにチェックを足す |
| `GameScene` クラスはしばらく残すか | はい（SF4 で消す） | IBL が Entity になるまでは、クラスが無いと PBR のモデルが描かれない |
| 最初のシーンは `main.cpp` に書くか、設定ファイルか | `main.cpp` | ゲームのコードに1行残るだけ。設定ファイルは「ビルドの設定」を作るときに |
