# SceneFlow：シーンの切り替えを、コードを書かずに（エディタで）

## このファイルについて

- 2026-09-29 の依頼：「シーンの遷移をコードでやるのは面倒なので、なんとかしたい」。
- 2026-09-30：5章の「決めること」は**全部「はい」**。質問1つ（Unity のシーンは、ファイルを表示している所から作るのでは？）と、気になること1つ（Parameters・PostEffect のようなゲーム全体の物は、1か所にまとめた方がいいのでは？）に [6章](#6-質問気になることへの答え2026-09-30) で答えた。それに合わせて **SF2 と SF4 の設計を直し、SF2.5（Settings ウィンドウ）を足した**。
- **SF0・SF1 は写経用のコードを [7章](#7-手順sf0sf1) に入れた**（パート A〜C）。→ 写経・コミット済み（873b470）。
- 2026-09-30（2回目）の依頼：「多分できている。シーンの切り替え方を知りたい」「develop にマージしてビルドしたら10分かかった。なぜ？」「Project ウィンドウに要る画像（アイコン）を描くので教えて。画像ファイルはその画像を小さく出して」「10章は全部はい」「Add Component のウィンドウが下の方に出て、下の方の Component が選べない・スクロールできない」。
  - 写した物の確認 → [11章](#11-写したコードの確認873b4702026-09-30-2回目)、シーンの切り替えの解説 → [12章](#12-シーンの切り替えはどう動いているか解説)、ビルドが遅い理由 → [13章](#13-ビルドが遅い理由develop-にマージしたら10分)、描いてほしい画像 → [14章](#14-project-ウィンドウのアイコン描いてほしい画像)
  - **SF2（Project ウィンドウ）・SF2.5（Settings ウィンドウ）・Add Component の直しの写経用コードを [15章](#15-手順sf2sf25add-component-の直し) に入れた**（パート D〜J）。ブランチは `feature/project-window`（develop と同じ場所から切った）。→ 写経・コミット済み（bd7e4c2）。
- 2026-10-01 の依頼：「次にやることは？（課題の 2D 横スクロールアクション、20 時間くらい、10/28 締め切り）」＋質問・気になること5つ（`generic_string` は C++26 で非推奨／`TransitionFromCopyDest` は似た関数がある・public にしては／エンジンにアイコンの画像を入れてよいか・重くないか／小さい絵をなぜ DirectXTex で読まない／Camera が Rendering3D の中なのは変）。
  - 写した物の確認 → [18章](#18-写したコードの確認bd7e4c22026-10-01)（**1か所だけ動きが違う**：Scene View が Settings の Project の下に出る）、質問の答え → [19章](#19-質問気になることへの答え2026-10-01)、**直しの写経用コード → [20章](#20-手順パート-k質問から出た直しと写し違いの直し)（パート K）**。
  - 次にやること（課題のゲーム）の相談と設計は **[ActionGame.md](ActionGame.md)**（新しいファイル）。
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
| [10. 決めること（2回目）](#10-決めること2回目) | SF2・SF2.5・SF4 を始める前に決めてほしいこと（答え：全部はい） |
| [11. 写したコードの確認](#11-写したコードの確認873b4702026-09-30-2回目) | **873b470 と md の違い（エンジンは md どおり。ゲームに2つ残り）** |
| [12. シーンの切り替えはどう動いているか](#12-シーンの切り替えはどう動いているか解説) | **`LoadScene` から切り替わるまで・`REGISTER_SCENE`・Play / Stop・エディタの「開く」との違い** |
| [13. ビルドが遅い理由](#13-ビルドが遅い理由develop-にマージしたら10分) | **10分かかった理由と、速くする設定** |
| [14. Project ウィンドウのアイコン](#14-project-ウィンドウのアイコン描いてほしい画像) | **描いてほしい画像の一覧と決まり** |
| [15. 手順（SF2・SF2.5・Add Component）](#15-手順sf2sf25add-component-の直し) | **写経用のコード（パート D〜J）** |
| [16. 確認すること](#16-確認すること) | 写した後に確かめること |
| [17. 検証したこと](#17-検証したこと2026-09-30-2回目claude) | Claude がやったビルドと、動かして確かめたこと |
| [18. 写したコードの確認](#18-写したコードの確認bd7e4c22026-10-01) | **bd7e4c2 と md の違い（Scene View の置き場所・/MP が Debug だけ）** |
| [19. 質問・気になることへの答え](#19-質問気になることへの答え2026-10-01) | **`generic_string`・バリアの関数・アイコンの重さと権利・DirectXTex で読まない理由・Camera のカテゴリ・次にやること** |
| [20. 手順（パート K）](#20-手順パート-k質問から出た直しと写し違いの直し) | **写経用のコード（K-1〜K-7）** |
| [21. 確認すること](#21-確認すること) | 写した後に確かめること |
| [22. 検証したこと](#22-検証したこと2026-10-01claude) | Claude がやったビルドと、動かして確かめたこと |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| SF0 | 小さな整理（`AudioBus`・`GetMousePositionInWindow`・Scene ビューのポストエフェクト・ゲームの残り物） | ✓ 写経・コミット済み（873b470）。ゲームに残り1つ（11章 → パート I） |
| SF1 | シーン＝ファイル。`SceneManager::LoadScene("名前")`・`REGISTER_SCENE`・`Engine::Initialize(wc, "名前")`。Play は開いているシーンから | ✓ 写経・コミット済み（873b470）。`main.cpp` を戻す（パート I） |
| SF2 | **Project ウィンドウ**（シーンを作る・開く）・File メニュー・保存していない印・最後に開いたシーンから起動 | ✓ 写経・コミット済み（bd7e4c2） |
| SF2.5 | **Settings ウィンドウ**（ゲーム全体・シーン・エディタの設定を1か所に） | ✓ 写経・コミット済み（bd7e4c2）。**写し違い1つ**（18章 → パート K-5） |
| ― | Add Component のポップアップの直し | ✓ 写経・コミット済み（bd7e4c2） |
| ― | ビルドを速くする設定（/MP） | エンジンの Debug だけ済み。**残り（Release・ゲーム）**はパート K-6 |
| ― | 質問から出た直し（Camera のカテゴリ・バリアの関数・アセットの一覧を UTF-8・スライダーをやめる） | **写経待ち**（20章 パート K） |
| SF3 | コードを書かない切り替え（Scene Change Component）とフェード | 設計済み。**フェードは課題のゲームのために先に作る**（[ActionGame.md](ActionGame.md) の E5） |
| SF4 | 空・IBL を**シーンの設定**に → `GameScene` クラスが要らなくなる | 設計（6章で直した）。**「空を消す」だけ先に作る**（ActionGame.md の E4） |

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
| 見る | 左に `resources/` のフォルダ、右に選んだフォルダの中身。ファイルの種類（Scene・Model・Texture・Sound・その他）をアイコンで出す。**画像ファイルは、その画像の小さい絵**（2026-09-30 2回目の依頼。14章） |
| **シーンを作る** | 右の空いた所を右クリック → Create → Scene → 名前を打つ（Unity と同じ）。中身はカメラと平行光源（DefaultScene の最初の配置と同じ）。**作っただけでは開かない**（作った形：どのフォルダで押しても `resources/scenes/` に中身が空のファイルを作り、名前を打てる状態にする。開くとカメラと平行光源から始まる） |
| **シーンを開く** | ダブルクリック。保存していない変更があれば「保存しますか？ Save / Don't Save / Cancel」。**停止中だけ**（再生中は押せない） |
| 名前を変える・消す | F2・右クリック → Rename / Delete（消すのはごみ箱へ。開いているシーンは消せない） |
| フォルダを作る | Create → Folder |
| File メニュー（近道） | New Scene（名前を打つ → 作って開く）・Open Scene（`resources/scenes/` の一覧）・Save（Ctrl+S）・Save As（名前を打つ。ファイルを写して、開いているシーンをそちらに） |
| 保存していない印（`*`） | 保存した時点から Entity の変更（履歴）があれば Control ウィンドウの名前と Project の名前に `*`。Undo で保存した時点まで戻したら消える |
| 最後に開いていたシーン | エディタの設定ファイルに覚えて、次の起動でそれを開く（`main.cpp` のシーンは製品版の最初のシーン。`imgui.ini` と同じ考え方） |
| シーンもアセットとして選べる | `AssetType::Scene` を足す（SF3 の「どのシーンへ」を、音やモデルと同じコンボで選ぶため）。**SF3 で足す**（SF2 ではまだ使う所が無いので） |
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
| ✓ | **SF0** 小さな整理（7章 パート A・C） | 小 | コミット済み（873b470） |
| ✓ | **SF1** シーン＝ファイル・`LoadScene`・開いているシーンから Play（7章 パート B・C） | 中 | コミット済み（873b470） |
| ✓ | **SF2** Project ウィンドウ・File メニュー ＋ **SF2.5** Settings ウィンドウ（15章 パート D〜J） | 中 | コミット済み（bd7e4c2）。直しは 20章 パート K |
| 4 | **S5 小さなゲームを1本** → **課題の 2D 横スクロールアクション（10/28 締め切り）にする**（2026-10-01） | 中 | [ActionGame.md](ActionGame.md)。SF3 のフェード・SF4 の「空を消す」は、ここで先に作る |
| 5 | SF3・SF4 の残り | 中 | |
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

**答え（2026-09-30 2回目）：全部「はい」**。その通りに 15章で作った。

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

---

## 11. 写したコードの確認（873b470、2026-09-30 2回目）

写した物（本体 873b470・ゲームの今のファイル）を、Claude が検証に使ったコードと1行ずつ比べた。

**エンジン：md どおり**。違いは次の3つだけで、どれも動きには関係ない（直さなくてよい。直すなら1行ずつ）。

| ファイル | 違い | 直すなら |
|---|---|---|
| `Sound/AudioBus.h` | 説明のコメントが「ゲーム**空は** AudioSystem::SetBusVolume」になっている・2行抜けている | 「ゲーム**からは**」 |
| `Editor/Viewport/EditorViewport.cpp` 79行目 | 行の右のコメントが `// Game：全部          // Game：ブルームあり` と2つ | 後ろの `// Game：ブルームあり` を消す |
| `include` の並び（ImGuiManager.cpp・SceneManager.cpp・IScene.h） | 並びが md と違う（名前の順） | そのままでよい |

**ゲーム：2つ残っている**（パート I で直す）

| 何 | 今 | どうする |
|---|---|---|
| `main.cpp` | `Engine::Initialize(wc, "Result");`（C-5 の 2. のまま） | `"GameScene"` に戻す。**これからは `main.cpp` を書き換えなくてよい**（Project ウィンドウでシーンを開け、エディタは最後に開いたシーンから始まる） |
| `Source/Components/AudioKeyTest.cpp` | プロジェクトに残っている（C-1 の 1. の「Inspector で外して Save」は済んでいる。シーンファイルには残っていない） | ソリューションエクスプローラーで削除 |

`SceneKeyTest` は 8章の確認が終わるまで残しておいてよい（Enter で Result、Backspace で GameScene）。

---

## 12. シーンの切り替えはどう動いているか（解説）

### 12.1 3つの「シーン」

`SceneManager` は、次の3つを分けて覚えている。ここが分かると全部つながる。

| 名前 | 変数 | 何か | いつ変わる |
|---|---|---|---|
| **開いているシーン** | `openSceneName_`・`openSceneFactory_` | エディタで編集しているシーン。Play はここから始まり、Stop でここへ戻る | エディタで開いたとき（この md の SF2 から：Project ウィンドウ・File → Open Scene） |
| **今のシーン** | `currentScene_` | いま Update・Draw されているシーン（`IScene` のクラスの実体） | Play・Stop・Restart・`LoadScene` |
| **次のシーン** | `nextSceneName_` | `LoadScene("Result")` で頼まれた名前。**まだ切り替えていない** | `LoadScene` を呼んだとき（使ったら空に戻る） |

停止中は「開いているシーン＝今のシーン」。再生中に `LoadScene` で進むと、今のシーンだけが替わる（Control ウィンドウの `Scene: GameScene → 今は Result`）。

### 12.2 `LoadScene("Result")` を呼んでから、切り替わるまで

```
 フレーム N（Result へ行きたい）
 ┌──────────────────────────────────────────────────────────────┐
 │ SceneManager::Update                                          │
 │   └ SYSTEM(SceneKeyTestUpdate) が Enter を見て                 │
 │        SceneManager::LoadScene("Result")                       │
 │          ① 再生中？        …停止中なら何もしない（false）        │
 │          ② "Result" はある？ …無ければ Log に警告（false）        │
 │          ③ nextSceneName_ = "Result"   ← 予約だけ。まだ替えない │
 │ （この後も、フレーム N の Update・Draw は GameScene のまま）      │
 └──────────────────────────────────────────────────────────────┘
 フレーム N+1
 ┌──────────────────────────────────────────────────────────────┐
 │ SceneManager::Update の頭（誰も ForEach で回していない所）        │
 │   └ nextSceneName_ があるので ChangeScene(CreateScene("Result"))│
 │        CreateScene：REGISTER_SCENE の表に "Result" は？          │
 │           ある → そのクラス（GameScene など）                    │
 │           無い → DefaultScene（Entity だけで動くシーン）          │
 │           名前から resources/scenes/Result.scene.json に決まる   │
 │        ChangeScene：                                           │
 │           1. 前のシーンの Finalize（IBL など Entity でない物）    │
 │           2. Entity を全部消す → シーンファイルから作る            │
 │              （ファイルが無い・空なら CreateDefaultEntities）     │
 │           3. 新しいシーンの Initialize                          │
 │   └ Result の Update・SYSTEM が動く                              │
 └──────────────────────────────────────────────────────────────┘
```

- **なぜ次のフレームなのか**：`LoadScene` を呼ぶのは SYSTEM の中＝`ForEach` で Entity を回している最中。そこで Entity を全部消すと、回している途中の一覧が壊れる。だから「予約だけして、フレームの頭（誰も回していない所）で替える」。Component の追加・削除（`RequestAdd`）と同じ考え方。
- **同じフレームに何回も呼んだら**：最後に頼んだシーンへ行く（`nextSceneName_` を上書きするだけなので）。
- **Stop と同じフレームに呼んだら**：Stop が勝つ（作り直すときに `nextSceneName_` を捨てる）。

### 12.3 `REGISTER_SCENE(GameScene)` は何をしているか

```cpp
// GameScene.cpp
REGISTER_SCENE(GameScene);
// ↓ マクロを広げると
inline const SceneRegistrar<GameScene> sceneRegistrarOf_GameScene("GameScene");
```

- `SceneRegistrar` は「作られた瞬間に、名前と作り方を表に入れる」だけのクラス。グローバル変数は `main` より前に作られるので、**`main` が始まった時点で表に `"GameScene" → GameScene を作る関数` が入っている**。
- 表（`SceneTable()`）を関数の中の `static` にしているのは、グローバル変数の作られる順番がファイルをまたぐと決まっていないから（表より先に `REGISTER_SCENE` が動くと、無い表に書き込んで落ちる）。関数の中の `static` は「初めて呼ばれたときに必ず作られる」。
- `COMPONENT(...)`・`SYSTEM(...)` も同じ仕組み（GameComponent.h）。

### 12.4 Play・Stop とのつながり

| 操作 | SceneManager がすること |
|---|---|
| **Play** | 開いているシーンの全 Entity を JSON にして退避（`playSnapshot_`）→ 退避から作り直して再生 |
| **再生中に `LoadScene`** | 今のシーンだけ替わる。開いているシーンと退避はそのまま |
| **Stop** | **開いているシーン**を、退避（Play を押した瞬間）から作り直す。Result にいても GameScene に戻る |
| **Restart** | Stop と同じ作り直しをして、再生を続ける |

### 12.5 エディタの「開く」とゲームの `LoadScene` の違い（この md の SF2 から）

| | ゲームの `LoadScene("Result")` | エディタで開く（Project のダブルクリック・File → Open Scene） |
|---|---|---|
| いつ使える | 再生中だけ | 停止中だけ |
| 替わる物 | 今のシーンだけ | **開いているシーン**（Stop・Restart の戻り先も変わる） |
| 保存していない変更 | 関係ない（再生中は保存しない） | あれば「保存しますか？」を聞く |
| 次の起動 | 関係ない | 最後に開いたシーンから始まる（`Parameters/Editor/LastScene.json`） |

### 12.6 うまく切り替わらないとき

| 様子 | 原因 | 見る所 |
|---|---|---|
| 何も起きない | 停止中に呼んでいる（SYSTEM は再生中だけ動く） | Play を押してから |
| Console に「シーンがありません」 | 名前が違う（大文字・小文字も）・ファイルが `resources/scenes/` に無い | Project ウィンドウの `resources/scenes` |
| 切り替わるが、何も出ない | そのシーンのファイルが空（まだ Save していない）→ カメラと平行光源だけ | そのシーンを開いて置いて Save |
| パーティクル・ステージが出ない | そのシーンはクラスが無い（DefaultScene）。パーティクル・ステージは GameScene のクラスが作っている | SF4 で Entity・シーンの設定に移す |

---

## 13. ビルドが遅い理由（develop にマージしたら10分）

### 13.1 何が起きたか

1. **develop に切り替えて、マージした**（git の記録：`checkout: feature/scene-flow → develop` → `merge feature/scene-flow: Fast-forward`）。
   - develop は3つ前のコミット（7008f2e）だったので、切り替えた瞬間に、3つのブランチ分（Audio・GameBasics・SceneFlow）で変わった **54 ファイルが古い中身に書き戻され**、マージでまた**新しい中身に書き戻された**。
   - Visual Studio は「中身」ではなく「**ファイルの更新時刻**」でビルドし直すかを決める。中身が前にビルドした時と同じでも、時刻が新しくなったファイルは全部ビルドし直しになる。
   - 書き戻されたヘッダには `GameComponent.h`・`EntityManager.h`・`Matrix4x4.h`・`GlobalVariables.h` など、ほとんどの cpp が読む物が入っている。数えると **エンジンの cpp 86個のうち 59個＋ゲームの cpp 全部**がビルドし直しになる。
2. **1ファイルずつ順番にコンパイルしている**。
   - この PC の CPU は 10コア・16スレッドだが、エンジンとゲームのプロジェクトは「**複数プロセッサによるコンパイル（/MP）**」がオフ（Visual Studio の既定はオフ）。1つのプロジェクトの中の cpp は、1つずつ順番にコンパイルされる。
   - 1ファイルは重い（windows.h・d3d12.h・ImGui・json・magic_enum などを毎回読む）。前に測ったときで1ファイル 2.5秒くらい。70個 × 数秒 ＝ Debug だけで数分。
3. **Release もビルドしたなら、さらにかかる**。Release は「プログラム全体の最適化（/GL）」がオンで、リンクのときにもう一度まとめて最適化するので、Debug より遅い。

### 13.2 直し方（おすすめ順）

**① 複数プロセッサによるコンパイルをオンにする（コードは変えない。設定1つ）** → パート J

- エンジン（MyEngine_v1）とゲーム（CG3_Project）の両方で、**プロパティ → 構成：すべての構成 → C/C++ → 全般 → 複数プロセッサによるコンパイル → はい (/MP)**。
- これだけで、たくさんビルドし直すときは何倍か速くなる（1つずつだったのが、16個ずつ同時になる）。Claude の検証のビルドは、ずっとこれをオンにして回している（エラー・警告は同じ）。

**② develop へのマージで、ブランチを切り替えない**

作業が終わって develop に入れるとき、今は「develop に切り替える → マージ」をしている。**切り替えずに develop だけを進める**と、ファイルが1つも書き換わらないので、ビルドし直しが起きない。

```bash
git fetch . feature/scene-flow:develop
```

- 「このリポジトリ（`.`）の `feature/scene-flow` の位置まで、`develop` を進める」という意味。今いるブランチのファイルには触らない（develop にいるときは使えない。作業のブランチにいるまま打つ）。
- develop が先に進んでいて「早送り（fast-forward）」できないときは断られる（そのときは今まで通り切り替えてマージする）。
- 次の作業のブランチも、今の場所からそのまま切ればよい（`git switch -c feature/次の作業`。develop と同じコミットなので、ファイルは変わらない）。**今回 Claude が `feature/project-window` を作ったのもこのやり方**なので、このブランチでビルドしてもビルドし直しは起きない。

**③ 動くか確かめるだけなら Debug だけビルドする**（Release は出す前に1回でよい）

**④ リビルド（Rebuild）は使わない**（DirectXTex の Clean がシェーダーの `.inc` を消してしまう。全部ビルドし直したいときは `x64` フォルダを消して、ふつうにビルド）

**⑤ それでも遅ければ PCH（プリコンパイル済みヘッダ）**：GameBasics.md の判断（今はやらない）は変わらない。①の後にまだ困ったら、エンジンだけに入れる形を出す。

---

## 14. Project ウィンドウのアイコン（描いてほしい画像）

コードは画像が無くても動く（無い種類は、色の付いた形と頭文字で代わりに描く）。描いた画像を置けば、コードを直さずに差し替わる。**画像ファイル（png・jpg・bmp・hdr）は、その画像を小さく縮めた絵が出る**（頼まれた通り）ので、Texture のアイコンは「小さい絵を作っている一瞬」と「読めなかったとき」だけ出る。

### 14.1 描いてほしい物

| ファイル名 | 出す所 | 今の代わりの見た目 | 絵の案 |
|---|---|---|---|
| **`Folder.png`** | フォルダ（右の並び・左の木） | 黄色いフォルダの形 | 閉じたフォルダ |
| `FolderOpen.png`（**無くてもよい**） | 左の木の、中を見ているフォルダ | Folder と同じ | 開いたフォルダ（無ければ Folder.png を使う） |
| **`Scene.png`** | シーン（`.scene.json`） | 青い四角に「S」 | Unity のシーンのような絵（カチンコ・地球・山など） |
| **`Model.png`** | モデル（`.obj` `.gltf` `.glb` `.fbx`） | 水色の四角に「M」 | 立方体・ポリゴン |
| **`Texture.png`** | 画像（小さい絵ができるまで） | 紫の四角に「T」 | 山と太陽の写真の絵 |
| **`Sound.png`** | 音（`.wav` `.mp3`） | 緑の四角に「♪」 | 音符・スピーカー |
| **`File.png`** | それ以外（`.mtl` `.bin` `.json` `.blend` など） | 灰色の四角 | 白紙の紙（右上が折れた形） |

### 14.2 画像の決まり

| 項目 | 決まり |
|---|---|
| 形式 | PNG（透明を使える） |
| 大きさ | **正方形。128×128 がおすすめ**（64×64 でもよい）。16px（1行ずつの並び・左の木）〜128px（アイコンの並びを一番大きくしたとき）まで縮めて出す。縮めてもつぶれないように、線は太めに |
| 背景 | **透明**（ウィンドウの色が透けて見えるように） |
| 余白 | 上下左右に少し（1〜2割）。並べたときにくっつかない |
| 色 | 自由。上の「今の代わりの見た目」の色にそろえると、種類が見分けやすい |
| 名前 | 上の表の通り（1文字でも違うと読まれず、代わりの形が出る） |

### 14.3 置き場所

- エンジンのリポジトリの **`Resources/Editor/Icons/`**（`Resources/Textures/` と同じ並び）に置く。フォルダは無ければ作る。
- エンジンをビルドすると、ビルド後のコピー（xcopy）でゲームの `MyEngine/Resources/Editor/Icons/` に写される。**起動したときに読む**ので、置いたらゲームを起動し直す。
- ソリューションエクスプローラーに足さなくてよい（ファイルを置くだけ）。

---

## 15. 手順（SF2・SF2.5・Add Component の直し）

10章の答え（全部はい）の通りに作った。

| パート | 中身 | 順番 |
|---|---|---|
| **D** | Add Component のポップアップ（下に入らなければ上に出す・一覧だけスクロール） | いつでも（ほかと関係ない） |
| **E** | 部品（テクスチャの転送・ファイル名の決まり・拡張子の判定・ファイル名の入力欄・履歴の変更回数・空のシーンファイル） | F〜H より先 |
| **F** | シーンを作る・開く・名前を付けて保存・保存していない印（`*`）・最後に開いたシーン | E の後 |
| **G** | Settings ウィンドウ（GlobalVariables の書き直し・Scene ビューのカメラの引っ越し） | E の後 |
| **H** | Project ウィンドウ・アイコン・小さい絵・「保存しますか？」・メニュー（ImGuiManager は**最後**） | E・F・G の後 |
| **I** | ゲーム側（`main.cpp` を戻す・`AudioKeyTest` を消す） | いつでも |
| **J** | ビルドの設定（/MP） | いつでも（**最初にやると、この後のビルドが速い**） |

- 新しいファイルは、ソリューションエクスプローラーでエンジンのプロジェクトの同じフォルダ（フィルター）に追加する。**`Editor/Assets/` は新しいフォルダ**。
- 変える所が多いファイル（`SceneManager.h/.cpp`・`GlobalVariables.h/.cpp`）は全体を差し替え。

| 新しいファイル | パート |
|---|---|
| `MyEngine/String/FileNameRule.h` | E |
| `MyEngine/Editor/Windows/SettingsWindow.h/.cpp` | G |
| `MyEngine/Editor/Assets/AssetIcons.h/.cpp`・`AssetThumbnails.h/.cpp` | H |
| `MyEngine/Editor/Windows/SceneDialogs.h/.cpp`・`ProjectWindow.h/.cpp` | H |

### パート D：Add Component のポップアップ（`Editor/Windows/InspectorWindow.cpp` 3か所）

**何が悪かったか**：ポップアップを、いつも「ボタンの真下」に出していた。Add Component のボタンは Component の一覧の一番下にあるので、Component が多い Entity だとボタンが画面の下の方になり、ポップアップの下半分が画面（エディタのウィンドウ）の外にはみ出す。高さの上限（420）より小さいので ImGui はスクロールバーを出さず、はみ出した所はただ見えない＝選べない。

**直し方**：

1. 下に入りきらなくて、上の方が広ければ、**ボタンの上**に出す（`SetNextWindowPos` の3つ目の引数 pivot を `(0, 1)` にすると、「左下の角」をその位置に合わせる。カテゴリを開くと上へ伸びる）。
2. 高さの上限を「画面の残り」に合わせる。
3. **一覧だけを子ウィンドウに入れる**。`ImGuiChildFlags_AutoResizeY` は中身に合わせて高さが決まる子ウィンドウで、`SetNextWindowSizeConstraints` の上限を超えるとスクロールバーが出る。検索欄は子ウィンドウの外なので、スクロールしても見えたまま。
4. 子ウィンドウの中の `Selectable` を押しても、ポップアップは自動では閉じない（ImGui は「今のウィンドウがポップアップのとき」だけ閉じる）ので、自分で `CloseCurrentPopup()` を呼ぶ。

```cpp
// ===== 前（1か所目：include）=====
#include "InspectorWindow.h"

#include <cfloat>

// ===== 後 =====
#include "InspectorWindow.h"

#include <algorithm>
#include <cfloat>
```

```cpp
// ===== 前（2か所目：DrawAddComponentItem）=====
	ImGui::BeginDisabled(editor.Get(handle) || editor.IsAddPending(handle));
	if (ImGui::Selectable(editor.GetName())) { // Selectableを押すとポップアップは自動で閉じる
		EditorHistory::RequestAddComponent(handle, editor);
	}
	ImGui::EndDisabled();

// ===== 後 =====
	ImGui::BeginDisabled(editor.Get(handle) || editor.IsAddPending(handle));
	if (ImGui::Selectable(editor.GetName())) {
		EditorHistory::RequestAddComponent(handle, editor);
		// 一覧はスクロールするための子ウィンドウの中なので、Selectable を押してもポップアップは自動では閉じない。ここで閉じる
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
```

```cpp
// ===== 前（3か所目：DrawAddComponent の後半）=====
	// --- ボタンの真下に、ボタンと同じ幅の縦長のポップアップを出す ---
	const ImVec2 buttonMin = ImGui::GetItemRectMin();
	const ImVec2 buttonMax = ImGui::GetItemRectMax();
	const float width = buttonMax.x - buttonMin.x;
	ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y));
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, kAddComponentMaxHeight));
	if (!ImGui::BeginPopup("addComponent")) {
		return;
	}
（略：検索欄と items を集める所はそのまま）
	DrawAddComponentLevel(handle, items, 0, items.size(), 0, filter);

	ImGui::EndPopup();
}

// ===== 後 =====
	// --- ボタンと同じ幅の縦長のポップアップを、ボタンの下に入れば下、入らなければ上に出す ---
	// （ボタンが画面の下の方にあると、下に出したポップアップが画面の外にはみ出して、下の方の項目が選べなかった）
	const ImVec2 buttonMin = ImGui::GetItemRectMin();
	const ImVec2 buttonMax = ImGui::GetItemRectMax();
	const float width = buttonMax.x - buttonMin.x;
	const ImGuiViewport* viewport = ImGui::GetWindowViewport(); // エディタ全体の画面（メニューバーの下から）
	const float margin = ImGui::GetStyle().WindowPadding.y;     // 画面の端から少し離す
	const float spaceBelow = viewport->WorkPos.y + viewport->WorkSize.y - buttonMax.y - margin;
	const float spaceAbove = buttonMin.y - viewport->WorkPos.y - margin;
	const bool opensBelow = (spaceBelow >= kAddComponentMaxHeight) || (spaceBelow >= spaceAbove); // 下に全部入る、または下の方が広い
	const float maxHeight = (std::min)(kAddComponentMaxHeight, opensBelow ? spaceBelow : spaceAbove);
	if (opensBelow) {
		ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y)); // 左上をボタンの左下に
	} else {
		ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMin.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f)); // 左下をボタンの左上に（カテゴリを開くと上へ伸びる）
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, maxHeight));
	if (!ImGui::BeginPopup("addComponent")) {
		return;
	}
（略：検索欄と items を集める所はそのまま）
	// 一覧だけを子ウィンドウに入れて、スクロールさせる（検索欄はいつも見える）
	// AutoResizeY：中身に合わせて高さが決まる。上限（ポップアップの残りの高さ）を超えたらスクロールバーが出る
	const float listMaxHeight = (std::max)(maxHeight - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y, ImGui::GetFrameHeight());
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, listMaxHeight));
	if (ImGui::BeginChild("##componentList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY)) {
		DrawAddComponentLevel(handle, items, 0, items.size(), 0, filter);
	}
	ImGui::EndChild();

	ImGui::EndPopup();
}
```

- `listMaxHeight`：ポップアップの高さの上限から、検索欄と区切り線の分（`GetCursorPosY()`＝ここまでに使った高さ）と下の余白を引いた残り。
- `(std::min)`・`(std::max)` をかっこで囲むのは、windows.h の `min` / `max` マクロに邪魔されないため（CLAUDE.md の決まり）。

---

### パート E：部品

#### E-1. `MyEngine/String/FileNameRule.h`（新規）

シーン・フォルダの名前に使える文字の決まり（10章の答え：英数字と `_` `-`）を1か所に置く。入力欄（打てない文字は入らない）・SceneManager（作る前に確かめる）・Project ウィンドウ（名前の変更）が全部これを使うので、決まりを変えるときはここだけ直せばよい。

```cpp
#pragma once
#include <string_view>

//=============================================================================
// エディタで作る・名前を変えるファイルの名前の決まり（シーン・フォルダ・Project ウィンドウの Rename）
//=============================================================================
// 使えるのは英数字と _ - だけ。日本語のファイル名は、まだ全部のアセットで読めない（Audio.md 8章）ので、打てなくしている
// 読めるようにするときは、ここを広げれば全部の入力欄が一緒に変わる


// 1文字が使えるか（入力欄で1文字ずつ見る）
inline bool IsFileNameChar(unsigned int c) {
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-';
}

// 名前が使えるか（空はだめ）
inline bool IsValidFileName(std::string_view name) {
	if (name.empty()) {
		return false;
	}
	for (const char c : name) {
		if (!IsFileNameChar(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}
```

#### E-2. `Editor/Widgets/EditorWidgets.h`（2か所。ファイル名の入力欄）

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Editor/Widgets/ImeInput.h"

// ===== 後 =====
#include "MyEngine/Editor/Widgets/ImeInput.h"
#include "MyEngine/String/FileNameRule.h"
```

```cpp
// ===== 前（2か所目：InputText の後）=====
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways;
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, InputTextCallback, &text);
}

// ===== 後 =====
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways;
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, InputTextCallback, &text);
}

// ファイル名の入力欄のコールバック。使えない文字（FileNameRule.h）は、打っても入らない
inline int FileNameCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
		return IsFileNameChar(data->EventChar) ? 0 : 1; // 1 を返すと、その文字は入らない
	}
	return InputTextCallback(data); // 文字が増えたら std::string を伸ばす
}

/// <summary>
/// ファイル名の入力欄（std::string 版）。英数字と _ - しか打てない（シーン・フォルダの名前など）
/// </summary>
inline bool InputFileName(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackCharFilter;
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, FileNameCallback, &text);
}
```

- `ImGuiInputTextFlags_CallbackCharFilter` を付けると、1文字打つたびにコールバックが呼ばれ、1 を返すとその文字は入らない。

#### E-3. `ECS/ComponentUI.h`・`ComponentUI.cpp`（拡張子の判定を外から使えるように）

Project ウィンドウでも「このファイルはモデル？画像？音？」を見分けたい。前は `ComponentUI.cpp` の中だけ（しかも ImGui のある時だけ）の関数だったので、同じ拡張子の一覧を Project ウィンドウに**写さずに**使えるよう、外に出す。

`ComponentUI.h`（1か所）

```cpp
// ===== 前 =====
// パスのファイルを読み込んで番号をもらう（空・ファイルが無いなら0＝未選択）
uint32_t LoadAsset(AssetType type, const std::string& path);

// ===== 後 =====
// パスのファイルを読み込んで番号をもらう（空・ファイルが無いなら0＝未選択）
uint32_t LoadAsset(AssetType type, const std::string& path);
// その種類のアセットのファイルか（拡張子で見る。大文字でもよい）。Inspector のファイルの一覧・Project ウィンドウのアイコンで使う
bool IsAssetFile(std::string_view path, AssetType type);
```

- 引数を `std::filesystem::path` ではなく `std::string_view` にしたのは、`ComponentUI.h` はゲームの全部の Component が読むヘッダなので、重い `<filesystem>` を足したくないから。

`ComponentUI.cpp`（4か所）

```cpp
// ===== 前（1か所目：先頭の include）=====
#include "MyEngine/ECS/ComponentUI.h"

#include <filesystem>

// ===== 後 =====
#include "MyEngine/ECS/ComponentUI.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
```

```cpp
// ===== 前（2か所目：LoadAsset の後。#ifdef USE_IMGUI の前に足す）=====
		return SoundManager::Load(path); // 読めない形式なら0（SoundManager がログに出す）
	}
	return 0;
}

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <cstring>

// ===== 後 =====
		return SoundManager::Load(path); // 読めない形式なら0（SoundManager がログに出す）
	}
	return 0;
}

//=============================================================================
// アセットの種類ごとの拡張子（Inspector のファイルの一覧・Project ウィンドウのアイコン）
//=============================================================================
namespace {
constexpr std::string_view kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};
constexpr std::string_view kTextureExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".hdr"};
constexpr std::string_view kSoundExtensions[] = {".wav", ".mp3"}; // SoundManager（Media Foundation）が読める物

// その種類の拡張子の一覧
std::span<const std::string_view> ExtensionsOf(AssetType type) {
	switch (type) {
	case AssetType::Model:
		return kModelExtensions;
	case AssetType::Texture:
		return kTextureExtensions;
	case AssetType::Sound:
		return kSoundExtensions;
	}
	return {};
}
} // namespace

bool IsAssetFile(std::string_view path, AssetType type) {
	// 最後の「.」から後ろが拡張子（フォルダの名前に入っている「.」は見ない）
	const size_t dot = path.find_last_of('.');
	const size_t slash = path.find_last_of("/\\");
	if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
		return false; // 拡張子が無い
	}
	// 大文字でも通るように、小文字へ直して比べる
	std::string extension(path.substr(dot));
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const std::span<const std::string_view> candidates = ExtensionsOf(type);
	return std::find(candidates.begin(), candidates.end(), extension) != candidates.end();
}

#ifdef USE_IMGUI
#include <cstring>
```

```cpp
// ===== 前（3か所目：#ifdef USE_IMGUI の中。3行を消す＝上へ移した）=====
constexpr const char* kAssetSearchRoots[] = {"resources", "MyEngine/Resources"}; // 探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr std::string_view kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};
constexpr std::string_view kTextureExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".hdr"};
constexpr std::string_view kSoundExtensions[] = {".wav", ".mp3"};       // SoundManager（Media Foundation）が読める物
constexpr size_t kAssetTypeCount = magic_enum::enum_count<AssetType>(); // AssetType の数（種類を足しても、ここは直さなくてよい）

// ===== 後 =====
constexpr const char* kAssetSearchRoots[] = {"resources", "MyEngine/Resources"}; // 探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr size_t kAssetTypeCount = magic_enum::enum_count<AssetType>(); // AssetType の数（種類を足しても、ここは直さなくてよい）
```

```cpp
// ===== 前（4か所目：#ifdef USE_IMGUI の中の ExtensionsOf と IsAssetFile を丸ごと消す＝上へ移した。AssetFiles の中の呼び方を変える）=====
// その種類の拡張子の一覧
std::span<const std::string_view> ExtensionsOf(AssetType type) {
（略：中身）
}

（略：SearchRootsText はそのまま）

// その種類の拡張子か（大文字でも通るように小文字へ直して比べる）
bool IsAssetFile(const std::filesystem::path& path, AssetType type) {
（略：中身）
}

（略：AssetFiles の途中）
			if (entry.is_regular_file(error) && IsAssetFile(entry.path(), type)) {

// ===== 後（ExtensionsOf・IsAssetFile は、ここには無い）=====
（略：SearchRootsText はそのまま）

（略：AssetFiles の途中）
			if (entry.is_regular_file(error) && IsAssetFile(entry.path().generic_string(), type)) {
```

#### E-4. `Graphics/GPU/UploadContext.h`・`UploadContext.cpp`（テクスチャを、フレームの途中で GPU へ送る）

Project ウィンドウの小さい絵は、ImGui を描いている最中（フレームの途中）に作る。`TextureManager::Load` は描画のコマンドリストを一度閉じて実行してしまう（フレームの途中で呼ぶと、そこまで積んだ描画のコマンドが先に走る）ので使わない。モデルの頂点を送るのに使っている **UploadContext（描画とは別のコマンドリスト）** に、テクスチャを送る関数を足す。

`UploadContext.h`（2か所）

```cpp
// ===== 前（1か所目）=====
#pragma once
#include <cstdint>

#include <d3d12.h>

// ===== 後 =====
#pragma once
#include <cstdint>
#include <span>

#include <d3d12.h>
```

```cpp
// ===== 前（2か所目：QueueUpload の後）=====
	static void QueueUpload(ID3D12Resource* dest, const void* data, size_t size, D3D12_RESOURCE_STATES finalState);

	/// <summary>予約した全コピーをGPUに流して完了を待ち、stagingを再利用可能に戻す</summary>

// ===== 後 =====
	static void QueueUpload(ID3D12Resource* dest, const void* data, size_t size, D3D12_RESOURCE_STATES finalState);

	/// <summary>
	/// dest(DEFAULTヒープの2Dテクスチャ)へ、ミップマップの段ごとの中身を転送する予約。
	/// <para>subresources[i] が i 段目（0 が一番大きい絵）。1行の幅は、中で GPU の決まり（256バイトの倍数）に詰め直す</para>
	/// <para>描画のコマンドリストとは別のリストで送るので、フレームの途中（ImGui を描いている間）でも使える</para>
	/// </summary>
	/// <param name="finalState">転送後の状態（シェーダーで読むなら PIXEL_SHADER_RESOURCE）</param>
	static void QueueTextureUpload(ID3D12Resource* dest, std::span<const D3D12_SUBRESOURCE_DATA> subresources, D3D12_RESOURCE_STATES finalState);

	/// <summary>予約した全コピーをGPUに流して完了を待ち、stagingを再利用可能に戻す</summary>
```

`UploadContext.cpp`（3か所。QueueUpload の中の状態の変え方を関数にして、新しい関数と共通にする）

```cpp
// ===== 前（1か所目：include と静的メンバ変数）=====
#include <cassert>
#include <cstring>

（略）
// 静的メンバ変数
UploadContext* UploadContext::instance_ = nullptr;

// ===== 後 =====
#include <cassert>
#include <cstring>
#include <vector>

（略）
// 静的メンバ変数
UploadContext* UploadContext::instance_ = nullptr;

namespace {
// コピーし終わった dest を、コピー先の状態から使う状態へ変える（QueueUpload・QueueTextureUpload で共通）
void TransitionFromCopyDest(ID3D12GraphicsCommandList* commandList, ID3D12Resource* dest, D3D12_RESOURCE_STATES finalState) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = dest;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = finalState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
}
} // namespace
```

```cpp
// ===== 前（2か所目：QueueUpload の後半）=====
	// Resourceの状態を変更
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = dest;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = finalState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	instance_->commandList_->ResourceBarrier(1, &barrier);

// ===== 後 =====
	// Resourceの状態を変更
	TransitionFromCopyDest(instance_->commandList_.Get(), dest, finalState);
```

```cpp
// ===== 前（3か所目：QueueUpload の終わり。この後に新しい関数を足す）=====
	instance_->stagingOffset_ = (instance_->stagingOffset_ + size + 255) & ~size_t(255);
	instance_->hasPending_ = true;
}

// ===== 後 =====
	instance_->stagingOffset_ = (instance_->stagingOffset_ + size + 255) & ~size_t(255);
	instance_->hasPending_ = true;
}

//=============================================================================
// テクスチャの転送の予約：段ごとに1行ずつ staging へ書き、staging→dest のコピーを記録する
//=============================================================================
void UploadContext::QueueTextureUpload(ID3D12Resource* dest, std::span<const D3D12_SUBRESOURCE_DATA> subresources, D3D12_RESOURCE_STATES finalState) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	const UINT count = static_cast<UINT>(subresources.size());

	// --- staging の中での置き方を GPU に聞く ---
	// GPU がテクスチャをコピーするときは「1行の幅が256バイトの倍数」の決まりがあるので、画像の1行の幅とは違うことがある
	const D3D12_RESOURCE_DESC desc = dest->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(count); // 段ごとの置き場所（先頭からの位置・1行の幅）
	std::vector<UINT> rowCounts(count);                             // 段ごとの行の数
	std::vector<UINT64> rowSizes(count);                            // 段ごとの1行の中身のバイト数
	UINT64 totalBytes = 0;
	DirectXCommon::GetDevice()->GetCopyableFootprints(&desc, 0, count, 0, layouts.data(), rowCounts.data(), rowSizes.data(), &totalBytes);
	MY_ASSERT_MSG(totalBytes <= instance_->stagingCapacity_, "1回の転送サイズがstaging容量を超えています。容量を増やしてください。");

	// --- テクスチャは512バイトの倍数の位置から置く。入りきらなければ、いったん Flush して空にする ---
	constexpr size_t kAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	size_t base = (instance_->stagingOffset_ + kAlignment - 1) & ~(kAlignment - 1);
	if (base + totalBytes > instance_->stagingCapacity_) {
		Flush();
		base = 0;
	}

	for (UINT i = 0; i < count; ++i) {
		// 1行ずつ写す（staging の1行の幅は layouts[i].Footprint.RowPitch、画像の1行の幅は subresources[i].RowPitch）
		uint8_t* destRows = instance_->stagingMapped_ + base + layouts[i].Offset;
		const auto* sourceRows = static_cast<const uint8_t*>(subresources[i].pData);
		for (UINT row = 0; row < rowCounts[i]; ++row) {
			std::memcpy(destRows + static_cast<size_t>(row) * layouts[i].Footprint.RowPitch, sourceRows + static_cast<size_t>(row) * subresources[i].RowPitch, static_cast<size_t>(rowSizes[i]));
		}

		// staging の i 段目 → dest の i 段目
		D3D12_TEXTURE_COPY_LOCATION destination{};
		destination.pResource = dest;
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destination.SubresourceIndex = i;
		D3D12_TEXTURE_COPY_LOCATION source{};
		source.pResource = instance_->staging_.Get();
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = layouts[i];
		source.PlacedFootprint.Offset += base; // GetCopyableFootprints は0から数えた位置を返すので、staging の中の位置に直す
		instance_->commandList_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	}

	// Resourceの状態を変更
	TransitionFromCopyDest(instance_->commandList_.Get(), dest, finalState);

	// Offsetを進める
	instance_->stagingOffset_ = (base + static_cast<size_t>(totalBytes) + 255) & ~size_t(255);
	instance_->hasPending_ = true;
}
```

**解説（テクスチャの転送）**

- バッファ（頂点）は「バイトの列をそのまま写す」だけだが、テクスチャは GPU の中で行の並び方が違うので、`CopyTextureRegion` に「staging の中で、何バイト目から・1行が何バイトで並んでいるか（footprint）」を教える。
- その footprint は自分で計算せず `GetCopyableFootprints` に聞く。1行の幅は 256 バイトの倍数に切り上がる（128ピクセル×4バイト＝512 はそのまま、1ピクセルの段は 4→256）。だから画像の行を1行ずつ、切り上がった幅の位置へ写す。
- ミップマップの段は小さくなっていくので、段ごとに同じことをする。

#### E-5. `Editor/History/EditorHistory.h`・`EditorHistory.cpp`（履歴が動いた回数）

保存していない印（`*`）を出すために、「前に比べたときから Entity が変わったか」を安く知りたい。Entity を変える操作は全部 EditorHistory を通るので、**履歴が動いた回数**を数えておき、回数が変わったフレームだけ比べ直す（パート F の `UpdateDirty`）。

`EditorHistory.h`（2か所）

```cpp
// ===== 前（1か所目）=====
#pragma once
#include <string>

// ===== 後 =====
#pragma once
#include <cstdint>
#include <string>
```

```cpp
// ===== 前（2か所目）=====
	// シーンの切り替え・終了で呼ぶ。履歴を捨てる（コピーした物は残すので、Stopの後にも貼れる）
	static void Clear();

// ===== 後 =====
	// シーンの切り替え・終了で呼ぶ。履歴を捨てる（コピーした物は残すので、Stopの後にも貼れる）
	static void Clear();
	// 履歴が動いた回数（積んだ・戻した・やり直した・捨てた）。前に見たときと同じなら、Entity は変わっていない
	// SceneManager が「保存していない変更があるか（*）」を調べ直すかどうかに使う
	static uint64_t GetChangeCount();
```

`EditorHistory.cpp`（6か所。`++changeCount;` を足していく）

```cpp
// ===== 前（1か所目：履歴の変数）=====
std::vector<std::function<void()>> requests; // 次のFlushで実行する操作（頼まれた順）

// ===== 後 =====
std::vector<std::function<void()>> requests; // 次のFlushで実行する操作（頼まれた順）
uint64_t changeCount = 0;                    // 履歴が動いた回数（積んだ・戻した・やり直した・捨てた）。GetChangeCount で渡す
```

```cpp
// ===== 前（2か所目：Append の最後）=====
	cursor = history.size();
}

