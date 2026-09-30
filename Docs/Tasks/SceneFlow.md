# SceneFlow：シーンの切り替えを、コードを書かずに（エディタで）

## このファイルについて

- 2026-09-29 の依頼：「シーンの遷移をコードでやるのは面倒なので、なんとかしたい」。
- 2026-09-30：5章の「決めること」は**全部「はい」**。質問1つ（Unity のシーンは、ファイルを表示している所から作るのでは？）と、気になること1つ（Parameters・PostEffect のようなゲーム全体の物は、1か所にまとめた方がいいのでは？）に [6章](#6-質問気になることへの答え2026-09-30) で答えた。それに合わせて **SF2 と SF4 の設計を直し、SF2.5（Settings ウィンドウ）を足した**。
- **SF0・SF1 は写経用のコードを [7章](#7-手順sf0sf1) に入れた**（パート A〜C）。SF2 から先は設計だけ（[10章](#10-決めること2回目) に答えてもらったら、次にコードを出す）。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。
- 関連：[Serialize.md](Serialize.md)（シーンファイル・Play / Stop の仕組み、S5 小さなゲーム）、[GameBasics.md](GameBasics.md) の12章（SF0 の元になった質問）、[Animation.md](Animation.md)（SceneFlow と S5 の後）、[Reference.md](../Reference.md)（SF1 の後の書き方に更新済み）。

| 読む順番 | 何が書いてあるか |
|---|---|
| [1. 今、何が面倒か](#1-今何が面倒か) | タイトル → ゲーム → リザルトを作るときに、今やらないといけないこと |
| [2. どうしたいか](#2-どうしたいかunity-と同じ形) | 目標の形（Unity と同じ） |
| [3. 仕組みの案](#3-仕組みの案) | SF0〜SF4（**SF2・SF4 は 2026-09-30 に直した。SF2.5 を足した**） |
| [4. 順番](#4-順番) | SF0〜SF4 と、S5・アニメーションとの順番 |
| [5. 決めること（1回目）](#5-決めること1回目) | 答え：全部はい |
| [6. 質問・気になることへの答え](#6-質問気になることへの答え2026-09-30) | **Unity のシーンの作り方／ゲーム全体の設定をまとめる／アニメの順番** |
| [7. 手順（SF0・SF1）](#7-手順sf0sf1) | **写経用のコード（パート A〜C）** |
| [8. 確認すること](#8-確認すること) | 写した後に確かめること |
| [9. 検証したこと](#9-検証したこと2026-09-30claude) | Claude がやったビルドと、動かして確かめたこと |
| [10. 決めること（2回目）](#10-決めること2回目) | **SF2・SF2.5・SF4 を始める前に決めてほしいこと** |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| SF0 | 小さな整理（`AudioBus`・`GetMousePositionInWindow`・Scene ビューのポストエフェクト・ゲームの残り物） | **写経待ち**（7章 パート A・C） |
| SF1 | シーン＝ファイル。`SceneManager::LoadScene("名前")`・`REGISTER_SCENE`・`Engine::Initialize(wc, "名前")`。Play は開いているシーンから | **写経待ち**（7章 パート B・C） |
| SF2 | **Project ウィンドウ**（シーンを作る・開く）・File メニュー・保存していない印 | 設計（6章で直した） |
| SF2.5 | **Settings ウィンドウ**（ゲーム全体・シーン・エディタの設定を1か所に） | 設計（6章で足した） |
| SF3 | コードを書かない切り替え（Scene Change Component）とフェード | 設計済み |
| SF4 | 空・IBL を**シーンの設定**に → `GameScene` クラスが要らなくなる | 設計（6章で直した） |

---

## 1. 今、何が面倒か

タイトル → ゲーム → リザルト の3つを作るとき、今のエンジンでやらないといけないこと：

| # | 今やること | 面倒な所 | 直るのは |
|---|---|---|---|
| 1 | シーンごとに C++ のクラスを作る（`TitleScene.h/.cpp` …）。`GetSceneFile()` も書く | 中身がほぼ空でも2ファイル | **SF1**（クラスが無いシーンは DefaultScene で動く） |
| 2 | 切り替えは `NextScene()` を上書きして、キーを見て `return std::make_unique<ResultScene>();` | シーンのクラスが次のシーンのヘッダを読む。遷移を変えるたびにビルド | **SF1**（`LoadScene("Result")`）・SF3（コード無し） |
| 3 | **エディタで開けるのは、`main.cpp` で最初に作るシーンだけ** | タイトルの配置を直すには、`main.cpp` を書き換えてビルド → 直したら戻す | SF2（Project ウィンドウで開く） |
| 4 | Play はいつも最初のシーンから | リザルトの見た目を確かめるのに、毎回ゲームを最後まで遊ぶ | **SF1**（開いているシーンから）＋SF2 |
| 5 | 切り替えの演出（フェード）が無い | パッと変わる | SF3 |
| 6 | 次のシーンへ値（スコア）を渡す方法が決まっていない | 各自でグローバル変数 | **SF1**（決まりを Reference.md に） |

いちばん困るのは **3 と 4**（編集したいシーンを開けない）。1 と 2 は「コードを書く量」の問題。

---

## 2. どうしたいか（Unity と同じ形）

- **シーン＝シーンファイル**（`resources/scenes/Title.scene.json`）。**名前はファイル名**（`"Title"`）。シーンごとの C++ クラスは要らない。
- **エディタ**：**Project ウィンドウ**（`resources/` のファイルの一覧）で右クリック → Create → Scene で作り、ダブルクリックで開く（2026-09-30 に足した）。File メニューの New Scene / Open Scene / Save / Save As も近道として置く。Control ウィンドウに開いているシーンの名前（保存していなければ `Title *`）。
- **Play は開いているシーンから始まり、Stop で「そのシーンの、Play を押した瞬間」に戻る**（ゲームの中でリザルトへ進んでいても）。
- **ゲームのコードからは1行**：`SceneManager::LoadScene("Result");`
- **コードを書かずに**：Entity に **Scene Change** を付けて「決定キーを押したら／ぶつかったら／○秒たったら → Result へ（フェード 0.5秒）」を Inspector で選ぶ。
- **ゲーム全体の設定・シーンの設定・エディタの設定は、Settings ウィンドウ1つにまとめる**（2026-09-30 に足した）。
- 製品版で起動したときの最初のシーンは、`main.cpp` に1行（`Engine::Initialize(wc, "Title")`）。

---

## 3. 仕組みの案

### SF0：小さな整理（質問から出た物をまとめて）→ 7章 パート A・C

| 何を | どう | 元 |
|---|---|---|
| `SoundBus` → `AudioBus` | 型名とファイル名の置き換え。保存データ（`"BGM"`・`"SE"`）はそのまま読める | GameBasics.md 12章の3 |
| `GetMousePositionX/Y` → `GetMousePositionInWindow()` | `Vector2` を返す1つに | 12章の5 |
| Scene ビューにもポストエフェクト | **View メニュー → Scene View → Post Effects**（既定 ON）。Lens は Scene では掛けない | 12章の6 |
| ゲームの残り物 | `GameScene` の `RegisterGV`/`ApplyGV`・`lightDirection_`・`lightIntensity_`・`kPointLightNum`、`AudioKeyTest`（確かめ終わった）。**`LoadFiles()` も消す**（6.2 の「見つけたこと」） | 12章の1 |
| Reference.md に名前の決まり | 「Sound＝素材、Audio＝鳴らす仕組み」「Calc＝その場で計算」 | 12章の2・3（Reference.md 1章に書いた） |

### SF1：シーン＝ファイル（エンジン）→ 7章 パート B・C

| 変えること | 中身 |
|---|---|
| `SceneManager::LoadScene(name)` | どこからでも呼べる static 関数。**実際に切り替えるのは次のフレームの頭**（今の `NextScene()` と同じ）。シーンが無ければ Log に警告して `false`（切り替えない）。どのウィンドウの SceneManager に頼むかは「今 Update しているウィンドウ」（Update の外なら最初のウィンドウ）。**停止中は何もしない**（エディタで開いているシーンを、ゲームのコードが替えないように） |
| 名前 → クラスの表 | `REGISTER_SCENE(GameScene)` と書いたクラスは、同じ名前のシーンファイルを開くときに使う（COMPONENT と同じ、書くだけで登録される仕組み）。**表に無い名前は `DefaultScene`**（エンジンのシーン。Entity だけで動き、IBL はエンジンの既定の空で作るので PBR のモデルも出る） |
| シーンファイルの場所 | 名前から決まる（`"Title"` → `resources/scenes/Title.scene.json`）。`GetSceneFile()` は上書きしなくてよい |
| Play / Stop | 「最初のシーンの作り方」の代わりに「**開いているシーンの作り方**」を覚える。Stop・Restart はそのシーンを、Play の瞬間の退避から作り直す（退避の仕組みは今のまま） |
| 最初のシーン | `Engine::Initialize(wc, "GameScene")`（名前を渡す）。前の書き方（`[] { return std::make_unique<GameScene>(); }`）もそのまま使える。**名前のファイルが無ければ、カメラと平行光源だけの新しいシーン**で開く（Save するとファイルができる） |
| Control ウィンドウ | `Scene: GameScene (resources/scenes/GameScene.scene.json)`。再生中に別のシーンへ進んだら `Scene: GameScene → 今は Result（Stop で GameScene に戻ります）` |
| `IScene::NextScene()` | 残す（前のシーンのコードが動くように）。新しく書くときは `LoadScene` |
| 値の受け渡し（スコア） | 決まりだけ：**ゲームに1つの値は `inline` のグローバルな構造体**（Reference.md 6章の `gGameState`）。シーンが替わっても消えない。Entity ごと持ち越す仕組み（Unity の DontDestroyOnLoad）は困ってから |
| BGM をまたいで鳴らす | 今のまま：`SoundManager::Play` で直接鳴らした音は、シーンを替えても止まらない（止まるのは Play / Stop / Restart だけ） |

### SF2：Project ウィンドウと File メニュー（2026-09-30 直した）

前の設計は File メニューだけだった。**Unity と同じく「Project ウィンドウで作る・開く」を主にして**、File メニューは近道として残す（理由は 6.1）。

```
┌ Project ────────────────────────────────────────────────┐
│ resources / scenes                                      │
├───────────────────┬─────────────────────────────────────┤
│ ▼ resources        │  [Scene] GameScene                  │
│   ▶ enemy          │  [Scene] Result                     │
│   ▶ monsterBall    │  [Scene] Title                      │
│   ▼ scenes   ←選択 │                                     │
│   ▶ sound          │  （空いた所を右クリック）             │
│                    │   Create ▶ Scene / Folder           │
└───────────────────┴─────────────────────────────────────┘
```

| できること | 中身 |
|---|---|
| 見る | 左に `resources/` のフォルダ、右に選んだフォルダの中身。ファイルの種類（Scene・Model・Texture・Sound・その他）を名前の前に出す |
| **シーンを作る** | 右の空いた所を右クリック → Create → Scene → 名前を打つ（Unity と同じ）。中身はカメラと平行光源（DefaultScene の最初の配置と同じ）。**作っただけでは開かない** |
| **シーンを開く** | ダブルクリック。保存していない変更があれば「保存しますか？ Save / Don't Save / Cancel」。**停止中だけ**（再生中は押せない） |
| 名前を変える・消す | F2・右クリック → Rename / Delete（消すのはごみ箱へ。開いているシーンは消せない） |
| フォルダを作る | Create → Folder |
| File メニュー（近道） | New Scene（名前を打つ → 作って開く）・Open Scene（`resources/scenes/` の一覧）・Save（Ctrl+S）・Save As（名前を打つ。ファイルを写して、開いているシーンをそちらに） |
| 保存していない印（`*`） | 保存した時点から Entity の変更（履歴）があれば Control ウィンドウの名前と Project の名前に `*`。Undo で保存した時点まで戻したら消える |
| 最後に開いていたシーン | エディタの設定ファイルに覚えて、次の起動でそれを開く（`main.cpp` のシーンは製品版の最初のシーン。`imgui.ini` と同じ考え方） |
| シーンもアセットとして選べる | `AssetType::Scene` を足す（SF3 の「どのシーンへ」を、音やモデルと同じコンボで選ぶため） |
| 後で | Project から Inspector のモデル・音の欄へドラッグ、Project の中で動かしたらシーンの中のパスも書き換える（Audio.md 2章） |

### SF2.5：Settings ウィンドウ（2026-09-30 足した）

**ゲーム全体・シーンごと・エディタだけ の設定を、1つのウィンドウにまとめる**（理由と中身は 6.2）。

- 左に一覧・右にページ（Unity の Project Settings と同じ形）。一覧は**保存先ごとの3つの見出し**（Project／Scene／Editor）。
- 今の Parameters ウィンドウと、グループごとの小さいウィンドウ（PostEffect・Audio・Debug・GameScene）は無くなり、全部この中に入る。
- ゲームのコード（`GlobalVariables::GetInstance()->Group("GameScene")...`）は**書き方が変わらない**（Project の下に出る）。

### SF3：コードを書かない切り替え

- **Scene Change Component**（エンジンの Component）：

| 項目 | 中身 |
|---|---|
| Scene | 行き先（`AssetType::Scene` のコンボ） |
| Trigger | 何が起きたら：**決定キー**（Space・Enter・Z・パッドのA・Game ビューのクリックのどれか）／**ぶつかったら**（その Entity の Collider の Enter）／**時間**（Delay 秒たったら） |
| Fade | フェードの秒数（0 ならパッと） |

- **フェード**：`SceneManager::LoadScene(name, fadeSeconds)`。暗くする → 切り替える → 明るくする を SceneManager がやる（画面いっぱいの黒いスプライトの濃さを変えるだけ）。フェードの間はゲームの入力を止める。
- ボタンの UI（「スタート」の文字の上をクリック）は、2D の UI が無いので今は「Game ビューのどこかをクリック」まで。UI を作るときに足す。

### SF4：シーンのクラスを要らなくする（2026-09-30 直した）

- 今 `GameScene` が持っている物のうち、Entity でない物をエンジンへ移す：
  - **空・IBL → シーンの設定（Settings ウィンドウの Scene のページ）**。HDR のファイル・明るさ・回転。シーンファイルの `"settings"` に入る。**前の案（Environment Component）はやめた**：Entity に付ける形だと、2つ置けてしまう・消せてしまう。「シーンに1つだけの設定」は Unity でも Lighting ウィンドウ（シーンファイルに入る）で、Entity ではない。
  - IBL の作り直し（重い）は HDR を変えたときだけ。**作り直すとき、前の IBL の SRV スロットを返す**（Collision.md の SRV 漏れと同じ罠）。
  - `Particles` → パーティクルを Component にするとき（学校の Field の課題と一緒に）。`Stage` → Entity に置き換え。
- **DefaultScene がシーンの設定から IBL を作る**（SF1 では既定の空で作っている）。
- **Entity の描画をエンジンが自動でやる**（今は各シーンの `Draw()` が `ModelRenderSystem::Draw` を呼んでいる）。
- ここまで来ると `GameScene` クラスを消せて、`main.cpp` は `Engine::Initialize(wc, "Title")` だけになる。

### 後回し

| 何を | いつ |
|---|---|
| Entity を次のシーンへ持ち越す（DontDestroyOnLoad） | BGM 係・プレイヤーの状態を Entity ごと持ち越したくなったら |
| 裏で読み込む（ロード画面） | 読み込みが1秒を超えるようになったら（マルチスレッドの話と一緒に） |
| シーンを重ねる（Additive。UI のシーン＋ゲームのシーン） | 必要になったら |
| プレハブ（Entity のまとまりをファイルに） | Serialize.md の「次」。シーンを開けるようになった後がやりやすい |
| シーンごとのポストエフェクト（Unity の Volume） | 「このシーンだけ赤く暗く」をやりたくなったら（6.2） |

---

## 4. 順番

2026-09-30：アニメーションの締め切りは考えなくてよい（学校の課題はずっと先）ので、**SceneFlow → S5 → アニメーション** の順にした。

| 順 | 作業 | 大きさ | ひとこと |
|---|---|---|---|
| ✓ | [GameBasics.md](GameBasics.md) のパートF（起動できないバグの直し） | 小 | コミット済み（c43829b） |
| 1 | **SF0** 小さな整理（7章 パート A・C） | 小 | 触るファイルは多いが、1つずつは数行 |
| 2 | **SF1** シーン＝ファイル・`LoadScene`・開いているシーンから Play（7章 パート B・C） | 中 | 1・2・4・6 の面倒が無くなる |
| 3 | **SF2** Project ウィンドウ・File メニュー ＋ **SF2.5** Settings ウィンドウ | 中 | 次の md で一緒に出す。タイトル・リザルトをエディタで作れるようになる |
| 4 | **S5 小さなゲームを1本**（タイトル → ゲーム → リザルト） | 中 | ここで SF3（コード無しの切り替え・フェード）が欲しくなったら先に |
| 5 | SF3・SF4 | 中 | |
| 6 | [Animation.md](Animation.md)（AN1 クオータニオンから） | 大 | SceneFlow とぶつかるファイルはほぼ無い |

---

## 5. 決めること（1回目）

**答え（2026-09-30）：全部「はい」。**

| 決めること | おすすめ | 理由 | 答え |
|---|---|---|---|
| シーンの名前＝ファイル名（`resources/scenes/Title.scene.json` → `"Title"`）でよいか | はい | 名前を2か所に書かない。ファイル名を変えたら名前も変わる（Unity と同じ） | はい |
| Play は「開いているシーンから」でよいか | はい | Unity と同じ。「いつも最初のシーンから」が欲しくなったら、Control ウィンドウにチェックを足す | はい |
| `GameScene` クラスはしばらく残すか | はい（SF4 で消す） | 空・IBL がシーンの設定になるまでは、`GameScene` が IBL を作っている | はい |
| 最初のシーンは `main.cpp` に書くか、設定ファイルか | `main.cpp` | ゲームのコードに1行残るだけ。設定ファイルは「ビルドの設定」を作るときに | はい（`main.cpp`） |

---

## 6. 質問・気になることへの答え（2026-09-30）

### 6.1 Unity では、シーンは「ファイルを表示している所」から作るのでは？ → そう。このエンジンにも Project ウィンドウを作る

Unity のシーンの作り方は2つある：

| 作り方 | 手順 | 作った後 |
|---|---|---|
| **Project ウィンドウ**（Assets のファイルの一覧） | 置きたいフォルダで右クリック → Create → Scene → 名前を打つ | ファイル（`Title.unity`）ができるだけ。**ダブルクリックで開く** |
| File メニュー | File → New Scene（Ctrl+N） | 名前の無いシーンが開く。最初の Save で、保存先と名前を聞かれる |

ふだんよく使うのは **Project ウィンドウの方**（シーンも「ファイルの1つ」として、モデルや音と同じ所で作る・開く・名前を変える・消す）。前の SF2 は File メニューだけの設計だったので、**Project ウィンドウを主にするように直した**（3章の SF2）。

- このエンジンには、まだ Project ウィンドウが無い（モデル・テクスチャ・音は Inspector のコンボで選ぶだけで、ファイルを見る・作る・名前を変える場所が無い）。
- Project ウィンドウがあると、シーン以外にもよいことがある：フォルダを作る・名前を変える、（後で）Inspector へドラッグ、中で動かしたときにシーンの中のパスも直す（Audio.md 2章で「エクスプローラで動かすとパスが切れる。Project パネルの中で動かす形にすれば直る」と書いた物）。
- File メニュー（New / Open / Save / Save As）も Unity と同じく残す。キーだけで済ませたいとき用。

### 6.2 ゲーム全体の物（Parameters・PostEffect）は1か所にまとめた方がいい → そのとおり。Settings ウィンドウにまとめる（SF2.5）

**今どうなっているか**

- Parameters・PostEffect・Audio・Debug は、中身は全部同じ仕組み（`GlobalVariables` のグループ）。Parameters ウィンドウの中に、グループごとの小さいウィンドウをドッキングしている。
- 最初は Parameters の中に並ぶが、タブを動かせるので、ゲームの今の配置（`imgui.ini`）では **PostEffect が Profiler・Console の横、Debug が Inspector の横**にばらけている（検証で起動した画面で確認）。どれがゲーム全体の設定なのか、見た目で分からない。
- しかも、**保存先も「いつまで有効か」も違う3種類が混ざっている**：

| 今のタブ | 中身 | 種類 | Unity では |
|---|---|---|---|
| PostEffect | ブルーム・レンズ | ゲーム全体 | 全体の Volume（Project Settings → Graphics） |
| Audio | 音量 Master・BGM・SE | ゲーム全体 | Project Settings → Audio |
| GameScene（ゲームが登録） | パーティクルの発生位置など | ゲーム全体（ゲームの調整値） | 自分で作る設定ファイル（ScriptableObject） |
| Debug | Scene ビューのカメラの位置・視野角 | **エディタだけ**（製品版では要らない） | Preferences・Scene ビューが自分で覚える |
| （SF4 で作る）空・IBL | 空の画像・明るさ・回転 | **シーンごと** | Lighting ウィンドウ → Environment（シーンファイルに入る） |

**案：Settings ウィンドウ1つに、保存先ごとの3つの見出しで並べる**

```
┌ Settings ──────────────────────────────────────────────────────┐
│ ▼ Project（ゲーム全体）      │ Post Effect *                  [Save] │
│     Audio                   │ ▼ Bloom                              │
│   > Post Effect *           │     Enabled     1                    │
│     ── ゲーム ──             │     Threshold   1.00                 │
│     GameScene               │ ▶ Lens                               │
│ ▼ Scene: GameScene          │                                      │
│     Environment（SF4）       │                                      │
│ ▼ Editor（エディタだけ）      │  保存先: Parameters/PostEffect/      │
│     Scene View Camera       │                                      │
└─────────────────────────────┴──────────────────────────────────────┘
```

| 見出し | 入る物 | 保存先 | Play / Stop で戻るか | 製品版に入るか |
|---|---|---|---|---|
| **Project** | Audio・Post Effect（エンジン）、ゲームが登録したグループ（区切り線の下） | `Parameters/<名前>/`（今のまま） | 戻らない | 入る |
| **Scene: 〈開いているシーン〉** | Environment（空・IBL）…SF4 から | シーンファイルの `"settings"` | 戻る（シーンの一部なので） | 入る |
| **Editor** | Scene View Camera（今の Debug） | `Parameters/Editor/` | 戻らない | 入らない |

- Parameters ウィンドウと、グループごとの小さいウィンドウは無くなる。メニューから開く（どこに置くかは 10章）。
- 値の Undo（Ctrl+Z）は今のまま効く。
- 保存：ページごとの Save ボタン＋、変えて保存していないページに `*`（今はカテゴリごとの Save しかなく、保存したかどうかが分からない）。
- ゲームのコードは変わらない。エンジンの物だけ、登録するときに見出し（Project / Editor）を1つ足す。

**PostEffect はゲーム全体でよいか**

- 今は**ゲーム全体でよい**（どのシーンでも同じブルーム）。
- 「ボスのシーンだけ赤く暗く」のようにシーンごとに変えたくなったら、Scene の見出しに「Post Effect（上書き）」を足す。Unity の Volume と同じ考え方で、**全体の値をシーンの値で上書き**する（3章の「後回し」）。

**空・IBL は「シーンの設定」にする（SF4 を直した）**

- 前の案は「Environment Component を Entity に付ける」だった。でも空は、パーティクルや敵のように「何個も置く物」ではなく、「シーンに1つだけの設定」。Component にすると、2つ付けられる・Entity ごと消せてしまう。
- ゲーム全体の設定と同じく Settings ウィンドウに置き、保存先だけシーンファイルにする（Unity の Lighting ウィンドウと同じ）。タイトルは夜空、ゲームは昼、のようにシーンごとに変えられる。

**見つけたこと：Play を押すと、保存していない設定がファイルの値に戻っていた（パート C で直した）**

- `GameScene::Initialize` が `GlobalVariables::GetInstance()->LoadFiles()`（**全部のグループ**をファイルから読み直す）を呼んでいた。シーンは Play・Stop・Restart のたびに作り直されるので、**保存していない PostEffect・音量・Scene ビューのカメラの変更が、Play を押すたびにファイルの値に戻る**（まだ保存したことが無いグループは戻らないので、今まで気づきにくかった）。
- 起動のときは `Engine::Initialize` の最後で1回読んでいるので、シーンから呼ばなくてよい。**C-2 で消した**（パーティクルの値も、起動のときに読まれる）。
- Settings ウィンドウでは「Project と Editor は Play / Stop で戻らない」を決まりにする（Unity の Project Settings と同じ）。

### 6.3 アニメーションの締め切りは考えなくていい → SceneFlow → S5 → アニメーション の順にした

- 4章の順番を直した。アニメーションは SceneFlow（SF0〜SF4）と S5 の後。
- [Animation.md](Animation.md) の「決めること」も更新した。始めるときに、学校で書いた関数の名前と引数を教えてほしい（そこに合わせて、つなぐ所を書く）のは変わらない。

---

## 7. 手順（SF0・SF1）

- **パート A**（SF0 のエンジン側）・**パート B**（SF1 のエンジン側）は互いに独立。どちらから写してもよい。
- **パート C**（ゲーム側）は B の後（`REGISTER_SCENE`・`LoadScene` を使う）。C-1 だけは A・B と関係ないので、いつやってもよい。
- 変える所が多いファイルは「全体を差し替え」にした。
- 写し終わったら、エンジンをビルド → ゲームをビルド（エンジンのヘッダがゲームへ写される）。

| パート | 中身 | 触るファイル |
|---|---|---|
| A-0 | ブランチ | — |
| A-1 | `SoundBus` → `AudioBus` | `Sound/SoundBus.h` → `AudioBus.h`（名前を変える）＋ 5ファイルの置き換え |
| A-2 | `GetMousePositionInWindow` | `Input/InputManager.h/.cpp` |
| A-3 | Scene ビューにもポストエフェクト | `Graphics/PostEffect/PostProcess.h/.cpp`・`Graphics/Renderer/SceneRenderer.h/.cpp`・`Editor/Viewport/EditorViewport.h/.cpp`・`Editor/ImGuiManager.cpp` |
| B-1〜B-3 | シーンの名前・登録・`LoadScene` | `Scene/IScene.h`・`SceneManager.h`・`SceneManager.cpp`（全体を差し替え） |
| B-4 | `DefaultScene`（新規） | `Scene/DefaultScene.h/.cpp` |
| B-5〜B-8 | つなぐ所 | `Scene/Skybox.h/.cpp`・`Engine.h/.cpp`・`Window/WindowManager.cpp`・`ECS/GameComponent.h` |
| C-1 | `AudioKeyTest` を外す | ゲーム：`Source/Components/AudioKeyTest.cpp`（消す） |
| C-2・C-3 | `GameScene` の整理・名前で最初のシーン | ゲーム：`GameScene.h/.cpp`・`main.cpp` |
| C-4・C-5 | 確認用 | ゲーム：`Source/Components/SceneKeyTest.cpp`（新規）・`resources/scenes/Result.scene.json`（写す） |

### A-0. ブランチ

- **本体（`MyEngine_v1`）は、もう `feature/scene-flow` を作って切り替えてある**（`feature/game-basics` の c43829b の先。このファイルと Reference.md・Animation.md・Serialize.md の変更だけが入っている。コミットはしていない）。
- ゲーム（`CG3_Project`）はブランチを作らなくてよい（今まで通り）。

---

### パート A：SF0 の整理（エンジン）

#### A-1. `SoundBus` → `AudioBus`

**なぜ**：名前の決まり「**Sound＝音の素材**（ファイル・波形。`SoundManager`・`AssetType::Sound`）、**Audio＝鳴らす仕組み**（`AudioSourceComponent`・`AudioSystem`）」から、音量つまみの通り道（鳴らす仕組み）の `SoundBus` だけが外れていた（GameBasics.md 12章の3）。シーンファイルに保存されるのは値の名前（`"BGM"`・`"SE"`）なので、型の名前を変えても前のファイルはそのまま読める。

**手順**（Visual Studio の機能で置き換える）：

1. ソリューションエクスプローラーで `MyEngine/Sound/SoundBus.h` を右クリック → **名前の変更** → `AudioBus.h`（ファイルとプロジェクトの両方の名前が変わる）。
2. `AudioBus.h` を**全体を差し替え**（下のコード）。
3. **編集 → 検索と置換 → フォルダーを指定して置換**（Ctrl+Shift+H）：
   - 検索する文字列：`SoundBus` ／ 置換後の文字列：`AudioBus`
   - **大文字と小文字を区別する・単語単位で検索する** に✓、対象：**現在のプロジェクト**（MyEngine_v1）
   - 「すべて置換」→ **5 ファイル・29 か所**が置き換わればよい（`#include "MyEngine/Sound/SoundBus.h"` の3つも `AudioBus.h` になる）。

| ファイル | か所 |
|---|---|
| `Components/AudioSourceComponent.h` | 3（include・型・既定値） |
| `Sound/SoundManager.h` | 11 |
| `Sound/SoundManager.cpp` | 4 |
| `Systems/AudioSystem.h` | 3 |
| `Systems/AudioSystem.cpp` | 8 |

4. 最後に `SoundBus` で検索して、**0件**ならよい（ゲーム側は `SoundBus` を使っていない）。

`MyEngine/Sound/AudioBus.h`（全体を差し替え）

```cpp
#pragma once
#include <cstdint>

// 音の種類。鳴らす音は必ずどちらかを通り、種類ごとに音量を決められる
// （Parameters ウィンドウの Audio / Volume、ゲームからは AudioSystem::SetBusVolume）
// 名前の決まり：Sound＝音の素材（ファイル・波形）、Audio＝鳴らす仕組み。これは鳴らす仕組みなので Audio（Reference.md 1章）
// SoundManager.hは XAudio2 を読むので重い。Component からはこのヘッダだけを読む
// 種類の数は magic_enum が数えるので、Count のような「種類ではない値」を足さないこと（Reference.md 18章）

/// <summary>
/// 音の種類（音量のつまみ）
/// </summary>
enum class AudioBus : uint8_t {
	BGM, // 音楽
	SE,  // 効果音
};
```

#### A-2. `GetMousePositionX/Y` → `GetMousePositionInWindow()`

**なぜ**：`GetMousePositionX/Y` はウィンドウ全体の中の位置なので、**エディタでは Hierarchy などのパネルの分だけずれる**（製品版ではたまたまゲーム画面と同じになるので、ゲームが使うと「エディタでだけずれる」バグになる）。`GetMousePositionInGame`（`Vector2`）と対になる名前・形にそろえて、「ゲームは `InGame`、`InWindow` はエディタ・エンジン用」とコメントに書く（GameBasics.md 12章の5）。今これを使っているのは `InputManager` の中だけ。

`MyEngine/Input/InputManager.h`（2つの宣言を1つに）

```cpp
// ===== 前 =====
	/// <summary>マウスX座標 (クライアント座標、ウィンドウ左端が 0)。エディタでは Game ビューの中の位置ではない（GetMousePositionInGame を使う）</summary>
	static long GetMousePositionX();

	/// <summary>マウスY座標 (クライアント座標、ウィンドウ上端が 0)。</summary>
	static long GetMousePositionY();

// ===== 後 =====
	/// <summary>
	/// マウスの位置を、ウィンドウの中のピクセルで（左上が 0,0。右と下が +）。エディタ・エンジン用
	/// <para>エディタでは Hierarchy などのパネルの分だけずれる（Game ビューの中の位置ではない）。ゲームは GetMousePositionInGame を使う</para>
	/// </summary>
	static Vector2 GetMousePositionInWindow();
```

`MyEngine/Input/InputManager.cpp`（2か所）

```cpp
// ===== 前（1か所目）=====
long InputManager::GetMousePositionX() { return GetInstance()->mouse_.GetPositionX(); }
long InputManager::GetMousePositionY() { return GetInstance()->mouse_.GetPositionY(); }

// ===== 後 =====
Vector2 InputManager::GetMousePositionInWindow() {
	const MouseInput& mouse = GetInstance()->mouse_;
	return {static_cast<float>(mouse.GetPositionX()), static_cast<float>(mouse.GetPositionY())};
}
```

```cpp
// ===== 前（2か所目：GetMousePositionInGame の中）=====
	const float u = (static_cast<float>(GetMousePositionX()) - topLeft.x) / size.x;
	const float v = (static_cast<float>(GetMousePositionY()) - topLeft.y) / size.y;

// ===== 後 =====
	const Vector2 mouse = GetMousePositionInWindow();
	const float u = (mouse.x - topLeft.x) / size.x;
	const float v = (mouse.y - topLeft.y) / size.y;
```

#### A-3. Scene ビューにもポストエフェクト（Lens は外す）

**なぜ・どう**（GameBasics.md 12章の6）：

- 今は `EditorViewport::Render` で、Scene ビューだけ効果なし（`RenderToTexture(..., false)`）にしている。ブルームの定数バッファは1フレームに何回でも使える枠になっていて、2つのビューで使っても足りる（検証で12回 Play / Stop しても問題なし）。
- **Lens（歪み・色収差・周辺を暗く）は Scene では掛けない**：絵を歪ませると、その上に ImGui で描くギズモ（ライトのアイコン・Collider の線）と、物の見えている位置がずれる。クリックで選ぶ位置もずれる。
- `bool usePost` だと「ブルームはするがレンズはしない」を言えないので、**`PostEffectMode`（None / All / NoLens）**にした。`bool` のままだと `RenderToTexture(..., true, false)` のような「何の true か分からない」呼び方になる。
- 切り替えは **View メニュー → Scene View → Post Effects**（既定 ON）。Collider の線と同じく、全ウィンドウ共通の static な値。

`MyEngine/Graphics/PostEffect/PostProcess.h`（2か所）

```cpp
// ===== 前（1か所目：include の後）=====
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"


/// <summary>

// ===== 後 =====
#include "MyEngine/Graphics/RenderTarget/RenderTexture.h"

// ポストエフェクトをどこまで掛けるか（SceneRenderer::RenderToTexture に渡す）
enum class PostEffectMode {
	None,   // 掛けない
	All,    // 全部（Game ビュー・製品版）
	NoLens, // レンズ（歪み・色収差・周辺を暗く）だけ外す。Scene ビュー用（絵を歪ませると、ギズモやクリックで選ぶ位置が物とずれる）
};


/// <summary>
```

```cpp
// ===== 前（2か所目）=====
	void Composite(float width, float height);

// ===== 後 =====
	// useLens が false ならレンズを掛けずに通す（Scene ビュー）
	void Composite(float width, float height, bool useLens = true);
```

`MyEngine/Graphics/PostEffect/PostProcess.cpp`

```cpp
// ===== 前 =====
void PostProcess::Composite(float width, float height) {
	// 切っていても最終出力として通す。全部0ならただのコピーになる
	lens_.Render(*source_, width, height, kLensEnabled != 0);
}

// ===== 後 =====
void PostProcess::Composite(float width, float height, bool useLens) {
	// 切っていても最終出力として通す。全部0ならただのコピーになる
	lens_.Render(*source_, width, height, useLens && kLensEnabled != 0);
}
```

`MyEngine/Graphics/Renderer/SceneRenderer.h`

```cpp
// ===== 前 =====
	static void RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, bool usePost = true);

// ===== 後 =====
	static void RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, PostEffectMode mode = PostEffectMode::All);
```

`MyEngine/Graphics/Renderer/SceneRenderer.cpp`（2か所）

```cpp
// ===== 前（1か所目）=====
void SceneRenderer::RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, bool usePost) {
	// ポストエフェクトを使わないときはそのまま描く
	if (!usePost || !instance_->postProcess_) {

// ===== 後 =====
void SceneRenderer::RenderToTexture(const Camera* camera, RenderTexture* target, const std::wstring& windowTitle, PostEffectMode mode) {
	// ポストエフェクトを使わないときはそのまま描く
	if (mode == PostEffectMode::None || !instance_->postProcess_) {
```

```cpp
// ===== 前（2か所目：同じ関数の下の方）=====
	instance_->postProcess_->Composite(width, height);
	RenderUI(windowTitle, width, height); // UIは効果の外側に重ねる
	target->PostDraw();

// ===== 後 =====
	instance_->postProcess_->Composite(width, height, mode != PostEffectMode::NoLens);
	RenderUI(windowTitle, width, height); // UIは効果の外側に重ねる
	target->PostDraw();
```

> `RenderToWindow`（製品版）の `Composite(width, height)` はそのまま（`useLens` の既定は true）。

`MyEngine/Editor/Viewport/EditorViewport.h`（`#ifdef USE_IMGUI` のゲッターの所）

```cpp
// ===== 前 =====
	bool IsSceneHovered() const { return sceneView_.IsHovered(); }
#endif

// ===== 後 =====
	bool IsSceneHovered() const { return sceneView_.IsHovered(); }
	// Scene ビューにもポストエフェクトを掛けるか（全ウィンドウ共通。View メニューで切り替える。レンズは Scene では掛けない）
	static bool& GetShowSceneEffects();
#endif
```

`MyEngine/Editor/Viewport/EditorViewport.cpp`（2か所）

```cpp
// ===== 前（1か所目：Render の中）=====
	// --- 同じキューを2カメラで2回描く ---
	SceneRenderer::RenderToTexture(debugCamera_.get(), sceneRT, windowTitle, false); // Scene：なし
	SceneRenderer::RenderToTexture(gameCamera, gameRT, windowTitle);                 // Game：ブルームあり

// ===== 後 =====
	// --- 同じキューを2カメラで2回描く ---
	// Scene はレンズだけ外す（絵を歪ませると、ギズモ・クリックで選ぶ位置が物とずれる）。View メニューで全部切れる
	const PostEffectMode sceneMode = GetShowSceneEffects() ? PostEffectMode::NoLens : PostEffectMode::None;
	SceneRenderer::RenderToTexture(debugCamera_.get(), sceneRT, windowTitle, sceneMode); // Scene：レンズ以外
	SceneRenderer::RenderToTexture(gameCamera, gameRT, windowTitle);                      // Game：全部
```

```cpp
// ===== 足す（2か所目：Render の関数の後、「調整項目」の大見出しの前）=====
//=============================================================================
// Scene ビューにもポストエフェクトを掛けるか
//=============================================================================
bool& EditorViewport::GetShowSceneEffects() {
	static bool showSceneEffects = true; // 既定は掛ける（Unity の Scene ビューと同じ）
	return showSceneEffects;
}


```

`MyEngine/Editor/ImGuiManager.cpp`（2か所）

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Editor/Profiler.h"

// ===== 後 =====
#include "MyEngine/Editor/Profiler.h"
#include "MyEngine/Editor/Viewport/EditorViewport.h"
```

```cpp
// ===== 前（2か所目：View メニュー）=====
			ImGui::MenuItem("Colliders", nullptr, &CollisionSystem::GetShowGizmos());
			ImGui::EndMenu();

// ===== 後 =====
			ImGui::MenuItem("Colliders", nullptr, &CollisionSystem::GetShowGizmos());
			// Scene ビューの絵（ブルームなど。レンズは Scene では掛けない）
			ImGui::SeparatorText("Scene View");
			ImGui::MenuItem("Post Effects", nullptr, &EditorViewport::GetShowSceneEffects());
			ImGui::EndMenu();
```

---

### パート B：SF1 シーン＝ファイル（エンジン）

**仕組みの全体**（読まなくても写せるが、知っておくと直せる）

```
Engine::Initialize(wc, "GameScene")
  └ 作り方 [ ] { return SceneManager::CreateScene("GameScene"); } を SceneManager に渡す
        CreateScene(name)：REGISTER_SCENE した名前 → そのクラス（GameScene）
                           していない名前       → DefaultScene（Entity だけで動く）
                           どちらでも SetSceneName(name) → ファイルは resources/scenes/<name>.scene.json

SceneManager（ウィンドウごとに1つ）
  openSceneFactory_ … 開いているシーンの作り方。Stop・Restart はこれで作り直す（Play の瞬間の退避から）
  nextSceneName_    … LoadScene で頼まれた名前。次のフレームの頭で CreateScene(name) に切り替える

SceneManager::LoadScene("Result")（static。どこからでも）
  └ 頼む相手：今 Update しているシーンマネージャ（updating_）、Update の外なら最初のウィンドウの（main_）
     停止中 → 何もしない ／ シーンが無い → 警告して false ／ ある → nextSceneName_ に入れて true
```

- **`REGISTER_SCENE` の中身**：`COMPONENT` と同じく「static 変数を1つ作り、その変数が作られるとき（main より前）に表へ登録する」。表は `SceneManager.cpp` の関数の中の static（`SceneTable()`）にしてある。**ふつうの static 変数だと、ゲームの cpp の登録の方が先に走って、表がまだ作られていない**ことがある（static 変数が作られる順番は、ファイルをまたぐと決まっていない）。関数の中の static は「初めて呼ばれたときに作られる」ので、登録がいつ走っても大丈夫。
- **停止中の `LoadScene` を無視する理由**：停止中はゲームのコード（Update・SYSTEM）は動かないので、ふつうは来ない。来るのは「シーンの `Initialize` で `LoadScene` を呼んだ」とき（例：起動用の Boot シーンがすぐ Title へ進む）。これを効かせると、**エディタで Boot シーンを開いた瞬間に Title へ替わってしまい、Boot を編集できない**。再生中（Play を押した後の作り直しの `Initialize` も再生中）なら効く。エディタからシーンを開くのは SF2 で別の関数にする。
- **開いているシーンの作り方を覚える理由**：前は `sceneFactory_`（最初のシーンの作り方）で Stop のときに作り直していた。SF2 で「別のシーンを開く」ができると、戻る先が変わるので、「今開いているシーンの作り方」として持つ。
- **`LoadScene` と `Stop` を同じフレームで頼まれたら**：`ReloadImmediate` の頭で `nextSceneName_` も捨てる（前に `NextScene()` で直したのと同じ罠。Serialize.md の2回目のレビュー）。

#### B-1. `MyEngine/Scene/IScene.h`（全体を差し替え）

変わった所：シーンファイルの場所の決まり（`kSceneFolder`・`kSceneFileExtension`・`MakeSceneFilePath`）、`GetSceneFile()` の既定が「名前から決めた場所」、`GetSceneName`・`SetSceneName`、`REGISTER_SCENE` と `SceneRegistry`。

```cpp
#pragma once
#include <string>
#include <memory>
#include <functional>

#include "MyEngine/Systems/CameraSystem.h"

// 前方宣言
class Camera;

// シーンファイルの置き場所と、名前の後ろに付く部分（"resources/scenes/Title.scene.json" のシーンの名前は "Title"）
inline constexpr const char* kSceneFolder = "resources/scenes/";
inline constexpr const char* kSceneFileExtension = ".scene.json";
// 名前からシーンファイルの場所を作る（"Title" → "resources/scenes/Title.scene.json"）
inline std::string MakeSceneFilePath(const std::string& name) { return std::string(kSceneFolder) + name + kSceneFileExtension; }


/// <summary>
/// シーン基底クラス
/// <para>シーンファイルを使うシーン（GetSceneFile が nullptr 以外）では、Entityはエンジンが作って消す。
/// 呼ばれる順番は「前のシーンの Finalize → 全Entityを消す → Entityを作る（Playの退避 / シーンファイル / CreateDefaultEntities）→ Initialize」</para>
/// <para>名前で作ったシーン（Engine::Initialize・SceneManager::LoadScene に名前を渡す）は、名前からシーンファイルが決まる</para>
/// </summary>
class IScene {
public:
	virtual ~IScene() = default;
	// Entityがそろった後に呼ばれる。カメラ・IBLなど、Entityではない物を作る
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	// Initializeで作った物（Entityではない物）を片付ける。Entityはエンジンが消すので、ここでは消さなくてよい
	virtual void Finalize() = 0;
	// ゲームの絵を撮るカメラ。ふつうは上書きしない（Hierarchy に置いた Camera Component のカメラになる。CameraSystem）
	virtual Camera* GetCamera() { return CameraSystem::GetMainCamera(); }
	// 前の書き方の切り替え（次のシーンを返す）。新しく書くときは SceneManager::LoadScene("名前")
	virtual std::unique_ptr<IScene> NextScene() { return nullptr; }

	// ===== シーンファイル =====
	// Entityを保存するファイル。名前で作ったシーンは「resources/scenes/<名前>.scene.json」になるので、上書きしなくてよい
	// nullptr ならファイルを使わない（今まで通り、Initializeで作ってFinalizeで消す）
	virtual const char* GetSceneFile() const { return sceneFile_.empty() ? nullptr : sceneFile_.c_str(); }
	// シーンファイルがまだ無いときだけ、Initialize の前に呼ばれる。最初の配置をコードで作る
	// （Saveするとファイルができるので、次からは呼ばれない。ファイルを消すと、またこの配置から始まる）
	// ここで作ったEntityのHandleをメンバに覚えない（Play・Stop・次の起動ではこの関数が呼ばれず、Handleも変わる）。
	// 覚えたいときは Initialize の中で EntityManager::FindByName などで探す
	virtual void CreateDefaultEntities() {}

	// ===== 名前 =====
	// シーンの名前（シーンファイルの名前。"Title" など）。名前の無い前の書き方のシーンは、シーンファイルの名前になる（ファイルも無ければ空）
	const std::string& GetSceneName() const { return sceneName_; }

	// ===== エンジンが自動でセットする =====
	// WindowManager がセットする
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }
	const std::wstring& GetWindowTitle() const { return windowTitle_; }
	// SceneManager が作った直後にセットする。シーンファイルの場所も名前から決まる
	void SetSceneName(const std::string& name) {
		sceneName_ = name;
		sceneFile_ = MakeSceneFilePath(name);
	}

protected:
	std::wstring windowTitle_ = L"Title";

private:
	std::string sceneName_; // シーンの名前（空なら名前が無い）
	std::string sceneFile_; // 名前から決めたシーンファイルの場所（GetSceneFile を上書きしたシーンでは使われない）
};

// シーンの作り方。Stop / Restart で作り直すときにも使う
using SceneFactory = std::function<std::unique_ptr<IScene>()>;


//=============================================================================
// シーンのクラスを名前で登録する（REGISTER_SCENE）
//=============================================================================
namespace SceneRegistry {
// 名前と作り方を登録する（REGISTER_SCENE が main より前に呼ぶ）
void Register(const char* name, SceneFactory factory);
// 名前から作り方を探す（登録していなければ nullptr）
SceneFactory Find(const std::string& name);
} // namespace SceneRegistry

/// <summary>
/// REGISTER_SCENE(...) が1つ作る物。作られた時点で、名前と作り方が表に入る
/// </summary>
template<class T> class SceneRegistrar {
public:
	explicit SceneRegistrar(const char* name) {
		SceneRegistry::Register(name, [] { return std::make_unique<T>(); });
	}
};

/// <summary>
/// シーンのクラスを登録する。REGISTER_SCENE(GameScene) と書くと、"GameScene" のシーン（resources/scenes/GameScene.scene.json）を開くときにこのクラスが使われる
/// <para>登録していない名前のシーンは、エンジンの DefaultScene（Entity だけで動くシーン）で開く</para>
/// </summary>
#define REGISTER_SCENE(Type) inline const SceneRegistrar<Type> sceneRegistrarOf_##Type(#Type)
```

#### B-2. `MyEngine/Scene/SceneManager.h`（全体を差し替え）

変わった所：`Initialize(SceneFactory)`（前は `SetSceneFactory` → `Initialize()` の2回）、`LoadScene`・`HasScene`・`CreateScene`・`GetOpenSceneName`、`SetScene`・`SetSceneFactory` を消した（どこからも使っていない／`Initialize` に入れた）、メンバの `nextSceneName_`・`openSceneFactory_`・`openSceneName_`・`updating_`・`main_`。

```cpp
#pragma once
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Scene/IScene.h"
#include <functional>
#include <memory>
#include <string>

// 実行状態
enum class PlayState {
	Editing, // 停止中。Updateを回さない
	Playing, // 再生中
	Paused,  // 一時停止
};

/// <summary>
/// シーン一括管理クラス
/// <para>Entityを作り直す・シーンを切り替える・保存するのは、全部 Update の頭（フレームの境目）で行う</para>
/// <para>「開いているシーン」＝エディタで編集しているシーン。Play はここから始まり、Stop でここ（Playを押した瞬間の状態）へ戻る</para>
/// </summary>
class SceneManager {
public:
	// 最初のシーンを作る。openScene は「開いているシーン」の作り方（Stop・Restart で作り直すときにも使う）
	void Initialize(SceneFactory openScene);
	void Update();
	void Draw();
	void Finalize(); // アプリの終了時に1回。今のシーンの Finalize を呼ぶ

	// ===== シーンの切り替え（ゲームのコードから。どこからでも呼べる）=====
	// 次のフレームの頭で、name のシーン（resources/scenes/<name>.scene.json）へ切り替える。切り替わるのは今 Update しているウィンドウ
	// 再生中だけ効く（停止中は何もしない）。シーンが無ければ Log に警告して false（切り替えない）
	static bool LoadScene(const std::string& name);
	// name のシーンがあるか（シーンファイルがある、または REGISTER_SCENE で登録した名前）
	static bool HasScene(const std::string& name);
	// name のシーンを作る（REGISTER_SCENE した名前ならそのクラス、無ければ DefaultScene）。Entity はまだ作らない
	static std::unique_ptr<IScene> CreateScene(const std::string& name);

	// ===== 再生コントロール =====
	void Play();  // 停止中なら、今のEntityを退避してから再生
	void Pause(); // 一時停止 / 再開
	void Stop();  // 停止して、開いているシーンの「Playを押した瞬間の状態」へ戻す
	// 作り直す。再生中はPlayを押した瞬間の状態から、停止中はシーンファイルから（保存していない編集は消える）
	void RequestReload() { isReloadRequested_ = true; }
	// シーンファイルに保存する（停止中だけ。実際に書くのは次のフレームの頭）
	void RequestSave() { isSaveRequested_ = true; }

	// ===== ゲッター =====
	IScene* GetCurrentScene() { return currentScene_.get(); }
	PlayState GetPlayState() const { return playState_; }
	const char* GetSceneFile() const { return currentScene_ ? currentScene_->GetSceneFile() : nullptr; } // シーンファイルを使わないならnullptr
	// 開いているシーンの名前（Playを押したシーン。再生中に切り替わっても、これは変わらない）
	const std::string& GetOpenSceneName() const { return openSceneName_; }

	// ===== セッター =====
	void RequestNextScene(std::unique_ptr<IScene> next) { nextScene_ = std::move(next); } // 次のフレームの頭で切り替える
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }

private:
	// 開いているシーンを作り直す（Play・Stop・Restart）
	void ReloadImmediate();
	// シーンを入れ替える。useSnapshot なら、Playを押した瞬間に退避したEntityを使う
	void ChangeScene(std::unique_ptr<IScene> next, bool useSnapshot);
	// 新しいシーンのEntityを作る（退避 → シーンファイル → CreateDefaultEntities の順に探す）
	void LoadEntities(bool useSnapshot);
	// シーンファイルに書く
	void SaveImmediate();

	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> nextScene_; // 次のフレームの頭で切り替えるシーン（NextScene() が返した物）
	std::string nextSceneName_;         // 次のフレームの頭で切り替えるシーンの名前（LoadScene で頼まれた物）
	SceneFactory openSceneFactory_;     // 開いているシーンの作り方（Stop・Restart で作り直す）
	std::string openSceneName_;         // 開いているシーンの名前
	std::wstring windowTitle_;
#ifdef USE_IMGUI
	PlayState playState_ = PlayState::Editing; // エディタでは停止状態から始める
#else
	PlayState playState_ = PlayState::Playing; // Release は即座に動かす
#endif
	bool isReloadRequested_ = false;
	bool isSnapshotRequested_ = false; // 次に作り直す前に、今のEntityを退避する（Playを押した）
	bool isSaveRequested_ = false;
	std::string playSnapshot_; // Playを押した瞬間の全Entity（シーンファイルと同じJSON）。空なら退避なし

	// ===== LoadScene の行き先（static 関数からは this が無いので、ここで覚える）=====
	static SceneManager* updating_; // 今 Update しているシーンマネージャ（Update の中だけ入っている）
	static SceneManager* main_;     // 最初に作ったシーンマネージャ（Update の外で LoadScene を呼ばれたときの行き先）
};
```

#### B-3. `MyEngine/Scene/SceneManager.cpp`（全体を差し替え）

変わった所：名前 → 作り方 の表（`SceneTable`・`SceneRegistry`）、`Initialize`、`Update`（`updating_`・`nextSceneName_`・停止中の早い return をやめて if に）、`Finalize`（`main_`）、「シーンの切り替え」の3関数（新しい区画）、`ReloadImmediate`（`nextSceneName_.clear()`・`openSceneFactory_`）、`ChangeScene`（名前の無いシーンに名前を付ける）、`LoadEntities` のログ（Save するとファイルができます）。Play / Pause / Stop / 保存は前と同じ。

```cpp
#include "SceneManager.h"

#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <string_view>
#include <utility>

#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Scene/DefaultScene.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Time/Time.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#endif

// 静的メンバ変数
SceneManager* SceneManager::updating_ = nullptr;
SceneManager* SceneManager::main_ = nullptr;

namespace {
// 名前 → 作り方 の表
// REGISTER_SCENE は main より前（static 変数が作られるとき）に呼ばれる。表をふつうの static 変数にすると、
// 表の方がまだ作られていないことがあるので、関数の中の static にする（初めて呼ばれたときに必ず作られる）
std::map<std::string, SceneFactory>& SceneTable() {
	static std::map<std::string, SceneFactory> table;
	return table;
}

// シーンファイルの場所から名前を作る（"resources/scenes/Title.scene.json" → "Title"）
std::string SceneNameFromFile(std::string_view file) {
	const size_t slash = file.find_last_of("/\\");
	std::string_view name = (slash == std::string_view::npos) ? file : file.substr(slash + 1);
	const std::string_view extension = kSceneFileExtension;
	if (name.ends_with(extension)) {
		name.remove_suffix(extension.size());
	}
	return std::string(name);
}
} // namespace


//======================================================================================================
// シーンの登録（REGISTER_SCENE）
//======================================================================================================
void SceneRegistry::Register(const char* name, SceneFactory factory) { SceneTable()[name] = std::move(factory); }

SceneFactory SceneRegistry::Find(const std::string& name) {
	const auto it = SceneTable().find(name);
	return (it != SceneTable().end()) ? it->second : nullptr;
}


//======================================================================================================
// 初期化・更新・描画・終了
//======================================================================================================
void SceneManager::Initialize(SceneFactory openScene) {
	MY_ASSERT_MSG(openScene != nullptr, "最初のシーンが渡されていません（Engine::Initialize の2つ目の引数）");
	// Update の外で LoadScene を呼ばれたときは、最初のウィンドウのシーンを切り替える
	if (main_ == nullptr) {
		main_ = this;
	}
	openSceneFactory_ = std::move(openScene);
	ChangeScene(openSceneFactory_(), false);
	openSceneName_ = currentScene_->GetSceneName();
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");
	// この Update の間に呼ばれた LoadScene（シーンの Update・SYSTEM）は、このウィンドウのシーンを切り替える
	updating_ = this;

	// ===== フレームの頭：Entityを作り直す・保存するのはここだけ =====
	// 前のフレームの予約（Componentの追加・Undoなど）は反映済みで、まだ誰も ForEach で回していない
	if (isReloadRequested_) {
		isReloadRequested_ = false;
		ReloadImmediate();
	}
	if (nextScene_) {
		ChangeScene(std::move(nextScene_), false); // 前のフレームで NextScene() が返したシーン
	}
	if (!nextSceneName_.empty()) {
		// 前のフレームで LoadScene に頼まれたシーン（頼まれた名前は空に戻す）
		ChangeScene(CreateScene(std::exchange(nextSceneName_, std::string())), false);
	}
	// 保存は作り直しの後（Stop と Save を同じフレームで頼まれても、戻した後の状態を書く）
	if (isSaveRequested_) {
		isSaveRequested_ = false;
		SaveImmediate();
	}

	// 停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ != PlayState::Editing) {
		// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
		Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
		currentScene_->Update();
		GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）

		// シーン遷移は予約だけ（Entityの入れ替えは、次のフレームの頭で行う）
		if (std::unique_ptr<IScene> next = currentScene_->NextScene()) {
			nextScene_ = std::move(next);
		}
	}
	updating_ = nullptr;
}

void SceneManager::Draw() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、描画できませんでした");
	currentScene_->Draw();
}

void SceneManager::Finalize() {
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset(); // 2回 Finalize しないように、ここで捨てる
	}
	// 閉じたウィンドウへ LoadScene を送らないように
	if (main_ == this) {
		main_ = nullptr;
	}
}

//======================================================================================================
// シーンの切り替え（ゲームのコードから）
//======================================================================================================
bool SceneManager::LoadScene(const std::string& name) {
	SceneManager* target = (updating_ != nullptr) ? updating_ : main_;
	if (target == nullptr) {
		LogManager::Warning(std::format("LoadScene(\"{}\")：ウィンドウがまだ無いので切り替えられません", name));
		return false;
	}
	// 停止中はゲームのコードが動かないので、ふつうは来ない（来るのはシーンの Initialize から呼んだとき）
	// エディタで開いているシーンを、ゲームのコードが勝手に替えないようにする
	if (target->playState_ == PlayState::Editing) {
		LogManager::Log(std::format("LoadScene(\"{}\")：停止中なので切り替えません（Play 中だけ効きます）", name));
		return false;
	}
	if (!HasScene(name)) {
		LogManager::Warning(std::format("LoadScene(\"{}\")：シーンがありません（{} も REGISTER_SCENE({}) もありません）", name, MakeSceneFilePath(name), name));
		return false;
	}
	target->nextSceneName_ = name; // 同じフレームに何回も呼ばれたら、最後に頼まれたシーンへ
	return true;
}

bool SceneManager::HasScene(const std::string& name) {
	if (name.empty()) {
		return false;
	}
	if (SceneRegistry::Find(name) != nullptr) {
		return true; // クラスがあれば、ファイルが無くても CreateDefaultEntities から作れる
	}
	std::error_code error; // 調べられなかったときも例外を出さず「無い」にする
	return std::filesystem::exists(MakeSceneFilePath(name), error);
}

std::unique_ptr<IScene> SceneManager::CreateScene(const std::string& name) {
	// REGISTER_SCENE したクラスがあればそれ、無ければ Entity だけで動くエンジンのシーン
	const SceneFactory factory = SceneRegistry::Find(name);
	std::unique_ptr<IScene> scene = factory ? factory() : std::make_unique<DefaultScene>();
	scene->SetSceneName(name);
	return scene;
}

//======================================================================================================
// 再生コントロール
//======================================================================================================
void SceneManager::Play() {
	if (playState_ == PlayState::Editing) {
		isSnapshotRequested_ = true; // 作り直す直前に、編集した状態を退避する（Stopでここへ戻す）
		RequestReload();             // 退避した物から作り直して、最初から再生する
	}
	playState_ = PlayState::Playing;
	AudioSystem::SetPaused(false); // 一時停止中に Play を呼ばれても、音が止まったままにならないように
}

void SceneManager::Pause() {
	if (playState_ == PlayState::Playing) {
		playState_ = PlayState::Paused;
	} else if (playState_ == PlayState::Paused) {
		playState_ = PlayState::Playing;
	}
	AudioSystem::SetPaused(playState_ == PlayState::Paused); // 時間と一緒に音も止める / 続きから鳴らす
}

void SceneManager::Stop() {
	if (playState_ == PlayState::Editing) {
		return; // 停止中に押しても作り直さない（保存していない編集を消さないように）
	}
	playState_ = PlayState::Editing;
	// 音はすぐに止める（作り直しは次のフレームの頭。一時停止を先に解くと、止めるまでの1フレームだけ鳴ってしまう）
	AudioSystem::StopAll();
	AudioSystem::SetPaused(false);
	RequestReload(); // Playを押した瞬間の状態へ戻す
}

//======================================================================================================
// 作り直す・入れ替える（フレームの頭でだけ呼ぶ）
//======================================================================================================
void SceneManager::ReloadImmediate() {
	// 鳴っている音を全部止める（シーンの Initialize が鳴らした BGM が、Play / Stop / Restart のたびに重なっていかないように）
	AudioSystem::StopAll();
	// Playを押した直後なら、作り直す前に今のEntityを退避する（フレームの頭なので、見えている通りに写せる）
	if (isSnapshotRequested_) {
		isSnapshotRequested_ = false;
		if (GetSceneFile() != nullptr) {
			playSnapshot_ = SceneSerializer::SaveToText();
		}
	}
	// 再生中に頼まれたシーン遷移は捨てる（開いているシーンに戻るので、その遷移はもう関係ない）
	// これが残っていると、Stopを押したフレームにそのまま次のシーンへ飛んでしまう
	nextScene_.reset();
	nextSceneName_.clear();
	// Stop・Restartで開いているシーンに戻る（タイトル → ゲームと切り替わった後でも、Playを押したシーンへ戻る）
	ChangeScene(openSceneFactory_(), true);
	// 停止に戻ったら退避はもう要らない（次のPlayで取り直す）
	if (playState_ == PlayState::Editing) {
		playSnapshot_.clear();
	}
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> next, bool useSnapshot) {
#ifdef USE_IMGUI
	// 履歴が覚えている相手（EntityId）が作り直されるので、履歴は捨てる
	EditorHistory::Clear();
	// 選んでいたEntityを番号で覚えておく（作り直すとHandleは変わるが、番号は同じ）
	const Entity* selected = EntityManager::Get(HierarchyWindow::GetSelected());
	const EntityId selectedId = selected ? selected->id : 0;
#endif

	// ===== 1. 古いシーンの片付け（カメラ・IBLなど、Entityではない物）=====
	const bool oldUsesFile = currentScene_ && currentScene_->GetSceneFile() != nullptr;
	if (currentScene_) {
		currentScene_->Finalize();
	}
	currentScene_ = std::move(next);
	currentScene_->SetWindowTitle(windowTitle_);
	// 名前の無い前の書き方のシーン（GetSceneFile を上書きしたシーン）は、シーンファイルの名前を名前にする（Control ウィンドウに出す）
	if (currentScene_->GetSceneName().empty() && currentScene_->GetSceneFile() != nullptr) {
		currentScene_->SetSceneName(SceneNameFromFile(currentScene_->GetSceneFile()));
	}

	// ===== 2. Entityを入れ替える =====
	// シーンファイルを使うシーンは、全Entityを自分の物として扱う（出るときも入るときも全部消す）
	// 使わないシーンは今まで通り、自分で作って自分で消す
	const bool newUsesFile = currentScene_->GetSceneFile() != nullptr;
	if (oldUsesFile || newUsesFile) {
		SceneSerializer::DestroyAllNow();
	}
	if (newUsesFile) {
		LoadEntities(useSnapshot);
	}
	EntityManager::FlushComponentChanges(); // CreateDefaultEntities で予約した分もここで付く＝Initialize の時点でそろっている

	// ===== 3. 新しいシーンの準備（Entityがそろった後）=====
	const size_t entityCount = EntityManager::GetCount();
	currentScene_->Initialize();
	// Initialize で Entity を作ると、ファイル・退避から作った物に毎回足されて、Play / Stop のたびに増えていく
	if (newUsesFile && EntityManager::GetCount() != entityCount) {
		LogManager::Warning("シーンファイルを使うシーンの Initialize で Entity を作っています。Play / Stop のたびに増えるので、CreateDefaultEntities へ移してください");
	}

#ifdef USE_IMGUI
	HierarchyWindow::SetSelected(EntityManager::FindById(selectedId)); // 同じ番号のEntityを選び直す（居なければ選択なし）
#endif
}

void SceneManager::LoadEntities(bool useSnapshot) {
	// --- Playを押した瞬間の状態に戻す（Stop・Restart）---
	if (useSnapshot && !playSnapshot_.empty()) {
		SceneSerializer::LoadFromText(playSnapshot_);
		return;
	}
	// --- シーンファイルから作る（起動・シーン切り替え）---
	const char* file = currentScene_->GetSceneFile();
	switch (SceneSerializer::LoadFile(file)) {
	case SceneLoadResult::Loaded:
		// ファイルがある間は、CreateDefaultEntities を直しても反映されない（気づけるようにログに出す）
		LogManager::Log(std::format("CreateDefaultEntities() は呼んでいません（{} を消すと、また呼ばれます）", file));
		break;
	case SceneLoadResult::NotFound:
#ifdef USE_IMGUI
		LogManager::Log(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります（Save するとファイルができます）: {}", file));
#else
		// 製品版でファイルが無いのは、入れ忘れの可能性が高い
		LogManager::Warning(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります（resources に入れ忘れていませんか）: {}", file));
#endif
		currentScene_->CreateDefaultEntities();
		break;
	case SceneLoadResult::Failed:
		// 空のシーンで始まる。読めなかったファイルは「〇〇.broken」に写してあるので、Saveで上書きしても残る
		LogManager::Error(std::format("シーンファイルを読めなかったので、空のシーンで始めます（元のファイルは .broken に残しました）: {}", file));
		break;
	}
}

//======================================================================================================
// 保存
//======================================================================================================
void SceneManager::SaveImmediate() {
	const char* file = GetSceneFile();
	if (file == nullptr) {
		LogManager::Warning("このシーンはシーンファイルを使っていないので保存できません（IScene::GetSceneFile）");
		return;
	}
	// 再生中に保存すると、動いた後の状態がファイルに残ってしまう（Unityも再生中は保存できない）
	// 退避を持っている間＝まだ Play 中の Entity のまま（Stop は次の作り直しで戻る）なので、それも断る
	if (playState_ != PlayState::Editing || !playSnapshot_.empty()) {
		LogManager::Warning("再生中は保存できません。Stopしてから保存してください");
		return;
	}
	SceneSerializer::SaveFile(file);
}
```

#### B-4. `MyEngine/Scene/DefaultScene.h`・`DefaultScene.cpp`（新規）

ソリューションエクスプローラーで `MyEngine/Scene` フィルタに2つ追加する。

- シーンのクラスを書いていないシーン（`REGISTER_SCENE` していない名前）は、これで開く。**Entity だけで動く**（毎フレームの処理は SYSTEM、描画は `ModelRenderSystem`）。
- **IBL をエンジンの既定の空（天球と同じ `skybox.hdr`）で作る**ので、PBR のモデルも描かれる（IBL が無いと PBR のモデルは描かれない）。シーンごとに空を変えるのは SF4。
- ファイルが無い（新しいシーン）ときは、カメラと平行光源を置く（Unity の新しいシーンと同じ）。

```cpp
#pragma once
#include "MyEngine/Graphics/IBL/IBLEnvironment.h"
#include "MyEngine/Scene/IScene.h"


/// <summary>
/// シーンのクラスを書いていないシーン（REGISTER_SCENE していない名前のシーン）で使う、エンジンのシーン
/// <para>Entity だけで動く。毎フレームの処理は SYSTEM(...)、描画は ModelRenderSystem がする</para>
/// <para>IBL（PBR のモデルの環境光）は、天球と同じエンジンの既定の画像から作る</para>
/// </summary>
class DefaultScene final : public IScene {
public:
	void Initialize() override;
	void Update() override {}
	void Draw() override;
	void Finalize() override {}
	// シーンファイルがまだ無い（新しいシーン）ときの最初の配置：カメラと平行光源（Unity の新しいシーンと同じ）
	void CreateDefaultEntities() override;

private:
	IBLEnvironment environment_; // 環境光。PBR のモデルはこれが無いと描かれない
};
```

```cpp
#include "DefaultScene.h"

#include <numbers>

#include "MyEngine/Components/CameraComponent.h"
#include "MyEngine/Components/LightComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Scene/Skybox.h"
#include "MyEngine/Systems/ModelRenderSystem.h"

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace


//=============================================================================
// 初期化（Entity がそろった後に呼ばれる）
//=============================================================================
void DefaultScene::Initialize() {
	// 環境光（IBL）。天球と同じ既定の画像から作る（シーンごとに空を変えるのは SceneFlow.md の SF4）
	environment_.MakeFromHDR(TextureManager::Load(Skybox::kDefaultSkyPath));
}


//=============================================================================
// 描画
//=============================================================================
void DefaultScene::Draw() {
	// 全部の Entity のモデルを描く（カメラは Hierarchy の Camera Component のカメラ）
	ModelRenderSystem::Draw({}, GetCamera(), &environment_, GetWindowTitle());
}


//=============================================================================
// 新しいシーンの最初の配置（シーンファイルがまだ無いときだけ呼ばれる）
//=============================================================================
void DefaultScene::CreateDefaultEntities() {
	// カメラ（Unity の新しいシーンと同じ位置。見た目は Inspector で直して Save する）
	const Handle<Entity> camera = EntityManager::Create("Main Camera");
	EntityManager::Get<TransformComponent>(camera)->translation = {0.0f, 1.0f, -10.0f};
	EntityManager::RequestAdd<CameraComponent>(camera);

	// 平行光源（+Z の向きに照らす。向きは GameScene の最初の配置と同じ）
	const Handle<Entity> light = EntityManager::Create("Directional Light");
	TransformComponent* lightTransform = EntityManager::Get<TransformComponent>(light);
	lightTransform->translation = {0.0f, 3.0f, 0.0f};
	lightTransform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f};
	EntityManager::RequestAdd<DirectionalLightComponent>(light);
}
```

#### B-5. `MyEngine/Scene/Skybox.h`・`Skybox.cpp`（既定の空の場所を外から読めるように）

`Skybox.h`（`CalcCubeSize` の後に足す）

```cpp
// ===== 前 =====
	static uint32_t CalcCubeSize(float equirectWidth);

	// ===== セッター =====

// ===== 後 =====
	static uint32_t CalcCubeSize(float equirectWidth);

	// 既定の空の画像（起動したときの天球。DefaultScene の IBL もこれで作る）
	static constexpr const char* kDefaultSkyPath = "MyEngine/Resources/Textures/skybox.hdr";

	// ===== セッター =====
```

`Skybox.cpp`（名前の無い namespace の1行を消す。`LoadDefaultTexture` の `kDefaultSkyPath` は、そのまま `Skybox::kDefaultSkyPath` を指すようになる）

```cpp
// ===== 前 =====
const std::string psName = "SkyboxPS";
const std::string kDefaultSkyPath = "MyEngine/Resources/Textures/skybox.hdr";
}

// ===== 後 =====
const std::string psName = "SkyboxPS";
}
```

#### B-6. `MyEngine/Engine.h`・`Engine.cpp`（最初のシーンを名前で）

`Engine.h`（今の `Initialize` の宣言の後に足す）

```cpp
// ===== 前 =====
	static void Initialize(const WindowConfig& config, SceneFactory initialScene);

	/// <summary>
	/// メッセージ処理。falseが返ったらループを抜ける。

// ===== 後 =====
	static void Initialize(const WindowConfig& config, SceneFactory initialScene);

	/// <summary>
	/// エンジンの初期化とウィンドウ生成。最初のシーンを名前で渡す（resources/scenes/<名前>.scene.json）
	/// <para>REGISTER_SCENE(名前) したクラスがあればそれ、無ければ DefaultScene（Entity だけで動くシーン）で開く</para>
	/// </summary>
	/// <param name="config">ウィンドウ設定(サイズ・タイトル・ImGui設定・アスペクト比など)</param>
	/// <param name="firstScene">最初のシーンの名前（例: "Title"）</param>
	static void Initialize(const WindowConfig& config, const std::string& firstScene);

	/// <summary>
	/// メッセージ処理。falseが返ったらループを抜ける。
```

`Engine.cpp`（今の `Initialize` の関数の後、「ウィンドウが受け取るメッセージ」の前に足す）

```cpp
// ===== 足す =====
//=============================================================================
// 初期化（最初のシーンを名前で）
//=============================================================================
void Engine::Initialize(const WindowConfig& config, const std::string& firstScene) {
	// 名前からシーンを作る「作り方」を渡す（Stop・Restart でも、この作り方で開いているシーンを作り直す）
	Initialize(config, [firstScene] { return SceneManager::CreateScene(firstScene); });
}


```

> `Engine::Initialize(wc, "Title")` と `Engine::Initialize(wc, [] { ... })` は、引数の形で呼び分けられる（文字列は `std::string` に、ラムダは `SceneFactory` にしかなれない）。前の書き方のゲームもそのまま動く。

#### B-7. `MyEngine/Window/WindowManager.cpp`（2か所）

```cpp
// ===== 前（1か所目：AddWindow の中）=====
	sceneManager->SetWindowTitle(config.title);
	sceneManager->SetSceneFactory(std::move(sceneFactory));
	sceneManager->Initialize();

// ===== 後 =====
	sceneManager->SetWindowTitle(config.title);
	sceneManager->Initialize(std::move(sceneFactory));
```

```cpp
// ===== 前（2か所目：DrawPlayToolbar の最後）=====
	// どのファイルに保存されるか
	ImGui::TextDisabled("Scene: %s", sceneFile ? sceneFile : "(シーンファイルを使っていない)");
	ImGui::End();

// ===== 後 =====
	// --- 開いているシーン（再生中に LoadScene で別のシーンへ進んだら、今のシーンも出す）---
	const std::string& openName = sceneManager->GetOpenSceneName();
	const IScene* scene = sceneManager->GetCurrentScene();
	const std::string currentName = scene ? scene->GetSceneName() : std::string();
	if (currentName == openName) {
		ImGui::TextDisabled("Scene: %s  (%s)", openName.c_str(), sceneFile ? sceneFile : "シーンファイルを使っていない"); // どのファイルに保存されるか
	} else {
		ImGui::TextDisabled("Scene: %s → 今は %s（Stop で %s に戻ります）", openName.c_str(), currentName.c_str(), openName.c_str());
	}
	ImGui::End();
```

#### B-8. `MyEngine/ECS/GameComponent.h`（2か所。ゲームの SYSTEM から `LoadScene` を呼べるように）

「ゲーム側はこのヘッダ1つだけをインクルードする」決まりなので、`SceneManager.h` もここで読む。

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Systems/AudioSystem.h"

// ===== 後 =====
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/Systems/AudioSystem.h"
```

```cpp
// ===== 前（2か所目：最初の説明のコメントの最後）=====
//   カメラは Entity に付ける Camera Component。追いかけるなら③でそのEntityの Transform を動かす（CameraSystem::GetMainCameraEntity）

// ===== 後 =====
//   カメラは Entity に付ける Camera Component。追いかけるなら③でそのEntityの Transform を動かす（CameraSystem::GetMainCameraEntity）
//   シーンを替えるときは、③の中で SceneManager::LoadScene("Result") を呼ぶ（切り替わるのは次のフレームの頭）
```

---

### パート C：ゲーム側（`CG3_Project`）

#### C-1. `AudioKeyTest` を外す（音の確認は終わった）

シーンファイルの MonsterBall に `AudioKeyTest` が付いているので、**先に Inspector で外して Save** してから、ファイルを消す（逆にすると、読み込むときに「知らないComponent "AudioKeyTest" を飛ばしました」が1回出る。出ても、そのまま Save すれば消える）。

1. エディタで MonsterBall を選ぶ → Inspector の **AudioKeyTest の見出しを右クリック → Remove Component** → **Save**。
2. ソリューションエクスプローラーで `Source/Components/AudioKeyTest.cpp` を右クリック → 削除（ファイルも消す）。

#### C-2. `GameScene.h`・`GameScene.cpp`（残り物を消す・名前で登録する）

- `REGISTER_SCENE(GameScene);` で、`"GameScene"` のシーンを開くときにこのクラスを使うようにする。
- `GetSceneFile()` の上書きを消す（名前 `"GameScene"` から同じ `resources/scenes/GameScene.scene.json` に決まる）。
  - **C-3（main.cpp を名前にする）と一緒にやること**。前の書き方（`std::make_unique<GameScene>()`）のまま上書きだけ消すと、名前が無いのでシーンファイルを使わないシーンになり、Entity が読まれない。
- 使っていない物（`lightDirection_`・`lightIntensity_`・`kPointLightNum`・空の `RegisterGV` / `ApplyGV`）を消す。
- **`GlobalVariables::GetInstance()->LoadFiles()` を消す**（6.2 の「見つけたこと」：Play のたびに、保存していない PostEffect・音量などがファイルの値に戻っていた）。パーティクルの値は、起動のときに `Engine::Initialize` の最後で読まれる。

`GameScene.h`（2か所）

```cpp
// ===== 前（1か所目）=====
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

// ===== 後 =====
/// <summary>
/// ゲームシーン（GameScene.cpp の REGISTER_SCENE で "GameScene" のシーンに登録してある）
/// </summary>
class GameScene : public IScene {
public:
	~GameScene() override;
```

```cpp
// ===== 前（2か所目）=====
	// GetCamera は上書きしない（Hierarchy に置いた Camera Component のカメラが使われる）

	// シーンファイル（Entityはここに保存され、次からはここから作られる）
	const char* GetSceneFile() const override { return "resources/scenes/GameScene.scene.json"; }
	// シーンファイルがまだ無いときだけ呼ばれる最初の配置
	void CreateDefaultEntities() override;

	void RegisterGV();
	void ApplyGV();

public:

// ===== 後 =====
	// GetCamera は上書きしない（Hierarchy に置いた Camera Component のカメラが使われる）
	// GetSceneFile も上書きしない（名前 "GameScene" から resources/scenes/GameScene.scene.json に決まる）

	// シーンファイルがまだ無いときだけ呼ばれる最初の配置
	void CreateDefaultEntities() override;

public:
```

`GameScene.cpp`（5か所）

```cpp
// ===== 前（1か所目：include。GlobalVariables.h の1行を消す）=====
#include <MyEngine/Time/Time.h>
#include <MyEngine/UI/GlobalVariables.h>
#ifdef USE_IMGUI

// ===== 後 =====
#include <MyEngine/Time/Time.h>
#ifdef USE_IMGUI
```

```cpp
// ===== 前（2か所目：include の後）=====
#include <externals/imgui/imgui.h>

namespace {

// ===== 後 =====
#include <externals/imgui/imgui.h>

// "GameScene" のシーン（resources/scenes/GameScene.scene.json）を開くときは、このクラスを使う
REGISTER_SCENE(GameScene);

namespace {
```

```cpp
// ===== 前（3か所目：Initialize の最後）=====
	SceneRenderer::GetSkybox()->SetRotationY(0.0f);

	RegisterGV();
	GlobalVariables::GetInstance()->LoadFiles();
}

// ===== 後 =====
	SceneRenderer::GetSkybox()->SetRotationY(0.0f);
}
```

```cpp
// ===== 前（4か所目：Update の最後）=====
#endif

	ApplyGV();
}

// ===== 後 =====
#endif
}
```

```cpp
// ===== 前（5か所目：ファイルの最後。調整項目の区画ごと消す）=====
	ModelRenderSystem::Draw({}, GetCamera(), &environment_, GetWindowTitle());
}

//==========================================
// 調整項目
//==========================================
void GameScene::RegisterGV() {}

void GameScene::ApplyGV() {}

// ===== 後 =====
	ModelRenderSystem::Draw({}, GetCamera(), &environment_, GetWindowTitle());
}
```

#### C-3. `main.cpp`（最初のシーンを名前で）

```cpp
// ===== 前 =====
#include <MyEngine/Engine.h>
#include "GameScene.h"
（略）
	Engine::Initialize(wc, [] { return std::make_unique<GameScene>(); });

// ===== 後 =====
#include <MyEngine/Engine.h>
（略）
	// 最初のシーン（resources/scenes/GameScene.scene.json）。クラスは GameScene.cpp の REGISTER_SCENE で登録してある
	Engine::Initialize(wc, "GameScene");
```

- `#include "GameScene.h"` は要らなくなる（名前で頼むので、`main.cpp` はクラスを知らなくてよい）。
- `GameScene.cpp` はプロジェクトに入っているので、`REGISTER_SCENE` は起動のときに必ず走る。

#### C-4. `Source/Components/SceneKeyTest.cpp`（新規・確認用。確かめ終わったら消す）

ソリューションエクスプローラーで `Source/Components` に追加する（ゲームのプロジェクトはフォルダを自動では読まない）。

```cpp
#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Input/InputManager.h>

// シーンの切り替え（SceneManager::LoadScene）を試すための Component（SceneFlow.md の SF1 の確認用。確かめ終わったら消す）
// Enter：Result へ　Backspace：GameScene へ　M：無いシーン（警告が出て、切り替わらない）
struct SceneKeyTest {};
COMPONENT(SceneKeyTest, "Test") { ui.Label("Enter: Result / Backspace: GameScene / M: 無いシーン"); }

void SceneKeyTestUpdate(SceneKeyTest&) {
	if (InputManager::IsKeyTriggered(DIK_RETURN)) {
		SceneManager::LoadScene("Result");
	}
	if (InputManager::IsKeyTriggered(DIK_BACK)) {
		SceneManager::LoadScene("GameScene");
	}
	if (InputManager::IsKeyTriggered(DIK_M)) {
		SceneManager::LoadScene("Missing");
	}
}
SYSTEM(SceneKeyTestUpdate);
```

#### C-5. 確認用の Result シーンを作る（SF2 までは手で）

1. エクスプローラーで `CG3_Project/resources/scenes/GameScene.scene.json` を同じフォルダに写して、名前を **`Result.scene.json`** にする。
2. `main.cpp` を一時的に `Engine::Initialize(wc, "Result");` にしてビルド・起動 → **Result が開く**（Control に `Scene: Result`）。見分けが付くように何か変える（例：Player を消す、MonsterBall の名前を `ResultBall` に）→ **Save**。
3. `main.cpp` を `"GameScene"` に戻す。
4. GameScene と Result の両方で、どれか1つの Entity に **Add Component → Gameplay → Test → SceneKeyTest**（検索欄に `SceneKey` と打ってもよい）を付けて Save（2. の間に Result にも付けておくと手間が少ない）。

> `Result` はクラスを書いていないので **DefaultScene** で開く（パーティクル・ステージは出ない。PBR のモデルは既定の空の IBL で出る）。

---

## 8. 確認すること

**ビルド**

- [ ] エンジン（Debug・Release）がエラー0、警告が前と同じ（`DIRECTINPUT_VERSION` の2つだけ）
- [ ] ゲーム（Debug・Release）がエラー0・警告0
- [ ] `SoundBus` で検索して0件（A-1）

**SF0**

- [ ] 起動する。Scene ビューにブルームが掛かっている（空の明るい所がにじむ）
- [ ] View → Scene View → Post Effects のチェックを外すと、Scene ビューだけ効果が消える（Game ビューは変わらない）
- [ ] PostEffect の Lens の Vignette を大きくしても、**Scene ビューは暗くならない**（Game ビューだけ暗くなる）。ライトのアイコンと物の位置がずれない
- [ ] 音が今まで通り鳴る（Audio Source の Bus のコンボが BGM / SE のまま、シーンファイルの `"bus": "SE"` がそのまま読める）
- [ ] Game ビューのクリック（MouseClickTest）の位置が今まで通り

**SF1**

- [ ] Control ウィンドウに `Scene: GameScene  (resources/scenes/GameScene.scene.json)`。Save・Ctrl+S で今まで通り保存できる
- [ ] Play → **Enter** で Result に替わる（Control に `Scene: GameScene → 今は Result（Stop で GameScene に戻ります）`）→ **Backspace** で GameScene に戻る（パーティクル・ステージも戻る）
- [ ] **M**（無いシーン）で Console に警告「LoadScene("Missing")：シーンがありません…」が出て、切り替わらない
- [ ] Result にいる間に **Stop** → GameScene の「Play を押した瞬間」に戻る（Play 中に動いた物も戻る）
- [ ] Result にいる間に **Restart** → GameScene の最初から
- [ ] Play → Enter → Stop を**10回以上**繰り返しても落ちない。Profiler の GPU → Stats → **SRV Slots が増え続けない**（DefaultScene が作る IBL のスロットが返っている）
- [ ] 停止中に Enter を押しても何も起きない（SYSTEM は再生中だけ）
- [ ] C-5 の 2.：`main.cpp` を `"Result"` にすると Result が開き、**Play → Stop で Result に戻る**（開いているシーンから Play）
- [ ] `main.cpp` を無い名前（例 `"Title"`）にすると、**カメラと平行光源だけの新しいシーン**が開く。Save すると `resources/scenes/Title.scene.json` ができる（確かめたら、ファイルを消して `"GameScene"` に戻す）
- [ ] PostEffect のタブで一度 **Save All Categories** → 値を変えて（Save せずに）Play → **値が戻らない**（C-2 で `LoadFiles` を消した効果。前は Save した値に戻っていた）

**確認が終わったら**

- [ ] `SceneKeyTest` を Entity から外して Save → `SceneKeyTest.cpp` を消す（Result.scene.json は S5 で使うなら残してよい）
- [ ] コミット（エンジンは `feature/scene-flow`）

---

## 9. 検証したこと（2026-09-30、Claude）

- **やり方**：本体（c43829b）とゲーム（`CG3_Project`）を検証用のフォルダに写し、パート A〜C をそのまま入れて MSBuild でビルド・起動した（本体とゲームのファイルは触っていない）。
- **ビルド**：エンジンとゲームを Debug・Release とも**全部ビルドし直して**エラー0。警告は前と同じ（エンジンの `DIRECTINPUT_VERSION` の2つ）、ゲームは0。
- **動かした確認**（自動で Play / Stop / `LoadScene` を呼ぶ確認用の main を作って実行。**30項目すべて OK**）：
  - `main.cpp` の `"GameScene"` で GameScene のクラス（`REGISTER_SCENE`）が使われ、ファイルは名前から `resources/scenes/GameScene.scene.json`
  - 停止中の `LoadScene` は `false` で切り替わらない。`HasScene`（ファイルあり／登録あり／無い／空）
  - Play 中の `LoadScene("Result")` は次のフレームで切り替わる。Result は DefaultScene、開いているシーンの名前は GameScene のまま、Result の Entity（Player が無い）になる
  - 無いシーンは `false` で切り替わらない。Result → GameScene（クラスも GameScene に戻る）→ Result
  - **`LoadScene` と `Stop` を同じフレームで頼んでも、Stop が勝って GameScene の Play の瞬間に戻る**（Entity の数も同じ）
  - Result にいる間の Restart は、GameScene の最初から
  - Scene ビューのポストエフェクトを切る・入れる
  - **Play → Result → Stop を12回繰り返して、SRV スロットが 28 → 28**（DefaultScene の IBL の分も返っている）
- **画面**：停止中の Scene ビューにブルームが掛かること、Result（DefaultScene）で PBR の MonsterBall が既定の空の IBL で描かれることを、撮った画面で確認した。
- **製品版（Release）**：本物の `main.cpp`（`Engine::Initialize(wc, "GameScene")`）で起動して落ちないこと、ログで `resources/scenes/GameScene.scene.json` が読まれていることを確認した（写した検証用のシーンファイルには `AudioKeyTest` が残っていたので、C-1 に書いた「知らないComponent」の警告が1回出た）。
- **確かめていないこと**：キーボードでの操作（SceneKeyTest）とメニューのクリック（検証ではマウス・キーを動かさなかった）。8章でお願いします。

---

## 10. 決めること（2回目）

SF2（Project ウィンドウ）と SF2.5（Settings ウィンドウ）を次の md で一緒に出すために、決めてほしいこと。

| 決めること | おすすめ | 理由 |
|---|---|---|
| シーンを作る・開くのは Project ウィンドウを主にして、File メニューも残す | はい | Unity と同じ（6.1）。キーだけで済ませたいときの近道 |
| Project ウィンドウに出すのは `resources/` の下だけ（`MyEngine/Resources/` のエンジンの物は出さない） | はい | ゲームが触るのは `resources/`。エンジンの画像を消したり名前を変えたりできないように |
| Project の Create → Scene は「作るだけで開かない」（開くのはダブルクリック） | はい | Unity と同じ。開くと「保存しますか？」が出るので、作るたびに出ると邪魔 |
| シーンの名前に使える文字は英数字と `_` `-` だけ（打てない文字は弾く） | はい | 日本語のファイル名は、まだ全部のアセットで読めない（Audio.md 8章）。直すときに一緒に広げる |
| Settings ウィンドウ1つに Project／Scene／Editor の3つの見出しでまとめる | はい | 気になることのとおり1か所。見出しで「保存先・Play で戻るか・製品版に入るか」が分かる（6.2） |
| Settings を開くメニュー | **Edit → Settings** | Unity の Edit → Project Settings と同じ場所。今の「Setting」メニュー（ImGui の見た目）は、あとで Settings の Editor に入れる |
| Debug（Scene ビューのカメラ）の保存先を `Parameters/Editor/` に移す | はい | 「エディタだけの物」が見て分かる。今の `Parameters/Debug/` は1回だけ読み替える |
| 空・IBL は Component ではなくシーンの設定（SF4） | はい | 6.2 のとおり。シーンに1つだけの物は Entity にしない |
| PostEffect は今はゲーム全体で1つ（シーンごとの上書きは後で） | はい | 今困っていない。上書きの形（Unity の Volume）にすれば後から足せる |