// ===== 後 =====
	cursor = history.size();
	++changeCount;
}
```

```cpp
// ===== 前（3か所目：Failed）=====
void Failed() {
	history.clear();
	cursor = 0;

// ===== 後 =====
void Failed() {
	history.clear();
	cursor = 0;
	++changeCount;
```

```cpp
// ===== 前（4か所目：Clear）=====
void EditorHistory::Clear() {
	history.clear();
	cursor = 0;
	requests.clear();
	pendingEdits.clear();
}

// ===== 後 =====
void EditorHistory::Clear() {
	history.clear();
	cursor = 0;
	requests.clear();
	pendingEdits.clear();
	++changeCount;
}

uint64_t EditorHistory::GetChangeCount() { return changeCount; }
```

```cpp
// ===== 前（5か所目：RequestUndo の中）=====
		if (undo()) {
			--cursor;

// ===== 後 =====
		if (undo()) {
			--cursor;
			++changeCount;
```

```cpp
// ===== 前（6か所目：RequestRedo の中）=====
		if (redo()) {
			++cursor;

// ===== 後 =====
		if (redo()) {
			++cursor;
			++changeCount;
```

#### E-6. `Scene/SceneSerializer.h`・`SceneSerializer.cpp`（中身が空のシーンファイル＝まだ保存していない新しいシーン）

Project ウィンドウの Create → Scene は、**中身が空のファイル**を作る（エディタで開いていないシーンの Entity は作れないので）。空のファイルを「無い」と同じに扱えば、開いたときに `CreateDefaultEntities`（カメラと平行光源）から始まり、Save で中身が入る。

`SceneSerializer.h`（1か所）

```cpp
// ===== 前 =====
	NotFound, // ファイルが無い（まだ一度も保存していないシーン）

// ===== 後 =====
	NotFound, // ファイルが無い・中身が空（まだ一度も保存していないシーン）
```

`SceneSerializer.cpp`（1か所。LoadFile の頭）

```cpp
// ===== 前 =====
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		return SceneLoadResult::NotFound;
	}

// ===== 後 =====
	std::error_code error;
	// 無い・中身が空（Project ウィンドウで作ったばかりの、まだ保存していないシーン）なら、最初の配置から作ってもらう
	if (!std::filesystem::exists(path, error) || std::filesystem::file_size(path, error) == 0) {
		return SceneLoadResult::NotFound;
	}
```

---

### パート F：シーンを作る・開く・保存していない印

#### F-1. `Scene/SceneManager.h`（全体を差し替え）

足した物：`GetSceneNames`・`CreateSceneFile`・`RequestOpenScene`・`RequestSaveAs`・`IsDirty`・`GetMain`・`GetLastOpenedScene`（と private の関数・変数）。`SaveImmediate` は `bool` を返すようにした。

```cpp
#pragma once
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Scene/IScene.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// 実行状態
enum class PlayState {
	Editing, // 停止中。Updateを回さない
	Playing, // 再生中
	Paused,  // 一時停止
};


/// <summary>
/// シーン一括管理クラス
/// <para>Entityを作り直す・シーンを切り替える・保存する・開くのは、全部 Update の頭（フレームの境目）で行う</para>
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
	// resources/scenes/ にあるシーンの名前（名前の順）
	static std::vector<std::string> GetSceneNames();

	// ===== シーンファイルを作る・開く（エディタから。開く・名前を付けて保存は停止中だけ）=====
	// resources/scenes/<name>.scene.json を中身が空で作る（開くと CreateDefaultEntities のカメラと平行光源から始まる）
	// 名前に使えない文字がある（FileNameRule.h）・同じ名前がもうある・作れなかったら、Log に出して false
	static bool CreateSceneFile(const std::string& name);
	// 次のフレームの頭で、name のシーンを開く（保存していない変更は消える。先に聞くのはエディタの SceneDialogs）
	void RequestOpenScene(const std::string& name) { openRequestName_ = name; }
	// 次のフレームの頭で、今の Entity を name のシーンとして保存し、そのシーンを開き直す（名前を付けて保存）
	void RequestSaveAs(const std::string& name) { saveAsRequestName_ = name; }
	// 保存した（開いた）時点から Entity が変わっているか（Control・Project ウィンドウの *）
	bool IsDirty() const { return isDirty_; }

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
	// エディタが編集しているシーンマネージャ（最初に作ったウィンドウの物）。Project ウィンドウ・File メニューから使う
	static SceneManager* GetMain() { return main_; }

	// ===== エディタで最後に開いていたシーン（次の起動でそれを開く。Unity と同じ）=====
	// 覚えているシーンがまだあればその名前、無ければ fallback
	static std::string GetLastOpenedScene(const std::string& fallback);

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
	// シーンファイルに書く。書けたら true
	bool SaveImmediate();
	// 開いているシーンを name に替える（Stop・Restart でもこのシーンに戻るようになる）
	void OpenImmediate(const std::string& name);
	// 今の Entity を name のシーンファイルに書いて、そのシーンを開き直す
	void SaveAsImmediate(const std::string& name);
	// 停止中か（シーンファイルを書く・開くのは停止中だけ）。違えば what を Log に出して false
	bool CanEditFiles(const std::string& what) const;
	// 今の Entity を「保存した状態」として覚える（開いた・保存した直後）
	void MarkSaved();
	// 保存した状態と今の Entity を比べ直す（履歴が動いたフレームだけ）
	void UpdateDirty();

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

	// ===== エディタから頼まれたこと（次のフレームの頭で行う）=====
	std::string openRequestName_;   // 開くシーン（RequestOpenScene）
	std::string saveAsRequestName_; // 名前を付けて保存するシーン（RequestSaveAs）

	// ===== 保存していない変更（*）=====
	std::string savedSceneText_;      // 保存した（開いた）時点の全 Entity（SaveToText の形）。今と比べる
	uint64_t checkedChangeCount_ = 0; // 最後に比べたときの EditorHistory の変更回数（同じなら比べ直さない）
	bool isDirty_ = false;

	// ===== LoadScene の行き先（static 関数からは this が無いので、ここで覚える）=====
	static SceneManager* updating_; // 今 Update しているシーンマネージャ（Update の中だけ入っている）
	static SceneManager* main_;     // 最初に作ったシーンマネージャ（Update の外で LoadScene を呼ばれたときの行き先）
};
```

#### F-2. `Scene/SceneManager.cpp`（全体を差し替え）

```cpp
#include "SceneManager.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <string_view>
#include <utility>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Scene/DefaultScene.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/String/FileNameRule.h"
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
// エディタで最後に開いていたシーンを覚えておくファイル（エディタだけの設定。製品版では読まない）
constexpr const char* kLastSceneFile = "Parameters/Editor/LastScene.json";

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

// 最後に開いていたシーンの名前を書く（次の起動で GetLastOpenedScene が読む）
void SaveLastOpenedScene(const std::string& name) {
#ifdef USE_IMGUI
	if (name.empty()) {
		return; // 名前の無い前の書き方のシーンは覚えない
	}
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kLastSceneFile).parent_path(), error);
	std::ofstream stream(kLastSceneFile);
	stream << nlohmann::json{{"scene", name}}.dump(4) << '\n';
#else
	(void)name; // 製品版は覚えない
#endif
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
	MarkSaved();
	SaveLastOpenedScene(openSceneName_);
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");
	// この Update の間に呼ばれた LoadScene（シーンの Update・SYSTEM）は、このウィンドウのシーンを切り替える
	updating_ = this;

	// ===== フレームの頭：Entityを作り直す・保存する・開くのはここだけ =====
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
	bool isSaveFailed = false;
	if (isSaveRequested_) {
		isSaveRequested_ = false;
		isSaveFailed = !SaveImmediate();
	}
	// 名前を付けて保存・開くは、保存の後（「保存してから開く」を同じフレームで頼まれたら、保存した後に開く）
	if (!saveAsRequestName_.empty()) {
		SaveAsImmediate(std::exchange(saveAsRequestName_, std::string()));
	}
	if (!openRequestName_.empty()) {
		const std::string name = std::exchange(openRequestName_, std::string());
		if (isSaveFailed) {
			// 保存できなかったのに開くと、編集が消えてしまう
			LogManager::Warning(std::format("保存できなかったので、{} を開くのをやめました", name));
		} else {
			OpenImmediate(name);
		}
	}
#ifdef USE_IMGUI
	UpdateDirty();
#endif

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

std::vector<std::string> SceneManager::GetSceneNames() {
	std::vector<std::string> names;
	std::error_code error;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(kSceneFolder, error)) {
		const std::string file = entry.path().filename().string();
		if (entry.is_regular_file(error) && file.ends_with(kSceneFileExtension)) {
			names.push_back(SceneNameFromFile(file));
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

//======================================================================================================
// シーンファイルを作る（エディタから）
//======================================================================================================
bool SceneManager::CreateSceneFile(const std::string& name) {
	if (!IsValidFileName(name)) {
		LogManager::Warning(std::format("シーンの名前に使えるのは、英数字と _ - だけです: \"{}\"", name));
		return false;
	}
	const std::filesystem::path path = MakeSceneFilePath(name);
	std::error_code error;
	if (std::filesystem::exists(path, error)) {
		LogManager::Warning(std::format("同じ名前のシーンがもうあります: {}", path.generic_string()));
		return false;
	}
	std::filesystem::create_directories(path.parent_path(), error);
	// 中身が空のファイル＝まだ保存していない新しいシーン（開くと CreateDefaultEntities から始まる。SceneSerializer::LoadFile）
	std::ofstream stream(path);
	if (!stream) {
		LogManager::Error(std::format("シーンファイルを作れませんでした: {}", path.generic_string()));
		return false;
	}
	LogManager::Log(std::format("シーンを作りました: {}", path.generic_string()));
	return true;
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
		LogManager::Log(std::format("シーンファイルが無い（空）ので、CreateDefaultEntities() で作ります（Save するとファイルができます）: {}", file));
#else
		// 製品版でファイルが無いのは、入れ忘れの可能性が高い
		LogManager::Warning(std::format("シーンファイルが無い（空）ので、CreateDefaultEntities() で作ります（resources に入れ忘れていませんか）: {}", file));
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
// 保存・開く（エディタから。フレームの頭でだけ呼ぶ）
//======================================================================================================
bool SceneManager::CanEditFiles(const std::string& what) const {
	// 再生中に保存すると、動いた後の状態がファイルに残ってしまう（Unityも再生中は保存できない）
	// 退避を持っている間＝まだ Play 中の Entity のまま（Stop は次の作り直しで戻る）なので、それも断る
	if (playState_ != PlayState::Editing || !playSnapshot_.empty()) {
		LogManager::Warning(std::format("再生中は{}できません。Stop してからにしてください", what));
		return false;
	}
	return true;
}

bool SceneManager::SaveImmediate() {
	const char* file = GetSceneFile();
	if (file == nullptr) {
		LogManager::Warning("このシーンはシーンファイルを使っていないので保存できません（IScene::GetSceneFile）");
		return false;
	}
	if (!CanEditFiles("保存")) {
		return false;
	}
	if (!SceneSerializer::SaveFile(file)) {
		return false;
	}
	MarkSaved(); // ここが「保存した状態」になる（* が消える）
	return true;
}

void SceneManager::OpenImmediate(const std::string& name) {
	if (!CanEditFiles("シーンを開くことは")) {
		return;
	}
	if (!HasScene(name)) {
		LogManager::Warning(std::format("シーンがありません: {}", MakeSceneFilePath(name)));
		return;
	}
	AudioSystem::StopAll(); // Inspector で試し聞きしていた音を止める
#ifdef USE_IMGUI
	HierarchyWindow::SetSelected({}); // 前のシーンの Entity の番号で、別のシーンの Entity を選ばないように外しておく
#endif
	// これからは Stop・Restart でもこのシーンを作り直す
	openSceneFactory_ = [name] { return CreateScene(name); };
	ChangeScene(openSceneFactory_(), false);
	openSceneName_ = name;
	MarkSaved();
	SaveLastOpenedScene(name);
	LogManager::Log(std::format("シーンを開きました: {}", name));
}

void SceneManager::SaveAsImmediate(const std::string& name) {
	if (!CanEditFiles("名前を付けて保存")) {
		return;
	}
	if (!IsValidFileName(name)) {
		LogManager::Warning(std::format("シーンの名前に使えるのは、英数字と _ - だけです: \"{}\"", name));
		return;
	}
	if (!SceneSerializer::SaveFile(MakeSceneFilePath(name))) {
		return;
	}
	// 書いたファイルから開き直す（クラスも名前で決まり直す。REGISTER_SCENE していない名前なら DefaultScene）
	OpenImmediate(name);
}

//======================================================================================================
// 保存していない変更（*）
//======================================================================================================
void SceneManager::MarkSaved() {
#ifdef USE_IMGUI
	savedSceneText_ = (GetSceneFile() != nullptr) ? SceneSerializer::SaveToText() : std::string();
	checkedChangeCount_ = EditorHistory::GetChangeCount();
	isDirty_ = false;
#endif
}

void SceneManager::UpdateDirty() {
#ifdef USE_IMGUI
	// 再生中は比べない（動いている Entity は保存する物ではない。Stop で戻った後に、また比べる）
	if (playState_ != PlayState::Editing || !playSnapshot_.empty() || GetSceneFile() == nullptr) {
		return;
	}
	// Entity を変える操作は全部 EditorHistory を通るので、履歴が動いていなければ Entity も変わっていない
	const uint64_t changeCount = EditorHistory::GetChangeCount();
	if (changeCount == checkedChangeCount_) {
		return;
	}
	checkedChangeCount_ = changeCount;
	// 保存した時点の中身と比べる（Undo で保存した時点まで戻したら、同じになって * が消える）
	isDirty_ = (SceneSerializer::SaveToText() != savedSceneText_);
#endif
}

//======================================================================================================
// 最後に開いていたシーン
//======================================================================================================
std::string SceneManager::GetLastOpenedScene(const std::string& fallback) {
	std::ifstream stream(kLastSceneFile);
	if (!stream) {
		return fallback; // まだ覚えていない（初めての起動）
	}
	const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false); // false: 壊れていても例外を投げない
	const auto it = json.is_object() ? json.find("scene") : json.end();
	const std::string name = (it != json.end() && it->is_string()) ? it->get<std::string>() : std::string();
	// 消した・名前を変えたシーンなら、fallback から始める
	return HasScene(name) ? name : fallback;
}
```

**解説（SceneManager で増えた所）**

- **開く（`OpenImmediate`）**：`openSceneFactory_`（Stop・Restart で作り直すときの作り方）を「name のシーンを作る」に替えてから `ChangeScene` する。これで Play → Stop もそのシーンに戻る。前のシーンの選択（Hierarchy）は外す（Entity の番号が同じでも、別のシーンでは別物なので）。
- **フレームの頭の順番**：作り直し → `LoadScene` → **保存 → 名前を付けて保存 → 開く**。「保存しますか？ → Save」は `RequestSave` と `RequestOpenScene` を同じフレームに頼むので、この順番で「保存してから開く」になる。保存に失敗したら開かない（開くと編集が消えてしまう）。
- **保存していない印（`*`）**：
  1. 開いた・保存した直後の全 Entity を `SceneSerializer::SaveToText()` で文字にして覚える（`savedSceneText_`。Play のときの退避と同じ形）。
  2. 毎フレーム、`EditorHistory::GetChangeCount()` が前と同じなら何もしない（Entity を変える操作は全部履歴を通るので、履歴が動いていなければ変わっていない）。
  3. 動いていたら、今の全 Entity を文字にして、覚えた文字と比べる。**違えば `*`**。
  - 「変更したか」ではなく「保存した時と同じか」で決めるので、**Undo で保存した時点まで戻すと `*` が消える**（Unity と同じ）。
  - 再生中は比べない（動いている Entity は保存する物ではない）。Stop で戻ったら比べ直す。
- **最後に開いたシーン**：開くたびに `Parameters/Editor/LastScene.json` に名前を書く（エディタだけの設定なので `Parameters/Editor/` の下。SF2.5 の決まり）。

#### F-3. `Engine.h`・`Engine.cpp`（エディタは最後に開いたシーンから始める）

`Engine.h`（1か所。説明だけ）

```cpp
// ===== 前 =====
	/// <para>REGISTER_SCENE(名前) したクラスがあればそれ、無ければ DefaultScene（Entity だけで動くシーン）で開く</para>
	/// </summary>
	/// <param name="config">ウィンドウ設定(サイズ・タイトル・ImGui設定・アスペクト比など)</param>
	/// <param name="firstScene">最初のシーンの名前（例: "Title"）</param>

// ===== 後 =====
	/// <para>REGISTER_SCENE(名前) したクラスがあればそれ、無ければ DefaultScene（Entity だけで動くシーン）で開く</para>
	/// <para>エディタ（Debug）は、前回最後に開いていたシーンから始まる（覚えていなければ firstScene）</para>
	/// </summary>
	/// <param name="config">ウィンドウ設定(サイズ・タイトル・ImGui設定・アスペクト比など)</param>
	/// <param name="firstScene">製品版で最初に出るシーンの名前（例: "Title"）</param>
```

`Engine.cpp`（1か所）

```cpp
// ===== 前 =====
void Engine::Initialize(const WindowConfig& config, const std::string& firstScene) {
	// 名前からシーンを作る「作り方」を渡す（Stop・Restart でも、この作り方で開いているシーンを作り直す）
	Initialize(config, [firstScene] { return SceneManager::CreateScene(firstScene); });
}

// ===== 後 =====
void Engine::Initialize(const WindowConfig& config, const std::string& firstScene) {
#ifdef USE_IMGUI
	// エディタは、前回最後に開いていたシーンから始める（Unity と同じ。覚えていなければ firstScene）
	const std::string sceneName = SceneManager::GetLastOpenedScene(firstScene);
#else
	// 製品版は、いつも firstScene から
	const std::string sceneName = firstScene;
#endif
	// 名前からシーンを作る「作り方」を渡す（Stop・Restart でも、この作り方で開いているシーンを作り直す）
	Initialize(config, [sceneName] { return SceneManager::CreateScene(sceneName); });
}
```

#### F-4. `Window/WindowManager.cpp`（Control ウィンドウの名前に `*`。1か所）

```cpp
// ===== 前（DrawPlayToolbar の最後の方）=====
	const std::string currentName = scene ? scene->GetSceneName() : std::string();
	if (currentName == openName) {
		ImGui::TextDisabled("Scene: %s  (%s)", openName.c_str(), sceneFile ? sceneFile : "シーンファイルを使っていない"); // どのファイルに保存されるか
	} else {
		ImGui::TextDisabled("Scene: %s → 今は %s（Stop で %s に戻ります）", openName.c_str(), currentName.c_str(), openName.c_str());
	}

// ===== 後 =====
	const std::string currentName = scene ? scene->GetSceneName() : std::string();
	const char* dirtyMark = sceneManager->IsDirty() ? " *" : ""; // 保存していない変更がある印
	if (currentName == openName) {
		ImGui::TextDisabled("Scene: %s%s  (%s)", openName.c_str(), dirtyMark, sceneFile ? sceneFile : "シーンファイルを使っていない"); // どのファイルに保存されるか
	} else {
		ImGui::TextDisabled("Scene: %s%s → 今は %s（Stop で %s に戻ります）", openName.c_str(), dirtyMark, currentName.c_str(), openName.c_str());
	}
```

---

### パート G：Settings ウィンドウ（SF2.5）

#### G-1. `UI/GlobalVariables.h`（全体を差し替え）

変えた所：

| 何を | どう |
|---|---|
| グループの置き場所 | `enum class SettingsSection { Project, Editor }`。`Group("名前", SettingsSection::Editor)` で Editor の見出しへ（保存先も `Parameters/Editor/<名前>/`）。**ゲームのコードは今のまま**（省略すると Project） |
| 保存した値 | 項目ごとに「保存した値（`saved`）」を持つ。今の値と違えば `*` |
| 登録したときにファイルから読む | `Add` の中で、保存したファイルにその項目があればその値にする（下の「見つけたこと」） |
| 描く所 | 窓を作らない。`DrawGroup`（1ページの中身）を Settings ウィンドウが呼ぶ。保存完了のポップアップは無くした（`*` が消えるので分かる） |
| 消した物 | `Update()`（→ Settings ウィンドウ）・`DrawSavedPopup`・`savedMessage_` |

```cpp
#pragma once
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <iomanip>
#include <filesystem>
#include <type_traits>
#include <externals/imgui/imgui.h>
#include <externals/nlohmann/json.hpp>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Diagnostics//MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"

// 調整項目のグループの置き場所（Settings ウィンドウの見出しと、保存先のフォルダが決まる）
enum class SettingsSection {
	Project, // ゲーム全体の設定。Parameters/<グループ名>/ に保存。製品版にも入る
	Editor,  // エディタだけの設定（Scene ビューのカメラなど）。Parameters/Editor/<グループ名>/ に保存。製品版では使わない
};


/// <summary>
/// 調整項目の管理
/// <para>表示の階層は Settings(ウィンドウ) → グループ(左の一覧の1ページ) → カテゴリ(入れ子可) → アイテム。</para>
/// <para>保存はカテゴリ単位。Parameters/&lt;グループ名&gt;/&lt;カテゴリ名&gt;.json に書き出す（エディタだけのグループは Parameters/Editor/ の下）。</para>
/// <para>アイテムを登録したときに、保存したファイルに値があればその値になる（シーンを切り替えて後から登録しても、保存した値から始まる）</para>
/// </summary>
class GlobalVariables {
public:
    // ImGui::Combo用
    struct ComboItem {
        std::vector<std::string> options; // 選択肢リスト
        int currentIndex = 0;             // 現在の選択インデックス

		bool operator==(const ComboItem&) const = default; // 変わったかを比べる（Undo用）
    };

    // ImGui::ColorEdit4用
    struct ColorItem {
        uint32_t rgba = 0xFFFFFFFF;

		bool operator==(const ColorItem&) const = default;
    };

    using Item     = std::variant<bool, int32_t, float, Vector2, Vector3, Vector4, ComboItem, ColorItem>;
    using json     = nlohmann::json;
    // ツリーノード
    struct GVNode {
		struct Entry {
			std::string name;
			std::variant<Item, std::shared_ptr<GVNode>> value;
			Item saved; // アイテムのとき：保存した値（ファイルから読んだ・ファイルへ書いた値。ファイルに無ければ登録した値）。今の値と違えば * を出す
		};
		std::vector<Entry> entries_;
        // 子ノードを名前で検索
		GVNode* FindChild(const std::string& name) {
			for (auto& e : entries_) {
				if (e.name == name && std::holds_alternative<std::shared_ptr<GVNode>>(e.value)) {
					return std::get<std::shared_ptr<GVNode>>(e.value).get();
				}
			}
			return nullptr;
		}
        // 子ノードを名前で取得
		GVNode* GetOrCreateChild(const std::string& name) {
			if (GVNode* found = FindChild(name)) {
				return found;
			}
			entries_.push_back({name, std::make_shared<GVNode>()});
			return std::get<std::shared_ptr<GVNode>>(entries_.back().value).get();
		}
        // アイテムを名前で検索
		Item* FindItem(const std::string& name) {
			for (auto& e : entries_) {
				if (e.name == name && std::holds_alternative<Item>(e.value)) {
					return &std::get<Item>(e.value);
				}
			}
			return nullptr;
		}
    };

    // ===== カテゴリの追加 =====
    class CategoryBuilder {
	public:
		CategoryBuilder(GVNode* node, const std::string& groupName, const std::string& categoryPath) : node_(node), groupName_(groupName), categoryPath_(categoryPath) {}

        /// <summary>
		/// 子カテゴリに移動する。なければ作成する。
		/// <para>例: .Category("A").Category("B").Add(...)</para>
		/// </summary>
		CategoryBuilder Category(const std::string& categoryName) { return CategoryBuilder(node_->GetOrCreateChild(categoryName), groupName_, categoryPath_ + "/" + categoryName); }

		/// <summary>旧名。Category() と同じ。</summary>
		CategoryBuilder Group(const std::string& categoryName) { return Category(categoryName); }

        /// <summary>
		/// 項目を追加する。すでに登録済みの場合はスキップする（値を上書きしない）。
		/// <para>保存したファイルにこの項目の値があれば、value ではなくファイルの値になる</para>
		/// <para>例: .Add("X", x).Add("Y", y)</para>
		/// </summary>
		template<typename T> CategoryBuilder& Add(const std::string& itemName, const T& value) {
			if (!node_->FindItem(itemName)) {
				Item item(value);
				// 保存した値から始める（登録がエンジンの起動より後＝シーンを切り替えたときでも、ファイルの値になるように）
				GetInstance()->ReadSavedItem(groupName_, categoryPath_, itemName, item);
				node_->entries_.push_back({itemName, item, item}); // 今の値・保存した値（同じなので * は付かない）
				LogManager::Log(groupName_ + " / " + itemName);
			}
			return *this;
		}

    private:
		GlobalVariables::GVNode* node_;
		std::string groupName_;
		std::string categoryPath_; // グループの下からのカテゴリのパス（"Enemy/Transform"）。1つ目がファイルの名前になる
    };

    // ===== グループのルート =====
	class GroupRoot {
	public:
		GroupRoot(GVNode* root, const std::string& groupName) : root_(root), groupName_(groupName) {}

		/// <summary>
		/// カテゴリを取得または作成して CategoryBuilder を返す。
		/// <para>ここで作られたカテゴリが &lt;カテゴリ名&gt;.json の単位になる。</para>
		/// </summary>
		CategoryBuilder Category(const std::string& categoryName) { return CategoryBuilder(root_->GetOrCreateChild(categoryName), groupName_, categoryName); }

		/// <summary>旧名。Category() と同じ。</summary>
		CategoryBuilder Group(const std::string& categoryName) { return Category(categoryName); }

	private:
		GVNode* root_;
		std::string groupName_;
	};

	// 旧名エイリアス（既存コードの互換用）
	using GroupBuilder = CategoryBuilder;
	using SceneBuilder = GroupRoot;

	// シングルトン
	static GlobalVariables* GetInstance();

	// コピー禁止
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	/// <summary>
	/// グループを宣言する。（Settings ウィンドウの1ページ分）
	/// <para>既に存在する場合はそのグループのルートノードを返す（section は最初に宣言したときのまま）。</para>
	/// </summary>
	/// <param name="groupName">グループ名。保存先フォルダ名にもなる</param>
	/// <param name="section">置き場所。ゲームのグループは省略してよい（Project）</param>
	GroupRoot Group(const std::string& groupName, SettingsSection section = SettingsSection::Project);

	/// <summary>
	/// 値を取得する。カテゴリのパスは "/" 区切りで指定する。
	/// 例: Get&lt;Vector3&gt;("GameScene", "Enemy/Transform", "Position") → GameScene → Enemy → Transform → Position の値を返す
	/// </summary>
	/// <param name="groupName">グループ名</param>
	/// <param name="categoryPath">"/" 区切りのカテゴリパス（例: "Enemy/Transform"）</param>
	/// <param name="itemName">項目名</param>
	template<typename T>
	T Get(const std::string& groupName, const std::string& categoryPath, const std::string& itemName) const {
		const GVNode* node = FindNode(groupName, categoryPath);
		if (!node) {
			LogManager::Error("ノードが見つかりません: " + groupName + " / " + categoryPath);
			MY_ASSERT_MSG(false, "ノードが見つかりません");
		}
		for (const auto& e : node->entries_) {
			if (e.name == itemName && std::holds_alternative<Item>(e.value)) {
				return std::get<T>(std::get<Item>(e.value));
			}
		}
		LogManager::Error("アイテムが見つかりません: " + itemName);
		MY_ASSERT_MSG(false, "アイテムが見つかりません");
		return T{};
	}

	/// <summary>
	/// 登録済みアイテムの値を強制上書きする。
	/// 未登録の場合は何もしない。
	/// ウィンドウ追加後のComboリセット等、毎フレームでなく
	/// 特定タイミングで値を書き換えたいときに使う。
	/// </summary>
	template<typename T>
	void Set(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const T& value) {
		GVNode* node = FindNode(groupName, categoryPath);
		if (!node) {
			return;
		}

		Item* item = node->FindItem(itemName);
		if (!item) {
			return;
		}

		*item = Item(value);
	}

	/// <summary>enum型 E から ComboItem を自動生成する（optionsを手入力せずに済む）</summary>
	template<typename E> static ComboItem MakeEnumCombo(E current) {
		static_assert(std::is_enum_v<E>, "MakeEnumCombo は enum 専用です");
		ComboItem combo;
		for (std::string_view name : magic_enum::enum_names<E>()) {
			combo.options.emplace_back(name); // 識別子名をそのまま選択肢に
		}
		combo.currentIndex = static_cast<int>(magic_enum::enum_index(current).value_or(0));
		return combo;
	}

	/// <summary>ComboItem の選択インデックスを enum E に戻す</summary>
	template<typename E> static E GetEnumCombo(const ComboItem& combo) {
		static_assert(std::is_enum_v<E>, "GetEnumCombo は enum 専用です");
		size_t idx = static_cast<size_t>(combo.currentIndex);
		if (idx >= magic_enum::enum_count<E>()) { // JSONに古いindexが残っていた場合の保険
			idx = 0;
		}
		return magic_enum::enum_value<E>(idx);
	}

	// ===== 保存 =====

	/// <summary>
	/// カテゴリ1つを Parameters/&lt;グループ名&gt;/&lt;カテゴリ名&gt;.json に書き出す。
	/// </summary>
	void SaveCategory(const std::string& groupName, const std::string& categoryName);

	/// <summary>
	/// グループ内の全カテゴリを一括保存する。（カテゴリごとに1ファイル）
	/// </summary>
	void SaveGroup(const std::string& groupName);

	/// <summary>
	/// 登録済み全グループを一括保存する。
	/// </summary>
	void SaveAll();

	/// <summary>旧名。SaveGroup() と同じ。</summary>
	void SaveFile(const std::string& groupName) { SaveGroup(groupName); }

	// ===== 読み込み =====

	/// <summary>
	/// カテゴリ1つ分のjsonを読み込む。ファイルがなければスキップ。
	/// </summary>
	void LoadCategory(const std::string& groupName, const std::string& categoryName);

	/// <summary>
	/// グループ1つ分（フォルダ内の全カテゴリ）を読み込む。
	/// </summary>
	void LoadGroup(const std::string& groupName);

	/// <summary>
	/// 登録済みの全グループを、ファイルから読み直す。
	/// </summary>
	void LoadFiles();

#ifdef USE_IMGUI
	// ===== Settings ウィンドウから使う =====
	// section に入っているグループの名前（登録した順）
	std::vector<std::string> GetGroupNames(SettingsSection section) const;
	// 保存していない変更があるか（保存した値と違う項目が1つでもある）
	bool IsGroupDirty(const std::string& groupName) const;
	// 保存先のフォルダ（表示用。"Parameters/PostEffect/"）
	std::string GetSaveDirectory(const std::string& groupName) const;
	// 保存した値（ファイルの値。まだ保存していなければ登録した値）に戻す。Ctrl+Z で戻す前に戻せる
	void RevertGroup(const std::string& groupName);
	// グループ1つ分の中身（カテゴリと項目）を描く
	void DrawGroup(const std::string& groupName);
	// 描き終わったら毎フレーム呼ぶ。どの項目も触っていなければ、ここまでの変更を1件の履歴にする
	void EndDraw();
#endif

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;

	// グループ1つ分
	struct GroupData {
		std::string name;
		SettingsSection section = SettingsSection::Project;
		GVNode root;
	};

	// ===== 内部ヘルパー =====
	const GroupData* FindGroup(const std::string& groupName) const;
	GroupData* FindGroup(const std::string& groupName);
	const GVNode* FindNode(const std::string& groupName, const std::string& categoryPath) const;
	GVNode* FindNode(const std::string& groupName, const std::string& categoryPath);
	GVNode* FindGroupRoot(const std::string& groupName);
	void NodeToJson(const GVNode& node, json& out) const;
	void JsonToNode(const json& json, GVNode& node);
	// アイテム1つ ↔ jsonの値（型が合わない値は入れずに false）
	static json ItemToJson(const Item& item);
	static bool JsonToItem(const json& value, Item& item);
	// 保存したファイルから、これから登録するアイテム1つの値を読む（ファイル・項目が無ければ item はそのまま）
	void ReadSavedItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item) const;
	// ノードの中の全アイテムを「保存した」にする（ファイルへ書けた後）
	static void MarkSaved(GVNode& node);
	// ノードの中に、保存した値と違うアイテムがあるか
	static bool IsDirty(const GVNode& node);
	// jsonファイルを out に読み込む。ファイルなし・パース失敗なら false
	bool ReadJsonFile(const std::filesystem::path& filePath, json& out) const;
	// ノードの中身をそのままjsonルートに書き出す
	bool WriteNodeToFile(const std::filesystem::path& filePath, const GVNode& node) const;
	// フォルダ内の <カテゴリ名>.json を全部読む
	void LoadGroupCategories(const std::string& groupName);
	// 保存先パス（エディタだけのグループは Parameters/Editor/ の下）
	std::filesystem::path GroupDirectory(const std::string& groupName) const;
	std::filesystem::path CategoryFilePath(const std::string& groupName, const std::string& categoryName) const { return GroupDirectory(groupName) / (categoryName + ".json"); }
	// 項目1つの値を書き換える。見つからなければ false（Undo・Redo から使う）
	bool SetItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& value);
#ifdef USE_IMGUI
	// categoryPath は "/" 区切りのカテゴリ（グループ直下なら空）
	void DrawNode(GVNode& node, const std::string& groupName, const std::string& categoryPath);
	void DrawItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item);
	// 保存した値と違うアイテムを、保存した値に戻す（戻した物は履歴に積む）
	void RevertNode(GVNode& node, const std::string& groupName, const std::string& categoryPath);

	// ===== Undo・Redo（Inspector と同じく、マウスを離すまでの変更を1件にまとめる）=====
	struct PendingEdit {
		std::string groupName;
		std::string categoryPath;
		std::string itemName;
		Item before; // 触り始める前の値
		Item after;  // 最後に変えた値
	};
	void RecordEdit(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& before, const Item& after);
	void CommitEdits(); // ここまでの変更を EditorHistory に積む

	std::vector<PendingEdit> pendingEdits_; // まだ履歴に積んでいない変更
#endif

	std::vector<GroupData> groups_;
	const std::string kDirectoryPath = "Parameters/";              // ゲーム全体の設定の保存先
	const std::string kEditorDirectoryPath = "Parameters/Editor/"; // エディタだけの設定の保存先
};
```

#### G-2. `UI/GlobalVariables.cpp`（全体を差し替え）

```cpp
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <format>
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Diagnostics/LogManager.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#endif

namespace {
// "Enemy/Transform" → {"Enemy", "Transform"}（空の段は飛ばす）
std::vector<std::string> SplitCategoryPath(const std::string& categoryPath) {
	std::vector<std::string> names;
	std::istringstream ss(categoryPath);
	std::string token;
	while (std::getline(ss, token, '/')) {
		if (!token.empty()) {
			names.push_back(token);
		}
	}
	return names;
}

// 数が count 個の配列か（Vector2〜4 の値）
bool IsNumberArray(const nlohmann::json& value, size_t count) {
	if (!value.is_array() || value.size() != count) {
		return false;
	}
	for (const nlohmann::json& element : value) {
		if (!element.is_number()) {
			return false;
		}
	}
	return true;
}
} // namespace

//=============================================================================
// シングルトン
//=============================================================================
GlobalVariables* GlobalVariables::GetInstance() {
	static GlobalVariables instance;
	return &instance;
}

//=============================================================================
// グループを宣言する
// 既に存在する場合はそのルートノードをそのまま返す（データをリセットしない）。
//=============================================================================
GlobalVariables::GroupRoot GlobalVariables::Group(const std::string& groupName, SettingsSection section) {
	// 既存グループを検索
	if (GroupData* group = FindGroup(groupName)) {
		return GroupRoot(&group->root, groupName);
	}
	// 新規作成（置き場所はここで決まる。保存先のフォルダも section で変わる）
	groups_.push_back({groupName, section, GVNode{}});
	LogManager::Log("RegisterGroup: " + groupName);
	return GroupRoot(&groups_.back().root, groupName);
}

//=============================================================================
// グループを名前で探す
//=============================================================================
const GlobalVariables::GroupData* GlobalVariables::FindGroup(const std::string& groupName) const {
	for (const GroupData& group : groups_) {
		if (group.name == groupName) {
			return &group;
		}
	}
	return nullptr;
}

GlobalVariables::GroupData* GlobalVariables::FindGroup(const std::string& groupName) {
	// const版に委譲してconst_castで返す
	return const_cast<GroupData*>(static_cast<const GlobalVariables*>(this)->FindGroup(groupName));
}

//=============================================================================
// groupName と "/" 区切りの categoryPath からノードを検索する
//=============================================================================
const GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& groupName, const std::string& categoryPath) const {
	const GroupData* group = FindGroup(groupName);
	if (!group) {
		return nullptr;
	}
	// "/" 区切りでパスを分割して辿る
	const GVNode* node = &group->root;
	for (const std::string& token : SplitCategoryPath(categoryPath)) {
		bool found = false;
		for (const auto& e : node->entries_) {
			if (e.name == token && std::holds_alternative<std::shared_ptr<GVNode>>(e.value)) {
				node = std::get<std::shared_ptr<GVNode>>(e.value).get();
				found = true;
				break;
			}
		}
		if (!found) {
			return nullptr;
		}
	}
	return node;
}

// ===== 非const版 =====
GlobalVariables::GVNode* GlobalVariables::FindNode(const std::string& groupName, const std::string& categoryPath) {
	// const版に委譲してconst_castで返す
	return const_cast<GVNode*>(static_cast<const GlobalVariables*>(this)->FindNode(groupName, categoryPath));
}

//=============================================================================
// グループのルートノードを検索する
//=============================================================================
GlobalVariables::GVNode* GlobalVariables::FindGroupRoot(const std::string& groupName) {
	GroupData* group = FindGroup(groupName);
	return group ? &group->root : nullptr;
}

//=============================================================================
// 保存先のフォルダ（エディタだけのグループは Parameters/Editor/ の下）
//=============================================================================
std::filesystem::path GlobalVariables::GroupDirectory(const std::string& groupName) const {
	const GroupData* group = FindGroup(groupName);
	const bool isEditor = group && group->section == SettingsSection::Editor;
	return std::filesystem::path(isEditor ? kEditorDirectoryPath : kDirectoryPath) / groupName;
}

//=============================================================================
// 項目1つの値を書き換える（Undo・Redo から）
//=============================================================================
bool GlobalVariables::SetItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& value) {
	GVNode* node = FindNode(groupName, categoryPath);
	Item* item = node ? node->FindItem(itemName) : nullptr;
	if (!item || item->index() != value.index()) {
		return false; // 見つからない・型が違う
	}
	*item = value;
	return true;
}

//=============================================================================
// 保存した値（*）
//=============================================================================
// ノードの中の全アイテムを「保存した」にする
void GlobalVariables::MarkSaved(GVNode& node) {
	for (GVNode::Entry& entry : node.entries_) {
		if (const Item* item = std::get_if<Item>(&entry.value)) {
			entry.saved = *item;
		} else {
			MarkSaved(*std::get<std::shared_ptr<GVNode>>(entry.value));
		}
	}
}

// ノードの中に、保存した値と違うアイテムがあるか
bool GlobalVariables::IsDirty(const GVNode& node) {
	for (const GVNode::Entry& entry : node.entries_) {
		if (const Item* item = std::get_if<Item>(&entry.value)) {
			if (!(*item == entry.saved)) {
				return true;
			}
		} else if (IsDirty(*std::get<std::shared_ptr<GVNode>>(entry.value))) {
			return true;
		}
	}
	return false;
}

#ifdef USE_IMGUI
//=============================================================================
// Settings ウィンドウから使う
//=============================================================================
std::vector<std::string> GlobalVariables::GetGroupNames(SettingsSection section) const {
	std::vector<std::string> names;
	for (const GroupData& group : groups_) {
		if (group.section == section) {
			names.push_back(group.name);
		}
	}
	return names;
}

bool GlobalVariables::IsGroupDirty(const std::string& groupName) const {
	const GroupData* group = FindGroup(groupName);
	return group && IsDirty(group->root);
}

std::string GlobalVariables::GetSaveDirectory(const std::string& groupName) const { return GroupDirectory(groupName).generic_string() + "/"; }

void GlobalVariables::RevertGroup(const std::string& groupName) {
	GroupData* group = FindGroup(groupName);
	if (!group) {
		return;
	}
	RevertNode(group->root, groupName, "");
	CommitEdits(); // 戻した分を1件の履歴にする（Ctrl+Z で、戻す前の値にもどせる）
}

void GlobalVariables::RevertNode(GVNode& node, const std::string& groupName, const std::string& categoryPath) {
	for (GVNode::Entry& entry : node.entries_) {
		if (Item* item = std::get_if<Item>(&entry.value)) {
			if (!(*item == entry.saved)) {
				RecordEdit(groupName, categoryPath, entry.name, *item, entry.saved);
				*item = entry.saved;
			}
		} else {
			const std::string childPath = categoryPath.empty() ? entry.name : categoryPath + "/" + entry.name;
			RevertNode(*std::get<std::shared_ptr<GVNode>>(entry.value), groupName, childPath);
		}
	}
}

//=============================================================================
// グループ1つ分（Settings ウィンドウの1ページ）を描画する
// 直下のカテゴリは <カテゴリ名>.json の単位。保存していない変更があれば見出しに * を付ける
//=============================================================================
void GlobalVariables::DrawGroup(const std::string& groupName) {
	GroupData* group = FindGroup(groupName);
	if (!group) {
		return;
	}
	for (GVNode::Entry& entry : group->root.entries_) {
		// カテゴリに属さないアイテムはその場で描画（通常は無い）
		if (Item* item = std::get_if<Item>(&entry.value)) {
			DrawItem(groupName, "", entry.name, *item);
			continue;
		}
		// 「###」の後ろを ImGui の ID にする（* が付いたり消えたりしても、開いた・閉じたが変わらないように）
		GVNode& category = *std::get<std::shared_ptr<GVNode>>(entry.value);
		const std::string label = std::format("{}{}###{}/{}", entry.name, IsDirty(category) ? " *" : "", groupName, entry.name);
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Indent();
			DrawNode(category, groupName, entry.name);
			ImGui::Unindent();
		}
	}
}

void GlobalVariables::EndDraw() {
	// どの項目も触っていない（マウスを離した・入力を確定した）なら、ここまでの変更を1件の履歴にする
	if (!ImGui::IsAnyItemActive()) {
		CommitEdits();
	}
}

//=============================================================================
// ノードを再帰的に描画する
//=============================================================================
void GlobalVariables::DrawNode(GVNode& node, const std::string& groupName, const std::string& categoryPath) {
	for (auto& entry : node.entries_) {
		if (std::holds_alternative<Item>(entry.value)) {
			// ① itemはその場で直接描画
			DrawItem(groupName, categoryPath, entry.name, std::get<Item>(entry.value));

		} else {
			// ② 子カテゴリはCollapsingHeaderを開いて中身を再帰描画
			auto& childNode = std::get<std::shared_ptr<GVNode>>(entry.value);
			std::string childPath = categoryPath + "/" + entry.name;
			std::string headerLabel = entry.name + "##" + groupName + "/" + childPath;

			if (!ImGui::CollapsingHeader(headerLabel.c_str())) {
				continue;
			}
			ImGui::Indent();
			DrawNode(*childNode, groupName, childPath);
			ImGui::Unindent();
		}
	}
}

//=============================================================================
// 1つのアイテムをImGuiで描画する
// ラベル（左）＋ ウィジェット（右）の2列レイアウト
//=============================================================================
void GlobalVariables::DrawItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item) {
	const std::string& label = itemName;
	const std::string uid = "##" + groupName + "/" + categoryPath + "/" + itemName; // ImGuiのID（表示はしない）
	const Item before = item;                                                       // 描く前の値。描いた後と比べて、変わっていたらUndoのために覚える

	std::visit(
	    [&](auto& v) {
		using T = std::decay_t<decltype(v)>;

		    if constexpr (std::is_same_v<T, bool>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::Checkbox(uid.c_str(), &v);

		    } else if constexpr (std::is_same_v<T, int32_t>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragInt(uid.c_str(), &v);

		    } else if constexpr (std::is_same_v<T, float>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SameLine(150.0f);
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat(uid.c_str(), &v, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector2>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat2(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector3>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat3(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, Vector4>) {
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::DragFloat4(uid.c_str(), &v.x, 0.01f);

		    } else if constexpr (std::is_same_v<T, ComboItem>) {
			    std::vector<const char*> cstrs;
			    for (const auto& s : v.options) {
				    cstrs.push_back(s.c_str());
			    }
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    ImGui::Combo(uid.c_str(), &v.currentIndex, cstrs.data(), static_cast<int>(cstrs.size()));

		    } else if constexpr (std::is_same_v<T, ColorItem>) {
			    float col[4];
			    col[0] = ((v.rgba >> 24) & 0xFF) / 255.0f;
			    col[1] = ((v.rgba >> 16) & 0xFF) / 255.0f;
			    col[2] = ((v.rgba >> 8) & 0xFF) / 255.0f;
			    col[3] = (v.rgba & 0xFF) / 255.0f;
			    ImGui::Text("%s", label.c_str());
			    ImGui::SetNextItemWidth(-1);
			    if (ImGui::ColorEdit4(uid.c_str(), col)) {
				    uint32_t r = static_cast<uint32_t>(col[0] * 255.0f);
				    uint32_t g = static_cast<uint32_t>(col[1] * 255.0f);
				    uint32_t b = static_cast<uint32_t>(col[2] * 255.0f);
				    uint32_t a = static_cast<uint32_t>(col[3] * 255.0f);
				    v.rgba = (r << 24) | (g << 16) | (b << 8) | a;
			    }
		    }
	    },
	    item);

	if (!(item == before)) {
		RecordEdit(groupName, categoryPath, itemName, before, item);
	}
}

//=============================================================================
// Undo・Redo
//=============================================================================
// 変わった項目を覚える。同じ項目がもうあれば「変えた後」だけ新しくする（「前」は触り始めたときのまま）
void GlobalVariables::RecordEdit(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& before, const Item& after) {
	for (PendingEdit& edit : pendingEdits_) {
		if (edit.groupName == groupName && edit.categoryPath == categoryPath && edit.itemName == itemName) {
			edit.after = after;
			return;
		}
	}
	pendingEdits_.push_back({groupName, categoryPath, itemName, before, after});
}

// まとめて1件の履歴にする。戻すときは「前」、やり直すときは「後」を書く
void GlobalVariables::CommitEdits() {
	std::erase_if(pendingEdits_, [](const PendingEdit& edit) { return edit.before == edit.after; }); // 動かして元に戻しただけの物は捨てる
	if (pendingEdits_.empty()) {
		return;
	}
	auto apply = [](const std::vector<PendingEdit>& edits, bool undo) {
		for (const PendingEdit& edit : edits) {
			if (!GetInstance()->SetItem(edit.groupName, edit.categoryPath, edit.itemName, undo ? edit.before : edit.after)) {
				return false;
			}
		}
		return true;
	};
	EditorHistory::Record([apply, edits = pendingEdits_] { return apply(edits, true); }, [apply, edits = pendingEdits_] { return apply(edits, false); });
	pendingEdits_.clear();
}
#endif

//=============================================================================
// カテゴリ1つを Parameters/<グループ名>/<カテゴリ名>.json に書き出す
//=============================================================================
void GlobalVariables::SaveCategory(const std::string& groupName, const std::string& categoryName) {
	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		LogManager::Error("[GlobalVariables::SaveCategory] グループが見つかりません: " + groupName);
		return;
	}
	GVNode* category = root->FindChild(categoryName);
	if (!category) {
		LogManager::Error("[GlobalVariables::SaveCategory] カテゴリが見つかりません: " + groupName + " / " + categoryName);
		return;
	}
	if (WriteNodeToFile(CategoryFilePath(groupName, categoryName), *category)) {
		MarkSaved(*category); // ファイルと同じになった（* が消える）
	}
}

//=============================================================================
// グループ内の全カテゴリを一括保存する
//=============================================================================
void GlobalVariables::SaveGroup(const std::string& groupName) {
	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		LogManager::Error("[GlobalVariables::SaveGroup] グループが見つかりません: " + groupName);
		return;
	}

	int savedCount = 0;
	for (const auto& entry : root->entries_) {
		// カテゴリ（子ノード）だけがファイルの単位
		if (!std::holds_alternative<std::shared_ptr<GVNode>>(entry.value)) {
			continue;
		}
		GVNode& category = *std::get<std::shared_ptr<GVNode>>(entry.value);
		if (WriteNodeToFile(CategoryFilePath(groupName, entry.name), category)) {
			MarkSaved(category); // ファイルと同じになった（* が消える）
			++savedCount;
		}
	}
	LogManager::Log(std::format("[GlobalVariables::SaveGroup] {} のカテゴリを{}件保存しました", groupName, savedCount));
}

//=============================================================================
// 全グループを一括保存
//=============================================================================
void GlobalVariables::SaveAll() {
	for (const GroupData& group : groups_) {
		SaveGroup(group.name);
	}
	LogManager::Log("[GlobalVariables::SaveAll] 全グループ保存完了");
}

//=============================================================================
// ノードの中身をjsonのルートに書き出してファイルへ保存する
//=============================================================================
bool GlobalVariables::WriteNodeToFile(const std::filesystem::path& filePath, const GVNode& node) const {
	json j = json::object();
	NodeToJson(node, j);

	// ディレクトリ作成
	std::error_code ec;
	std::filesystem::create_directories(filePath.parent_path(), ec);

	std::ofstream ofs(filePath);
	if (!ofs) {
		LogManager::Error("[GlobalVariables] ファイルを開けません: " + filePath.string());
		assert(false && "GlobalVariables: ファイルオープン失敗");
		return false;
	}
	ofs << std::setw(4) << j << std::endl;
	LogManager::Log("[GlobalVariables] 保存完了: " + filePath.string());
	return true;
}

//=============================================================================
// アイテム1つ ↔ jsonの値
//=============================================================================
GlobalVariables::json GlobalVariables::ItemToJson(const Item& item) {
	return std::visit(
	    [](const auto& v) -> json {
		    using T = std::decay_t<decltype(v)>;
		    if constexpr (std::is_same_v<T, Vector2>) {
			    return json::array({v.x, v.y});
		    } else if constexpr (std::is_same_v<T, Vector3>) {
			    return json::array({v.x, v.y, v.z});
		    } else if constexpr (std::is_same_v<T, Vector4>) {
			    return json::array({v.x, v.y, v.z, v.w});
		    } else if constexpr (std::is_same_v<T, ColorItem>) {
			    return static_cast<int32_t>(v.rgba);
		    } else if constexpr (std::is_same_v<T, ComboItem>) {
			    return v.currentIndex;
		    } else {
			    return v; // bool・int32_t・float はそのまま
		    }
	    },
	    item);
}

bool GlobalVariables::JsonToItem(const json& value, Item& item) {
	return std::visit(
	    [&](auto& v) -> bool {
		    using T = std::decay_t<decltype(v)>;
		    if constexpr (std::is_same_v<T, bool>) {
			    if (!value.is_boolean()) {
				    return false;
			    }
			    v = value.get<bool>();
		    } else if constexpr (std::is_same_v<T, int32_t>) {
			    if (!value.is_number_integer()) {
				    return false;
			    }
			    v = value.get<int32_t>();
		    } else if constexpr (std::is_same_v<T, float>) {
			    if (!value.is_number()) {
				    return false;
			    }
			    v = value.get<float>();
		    } else if constexpr (std::is_same_v<T, Vector2>) {
			    if (!IsNumberArray(value, 2)) {
				    return false;
			    }
			    v = Vector2{value[0].get<float>(), value[1].get<float>()};
		    } else if constexpr (std::is_same_v<T, Vector3>) {
			    if (!IsNumberArray(value, 3)) {
				    return false;
			    }
			    v = Vector3{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
		    } else if constexpr (std::is_same_v<T, Vector4>) {
			    if (!IsNumberArray(value, 4)) {
				    return false;
			    }
			    v = Vector4{value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
		    } else if constexpr (std::is_same_v<T, ColorItem>) {
			    if (!value.is_number_integer()) {
				    return false;
			    }
			    v.rgba = static_cast<uint32_t>(value.get<int32_t>());
		    } else if constexpr (std::is_same_v<T, ComboItem>) {
			    if (!value.is_number_integer()) {
				    return false;
			    }
			    v.currentIndex = value.get<int>();
		    }
		    return true;
	    },
	    item);
}

//=============================================================================
// ノードをjsonに再帰的に書き出す
// アイテムはフラットに、子カテゴリは子オブジェクトとして書き出す
//=============================================================================
void GlobalVariables::NodeToJson(const GVNode& node, json& out) const {
	for (const auto& entry : node.entries_) {
		if (const Item* item = std::get_if<Item>(&entry.value)) {
			out[entry.name] = ItemToJson(*item);
		} else {
			json childJson = json::object();
			NodeToJson(*std::get<std::shared_ptr<GVNode>>(entry.value), childJson);
			out[entry.name] = childJson;
		}
	}
}

//=============================================================================
// jsonファイルの読み込み
// ファイルなし・壊れたjsonは false を返すだけで落とさない
//=============================================================================
bool GlobalVariables::ReadJsonFile(const std::filesystem::path& filePath, json& out) const {
	std::ifstream ifs(filePath);
	if (!ifs) {
		LogManager::Warning("[GlobalVariables] ファイルなし（スキップ）: " + filePath.string());
		return false;
	}
	// 例外を投げない版のパース
	out = json::parse(ifs, nullptr, false);
	if (out.is_discarded()) {
		LogManager::Error("[GlobalVariables] jsonの解析に失敗しました: " + filePath.string());
		return false;
	}
	return true;
}

//=============================================================================
// これから登録するアイテム1つの値を、保存したファイルから読む
//=============================================================================
void GlobalVariables::ReadSavedItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item) const {
	// categoryPath の1つ目がファイル（<カテゴリ名>.json）、2つ目からはファイルの中の入れ子
	const std::vector<std::string> names = SplitCategoryPath(categoryPath);
	if (names.empty()) {
		return; // カテゴリに入っていないアイテムは保存されない
	}
	const std::filesystem::path filePath = CategoryFilePath(groupName, names[0]);
	std::error_code error;
	json file;
	if (!std::filesystem::exists(filePath, error) || !ReadJsonFile(filePath, file)) {
		return; // まだ保存していない（登録した値のまま）
	}
	const json* node = &file;
	for (size_t i = 1; i < names.size(); ++i) {
		const auto it = node->find(names[i]);
		if (it == node->end() || !it->is_object()) {
			return;
		}
		node = &*it;
	}
	const auto it = node->find(itemName);
	if (it != node->end()) {
		JsonToItem(*it, item); // 型が違う値は入れない（登録した値のまま）
	}
}

//=============================================================================
// カテゴリ1つ分のjsonを読み込む
// 既に登録済みのアイテムにのみ値を適用する（新規アイテムは作らない）
//=============================================================================
void GlobalVariables::LoadCategory(const std::string& groupName, const std::string& categoryName) {
	std::filesystem::path filePath = CategoryFilePath(groupName, categoryName);
	json j;
	if (!ReadJsonFile(filePath, j)) {
		return;
	}

	GVNode* root = FindGroupRoot(groupName);
	if (!root) {
		// まだグループが登録されていない場合はスキップ（登録したときに、アイテムごとにファイルから読む）
		return;
	}
	GVNode* category = root->FindChild(categoryName);
	if (!category) {
		// まだカテゴリが登録されていない場合もスキップ
		return;
	}
	JsonToNode(j, *category);
	LogManager::Log("[GlobalVariables::LoadCategory] 読み込み完了: " + filePath.string());
}

//=============================================================================
// グループ1つ分を読み込む
//=============================================================================
void GlobalVariables::LoadGroup(const std::string& groupName) {
	LoadGroupCategories(groupName);
}

//=============================================================================
// Parameters/<グループ名>/ 以下の全カテゴリを読み込む
//=============================================================================
void GlobalVariables::LoadGroupCategories(const std::string& groupName) {
	std::filesystem::path dir = GroupDirectory(groupName);
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file() || entry.path().extension().string() != ".json") {
			continue;
		}
		LoadCategory(groupName, entry.path().stem().string());
	}
}


//=============================================================================
// jsonをノードに再帰的に読み込む
// 登録済みアイテムの型に合わせて値を復元する。読んだ値は「保存した値」にもなる
//=============================================================================
void GlobalVariables::JsonToNode(const json& j, GVNode& node) {
	for (GVNode::Entry& entry : node.entries_) {
		const auto it = j.find(entry.name);
		if (it == j.end()) {
			continue; // ファイルに無い項目は、今の値のまま
		}
		if (Item* item = std::get_if<Item>(&entry.value)) {
			// 型に合わせて復元
			if (JsonToItem(*it, *item)) {
				entry.saved = *item;
			}
		} else if (it->is_object()) {
			// 子カテゴリ
			JsonToNode(*it, *std::get<std::shared_ptr<GVNode>>(entry.value));
		}
	}
}

//=============================================================================
// 全ファイル読み込み
// 登録済みの全グループを Parameters/ から読み直す（まだ登録していないグループは、登録したときに読まれる）
//=============================================================================
void GlobalVariables::LoadFiles() {
	for (const GroupData& group : groups_) {
		LoadGroupCategories(group.name);
	}
}
```

**見つけたこと：GameScene 以外のシーンから始めると、パーティクルの調整値が保存した値にならなかった（SF1 の C-2 から）**

- SF1 で `GameScene::Initialize` の `LoadFiles()` を消した（Play のたびに保存していない値が戻るので）。代わりに「起動のときに `Engine::Initialize` の最後で1回読む」ことにしていた。
- ところが、ファイルの値は**登録済みの項目にしか入らない**。パーティクルの項目（GameScene グループ）は GameScene が `Initialize` で登録するので、**Result から起動して Play → GameScene に進んだ**ときは、起動のときの読み込みより後に登録される → ファイルの値が入らず、コードの初期値のまま。
- SF2 で「最後に開いたシーンから起動」になると、GameScene 以外から起動するのがふつうになるので、ここで直した。**`Add` で新しく項目を作るときに、その場でファイルから値を読む**。2回目からの登録（Play・Stop のたび）は、もう項目があるので何もしない＝保存していない値も戻らない。

**解説（`*` の決め方）**

- 各項目は「今の値」と「保存した値」の2つを持つ。保存した値は、ファイルから読んだとき・ファイルへ書いたときに今の値で上書きする。ファイルにまだ無い項目は、登録した値（コードの初期値）。
- `IsGroupDirty`：どれか1つでも「今 ≠ 保存」なら `*`。Undo で元に戻せば消える。
- `RevertGroup`：全部の項目を保存した値に戻す。戻した分は履歴に積むので、**Revert した後に Ctrl+Z で戻す前の値にもどせる**。

#### G-3. `Editor/Viewport/EditorViewport.cpp`（Scene ビューのカメラを Editor の見出しへ。3か所）

10章の答えの通り、Scene ビューのカメラ（前の Debug グループ）は「エディタだけの設定」なので `Parameters/Editor/` に移す。前のファイル（`Parameters/Debug/DebugCamera.json`）は、起動したときに1回だけ新しい場所へ移す（今までのカメラの位置がそのまま使える）。

```cpp
// ===== 前（1か所目：include）=====
#include "EditorViewport.h"
#include "MyEngine/Camera/Camera.h"

// ===== 後 =====
#include "EditorViewport.h"

#include <filesystem>
#include <format>

#include "MyEngine/Camera/Camera.h"
```

```cpp
// ===== 前（2か所目：namespace の中）=====
constexpr const char* kGVGroup = "Debug"; // ImGui登録グループ名
} // namespace

// ===== 後 =====
constexpr const char* kGVGroup = "Scene View"; // 調整項目のグループ名（Settings ウィンドウの Editor の見出しに出る）

// 前の保存先（Parameters/Debug/）にだけファイルがあれば、今の保存先へ移す（SceneFlow.md SF2.5。起きるのは1回だけ）
void MoveOldCameraFile() {
	const std::filesystem::path oldFile = "Parameters/Debug/DebugCamera.json";
	const std::filesystem::path newFile = "Parameters/Editor/Scene View/DebugCamera.json";
	std::error_code error;
	if (!std::filesystem::exists(oldFile, error) || std::filesystem::exists(newFile, error)) {
		return;
	}
	std::filesystem::create_directories(newFile.parent_path(), error);
	std::filesystem::rename(oldFile, newFile, error);
	std::filesystem::remove(oldFile.parent_path(), error); // 空になった Debug フォルダも消す（中に何か残っていれば消えない）
	LogManager::Log(std::format("Scene ビューのカメラの保存先を移しました: {} → {}", oldFile.generic_string(), newFile.generic_string()));
}
} // namespace
```

```cpp
// ===== 前（3か所目：Initialize と RegisterGV）=====
	// 現在値を初期値として登録 → 保存済みjsonがあれば読み込んでカメラへ反映
	RegisterGV();
（略）
	GlobalVariables::GetInstance()
	    ->Group(kGVGroup)

// ===== 後 =====
	// 現在値を初期値として登録 → 保存済みjsonがあれば読み込んでカメラへ反映
	MoveOldCameraFile();
	RegisterGV();
（略）
	GlobalVariables::GetInstance()
	    ->Group(kGVGroup, SettingsSection::Editor) // エディタだけの設定（Parameters/Editor/ に保存）
```

#### G-4. `Editor/Windows/SettingsWindow.h`・`SettingsWindow.cpp`（新規）

```cpp
#pragma once
#include <string>

/// <summary>
/// Settings ウィンドウ（ゲーム全体・シーン・エディタの設定を1か所に。Unity の Project Settings と同じ形）
/// <para>左に一覧（Project／Scene／Editor の見出し）、右に選んだページ。1ページ＝ GlobalVariables のグループ1つ</para>
/// <para>保存していない変更があるページには * が付く。Save でファイルに書き、Revert で保存した値に戻す</para>
/// </summary>
class SettingsWindow {
public:
	// 毎フレーム1回（ImGuiManager::Begin から）
	static void Draw();
	// ウィンドウを前に出す（Edit → Settings）
	static void Focus() { focusRequest_ = true; }

private:
	// 左の一覧
	static void DrawPageList();
	// 一覧の1行（押すとそのページを選ぶ）
	static void DrawPageItem(const std::string& groupName);
	// 右のページ
	static void DrawPage();

	static std::string selected_; // 選んでいるページ（グループの名前）
	static bool focusRequest_;    // 次のフレームでウィンドウを前に出す
};
```

```cpp
#ifdef USE_IMGUI
#include "SettingsWindow.h"

#include <algorithm>
#include <format>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/UI/GlobalVariables.h"

// 静的メンバ変数
std::string SettingsWindow::selected_;
bool SettingsWindow::focusRequest_ = false;

namespace {
constexpr float kListWidth = 170.0f; // 左の一覧の最初の幅（境目をドラッグで変えられる）

// そのページ（グループ）が登録されているか
bool HasPage(const std::string& groupName) {
	GlobalVariables* gv = GlobalVariables::GetInstance();
	for (const SettingsSection section : {SettingsSection::Project, SettingsSection::Editor}) {
		const std::vector<std::string> names = gv->GetGroupNames(section);
		if (std::find(names.begin(), names.end(), groupName) != names.end()) {
			return true;
		}
	}
	return false;
}
} // namespace


//=============================================================================
// 描画
//=============================================================================
void SettingsWindow::Draw() {
	GlobalVariables* gv = GlobalVariables::GetInstance();
	// 選んでいたページが無い（最初・まだ登録されていない）なら、Project の最初のページにする
	if (!HasPage(selected_)) {
		const std::vector<std::string> names = gv->GetGroupNames(SettingsSection::Project);
		selected_ = names.empty() ? std::string() : names.front();
	}

	if (focusRequest_) {
		ImGui::SetNextWindowFocus();
		focusRequest_ = false;
	}
	ImGui::SetNextWindowSize(ImVec2(640.0f, 420.0f), ImGuiCond_FirstUseEver); // 初めて出すときの大きさ（後は imgui.ini に覚えられる）
	if (ImGui::Begin("Settings")) {
		// 左右に分ける（境目はドラッグで動かせる）
		if (ImGui::BeginTable("##settingsLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthFixed, kListWidth);
			ImGui::TableSetupColumn("page", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();
			// --- 左：一覧 ---
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##pageList")) {
				DrawPageList();
			}
			ImGui::EndChild();
			// --- 右：ページ ---
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##page")) {
				DrawPage();
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
	}
	ImGui::End();

	// 描き終わった。どの項目も触っていなければ、ここまでの変更を1件の履歴にする（Ctrl+Z で戻せる）
	gv->EndDraw();
}

//=============================================================================
// 左の一覧
//=============================================================================
void SettingsWindow::DrawPageList() {
	GlobalVariables* gv = GlobalVariables::GetInstance();

	// --- Project：ゲーム全体（Parameters/<名前>/。製品版にも入る）---
	ImGui::SeparatorText("Project");
	for (const std::string& name : gv->GetGroupNames(SettingsSection::Project)) {
		DrawPageItem(name);
	}

	// --- Scene：開いているシーンの設定（シーンファイルに入る）---
	const SceneManager* sceneManager = SceneManager::GetMain();
	const std::string sceneName = sceneManager ? sceneManager->GetOpenSceneName() : std::string();
	ImGui::SeparatorText(std::format("Scene: {}", sceneName).c_str());
	ImGui::TextDisabled("まだ項目はありません");
	ImGui::SetItemTooltip("空・IBL（環境光）を SceneFlow.md の SF4 でここに足します");

	// --- Editor：エディタだけ（Parameters/Editor/<名前>/。製品版では使わない）---
	ImGui::SeparatorText("Editor");
	for (const std::string& name : gv->GetGroupNames(SettingsSection::Editor)) {
		DrawPageItem(name);
	}
}

void SettingsWindow::DrawPageItem(const std::string& groupName) {
	// * ＝保存していない変更がある。「###」の後ろを ID にする（* が付いても別の項目にならないように）
	const bool isDirty = GlobalVariables::GetInstance()->IsGroupDirty(groupName);
	const std::string label = std::format("{}{}###{}", groupName, isDirty ? " *" : "", groupName);
	if (ImGui::Selectable(label.c_str(), selected_ == groupName)) {
		selected_ = groupName;
	}
}

//=============================================================================
// 右のページ
//=============================================================================
void SettingsWindow::DrawPage() {
	if (selected_.empty()) {
		ImGui::TextDisabled("設定がまだ登録されていません");
		return;
	}
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const bool isDirty = gv->IsGroupDirty(selected_);

	// --- 見出し：名前（* 付き）と、右端に Revert・Save（変更が無ければ押せない）---
	const ImGuiStyle& style = ImGui::GetStyle();
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端
	const float buttonsWidth = ImGui::CalcTextSize("Revert").x + ImGui::CalcTextSize("Save").x + style.FramePadding.x * 4.0f + style.ItemSpacing.x;
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s%s", selected_.c_str(), isDirty ? " *" : "");
	ImGui::SameLine(right - buttonsWidth);
	ImGui::BeginDisabled(!isDirty);
	if (ImGui::Button("Revert")) {
		gv->RevertGroup(selected_); // 保存した値に戻す
	}
	ImGui::SetItemTooltip("保存した値に戻します（Ctrl+Z で、戻す前にもどせます）");
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		gv->SaveGroup(selected_);
	}
	ImGui::EndDisabled();
	ImGui::TextDisabled("保存先: %s", gv->GetSaveDirectory(selected_).c_str());
	ImGui::Separator();

	// --- 中身（カテゴリと項目）---
	gv->DrawGroup(selected_);
}
#endif
```

- 左右に分けるのに `BeginTable`（`Resizable`）を使っている。表の1行2列の中にそれぞれ子ウィンドウを置くと、「境目をドラッグで動かせる左右の枠」になる（Project ウィンドウも同じ）。
- 見出しは `ImGui::SeparatorText`（文字入りの区切り線）。

---

### パート H：Project ウィンドウ（SF2）

#### H-1. `Editor/Assets/AssetIcons.h`・`AssetIcons.cpp`（新規・アイコン）

```cpp
#pragma once
#include <externals/imgui/imgui.h>

// Project ウィンドウなどに出すアイコンの種類
// 画像は MyEngine/Resources/Editor/Icons/<名前>.png（名前はこの並びの名前そのまま。Folder.png など）
enum class AssetIcon {
	Folder,     // フォルダ
	FolderOpen, // 中を見ているフォルダ（左の木。画像が無ければ Folder で描く）
	Scene,      // シーン（.scene.json）
	Model,      // モデル（.obj .gltf .glb .fbx）
	Texture,    // 画像（小さい絵を作るまでの間・作れなかったとき）
	Sound,      // 音（.wav .mp3）
	File,       // それ以外
};


/// <summary>
/// エディタのアイコン
/// <para>画像が無い種類は、色の付いた形と頭文字で代わりに描く（画像を置くだけで差し替わる）</para>
/// </summary>
namespace AssetIcons {
// アイコンの画像を読む（ImGuiManager::Initialize から1回）。置いていない画像は飛ばす
void Initialize();
// min〜max の四角いっぱいに icon を描く
void Draw(ImDrawList* drawList, AssetIcon icon, const ImVec2& min, const ImVec2& max);
} // namespace AssetIcons
```

```cpp
#ifdef USE_IMGUI
#include "AssetIcons.h"

#include <array>
#include <cfloat>
#include <filesystem>
#include <format>
#include <string>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Graphics/Texture/TextureManager.h"

namespace {
// アイコンの画像の置き場所（エンジンの Resources/Editor/Icons/ が、ビルドのたびにゲームへ写される）
constexpr const char* kIconFolder = "MyEngine/Resources/Editor/Icons/";
constexpr size_t kIconCount = magic_enum::enum_count<AssetIcon>(); // アイコンの種類の数

// 種類ごとの画像（ImTextureID_Invalid なら画像なし。添え字は AssetIcon の並びの番号）
std::array<ImTextureID, kIconCount> iconTextures{};

// 画像が無いときの見た目
struct Fallback {
	ImU32 color;        // 形の色
	const char* letter; // 真ん中に出す文字（空なら出さない）
};
Fallback FallbackOf(AssetIcon icon) {
	switch (icon) {
	case AssetIcon::Folder:
	case AssetIcon::FolderOpen:
		return {IM_COL32(222, 184, 92, 255), ""};
	case AssetIcon::Scene:
		return {IM_COL32(80, 130, 210, 255), "S"};
	case AssetIcon::Model:
		return {IM_COL32(60, 160, 170, 255), "M"};
	case AssetIcon::Texture:
		return {IM_COL32(170, 90, 170, 255), "T"};
	case AssetIcon::Sound:
		return {IM_COL32(90, 160, 80, 255), "♪"};
	case AssetIcon::File:
		return {IM_COL32(120, 120, 120, 255), ""};
	}
	return {IM_COL32(120, 120, 120, 255), ""};
}

// フォルダの形（左上の出っぱり＋本体）
void DrawFolderShape(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 color) {
	const float width = max.x - min.x;
	const float height = max.y - min.y;
	const float rounding = width * 0.08f;
	drawList->AddRectFilled(ImVec2(min.x + width * 0.08f, min.y + height * 0.14f), ImVec2(min.x + width * 0.46f, min.y + height * 0.34f), color, rounding);
	drawList->AddRectFilled(ImVec2(min.x + width * 0.08f, min.y + height * 0.24f), ImVec2(max.x - width * 0.08f, max.y - height * 0.14f), color, rounding);
}

// ファイルの形（縦長の四角＋真ん中に文字）
void DrawFileShape(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const Fallback& fallback) {
	const float width = max.x - min.x;
	const float height = max.y - min.y;
	const ImVec2 paperMin(min.x + width * 0.18f, min.y + height * 0.08f);
	const ImVec2 paperMax(max.x - width * 0.18f, max.y - height * 0.08f);
	drawList->AddRectFilled(paperMin, paperMax, fallback.color, width * 0.08f);
	// 文字は、小さすぎて読めないときは出さない
	const float fontSize = height * 0.45f;
	if (fallback.letter[0] == '\0' || fontSize < 8.0f) {
		return;
	}
	const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, fallback.letter);
	const ImVec2 textPos((paperMin.x + paperMax.x - textSize.x) * 0.5f, (paperMin.y + paperMax.y - textSize.y) * 0.5f);
	drawList->AddText(nullptr, fontSize, textPos, IM_COL32_WHITE, fallback.letter);
}
} // namespace


//=============================================================================
// 画像を読む
//=============================================================================
void AssetIcons::Initialize() {
	for (const AssetIcon icon : magic_enum::enum_values<AssetIcon>()) {
		const std::string path = std::format("{}{}.png", kIconFolder, magic_enum::enum_name(icon));
		std::error_code error;
		if (!std::filesystem::exists(path, error)) {
			continue; // まだ置いていない（形で代わりに描く）
		}
		// 小さい画像なので、ふつうのテクスチャとして読む（ミップマップもできるので、小さく描いてもきれい）
		const uint32_t srvIndex = TextureManager::Load(path);
		if (const TextureManager::TextureData* data = TextureManager::GetTextureData(srvIndex)) {
			iconTextures[static_cast<size_t>(icon)] = static_cast<ImTextureID>(data->srvHandleGPU.ptr);
		}
	}
	// 開いたフォルダの画像が無ければ、閉じたフォルダの画像を使う
	ImTextureID& folderOpen = iconTextures[static_cast<size_t>(AssetIcon::FolderOpen)];
	if (folderOpen == ImTextureID_Invalid) {
		folderOpen = iconTextures[static_cast<size_t>(AssetIcon::Folder)];
	}
}

//=============================================================================
// 描く
//=============================================================================
void AssetIcons::Draw(ImDrawList* drawList, AssetIcon icon, const ImVec2& min, const ImVec2& max) {
	// --- 画像があれば、それを貼る ---
	const ImTextureID texture = iconTextures[static_cast<size_t>(icon)];
	if (texture != ImTextureID_Invalid) {
		drawList->AddImage(texture, min, max);
		return;
	}
	// --- 無ければ、形で代わりに描く ---
	const Fallback fallback = FallbackOf(icon);
	if (icon == AssetIcon::Folder || icon == AssetIcon::FolderOpen) {
		DrawFolderShape(drawList, min, max, fallback.color);
	} else {
		DrawFileShape(drawList, min, max, fallback);
	}
}
#endif
```

- 画像の名前は `magic_enum::enum_name(icon)`（`AssetIcon::Folder` → `"Folder"`）で作るので、種類を足したら画像を1つ置くだけ。
- `iconTextures` の添え字は `static_cast<size_t>(icon)`（enum の並びの番号＝0, 1, 2 …）。

#### H-2. `Editor/Assets/AssetThumbnails.h`・`AssetThumbnails.cpp`（新規・画像の小さい絵）

```cpp
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

#include <externals/imgui/imgui.h>

/// <summary>
/// Project ウィンドウに出す、画像ファイルの小さい絵（サムネイル）
/// <para>元の画像を小さく縮めて（長い辺 kThumbnailSize ピクセル）GPU に置き、覚えておく。元の画像のテクスチャは作らない（4K の画像でも軽い）</para>
/// <para>作るのは1フレームに1枚まで（大きい画像を何枚も一度に読むと止まって見えるので）。まだ作っていない間は false を返すので、呼ぶ側はアイコンで代わりに描く</para>
/// </summary>
namespace AssetThumbnails {
// 小さい絵の長い辺のピクセル数
inline constexpr uint32_t kThumbnailSize = 128;

// path（画像ファイル。AssetType::Texture の拡張子）の小さい絵を取る。無ければ作る
// writeTime はファイルの更新時刻（作ったときと違えば、描き直された画像なので作り直す）
// 取れたら texture と size（絵のピクセル数。縦横比を保って描くのに使う）を入れて true
bool Get(const std::string& path, std::filesystem::file_time_type writeTime, ImTextureID& texture, ImVec2& size);

// 全部捨てる（終了時。GPU を待った後に呼ぶ）
void Release();
} // namespace AssetThumbnails
```

```cpp
#ifdef USE_IMGUI
#include "AssetThumbnails.h"

#include <algorithm>
#include <format>
#include <unordered_map>
#include <utility>
#include <vector>

#include <d3d12.h>
#include <wincodec.h>
#include <wrl.h>

#include <externals/DirectXTex/DirectXTex.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/String/ConvertString.h"

namespace {
constexpr size_t kMaxThumbnails = 48; // 覚えておく数の上限（1枚で SRV のスロットを1つ使う。超えたら、しばらく描いていない物から捨てる）
constexpr int kLoadsPerFrame = 1;     // 1フレームに作る数

// 1枚分
struct Thumbnail {
	Microsoft::WRL::ComPtr<ID3D12Resource> resource; // 小さい絵（作れなかったら空）
	uint32_t srvSlot = 0;                            // SRV のスロット（0 なら作れなかった）
	ImVec2 size;                                     // 絵のピクセル数
	std::filesystem::file_time_type writeTime;       // 作ったときのファイルの更新時刻
	int lastUsedFrame = 0;                           // 最後に描いたフレーム（ImGui のフレーム番号）
};
std::unordered_map<std::string, Thumbnail> thumbnails; // パス → 小さい絵
int loadFrame = -1; // loadCount を数えているフレーム
int loadCount = 0;  // そのフレームに作った数

// 長い辺が kThumbnailSize になるように縮めた大きさ（元の方が小さければそのまま）
std::pair<uint32_t, uint32_t> FitSize(size_t width, size_t height) {
	const size_t longSide = (std::max)(width, height);
	if (longSide <= AssetThumbnails::kThumbnailSize) {
		return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	}
	const double scale = static_cast<double>(AssetThumbnails::kThumbnailSize) / static_cast<double>(longSide);
	return {(std::max)(1u, static_cast<uint32_t>(width * scale)), (std::max)(1u, static_cast<uint32_t>(height * scale))};
}

//=============================================================================
// 読む（CPU）。どちらも R8G8B8A8_UNORM_SRGB・長い辺 kThumbnailSize 以下の絵を image に作る
//=============================================================================
// png・jpg・bmp：WIC（Windows の画像の読み込み）で、縮めながら読む
// JPEG は読みながら 1/2・1/4・1/8 に縮められるので、4K の画像でも元の大きさで読むより速い
bool LoadWic(const std::wstring& path, DirectX::ScratchImage& image) {
	bool isWic2 = false;
	IWICImagingFactory* factory = DirectX::GetWICFactory(isWic2); // DirectXTex が作った物を借りる（こちらでは解放しない）
	if (factory == nullptr) {
		return false;
	}
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) || FAILED(decoder->GetFrame(0, &frame))) {
		return false;
	}
	UINT width = 0;
	UINT height = 0;
	frame->GetSize(&width, &height);
	const auto [thumbnailWidth, thumbnailHeight] = FitSize(width, height);

	// 縮める → 1ピクセル RGBA 8bit ずつにそろえる
	Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
	if (FAILED(factory->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(frame.Get(), thumbnailWidth, thumbnailHeight, WICBitmapInterpolationModeFant))) {
		return false;
	}
	if (FAILED(factory->CreateFormatConverter(&converter)) ||
	    FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
		return false;
	}
	// png・jpg の色は sRGB で書かれているので、_SRGB の形として受け取る（TextureManager と同じ）
	if (FAILED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, thumbnailWidth, thumbnailHeight, 1, 1))) {
		return false;
	}
	const DirectX::Image* pixels = image.GetImage(0, 0, 0);
	return SUCCEEDED(converter->CopyPixels(nullptr, static_cast<UINT>(pixels->rowPitch), static_cast<UINT>(pixels->slicePitch), pixels->pixels));
}

// hdr：DirectXTex で元の大きさのまま読んでから縮める（WIC は .hdr を読めない。大きい空の画像は少し時間がかかる）
bool LoadHdr(const std::wstring& path, DirectX::ScratchImage& image) {
	DirectX::ScratchImage hdr;
	if (FAILED(DirectX::LoadFromHDRFile(path.c_str(), nullptr, hdr))) {
		return false;
	}
	const auto [thumbnailWidth, thumbnailHeight] = FitSize(hdr.GetMetadata().width, hdr.GetMetadata().height);
	DirectX::ScratchImage resized;
	if (FAILED(DirectX::Resize(*hdr.GetImage(0, 0, 0), thumbnailWidth, thumbnailHeight, DirectX::TEX_FILTER_DEFAULT, resized))) {
		return false;
	}
	// 明るさ 1 を超える所は白になる（小さい絵で見分けがつけばよいので、明るさの調整はしない）
	return SUCCEEDED(DirectX::Convert(*resized.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, image));
}

//=============================================================================
// GPU に置く
//=============================================================================
bool Upload(const DirectX::ScratchImage& image, Thumbnail& thumbnail) {
	const DirectX::TexMetadata& metadata = image.GetMetadata();
	ID3D12Device* device = DirectXCommon::GetDevice();

	// --- 置き場所（DEFAULT ヒープ。コピーを受ける状態で作る）---
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = metadata.width;
	desc.Height = static_cast<UINT>(metadata.height);
	desc.DepthOrArraySize = 1;
	desc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	desc.Format = metadata.format;
	desc.SampleDesc.Count = 1;
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&thumbnail.resource)))) {
		return false;
	}

	// --- ミップマップの段ごとの中身を送る（描画とは別のコマンドリストなので、ImGui を描いている途中でもよい）---
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	for (size_t i = 0; i < image.GetImageCount(); ++i) {
		const DirectX::Image& level = image.GetImages()[i];
		subresources.push_back({level.pixels, static_cast<LONG_PTR>(level.rowPitch), static_cast<LONG_PTR>(level.slicePitch)});
	}
	UploadContext::QueueTextureUpload(thumbnail.resource.Get(), subresources, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	UploadContext::Flush(); // 送り終わるまで待つ（この後すぐ ImGui が描くので）

	// --- SRV（ImGui::Image に渡す番号）---
	thumbnail.srvSlot = DirectXCommon::AllocateSRVSlot();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = desc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	ID3D12DescriptorHeap* srvHeap = DirectXCommon::GetSRVDescriptorHeap();
	device->CreateShaderResourceView(thumbnail.resource.Get(), &srvDesc, DirectXCommon::GetCPUDescriptorHandle(srvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, thumbnail.srvSlot));
	thumbnail.size = ImVec2(static_cast<float>(metadata.width), static_cast<float>(metadata.height));
	return true;
}

// 1枚作る。作れなければ false（thumbnail は空のまま）
bool Create(const std::string& path, Thumbnail& thumbnail) {
	const std::wstring widePath = ConvertString(path);
	DirectX::ScratchImage image;
	const bool isHdr = path.ends_with(".hdr") || path.ends_with(".HDR");
	if (!(isHdr ? LoadHdr(widePath, image) : LoadWic(widePath, image))) {
		return false;
	}
	// ミップマップ（小さく描いてもチラつかないように）。1x1 の画像は作れないので、そのまま使う
	DirectX::ScratchImage mipImages;
	const bool hasMips = SUCCEEDED(DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_SRGB, 0, mipImages));
	return Upload(hasMips ? mipImages : image, thumbnail);
}

// SRV のスロットを返して、GPU の絵を捨てる
// 捨てるのは「このフレームにまだ描いていない物」だけなので、前のフレームの描画は（フレームの終わりに GPU を待っているので）終わっている
void Free(Thumbnail& thumbnail) {
	if (thumbnail.srvSlot != 0) {
		DirectXCommon::FreeSRVSlot(thumbnail.srvSlot); // スロットは、このフレームの GPU を待った後から配り直される
		thumbnail.srvSlot = 0;
	}
	thumbnail.resource.Reset();
}

// 覚えている数が上限なら、いちばん長く描いていない物を1つ捨てる
// このフレームに描いた物しか無ければ捨てずに false（今のフレームの ImGui がまだ使うので）
bool MakeRoom(int frame) {
	if (thumbnails.size() < kMaxThumbnails) {
		return true;
	}
	auto oldest = thumbnails.end();
	for (auto it = thumbnails.begin(); it != thumbnails.end(); ++it) {
		if (it->second.lastUsedFrame < frame && (oldest == thumbnails.end() || it->second.lastUsedFrame < oldest->second.lastUsedFrame)) {
			oldest = it;
		}
	}
	if (oldest == thumbnails.end()) {
		return false;
	}
	Free(oldest->second);
	thumbnails.erase(oldest);
	return true;
}

// SRV のスロット → ImGui に渡す番号（GPU から見たハンドル）
ImTextureID TextureIdOf(uint32_t srvSlot) {
	return static_cast<ImTextureID>(DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvSlot).ptr);
}
} // namespace


//=============================================================================
// 小さい絵を取る（無ければ作る）
//=============================================================================
bool AssetThumbnails::Get(const std::string& path, std::filesystem::file_time_type writeTime, ImTextureID& texture, ImVec2& size) {
	// フレームが変わったら、作った数を数え直す
	const int frame = ImGui::GetFrameCount();
	if (frame != loadFrame) {
		loadFrame = frame;
		loadCount = 0;
	}

	auto it = thumbnails.find(path);
	// --- 作ってあって、ファイルも変わっていない ---
	if (it != thumbnails.end() && it->second.writeTime == writeTime) {
		it->second.lastUsedFrame = frame;
		if (it->second.srvSlot == 0) {
			return false; // 作れなかった画像（ファイルが変わるまで作り直さない）
		}
		texture = TextureIdOf(it->second.srvSlot);
		size = it->second.size;
		return true;
	}

	// --- 作る（このフレームにもう作っていたら、次のフレームに回す）---
	if (loadCount >= kLoadsPerFrame) {
		return false;
	}
	// ファイルが描き直されていたら、前の絵を捨てる（このフレームにもう描いてしまった物は、次のフレームで）
	if (it != thumbnails.end()) {
		if (it->second.lastUsedFrame == frame) {
			return false;
		}
		Free(it->second);
		thumbnails.erase(it);
	}
	if (!MakeRoom(frame)) {
		return false;
	}
	++loadCount;
	Thumbnail& thumbnail = thumbnails[path];
	thumbnail.writeTime = writeTime;
	thumbnail.lastUsedFrame = frame;
	if (!Create(path, thumbnail)) {
		Free(thumbnail); // 途中まで作った物を捨てる（空のまま覚えておき、作り直さない）
		LogManager::Warning(std::format("小さい絵を作れませんでした: {}", path));
		return false;
	}
	texture = TextureIdOf(thumbnail.srvSlot);
	size = thumbnail.size;
	return true;
}

//=============================================================================
// 全部捨てる
//=============================================================================
void AssetThumbnails::Release() {
	for (auto& [path, thumbnail] : thumbnails) {
		Free(thumbnail);
	}
	thumbnails.clear();
}
#endif
```

**解説（小さい絵の作り方）**

| 段 | すること | なぜ |
|---|---|---|
| 読む | png・jpg・bmp は WIC（Windows の画像の読み込み）で**縮めながら**読む。hdr は DirectXTex で読んでから縮める | `TextureManager::Load` で元の大きさのまま読むと、4K の画像1枚で GPU のメモリを 80MB 使い、ミップマップを作るのにも時間がかかる。Project で見るだけなら 128px で足りる |
| 形をそろえる | `R8G8B8A8_UNORM_SRGB`（1ピクセル4バイト・色は sRGB） | png・jpg の色は sRGB で書かれている（TextureManager と同じ扱い） |
| ミップマップ | 128px → 64 → … → 1 | 16px（1行ずつの並び）に縮めて描いてもチラつかないように |
| GPU へ送る | パート E-4 の `QueueTextureUpload` → `Flush` | 描画のコマンドリストを止めずに、別のリストで送る |
| 覚える | パス → 絵。ファイルの更新時刻が変わったら作り直す | 画像を描き直したら、Project の絵も変わる |
| 上限 | 48枚。超えたら、しばらく描いていない物から捨てる | 絵1枚で SRV のスロット（全部で256）を1つ使う。今のエンジンは28個使っている |
| 1フレームに1枚 | まだ作っていない間はアイコンで代わりに描く | 4K の画像がたくさんあるフォルダ（`Goblets/textures`）を開いたときに、一度に全部作って止まらないように |

- **捨ててよいか**：捨てるのは「このフレームにまだ描いていない物」だけ。前のフレームの描画は、フレームの終わりに GPU を待っているので終わっている。SRV のスロットは `FreeSRVSlot` が「このフレームの GPU を待った後」から配り直すので、同じフレームの中で別の絵に使われることもない。
- `DirectX::GetWICFactory` は、DirectXTex が中で作った WIC の工場（画像を読む道具を作る物）を借りる関数。こちらで作ったり解放したりしなくてよい。

#### H-3. `Editor/Windows/SceneDialogs.h`・`SceneDialogs.cpp`（新規・「保存しますか？」と File メニュー）

```cpp
#pragma once
#include <string>

/// <summary>
/// シーンのファイルの操作で出す確認のウィンドウと、File メニューの中身
/// <para>保存していない変更があるまま別のシーンを開くときは「保存しますか？（Save / Don't Save / Cancel）」を聞く</para>
/// <para>New Scene・Save As は名前を聞く（使える文字は英数字と _ -。FileNameRule.h）</para>
/// <para>シーンを開く・作る・名前を付けて保存するのは停止中だけ（再生中はメニューが押せない）</para>
/// </summary>
class SceneDialogs {
public:
	// 毎フレーム1回（ImGuiManager::Begin から）。確認のウィンドウとショートカット（Ctrl+N・Ctrl+Shift+S）
	static void Draw();
	// File メニューの中身（ImGui::BeginMenu("File") の中から呼ぶ）
	static void DrawFileMenuItems();

	// name のシーンを開く。保存していない変更があれば、先に聞く（Project ウィンドウのダブルクリックからも）
	static void RequestOpen(const std::string& name);
	// 名前を聞いて、新しいシーンを作って開く
	static void RequestNew();
	// 名前を聞いて、今のシーンをその名前で保存し、開き直す
	static void RequestSaveAs();

private:
	// 確認のウィンドウの種類
	enum class Dialog {
		None,
		Unsaved,  // 保存しますか？
		NewScene, // 新しいシーンの名前
		SaveAs,   // 名前を付けて保存
	};
	static const char* TitleOf(Dialog dialog);
	static void DrawUnsavedDialog();
	// 名前を聞くウィンドウ（NewScene・SaveAs で共通）
	static void DrawNameDialog(Dialog dialog);
	static void HandleShortcuts();

	static Dialog openRequest_;      // 次のフレームに開く確認のウィンドウ（ほかのポップアップの中から開くと入れ子になるので、1フレーム預かる）
	static std::string pendingOpen_; // 「保存しますか？」に答えた後に開くシーン
	static std::string nameBuffer_;  // 名前の入力欄の中身
	static bool nameFocusRequest_;   // 名前の入力欄を出した最初のフレームだけ true（すぐに打てるように）
};
```

```cpp
#ifdef USE_IMGUI
#include "SceneDialogs.h"

#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/String/FileNameRule.h"

// 静的メンバ変数
SceneDialogs::Dialog SceneDialogs::openRequest_ = SceneDialogs::Dialog::None;
std::string SceneDialogs::pendingOpen_;
std::string SceneDialogs::nameBuffer_;
bool SceneDialogs::nameFocusRequest_ = false;

namespace {
// シーンのファイルを触れるか（エディタのシーンマネージャがあって、停止中）
bool CanEditScene() {
	const SceneManager* sceneManager = SceneManager::GetMain();
	return sceneManager != nullptr && sceneManager->GetPlayState() == PlayState::Editing;
}

// 次に出すウィンドウを、画面の真ん中に置く
void CenterNextWindow() { ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); }
} // namespace


//=============================================================================
// 頼む（Project ウィンドウ・メニュー・ショートカットから）
//=============================================================================
void SceneDialogs::RequestOpen(const std::string& name) {
	SceneManager* sceneManager = SceneManager::GetMain();
	if (!CanEditScene() || name == sceneManager->GetOpenSceneName()) {
		return; // 再生中・もう開いている
	}
	// 保存していない変更があれば、先に聞く（答えを聞いてから開く）
	if (sceneManager->IsDirty()) {
		pendingOpen_ = name;
		openRequest_ = Dialog::Unsaved;
		return;
	}
	sceneManager->RequestOpenScene(name);
}

void SceneDialogs::RequestNew() {
	if (!CanEditScene()) {
		return;
	}
	nameBuffer_.clear();
	nameFocusRequest_ = true;
	openRequest_ = Dialog::NewScene;
}

void SceneDialogs::RequestSaveAs() {
	const SceneManager* sceneManager = SceneManager::GetMain();
	if (!CanEditScene() || sceneManager->GetSceneFile() == nullptr) {
		return; // シーンファイルを使わない前の書き方のシーンは、保存できない
	}
	nameBuffer_ = sceneManager->GetOpenSceneName(); // 今の名前から直して付けられるように
	nameFocusRequest_ = true;
	openRequest_ = Dialog::SaveAs;
}

//=============================================================================
// 毎フレーム
//=============================================================================
void SceneDialogs::Draw() {
	HandleShortcuts();
	// 預かっていた確認のウィンドウを開く（ここはどのポップアップの中でもないので、入れ子にならない）
	if (openRequest_ != Dialog::None) {
		ImGui::OpenPopup(TitleOf(openRequest_));
		openRequest_ = Dialog::None;
	}
	DrawUnsavedDialog();
	DrawNameDialog(Dialog::NewScene);
	DrawNameDialog(Dialog::SaveAs);
}

const char* SceneDialogs::TitleOf(Dialog dialog) {
	switch (dialog) {
	case Dialog::Unsaved:
		return "Unsaved Changes";
	case Dialog::NewScene:
		return "New Scene";
	case Dialog::SaveAs:
		return "Save Scene As";
	case Dialog::None:
		break;
	}
	return "";
}

//=============================================================================
// File メニューの中身
//=============================================================================
void SceneDialogs::DrawFileMenuItems() {
	SceneManager* sceneManager = SceneManager::GetMain();
	const bool canEdit = CanEditScene();
	const bool hasFile = canEdit && sceneManager->GetSceneFile() != nullptr;

	if (ImGui::MenuItem("New Scene...", "Ctrl+N", false, canEdit)) {
		RequestNew();
	}
	// resources/scenes/ のシーンの一覧（開いているシーンにはチェック）
	if (ImGui::BeginMenu("Open Scene", canEdit)) {
		const std::vector<std::string> names = SceneManager::GetSceneNames();
		if (names.empty()) {
			ImGui::TextDisabled("resources/scenes/ にシーンがありません");
		}
		for (const std::string& name : names) {
			const bool isOpen = (name == sceneManager->GetOpenSceneName());
			if (ImGui::MenuItem(name.c_str(), nullptr, isOpen)) {
				RequestOpen(name);
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Save", "Ctrl+S", false, hasFile)) {
		sceneManager->RequestSave();
	}
	if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, hasFile)) {
		RequestSaveAs();
	}
}

//=============================================================================
// 保存しますか？
//=============================================================================
void SceneDialogs::DrawUnsavedDialog() {
	CenterNextWindow();
	if (!ImGui::BeginPopupModal(TitleOf(Dialog::Unsaved), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	SceneManager* sceneManager = SceneManager::GetMain();
	if (sceneManager == nullptr) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	ImGui::Text("%s に保存していない変更があります。", sceneManager->GetOpenSceneName().c_str());
	ImGui::Text("%s を開く前に保存しますか？", pendingOpen_.c_str());
	ImGui::Spacing();

	// Save：保存してから開く（SceneManager は同じフレームに頼まれたら「保存 → 開く」の順にする。保存できなければ開かない）
	if (ImGui::Button("Save")) {
		sceneManager->RequestSave();
		sceneManager->RequestOpenScene(pendingOpen_);
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	// Don't Save：変更を捨てて開く
	if (ImGui::Button("Don't Save")) {
		sceneManager->RequestOpenScene(pendingOpen_);
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	// Cancel：開かない（今のシーンのまま）
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

//=============================================================================
// 名前を聞く（New Scene・Save As）
//=============================================================================
void SceneDialogs::DrawNameDialog(Dialog dialog) {
	CenterNextWindow();
	if (!ImGui::BeginPopupModal(TitleOf(dialog), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}

	// --- 名前の入力欄（使えない文字は打っても入らない）---
	if (nameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		nameFocusRequest_ = false;
	}
	ImGui::SetNextItemWidth(280.0f);
	const bool entered = EditorWidgets::InputFileName("Name", nameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	// --- 使える名前か。だめなら理由を出して、押せなくする ---
	const char* problem = nullptr;
	if (nameBuffer_.empty()) {
		problem = "名前を入れてください（英数字と _ -）";
	} else if (!IsValidFileName(nameBuffer_)) {
		problem = "使えない文字が入っています（英数字と _ - だけ）"; // Save As の最初の名前（今のシーンの名前）が日本語のときなど
	} else if (SceneManager::HasScene(nameBuffer_)) {
		problem = "その名前のシーンはもうあります";
	}
	if (problem == nullptr) {
		ImGui::TextDisabled("%s", MakeSceneFilePath(nameBuffer_).c_str()); // できるファイル
	} else {
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s", problem);
	}
	ImGui::Spacing();

	// --- Create / Save（Enter でも）---
	ImGui::BeginDisabled(problem != nullptr);
	const bool pressed = ImGui::Button(dialog == Dialog::NewScene ? "Create" : "Save");
	ImGui::EndDisabled();
	if (problem == nullptr && (pressed || entered)) {
		if (dialog == Dialog::NewScene) {
			// 作って開く（保存していない変更があれば、次のフレームに「保存しますか？」が出る）
			if (SceneManager::CreateSceneFile(nameBuffer_)) {
				RequestOpen(nameBuffer_);
			}
		} else if (SceneManager* sceneManager = SceneManager::GetMain()) {
			sceneManager->RequestSaveAs(nameBuffer_);
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

//=============================================================================
// ショートカット（Ctrl+S は Control ウィンドウが受ける）
//=============================================================================
void SceneDialogs::HandleShortcuts() {
	// 文字の入力中・ドラッグ中・ほかのポップアップが出ている間は何もしない
	if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
		return;
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
		RequestNew();
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S)) {
		RequestSaveAs();
	}
}
#endif
```

- **なぜ1フレーム預かってから開くのか**（`openRequest_`）：ImGui のポップアップは「今開いているポップアップの中」で `OpenPopup` すると、その子として開く。New Scene の名前のウィンドウの中から「保存しますか？」を開くと入れ子になり、名前のウィンドウを閉じると一緒に閉じてしまう。どのポップアップの中でもない `Draw()` の頭で開けば、ふつうに開く。

#### H-4. `Editor/Windows/ProjectWindow.h`・`ProjectWindow.cpp`（新規）

```cpp
#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Assets/AssetIcons.h"


/// <summary>
/// Project ウィンドウ（resources/ のファイルを見る・作る・開く・名前を変える・消す。Unity の Project ウィンドウと同じ形）
/// <para>左に resources/ のフォルダの木、右に選んだフォルダの中身（アイコンの並び。画像ファイルはその画像の小さい絵）</para>
/// <para>シーンはダブルクリックで開く。Create → Scene は、どのフォルダで押しても resources/scenes/ に作る（シーンは名前で探すので、置き場所は1つ）</para>
/// <para>ファイルは1秒ごとに読み直すので、エクスプローラーで足した物もすぐに出る</para>
/// </summary>
class ProjectWindow {
public:
	// 毎フレーム1回（ImGuiManager::Begin から）
	static void Draw();

private:
	// 右に並べる1つ（ファイルかフォルダ）
	struct Item {
		std::filesystem::path path;                // resources/scenes/Title.scene.json
		std::string name;                          // 出す名前（シーン・モデル・画像・音は拡張子を除く。シーンは "Title"）
		AssetIcon icon = AssetIcon::File;          // 種類
		bool isFolder = false;                     // フォルダか
		std::filesystem::file_time_type writeTime; // 更新時刻（画像が描き直されたら、小さい絵を作り直す）
	};
	// 左の木の1つ
	struct Folder {
		std::filesystem::path path;
		std::string name;
		std::vector<Folder> children;
	};

	// ===== 読む =====
	// フォルダの木と、今のフォルダの中身を読み直す
	static void Rescan();
	static Folder ScanFolderTree(const std::filesystem::path& path);
	static std::vector<Item> ScanItems(const std::filesystem::path& folder);

	// ===== 描く =====
	// 今のフォルダの場所（押すとそこへ戻る）と、アイコンの大きさ
	static void DrawToolbar();
	// 左の木（フォルダ1つ分。子がいれば入れ子で描く）
	static void DrawFolderTree(const Folder& folder);
	// 右の中身（アイコンの並び、または1行ずつの並び）
	static void DrawItems();
	static void DrawGridItem(const Item& item, float cellWidth);
	static void DrawListItem(const Item& item);
	// アイコン（画像ファイルなら小さい絵）を min〜max に描く
	static void DrawItemIcon(ImDrawList* drawList, const Item& item, const ImVec2& min, const ImVec2& max);
	// 出す名前（開いているシーンで、保存していなければ * を付ける）
	static std::string LabelOf(const Item& item);

	// ===== 操作 =====
	// 項目の上でのクリック（選ぶ・ダブルクリックで開く・右クリックのメニュー）。項目を置いた直後に呼ぶ
	static void HandleItemInput(const Item& item);
	static void DrawItemMenu(const Item& item);
	static void DrawCreateMenu();
	// 何も無い所（クリックで選ぶのをやめる・右クリックで作成メニュー）
	static void HandleEmptySpace();
	static void HandleShortcuts();
	// ダブルクリック：フォルダは中へ、シーンはエディタで開く
	static void Open(const Item& item);
	static void OpenFolder(const std::filesystem::path& folder);
	static void CreateFolder();
	static void CreateScene();
	// 名前の変更
	static void StartRename(const Item& item);
	static void DrawRenameField(const Item& item);
	static void ApplyRename(const Item& item);
	// ごみ箱へ移すか聞くウィンドウ
	static void DrawDeleteDialog();
	// 名前を変える・消すことができない物か（開いているシーンと、それが入っているフォルダ）
	static bool IsLocked(const Item& item);
	// 選んでいる項目（無ければ nullptr）
	static const Item* FindSelected();

	static std::filesystem::path currentFolder_; // 右に中身を出しているフォルダ
	static std::filesystem::path selected_;      // 選んでいるファイル・フォルダ（空なら無し）
	static std::filesystem::path renaming_;      // 名前を変えている物（空なら変えていない）
	static std::string renameBuffer_;            // 入力中の名前（確定するまでファイルの名前は変えない）
	static bool renameFocusRequest_;             // 入力欄を出した最初のフレームだけ true
	static std::filesystem::path deleteTarget_;  // ごみ箱へ移すか聞いている物
	static bool deleteRequest_;                  // 次に「ごみ箱へ移しますか？」を開く
	static float iconSize_;                      // 右のアイコンの大きさ（kListModeSize 以下なら1行ずつの並び）
	static Folder rootFolder_;                   // 左の木（resources/）
	static std::vector<Item> items_;             // 右に出している中身
	static double lastScanTime_;                 // 最後に読み直した時刻（ImGui::GetTime）
	static bool rescanRequest_;                  // 次のフレームの頭で読み直す（作った・消した・名前を変えた・フォルダを移った）
	static bool revealRequest_;                  // 左の木で、今のフォルダまでの親を開く（右でフォルダの中へ入ったとき）
};
```

```cpp
#define NOMINMAX
#ifdef USE_IMGUI
#include "ProjectWindow.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <format>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/Editor/Assets/AssetThumbnails.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Editor/Windows/SceneDialogs.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/String/FileNameRule.h"

// 静的メンバ変数
std::filesystem::path ProjectWindow::currentFolder_ = "resources";
std::filesystem::path ProjectWindow::selected_;
std::filesystem::path ProjectWindow::renaming_;
std::string ProjectWindow::renameBuffer_;
bool ProjectWindow::renameFocusRequest_ = false;
std::filesystem::path ProjectWindow::deleteTarget_;
bool ProjectWindow::deleteRequest_ = false;
float ProjectWindow::iconSize_ = 64.0f;
ProjectWindow::Folder ProjectWindow::rootFolder_;
std::vector<ProjectWindow::Item> ProjectWindow::items_;
double ProjectWindow::lastScanTime_ = -1000.0;
bool ProjectWindow::rescanRequest_ = true;
bool ProjectWindow::revealRequest_ = false;

namespace {
const std::filesystem::path kRootFolder = "resources"; // 出すのはゲームのフォルダだけ（エンジンの MyEngine/Resources/ は出さない）
constexpr double kRescanSeconds = 1.0;                 // ファイルを読み直す間隔（エクスプローラーで足した物も出るように）
constexpr float kMinIconSize = 16.0f;                  // アイコンの大きさの下限
constexpr float kMaxIconSize = 128.0f;                 // 上限（小さい絵の大きさ AssetThumbnails::kThumbnailSize と同じ）
constexpr float kListModeSize = 24.0f;                 // これ以下なら、1行ずつの並びにする
constexpr float kCellPadding = 4.0f;                   // アイコンの並びの1つ分の、内側の余白
constexpr float kFolderListWidth = 180.0f;             // 左の木の最初の幅（境目をドラッグで変えられる）
constexpr std::string_view kHiddenExtensions[] = {".bak"}; // 出さないファイル（シーンの保存で残る1つ前のファイル）

// パス → 画面に出す文字（UTF-8。区切りは /）。日本語の名前でも文字化けしないように、UTF-8 にしてから ImGui に渡す
std::string ToUtf8(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

// 大文字・小文字を区別しない名前の比べ方（並べる順）
bool NameLess(const std::string& a, const std::string& b) {
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
}

// path が folder の中にあるか（folder そのものも含む）
bool IsInside(const std::filesystem::path& path, const std::filesystem::path& folder) {
	auto it = path.begin();
	for (const std::filesystem::path& part : folder) {
		if (it == path.end() || *it != part) {
			return false;
		}
		++it;
	}
	return true;
}

// ファイルの名前から種類を決める
AssetIcon IconOf(const std::string& fileName) {
	if (fileName.ends_with(kSceneFileExtension)) {
		return AssetIcon::Scene;
	}
	if (IsAssetFile(fileName, AssetType::Model)) {
		return AssetIcon::Model;
	}
	if (IsAssetFile(fileName, AssetType::Texture)) {
		return AssetIcon::Texture;
	}
	if (IsAssetFile(fileName, AssetType::Sound)) {
		return AssetIcon::Sound;
	}
	return AssetIcon::File;
}

// 出さないファイルか
bool IsHidden(const std::string& fileName) {
	return std::any_of(std::begin(kHiddenExtensions), std::end(kHiddenExtensions), [&](std::string_view extension) { return fileName.ends_with(extension); });
}

// 名前を変えるときに打つ部分（シーンは .scene.json の前、ほかのファイルは拡張子の前、フォルダは全部）
std::string EditableName(const std::filesystem::path& path, AssetIcon icon, bool isFolder) {
	const std::string fileName = ToUtf8(path.filename());
	if (isFolder) {
		return fileName;
	}
	if (icon == AssetIcon::Scene) {
		return fileName.substr(0, fileName.size() - std::string_view(kSceneFileExtension).size());
	}
	return ToUtf8(path.stem());
}

// 名前を newName に変えた後のパス（拡張子はそのまま）
std::filesystem::path RenamedPath(const std::filesystem::path& path, AssetIcon icon, bool isFolder, const std::string& newName) {
	if (isFolder) {
		return path.parent_path() / newName;
	}
	if (icon == AssetIcon::Scene) {
		return path.parent_path() / (newName + kSceneFileExtension);
	}
	return path.parent_path() / (newName + path.extension().string());
}

// シーンの名前で開けるシーンファイルか（resources/scenes/ の直下にある物だけ。LoadScene も名前からこの場所を探す）
bool IsOpenableScene(const std::filesystem::path& path, AssetIcon icon, const std::string& name) {
	return icon == AssetIcon::Scene && path == std::filesystem::path(MakeSceneFilePath(name));
}

// folder の中で、まだ使われていない名前（NewFolder・NewFolder1・NewFolder2 …）
std::filesystem::path UniquePath(const std::filesystem::path& folder, const std::string& baseName) {
	std::error_code error;
	for (int number = 0;; ++number) {
		const std::filesystem::path path = folder / (number == 0 ? baseName : baseName + std::to_string(number));
		if (!std::filesystem::exists(path, error)) {
			return path;
		}
	}
}

// まだ使われていないシーンの名前（NewScene・NewScene1 …）
std::string UniqueSceneName(const std::string& baseName) {
	for (int number = 0;; ++number) {
		const std::string name = (number == 0) ? baseName : baseName + std::to_string(number);
		if (!SceneManager::HasScene(name)) {
			return name;
		}
	}
}

// ごみ箱へ移す（Windows のごみ箱から元に戻せる）
bool MoveToRecycleBin(const std::filesystem::path& path) {
	std::error_code error;
	// SHFileOperation は「\0\0」で終わる絶対パスを受け取る（何個もまとめて渡せる形のため。std::wstring の終わりの \0 と足して2つ）
	std::wstring from = std::filesystem::absolute(path, error).make_preferred().wstring();
	from.push_back(L'\0');
	SHFILEOPSTRUCTW operation{};
	operation.wFunc = FO_DELETE;
	operation.pFrom = from.c_str();
	operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI; // ごみ箱へ・確かめるのはこちらで済んだ・進み具合を出さない
	if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted) {
		LogManager::Warning(std::format("ごみ箱へ移せませんでした: {}", ToUtf8(path)));
		return false;
	}
	LogManager::Log(std::format("ごみ箱へ移しました: {}", ToUtf8(path)));
	return true;
}

// エクスプローラーで、その物を選んだ状態で開く
void RevealInExplorer(const std::filesystem::path& path) {
	std::error_code error;
	const std::wstring arguments = L"/select,\"" + std::filesystem::absolute(path, error).make_preferred().wstring() + L"\"";
	ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

// エクスプローラーで、フォルダの中を開く
void OpenInExplorer(const std::filesystem::path& folder) {
	std::error_code error;
	const std::wstring target = std::filesystem::absolute(folder, error).make_preferred().wstring();
	ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// 名前を w の幅に収めて描く（収まれば真ん中、収まらなければ左に寄せて、はみ出した所は切る）
void DrawClippedText(ImDrawList* drawList, const std::string& text, float x, float y, float width) {
	const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
	const float textX = (textWidth <= width) ? x + (width - textWidth) * 0.5f : x;
	const ImVec4 clip(x, y, x + width, y + ImGui::GetTextLineHeight());
	drawList->AddText(nullptr, 0.0f, ImVec2(textX, y), ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, 0.0f, &clip);
}
} // namespace


//=============================================================================
// 描画
//=============================================================================
void ProjectWindow::Draw() {
	ImGui::SetNextWindowSize(ImVec2(800.0f, 300.0f), ImGuiCond_FirstUseEver); // 初めて出すときの大きさ（後は imgui.ini に覚えられる）
	if (!ImGui::Begin("Project")) {
		ImGui::End();
		return; // 隠れている間はファイルを読まない
	}

	// --- ファイルを読み直す（頼まれたとき・1秒ごと）---
	if (rescanRequest_ || ImGui::GetTime() - lastScanTime_ > kRescanSeconds) {
		Rescan();
	}
	std::error_code error;
	if (!std::filesystem::is_directory(kRootFolder, error)) {
		ImGui::TextDisabled("resources/ フォルダがありません（ゲームのプロジェクトのフォルダで起動してください）");
		ImGui::End();
		return;
	}

	// --- 左右に分ける（境目はドラッグで動かせる）---
	if (ImGui::BeginTable("##projectLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("folders", ImGuiTableColumnFlags_WidthFixed, kFolderListWidth);
		ImGui::TableSetupColumn("items", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		// ===== 左：フォルダの木 =====
		ImGui::TableNextColumn();
		if (ImGui::BeginChild("##folderTree")) {
			DrawFolderTree(rootFolder_);
			revealRequest_ = false;
		}
		ImGui::EndChild();

		// ===== 右：今のフォルダの中身 =====
		ImGui::TableNextColumn();
		DrawToolbar();
		ImGui::Separator();
		// 一番下の1行（選んでいる物のパス）の分を残して、残りを中身にする
		if (ImGui::BeginChild("##items", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()))) {
			DrawItems();
			HandleEmptySpace();
		}
		ImGui::EndChild();
		ImGui::TextDisabled("%s", ToUtf8(selected_.empty() ? currentFolder_ : selected_).c_str());
		ImGui::EndTable();
	}
	HandleShortcuts();

	// --- ごみ箱へ移すか聞く（メニューの中から開くと入れ子になるので、ここで開く）---
	if (deleteRequest_) {
		ImGui::OpenPopup("Delete Asset");
		deleteRequest_ = false;
	}
	DrawDeleteDialog();
	ImGui::End();
}

//=============================================================================
// 読む
//=============================================================================
void ProjectWindow::Rescan() {
	lastScanTime_ = ImGui::GetTime();
	rescanRequest_ = false;
	// 見ていたフォルダが消えていたら（名前を変えた・エクスプローラーで消した）、resources/ に戻る
	std::error_code error;
	if (!std::filesystem::is_directory(currentFolder_, error) || !IsInside(currentFolder_, kRootFolder)) {
		currentFolder_ = kRootFolder;
	}
	rootFolder_ = ScanFolderTree(kRootFolder);
	items_ = ScanItems(currentFolder_);
	// 名前を変えている物が無くなっていたら、やめる
	if (!renaming_.empty() && std::none_of(items_.begin(), items_.end(), [](const Item& item) { return item.path == renaming_; })) {
		renaming_.clear();
	}
}

ProjectWindow::Folder ProjectWindow::ScanFolderTree(const std::filesystem::path& path) {
	Folder folder;
	folder.path = path;
	folder.name = ToUtf8(path.filename());
	std::error_code error;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, error)) {
		if (entry.is_directory(error)) {
			folder.children.push_back(ScanFolderTree(entry.path()));
		}
	}
	std::sort(folder.children.begin(), folder.children.end(), [](const Folder& a, const Folder& b) { return NameLess(a.name, b.name); });
	return folder;
}

std::vector<ProjectWindow::Item> ProjectWindow::ScanItems(const std::filesystem::path& folder) {
	std::vector<Item> folders;
	std::vector<Item> files;
	std::error_code error;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, error)) {
		const std::string fileName = ToUtf8(entry.path().filename());
		Item item;
		item.path = entry.path();
		item.isFolder = entry.is_directory(error);
		if (item.isFolder) {
			item.name = fileName;
			item.icon = AssetIcon::Folder;
			folders.push_back(std::move(item));
			continue;
		}
		if (IsHidden(fileName)) {
			continue;
		}
		item.icon = IconOf(fileName);
		// 種類が分かる物は拡張子を出さない（アイコンで分かる。Unity と同じ）。分からない物は拡張子まで出す（plane.mtl と plane.blend を見分けられるように）
		item.name = (item.icon == AssetIcon::File) ? fileName : EditableName(item.path, item.icon, false);
		item.writeTime = entry.last_write_time(error);
		files.push_back(std::move(item));
	}
	// フォルダが先、その後にファイル。それぞれ名前の順
	auto byName = [](const Item& a, const Item& b) { return NameLess(a.name, b.name); };
	std::sort(folders.begin(), folders.end(), byName);
	std::sort(files.begin(), files.end(), byName);
	folders.insert(folders.end(), files.begin(), files.end());
	return folders;
}

//=============================================================================
// 見出し（今のフォルダの場所・アイコンの大きさ）
//=============================================================================
void ProjectWindow::DrawToolbar() {
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端

	// --- 今のフォルダの場所（resources / monsterBall）。押すとそのフォルダへ ---
	const std::filesystem::path folder = currentFolder_; // 押すと currentFolder_ が変わるので、写してから回す
	std::filesystem::path partial;
	for (const std::filesystem::path& part : folder) {
		const bool isFirst = partial.empty();
		partial /= part;
		if (!isFirst) {
			ImGui::SameLine(0.0f, 2.0f);
			ImGui::TextDisabled("/");
			ImGui::SameLine(0.0f, 2.0f);
		}
		ImGui::PushID(ToUtf8(partial).c_str());
		if (ImGui::SmallButton(ToUtf8(part).c_str())) {
			OpenFolder(partial);
		}
		ImGui::PopID();
	}

	// --- 右端：アイコンの大きさ（小さくすると1行ずつの並び）---
	constexpr float kDragWidth = 90.0f;
	ImGui::SameLine(right - kDragWidth);
	ImGui::SetNextItemWidth(kDragWidth);
	ImGui::DragFloat("##iconSize", &iconSize_, 0.5f, kMinIconSize, kMaxIconSize, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SetItemTooltip("アイコンの大きさ（%.0f 以下で1行ずつの並び）", kListModeSize);
}

//=============================================================================
// 左の木
//=============================================================================
void ProjectWindow::DrawFolderTree(const Folder& folder) {
	const bool isCurrent = (folder.path == currentFolder_);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (folder.children.empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (isCurrent) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (folder.path == kRootFolder) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}
	// 右でフォルダの中へ入ったら、そこまでの親を開いて見えるようにする
	if (revealRequest_ && !isCurrent && IsInside(currentFolder_, folder.path)) {
		ImGui::SetNextItemOpen(true);
	}

	ImGui::PushID(ToUtf8(folder.path).c_str());
	const bool isOpen = ImGui::TreeNodeEx("##folder", flags);
	// 押したらそのフォルダの中身を右に出す（矢印を押したときは開く・閉じるだけ）
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		OpenFolder(folder.path);
	}
	// 矢印の右に、アイコンと名前
	ImGui::SameLine();
	const float iconSide = ImGui::GetTextLineHeight();
	const ImVec2 iconMin = ImGui::GetCursorScreenPos();
	AssetIcons::Draw(ImGui::GetWindowDrawList(), isCurrent ? AssetIcon::FolderOpen : AssetIcon::Folder, iconMin, ImVec2(iconMin.x + iconSide, iconMin.y + iconSide));
	ImGui::Dummy(ImVec2(iconSide, iconSide));
	ImGui::SameLine();
	ImGui::TextUnformatted(folder.name.c_str());

	if (isOpen) {
		for (const Folder& child : folder.children) {
			DrawFolderTree(child);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

//=============================================================================
// 右の中身
//=============================================================================
void ProjectWindow::DrawItems() {
	if (items_.empty()) {
		ImGui::TextDisabled("空のフォルダです（右クリック → Create）");
		return;
	}
	// --- 1行ずつの並び ---
	if (iconSize_ <= kListModeSize) {
		for (const Item& item : items_) {
			DrawListItem(item);
		}
		return;
	}
	// --- アイコンの並び（入るだけ横に並べて、はみ出したら次の行）---
	const float cellWidth = iconSize_ + kCellPadding * 2.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cellWidth + spacing)));
	for (size_t i = 0; i < items_.size(); ++i) {
		if (i % static_cast<size_t>(columns) != 0) {
			ImGui::SameLine();
		}
		DrawGridItem(items_[i], cellWidth);
	}
}

void ProjectWindow::DrawGridItem(const Item& item, float cellWidth) {
	const bool isRenaming = (item.path == renaming_);
	const float textHeight = ImGui::GetTextLineHeight();
	const ImVec2 cellSize(cellWidth, kCellPadding + iconSize_ + kCellPadding + textHeight + kCellPadding);

	ImGui::PushID(ToUtf8(item.path).c_str());
	ImGui::BeginGroup();
	const ImVec2 cellMin = ImGui::GetCursorScreenPos();
	// 押せる四角（選ぶ・ダブルクリック・右クリック。マウスが乗る・選ぶと色が付く）。名前の変更中は、下に入力欄を重ねる
	ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
	if (isRenaming) {
		flags |= ImGuiSelectableFlags_AllowOverlap;
	}
	ImGui::Selectable("##cell", item.path == selected_, flags, cellSize);
	HandleItemInput(item);

	// アイコン（上の真ん中）
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 iconMin(cellMin.x + (cellWidth - iconSize_) * 0.5f, cellMin.y + kCellPadding);
	DrawItemIcon(drawList, item, iconMin, ImVec2(iconMin.x + iconSize_, iconMin.y + iconSize_));

	// 名前（下）。変更中は入力欄
	const float textY = iconMin.y + iconSize_ + kCellPadding;
	if (isRenaming) {
		ImGui::SetCursorScreenPos(ImVec2(cellMin.x, textY - ImGui::GetStyle().FramePadding.y));
		ImGui::SetNextItemWidth(cellWidth);
		DrawRenameField(item);
	} else {
		DrawClippedText(drawList, LabelOf(item), cellMin.x, textY, cellWidth);
	}
	ImGui::EndGroup();
	ImGui::PopID();
}

void ProjectWindow::DrawListItem(const Item& item) {
	const bool isRenaming = (item.path == renaming_);
	const float rowHeight = ImGui::GetFrameHeight();

	ImGui::PushID(ToUtf8(item.path).c_str());
	const ImVec2 rowMin = ImGui::GetCursorScreenPos();
	ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
	if (isRenaming) {
		flags |= ImGuiSelectableFlags_AllowOverlap;
	}
	ImGui::Selectable("##row", item.path == selected_, flags, ImVec2(0.0f, rowHeight)); // 幅0＝横いっぱい
	HandleItemInput(item);

	// 左にアイコン、右に名前
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float iconSide = rowHeight - 2.0f;
	DrawItemIcon(drawList, item, ImVec2(rowMin.x, rowMin.y + 1.0f), ImVec2(rowMin.x + iconSide, rowMin.y + 1.0f + iconSide));
	const float textX = rowMin.x + iconSide + ImGui::GetStyle().ItemInnerSpacing.x;
	if (isRenaming) {
		ImGui::SetCursorScreenPos(ImVec2(textX, rowMin.y));
		ImGui::SetNextItemWidth(-FLT_MIN);
		DrawRenameField(item);
	} else {
		const float textY = rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
		drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), LabelOf(item).c_str());
	}
	ImGui::PopID();
}

void ProjectWindow::DrawItemIcon(ImDrawList* drawList, const Item& item, const ImVec2& min, const ImVec2& max) {
	// 見えていない所は描かない（小さい絵も作らない）
	if (!ImGui::IsRectVisible(min, max)) {
		return;
	}
	// 画像ファイルは、その画像の小さい絵（縦横比を保って、四角の真ん中に）
	if (item.icon == AssetIcon::Texture) {
		ImTextureID texture = ImTextureID_Invalid;
		ImVec2 size;
		if (AssetThumbnails::Get(ToUtf8(item.path), item.writeTime, texture, size)) {
			const ImVec2 box(max.x - min.x, max.y - min.y);
			const float scale = (std::min)(box.x / size.x, box.y / size.y);
			const ImVec2 drawSize(size.x * scale, size.y * scale);
			const ImVec2 drawMin(min.x + (box.x - drawSize.x) * 0.5f, min.y + (box.y - drawSize.y) * 0.5f);
			drawList->AddImage(texture, drawMin, ImVec2(drawMin.x + drawSize.x, drawMin.y + drawSize.y));
			return;
		}
		// まだ作っていない（次のフレーム以降に作る）・作れなかった → アイコンで代わりに描く
	}
	AssetIcons::Draw(drawList, item.icon, min, max);
}

std::string ProjectWindow::LabelOf(const Item& item) {
	// 開いているシーンに保存していない変更があれば、名前に * を付ける（Control ウィンドウと同じ）
	const SceneManager* sceneManager = SceneManager::GetMain();
	if (sceneManager && sceneManager->IsDirty() && item.name == sceneManager->GetOpenSceneName() && IsOpenableScene(item.path, item.icon, item.name)) {
		return item.name + " *";
	}
	return item.name;
}

//=============================================================================
// 項目の上でのクリック
//=============================================================================
void ProjectWindow::HandleItemInput(const Item& item) {
	// --- 左・右クリックで選ぶ ---
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		selected_ = item.path;
	}
	// --- ダブルクリックで開く（フォルダは中へ、シーンはエディタで開く）---
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		Open(item);
	}
	// --- マウスを乗せたらファイルの名前（拡張子まで。名前が長くて切れていても分かる）---
	ImGui::SetItemTooltip("%s", ToUtf8(item.path.filename()).c_str());
	// --- 右クリックのメニュー ---
	if (ImGui::BeginPopupContextItem("itemMenu")) {
		DrawItemMenu(item);
		ImGui::EndPopup();
	}
}

void ProjectWindow::DrawItemMenu(const Item& item) {
	// --- 開く ---
	if (item.isFolder) {
		if (ImGui::MenuItem("Open")) {
			Open(item);
		}
	} else if (item.icon == AssetIcon::Scene) {
		const SceneManager* sceneManager = SceneManager::GetMain();
		const bool canOpen = IsOpenableScene(item.path, item.icon, item.name) && sceneManager && sceneManager->GetPlayState() == PlayState::Editing;
		if (ImGui::MenuItem("Open", nullptr, false, canOpen)) {
			Open(item);
		}
		ImGui::SetItemTooltip("停止中に、resources/scenes/ のシーンだけ開けます");
	}
	ImGui::Separator();

	// --- 名前を変える・消す（開いているシーンと、それが入っているフォルダはできない）---
	const bool isLocked = IsLocked(item);
	if (ImGui::MenuItem("Rename", "F2", false, !isLocked)) {
		StartRename(item);
	}
	if (ImGui::MenuItem("Delete", "Del", false, !isLocked)) {
		deleteTarget_ = item.path;
		deleteRequest_ = true;
	}
	if (isLocked) {
		ImGui::TextDisabled("開いているシーンは、名前を変える・消すができません");
	} else if (item.icon != AssetIcon::Scene) {
		ImGui::TextDisabled("名前を変える・消すと、使っているシーンからは見つからなくなります");
	}
	ImGui::Separator();

	// --- そのほか ---
	if (ImGui::MenuItem("Copy Path")) {
		ImGui::SetClipboardText(ToUtf8(item.path).c_str()); // Inspector やコードに貼れるように、resources/ から始まるパスで
	}
	if (ImGui::MenuItem("Show in Explorer")) {
		RevealInExplorer(item.path);
	}
	ImGui::Separator();
	DrawCreateMenu();
}

void ProjectWindow::DrawCreateMenu() {
	if (ImGui::BeginMenu("Create")) {
		if (ImGui::MenuItem("Folder")) {
			CreateFolder();
		}
		if (ImGui::MenuItem("Scene")) {
			CreateScene();
		}
		ImGui::SetItemTooltip("シーンは resources/scenes/ に作ります（シーンは名前で探すので、置き場所は1つ）");
		ImGui::EndMenu();
	}
}

//=============================================================================
// 何も無い所
//=============================================================================
void ProjectWindow::HandleEmptySpace() {
	// --- 何も無い所をクリックしたら、選ぶのをやめる ---
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		selected_.clear();
	}
	// --- 何も無い所の右クリック（項目の上では項目のメニューが出る）---
	if (ImGui::BeginPopupContextWindow("emptySpaceMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		DrawCreateMenu();
		ImGui::Separator();
		if (ImGui::MenuItem("Show in Explorer")) {
			OpenInExplorer(currentFolder_);
		}
		if (ImGui::MenuItem("Refresh")) {
			rescanRequest_ = true;
		}
		ImGui::EndPopup();
	}
}

//=============================================================================
// ショートカット（Project ウィンドウを触っているときだけ）
//=============================================================================
void ProjectWindow::HandleShortcuts() {
	// 名前の入力中・ほかの入力中は、入力欄のほうを優先する
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || !renaming_.empty() || ImGui::GetIO().WantTextInput) {
		return;
	}
	const Item* item = FindSelected();
	if (item == nullptr || IsLocked(*item)) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
		StartRename(*item);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		deleteTarget_ = item->path;
		deleteRequest_ = true;
	}
}

//=============================================================================
// 開く・作る
//=============================================================================
void ProjectWindow::Open(const Item& item) {
	if (item.isFolder) {
		OpenFolder(item.path);
		return;
	}
	// シーンはエディタで開く（保存していない変更があれば、先に聞く）
	if (IsOpenableScene(item.path, item.icon, item.name)) {
		SceneDialogs::RequestOpen(item.name);
	}
	// そのほかのファイルは、今は何もしない（後で：Inspector へドラッグ）
}

void ProjectWindow::OpenFolder(const std::filesystem::path& folder) {
	currentFolder_ = folder;
	selected_.clear();
	renaming_.clear();
	rescanRequest_ = true; // 中身は次のフレームの頭で読む（今は中身の一覧を回している途中かもしれないので）
	revealRequest_ = true;
}

void ProjectWindow::CreateFolder() {
	const std::filesystem::path path = UniquePath(currentFolder_, "NewFolder");
	std::error_code error;
	if (!std::filesystem::create_directory(path, error)) {
		LogManager::Warning(std::format("フォルダを作れませんでした: {}", ToUtf8(path)));
		return;
	}
	// 作ったフォルダを選んで、すぐに名前を打てるようにする（Unity と同じ）
	selected_ = path;
	renaming_ = path;
	renameBuffer_ = ToUtf8(path.filename());
	renameFocusRequest_ = true;
	rescanRequest_ = true;
}

void ProjectWindow::CreateScene() {
	const std::string name = UniqueSceneName("NewScene");
	if (!SceneManager::CreateSceneFile(name)) {
		return;
	}
	// resources/scenes/ を開いて、作ったシーンの名前をすぐに打てるようにする
	const std::filesystem::path path = MakeSceneFilePath(name);
	OpenFolder(path.parent_path());
	selected_ = path;
	renaming_ = path;
	renameBuffer_ = name;
	renameFocusRequest_ = true;
}

//=============================================================================
// 名前の変更
//=============================================================================
void ProjectWindow::StartRename(const Item& item) {
	if (IsLocked(item)) {
		return;
	}
	selected_ = item.path;
	renaming_ = item.path;
	renameBuffer_ = EditableName(item.path, item.icon, item.isFolder); // 拡張子は変えない
	renameFocusRequest_ = true;
}

void ProjectWindow::DrawRenameField(const Item& item) {
	// 出した最初のフレームだけ、入力欄にフォーカスを移す（すぐに文字を打てるように）
	const bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全部選んだ状態（そのまま打つと置き換わる）
	const bool entered = EditorWidgets::InputFileName("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	// 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）。Esc で抜けたら ImGui が元に戻すので、変わらない
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		ApplyRename(item);
	}
	// 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる（Hierarchy の名前の変更と同じ）
	bool finished = entered || ImGui::IsItemDeactivated();
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_.clear();
	}
}

void ProjectWindow::ApplyRename(const Item& item) {
	if (renameBuffer_ == EditableName(item.path, item.icon, item.isFolder)) {
		return; // 変わっていない
	}
	if (!IsValidFileName(renameBuffer_)) {
		LogManager::Warning(std::format("名前に使えるのは、英数字と _ - だけです: \"{}\"", renameBuffer_));
		return;
	}
	const std::filesystem::path newPath = RenamedPath(item.path, item.icon, item.isFolder, renameBuffer_);
	std::error_code error;
	// 同じ名前がもうあるならやめる（大文字・小文字だけ変えるときは、Windows では同じファイルなので通す）
	if (std::filesystem::exists(newPath, error) && !std::filesystem::equivalent(item.path, newPath, error)) {
		LogManager::Warning(std::format("同じ名前がもうあります: {}", ToUtf8(newPath)));
		return;
	}
	std::filesystem::rename(item.path, newPath, error);
	if (error) {
		LogManager::Warning(std::format("名前を変えられませんでした: {}（{}）", ToUtf8(item.path), error.message()));
		return;
	}
	LogManager::Log(std::format("名前を変えました: {} → {}", ToUtf8(item.path), ToUtf8(newPath)));
	if (selected_ == item.path) {
		selected_ = newPath;
	}
	rescanRequest_ = true;
}

//=============================================================================
// ごみ箱へ移すか聞く
//=============================================================================
void ProjectWindow::DrawDeleteDialog() {
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	ImGui::Text("%s をごみ箱へ移しますか？", ToUtf8(deleteTarget_).c_str());
	ImGui::TextDisabled("（Windows のごみ箱から元に戻せます）");
	ImGui::Spacing();
	if (ImGui::Button("Delete")) {
		MoveToRecycleBin(deleteTarget_);
		if (IsInside(selected_, deleteTarget_)) {
			selected_.clear();
		}
		rescanRequest_ = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

//=============================================================================
// 小さな道具
//=============================================================================
bool ProjectWindow::IsLocked(const Item& item) {
	const SceneManager* sceneManager = SceneManager::GetMain();
	if (sceneManager == nullptr || sceneManager->GetOpenSceneName().empty()) {
		return false;
	}
	// 開いているシーンそのもの、またはそれが入っているフォルダ（名前を変えると、保存する先が無くなってしまう）
	const std::filesystem::path openFile = MakeSceneFilePath(sceneManager->GetOpenSceneName());
	return item.path == openFile || (item.isFolder && IsInside(openFile, item.path));
}

const ProjectWindow::Item* ProjectWindow::FindSelected() {
	for (const Item& item : items_) {
		if (item.path == selected_) {
			return &item;
		}
	}
	return nullptr;
}
#endif
```

**解説（Project ウィンドウ）**

| できること | 操作 | 中で呼ぶ物 |
|---|---|---|
| フォルダの中を見る | 左の木をクリック・右でフォルダをダブルクリック・上の `resources / monsterBall` を押す | `OpenFolder` |
| シーンを開く | シーンをダブルクリック（右クリック → Open も） | `SceneDialogs::RequestOpen`（保存していなければ聞く） |
| シーンを作る | 何も無い所を右クリック → Create → Scene | `SceneManager::CreateSceneFile` → `resources/scenes/` を開いて、名前を打てる状態に（Unity と同じ） |
| フォルダを作る | Create → Folder | `std::filesystem::create_directory` → 名前を打てる状態に |
| 名前を変える | F2・右クリック → Rename・（作った直後） | `std::filesystem::rename`。拡張子（`.scene.json`・`.png`）はそのまま |
| 消す | Delete・右クリック → Delete → 「ごみ箱へ移しますか？」 | `SHFileOperationW`（`FOF_ALLOWUNDO`＝ごみ箱へ） |
| パスを写す | 右クリック → Copy Path | `resources/monsterBall/monsterBall.png` の形でクリップボードへ（コードに貼れる） |
| エクスプローラーで開く | 右クリック → Show in Explorer | `ShellExecuteW` |
| 大きさを変える | 右上の数字をドラッグ（24 以下で1行ずつの並び） | `iconSize_` |

- **開いているシーン（と、それが入っているフォルダ）は、名前を変える・消すができない**（保存する先が無くなるので）。
- **シーン以外のファイル・フォルダの名前を変える・消すと、それを使っているシーン・モデルからは見つからなくなる**（シーンファイルにはパスで書いてあるので）。メニューに注意を出している。中のパスも一緒に書き換えるのは「後で」（3章の SF2 の表）。
- ファイルは **1秒ごとに読み直す**（エクスプローラーで足した・消した物もすぐ出る。フォルダ1つ分なので軽い）。
- 名前は UTF-8 にしてから ImGui に渡す（`ToUtf8`）。`path.string()` は Windows では Shift-JIS になり、日本語のファイル名が文字化けする。
- `#define NOMINMAX` を先頭に書いているのは、ごみ箱・エクスプローラーのために `<Windows.h>` を読むから（`std::min` / `std::max` がマクロに邪魔されないように）。

#### H-5. `Editor/ImGuiManager.cpp`（最後に。7か所）

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Editor/Profiler.h"
#include "MyEngine/Editor/Gizmos/LightGizmo.h"
（略）
#include "MyEngine/Editor/Windows/InspectorWindow.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Window/Win32Window.h"

// ===== 後 =====
#include "MyEngine/Editor/Profiler.h"
#include "MyEngine/Editor/Assets/AssetIcons.h"
#include "MyEngine/Editor/Assets/AssetThumbnails.h"
#include "MyEngine/Editor/Gizmos/LightGizmo.h"
（略）
#include "MyEngine/Editor/Windows/InspectorWindow.h"
#include "MyEngine/Editor/Windows/ProjectWindow.h"
#include "MyEngine/Editor/Windows/SceneDialogs.h"
#include "MyEngine/Editor/Windows/SettingsWindow.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Window/Win32Window.h"
```

```cpp
// ===== 前（2か所目：Initialize の最後の方）=====
	// InspectorにエンジンのComponent（Transform・ModelRenderer）を登録する
	InspectorWindow::Initialize();

// ===== 後 =====
	// InspectorにエンジンのComponent（Transform・ModelRenderer）を登録する
	InspectorWindow::Initialize();
	// Project ウィンドウのアイコンの画像（MyEngine/Resources/Editor/Icons/）を読む
	AssetIcons::Initialize();
```

```cpp
// ===== 前（3か所目：Release）=====
	EditorHistory::Clear();
	ImGui_ImplDX12_Shutdown();

// ===== 後 =====
	EditorHistory::Clear();
	AssetThumbnails::Release(); // Project ウィンドウの小さい絵（GPU の物なので、DirectX より先に捨てる）
	ImGui_ImplDX12_Shutdown();
```

```cpp
// ===== 前（4か所目：Begin の dockFlags）=====
	                             ImGuiWindowFlags_NoMove |     // 移動なし
	                             ImGuiWindowFlags_NoNavFocus;  // ナビゲーションフォーカスなし

// ===== 後 =====
	                             ImGuiWindowFlags_NoMove |     // 移動なし
	                             ImGuiWindowFlags_NoBringToFrontOnFocus | // ドッキングしたウィンドウを触っても前に出てこない（浮いているウィンドウが後ろに隠れないように）
	                             ImGuiWindowFlags_NoNavFocus;  // ナビゲーションフォーカスなし
```

- 4か所目は検証で見つけた物：ドッキングの土台のウィンドウ（画面いっぱいの `DockSpaceWindow`）は、ドッキングしたウィンドウを触るたびに一番前に出てくる。すると、**浮いているウィンドウ（初めて出した Project・Settings）が土台の後ろに隠れて見えなくなる**。`NoBringToFrontOnFocus` で土台を前に出さないようにする（ImGui のドッキングの例と同じ設定）。

```cpp
// ===== 前（5か所目：File メニュー）=====
		if (ImGui::BeginMenu("File")) {
			// jsonから全シーンデータを再読み込み
			if (ImGui::MenuItem("Reload All Variables")) {
				GlobalVariables::GetInstance()->LoadFiles();
			}
			// ImGuiウィンドウ配置をimgui.iniに書き出し

// ===== 後 =====
		if (ImGui::BeginMenu("File")) {
			// シーン（New / Open / Save / Save As）
			SceneDialogs::DrawFileMenuItems();
			ImGui::Separator();
			// ImGuiウィンドウ配置をimgui.iniに書き出し
```

- Reload All Variables は無くした（Settings のページごとの Revert で、保存した値に戻せる）。

```cpp
// ===== 前（6か所目：Edit メニュー）=====
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, EditorHistory::CanRedo())) {
				EditorHistory::RequestRedo();
			}
			ImGui::EndMenu();

// ===== 後 =====
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, EditorHistory::CanRedo())) {
				EditorHistory::RequestRedo();
			}
			ImGui::Separator();
			// ゲーム全体・シーン・エディタの設定（Unity の Edit → Project Settings と同じ場所）
			if (ImGui::MenuItem("Settings")) {
				SettingsWindow::Focus();
			}
			ImGui::EndMenu();
```

```cpp
// ===== 前（7か所目：ウィンドウを描く所）=====
	// ===== 調整項目の更新 =====
	GlobalVariables::GetInstance()->Update();
（略：Profiler・Hierarchy）
	// ===== インスペクター（ヒエラルキーで選んだEntityを編集する）=====
	InspectorWindow::Draw();

// ===== 後 =====
	// ===== 設定（調整項目。前の Parameters ウィンドウ）=====
	SettingsWindow::Draw();
（略：Profiler・Hierarchy）
	// ===== インスペクター（ヒエラルキーで選んだEntityを編集する）=====
	InspectorWindow::Draw();

	// ===== プロジェクト（resources/ のファイル）=====
	ProjectWindow::Draw();

	// ===== シーンのファイルの確認（保存しますか？・名前を付けて保存など）=====
	SceneDialogs::Draw();
```

---

### パート I：ゲーム側（`CG3_Project`）

#### I-1. `main.cpp`（1か所）

```cpp
// ===== 前 =====
	Engine::Initialize(wc, "Result");

// ===== 後 =====
	// 製品版で最初に出るシーン（resources/scenes/GameScene.scene.json）。エディタは前回最後に開いていたシーンから始まる
	Engine::Initialize(wc, "GameScene");
```

- これからは、編集したいシーンのために `main.cpp` を書き換えなくてよい（Project ウィンドウでダブルクリック）。`main.cpp` は「製品版で最初に出るシーン」だけ。

#### I-2. `Source/Components/AudioKeyTest.cpp` を消す

ソリューションエクスプローラーで右クリック → 削除（ファイルも消す）。シーンファイルからはもう外してある（11章）。

---

### パート J：ビルドの設定（複数プロセッサによるコンパイル）

13章の①。エンジンとゲームの**両方**でやる。

1. ソリューションエクスプローラーで **MyEngine_v1** を右クリック → プロパティ。
2. 上の「構成」を **すべての構成**、「プラットフォーム」を **x64** にする。
3. 構成プロパティ → **C/C++ → 全般 → 複数プロセッサによるコンパイル → はい (/MP)** → OK。
4. **CG3_Project** でも同じことをする。

- vcxproj には `<MultiProcessorCompilation>true</MultiProcessorCompilation>` が Debug と Release の両方に入る（git の差分で確かめられる）。
- **複数プロセッサによるコンパイルとぶつかる設定**（「最小リビルドを有効にする（/Gm）」）は、今のプロジェクトでは使っていないので、そのままでよい。

---

## 16. 確認すること

**ビルド**

- [ ] エンジン（Debug・Release）がエラー0、警告が前と同じ（`DIRECTINPUT_VERSION` の2つだけ）
- [ ] ゲーム（Debug・Release）がエラー0・警告0
- [ ] パート J の後、ビルドの出力に、同時にいくつもの cpp の名前が並んで出る（1つずつではない）

**Add Component（パート D）**

- [ ] Component が多い Entity（Player など）で Inspector を一番下までスクロールし、Add Component を押す → ボタンの**上**にポップアップが出て、全部見える
- [ ] カテゴリを全部開くと、**一覧だけ**スクロールできる（検索欄は上に残る）
- [ ] Component を選ぶとポップアップが閉じて、Component が付く
- [ ] Component が少ない Entity では、今まで通りボタンの**下**に出る

**Settings ウィンドウ（パート G）**

- [ ] 起動すると Settings ウィンドウが浮いて出る（初めてのとき）。前の Parameters・PostEffect・Audio・Debug・GameScene のウィンドウは出ない。**Console の横などにドラッグしてドッキング**しておく
- [ ] 左に Project（Audio・PostEffect・GameScene）／Scene: GameScene／Editor（Scene View）
- [ ] PostEffect の値を変える → 一覧とページの見出しとカテゴリに `*` → **Revert** で戻る → **Ctrl+Z** で変えた値にもどる → **Save** で `*` が消え、`Parameters/PostEffect/` にファイルができる
- [ ] 値を変えて（Save せずに）Play → Stop → 値が戻らない
- [ ] `Parameters/Debug/DebugCamera.json` が `Parameters/Editor/Scene View/DebugCamera.json` に移っていて、Scene ビューのカメラが前の位置から始まる
- [ ] Edit → Settings で Settings ウィンドウが前に出る

**Project ウィンドウ（パート H）**

- [ ] 起動すると Project ウィンドウが浮いて出る（初めてのとき）。**下の Console の横などにドッキング**しておく
- [ ] 左の木で `monsterBall` を押す → 右に `monsterBall.png`・`room.hdr`・`sky.hdr` の**小さい絵**、モデル（M）・その他（灰色）のアイコン
- [ ] `Goblets/textures` を開く → 4K の画像の小さい絵が1枚ずつ出てくる（止まらない）
- [ ] 右上の数字を小さく（24 以下）すると1行ずつの並び、大きくするとアイコンが大きくなる
- [ ] マウスを乗せると、拡張子まで入ったファイルの名前が出る。下の行に、選んでいる物のパス

**シーンを作る・開く（パート F・H）**

- [ ] 何も無い所を右クリック → Create → Scene → `resources/scenes` が開き、`NewScene` の名前を打てる → `Title` と打って Enter → `Title` ができる
- [ ] `Title` をダブルクリック → 開く（カメラと平行光源だけ。Control に `Scene: Title`）
- [ ] 何か Entity を足す → Control と Project の `Title` に `*` → Ctrl+Z → `*` が消える
- [ ] もう一度足して、`GameScene` をダブルクリック → 「保存しますか？」→ **Cancel**（開かない）／**Don't Save**（捨てて開く）／**Save**（保存して開く）をそれぞれ確かめる
- [ ] File → New Scene（Ctrl+N）・Open Scene・Save As（Ctrl+Shift+S）
- [ ] 名前の欄に日本語・空白・記号が打てない
- [ ] 開いているシーン（と `scenes` フォルダ）は、右クリックの Rename・Delete が押せない
- [ ] ほかのシーンを Delete → 「ごみ箱へ移しますか？」→ Delete → ごみ箱に入っている（元に戻せる）
- [ ] 再生中は、シーンのダブルクリック・File の New / Open / Save As が効かない
- [ ] `Title` を開いたまま終了 → 起動すると **Title が開く**（`main.cpp` は GameScene のまま）
- [ ] Release（製品版）は、いつも `main.cpp` のシーン（GameScene）から始まる

**アイコン（14章。描いたら）**

- [ ] `Resources/Editor/Icons/` に置いてエンジンをビルド → 起動すると描いた絵になる

**確認が終わったら**

- [ ] 8章の残り（SceneKeyTest で Enter / Backspace / M など）も済んだら、`SceneKeyTest` を Entity から外して Save → `SceneKeyTest.cpp` を消す
- [ ] 試しに作ったシーン（`Title` など）は、要らなければ Project で Delete
- [ ] コミット（エンジンは `feature/project-window`）→ develop へは 13章の②のやり方で

---

## 17. 検証したこと（2026-09-30 2回目、Claude）

- **やり方**：本体（873b470）とゲーム（`CG3_Project`）を検証用のフォルダに写し、11章のゲーム側の2つとパート D〜I をそのまま入れて、MSBuild でビルド・起動した（本体とゲームのファイルは触っていない）。
- **ビルド**：エンジンとゲームを Debug・Release とも**全部ビルドし直して**エラー0。警告は前と同じ（エンジンの `DIRECTINPUT_VERSION` の2つと、Debug のリンクの `/utf-8` の1つ）、ゲームは0。`/MP` をオンにしてビルドしている（パート J と同じ）。
- **動かした確認**（自動で操作する確認用の main を作って実行。**89項目すべて OK**）：
  - 起動：`LastScene.json` ができる・`Parameters/Debug/DebugCamera.json` が `Parameters/Editor/Scene View/` に中身そのままで移る・Settings の見出し（Project：Audio・PostEffect・GameScene／Editor：Scene View）・保存先の表示
  - Settings：値を変えると `*`・Revert で戻る・その Revert を Ctrl+Z で戻せる・保存していない値が Play → Stop で戻らない（PostEffect も GameScene も）・**後から登録したグループがファイルの値から始まる**（入れ子のカテゴリ・Vector3 も）・Save で `*` が消えてファイルに書かれる・LoadGroup
  - シーンの `*`：Entity を足すと `*`・Undo で消える・Redo でまた付く・Play → Stop しても `*` のまま・消して元に戻すと消える
  - 作る：`CreateSceneFile` で空のファイル・同じ名前／空白入り／空／日本語は断る・`GetSceneNames`
  - 開く：空のシーンを開くとカメラと平行光源・DefaultScene・`*` なし・`LastScene.json` が替わる・保存・Save As（ファイルができて、そちらを開く）・Play → `LoadScene` → Stop で開いたシーンに戻る・**再生中は開かない**・**「保存 → 開く」を同じフレームに頼むと、保存してから開く**
  - 「保存しますか？」：`*` があるときだけ出る（無ければすぐ開く）
  - Project：フォルダを作る → 名前を変える（空白入りは断る・大文字小文字だけの変更も通る）・Create → Scene で `resources/scenes` に作って名前を打てる状態・開いているシーンと `scenes` フォルダは Rename / Delete できない・シーンの名前を変えても `.scene.json` は残る
  - 小さい絵：png・4K の jpg・hdr・1×1 の png で作れる（長い辺 128、hdr は 128×64）・同じ物は作り直さない・**60枚作っても SRV のスロットは 48 枚分しか増えない**（古い物から捨てる）
  - Add Component：Player を選び、Inspector を一番下までスクロールして開く → **ボタンの上に開いて画面に収まる**・カテゴリを全部開くと**一覧だけスクロールする**（高さ 316 に中身 374）
- **画面**：Project ウィンドウ（`monsterBall` の小さい絵・`Goblets/textures` の 4K の画像・1行ずつの並び・左の木）、「保存しますか？」、Add Component（上に開いてスクロール）を撮った画面で確認した。ここで「浮いているウィンドウが、ドッキングしたウィンドウを触ると後ろに隠れる」のを見つけて、H-5 の4か所目で直した。
- **本物の main.cpp**：Debug・Release とも起動して、閉じるまで落ちない（終わりのコード 0）。
- **確かめていないこと**：マウス・キーボードでの操作（右クリックのメニュー・ダブルクリック・名前の入力・ドラッグ・Ctrl+N などのショートカット）、ごみ箱へ移す（検証の PC のごみ箱に入るので、やっていない）、エクスプローラーで開く。16章でお願いします。

---

## 18. 写したコードの確認（bd7e4c2、2026-10-01）

15章（パート D〜J）のコードの行が、写した物（本体 bd7e4c2・ゲームの今のファイル）にあるかを、機械で1行ずつ調べた（3422 行）。見つからなかった 36 行のうち、ほとんどは**書き方の違いだけ**（2行を1行にまとめた・空白のそろえ方）で、動きは同じ。**動きが違うのは1か所**（G-3）。

| ファイル | 違い | どうなるか | 直し |
|---|---|---|---|
| `Editor/Viewport/EditorViewport.cpp`（G-3） | `RegisterGV` の `->Group(kGVGroup)` に `SettingsSection::Editor` が付いていない。代わりに `Initialize` の `RegisterGV();` の後に `Group(kGVGroup, SettingsSection::Editor)` の1行がある | **置き場所はグループを最初に作った呼び出しで決まる**（`Group` は、もうあるグループなら何もしない）。先に `RegisterGV` が「Project」で作ってしまうので、後の1行は効かない → Settings ウィンドウで **Scene View が Project の見出しの下**に出て、Save すると `Parameters/Scene View/`（ゲームと一緒に配る方）に書かれる | **パート K-5** |
| 同じ（G-3） | `MoveOldCameraFile`（前の `Parameters/Debug/DebugCamera.json` を移す）と `<filesystem>`・`<format>` の include が無い | ゲームの `Parameters/` にはもう `Debug/` が無いので、**無くて困らない**（移す物が無い） | 足さなくてよい |
| `Editor/Windows/InspectorWindow.cpp`（D の2か所目） | `if (ImGui::Selectable(...)) {` の行の右に、前のコメント `// Selectableを押すとポップアップは自動で閉じる` が残っている | 動きは同じ。すぐ下のコメント（自動では閉じない）と逆のことが書いてある | **パート K-5** |
| `Editor/Viewport/EditorViewport.cpp` 80行目 | `// Game：全部          // Game：ブルームあり`（11章で書いた物がそのまま） | 動きは同じ | **パート K-5** |
| `MyEngine_v1.vcxproj`（J） | 複数プロセッサによるコンパイル（/MP）が **Debug だけ**。Release とゲーム（`CG3_Project`）はオフのまま | Release・ゲームのビルドが1ファイルずつ | **パート K-6** |

- **ビルド**：写した物そのまま（直す前）で、エンジンとゲームの Debug・Release ともエラー0。警告は前と同じ（エンジンの `DIRECTINPUT_VERSION` 2つ・Debug のリンクの `/utf-8` 1つ、ゲーム0）。
- 描いてもらったアイコン7つも入っている（14章）。大きさの話は 19.3。

---

## 19. 質問・気になることへの答え（2026-10-01）

### 19.1 `ComponentUI.cpp` の `generic_string()` は C++26 で非推奨。大丈夫？ → 今は動くが、非推奨になった理由がこのエンジンの「日本語のファイル名が読めない」原因そのもの。一覧は UTF-8 に直す（パート K-3）

- **今（C++23）**：このプロジェクトは C++23（`stdcpp23`）なので、警告も出ずに動く。C++26 でも「非推奨」は「まだ使えるが、そのうち消すかも」なので、すぐ壊れはしない。
- **非推奨になった理由**：引数の無い `string()` / `generic_string()` は「その PC の文字コード」で返す。**日本語の Windows では Shift-JIS**。ところが ImGui・`std::format`・json・このエンジンの Manager は、文字を **UTF-8** のつもりで扱う。ずれると文字化けしたり、変換できずに例外が出たりする。代わりは、表示用の `generic_display_string()`、Windows の古い API に渡す用の `generic_native_encoded_string()`（どちらも C++26）。
- **確かめたこと**（検証用の小さなプログラム）：`resources/テスト.png` のパスで

| 作り方 | 「テスト」の部分のバイト | |
|---|---|---|
| `generic_string()` | `83 65 83 58 83 67` | Shift-JIS |
| `ToUtf8()`（パート K-3 で足す） | `E3 83 86 E3 82 B9 E3 83 88` | UTF-8 |
| UTF-8 の文字を `std::filesystem::path(文字)` に渡す | 例外「No mapping for the Unicode character exists in the target multi-byte code page.」 | 受け取る側が Shift-JIS だと思って読むため |

- **このエンジンで起きること**：日本語の名前の画像を `resources/` に入れると、Inspector の一覧には Shift-JIS の文字が入る（ImGui では文字化け）→ 選ぶと TextureManager が UTF-8 だと思って読むので読めない → **assert で止まる**。Audio.md 8章に書いた「日本語のファイル名は読めない」はこれ。課題のゲームで素材サイトの画像を入れると、日本語の名前のことがあるので当たりやすい。
- **直し（パート K-3）**：
  1. Project ウィンドウの中にあった `ToUtf8(path)`（パス → UTF-8 の文字）を `String/ConvertString.h` に移して、どこからでも使えるようにする（中身は、今ある `ConvertString(wstring)` を呼ぶだけ）。
  2. Inspector の一覧を `ToUtf8` で作る（`generic_string()` を使わない）。
  3. **英数字以外が入った名前は一覧に出さない**。Console に「名前を英数字にしてください」と出す（選んで落ちるより、出ない方が原因が分かる）。
  - 日本語の名前を**読めるようにする**のは、Manager（モデル・テクスチャ・音）の中のパスの扱いも全部直すので、今回はやらない（Audio.md 8章のまま。エディタで作る名前は、もう英数字しか打てない）。
- 他の `.string()` / `generic_string()`（SceneManager・GlobalVariables のログなど）は、英数字だけのパスなので今は困らない。C++26 に上げるときに `ToUtf8` へまとめて替える。

### 19.2 `UploadContext` の `TransitionFromCopyDest` は、似た関数がもうあった。どこかのクラスの public 関数にしてもいいのでは？ → そのとおり。`DirectXCommon::TransitionBarrier` に「コマンドリストを渡す版」を足して、それを使う（パート K-2）

- **似た関数**：`DirectXCommon::TransitionBarrier(resource, before, after)`。バリアの詰め方は同じ。md を書いたとき、CLAUDE.md の「namespace に関数を作る前に、public にもう無いか確認する」をやっていなかった。
- **ただ、そのままは呼べなかった**：`DirectXCommon::TransitionBarrier` は**描画のコマンドリスト**に積む。UploadContext は**自分のコマンドリスト**でコピーしているので、バリアもそちらに積まないと「コピーと違う時に状態が変わる」（D3D12 のデバッグレイヤーが「状態が合わない」と怒る）。これが別に作った理由だが、別の関数にする理由にはならない。
- **直し（意見に賛成）**：`DirectXCommon` に **コマンドリストも受け取る `TransitionBarrier`** を public で足す（同じ名前の引数違い）。今までの版は「描画のコマンドリストを渡してそれを呼ぶ」だけにする。UploadContext は namespace の関数を消して、これを呼ぶ。
  - バリアの詰め方（6行）は、エンジンが既に使っている `d3dx12.h`（DirectXTex に入っている。TextureManager が `UpdateSubresources` で使っている）の **`CD3DX12_RESOURCE_BARRIER::Transition`** に任せる。詰める所が 3か所（`TransitionBarrier`・`TransitionBarriers`・UploadContext）→ 0か所になる。
- 置き場所を `DirectXCommon` にしたのは、バリアを出す関数の仲間がもうそこにあるから（「GPU の便利な道具」は今は DirectXCommon に集まっている）。

### 19.3 エンジンの中にフォルダなどの画像を入れているが、使ってもいいのか。重くならないか → 重さは気にしなくてよい。権利は「どこから持ってきたか」次第

**重さ**

| | 大きさ |
|---|---|
| ファイル（7枚） | 合わせて約 19KB |
| 読むとき | 起動のときに1回だけ。**エディタの時だけ**（`AssetIcons` は `#ifdef USE_IMGUI` の中。製品版＝Release は読まない） |
| GPU のメモリ | 512×512 の3枚（File・Sound・Texture）が1枚 約1.3MB（ミップ込み）、128×128 の4枚が1枚 約85KB → **合わせて約4.3MB**。比べると `Goblets` の 4K の画像は1枚 約85MB |

- 気になるなら、512 の3枚を **128×128 に縮める**（パート K-7。任意）→ 合わせて約0.6MB。表示は最大 128 なので、見た目は変わらない（14章の決まりも「128 推奨」）。
- ビルドのたびにゲームの `MyEngine/Resources/` へ写されるので、製品版のフォルダにも入る（19KB なので困らない。気になるなら配るときに `Editor/` を消す）。

**使っていいか（権利）**

- **自分で描いたなら問題ない。**
- **素材サイト・アイコン集から持ってきたなら、その規約次第**。よくある決まりは「クレジット（作者名）を書く」「素材そのものを配る（再配布）のは禁止」。リポジトリを公開したり、エンジンを人に渡したりすると、素材そのものを配ることになる場合がある。
- GitHub のリポジトリは外から見えなかった（非公開のはず）ので、今は人に配っていない。公開する前に規約を見る。どこから持ってきたかを `Resources/Editor/Icons/CREDITS.txt` に1行書いておくと、後で困らない。

### 19.4 小さい絵を作るのに、png などはなぜ DirectXTex で読まない？ → 大きい jpg を速く読むため。png だけならどちらでも同じくらい

- DirectXTex の png・jpg の読み込み（`LoadFromWICFile`）も、中では **WIC（Windows の画像の読み込み）**を使っている。ただし**元の大きさのまま全部**読む。4K の画像なら 4096×4096×4 バイト＝64MB の絵を作ってから、128 に縮める。
- WIC の縮小（`IWICBitmapScaler`）を自分でつなぐと、**JPEG は読みながら 1/2・1/4・1/8 に縮められる**（JPEG は細かい所の情報を分けて持っているので、読まずに飛ばせる）。4K の jpg でも 512 の大きさから読み始められる。
- `resources/Goblets/textures/` に **4K の jpg が9枚**（1枚 約4〜11MB）ある。フォルダを開いたときに引っかからないように、こうした（16章の確認「4K の画像の小さい絵が1枚ずつ出てくる（止まらない）」）。
- **PNG は縮めながら読めない**（全部読むしかない）ので、png だけなら DirectXTex でも差は小さい。
- hdr は WIC が読めないので DirectXTex（`LoadHdr`）。
- まとめ：**今のままでよい**。もし「読み方を1つにしたい」なら、png は DirectXTex に寄せてもよいが、jpg の分は WIC のまま残すことになるので、かえって2通りになる。

### 19.5 Add Component の Camera が Rendering3D の中なのは変。Camera として独立させた方がよくない？ → 賛成。上の段に「Camera」を足して、Core の次に置く（パート K-1）

- カメラは 3D でも 2D でも使う（課題の 2D アクションでも使う）ので、Rendering3D の中はおかしい。
- Unity は Camera を「Rendering」の中に置いているが、このエンジンは Rendering を 3D・2D に分けているので、どちらにも入らない物は独立させるのがよい。
- ゲームの `CameraFollow`（`COMPONENT(CameraFollow, "Camera")`）は、今まで通り **Gameplay → Camera** の下（ゲームの Component はいつも Gameplay の下）。エンジンの Camera は一番上の段の **Camera**。

### 19.6 次にやること → [ActionGame.md](ActionGame.md)

- 課題の 2D 横スクロールアクションを、4章の「S5 小さなゲーム」の代わりにする。相談と設計は ActionGame.md に書いた。**ActionGame.md の7章に答えてもらったら**、エンジンに足す物（スプライト・押し戻し・移動ギズモ・空を消す・画面の絵とフェード）の写経用コードを出す。
- それまでにできること：この md の**パート K**（小さな直し）を写す → 21章を確かめる → コミットして develop へ。

---

## 20. 手順（パート K：質問から出た直しと、写し違いの直し）

| パート | 中身 | 順番 |
|---|---|---|
| **K-1** | Camera を Add Component の独立したカテゴリに（19.5） | いつでも |
| **K-2** | `TransitionFromCopyDest` を消して `DirectXCommon::TransitionBarrier` に1つにする（19.2） | いつでも |
| **K-3** | Inspector のアセットの一覧を UTF-8 で作る・日本語の名前は出さない（19.1） | いつでも |
| **K-4** | スライダーをやめて、つまんで動かす形に（CLAUDE.md の決まり） | いつでも |
| **K-5** | 写し違いの直し（18章） | いつでも |
| **K-6** | /MP の残り（Release・ゲーム） | **最初にやると、この後のビルドが速い** |
| **K-7** | アイコンを 128 に縮める（任意） | いつでも |

- 新しいファイルは無い。

### K-1. Camera を独立したカテゴリに（4ファイル）

**`Systems/CameraSystem.cpp`**（`Initialize` の最後の行）

```cpp
// ===== 前 =====
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraComponent>>("Camera", "Rendering3D", &DescribeCamera));

// ===== 後 =====
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraComponent>>("Camera", "Camera", &DescribeCamera)); // 3D・2D どちらでも使うので、独立したカテゴリ
```

**`Editor/Inspector/ComponentEditor.cpp`**（上の段の並び順。Core の次に Camera）

```cpp
// ===== 前 =====
constexpr std::string_view kTopLevelOrder[] = {"Core", "Rendering3D", "Rendering2D", "Lighting", "Effects", "Physics", "Audio", "Gameplay"};

// ===== 後 =====
constexpr std::string_view kTopLevelOrder[] = {"Core", "Camera", "Rendering3D", "Rendering2D", "Lighting", "Effects", "Physics", "Audio", "Gameplay"};
```

**`Editor/Inspector/ComponentEditor.h`**（先頭のコメントの表に1行）

```cpp
// ===== 前 =====
//   Core        Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
//   Rendering3D 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）

// ===== 後 =====
//   Core        Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
//   Camera      カメラ（3D・2D のどちらでも使うので、Rendering3D には入れない）
//   Rendering3D 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
```

**`Components/CameraComponent.h`**（3行目のコメント）

```cpp
// ===== 前 =====
// ゲームの絵を撮るカメラ。Entityに付けるComponent（カテゴリ：Rendering3D）

// ===== 後 =====
// ゲームの絵を撮るカメラ。Entityに付けるComponent（カテゴリ：Camera）
```

- 保存しているシーンファイルには、カテゴリは書かれていない（型の名前 `"Camera"` だけ）。なので、前に保存したシーンもそのまま読める。

### K-2. バリアの関数を1つにする（3ファイル）

**`Graphics/GPU/DirectXCommon.h`**（`TransitionBarrier` の宣言。下に1つ足す）

```cpp
// ===== 前 =====
	/// <summary>
	/// リソースのトランジションバリアを1つ発行する
	/// </summary>
	/// <param name="resource">状態を変えるリソース</param>
	/// <param name="before">変更前の状態</param>
	/// <param name="after">変更後の状態</param>
	static void TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

// ===== 後 =====
	/// <summary>
	/// リソースのトランジションバリアを1つ発行する（描画のコマンドリストへ積む）
	/// </summary>
	/// <param name="resource">状態を変えるリソース</param>
	/// <param name="before">変更前の状態</param>
	/// <param name="after">変更後の状態</param>
	static void TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

	/// <summary>
	/// リソースのトランジションバリアを1つ、渡したコマンドリストへ発行する
	/// <para>描画とは別のコマンドリストを持っている所（UploadContext など）から使う。バリアは、コピーと同じリストに積まないと順番が守られない</para>
	/// </summary>
	/// <param name="commandList">バリアを積むコマンドリスト</param>
	/// <param name="resource">状態を変えるリソース</param>
	/// <param name="before">変更前の状態</param>
	/// <param name="after">変更後の状態</param>
	static void TransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
```

**`Graphics/GPU/DirectXCommon.cpp`**（3か所）

```cpp
// ===== 前（1か所目：include）=====
#include <dxgidebug.h>

#include "MyEngine/String/ConvertString.h"

// ===== 後 =====
#include <dxgidebug.h>

#include <externals/DirectXTex/d3dx12.h>

#include "MyEngine/String/ConvertString.h"
```

```cpp
// ===== 前（2か所目：TransitionBarrier）=====
void DirectXCommon::TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	instance_->commandList_->ResourceBarrier(1, &barrier);
}

// ===== 後 =====
// ===== 描画のコマンドリストへ =====
void DirectXCommon::TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	MY_ASSERT_MSG(instance_, "DirectXCommon::Initialize()を先に呼んでください");
	TransitionBarrier(instance_->commandList_.Get(), resource, before, after);
}

// ===== 渡したコマンドリストへ =====
void DirectXCommon::TransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	// 中身の詰め方（全サブリソース・フラグなし）は d3dx12.h の CD3DX12_RESOURCE_BARRIER::Transition にまかせる
	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
	commandList->ResourceBarrier(1, &barrier);
}
```

```cpp
// ===== 前（3か所目：TransitionBarriers の for の中）=====
	for (const auto& [resource, before, after] : resources) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barriers.push_back(barrier);
	}

// ===== 後 =====
	for (const auto& [resource, before, after] : resources) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after));
	}
```

- `CD3DX12_RESOURCE_BARRIER` は `D3D12_RESOURCE_BARRIER` を継承しただけの「作る関数付き」の型（中身の大きさは同じ）。なので `std::vector<D3D12_RESOURCE_BARRIER>` にそのまま入れてよい。
- 同じ名前の関数が2つあっても、引数の数（3つ・4つ）で呼び分けられる。

**`Graphics/GPU/UploadContext.cpp`**（3か所）

```cpp
// ===== 前（1か所目：静的メンバ変数の下の namespace を丸ごと消す）=====
// 静的メンバ変数
UploadContext* UploadContext::instance_ = nullptr;

namespace {
// コピーし終わった dest を、コピー先の状態から使う状態へ変える（QueueUpload・QueueTextureUpload で共通）
void TransitionFromCopyDest(ID3D12GraphicsCommandList* commandList, ID3D12Resource* dest, D3D12_RESOURCE_STATES finalState) {
（略：中身）
}
} // namespace

// ===== 後 =====
// 静的メンバ変数
UploadContext* UploadContext::instance_ = nullptr;
```

```cpp
// ===== 前（2か所目：QueueUpload の中）=====
	// Resourceの状態を変更
	TransitionFromCopyDest(instance_->commandList_.Get(), dest, finalState);

// ===== 後 =====
	// Resourceの状態を変更（コピーと同じ、転送用のコマンドリストに積む）
	DirectXCommon::TransitionBarrier(instance_->commandList_.Get(), dest, D3D12_RESOURCE_STATE_COPY_DEST, finalState);
```

```cpp
// ===== 前（3か所目：QueueTextureUpload の中）=====
	// Resourceの状態を変更
	TransitionFromCopyDest(instance_->commandList_.Get(), dest, finalState);

// ===== 後 =====
	// Resourceの状態を変更（コピーと同じ、転送用のコマンドリストに積む）
	DirectXCommon::TransitionBarrier(instance_->commandList_.Get(), dest, D3D12_RESOURCE_STATE_COPY_DEST, finalState);
```

- `UploadContext.cpp` は、もう `DirectXCommon.h` を include している。

### K-3. アセットの一覧を UTF-8 で作る（5ファイル）

**`String/ConvertString.h`**（全体を差し替え）

```cpp
#pragma once
#include <filesystem>
#include <string>

// string->wstring
std::wstring ConvertString(const std::string& str);
// wstring->string
std::string ConvertString(const std::wstring& str);
// パス → UTF-8 の文字（区切りは /）。ImGui・ログ・シーンファイルに出すパスはこれで作る
// path::string() / generic_string() は、日本語の Windows では Shift-JIS になる（C++26 で非推奨になった理由もこれ）
std::string ToUtf8(const std::filesystem::path& path);
```

**`String/ConvertString.cpp`**（一番下に足す）

```cpp
std::string ToUtf8(const std::filesystem::path& path) {
	// Windows のパスの中身は UTF-16（wstring）なので、上の wstring → UTF-8 をそのまま使う
	return ConvertString(path.generic_wstring());
}
```

- 前の Project ウィンドウの `ToUtf8` は `generic_u8string()`（`char8_t` の文字列）を `std::string` に詰め替えていた。結果は同じ（検証で1バイトずつ比べた）。こちらは今ある `ConvertString` を使うので、新しい変換を書かなくて済む。

**`String/FileNameRule.h`**（一番下に足す）

```cpp
// アセットとして読めるパスか（UTF-8 のパスを渡す）
// 今は英数字・記号（ASCII）だけ。日本語などが入っていると Manager が読めずに止まることがある（Audio.md 8章）ので、Inspector の一覧に出さない
inline bool IsLoadableAssetPath(std::string_view path) {
	for (const char c : path) {
		if (static_cast<unsigned char>(c) >= 0x80) {
			return false; // UTF-8 で 0x80 以上 ＝ ASCII ではない文字の一部
		}
	}
	return true;
}
```

- UTF-8 では、英数字・記号（ASCII）は1バイトで 0x00〜0x7F、それ以外（日本語など）は全部のバイトが 0x80 以上になる。なので「0x80 以上のバイトが1つでもあるか」で分かる。

**`ECS/ComponentUI.cpp`**（2か所）

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Sound/SoundManager.h"

// ===== 後 =====
#include "MyEngine/Sound/SoundManager.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/String/FileNameRule.h"
```

```cpp
// ===== 前（2か所目：AssetFiles の for の中）=====
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsAssetFile(entry.path().generic_string(), type)) {
				files[slot].push_back(entry.path().generic_string());
			}
		}

// ===== 後 =====
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (!entry.is_regular_file(error)) {
				continue;
			}
			// UTF-8 で作る（generic_string() は Shift-JIS になり、ImGui・Manager は UTF-8 のつもりで読むので合わない）
			const std::string path = ToUtf8(entry.path());
			if (!IsAssetFile(path, type)) {
				continue;
			}
			// 日本語などが入った名前は、選ぶと Manager が読めずに止まるので、一覧に出さない（Audio.md 8章）
			if (!IsLoadableAssetPath(path)) {
				LogManager::Warning(std::format("名前に英数字以外の文字があるので、一覧に出しません（名前を英数字にしてください）: {}", path));
				continue;
			}
			files[slot].push_back(path);
		}
```

**`Editor/Windows/ProjectWindow.cpp`**（2か所：自分の `ToUtf8` を消して、共通の物を使う）

```cpp
// ===== 前（1か所目：include）=====
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/String/FileNameRule.h"

// ===== 後 =====
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/String/FileNameRule.h"
```

```cpp
// ===== 前（2か所目：namespace の中。この5行を消す）=====
// パス → 画面に出す文字（UTF-8。区切りは /）。日本語の名前でも文字化けしないように、UTF-8 にしてから ImGui に渡す
std::string ToUtf8(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

// ===== 後 =====
（何も無い。ToUtf8 は ConvertString.h の物を使う）
```

- 消さずに残すと、同じ名前の関数が2つ見えて「あいまい（C2668）」のエラーになる。
- Project ウィンドウは、日本語の名前のファイルも今まで通り出す（出すだけなら読めるので）。出さないのは Inspector の「選ぶ」一覧だけ。

### K-4. スライダーをやめる（4ファイル）

CLAUDE.md の「ImGui は Slider を使わず Drag にする」に合わせる。`Range(min, max)` で書いた項目（Camera の Field Of View・Audio Source の Volume / Pitch）と、Model Renderer の Material（Metallic・Roughness・Alpha Cutoff）がスライダーだった。

- 範囲がある項目は「**200 ピクセル動かすと端から端まで**」の速さにする（スライダーと同じくらいの感覚）。
- Ctrl+クリックで数字を打っても範囲に収める（`ImGuiSliderFlags_AlwaysClamp`）。今までの `AtLeast` の項目も、打った数字が下限より小さければ下限になる。

**`ECS/ComponentUI.h`**（`FieldStyle` と `Range`）

```cpp
// ===== 前 =====
struct FieldStyle {
	float min = 0.0f;              // 下限（min < max のときだけ効く）
	float max = 0.0f;              // 上限
	float dragSpeed = 0.0f;        // つまんで動かす速さ（0なら型ごとの既定）
	bool slider = false;           // true: スライダー、false: つまんで動かす
	const char* tooltip = nullptr; // マウスを乗せたときに出す説明
（略：Tip はそのまま）
	FieldStyle Drag(float speed) const {
		FieldStyle copy = *this;
		copy.dragSpeed = speed;
		copy.slider = false;
		return copy;
	}
};

// 範囲を決める（スライダーになる）
inline FieldStyle Range(float min, float max) {
	FieldStyle style;
	style.min = min;
	style.max = max;
	style.slider = true;
	return style;
}

// ===== 後 =====
struct FieldStyle {
	float min = 0.0f;              // 下限（min < max のときだけ効く）
	float max = 0.0f;              // 上限
	float dragSpeed = 0.0f;        // つまんで動かす速さ（0なら既定。範囲があれば「200ピクセルで端から端まで」）
	const char* tooltip = nullptr; // マウスを乗せたときに出す説明
（略：Tip はそのまま）
	FieldStyle Drag(float speed) const {
		FieldStyle copy = *this;
		copy.dragSpeed = speed;
		return copy;
	}
};

// 範囲を決める（つまんで動かす。数字を打っても範囲の外にはならない）
// スライダーは使わない（CLAUDE.md の決まり：細かい値を打てる方がいい）
inline FieldStyle Range(float min, float max) {
	FieldStyle style;
	style.min = min;
	style.max = max;
	return style;
}
```

**`ECS/ComponentUI.cpp`**（3か所）

```cpp
// ===== 前（1か所目：#ifdef USE_IMGUI の下の include）=====
#include <cstring>
#include <vector>

// ===== 後 =====
#include <cstring>
#include <limits>
#include <vector>
```

```cpp
// ===== 前（2か所目：namespace の頭。HasRange と DragSpeed）=====
namespace {
// 範囲が決まっているか
bool HasRange(const FieldStyle& style) { return style.min < style.max; }

// つまんで動かす速さ（指定が無ければ型ごとの既定）
float DragSpeed(const FieldStyle& style, float defaultSpeed) { return style.dragSpeed > 0.0f ? style.dragSpeed : defaultSpeed; }

// ===== 後 =====
namespace {
constexpr float kRangeDragPixels = 200.0f; // 範囲がある項目を、何ピクセル動かすと端から端まで行くか（スライダーの代わり）

// 範囲が決まっているか
bool HasRange(const FieldStyle& style) { return style.min < style.max; }

// つまんで動かす速さ
// 指定が無ければ、両端がある範囲（Range）は kRangeDragPixels で端から端まで、それ以外（AtLeast・範囲なし）は型ごとの既定
float DragSpeed(const FieldStyle& style, float defaultSpeed) {
	if (style.dragSpeed > 0.0f) {
		return style.dragSpeed;
	}
	if (HasRange(style) && style.max < FLT_MAX) {
		return (style.max - style.min) / kRangeDragPixels;
	}
	return defaultSpeed;
}
```

```cpp
// ===== 前（3か所目：InspectorComponentUI の Field 5つ。float・int・Vector2・Vector3・Vector4）=====
	void Field(const char* label, float& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat(label, &value, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}
（略：int・Vector2・Vector3・Vector4 も同じ形）

// ===== 後（bool の Field はそのまま）=====
	// 範囲があるときは AlwaysClamp（Ctrl+クリックで数字を打っても、範囲に収める）
	// 範囲が無いときに付けないのは、この ImGui では「min も max も 0」を「0 から動かせない範囲」として扱うため
	void Field(const char* label, float& value, FieldStyle style) override {
		if (HasRange(style)) {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f), style.min, style.max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		} else {
			ImGui::DragFloat(label, &value, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, int& value, FieldStyle style) override {
		const int min = static_cast<int>(style.min);
		// AtLeast の上限（FLT_MAX）は int に入らない（そのまま変換すると値が壊れる）ので、int の一番大きい値にする
		// (std::numeric_limits<int>::max) とかっこで囲むのは、windows.h の max マクロに邪魔されないため
		constexpr int kIntMax = (std::numeric_limits<int>::max)();
		const int max = (style.max >= static_cast<float>(kIntMax)) ? kIntMax : static_cast<int>(style.max);
		if (HasRange(style)) {
			ImGui::DragInt(label, &value, DragSpeed(style, 1.0f), min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
		} else {
			ImGui::DragInt(label, &value, DragSpeed(style, 1.0f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, bool& value, FieldStyle style) override {
		ImGui::Checkbox(label, &value);
		DrawTooltip(style);
	}

	void Field(const char* label, Vector2& value, FieldStyle style) override {
		if (HasRange(style)) {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		} else {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector3& value, FieldStyle style) override {
		if (HasRange(style)) {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		} else {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector4& value, FieldStyle style) override {
		if (HasRange(style)) {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		} else {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}
```

- **int の `max` の直し**：前は `static_cast<int>(style.max)` だった。`AtLeast` の上限は `FLT_MAX`（とても大きい float）で、int に入らない値を int にするのは C++ では「何が起きてもよい」（未定義動作）。今は int の項目に `AtLeast` を付けている所が無いので起きていないが、ゲームで `ui.Field("HP", value.hp, AtLeast(0.0f))`（hp が int）と書いたときのために直しておく。
- `FLT_MAX` は `ComponentUI.h` が読んでいる `<cfloat>` にある。

**`Editor/Inspector/ModelRendererEditor.cpp`**（Material の中の3行）

```cpp
// ===== 前 =====
		ImGui::SliderFloat("Metallic", &render.material.metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &render.material.roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha Cutoff", &render.material.alphaCutoff, 0.0f, 1.0f);

// ===== 後 =====
		// 0〜1 の値。スライダーではなくつまんで動かす（CLAUDE.md の決まり）。数字を打っても 0〜1 に収める
		ImGui::DragFloat("Metallic", &render.material.metallic, 0.005f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Roughness", &render.material.roughness, 0.005f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Alpha Cutoff", &render.material.alphaCutoff, 0.005f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
```

**`Systems/AudioSystem.cpp`**（namespace の頭のコメント2つ。動きは変わらない）

```cpp
// ===== 前 =====
constexpr float kMaxVolume = 3.0f; // 音量の上限（Inspector のスライダーと同じ。1より上は元より大きくなる）
constexpr float kMinPitch = 0.5f;  // ピッチの下限（Inspector のスライダーと同じ）

// ===== 後 =====
constexpr float kMaxVolume = 3.0f; // 音量の上限（Inspector で動かせる範囲と同じ。1より上は元より大きくなる）
constexpr float kMinPitch = 0.5f;  // ピッチの下限（Inspector で動かせる範囲と同じ）
```

- エンジンの中で `ImGui::Slider…` を使っているのは、`ComponentUI.cpp` と `ModelRendererEditor.cpp` だけだった（検索で確かめた）。

### K-5. 写し違いの直し（2ファイル）

**`Editor/Viewport/EditorViewport.cpp`**（3か所）

```cpp
// ===== 前（1か所目：Initialize）=====
	RegisterGV();
	GlobalVariables::GetInstance()->Group(kGVGroup, SettingsSection::Editor); // エディタだけの設定（Parameters/Editor/ に保存）
	ApplyGV();

// ===== 後 =====
	RegisterGV();
	ApplyGV();
```

```cpp
// ===== 前（2か所目：Render の中）=====
	SceneRenderer::RenderToTexture(gameCamera, gameRT, windowTitle);                     // Game：全部          // Game：ブルームあり

// ===== 後 =====
	SceneRenderer::RenderToTexture(gameCamera, gameRT, windowTitle);                     // Game：全部
```

```cpp
// ===== 前（3か所目：RegisterGV）=====
	GlobalVariables::GetInstance()
	    ->Group(kGVGroup)
	    .Category("DebugCamera")

// ===== 後 =====
	GlobalVariables::GetInstance()
	    ->Group(kGVGroup, SettingsSection::Editor) // エディタだけの設定（Parameters/Editor/ に保存）。置き場所はグループを最初に作るここで決まる
	    .Category("DebugCamera")
```

- もし Settings ウィンドウで Scene View を Save していたら、`Parameters/Scene View/` ができている。消してよい（直した後に Save すると `Parameters/Editor/Scene View/` にできる）。

**`Editor/Windows/InspectorWindow.cpp`**（`DrawAddComponentItem`）

```cpp
// ===== 前 =====
	if (ImGui::Selectable(editor.GetName())) { // Selectableを押すとポップアップは自動で閉じる

// ===== 後 =====
	if (ImGui::Selectable(editor.GetName())) {
```

### K-6. /MP の残り（Release・ゲーム）

15章のパート J の 2. で「構成」が **Debug のまま**だったと思う（vcxproj に Debug の分しか入っていない）。

1. **MyEngine_v1** を右クリック → プロパティ → 上の「構成」を **すべての構成**、「プラットフォーム」を **x64** → C/C++ → 全般 → 複数プロセッサによるコンパイル → **はい (/MP)**。
2. **CG3_Project** でも同じ。
3. `git diff` で、`MyEngine_v1.vcxproj` の Release の所と、`CG3_Project.vcxproj` の Debug・Release の所に `<MultiProcessorCompilation>true</MultiProcessorCompilation>` が入ったのを確かめる。

### K-7. アイコンを 128 に縮める（任意）

`Resources/Editor/Icons/` の **File.png・Sound.png・Texture.png の3枚が 512×512**（ほかの4枚は 128×128）。ペイントなどで 128×128 に縮めて上書きする。見た目は変わらず、GPU のメモリが約4.3MB → 約0.6MB になる（19.3）。

---

## 21. 確認すること

**ビルド**

- [ ] エンジン（Debug・Release）がエラー0、警告は前と同じ（`DIRECTINPUT_VERSION` の2つ、Debug のリンクの `/utf-8` 1つ）
- [ ] ゲーム（Debug・Release）がエラー0・警告0
- [ ] K-6 の後、Release・ゲームのビルドでも cpp の名前がいくつも並んで出る

**K-1**

- [ ] Add Component の一番上の段が **Camera → Rendering3D → …** の順。Camera の中に Camera
- [ ] Gameplay → Camera の中に CameraFollow（ゲームの物）はそのまま
- [ ] Main Camera の入ったシーンを開いても、Camera Component がそのまま（保存し直さなくてよい）

**K-2**

- [ ] 起動してモデル（monsterBall など）が今まで通り出る
- [ ] Project ウィンドウで `monsterBall` や `Goblets/textures` を開くと、小さい絵が出る
- [ ] Output（出力）ウィンドウに D3D12 のエラー（`D3D12 ERROR`）が出ていない

**K-3**

- [ ] エクスプローラーで `resources/` に日本語の名前の画像（例：`uvChecker.png` を写して `テスト.png`）を置く → Inspector の Model Renderer の Texture のコンボで **Refresh** → 一覧に**出ない**・Console に「名前に英数字以外の文字があるので…」の警告
- [ ] Project ウィンドウには `テスト` が出る（文字化けしない）・小さい絵も出る
- [ ] 確かめたら `テスト.png` を消す

**K-4**

- [ ] Main Camera の Field Of View・Audio Source の Volume / Pitch が、つまんで動かす形（押しても値が飛ばない）。200 ピクセルくらい動かすと端から端まで
- [ ] Ctrl+クリックで範囲の外の数字（Volume に 100 など）を打つと、上限になる
- [ ] Model Renderer → Material の Metallic・Roughness・Alpha Cutoff も同じ

**K-5**

- [ ] Settings ウィンドウの左の一覧で、**Scene View が Editor の見出しの下**
- [ ] Scene View の Save → 保存先の表示とファイルが `Parameters/Editor/Scene View/DebugCamera.json`
- [ ] 16章の残り（マウス・キーボードでの操作など）も済んだら

**確認が終わったら**

- [ ] コミット（`feature/project-window`）→ develop へ（13章の②のやり方）
- [ ] [ActionGame.md](ActionGame.md) の7章に答える

---

## 22. 検証したこと（2026-10-01、Claude）

- **やり方**：本体（bd7e4c2）とゲーム（`CG3_Project` の今のファイル）を検証用のフォルダに写し、パート K-1〜K-5 をそのまま入れて、MSBuild でビルド・起動した（本体とゲームのファイルは触っていない）。
- **ビルド**：直す前（写した物そのまま）と、パート K を入れた後の両方で、エンジンとゲームの Debug・Release ともエラー0。警告は同じ（エンジン：`DIRECTINPUT_VERSION` の2つ・Debug のリンクの `/utf-8` の1つ、ゲーム：0）。
- **動かした**：Debug・Release とも起動 → 20秒・15秒動かして → 閉じる（終わりのコード 0）。Debug は GameScene（monsterBall・Player・Enemy のモデル＝`UploadContext` の転送とバリアを通る）を開いた画面を撮って、モデルが出ていることと、ログに警告・エラーが1つも無いことを確かめた。
- **`ToUtf8` と `IsLoadableAssetPath`**：小さなプログラム（`/W4`・`/utf-8`）で、`resources/テスト.png` のバイト（19.1 の表）・前の Project ウィンドウの作り方と1バイトも違わないこと・英数字のパスは通って日本語は弾くこと・UTF-8 の文字を `path` に渡すと例外になること、を確かめた。
- **確かめていないこと**：Inspector のコンボ・Add Component・Settings ウィンドウをマウスで触る所（K-1 の並び・K-3 の一覧・K-4 の動かし方・K-5 の見出し）。21章でお願いします。
