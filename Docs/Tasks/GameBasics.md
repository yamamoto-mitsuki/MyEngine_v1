# GameBasics：ゲームに要る物（音の続き・カメラ・入力）と、見つけたバグ2つ

## このファイルについて

- 2026-09-28 の依頼：音の残り（**種類ごとの音量＝やる**、3Dの音・試し聞き＝簡単なら）、「次は Input・カメラ？」、「PCH は本当に要るのか」、触っていて見つけたバグ2つ（**PostEffect ウィンドウで Undo / Redo が効かない**、**独立した Entity を親子にすると位置が変わる**）。
- 先に結論：
  - バグ2つは直した（パートA）。
  - 音の3つはどれも簡単だったので、全部入れた（パートB）。
  - カメラは Component にして、Hierarchy に置く物にした（パートC）。入力は見つけた穴を埋めた（パートD）。ゲーム側の直しと例はパートE。
  - **PCH は今はやらない**（0章。ゲーム本編への影響は無い）。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階。
- ブランチは `feature/game-basics`（`feature/Audio` の先。A-0）。
- **2026-09-29 追記**：A〜E を写したら「起動すると `array subscript out of range` で止まる」（83fb0bb）。原因は `SoundBus` の最後に足した `Count`。直し方と、照合で見つけた写し違い2つは **[11章（パートF）](#11-起動できないバグと写し違いパートf2026-09-29)**。質問6つへの答えは **[12章](#12-質問への答え2回目2026-09-29)**。次にやること（シーンの切り替え・アニメーション）は [SceneFlow.md](SceneFlow.md)・[Animation.md](Animation.md)。

| 読む順番 | 何が書いてあるか |
|---|---|
| **[11. 起動できないバグと写し違い](#11-起動できないバグと写し違いパートf2026-09-29)** | **まずここ。`Count` が原因の assert と、写し違い2つの直し（パートF）** |
| [12. 質問への答え（2回目）](#12-質問への答え2回目2026-09-29) | GlobalVariables・CalcWorldMatrix・Audio と Sound・SoundManager・マウス座標・Scene ビューのポストエフェクト |
| [0. 質問への答え](#0-質問への答え) | PCH は要るか・次は Input とカメラか・音の残り |
| [1. バグ：親子にすると位置が変わる](#1-バグ親子にすると位置が変わるパートa) | 原因と直し方（行列を大きさ・回転・位置に戻す） |
| [2. バグ：PostEffect の Undo / Redo](#2-バグposteffect-ウィンドウの-undo--redoパートa) | 原因と直し方 |
| [3. 音の続き](#3-音の続きパートb) | 種類ごとの音量・3Dの音・試し聞き |
| [4. カメラ](#4-カメラパートc) | Camera Component |
| [5. 入力](#5-入力パートd) | 埋めた穴と、足した関数 |
| [6. 手順](#6-手順パート-ae) | 写経するコード（パートA〜E） |
| [7. 確認すること](#7-確認すること) | 写したあとに実行して見ること |
| [9. 決めたこと](#9-決めたこと) | 理由の一覧 |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| A | バグ2つ：親子にすると位置が変わる／Parameters（PostEffect）ウィンドウの Undo・Redo | 写経済み（83fb0bb）。**A-8 に写し違い1つ → F-3** |
| B | 音：種類ごとの音量（BGM / SE / Master）・3Dの音・試し聞き | 写経済み（83fb0bb）。**`Count` を足したので起動できない → F-1・F-2** |
| C | カメラ：Camera Component と CameraSystem | 写経済み（83fb0bb） |
| D | 入力：リンクエラー・パッド・Game ビューのマウス座標・移動の入力 | 写経済み（83fb0bb）。**D-3 が抜けている → F-4** |
| E | ゲーム側：GameScene のカメラを Camera Component に・例の Component 3つ | E-1・E-2 は写経済み。E-3（エディタで Main Camera を置いて Save）・E-4（例の3つ）はまだ（11章） |
| **F** | **起動できないバグ（`SoundBus::Count`）と写し違い2つの直し** | **写経待ち（2026-09-29）** |

**A を C より先に**（C は A で作る `DecomposeAffineMatrix` を使う）。B と D はいつでもよい（A〜D のどれを先にしても、途中の状態でビルドが通ることを確かめてある）。E は C と D の後。

---

## 0. 質問への答え

### PCH は本当に要るか → 今は要らない

- **ゲーム本編への影響は無い（0）。** PCH（プリコンパイル済みヘッダ）は「ヘッダを読む手間」を先に済ませておく仕組みで、出来上がる exe の動き・見た目・速さは変わらない。変わるのはビルドの待ち時間だけ。
- どれだけ待ちが減るか（Entity.md の実測）：ゲームの Component 1ファイルのビルドが約2.5秒で、そのうち約2.4秒がヘッダを読む時間（`MyAssert.h` → `LogManager.h` → spdlog / fmt）。PCH を入れると1ファイル約0.2秒の見込み。VS は直したファイルだけビルドし直すので、**1回のビルドで減るのは「直したファイルの数 × 約2秒」**。
- 簡単か：設定だけなら10分くらい。ただし落とし穴がある。
  1. 全部の `.cpp` の頭に `pch.h` が入る（「強制インクルード」を使う場合）。`pch.h` で `windows.h` を読むと、今まで読んでいなかったファイルにも `max` マクロが入る（このエンジンで2回ぶつかった）。`pch.h` の頭に `#define NOMINMAX` が要る。
  2. エンジン（静的ライブラリ）とゲームで別々に作る。Debug / Release も別。
  3. `pch.h` に入れたヘッダを直すと、全部のファイルがビルドし直しになる。エンジンのヘッダは今も毎回のように直しているので、エンジン側に入れると、かえって待つことがある。
- **おすすめ：今はやらない。** ゲームの Component が増えて（20個くらい）、「1行直して F5 → 待つ」が気になりだしたら、**ゲーム側だけ**に入れる。それまでは S5（小さなゲームを1本通す）を先に。

### 次は Input とカメラか → 両方このファイルで入れた（大きさは違う）

- **カメラ（パートC）は大きかった。** 今のカメラは GameScene が `new` するだけの物で、保存されない・エディタで置けない・動かすにはコードを直してビルド・何かを追いかけられない。→ **Camera Component** にして、ライトと同じく Hierarchy に置く物にした。
- **入力（パートD）は、今でも使える**（キー・マウス・パッド）。触って見つけた穴を埋めただけ。
  - `GetMousePositionX/Y` が、宣言だけで中身が無かった（呼ぶとリンクエラー）。
  - エディタでは、マウスの位置が Game ビューの中の位置にならない（ウィンドウ全体での位置）→ ゲーム画面の座標で返す関数を足した。
  - パッドは、別のアプリを触っている間・ImGui に文字を打っている間も効いていた（キーボードは止まる）。
  - 移動（WASD・矢印キー・左スティック）をまとめて読む関数を足した。
  - 「Input は Component にしない」は前の判断のまま（Serialize.md）。キーの割り当て（"Jump" = Space か A ボタン、のような名前での読み方）は、後で要るときに（10章）。

### 音の残り → 3つとも入れた（パートB）

| 項目 | 結果 |
|---|---|
| 音の種類ごとの音量（やる） | 入れた。**BGM・SE・全体（Master）**。Parameters ウィンドウの **Audio** タブで変えて Save。ゲームの設定画面からは `AudioSystem::SetBusVolume` |
| 3Dの音（簡単なら） | 簡単だった。Audio Source の **Spatial** に印を付けると、ゲームのカメラから離れると小さくなり、左右に振れる |
| 試し聞き（簡単なら） | 簡単だった。Audio Source の Sound の下に **▶ Preview**。停止中でも鳴る |

AudioKeyTest の Inspector に何も出ないのは、中身の無い「印だけの Component」（`struct AudioKeyTest {};`）だから（正しい）。確かめ終わったので消してよい。

---

## 1. バグ：親子にすると位置が変わる（パートA）

### 何が起きていたか

- Hierarchy でドラッグ＆ドロップして親を付けると、Transform の値（Position・Rotation・Scale）はそのままで、親だけが変わっていた。
- Transform の値は「**親から見た値**」。同じ値のまま親を変えると、新しい親の位置・回転・大きさの分だけ動いて見える。例：親が (5, 0, 0) にいて、子が (10, 0, 0) → 子は (15, 0, 0) に飛ぶ。親の Scale が 2 なら、子も2倍に見える。

### どう直したか

付け替える前の見た目（**ワールド行列**）を覚えておき、「新しい親から見て、どの値なら同じ見た目になるか」を計算して入れ直す。

```
自分のワールド = 自分のローカル × 親のワールド        （UpdateTransforms がしている計算）
        ↓ 両辺に、右から「親のワールドの逆行列」を掛ける
自分のローカル = 自分のワールド × 親のワールドの逆
```

ローカルは行列の形で出てくるので、大きさ・回転・位置の3つに戻す関数 **`DecomposeAffineMatrix`**（`MakeAffineMatrix` の逆）を作った。

- 行列の4行目が位置。1〜3行目の長さが大きさ。大きさで割った残りが回転の行列。
- 回転の行列（X×Y×Z の順に掛けた物）を書き出すと、決まった場所に sin・cos がそのまま出てくる（例：`r[0][2] = -sin(y)`）。そこから `asin`・`atan2` で角度に戻す。詳しくは Matrix4x4.cpp のコメント。
- Y軸でちょうど真横（±90度）まで回っていると、X と Z が同じ回り方になって区別できない（ジンバルロック）。そのときは Z = 0 として X に全部入れる（見た目は同じ）。

使う所：
- **Hierarchy のドラッグ＆ドロップ**（何も無い所へ落として親を外すときも）。
- ゲームからは **`EntityManager::SetParentKeepWorld`**（物を掴む・乗り物に乗る、など）。前の `SetParent` は値をそのまま使う物として残す（シーンの読み込みは、ファイルの値をそのまま使いたいので）。
- Undo・Redo は、付け替えの前と後の Transform の値を両方覚えて、そのまま戻す（計算し直さないので、何回行き来しても値がずれない）。

### 直しきれないこと（Unity も同じ）

親の大きさが軸ごとに違い（例：Y だけ3倍）、しかも子が親に対して回っていると、子は「ゆがんだ形」になる。Transform の3つ（大きさ・回転・位置）ではゆがみを表せないので、**位置は合うが、向きと大きさは近い値になる**。親の大きさがどの軸も同じなら、ぴったり合う（テストでは誤差 0.000001 以下）。

---

## 2. バグ：PostEffect ウィンドウの Undo / Redo（パートA）

### 何が起きていたか

- Undo の履歴（`EditorHistory`）が覚えていたのは、Entity の操作（作る・消す・名前・有効・親子）と、Inspector の値の変更だけだった。
- PostEffect ウィンドウは、Parameters ウィンドウ（`GlobalVariables`）のタブの1つ。つまみを動かすと値を直接書き換えるだけで、**履歴に何も積んでいなかった**。
- だから Ctrl+Z を押すと、PostEffect は戻らず、**その前にした Entity の操作が戻っていた**（気づきにくい）。

### どう直したか

Inspector と同じ仕組みを、`GlobalVariables` にも入れた。

1. 項目を描く前の値を写しておき、描いた後と比べる。変わっていたら「前」と「後」を覚える（同じ項目なら「前」は触り始めたときのまま、「後」だけ新しくする）。
2. どの項目も触っていない（マウスを離した）フレームで、覚えた分を1件の履歴にまとめて積む。ドラッグの途中は積まないので、1回つまんで動かしたら1件。動かして元の値に戻しただけなら積まない。
3. `EditorHistory` に、Entity の外の値を積む入口 **`Record(undo, redo)`** を足した。戻し方（グループ名・カテゴリ・項目名で探して書き戻す）は `GlobalVariables` が渡す。

- PostEffect だけでなく、**Parameters の全部のタブ**（Debug のカメラ・GameScene の調整項目・新しい Audio の音量）で効く。
- 項目をポインタではなく名前で覚える理由：項目は `std::vector` の中にあり、項目が足されると置き場所（メモリ）が変わることがある。

注意：
- Play / Stop・シーンの切り替えで、履歴は消える（Entity の履歴と同じ）。
- **Parameters の値はシーンに入っていない**ので、Play 中に変えた値は Stop しても戻らない（前からそう）。Play 中なら Ctrl+Z で戻せる。
- File → Reload All Variables で読み直した値は、履歴に入らない。

---

## 3. 音の続き（パートB）

### できること

| 操作 | どうなるか |
|---|---|
| Audio Source の **Bus** | **BGM** か **SE**（効果音）。種類ごとの音量が掛かる。新しく付けた Audio Source は SE |
| Parameters ウィンドウ → **Audio** タブ → **Volume** | **Master**（全部）・**BGM**・**SE** の音量（0〜2。1が元の大きさ）。鳴っている音にもすぐ効く。Save で `Parameters/Audio/Volume.json` に残り、次の起動からその値。Ctrl+Z も効く |
| ゲームのコード | `AudioSystem::SetBusVolume(SoundBus::BGM, 0.5f)`・`GetBusVolume`・`SetMasterVolume`・`GetMasterVolume`（設定画面など）。Parameters の値を書き換えるので、ウィンドウの表示も一緒に変わる |
| Audio Source の **Spatial** | 3Dの音。**ゲームのカメラ**から離れると小さくなり、左右に振れる（位置は Transform。親の下でもよい） |
| **Min Distance** / **Max Distance** | Min まで近いと元の大きさ、Max より遠いと聞こえない。その間はまっすぐ小さくなる |
| **▶ Preview** | 試し聞き。**停止中でも鳴る**。Volume・Pitch・Bus は Inspector の値（ループ・3D は使わず、1回だけ）。鳴っている間は **■ Stop Preview** になる。Play / Stop でも止まる |
| **Volume** | 前は 1 より上にしても 1 で止まっていた（スライダーは 2 まで動くのに）→ **2 まで効く**ように直した |
| `SoundManager::Play` | 最後の引数で種類を選ぶ（書かなければ SE）。**直接鳴らす BGM には `SoundBus::BGM` を渡す**（渡さないと、BGM の音量つまみが効かない） |
| `SoundManager::SetPan` | **真ん中で左右とも元の大きさ**になった（前は真ん中で左右とも半分＝小さく聞こえた）。右へ振るほど左が小さくなる |
| 保存 | `"bus": "BGM", "spatial": true, "minDistance": 1.0, "maxDistance": 20.0` が増える。**前に保存したシーンもそのまま読める**（無い項目は初期値＝SE・3Dでない） |

### ゲームでの書き方

```cpp
	// BGM を直接鳴らす（シーンをまたいで鳴らし続けたいとき）。最後に SoundBus::BGM を渡す
	const uint32_t bgm = SoundManager::Load("resources/sounds/bgm.wav");
	SoundManager::Play(bgm, true, 1.0f, 1.0f, SoundBus::BGM);
```

```cpp
	// 設定画面（0〜1）。Parameters ウィンドウの Audio / Volume も一緒に変わる
	AudioSystem::SetBusVolume(SoundBus::BGM, 0.5f);
	AudioSystem::SetBusVolume(SoundBus::SE, 0.8f);
	AudioSystem::SetMasterVolume(1.0f);
	const float bgmVolume = AudioSystem::GetBusVolume(SoundBus::BGM);
```

- **BGM の Audio Source は Bus を BGM にする**（Loop・Play On Start と一緒に）。
- 効果音を3Dにするなら、音を出す Entity（敵など）の Audio Source に Spatial。`PlayOneShot` の音は、鳴らした瞬間の位置で大きさ・左右が決まる（鳴っている間は動かない）。
- 聞く人はゲームのカメラ。カメラが遠くにあると、3D の音は全部小さい（Max Distance を大きくする）。

### 仕組み（読まなくても写せるが、知っておくと直せる）

#### 音の通り道

```
鳴らす音（SourceVoice）──→ BGM の通り道（SubmixVoice・左右2ch）──┐
                      └─→ SE の通り道（SubmixVoice・左右2ch） ──┴→ マスター（MasteringVoice）→ スピーカー
                              ↑ ここの音量 = 種類ごとの音量             ↑ ここの音量 = Master
```

- XAudio2 では、音は「声（Voice）」から「声」へ流れる。前は鳴らす音からマスターへ直接流していた。間に種類ごとの通り道（SubmixVoice）を2つ挟み、その音量を種類ごとの音量にした。
- どの通り道へ流すかは、音を作るときに決める（`CreateSourceVoice` の送り先）。鳴っている途中で Bus を変えたときは、次に鳴らしたときから効く。
- 通り道は左右の2ch にした。左右の振り分け（パン・3D）は、スピーカーが何chでも「左・右の2つ」に書けばよい（このPCのスピーカー出力は 8ch＝7.1ch だった）。

#### 3Dの音の計算（`AudioSystem.cpp` の `CalcSpatial`）

- 聞く人：ゲームのカメラ（パーティクルと同じ `GetCamera()`）の位置と、右の向き（カメラの行列の1行目）。
- 大きさ：`1 - (距離 - Min) / (Max - Min)` を 0〜1 に収めた物。
- 左右：音のある向きとカメラの右の向きの内積（真右で 1、真左で -1、正面・真後ろ・真上で 0）。
- 大きさ・左右は、音量（`SetVolume`）とは別の所（出力の割り振り＝`SetOutputMatrix`）に掛ける。だから Inspector の Volume・フェードとぶつからない。
- 毎フレーム計算するが、`SoundManager` に伝えるのは変わったときだけ。
- 前後の区別（後ろの音がこもる）・ドップラー（近づくと高く聞こえる）は無い（10章）。

---

## 4. カメラ（パートC）

### できること

| 操作 | どうなるか |
|---|---|
| Hierarchy の **＋ → Camera** | 「Main Camera」ができる（(0, 0, -10) から原点を正面に見る） |
| Add Component → Rendering3D → **Camera** | どの Entity にも付けられる |
| Transform | カメラの位置・向き（大きさは使わない）。**親の下に置くと一緒に動く**（乗り物の中など） |
| **Field Of View** | 縦の視野角（**度**）。小さいほど望遠。前の GameScene のカメラ（0.45 ラジアン）は約 25.8 度 |
| **Near** / **Far** | 映る距離の範囲 |
| **Priority** | 有効な Camera が複数あるとき、大きい方を使う。切り替えは、この値を変えるか、Entity の有効・無効で |
| Game ビュー | **停止中も、そのカメラから撮る**（置いて動かすと、その場で Game ビューの絵が変わる） |
| Scene ビュー | 使っているカメラの映る範囲が線で出る（前と同じ） |
| Save / Play / Stop | カメラも保存される。Play 中に動いたカメラは、Stop で Play の前の位置に戻る |
| Camera が1つも無い | 既定のカメラ（(0, 0, -10) から +Z を向く）で撮る。Log に1回出る |

### ゲームでの書き方

- シーンは何もしなくてよい。**`GetCamera()` を上書きしない**（上書きすると、そちらのカメラが使われて、Camera Component は無視される）。
- カメラを使う物（`ModelRenderSystem::Draw`・Stage）には `GetCamera()` を渡す。いつも同じカメラを返すので、ポインタを覚えておいてよい。
- **カメラを動かす＝その Entity の Transform を動かす。** 追いかけるカメラは、パートEの `CameraFollow`（Main Camera に付けて、Target に自機を入れる）。
- 今使っているカメラの Entity は `CameraSystem::GetMainCameraEntity()`。

```cpp
void MoveCameraUp(float deltaTime) {
	const Handle<Entity> camera = CameraSystem::GetMainCameraEntity(); // Camera Component が無ければ無効な Handle
	if (TransformComponent* transform = EntityManager::Get<TransformComponent>(camera)) {
		transform->translation.y += 2.0f * deltaTime;
	}
}
```

### 仕組み

- `CameraSystem` がカメラ（`Camera` クラス）を1つだけ持つ。毎フレーム、有効な Camera Component の中で Priority が一番大きい物を選び、その Transform の**ワールド行列**から位置・向き（`DecomposeAffineMatrix`）を、Camera Component から写り方を写す。
- `IScene::GetCamera()` を上書きしなかったときの中身が `CameraSystem::GetMainCamera()`。エンジンの描画・パーティクル・ギズモ・製品版の描画は、今までどおり `GetCamera()` を使っているので、何も変えずに Camera Component のカメラになる。
- 縦横比は 16:9 に固定（Game ビューの描画先とスプライトの基準が 1280×720 なので）。

1フレームの中の順番（Reference.md の1章の表に足す）：

| 順 | 何が起きるか |
|---|---|
| 5 | ゲームの `SYSTEM`（ここで Camera の Entity の Transform を動かす） |
| 6 | `EntityManager::UpdateTransforms`（ワールド行列） |
| **6.5** | **`CameraSystem::Update`**（ワールド行列から、カメラへ写す。親の下のカメラも、そのフレームの位置で撮る） |
| 7〜10 | パーティクル・ライト・破棄・当たり判定・音（3Dの音の聞く人） |
| 11 | 描画（`GetCamera()` のカメラで撮る） |

---

## 5. 入力（パートD）

| 関数 | すること |
|---|---|
| `InputManager::GetMousePositionInGame()` | マウスの位置を、**ゲーム画面の座標**で（スプライトの `position` と同じ：真ん中が0、右と上が+、端が±1）。エディタでは Game ビューの絵の中、製品版ではウィンドウの中が画面 |
| `InputManager::IsMouseInGame()` | マウスがゲーム画面の上にあるか |
| `InputManager::GetMoveAxis()` | 移動の入力（**WASD・矢印キー・左スティック**をまとめた物）。x は右が+、y は上（奥）が+。-1〜1、斜めでも長さは1まで |
| `GetMousePositionX/Y` | 呼ぶとリンクエラー（`LNK2019`）だった → 中身を書いた（ウィンドウの中の位置。エディタでは Game ビューの中の位置ではないので、ゲームには上の関数を使う） |
| パッドの関数 | **ウィンドウが手前に無いとき・ImGui に文字を打っているときは「押していない」扱い**（キーボードと同じにした）。スティック・トリガーは 0 |
| `IsKeyReleased` / `IsMouseButtonReleased` | 「押した」「押している」と同じく、ImGui が使っているときは false にそろえた |

```cpp
// center・half はスプライトと同じ座標（真ん中が0、端が±1）。half はボタンの幅・高さの半分
bool IsButtonClicked(const Vector2& center, const Vector2& half) {
	if (!InputManager::IsMouseButtonTriggered(0)) {
		return false;
	}
	const Vector2 mouse = InputManager::GetMousePositionInGame();
	return std::abs(mouse.x - center.x) <= half.x && std::abs(mouse.y - center.y) <= half.y;
}
```

- 移動は `GetMoveAxis()` の y を、地面の奥（+Z）への移動にするのが普通（パートEの `PlayerMove`）。

---

## 6. 手順（パート A〜E）

| パート | 触るファイル |
|---|---|
| A | `Math/Matrix4x4.h`・`.cpp`、`ECS/EntityManager.h`・`.cpp`、`Editor/History/EditorHistory.h`・`.cpp`、`UI/GlobalVariables.h`・`.cpp` |
| B | 新規 `Sound/SoundBus.h`、`Sound/SoundManager.h`・`.cpp`、`Components/AudioSourceComponent.h`（全体）、`ECS/ComponentUI.h`・`.cpp`、`ECS/ComponentSerializer.cpp`、`Systems/AudioSystem.h`・`.cpp`（全体）、`Window/WindowManager.cpp`（1行） |
| C | 新規 `Components/CameraComponent.h`・`Systems/CameraSystem.h`・`.cpp`、`Scene/IScene.h`、`Engine.cpp`、`Window/WindowManager.cpp`、`Editor/Windows/HierarchyWindow.cpp`、`ECS/GameComponent.h` |
| D | `Input/InputManager.h`・`.cpp`、`Editor/Viewport/EditorViewport.cpp` |
| E | ゲーム：`GameScene.h`・`.cpp`、新規 `Source/Components/PlayerMove.cpp`・`CameraFollow.cpp`・`MouseClickTest.cpp` |

パスは全部 `MyEngine/` の下（E はゲーム `CG3_Project/` の下）。**「（Nか所）」の手順は、見出しのコードに書いてある前後の行を探して、同じ形にする**（コードの中の、元からある行は目印）。

### A-0. ブランチ

`feature/Audio` はコミット済みなので、その先に切る（develop にはまだ Audio が入っていないため）。

```bash
git switch feature/Audio
git switch -c feature/game-basics
```

（この md はもう `feature/game-basics` に置いてある。）

---

### A. バグ2つ

#### A-1. `MyEngine/Math/Matrix4x4.h`（1か所）

`MakeAffineMatrix` の下に3行足す。

```cpp
// 3次元アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
// MakeAffineMatrix の逆。行列から 大きさ・回転（ラジアン。X→Y→Zの順）・位置 を取り出す
// 親の大きさが軸ごとに違い、しかも子が回っていると「ゆがみ」が混ざり、3つでは表しきれない。そのときは近い値になる
Transform DecomposeAffineMatrix(const Matrix4x4& matrix);
```

#### A-2. `MyEngine/Math/Matrix4x4.cpp`（1か所）

`MakeAffineMatrix` の関数の下（`// 透視投影行列` の上）に足す。

```cpp
// 行列 → 大きさ・回転・位置（MakeAffineMatrix の逆）
// 行列 = 大きさ × 回転(X×Y×Z) × 位置 なので、
//   4行目   … 位置
//   1〜3行目 … 「i番目の大きさ × 回転の i行目」。行の長さが大きさで、大きさで割ると回転の行が残る
// 回転の行列 X×Y×Z を掛け算して並べると、次の場所に sin・cos がそのまま出てくる
//   r[0][2] = -sin(y)
//   r[1][2] =  sin(x)cos(y)   r[2][2] = cos(x)cos(y)   → x = atan2(r[1][2], r[2][2])
//   r[0][1] =  cos(y)sin(z)   r[0][0] = cos(y)cos(z)   → z = atan2(r[0][1], r[0][0])
// cos(y) が 0（Y軸で真横まで回った）のときは x と z が同じ回り方になって区別できないので、z = 0 として x に入れる
Transform DecomposeAffineMatrix(const Matrix4x4& matrix) {
	constexpr float kEpsilon = 1e-6f;
	Transform result;

	// --- 位置：4行目 ---
	result.translation = {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};

	// --- 大きさ：1〜3行目の長さ ---
	Vector3 rows[3];
	for (int i = 0; i < 3; ++i) {
		rows[i] = {matrix.m[i][0], matrix.m[i][1], matrix.m[i][2]};
	}
	result.scale = {Length(rows[0]), Length(rows[1]), Length(rows[2])};
	// 鏡に映したように裏返っている（行列式が負）ときは、X の大きさを負にして表す
	if (Dot(Cross(rows[0], rows[1]), rows[2]) < 0.0f) {
		result.scale.x = -result.scale.x;
	}

	// --- 回転：各行を大きさで割る ---
	const float scales[3] = {result.scale.x, result.scale.y, result.scale.z};
	float r[3][3] = {
	    {1.0f, 0.0f, 0.0f},
	    {0.0f, 1.0f, 0.0f},
	    {0.0f, 0.0f, 1.0f},
	};
	for (int i = 0; i < 3; ++i) {
		// 大きさが0の軸は向きが分からないので、回っていない行のままにする
		if (std::abs(scales[i]) > kEpsilon) {
			r[i][0] = rows[i].x / scales[i];
			r[i][1] = rows[i].y / scales[i];
			r[i][2] = rows[i].z / scales[i];
		}
	}
	result.rotation.y = std::asin(std::clamp(-r[0][2], -1.0f, 1.0f));
	if (std::abs(r[0][2]) < 1.0f - kEpsilon) {
		result.rotation.x = std::atan2(r[1][2], r[2][2]);
		result.rotation.z = std::atan2(r[0][1], r[0][0]);
	} else {
		// cos(y) = 0 のとき、r[1][1] = cos(x∓z)、r[2][1] = -sin(x∓z) になる
		result.rotation.x = std::atan2(-r[2][1], r[1][1]);
		result.rotation.z = 0.0f;
	}
	return result;
}
```

#### A-3. `MyEngine/ECS/EntityManager.h`（1か所）

「親子」の `SetParent` の所を、次の形にする（コメント1行と、関数2つを足す）。

```cpp
	// ===== 親子 =====
	// 親を付け替える。自分の子孫を親にしようとしたときは何もしない（輪になるのを防ぐ）
	// translation / rotation / scale はそのまま＝「新しい親から見て同じ位置」になるので、見た目の位置は変わる
	static void SetParent(Handle<Entity> child, Handle<Entity> parent);
	// 親を付け替える。見た目の位置・向き・大きさは変えない（新しい親から見た値に直す。Hierarchy のドラッグ＆ドロップはこちら）
	static void SetParentKeepWorld(Handle<Entity> child, Handle<Entity> parent);
	// 親まで掛けたワールド行列を、今の translation / rotation / scale から計算する（UpdateTransforms を待たずに正しい値が欲しいとき）
	static Matrix4x4 CalcWorldMatrix(Handle<Entity> handle);
	// 親をたどって、handleがancestorの子孫かを調べる（handle自身は含まない）
	static bool IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor);
```

#### A-4. `MyEngine/ECS/EntityManager.cpp`（2か所）

(1) インクルードに1行足す。

```cpp
#include <algorithm>
#include <cmath>
```

(2) `SetParent` の関数の下（`IsDescendantOf` の上）に、2つ足す。

```cpp
void EntityManager::SetParentKeepWorld(Handle<Entity> child, Handle<Entity> parent) {
	TransformComponent* transform = Get<TransformComponent>(child);
	if (!transform) {
		return;
	}
	// 付け替える前の見た目（ワールド行列）を覚えておく
	const Matrix4x4 world = CalcWorldMatrix(child);
	// 新しい親のワールド行列。大きさが0の親は逆行列が作れないので、値は直さずに付け替えるだけにする
	const Matrix4x4 parentWorld = CalcWorldMatrix(parent); // parent が無効（root）なら単位行列
	const Vector3 axisX = {parentWorld.m[0][0], parentWorld.m[0][1], parentWorld.m[0][2]};
	const Vector3 axisY = {parentWorld.m[1][0], parentWorld.m[1][1], parentWorld.m[1][2]};
	const Vector3 axisZ = {parentWorld.m[2][0], parentWorld.m[2][1], parentWorld.m[2][2]};
	const bool canInvert = std::abs(Dot(Cross(axisX, axisY), axisZ)) > 1e-8f; // 3つの軸が作る箱の体積（行列式）が0でない

	SetParent(child, parent);
	const Entity* entity = Get(child);
	if (!entity || entity->parent != parent) {
		return; // 輪になるので断られた
	}
	if (!canInvert) {
		LogManager::Warning("大きさが0の親に付けたので、位置・向き・大きさはそのままにしました");
		return;
	}

	// 自分のワールド = 自分のローカル × 親のワールド なので、ローカル = 自分のワールド × 親のワールドの逆
	const Transform local = DecomposeAffineMatrix(world * Inverse(parentWorld));
	transform->translation = local.translation;
	transform->rotation = local.rotation;
	transform->scale = local.scale;
	transform->worldMatrix = world; // 見た目は同じなので、次の UpdateTransforms を待たずに読んでも正しい
}

Matrix4x4 EntityManager::CalcWorldMatrix(Handle<Entity> handle) {
	// 自分 → 親 → 親の親 … の順に掛けていく（UpdateTransforms と同じ「ローカル × 親のワールド」）
	Matrix4x4 world = MakeIdentity4x4();
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		const Entity* entity = instance_->entities_.Get(current);
		const TransformComponent* transform = Get<TransformComponent>(current);
		if (!entity || !transform) {
			break; // rootの上まで来た（無効なHandle）
		}
		world = world * MakeAffineMatrix(transform->scale, transform->rotation, transform->translation);
		current = entity->parent;
	}
	return world;
}
```

#### A-5. `MyEngine/Editor/History/EditorHistory.h`（3か所）

(1) インクルードに1行足す。

```cpp
#pragma once
#include <functional>
#include <string>
#include <vector>
```

(2) `RequestReparent` の行のコメントを直す（見た目の位置を変えなくなったので）。

```cpp
	static void RequestReparent(Handle<Entity> child, Handle<Entity> parent); // parentが無効ならrootへ。見た目の位置は変えない
```

(3) 一番下（`CommitComponentChanges` の下、`};` の上）に足す。

```cpp
	// 操作の区切り（マウスを離した・入力を確定した）で呼ぶ。ここまでの変更を1件の履歴にまとめる
	static void CommitComponentChanges();

	// ===== Entity の外の値（Parameters ウィンドウの調整項目など）=====
	// もう変えた値を、1件の履歴として積む。undo は「前の値に戻す」、redo は「変えた後の値にする」処理
	// 相手が見つからないなどで戻せなかったら false を返す（履歴が消える）
	static void Record(std::function<bool()> undo, std::function<bool()> redo);
};
```

#### A-6. `MyEngine/Editor/History/EditorHistory.cpp`（2か所）

(1) `EditorHistory::RequestReparent` を丸ごと差し替える。

```cpp
void EditorHistory::RequestReparent(Handle<Entity> child, Handle<Entity> parent) {
	if (parent.IsValid() && !EntityManager::IsAlive(parent)) {
		return;
	}
	requests.push_back([childId = IdOf(child), parentId = IdOf(parent)] {
		const Handle<Entity> childHandle = EntityManager::FindById(childId);
		const Entity* entity = EntityManager::Get(childHandle);
		if (!entity) {
			return;
		}
		const EntityId oldParentId = IdOf(entity->parent);
		if (oldParentId == parentId) {
			return;
		}
		// 見た目の位置を変えずに付け替える。Transform の値（親から見た位置）が変わるので、前と後の両方を覚えておく
		const TransformComponent oldTransform = *EntityManager::Get<TransformComponent>(childHandle);
		EntityManager::SetParentKeepWorld(childHandle, EntityManager::FindById(parentId));
		if (IdOf(EntityManager::Get(childHandle)->parent) != parentId) {
			return; // 輪になる付け替えはSetParentが断る（何も変わっていないので、履歴にも積まない）
		}
		const TransformComponent newTransform = *EntityManager::Get<TransformComponent>(childHandle);

		// Undo・Redo は計算し直さず、覚えた値をそのまま戻す（何度行き来しても値がずれない）
		auto set = [childId](EntityId newParentId, const TransformComponent& transform) {
			const Handle<Entity> target = EntityManager::FindById(childId);
			const Handle<Entity> parentHandle = EntityManager::FindById(newParentId);
			if (!target.IsValid() || (newParentId != 0 && !parentHandle.IsValid())) {
				return false;
			}
			EntityManager::SetParent(target, parentHandle);
			if (EntityManager::Get(target)->parent != parentHandle) {
				return false; // 輪になる付け替えはSetParentが断る
			}
			*EntityManager::Get<TransformComponent>(target) = transform;
			return true;
		};
		Append({[set, oldParentId, oldTransform] { return set(oldParentId, oldTransform); }, [set, parentId, newTransform] { return set(parentId, newTransform); }});
	});
}
```

- 前は `Execute`（実行してから積む）だったが、付け替えは `SetParentKeepWorld` でもう済ませたので、`Append`（積むだけ）にした。

(2) ファイルの一番下に足す。

```cpp
//=============================================================================
// Entity の外の値
//=============================================================================
void EditorHistory::Record(std::function<bool()> undo, std::function<bool()> redo) {
	// 値はもう変わっているので、実行はせずに積むだけ。ほかの予約と順番が入れ替わらないよう、予約の列を通す
	requests.push_back([undo = std::move(undo), redo = std::move(redo)] { Append({undo, redo}); });
}
```

#### A-7. `MyEngine/UI/GlobalVariables.h`（2か所）

(1) `ComboItem` と `ColorItem` に、比べる演算子を1行ずつ足す（`= default` は「メンバを全部比べる」をコンパイラに作らせる書き方）。

```cpp
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
```

(2) 一番下の `private:` の中、`CategoryFilePath` の下から `#endif` までを、次の形にする。

```cpp
	std::filesystem::path CategoryFilePath(const std::string& groupName, const std::string& categoryName) const { return GroupDirectory(groupName) / (categoryName + ".json"); }
	// 項目1つの値を書き換える。見つからなければ false（Undo・Redo から使う）
	bool SetItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& value);
#ifdef USE_IMGUI
	void DrawGroup(const std::string& groupName, GVNode& root);
	// categoryPath は "/" 区切りのカテゴリ（グループ直下なら空）
	void DrawNode(GVNode& node, const std::string& groupName, const std::string& categoryPath);
	void DrawItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item);
	void DrawSavedPopup();

	// ===== Undo・Redo（Inspector と同じく、マウスを離すまでの変更を1件にまとめる）=====
	struct PendingEdit {
		std::string groupName;
		std::string categoryPath;
		std::string itemName;
		Item before; // 触り始める前の値
		Item after;  // 最後に変えた値
	};
	void RecordEdit(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, const Item& before, const Item& after);
	void CommitEdits(); // どの項目も触っていなければ、ここまでの変更を EditorHistory に積む

	std::string savedMessage_;              // 保存完了ポップアップに出す文言
	std::vector<PendingEdit> pendingEdits_; // まだ履歴に積んでいない変更
#endif
```

#### A-8. `MyEngine/UI/GlobalVariables.cpp`（6か所）

(1) インクルードに足す（`LogManager.h` の下）。

```cpp
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Diagnostics/LogManager.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#endif
```

(2) `GlobalVariables::Update` の最後（`#endif` の上）に足し、関数の下に `SetItem` を足す。

```cpp
		ImGui::Begin(groupName.c_str()); // "Debug" や "GameScene" の窓を開く
		DrawGroup(groupName, rootNode);
		ImGui::End();
	}

	// どの項目も触っていない（マウスを離した・入力を確定した）なら、ここまでの変更を1件の履歴にする
	if (!ImGui::IsAnyItemActive()) {
		CommitEdits();
	}
#endif
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
```

(3) `DrawGroup` の中の2行を直す（`DrawItem`・`DrawNode` の引数が変わる）。

```cpp
		// カテゴリに属さないアイテムはその場で描画（通常は無い）
		if (std::holds_alternative<Item>(entry.value)) {
			DrawItem(groupName, "", entry.name, std::get<Item>(entry.value));
			continue;
		}
```

```cpp
		if (opened) {
			ImGui::Indent();
			DrawNode(*categoryNode, groupName, entry.name);
			ImGui::Unindent();
		}
```

(4) `DrawNode` を丸ごと差し替える（カテゴリのパスを、グループ名と分けて渡すようにした）。

```cpp
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
```

(5) `DrawItem` の頭（`std::visit` の上まで）を差し替える。`std::visit` の中身はそのまま（`label` と `uid` を、ここで作るようにした）。

```cpp
void GlobalVariables::DrawItem(const std::string& groupName, const std::string& categoryPath, const std::string& itemName, Item& item) {
	const std::string& label = itemName;
	const std::string uid = "##" + groupName + "/" + categoryPath + "/" + itemName; // ImGuiのID（表示はしない）
	const Item before = item; // 描く前の値。描いた後と比べて、変わっていたらUndoのために覚える

	std::visit( [&](auto& v) {
```

(6) `DrawItem` の最後（`item);` の下）に足し、関数の下（`// 保存完了ポップアップ` の上）に2つ足す。

```cpp
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
```

- `item == before` は `std::variant` 同士の比較。中に入っている型が同じで、値も同じなら true。`ComboItem`・`ColorItem` に `operator==` を足したのは、これのため（無いとコンパイルエラー）。
- `!(item == before)` と書いたのは、`!=` を用意していない型があっても通るように。

#### A-9. ビルド

エンジンをビルド（Debug と Release）→ ゲームをビルド。

---

### B. 音

#### B-1. 新規：`MyEngine/Sound/SoundBus.h`

```cpp
#pragma once
#include <cstdint>

// 音の種類。鳴らす音は必ずどちらかを通り、種類ごとに音量を決められる
// （Parameters ウィンドウの Audio / Volume、ゲームからは AudioSystem::SetBusVolume）
// SoundManager.h は XAudio2 を読むので重い。Component からはこのヘッダだけを読む


/// <summary>
/// 音の種類（音量のつまみ）
/// </summary>
enum class SoundBus : uint8_t {
	BGM, // 音楽
	SE,  // 効果音
};
```

#### B-2. `MyEngine/Sound/SoundManager.h`（7か所）

(1) インクルードに2行足す。

```cpp
#pragma once
#include <array>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <xaudio2.h>
#include "MyEngine/Math/Easing.h"
#include "MyEngine/Sound/SoundBus.h"
```

(2) `SoundConfig` の `// --- Play ---` に1行足す（そろえるために空白も直した）。

```cpp
		// --- Play ---
		float volume = 1.0f;         // 音量。0.0f=無音、1.0f=原音量
		float pitch = 1.0f;          // ピッチ。0.5f=1オクターブ下、1.0f=原音程、2.0f=1オクターブ上
		bool loop = false;           // trueでループ再生
		SoundBus bus = SoundBus::SE; // 音の種類（どの音量つまみを通すか）
```

(3) 1つ目の `Play` の説明に1行足し、引数を1つ増やす。

```cpp
	/// <param name="pitch">ピッチ 0.5f=1オクターブ下 1.0f=原音程 2.0f=1オクターブ上</param>
	/// <param name="bus">音の種類。BGM なら SoundBus::BGM を渡す（種類ごとの音量が掛かる）</param>
	/// <returns>再生ハンドル。Stop/Pause等の操作に使う</returns>
	static uint32_t Play(uint32_t soundHandle, bool loop, float volume = 1.0f, float pitch = 1.0f, SoundBus bus = SoundBus::SE);
```

(4) 1つ目の `SetPan` の説明に1行足す（真ん中の大きさが変わったので）。

```cpp
	/// <summary>
	/// 左右のパンニング（音の定位）を設定する。-1.0f=左 0.0f=中央 1.0f=右
	/// <para>中央では左右とも元の大きさ。右へ振るほど左が小さくなる（逆も同じ）</para>
	/// </summary>
	static void SetPan(uint32_t playHandle, float pan);
```

(5) 2つ目の `SetPan`（`SoundConfig` 版）の下に足す。

```cpp
	static void SetPan(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 3Dの音用。距離で決めた大きさ（0〜1）と左右（-1〜1）を掛ける。SetVolume の音量とは別に掛かる
	/// <para>毎フレーム呼んでよい（ログを出さない）。SetPan と同じ所を書き換えるので、後から呼んだ方が効く</para>
	/// </summary>
	static void SetSpatial(uint32_t playHandle, float gain, float pan);
```

(6) `GetSoundPath` の下（`private:` の上）に足す。

```cpp
	static const std::string& GetSoundPath(uint32_t soundHandle);

	// ===== 音の種類ごとの音量（0=無音、1=元の大きさ）=====
	// 鳴っている音にもすぐ効く。ふつうは AudioSystem（Parameters の Audio / Volume）から変える

	static void SetBusVolume(SoundBus bus, float volume);
	static float GetBusVolume(SoundBus bus);
	// 全部の音に掛かる音量
	static void SetMasterVolume(float volume);
	static float GetMasterVolume();
```

(7) `private:` の中の2か所。`PlayInternal` の引数を1つ増やし、`masterVoice_` の下に3行足す。

```cpp
	uint32_t PlayInternal(uint32_t soundHandle, bool loop, float volume, float pitch, SoundBus bus);
```

```cpp
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	// 音の種類ごとの通り道（SubmixVoice）。鳴らす音 → ここ → masterVoice_ の順に流れ、ここの音量が種類ごとの音量になる
	static constexpr size_t kBusCount = 2; // SoundBus の数
	std::array<IXAudio2SubmixVoice*, kBusCount> busVoices_ = {};
```

#### B-3. `MyEngine/Sound/SoundManager.cpp`（8か所）

(1) インクルードに足す（Windows のヘッダ群の下、`MyAssert.h` の上。外部ライブラリなので、ここ）。

```cpp
#include <xaudio2fx.h>
#include <mfreadwrite.h>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Diagnostics/MyAssert.h"
```

(2) `SoundManager* SoundManager::instance_ = nullptr;` の下に足す（左右への割り振り。`SetPan` と3Dの音の両方が使う）。

```cpp
// staticメンバの定義
SoundManager* SoundManager::instance_ = nullptr;

namespace {
constexpr uint32_t kBusChannels = 2; // 種類ごとの通り道は2ch（左右）。左右の振り分けは、いつもこの2chに対して書けばよい

// 音の出口（左右の2ch）への割り振りを書く。gain は全体に掛ける大きさ、pan は -1=左 〜 1=右
// 中央（pan=0）は左右とも gain のまま＝何もしていない音と同じ大きさ。右へ振るほど左だけ小さくする
void ApplyOutputMatrix(IXAudio2SourceVoice* voice, float gain, float pan) {
	const float left = gain * std::clamp(1.0f - pan, 0.0f, 1.0f);
	const float right = gain * std::clamp(1.0f + pan, 0.0f, 1.0f);
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	const uint32_t inputChannels = details.InputChannels;
	// matrix[入力ch数 × 出口ch + 入力ch] が「入力chから出口chへ送る大きさ」
	std::vector<float> matrix(inputChannels * kBusChannels, 0.0f);
	if (inputChannels == 1) {
		// モノラル：1つの音を左右の両方へ
		matrix[0] = left;
		matrix[1] = right;
	} else {
		// ステレオ以上：偶数chを左、奇数chを右へ
		for (uint32_t channel = 0; channel < inputChannels; ++channel) {
			const uint32_t output = channel % 2;
			matrix[inputChannels * output + channel] = (output == 0) ? left : right;
		}
	}
	// 送り先は種類ごとの通り道1つだけなので、nullptr（＝その1つ）でよい
	voice->SetOutputMatrix(nullptr, inputChannels, kBusChannels, matrix.data());
}
} // namespace
```

(3) `Play` の2つに、種類を渡す。

```cpp
uint32_t SoundManager::Play(uint32_t soundHandle, bool loop, float volume, float pitch, SoundBus bus) {
	MY_ASSERT(instance_ && "[SoundManager::Play] Init()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, loop, volume, pitch, bus);
}

uint32_t SoundManager::Play(uint32_t soundHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, config.loop, config.volume, config.pitch, config.bus);
}
```

(4) `GetSoundPath` の関数の下（`// 初期化` の見出しの上）に足す。

```cpp
//=============================================================================
// 3Dの音（距離の大きさと左右）
//=============================================================================
void SoundManager::SetSpatial(uint32_t playHandle, float gain, float pan) {
	// 毎フレーム呼ばれるのでログなし。鳴り終わった音には何もしない
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	const auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end() || it->second == nullptr) {
		return;
	}
	ApplyOutputMatrix(it->second, gain, pan);
}

//=============================================================================
// 音の種類ごとの音量
//=============================================================================
void SoundManager::SetBusVolume(SoundBus bus, float volume) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("{} の音量を{:.3f}にした", magic_enum::enum_name(bus), volume));
	instance_->busVoices_[static_cast<size_t>(bus)]->SetVolume(volume);
}

float SoundManager::GetBusVolume(SoundBus bus) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	float volume = 0.0f;
	instance_->busVoices_[static_cast<size_t>(bus)]->GetVolume(&volume);
	return volume;
}

void SoundManager::SetMasterVolume(float volume) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("全体の音量を{:.3f}にした", volume));
	instance_->masterVoice_->SetVolume(volume);
}

float SoundManager::GetMasterVolume() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	float volume = 0.0f;
	instance_->masterVoice_->GetVolume(&volume);
	return volume;
}
```

(5) `InitInternal` の、マスターボイスを作った後に足す。

```cpp
	// マスターボイス作成（最終的な音声出力先）
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	MY_ASSERT_MSG(SUCCEEDED(hr), "CreateMasteringVoice失敗");
	// 音の種類ごとの通り道（左右2ch・スピーカーと同じサンプリングレート）。鳴らす音はどれか1つを通ってマスターへ行く
	static_assert(magic_enum::enum_count<SoundBus>() == kBusCount, "SoundBus を足したら kBusCount も直してください");
	XAUDIO2_VOICE_DETAILS masterDetails;
	masterVoice_->GetVoiceDetails(&masterDetails);
	for (IXAudio2SubmixVoice*& busVoice : busVoices_) {
		hr = xAudio2_->CreateSubmixVoice(&busVoice, kBusChannels, masterDetails.InputSampleRate);
		MY_ASSERT_MSG(SUCCEEDED(hr), "CreateSubmixVoice失敗");
	}
	LogManager::Log("Initialized");
}
```

(6) `ReleaseInternal` の、マスターボイスを破棄する前に足す。

```cpp
	soundMap_.clear();
	pathID_.clear();
	// 種類ごとの通り道を破棄（流れ込む音を全部消した後、流れ出る先のマスターより先に）
	for (IXAudio2SubmixVoice*& busVoice : busVoices_) {
		if (busVoice) {
			busVoice->DestroyVoice();
			busVoice = nullptr;
		}
	}
	// マスターボイスを破棄
	if (masterVoice_) {
```

(7) `PlayInternal` の頭と、`SourceVoice` を作る所。

```cpp
uint32_t SoundManager::PlayInternal(uint32_t soundHandle, bool loop, float volume, float pitch, SoundBus bus) {
	LogManager::Log(std::format("soundHandle={} loop={} volume={:.3f} pitch={:.3f} bus={}", soundHandle, loop, volume, pitch, magic_enum::enum_name(bus)));
```

```cpp
	const SoundData& data = soundMap_[soundHandle];
	HRESULT hr;
	// SourceVoice作成。送り先は音の種類の通り道（マスターへ直接は送らない）
	XAUDIO2_SEND_DESCRIPTOR send = {0, busVoices_[static_cast<size_t>(bus)]};
	const XAUDIO2_VOICE_SENDS sends = {1, &send};
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &data.wfex, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends);
```

(8) `SetPanInternal` の後半（`// pan値からL/Rの出力比率を計算する` から関数の最後の `LogManager::Log` まで）を、次の3行にする。

```cpp
	// 左右への割り振り（3Dの音の SetSpatial と同じ関数。大きさは変えない）
	ApplyOutputMatrix(voice, 1.0f, pan);
	LogManager::Log(std::format("[SoundManager::SetPanInternal] playHandle={} パンを{:.3f}に設定した", playHandle, pan));
}
```

#### B-4. `MyEngine/Components/AudioSourceComponent.h`（全体を差し替え）

```cpp
#pragma once
#include <cstdint>

#include "MyEngine/Sound/SoundBus.h"

// 音を鳴らすためのデータ。Entityに付けるComponent（カテゴリ：Audio）
// 鳴っている音の番号（SoundManager の再生ハンドル）は持たない。AudioSystem が持つ
// （ここに入れると、コピー・Undo で2つの AudioSource が同じ音を指してしまう）


/// <summary>
/// 音源
/// </summary>
struct AudioSourceComponent {
	uint32_t soundHandle = 0;    // SoundManager::Load の番号（0は未選択）。保存はパスで
	float volume = 1.0f;         // 0=無音、1=元の大きさ、2=2倍
	float pitch = 1.0f;          // 0.5=1オクターブ下、1=元の高さ、2=1オクターブ上
	bool loop = false;           // 最後まで鳴ったら頭から鳴らし直す（BGM）
	bool playOnStart = true;     // シーンが始まったとき（Play を押したとき）に鳴らす
	SoundBus bus = SoundBus::SE; // 音の種類。種類ごとの音量（Parameters の Audio / Volume）が掛かる

	// --- 3Dの音（聞く人＝ゲームのカメラ）---
	bool spatial = false;      // true: 離れると小さくなり、左右に振れる（位置は Transform）
	float minDistance = 1.0f;  // ここまで近いと元の大きさ
	float maxDistance = 20.0f; // ここより遠いと聞こえない（その間はまっすぐ小さくなる）
};
```

#### B-5. `MyEngine/ECS/ComponentUI.h`・`ComponentUI.cpp`・`ComponentSerializer.cpp`（ボタン）

試し聞きの ▶ を Inspector に出すため、見せ方の道具（`ComponentUI`）にボタンを足す。保存・読み込みのときは、いつも「押されていない」。ゲームの `COMPONENT(...)` の中でも使える（`if (ui.Button("Reset")) { value = {}; }` など）。

(1) `ComponentUI.h`：`Space()` の下に足す。

```cpp
	virtual void Space() = 0;                               // 1行あける

	// ===== ボタン（保存には関係しない。押された瞬間だけ true。保存・読み込みのときはいつも false）=====
	// 値を変えずに何かしたいとき（音の試し聞きなど）に使う
	virtual bool Button(const char* label) = 0;
```

(2) `ComponentUI.cpp`：2つの `InspectorComponentUI`（上が Inspector に描く係、下が Release の何もしない係）の `Space()` の下に1行ずつ。

```cpp
	void Space() override { ImGui::Spacing(); }
	bool Button(const char* label) override { return ImGui::Button(label); }
```

```cpp
	void Space() override {}
	bool Button(const char*) override { return false; }
```

(3) `ComponentSerializer.cpp`：書く係（`JsonWriterUI`）と読む係（`JsonReaderUI`）の `Space()` の下に1行ずつ（書く係のコメントも直す）。

```cpp
	// 飾り・ボタンは保存しない
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}
	bool Button(const char*) override { return false; }
```

```cpp
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}
	bool Button(const char*) override { return false; }

	// ファイルにあったのに誰も読まなかった項目を知らせる（ラベルを変えた・項目を消したときに気づけるように）
```

#### B-6. `MyEngine/Systems/AudioSystem.h`・`AudioSystem.cpp`（全体を差し替え）

`AudioSystem.h`：

```cpp
#pragma once
#include "MyEngine/Components/AudioSourceComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Sound/SoundBus.h"

// 前方宣言
class Camera;


/// <summary>
/// AudioSourceComponent を持つEntityの音を、鳴らす・止める
/// <para>鳴っている音の番号（SoundManager の再生ハンドル）はここが持つ。Componentはデータだけ</para>
/// <para>エディタの停止中は鳴らさない。Entityを消す・無効にする・AudioSource を外すと、その音は止まる</para>
/// <para>一覧などの中身は AudioSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class AudioSystem {
public:
	// AudioSourceComponent を登録する（置き場所・保存・Inspector）。種類ごとの音量を Parameters に出す。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::FlushDestroy の後に呼ぶ
	/// <para>種類ごとの音量を伝え、消えた・外した・無効になった AudioSource の音を止め、始まったばかりの AudioSource を鳴らし（Play On Start）、
	/// Inspector で変えた音量・ピッチと、3Dの音の大きさ・左右を鳴っている音に伝える</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false）</param>
	/// <param name="listener">聞く人（3Dの音の距離と左右の基準）。ゲームのカメラを渡す。nullptr なら3Dの音も普通の音と同じに鳴る</param>
	static void Update(bool isGameRunning, const Camera* listener);

	// ===== ゲームから（SYSTEM の中で）使う =====

	// そのEntityの AudioSource を頭から鳴らす（鳴っていたら止めてから）。AudioSource が無い・音が未選択なら false
	static bool Play(Handle<Entity> entity);
	// そのEntityの音を止める（鳴っていなければ何もしない）
	static void Stop(Handle<Entity> entity);
	// そのEntityの音が鳴っているか（一時停止中も true。鳴り終わったら false）
	static bool IsPlaying(Handle<Entity> entity);
	// 重ねて鳴らす（効果音）。Entityを消しても最後まで鳴る。AudioSystem::Stop では止まらない（エディタの Stop では止まる）
	// 3Dの音なら、鳴らした瞬間の位置で大きさと左右が決まる（鳴っている間は動かない）
	static bool PlayOneShot(Handle<Entity> entity);

	// ===== 音の種類ごとの音量（設定画面から。0=無音、1=元の大きさ）=====
	// Parameters ウィンドウの Audio / Volume と同じ値（そこで Save すると、次の起動からその値で始まる）
	static void SetBusVolume(SoundBus bus, float volume);
	static float GetBusVolume(SoundBus bus);
	static void SetMasterVolume(float volume);
	static float GetMasterVolume();

	// ===== エディタの再生コントロールから使う（SceneManager が呼ぶ）=====

	// 全部の音を止める（AudioSource の音も、SoundManager で直接鳴らした音も、試し聞きも）。シーンを作り直す前に呼ぶ
	static void StopAll();
	// 全部の音を一時停止する / 再開する
	static void SetPaused(bool paused);

	// ===== 試し聞き（Inspector の Audio Source の ▶ から）=====
	// 停止中でも鳴らせる。同時に1つだけ（鳴っていたら止めてから鳴らす）。ループ・3D・Play On Start は使わない
	static void Preview(const AudioSourceComponent& source);
	static void StopPreview();
	static bool IsPreviewing();
};
```

`AudioSystem.cpp`：

```cpp
#include "AudioSystem.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Sound/SoundManager.h"
#include "MyEngine/UI/GlobalVariables.h"

namespace {
constexpr float kMaxVolume = 2.0f; // 音量の上限（Inspector のスライダーと同じ。1より上は元より大きくなる）
constexpr float kMinPitch = 0.5f;  // ピッチの下限（Inspector のスライダーと同じ）
constexpr float kMaxPitch = 2.0f;  // ピッチの上限（XAudio2 は、音を作るときの上限＝既定の2倍までしか上げられない）

// 種類ごとの音量を置く場所（Parameters ウィンドウの Audio タブ → Volume）
constexpr const char* kVolumeGroup = "Audio";
constexpr const char* kVolumeCategory = "Volume";
constexpr const char* kMasterName = "Master";
constexpr size_t kBusCount = magic_enum::enum_count<SoundBus>();

/// <summary>
/// AudioSource 1つ分の、実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct SourceState {
	Handle<Entity> entity;    // 持ち主
	uint32_t playHandle = 0;  // 鳴っている音（SoundManager の再生ハンドル。0は鳴っていない）
	uint32_t soundHandle = 0; // 鳴らした音の素材（Inspector で音を変えたら止める）
	float volume = 1.0f;      // SoundManager に最後に伝えた音量（変わったときだけ伝える。SoundManager は呼ぶたびにログを出すので）
	float pitch = 1.0f;       // 最後に伝えたピッチ
	float gain = 1.0f;        // 最後に伝えた3Dの大きさ（3Dでない音は1のまま）
	float pan = 0.0f;         // 最後に伝えた3Dの左右（3Dでない音は0のまま）
};

/// <summary>
/// AudioSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct AudioState {
	// 見つけた AudioSource の一覧。鳴っていなくても持つ（「もう始まった」の印。無いと Play On Start が毎フレーム鳴り直す）
	std::vector<SourceState> sources;
	// 聞く人（ゲームのカメラ）。3Dの音の距離と左右の基準
	bool hasListener = false;
	Vector3 listenerPosition = {0.0f, 0.0f, 0.0f};
	Vector3 listenerRight = {1.0f, 0.0f, 0.0f};
	// SoundManager に最後に伝えた、種類ごとの音量（-1 は「まだ伝えていない」）
	float masterVolume = -1.0f;
	std::array<float, kBusCount> busVolumes;
	// 試し聞きの音（0は鳴っていない）
	uint32_t previewHandle = 0;
};
std::unique_ptr<AudioState> state;

// 範囲に収めた音量・ピッチ（ファイルを手で書き換えて範囲の外になっても、XAudio2 に変な値を渡さない）
float ClampVolume(float volume) { return std::clamp(volume, 0.0f, kMaxVolume); }
float ClampPitch(float pitch) { return std::clamp(pitch, kMinPitch, kMaxPitch); }

// 種類の名前（Parameters の項目名。"BGM" / "SE"）
std::string BusName(SoundBus bus) { return std::string(magic_enum::enum_name(bus)); }

// Entityの状態を探す（無ければ nullptr）。AudioSource の数しか無いので、順に見れば足りる
SourceState* FindSource(Handle<Entity> entity) {
	const auto it = std::ranges::find(state->sources, entity, &SourceState::entity);
	return (it != state->sources.end()) ? &*it : nullptr;
}

// Entityの状態を探し、無ければ足す（ゲームの Play が、そのEntityを Update が見つけるより先だったとき）
SourceState& FindOrAddSource(Handle<Entity> entity) {
	if (SourceState* source = FindSource(entity)) {
		return *source;
	}
	state->sources.push_back({entity});
	return state->sources.back();
}

//=============================================================================
// 3Dの音
//=============================================================================
// 聞く人（カメラ）の位置と右の向き。カメラの行列の4行目が位置、1行目が右の向き
void UpdateListener(const Camera* listener) {
	state->hasListener = (listener != nullptr);
	if (listener == nullptr) {
		return;
	}
	const Matrix4x4 cameraMatrix = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, listener->GetRotation(), listener->GetTranslation());
	state->listenerPosition = listener->GetTranslation();
	state->listenerRight = Normalize(Vector3{cameraMatrix.m[0][0], cameraMatrix.m[0][1], cameraMatrix.m[0][2]});
}

// 3Dの音の大きさ（gain）と左右（pan）を決める。3Dでない音・聞く人がいないときは 1 と 0（普通の音と同じ）
void CalcSpatial(Handle<Entity> entity, const AudioSourceComponent& component, float& gain, float& pan) {
	gain = 1.0f;
	pan = 0.0f;
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	if (!component.spatial || !state->hasListener || transform == nullptr) {
		return;
	}
	const Vector3 toSource = GetWorldPosition(*transform) - state->listenerPosition;
	const float distance = Length(toSource);
	// 大きさ：minDistance までは1、maxDistance で0、その間はまっすぐ下がる
	const float minDistance = (std::max)(component.minDistance, 0.0f);
	const float maxDistance = (std::max)(component.maxDistance, minDistance + 0.001f); // 0で割らないように、min より少しは遠くする
	gain = 1.0f - std::clamp((distance - minDistance) / (maxDistance - minDistance), 0.0f, 1.0f);
	// 左右：音のある向きと、カメラの右の向きの内積（真右で1、真左で-1、正面・真後ろ・真上で0）
	if (distance > 1e-4f) {
		pan = Dot(toSource / distance, state->listenerRight);
	}
}

// 鳴っている音に3Dの大きさ・左右を伝える（変わったときだけ）
void UpdateSpatial(SourceState& source, const AudioSourceComponent& component) {
	float gain = 1.0f;
	float pan = 0.0f;
	CalcSpatial(source.entity, component, gain, pan);
	if (gain != source.gain || pan != source.pan) {
		source.gain = gain;
		source.pan = pan;
		SoundManager::SetSpatial(source.playHandle, gain, pan);
	}
}

//=============================================================================
// 鳴らす・止める
//=============================================================================
// 鳴っていれば止める（鳴り終わった・StopAll で止まった音には何もしない＝SoundManager に警告を出させない）
void StopSource(SourceState& source) {
	if (SoundManager::IsPlaying(source.playHandle)) {
		SoundManager::Stop(source.playHandle);
	}
	source.playHandle = 0;
}

// 頭から鳴らす（鳴っていたら止めてから）。音が未選択なら鳴らさない
bool StartSource(SourceState& source, const AudioSourceComponent& component) {
	StopSource(source);
	if (component.soundHandle == 0) {
		return false;
	}
	source.soundHandle = component.soundHandle;
	source.volume = ClampVolume(component.volume);
	source.pitch = ClampPitch(component.pitch);
	source.playHandle = SoundManager::Play(source.soundHandle, component.loop, source.volume, source.pitch, component.bus);
	// 作ったばかりの音は、大きさ1・真ん中。3Dの音なら、最初の1フレームから距離の大きさで鳴らす
	source.gain = 1.0f;
	source.pan = 0.0f;
	UpdateSpatial(source, component);
	return source.playHandle != 0;
}

// 鳴っている音に、Inspector などで変えた値を伝える
void ApplyChanges(SourceState& source, const AudioSourceComponent& component) {
	// 鳴り終わった（ループしない音が最後まで鳴った。SoundManager::CleanupSourceVoices が片付けた）
	if (!SoundManager::IsPlaying(source.playHandle)) {
		source.playHandle = 0;
		return;
	}
	// 音そのものを変えた → 止める（前の音のまま鳴り続けないように）
	if (component.soundHandle != source.soundHandle) {
		StopSource(source);
		return;
	}
	// 音量・ピッチ（変わったときだけ伝える）
	const float volume = ClampVolume(component.volume);
	if (volume != source.volume) {
		source.volume = volume;
		SoundManager::SetVolume(source.playHandle, volume);
	}
	const float pitch = ClampPitch(component.pitch);
	if (pitch != source.pitch) {
		source.pitch = pitch;
		SoundManager::SetPitch(source.playHandle, pitch);
	}
	// 3Dの音（動いた・カメラが動いた・Spatial を切り替えた）
	UpdateSpatial(source, component);
}

//=============================================================================
// 種類ごとの音量
//=============================================================================
// Parameters の値を SoundManager に伝える（変わったときだけ）
void ApplyVolumes() {
	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float master = ClampVolume(gv->Get<float>(kVolumeGroup, kVolumeCategory, kMasterName));
	if (master != state->masterVolume) {
		state->masterVolume = master;
		SoundManager::SetMasterVolume(master);
	}
	for (SoundBus bus : magic_enum::enum_values<SoundBus>()) {
		const float volume = ClampVolume(gv->Get<float>(kVolumeGroup, kVolumeCategory, BusName(bus)));
		float& applied = state->busVolumes[static_cast<size_t>(bus)];
		if (volume != applied) {
			applied = volume;
			SoundManager::SetBusVolume(bus, volume);
		}
	}
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// AudioSource の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Collider と同じ書き方）
// 保存の名前はメンバ名にし、Inspector には「表示###保存の名前」で読みやすい名前を出す
void DescribeAudioSource(ComponentUI& ui, AudioSourceComponent& value) {
	ui.AssetField("Sound###sound", value.soundHandle, AssetType::Sound, Tip("resources の下の .wav / .mp3"));
	// 試し聞き（停止中でも鳴る。保存・読み込みのときは Button が false を返すので何も起きない）
	if (AudioSystem::IsPreviewing()) {
		if (ui.Button("■ Stop Preview")) {
			AudioSystem::StopPreview();
		}
	} else if (ui.Button("▶ Preview")) {
		AudioSystem::Preview(value);
	}
	ui.Field("Volume###volume", value.volume, Range(0.0f, kMaxVolume));
	ui.Field("Pitch###pitch", value.pitch, Range(kMinPitch, kMaxPitch).Tip("0.5で1オクターブ下、2で1オクターブ上"));
	ui.Field("Loop###loop", value.loop, Tip("最後まで鳴ったら頭から（BGM）。鳴っている途中で変えたときは、次に鳴らしたときから効く"));
	ui.Field("Play On Start###playOnStart", value.playOnStart, Tip("Play を押したとき（シーンが始まったとき）に鳴らす"));
	ui.Field("Bus###bus", value.bus, Tip("音の種類。種類ごとの音量は Parameters ウィンドウの Audio / Volume"));
	ui.Separator("3D");
	ui.Field("Spatial###spatial", value.spatial, Tip("離れると小さくなり、左右に振れる（聞く人はゲームのカメラ）"));
	ui.Field("Min Distance###minDistance", value.minDistance, AtLeast(0.0f).Tip("ここまで近いと元の大きさ"));
	ui.Field("Max Distance###maxDistance", value.maxDistance, AtLeast(0.0f).Tip("ここより遠いと聞こえない"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void AudioSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<AudioState>();
	state->busVolumes.fill(-1.0f);

	// AudioSource の置き場所・保存・Inspector を登録する。見せ方は DescribeAudioSource 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<AudioSourceComponent>();
	ComponentSerializer::Register<AudioSourceComponent>("Audio Source", &DescribeAudioSource);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<AudioSourceComponent>>("Audio Source", "Audio", &DescribeAudioSource));

	// 種類ごとの音量を Parameters ウィンドウに出す（Save すると Parameters/Audio/Volume.json に残り、次の起動で読む）
	GlobalVariables* gv = GlobalVariables::GetInstance();
	GlobalVariables::CategoryBuilder volume = gv->Group(kVolumeGroup).Category(kVolumeCategory);
	volume.Add(kMasterName, 1.0f);
	for (SoundBus bus : magic_enum::enum_values<SoundBus>()) {
		volume.Add(BusName(bus), 1.0f);
	}
	gv->LoadGroup(kVolumeGroup);
}

// SoundManager はウィンドウと一緒に先に片付いているので、ここでは SoundManager を呼ばない
void AudioSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void AudioSystem::Update(bool isGameRunning, const Camera* listener) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	std::vector<SourceState>& sources = state->sources;

	// ===== 種類ごとの音量・聞く人（停止中も。試し聞きにも効くように）=====
	ApplyVolumes();
	UpdateListener(listener);

	// ===== 停止中は鳴らさない（試し聞きは止めない）=====
	if (!isGameRunning) {
		for (SourceState& source : sources) {
			StopSource(source);
		}
		sources.clear(); // 次に Play したときは、始まりからやり直す
		return;
	}

	// ===== 1. 消えた・外した・無効になった AudioSource の音を止めて、一覧から外す =====
	const auto isGone = [](const SourceState& source) { return EntityManager::Get<AudioSourceComponent>(source.entity) == nullptr || !EntityManager::IsActiveInHierarchy(source.entity); };
	for (SourceState& source : sources) {
		if (isGone(source)) {
			StopSource(source);
		}
	}
	std::erase_if(sources, isGone);

	// ===== 2. 初めて見つけた AudioSource は一覧に足して、Play On Start なら鳴らす。見つけ済みの物は変更を伝える =====
	EntityManager::ForEach<AudioSourceComponent>([&sources](Handle<Entity> entity, const AudioSourceComponent& component) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (SourceState* source = FindSource(entity)) {
			ApplyChanges(*source, component);
			return;
		}
		sources.push_back({entity});
		if (component.playOnStart) {
			StartSource(sources.back(), component);
		}
	});
}

//=============================================================================
// ゲームから
//=============================================================================
bool AudioSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const AudioSourceComponent* component = EntityManager::Get<AudioSourceComponent>(entity);
	// AudioSource が無い（RequestAdd した直後は、次のフレームまで付いていない）・無効なEntity
	if (component == nullptr || !EntityManager::IsActiveInHierarchy(entity)) {
		return false;
	}
	// 一覧に入れるので、この後の Update が Play On Start でもう一度鳴らすことはない
	return StartSource(FindOrAddSource(entity), *component);
}

void AudioSystem::Stop(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	if (SourceState* source = FindSource(entity)) {
		StopSource(*source);
	}
}

bool AudioSystem::IsPlaying(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const SourceState* source = FindSource(entity);
	return source != nullptr && SoundManager::IsPlaying(source->playHandle);
}

bool AudioSystem::PlayOneShot(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const AudioSourceComponent* component = EntityManager::Get<AudioSourceComponent>(entity);
	if (component == nullptr || component->soundHandle == 0) {
		return false;
	}
	// 一覧には入れない（持ち主が消えても止めない）。ループはしない（止める手段が StopAll しか無いので）
	const uint32_t playHandle = SoundManager::Play(component->soundHandle, false, ClampVolume(component->volume), ClampPitch(component->pitch), component->bus);
	// 3Dの音は、鳴らした瞬間の位置で大きさと左右を決める（一覧に無いので、この後は動かない）
	float gain = 1.0f;
	float pan = 0.0f;
	CalcSpatial(entity, *component, gain, pan);
	if (component->spatial) {
		SoundManager::SetSpatial(playHandle, gain, pan);
	}
	return playHandle != 0;
}

//=============================================================================
// 種類ごとの音量
//=============================================================================
// Parameters の値を書き換える（SoundManager へは次の Update で伝わる）
void AudioSystem::SetBusVolume(SoundBus bus, float volume) { GlobalVariables::GetInstance()->Set<float>(kVolumeGroup, kVolumeCategory, BusName(bus), volume); }
float AudioSystem::GetBusVolume(SoundBus bus) { return GlobalVariables::GetInstance()->Get<float>(kVolumeGroup, kVolumeCategory, BusName(bus)); }
void AudioSystem::SetMasterVolume(float volume) { GlobalVariables::GetInstance()->Set<float>(kVolumeGroup, kVolumeCategory, kMasterName, volume); }
float AudioSystem::GetMasterVolume() { return GlobalVariables::GetInstance()->Get<float>(kVolumeGroup, kVolumeCategory, kMasterName); }

//=============================================================================
// エディタの再生コントロールから
//=============================================================================
void AudioSystem::StopAll() {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::StopAll();
	state->sources.clear(); // 作り直した後に見つけた AudioSource は「始まったばかり」になる（Play On Start がまた効く）
	state->previewHandle = 0;
}

void AudioSystem::SetPaused(bool paused) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::SetPaused(paused);
}

//=============================================================================
// 試し聞き
//=============================================================================
void AudioSystem::Preview(const AudioSourceComponent& source) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	StopPreview();
	if (source.soundHandle == 0) {
		return;
	}
	state->previewHandle = SoundManager::Play(source.soundHandle, false, ClampVolume(source.volume), ClampPitch(source.pitch), source.bus);
}

void AudioSystem::StopPreview() {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	if (SoundManager::IsPlaying(state->previewHandle)) {
		SoundManager::Stop(state->previewHandle);
	}
	state->previewHandle = 0;
}

bool AudioSystem::IsPreviewing() {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	return SoundManager::IsPlaying(state->previewHandle);
}
```

前の版から変わった所：
- `kMaxVolume`（音量の上限 2。前は `ClampVolume` が 1 で止めていた）、`SourceState` の `gain`・`pan`、`AudioState` の聞く人・種類ごとの音量・試し聞き。
- 「3Dの音」「種類ごとの音量」の関数と、`DescribeAudioSource` の ▶ ボタン・Bus・3D の項目。
- `StartSource`・`ApplyChanges`・`PlayOneShot` が、種類と3Dを使う。`Update` の頭で音量と聞く人を更新。
- 下の「種類ごとの音量」「試し聞き」の関数。

#### B-7. `MyEngine/Window/WindowManager.cpp`（1行）

`UpdateAll` の中の `AudioSystem::Update` に、カメラを渡す（上のほうで探しているパーティクル用のカメラ＝ゲームのカメラ）。

```cpp
	AudioSystem::Update(isGameRunning, particleCamera); // 3Dの音は、パーティクルと同じゲームのカメラで聞く
```

#### B-8. プロジェクトに追加・ビルド

1. `MyEngine/Sound/SoundBus.h` をプロジェクトに追加（ヘッダだけなので、足し忘れてもビルドは通る。ソリューションエクスプローラーに出すため）。
2. エンジンをビルド（Debug と Release）→ ゲームをビルド。

---

### C. カメラ

#### C-1. 新規：`MyEngine/Components/CameraComponent.h`

```cpp
#pragma once

// ゲームの絵を撮るカメラ。Entityに付けるComponent（カテゴリ：Rendering3D）
// 位置・向きは Transform（親の下に置けば一緒に動く）。ここに持つのは写り方だけ
// 実際のカメラ（Camera クラス）は CameraSystem が1つ持ち、毎フレーム一番 priority の高い Camera の値を写す


/// <summary>
/// カメラ
/// </summary>
struct CameraComponent {
	float fovY = 45.0f;    // 縦の視野角（度）。小さいほど望遠
	float nearZ = 0.1f;    // これより近い物は映らない
	float farZ = 1000.0f;  // これより遠い物は映らない
	int priority = 0;      // 有効な Camera が複数あるとき、大きい方を使う（同じなら先に見つけた方）
};
```

#### C-2. 新規：`MyEngine/Systems/CameraSystem.h`・`CameraSystem.cpp`

`CameraSystem.h`：

```cpp
#pragma once
#include "MyEngine/Components/CameraComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

// 前方宣言
class Camera;


/// <summary>
/// CameraComponent を持つEntityから、ゲームの絵を撮るカメラを決める
/// <para>カメラ（Camera クラス）はここが1つだけ持ち、毎フレーム、有効な Camera Component の中で priority が一番大きい物の
/// 位置・向き（Transform）と写り方をそこへ写す。シーンの GetCamera() は、上書きしなければこのカメラを返す</para>
/// </summary>
class CameraSystem {
public:
	// CameraComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	// 毎フレーム1回、EntityManager::UpdateTransforms の後に呼ぶ（親の下に置いたカメラも、そのフレームの位置で撮るため）
	static void Update();

	// ゲームの絵を撮るカメラ。Camera Component が1つも無いときは、既定の位置（(0, 0, -10) から +Z を向く）のカメラ
	// nullptr にはならない。いつも同じ物を返すので、ポインタを覚えておいてよい
	static Camera* GetMainCamera();
	// 今使っている Camera Component の Entity（無ければ無効な Handle）
	static Handle<Entity> GetMainCameraEntity();
};
```

`CameraSystem.cpp`：

```cpp
#include "CameraSystem.h"

#include <algorithm>
#include <memory>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"

namespace {
constexpr float kAspectRatio = 1280.0f / 720.0f; // 縦横比（Game ビューの描画先・スプライトの基準と同じ 16:9）
constexpr float kMinFovY = 1.0f;                 // 視野角の下限（度）
constexpr float kMaxFovY = 179.0f;               // 視野角の上限（度）。180度以上は透視投影で作れない
const Vector3 kDefaultPosition = {0.0f, 0.0f, -10.0f}; // Camera Component が無いときの位置（原点を正面から見る）

/// <summary>
/// CameraSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct CameraState {
	Camera camera;               // ゲームの絵を撮るカメラ（GetMainCamera はいつもこれを返す）
	Handle<Entity> mainEntity;   // 今使っている Camera Component の Entity
	bool hasLoggedNoCamera = false; // 「Camera Component が無い」を知らせたか（続けて何度も出さない）
};
std::unique_ptr<CameraState> state;

// 写り方をカメラへ（視野角は度で持っているので、ラジアンに直して渡す）
void ApplyProjection(const CameraComponent& component) {
	const float fovY = std::clamp(component.fovY, kMinFovY, kMaxFovY) * kDegToRad;
	const float nearZ = (std::max)(component.nearZ, 0.001f);
	const float farZ = (std::max)(component.farZ, nearZ + 0.01f); // far が near より手前だと、何も映らない
	state->camera.Initialize(fovY, kAspectRatio, nearZ, farZ);     // Initialize の中で行列も作り直す
}

// Camera Component が無いときの、既定のカメラ
void UseDefaultCamera() {
	state->camera.SetTranslation(kDefaultPosition);
	state->camera.SetRotation({0.0f, 0.0f, 0.0f});
	state->camera.SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(CameraComponent{});
}

// Camera の見せ方。Inspector とシーンファイルの両方がこの並びを使う（保存の名前はメンバ名）
void DescribeCamera(ComponentUI& ui, CameraComponent& value) {
	ui.Field("Field Of View###fovY", value.fovY, Range(kMinFovY, kMaxFovY).Tip("縦の視野角（度）。小さいほど望遠"));
	ui.Field("Near###nearZ", value.nearZ, AtLeast(0.001f).Tip("これより近い物は映らない"));
	ui.Field("Far###farZ", value.farZ, AtLeast(0.01f).Tip("これより遠い物は映らない"));
	ui.Field("Priority###priority", value.priority, Tip("有効な Camera が複数あるとき、大きい方を使う（切り替えはこの値か、Entity の有効・無効で）"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void CameraSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<CameraState>();
	UseDefaultCamera(); // 最初の Update より前（シーンの Initialize）に使われても、ちゃんとしたカメラを返せるように

	EntityManager::RegisterComponent<CameraComponent>();
	ComponentSerializer::Register<CameraComponent>("Camera", &DescribeCamera);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraComponent>>("Camera", "Rendering3D", &DescribeCamera));
}

void CameraSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void CameraSystem::Update() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");

	// ===== 1. 使う Camera を選ぶ（有効な物の中で priority が一番大きい物。同じなら先に見つけた物）=====
	Handle<Entity> best;
	CameraComponent bestComponent;
	EntityManager::ForEach<CameraComponent>([&](Handle<Entity> entity, const CameraComponent& component) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (!best.IsValid() || component.priority > bestComponent.priority) {
			best = entity;
			bestComponent = component;
		}
	});
	state->mainEntity = best;

	// ===== 2. 無ければ既定のカメラ =====
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(best);
	if (transform == nullptr) {
		if (!state->hasLoggedNoCamera) {
			LogManager::Log("Camera Component が無いので、既定のカメラ（(0, 0, -10) から +Z を向く）で撮ります。Hierarchy の ＋ → Camera で置けます");
			state->hasLoggedNoCamera = true;
		}
		UseDefaultCamera();
		return;
	}
	state->hasLoggedNoCamera = false;

	// ===== 3. 位置・向きはワールド行列から（親の下に置いても、見えている場所・向きから撮る）=====
	// 大きさは写さない（ビュー行列に大きさが混ざると、絵が伸び縮みする）
	const Transform world = DecomposeAffineMatrix(transform->worldMatrix);
	state->camera.SetTranslation(world.translation);
	state->camera.SetRotation(world.rotation);
	state->camera.SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(bestComponent);
}

//=============================================================================
// 取得
//=============================================================================
Camera* CameraSystem::GetMainCamera() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return &state->camera;
}

Handle<Entity> CameraSystem::GetMainCameraEntity() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return state->mainEntity;
}
```

- `kDegToRad` は `GameComponent.h` にある物（度 → ラジアン）。
- `Camera::Initialize` は、値を入れて行列を作り直す関数（何度呼んでもよい）。

#### C-3. `MyEngine/Scene/IScene.h`（2か所）

(1) インクルードに足す。

```cpp
#include <string>
#include <memory>
#include <functional>

#include "MyEngine/Systems/CameraSystem.h"
```

(2) `GetCamera` を、上書きしなかったら Camera Component のカメラを返すようにする（コメントも1行足す）。

```cpp
	// ゲームの絵を撮るカメラ。ふつうは上書きしない（Hierarchy に置いた Camera Component のカメラになる。CameraSystem）
	virtual Camera* GetCamera() { return CameraSystem::GetMainCamera(); }
```

#### C-4. `MyEngine/Engine.cpp`（3か所）

(1) インクルード（Systems の所）。

```cpp
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
```

(2) `Initialize` の `AudioSystem::Initialize()` の下。

```cpp
	AudioSystem::Initialize();     // AudioSourceをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	CameraSystem::Initialize();    // CameraをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```

(3) `Finalize` の `AudioSystem::Release()` の下。

```cpp
	AudioSystem::Release();     // シーン（ウィンドウ）の破棄より後にする
	CameraSystem::Release();    // シーン（ウィンドウ）の破棄より後にする
```

#### C-5. `MyEngine/Window/WindowManager.cpp`（2か所）

(1) インクルード。

```cpp
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/LightSystem.h"
```

(2) `UpdateAll` の `EntityManager::UpdateTransforms()` の下に1行。

```cpp
	EntityManager::UpdateTransforms();     // ワールド行列（親→子）
	CameraSystem::Update();                // Camera Component の位置・向きをゲームのカメラへ写す（ワールド行列の後、使う人たちの前）
	ParticleManager::Update();             // パーティクル更新
```

- パーティクル用のカメラを探す所（`UpdateTransforms` の上）はそのまま。探しているのはポインタだけで、中身（位置・行列）はこの行で更新される。

#### C-6. `MyEngine/Editor/Windows/HierarchyWindow.cpp`（2か所）

(1) インクルード。

```cpp
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Components/CameraComponent.h"
#include "MyEngine/Components/LightComponent.h"
```

(2) `DrawCreateMenu` の、Light のメニューの下（`ImGui::Separator();` の上）に足す。

```cpp
		ImGui::EndMenu();
	}
	// --- カメラ：Camera Component を付けた状態で作る（ゲームの絵はこのカメラから撮る）---
	if (ImGui::MenuItem("Camera")) {
		TransformComponent transform;
		transform.translation = {0.0f, 0.0f, -10.0f}; // 原点を正面（-Z側）から見る
		EditorHistory::RequestCreate("Main Camera", {}, {ComponentSnapshot::Make(transform), ComponentSnapshot::Make(CameraComponent{})});
	}
	ImGui::Separator();
```

#### C-7. `MyEngine/ECS/GameComponent.h`（2か所）

ゲームの Component から `CameraSystem` を使えるように。

(1) 説明のコメントに1行足す。

```cpp
//   音は、③の中で AudioSystem::Play(entity) / PlayOneShot(entity) を呼ぶ（Audio Source は Add Component → Audio で付ける）
//   カメラは Entity に付ける Camera Component。追いかけるなら③でそのEntityの Transform を動かす（CameraSystem::GetMainCameraEntity）
```

(2) インクルード。

```cpp
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CameraSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
```

#### C-8. プロジェクトに追加・ビルド

1. 新しい3ファイルをプロジェクトに追加する。
   - `MyEngine/Components/CameraComponent.h`
   - `MyEngine/Systems/CameraSystem.h`・`CameraSystem.cpp`
   - **`CameraSystem.cpp` を足し忘れても、エンジンのビルドは通ってしまう**（静的ライブラリなので）。エラーはゲームのリンクで `error LNK2019: ... CameraSystem::Initialize` のように出る。
2. エンジンをビルド（Debug と Release）→ ゲームをビルド。**ゲームは直さなくてもビルドが通り、今までどおり GameScene のカメラで動く**（GameScene が `GetCamera` を上書きしているので）。Camera Component に切り替えるのはパートE。

---

### D. 入力

#### D-1. `MyEngine/Input/InputManager.h`（5か所）

(1) インクルードに1行足す。

```cpp
#include "MyEngine/Input/MouseInput.h"
#include "MyEngine/Input/KeyboardInput.h"
#include "MyEngine/Input/XInputGamepad.h"
#include "MyEngine/Math/Vector2.h"
```

(2) `GetMousePositionX/Y` の所を、次の形にする（説明を直し、関数を2つ足す）。

```cpp
	/// <summary>マウスX座標 (クライアント座標、ウィンドウ左端が 0)。エディタでは Game ビューの中の位置ではない（GetMousePositionInGame を使う）</summary>
	static long GetMousePositionX();

	/// <summary>マウスY座標 (クライアント座標、ウィンドウ上端が 0)。</summary>
	static long GetMousePositionY();

	/// <summary>
	/// マウスの位置を、ゲーム画面の座標で（スプライトの position と同じ：画面の真ん中が0、右と上が+、端が±1）
	/// <para>エディタでは Game ビューの絵の中、製品版ではウィンドウの中が画面。画面の外なら ±1 を超える</para>
	/// </summary>
	static Vector2 GetMousePositionInGame();

	/// <summary>マウスがゲーム画面の上にあるか（エディタでは Game ビューの絵の上）</summary>
	static bool IsMouseInGame();
```

(3) 「Xbox コントローラー」の見出しに1行足す。

```cpp
	//============================================================
	// Xbox コントローラー
	// キーボードと同じく、ウィンドウが手前に無いとき・ImGui に文字を打っているときは何も押していない扱い
	//============================================================
```

(4) `GetTriggerDeadzone()` の下から `#endif` までを、次の形にする（`GetMoveAxis` と `SetGameViewRect` を足す）。

```cpp
	static float GetTriggerDeadzone();

	//============================================================
	// まとめて読む
	//============================================================

	/// <summary>
	/// 移動の入力（WASD・矢印キー・左スティックをまとめた物）。x は右が+、y は上（奥）が+
	/// <para>-1〜1。斜め（キー2つ・キーとスティックを一緒）でも長さは1まで（斜めに速くならない）</para>
	/// </summary>
	static Vector2 GetMoveAxis();

	// 入力を検知するウィンドウをセット
	static void SetActiveWindow(HWND hwnd) { GetInstance()->activeHwnd_ = hwnd; }

#ifdef USE_IMGUI
	// マウスがViewportにあるか
	static bool IsMouseInViewport() { return GetInstance()->mouseInViewport_; }
	static void SetMouseInViewport(bool inViewport) { GetInstance()->mouseInViewport_ = inViewport; }
	// Viewportがフォーカスされてるか
	static void SetViewportFocused(bool focused) { GetInstance()->viewportFocused_ = focused; }
	static bool IsViewportFocused() { return GetInstance()->viewportFocused_; }
	// Game ビューの絵がウィンドウのどこにあるか（左上・大きさ。クライアント座標）。EditorViewport が毎フレーム渡す
	static void SetGameViewRect(const Vector2& topLeft, const Vector2& size) {
		GetInstance()->gameViewTopLeft_ = topLeft;
		GetInstance()->gameViewSize_ = size;
	}
#endif
```

(5) `private:` の中、`IsMouseCapturedByImGui` から `#endif` までを、次の形にする。

```cpp
	static bool IsMouseCapturedByImGui();
	static bool IsKeyboardCapturedByImGui();
	static bool IsTypingInImGui(); // ImGui の入力欄に文字を打っている最中か
	static bool IsActive() { return GetInstance()->activeHwnd_ != nullptr; }
	static bool IsGamepadBlocked() { return !IsActive() || IsTypingInImGui(); } // パッドを「何も押していない」扱いにするか
	// ゲーム画面がウィンドウのどこにあるか（左上・大きさ。クライアント座標）
	static void GetGameViewRect(Vector2& topLeft, Vector2& size);

	HWND activeHwnd_ = nullptr;
	HWND mainHwnd_ = nullptr; // Initialize で受け取ったウィンドウ（製品版のゲーム画面＝このウィンドウの中）

#ifdef USE_IMGUI
	bool mouseInViewport_ = false; // マウスがViewportにあるか
	bool viewportFocused_ = false; // Viewportがフォーカスされてるか
	Vector2 gameViewTopLeft_ = {0.0f, 0.0f}; // Game ビューの絵の左上（クライアント座標）
	Vector2 gameViewSize_ = {0.0f, 0.0f};    // Game ビューの絵の大きさ（0なら、まだ描いていない）
#endif
```

#### D-2. `MyEngine/Input/InputManager.cpp`（6か所）

(1) `IsKeyReleased` に1行足す。

```cpp
bool InputManager::IsKeyReleased(uint8_t key) {
	if (!IsActive()) return false;
	if (IsKeyboardCapturedByImGui()) return false;
	return GetInstance()->keyboard_.IsReleased(key);
}
```

(2) `IsMouseButtonReleased` に1行足し、その下に4つ足す（`GetMouseDeltaX` の上）。

```cpp
bool InputManager::IsMouseButtonReleased(int b) {
	if (!IsActive()) return false;
	if (IsMouseCapturedByImGui()) return false;
	return GetInstance()->mouse_.IsButtonReleased(b);
}
// 宣言だけあって中身が無かった（呼ぶとリンクエラー LNK2019 になっていた）
long InputManager::GetMousePositionX() { return GetInstance()->mouse_.GetPositionX(); }
long InputManager::GetMousePositionY() { return GetInstance()->mouse_.GetPositionY(); }

Vector2 InputManager::GetMousePositionInGame() {
	Vector2 topLeft;
	Vector2 size;
	GetGameViewRect(topLeft, size);
	if (size.x < 1.0f || size.y < 1.0f) {
		return {0.0f, 0.0f}; // ゲーム画面がまだ無い（最初のフレーム・Game ビューを閉じている）
	}
	// 画面の中の割合（左上が0、右下が1）
	const float u = (static_cast<float>(GetMousePositionX()) - topLeft.x) / size.x;
	const float v = (static_cast<float>(GetMousePositionY()) - topLeft.y) / size.y;
	// スプライトと同じ座標へ（真ん中が0、右と上が+、端が±1。ウィンドウは下が+なので上下を返す）
	return {u * 2.0f - 1.0f, 1.0f - v * 2.0f};
}

bool InputManager::IsMouseInGame() {
	Vector2 topLeft;
	Vector2 size;
	GetGameViewRect(topLeft, size);
	if (size.x < 1.0f || size.y < 1.0f) {
		return false;
	}
	const Vector2 position = GetMousePositionInGame();
	return position.x >= -1.0f && position.x <= 1.0f && position.y >= -1.0f && position.y <= 1.0f;
}
```

(3) 「Xbox コントローラー」の10行を差し替える（`IsGamepadConnected` はそのまま）。

```cpp
// ---- Xbox コントローラー ----
// キーボードと同じく、ウィンドウが手前に無いとき・ImGui に文字を打っているときは、何も押していない扱いにする
// （XInput はウィンドウに関係なく読めるので、見ないと、別のアプリを触っている間もゲームが動いてしまう）
bool InputManager::IsGamepadConnected(int i) { return GetInstance()->gamepad_.IsConnected(i); }
bool InputManager::IsButtonPressed(uint16_t btn, int i) { return !IsGamepadBlocked() && GetInstance()->gamepad_.IsButtonPressed(btn, i); }
bool InputManager::IsButtonTriggered(uint16_t btn, int i) { return !IsGamepadBlocked() && GetInstance()->gamepad_.IsButtonTriggered(btn, i); }
bool InputManager::IsButtonReleased(uint16_t btn, int i) { return !IsGamepadBlocked() && GetInstance()->gamepad_.IsButtonReleased(btn, i); }
float InputManager::GetLeftStickX(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetLeftStickX(i); }
float InputManager::GetLeftStickY(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetLeftStickY(i); }
float InputManager::GetRightStickX(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetRightStickX(i); }
float InputManager::GetRightStickY(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetRightStickY(i); }
float InputManager::GetLeftTrigger(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetLeftTrigger(i); }
float InputManager::GetRightTrigger(int i) { return IsGamepadBlocked() ? 0.0f : GetInstance()->gamepad_.GetRightTrigger(i); }
```

(4) 「デッドゾーン」の6行の下（`// 内部ヘルパー` の見出しの上）に足す。

```cpp
float InputManager::GetTriggerDeadzone() { return GetInstance()->gamepad_.GetTriggerDeadzone(); }

// ---- まとめて読む ----
Vector2 InputManager::GetMoveAxis() {
	// キーボード：押している向きを足す（反対の向きを同時に押すと、打ち消し合って0）
	Vector2 move = {0.0f, 0.0f};
	if (IsKeyPressed(DIK_D) || IsKeyPressed(DIK_RIGHT)) {
		move.x += 1.0f;
	}
	if (IsKeyPressed(DIK_A) || IsKeyPressed(DIK_LEFT)) {
		move.x -= 1.0f;
	}
	if (IsKeyPressed(DIK_W) || IsKeyPressed(DIK_UP)) {
		move.y += 1.0f;
	}
	if (IsKeyPressed(DIK_S) || IsKeyPressed(DIK_DOWN)) {
		move.y -= 1.0f;
	}
	// パッド：左スティック（1つ目のコントローラー。デッドゾーン処理済み）
	move.x += GetLeftStickX();
	move.y += GetLeftStickY();
	// 斜めでも速くならないように、長さを1までにする（キー2つだと長さが√2＝約1.41になる）
	const float length = Length(move);
	if (length > 1.0f) {
		move /= length;
	}
	return move;
}
```

(5) `initInternal` の頭に1行。

```cpp
void InputManager::initInternal(HINSTANCE hInstance, HWND hwnd) {
	mainHwnd_ = hwnd;
```

(6) ファイルの一番下（`IsKeyboardCapturedByImGui` の下）に2つ足す。

```cpp
bool InputManager::IsTypingInImGui() {
#ifdef USE_IMGUI
	return ImGui::GetIO().WantTextInput;
#else
	return false;
#endif
}

void InputManager::GetGameViewRect(Vector2& topLeft, Vector2& size) {
#ifdef USE_IMGUI
	// エディタ：Game ビューの絵（EditorViewport が毎フレーム教えてくれる）
	topLeft = GetInstance()->gameViewTopLeft_;
	size = GetInstance()->gameViewSize_;
#else
	// 製品版：ウィンドウの中全部（ゲームの絵はウィンドウいっぱいに描かれる）
	RECT client{};
	GetClientRect(GetInstance()->mainHwnd_, &client);
	topLeft = {0.0f, 0.0f};
	size = {static_cast<float>(client.right - client.left), static_cast<float>(client.bottom - client.top)};
#endif
}
```

#### D-3. `MyEngine/Editor/Viewport/EditorViewport.cpp`（1か所）

`Render` の `gameView_.Draw(gameRT);` の下に足す。

```cpp
	// --- Game ---
	gameView_.Draw(gameRT);
	// ゲームのマウス座標（InputManager::GetMousePositionInGame）は、この絵の中を画面として数える
	const ImVec2& gameMin = gameView_.GetImageMin();
	const ImVec2& gameSize = gameView_.GetImageSize();
	InputManager::SetGameViewRect({gameMin.x, gameMin.y}, {gameSize.x, gameSize.y});
```

#### D-4. ビルド

エンジンをビルド（Debug と Release）→ ゲームをビルド。

---

### E. ゲーム側

#### E-1. `CG3_Project/GameScene.h`（3か所）

(1) 前方宣言の3行を消す（もう使わない）。

```cpp
// 前方宣言
class Camera;
class DebugCamera;
```

(2) `GetCamera` の上書きを消して、コメントにする。

```cpp
	void Finalize() override;
	// GetCamera は上書きしない（Hierarchy に置いた Camera Component のカメラが使われる）
```

(3) メンバの `camera_` を消す。

```cpp
	// エンジン組み込み
	Particles* particles_;
```

#### E-2. `CG3_Project/GameScene.cpp`（6か所）

(1) インクルード：`Camera.h` を `CameraComponent.h` に替える。

```cpp
#include <MyEngine/Components/CameraComponent.h>
#include <MyEngine/Diagnostics/MyAssert.h>
```

(2) 無名 namespace に1行足す。

```cpp
namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
} // namespace
```

(3) `Finalize` から `camera_` を消す。

```cpp
	// Entityはエンジンが消すので、ここでは Initialize で作った物（Entityではない物）だけを片付ける
	delete stage_;
	delete particles_;

	stage_ = nullptr;
	particles_ = nullptr;
```

(4) `CreateDefaultEntities` の最後にカメラを足す（シーンファイルが無いときの最初の配置。**今はシーンファイルがあるので呼ばれない**。E-3 でエディタから置く）。

```cpp
	const Handle<Entity> ball = EntityManager::Create("MonsterBall");
	EntityManager::RequestAdd<ModelRendererComponent>(ball, monsterBall);

	// カメラ（前に Initialize で new していたカメラと同じ位置・視野角）
	CameraComponent camera;
	camera.fovY = 0.45f * kRadToDeg; // 約25.8度
	camera.farZ = 100.0f;
	const Handle<Entity> mainCamera = EntityManager::Create("Main Camera");
	EntityManager::Get<TransformComponent>(mainCamera)->translation = {0.0f, 0.0f, -30.0f};
	EntityManager::RequestAdd<CameraComponent>(mainCamera, camera);
}
```

(5) `Initialize` の頭からステージまでを、次の形にする（カメラを作らない・`ParticleManager::SetCamera` を消す・ステージには `GetCamera()`）。

```cpp
	// カメラは Entity（Main Camera の Camera Component）。GetCamera() がそのカメラを返す

	// 環境光（IBL）。PBRのモデルはこれが無いと描かれない
	environment_.MakeFromHDR(TextureManager::Load("resources/monsterBall/sky.hdr"));

	// パーティクル（カメラはエンジンが毎フレーム渡す）
	particles_ = new Particles();
	particles_->Initialize();
	// ステージ（GetCamera() はいつも同じカメラを返すので、覚えさせてよい）
	stage_ = new Stage();
	stage_->Initialize();
	stage_->SetCamera(GetCamera());
```

(6) `Draw` の `camera_` を `GetCamera()` に。

```cpp
	ModelRenderSystem::Draw({}, GetCamera(), &environment_, GetWindowTitle());
```

#### E-3. エディタで Main Camera を置く

`resources/scenes/GameScene.scene.json` がもうあるので、`CreateDefaultEntities` は呼ばれない。エディタで置いて Save する。

1. ゲームをビルドして起動（停止中）。
2. Hierarchy の **＋ → Camera**。「Main Camera」ができる。
3. 前と同じ絵にするなら：Inspector の Transform の Position を **(0, 0, -30)**、Camera の **Field Of View を 25.8**、**Far を 100**。
4. **Save**（Ctrl+S）。

#### E-4. 新規：例の Component 3つ（`CG3_Project/Source/Components/`）

`PlayerMove.cpp`（自機の移動。`GetMoveAxis` の使い方）：

```cpp
#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Input/InputManager.h>

// 自機の移動：WASD・矢印キー・左スティックで、地面（XZ）の上を動く

// ① データ
struct PlayerMove {
	float speed = 5.0f; // 1秒に進む距離
};

// ② Inspectorでの見せ方
COMPONENT(PlayerMove, "Movement") {
	ui.Field("Speed", value.speed, AtLeast(0.0f).Tip("1秒に進む距離"));
}

// ③ 毎フレームの処理
void PlayerMoveUpdate(PlayerMove& player, TransformComponent& transform, float deltaTime) {
	// 入力の上（y）を、奥（+Z）への移動にする
	const Vector2 move = InputManager::GetMoveAxis();
	transform.translation += Vector3{move.x, 0.0f, move.y} * (player.speed * deltaTime);
}
SYSTEM(PlayerMoveUpdate);
```

`CameraFollow.cpp`（追いかけるカメラ。Main Camera に付ける）：

```cpp
#include <cmath>

#include <MyEngine/ECS/GameComponent.h>

// カメラの追いかけ：Main Camera（Camera Component を付けた Entity）に付けて、Target に追いかける相手を入れる
// Main Camera は一番上（親なし）に置く（translation をワールドの位置として書くため）

// ① データ
struct CameraFollow {
	EntityRef target;                      // 追いかける相手（Hierarchy からドラッグ＆ドロップ）
	Vector3 offset = {0.0f, 5.0f, -15.0f}; // 相手から見て、どこに居るか
	float followSpeed = 5.0f;              // 追いつく速さ（大きいほどすぐ追いつく。0なら動かない）
	bool lookAtTarget = true;              // 相手のほうを向く
};

// ② Inspectorでの見せ方
COMPONENT(CameraFollow, "Camera") {
	ui.Field("Target", value.target);
	ui.Field("Offset", value.offset, Tip("相手から見て、どこに居るか"));
	ui.Field("Follow Speed", value.followSpeed, AtLeast(0.0f).Tip("大きいほどすぐ追いつく"));
	ui.Field("Look At Target", value.lookAtTarget);
}

// ③ 毎フレームの処理
void CameraFollowUpdate(CameraFollow& follow, TransformComponent& transform, float deltaTime) {
	const TransformComponent* target = EntityManager::Get<TransformComponent>(follow.target);
	if (target == nullptr) {
		return; // 相手が入っていない・消えた
	}
	const Vector3 targetPosition = GetWorldPosition(*target); // 1フレーム前の位置（worldMatrix は SYSTEM の後で計算される）

	// --- 位置：行きたい所へ、残りの距離の一定の割合ずつ近づく（フレームレートが変わっても同じ速さになる書き方）---
	const Vector3 goal = targetPosition + follow.offset;
	const float t = 1.0f - std::exp(-follow.followSpeed * deltaTime);
	transform.translation = Lerp(transform.translation, goal, t);

	// --- 向き：相手のほうを向く（回転は X→Y→Z の順なので、上下は X、左右は Y で回す）---
	if (follow.lookAtTarget) {
		const Vector3 direction = Normalize(targetPosition - transform.translation);
		if (Length(direction) > 0.0f) {
			transform.rotation = {std::asin(-direction.y), std::atan2(direction.x, direction.z), 0.0f};
		}
	}
}
SYSTEM(CameraFollowUpdate);
```

- `1.0f - std::exp(-速さ × deltaTime)` は「毎秒、残りの距離の決まった割合だけ近づく」の、フレームレートに左右されない書き方（`Lerp(今, 行き先, 0.1f)` のように決まった数を使うと、60fps と 144fps で追いつく速さが変わる）。
- 向きの式：回転は X→Y→Z の順なので、Z = 0 のとき「前（+Z）」の向きは `(cos(x)sin(y), -sin(x), cos(x)cos(y))`。これが相手への向き `direction` になる x・y を逆に解いたのが `asin(-direction.y)` と `atan2(direction.x, direction.z)`。

`MouseClickTest.cpp`（確認用。確かめたら消してよい）：

```cpp
// 確認用：Game ビューをクリックすると、マウスの位置（ゲーム画面の座標）を Log に出す（確かめたら消してよい）
#include <format>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Input/InputManager.h>

// 中身の無い、印だけのComponent
struct MouseClickTest {};
COMPONENT(MouseClickTest, "Test") {}

void MouseClickTestUpdate(MouseClickTest&) {
	if (InputManager::IsMouseButtonTriggered(0)) {
		const Vector2 position = InputManager::GetMousePositionInGame();
		LogManager::Log(std::format("Mouse: ({:.2f}, {:.2f}) 画面の中: {}", position.x, position.y, InputManager::IsMouseInGame() ? "はい" : "いいえ"));
	}
}
SYSTEM(MouseClickTestUpdate);
```

AudioKeyTest.cpp は確認が済んだので消してよい（残してもよい）。

#### E-5. プロジェクトに追加・ビルド

1. 3つのファイルをゲームのプロジェクト（CG3_Project）に追加する（VS で `Source/Components` を右クリック → 追加 → 新しい項目。AudioKeyTest.cpp と同じ所）。**足し忘れてもエラーにならず、Add Component に出ないだけ**（ビルドされないので登録もされない）。
2. ゲームをビルド（Debug と Release）。

---

## 7. 確認すること

**バグ2つ（パートA）**

1. Hierarchy で Entity を2つ作る（「Parent」「Child」）。Parent の Position を (5, 0, 0)、Rotation を (0, 45, 0)、Scale を (2, 2, 2) にする。Child の Position を (10, 0, 0)。
2. Child を Parent へドラッグ＆ドロップ → **Scene ビューで Child が動かない**。Inspector の Child の値が変わる（Position (1.768, 0, 1.768)、Rotation (0, -45, 0)、Scale (0.5, 0.5, 0.5)）。Transform の下の「World Position」は 10, 0, 0 のまま。
3. **Ctrl+Z** → 親が外れて、値も (10, 0, 0) に戻る。**Ctrl+Y** → また付く。何回行き来しても値がずれない。
4. Child を Hierarchy の何も無い所へドラッグ（親を外す）→ 動かない。
5. Parent の Scale を (1, 3, 1) にして、Child（Rotation を (0, 0, 30) にしておく）を付ける → 位置は動かないが、形が少し変わる（1章の「直しきれないこと」。Unity も同じ）。
6. Parameters ウィンドウの **PostEffect** タブ → Bloom → Threshold をつまんで動かす → **Ctrl+Z で戻る、Ctrl+Y でやり直す**。Edit メニューの Undo / Redo も同じ。
7. PostEffect の値を2つ（例：Threshold と Lens の Vignette）動かしてから Ctrl+Z を2回 → 新しい順に1つずつ戻る。
8. Threshold をつまんで、あちこち動かしてから離す → Ctrl+Z 1回で、つまむ前の値に戻る（途中の値は履歴に入らない）。
9. Entity を1つ作る → PostEffect を動かす → Ctrl+Z 2回 → PostEffect が戻り、次に Entity が消える（順番どおり）。
10. Debug タブ（DebugCamera の Translation）でも、Ctrl+Z で戻る（Scene ビューのカメラが戻る）。

**音（パートB）**

11. 起動すると、Parameters ウィンドウに **Audio** タブがあり、Volume の下に **Master・BGM・SE**（全部 1）。
12. BGM 用の Audio Source の **Bus を BGM** にして Play。Audio タブの **BGM を 0 に近づける → BGM だけ小さくなる**。SE を動かしても BGM は変わらない。Master は全部。Stop してから Audio タブの **Save** → 再起動しても値が残る（`Parameters/Audio/Volume.json`）。
13. Audio Source の Sound の下の **▶ Preview**（停止中）→ 鳴る。ボタンが **■ Stop Preview** になり、押すと止まる。鳴り終わると ▶ に戻る。Volume・Pitch・Bus を変えて ▶ → その値で鳴る。
14. Volume を 1.5 にして ▶ → 1 のときより大きい（前は 1 で止まっていた）。
15. 音を出す Entity に **Spatial** を付ける（Loop・Play On Start も）。Play → Scene ビューで、その Entity をゲームのカメラの右へ動かす → **右から聞こえる**。左へ → 左から。遠くへ（Max Distance より遠く）→ 聞こえなくなる。
16. Spatial を外す → 真ん中から、元の大きさで聞こえる。
17. 前に保存したシーン（Audio Source の入った物）が、そのまま読める（Bus は SE、Spatial は外れている）。Log に「読む所がありません」の警告が出ない。

**カメラ（パートC・E）**

18. E-3 の手順で Main Camera を置く → **Game ビューが Main Camera から撮った絵になる**（Position を動かすと、停止中でもその場で変わる）。Scene ビューに、映る範囲の線が出る。
19. Field Of View を小さくする → 望遠になる。
20. Main Camera を別の Entity の子にして、親を動かす → カメラも一緒に動く（子にしても、見えている位置は変わらない＝パートAの直し）。
21. Camera をもう1つ置いて、Priority を 1 にする → そちらから撮る。そのカメラの Entity の Active を外す → 元のカメラに戻る。
22. Main Camera を消す → Log に「Camera Component が無いので、既定のカメラ…」が出て、(0, 0, -10) から撮る。Ctrl+Z で戻す。
23. Save → 再起動 → カメラが同じ所にある。Play 中にカメラを動かして Stop → Play の前の位置に戻る。
24. **Release** でビルドして起動 → Main Camera から撮った絵。
25. 自機にする Entity（モデル付き）に **PlayerMove**（Add Component → Gameplay → Movement）を付ける。Main Camera に **CameraFollow**（Gameplay → Camera）を付け、Target に自機を Hierarchy からドラッグ＆ドロップ。Play → WASD・矢印キー・左スティックで自機が動き、カメラが後ろから追いかける。

**入力（パートD）**

26. どれかの Entity に **MouseClickTest**（Gameplay → Test）を付けて Play。Game ビューの真ん中をクリック → Log に `Mouse: (0.00, 0.00) 画面の中: はい` に近い値。左上の角 → (-1, 1) に近い。右下 → (1, -1) に近い。Game ビューの外（Hierarchy など）をクリック → 何も出ない（ImGui が使っている）。
27. Game ビューのウィンドウの大きさを変えても、角をクリックすると ±1 に近い（絵の中の位置で数えている）。
28. パッドをつないで Play。Hierarchy で名前を変えている（文字を打っている）間、スティックを倒しても自機が動かない。別のアプリをクリックして手前にしている間も動かない。

---

## 8. 検証したこと（2026-09-28、Claude）

- **コンパイル**：VS 18（v145）の cl で `/std:c++latest /permissive- /W4 /utf-8`（`UNICODE`）。
  - 全部（A〜D）：エンジン 85ファイル Debug（`USE_IMGUI`）/ Release **エラー0、警告は前と同じ**（Debug 4・Release 7。ファイルと警告番号が前と同じ物だけ）。
  - **途中の状態**：A だけ・B だけ・A+C・D だけ、でもエンジンがエラー0（パートを別々に写しても、途中でビルドが通る）。A+C のエンジンで、今のゲーム（GameScene を直す前）もエラー0。
  - ゲーム：全部＋E（`GameScene`・`Stage`・`Particles`・`main`・`Spin`・`AudioKeyTest`・`PlayerMove`・`CameraFollow`・`MouseClickTest`・この md と Reference.md の例を集めたファイル）Debug / Release **エラー0・警告0**。E を写す前の今のゲームも、新しいエンジンでエラー0。
- **動作テスト1（140項目 全通過）**：本物の `Matrix4x4`・`EntityManager`・`EditorHistory`・`GlobalVariables`・`CameraSystem`・`Camera`・`SceneSerializer` を使い、描画・音だけ差し替えた。
  - `DecomposeAffineMatrix`：ばらばらな大きさ・回転・位置 20000通りで、組み立て直した行列との差が最大 0.000005。Y が ±90度（真横）・裏返し（負の大きさ）・大きさ0 の軸でも、組み立て直すと同じ行列（大きさ0 でも NaN にならない）。
  - `SetParentKeepWorld`：回って大きさ2の親 → 大きさ0.5の子、の下へ付け替えても、ワールド行列の差 0.000001 以下。root へ戻すと元の値。輪になる付け替えは値も変えない。大きさ0の親は、付けるが値はそのまま。軸ごとに大きさの違う親：位置の差は 0（向き・大きさは近い値）。
  - **Hierarchy のドラッグ＆ドロップ（`RequestReparent`）**：見た目が変わらない。Undo / Redo を5往復しても、値がぴったり元どおり。同じ親・輪になる付け替えは履歴に積まない。
  - **Parameters の Undo / Redo：本物の ImGui を画面なしで動かし、マウスでつまんで動かした**（見出しをクリックして開き、float を横にドラッグ、bool をクリック、入れ子のカテゴリの項目も）。3件の変更が新しい順に1つずつ戻り、やり直せる。ドラッグの途中は積まれず、離したときに1件。動かして元の値に戻しただけなら積まれない。
  - カメラ：Camera が無いと (0, 0, -10)、シーンの `GetCamera()`（上書きしない）も同じカメラ。置くと位置・向き・視野角（度 → ラジアン）・Near・Far・縦横比が写り、ビュー行列がカメラの行列の逆と一致。大きさ3の親の下でも見えている位置から撮り、大きさは写さない。親を動かすとついてくる。Priority の大きい方、無効にすると次、親を無効にすると既定のカメラ。視野角0・Far < Near でも壊れない。保存 → 全部消して読み込み → もう一度保存で同じ文字列。
- **動作テスト2（81項目 全通過）**：本物の `SoundManager`（XAudio2 で、このPCのスピーカーに実際に鳴らした）・`AudioSystem`・`EntityManager`・`SceneSerializer`・`Camera`。
  - 種類ごとの通り道が2つでき、どちらも2ch・スピーカーと同じサンプリングレート（このPCは 8ch・48000Hz）。何も書かない `Play` は SE、`SoundBus::BGM`・`SoundConfig::bus` は BGM へ流れる（違う通り道へは流れていない）。
  - **何もしていない音の割り振りは、モノラルで左右とも1、ステレオで左→左・右→右が1**。真ん中の `SetPan` / `SetSpatial(1, 0)` がこれと同じ（＝3Dでない音と大きさが変わらない）。
  - `SetSpatial` の大きさと左右（モノラル・ステレオ）、種類ごと・全体の音量の書き込みと読み出し。
  - AudioSystem：Parameters の Audio / Volume が停止中でも伝わる、範囲の外（3）は 2 に収める。3D：右に10（Min 1・Max 21）で左0・右0.55、左に5で左0.8・右0、真後ろで両方0.9、Min より近いと1、Max より遠いと0（鳴り続けてはいる）、**カメラを90度回すと左右が入れ替わる**、カメラが動くと変わる、Spatial を外すと元どおり、聞く人がいないと普通の音、Max < Min と書いても 0 で割らない、鳴らし直した最初のフレームから距離の大きさ、`PlayOneShot` も鳴らした瞬間の位置。音量 1.5 がそのまま効き、5 は 2 に。3Dでない音は割り振りに触らない。
  - 試し聞き：Inspector の値（音量・Bus）で鳴る、停止中の `Update` では止まらない、2回押しても1つ、止める・`StopAll` で止まる、未選択は鳴らない、ループせずに鳴り終わる。
  - 保存：`"bus":"BGM"`・`"spatial":true`・`"minDistance":2.0` が書かれ、全部消して読むと元どおり。**新しい項目を消した（前の形の）ファイルを読むと、SE・3Dでない・1・20**。
- **確かめていないこと**：画面の見た目（Inspector の ▶ ボタン・Camera の区画・Parameters の Audio タブ）、本物のエディタでの操作（Hierarchy のドラッグ＆ドロップ・Ctrl+Z はテストの中で同じ関数を呼んで確かめた）、実際の聞こえ方（テストでは割り振りの数を見た）、パッド（つないでいないので、手前に無いとき・文字入力中に 0 になる所は読んで確かめただけ）、Game ビューでのマウス座標（計算は読んで確かめただけ）。

---

## 9. 決めたこと

| 決めたこと | 理由 |
|---|---|
| 親の付け替えは、見た目を変えない（`SetParentKeepWorld`）をエディタの標準にする | Unity と同じ。値を保つ付け替えは、見た目がいきなり飛ぶ |
| 前の `SetParent` は残す | シーンの読み込み・コピーは、ファイルの値をそのまま使いたい |
| 付け替えの Undo は、前と後の値を覚えて戻す（計算し直さない） | 計算し直すと、行き来するたびに小さな誤差がたまる |
| ゆがみ（せん断）は近い値にする（表せるようにはしない） | Transform を行列にしないと表せない。Unity も同じ |
| Parameters の Undo は、Inspector と同じ「描く前後を比べる → 離したら1件」 | 項目ごとに書き足さなくてよい。つまんでいる途中を積まない |
| 履歴は1本（Entity の操作と Parameters を同じ列に並べる） | Ctrl+Z がやった順に戻る。別々だと、どちらが戻るか分からない |
| Parameters の項目は名前で探して戻す | 項目は `std::vector` の中で、足されると場所が変わる |
| 種類は BGM と SE の2つ（＋全体） | 設定画面でよくある3つ。足すときは `SoundBus` に1行足すだけ（2026-09-29 から `kBusCount` は `magic_enum::enum_count` が数える。数える用の `Count` は足さない＝11章） |
| 種類ごとの音量は Parameters（GlobalVariables）に置く | 保存・読み込み・ウィンドウ・Undo がそのまま使える。ゲームの設定画面からも同じ値を書き換える |
| 何も書かない `SoundManager::Play` は SE | 数が多いのは効果音。BGM を直接鳴らすときだけ `SoundBus::BGM` を書く |
| 通り道は左右2ch | スピーカーが何chでも、左右の振り分けを2つに書けばよい |
| 3Dの大きさは、距離でまっすぐ下げる（Min〜Max） | 分かりやすい（Unity の Linear と同じ）。耳の聞こえ方に近い「距離の逆数」は後で選べるようにしてもよい |
| 3Dの大きさ・左右は、音量ではなく出力の割り振りに掛ける | Inspector の Volume・フェードと掛け合わせても、どちらも壊れない |
| パンの真ん中は左右とも元の大きさ | 3Dでない音と大きさをそろえる（前は真ん中で半分になっていた） |
| 試し聞きは同時に1つ・ループしない・3Dにしない | 止め忘れで鳴り続けないように。聞きたいのは音そのもの |
| ボタンは `ComponentUI::Button` にした（Audio Source 専用の Editor は作らない） | Describe 1つで Inspector と保存を兼ねる書き方を崩さない。ゲームの Component でも使える |
| Volume の上限を 2 に | スライダーは 2 まで動くのに、1 で止めていた |
| カメラは Component（Transform＋写り方）にして、Camera クラスは CameraSystem が1つだけ持つ | 描画・パーティクルなどは今までどおり `Camera*` を受け取れる。Component にポインタや仮想関数を入れない（ARCHITECTURE.md 原則2） |
| `IScene::GetCamera()` の既定を CameraSystem のカメラに | シーンが何もしなくても Camera Component が使われる。上書きしている前のシーンは、そのまま動く（切り替えを1つずつできる） |
| 使うカメラは Priority の大きい物 | 切り替え（イベントシーン・死んだ後のカメラ）を、値1つか有効・無効で書ける |
| Camera が無いときは既定のカメラ（落とさない） | 置き忘れ・消してしまったときも、エディタが動く。Log に1回出す |
| 視野角は度で持つ | Inspector で読みやすい。スポットライトの角度と同じ |
| 縦横比は 16:9 に固定 | Game ビューの描画先とスプライトの基準が 1280×720 |
| `CameraSystem::Update` は `UpdateTransforms` の直後 | 親の下のカメラも、そのフレームの位置で撮る。パーティクル・音（聞く人）より前 |
| マウスの位置は、スプライトと同じ座標で返す | スプライトのボタンと、そのまま比べられる。エディタと製品版で同じ値になる |
| パッドも、ウィンドウが手前に無いとき・文字入力中は止める | キーボードと同じにそろえる（Serialize.md で「パッドを使うときに足す」と決めていた物） |
| `GetMoveAxis` は WASD・矢印・左スティックに決め打ち | 小さなゲーム（S5）で、毎回同じ4行を書かなくて済む。割り当てを変えたくなったら、名前で読む仕組みを作る |
| PCH は今はやらない | ゲーム本編に関係なく、今の規模では待ち時間の差が小さい。エンジンのヘッダを直すことが多い間は、かえって遅くなることがある |

---

## 10. 未解決・後回し

| 何を | なぜ後か / どうするか |
|---|---|
| PCH | 0章。ゲームの Component が増えて待ちが気になったら、ゲーム側だけに：`pch.h`（頭に `#define NOMINMAX`、中は `<MyEngine/ECS/GameComponent.h>` など重いヘッダ）と `pch.cpp` を作り、プロジェクトのプロパティで「プリコンパイル済みヘッダーを使用する（/Yu）」、`pch.cpp` だけ「作成する（/Yc）」、C/C++ → 詳細設定 → 「必ず使用されるインクルード ファイル」に `pch.h`。Debug / Release の両方 |
| 3Dの音の前後・ドップラー | XAudio2 の X3DAudio を使えば出せる（左右の割り振りを X3DAudio が計算する）。Unity の標準の3D音も前後は同じ聞こえ方なので、困ったら |
| 3Dの音の大きさの下がり方を選ぶ | 今はまっすぐ（Linear）だけ。距離の逆数（Unity の Logarithmic）を `enum` で選べるようにする |
| Bus を鳴っている途中で変えても、次に鳴らすまで効かない | XAudio2 は `SetOutputVoices` で送り先を変えられる。困ったら |
| ゲームの設定画面の値を、プレイヤーごとに保存する | 今は Parameters（開発用）に保存される。製品では、セーブデータとして別に持つ |
| キーの割り当て（名前で読む入力） | `InputManager::IsActionPressed("Jump")` のように、名前 → キー・ボタンの表を持つ。キーコンフィグを作るとき |
| Scene ビューのクリックも、Play 中のゲームに届く | マウスが Game ビューか Scene ビューの上なら、ゲームに通している（Scene ビューのカメラ操作と同じ入口なので）。ゲームのクリックは `IsMouseInGame()` と一緒に見る |
| 映っていないカメラの範囲の線 | 今は使っているカメラだけ Scene ビューに線が出る。選んだ Camera の線も出すなら、Collider と同じギズモにする |
| 画面分割（カメラ2つを同時に） | 描くカメラは1つ。要るときに、ウィンドウ（または Game ビュー）ごとにカメラを持たせる |
| Paste As Child は、貼った値を新しい親から見た値として使う | 見た目の位置が変わる（ドラッグ＆ドロップとは違う）。困ったら、貼った後に `SetParentKeepWorld` と同じ直しを入れる |
| 軸ごとに大きさの違う親の下で、回った子の形が少し変わる | 1章。Transform を行列で持てば直るが、Inspector で触れなくなる。Unity も同じ |
| `MyEngine/Light/LightIncludes.h` が残っている | Audio.md のパートAで消すはずだった物（どこからも読まれていない）。プロジェクトから外して、ファイルを消してよい |

---

## 11. 起動できないバグと写し違い（パートF、2026-09-29）

### 何が起きていたか

- 起動すると、最初のフレームで `Debug Assertion Failed!`（`<array>` の 525行、`array subscript out of range`）。
- **原因は `SoundBus` の最後に足した `Count`**（「板書から変えた所」として教えてもらった所）。
  - このエンジンは **magic_enum** で enum を「名前の一覧」として使っている（`AudioSystem` の `for (SoundBus bus : magic_enum::enum_values<SoundBus>())`、Inspector の Bus のコンボ、シーンファイルへの保存）。magic_enum は **enum に書いてある名前を全部拾う**ので、`Count` も BGM・SE と並ぶ「3つ目の音の種類」になる。
  - その結果、`AudioSystem::Initialize` が Parameters に「Count」の音量を足し、毎フレームの `ApplyVolumes` が `SoundManager::SetBusVolume(SoundBus::Count, 1)` を呼ぶ → 通り道 `busVoices_` は2つしか無いのに3つ目（`[2]`）を読む → Debug の `std::array` がそれを見つけて止めた（Release だと止まらずに隣のメモリを読むので、どこで落ちるか分からない）。
  - デバッガで確かめた、止まった所までの呼ばれた順：`WinMain` → `WindowManager::UpdateAll` → `AudioSystem::Update` → `ApplyVolumes` → `SoundManager::SetBusVolume` → `std::array<IXAudio2SubmixVoice*,2>::operator[]`。
  - `Count` を残したままだと、ほかにも：Inspector の Audio Source の **Bus のコンボに「Count」が出て**、選ぶと鳴らした瞬間に同じ所で止まる。保存すると `"bus": "Count"` と書かれる。
- **md の B-3 (5) の `static_assert(magic_enum::enum_count<SoundBus>() == kBusCount, ...)` は、まさにこれをビルドのときに止めるための行だった**（`Count` を足すと 3 と 2 で合わず、ビルドが止まる）。この行が無かったので、実行するまで気づけなかった。
- 「2 と手で書かずに、数を自動で追いかけたい」という狙いは正しい。**`Count` を使わず、`magic_enum::enum_count<SoundBus>()` に数えさせる**（F-2）。これなら種類を足すときに直すのは enum だけで、`static_assert` も要らなくなる（同じ物同士を比べるだけになるので）。

> **決まりにする：magic_enum で回す enum（Inspector のコンボ・保存・`enum_values` の for に使う enum）には、数える用の `Count` を足さない。数は `magic_enum::enum_count<E>()` で取る。** 今エンジンにある `Count` は `SoundBus` だけだった（ほかの enum も調べた）。Reference.md の18章にも足した。

### 照合で見つけた写し違い（2つ）

md のコードの行が今のソースに全部あるかを、機械で照らし合わせた。空白・コメントの違い、`particleCamera` → `gameCamera` のような名前の変更、Volume の上限を 3 にしたのは問題なし。直す必要があるのは次の2つ。

| 場所 | 何が違うか | 何が起きるか |
|---|---|---|
| A-8 (3) `GlobalVariables.cpp` の `DrawGroup` | `DrawNode(*categoryNode, headerPath, entry.name)`（md は `groupName`） | Parameters の値を動かして **Ctrl+Z を押すと、Log に「Undo/Redoの対象が見つからなかったので、履歴を消しました」と出て、履歴が全部消える**。2つ目の引数は、Undo のときに項目を探し直すための「グループ名」。そこへ `"Audio/Volume"` のような「グループ/カテゴリ」が渡るので、その名前のグループが見つからない |
| D-3 `EditorViewport.cpp` | 丸ごと抜けている | `GetMousePositionInGame()` がいつも (0, 0)、`IsMouseInGame()` がいつも false（Game ビューの場所が InputManager に伝わらない） |

ほかに気づいたこと（直さなくても動く）：

- **Main Camera がシーンに無い**：`CreateDefaultEntities` に Main Camera を書いてくれたが、`resources/scenes/GameScene.scene.json` がもうあるので、この関数は呼ばれない（Log に「CreateDefaultEntities() は呼んでいません」と出ている）。今の Game ビューは既定のカメラ (0, 0, -10) から撮っている。→ **E-3 のとおり、エディタで ＋ → Camera を置き、Position (0, 0, -30)・Field Of View 25.8・Far 100 にして Save**。`CreateDefaultEntities` に書いた分はそのままでよい（シーンファイルを消したときの最初の配置になる）。
- **E-4（例の Component 3つ）はまだ**：任意。`MouseClickTest` は F-4 の確認（7章 26・27）に使うので、作っておくと楽。

### F-1. `MyEngine/Sound/SoundBus.h`（全体を差し替え）

`Count` を消す（コメントの「ゲーム空は」も「ゲームからは」に直した）。

```cpp
#pragma once
#include <cstdint>

// 音の種類。鳴らす音は必ずどちらかを通り、種類ごとに音量を決められる
// （Parameters ウィンドウの Audio / Volume、ゲームからは AudioSystem::SetBusVolume）
// SoundManager.h は XAudio2 を読むので重い。Component からはこのヘッダだけを読む
// 種類を足すときは、ここに1行足すだけ（数は magic_enum が数える。数える用の Count は足さない）

/// <summary>
/// 音の種類（音量のつまみ）
/// </summary>
enum class SoundBus : uint8_t {
	BGM, // 音楽
	SE,  // 効果音
};
```

### F-2. `MyEngine/Sound/SoundManager.h`（2か所）

(1) 頭の include に magic_enum を足す（「外部ライブラリ（.h あり）」の段）。

```cpp
#pragma once
#include <array>
#include <string>
#include <unordered_map>

#include <wrl.h>
#include <xaudio2.h>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Math/Easing.h"
#include "MyEngine/Sound/SoundBus.h"
```

(2) `kBusCount` を magic_enum に数えさせる（private の、`masterVoice_` の下）。

```cpp
	// 音の種類ごとの通り道（SubmixVoice）。鳴らす音 → ここ → masterVoice_ の順に流れ、ここの音量が種類ごとの音量になる
	static constexpr size_t kBusCount = magic_enum::enum_count<SoundBus>(); // SoundBus の数（magic_enum が enum の名前を数える。種類を足しても直さなくてよい）
	std::array<IXAudio2SubmixVoice*, kBusCount> busVoices_ = {};
```

- `SoundManager.cpp` の `static_assert`（B-3 (5)）は、もう要らない（写していないので、そのままでよい）。
- `AudioSystem.cpp` の `kBusCount` は前から `magic_enum::enum_count<SoundBus>()` なので直さなくてよい（`Count` を消すと 3 → 2 に戻る）。
- ヘッダで magic_enum を読んでよいか：`ComponentUI.h` もヘッダで読んでいる。`SoundManager.h` を読むのはエンジンの cpp 5つだけ（ゲームからは読まない）。

### F-3. `MyEngine/UI/GlobalVariables.cpp`（1行）

`DrawGroup` の中。`DrawNode` の2つ目の引数を `headerPath` から `groupName` に。

```cpp
		if (opened) {
			ImGui::Indent();
			DrawNode(*categoryNode, groupName, entry.name);
			ImGui::Unindent();
		}
```

`headerPath` は、見出しと Save ボタンの ImGui の ID にだけ使う（そちらはそのまま）。

### F-4. `MyEngine/Editor/Viewport/EditorViewport.cpp`（D-3 と同じ）

`Render` の `gameView_.Draw(gameRT);` の下に足す（`InputManager.h` はもう include してある）。

```cpp
	// --- Game ---
	gameView_.Draw(gameRT);
	// ゲームのマウス座標（InputManager::GetMousePositionInGame）は、この絵の中を画面として数える
	const ImVec2& gameMin = gameView_.GetImageMin();
	const ImVec2& gameSize = gameView_.GetImageSize();
	InputManager::SetGameViewRect({gameMin.x, gameMin.y}, {gameSize.x, gameSize.y});
```

### F-5. ビルドと確認

1. エンジンをビルド（Debug と Release）→ ゲームをビルド（Debug と Release）。
2. 起動して止まらない。Parameters ウィンドウの Audio タブ → Volume が **Master・BGM・SE の3つ**（Count が無い）。Audio Source の **Bus のコンボが BGM・SE の2つ**。
3. E-3 の Main Camera を置いて Save。
4. 7章の 6〜10（Parameters の Undo。Audio タブの BGM を動かして Ctrl+Z でも）、11〜17（音）、18〜24（カメラ）、26〜27（マウス。`MouseClickTest` を作ったら）。

### 検証したこと（2026-09-29、Claude）

- 本体（`feature/game-basics` の 83fb0bb）とゲーム（CG3_Project の今の作業中の状態）を丸ごと scratchpad に写し、**VS と同じ MSBuild で exe まで作って実行した**（本体・ゲームのフォルダには何も書いていない）。
- **直す前**：起動して5秒で同じダイアログ。デバッガ（cdb）で止まった所までの呼ばれた順を取った（上の「何が起きていたか」）。
- **F-1〜F-4 を当てた後**：エンジン＋ゲームの Debug・Release ともエラー0（警告は前からある `DIRECTINPUT_VERSION` の再定義2つだけ）。**Debug を30秒・Release を15秒動かして止まらない**。エディタの画面（Hierarchy・Control・Scene ビュー・Profiler）が出ることを画像で確かめた。
- **確かめていないこと**：Play を押した後（外からクリックを送れなかった）、Parameters の Ctrl+Z とマウス座標の操作（F-3・F-4 は、9/28 に動作テストで確かめた md のコードと同じ形に戻しただけ）。

---

## 12. 質問への答え（2回目、2026-09-29）

### 1. GlobalVariables はもう要らない？ → 要る。役割が「Entity に属さない値」になった

コンポーネント化で **「物ごとの値」は Component へ移った**（前は MonsterBall の位置を GlobalVariables が毎フレーム上書きしていた）。残るのは「ゲーム全体で1つの値」と「エンジン・エディタの設定」で、これを Component にすると置き場所に困る（どの Entity に付ける？その Entity を消したら？）。今の使われ方：

| 使っている所 | 何を置いているか | これからも GlobalVariables？ |
|---|---|---|
| `PostProcess`（エンジン） | ブルーム・レンズの設定 | ◯（シーンごとに変えたくなったら Component にする。Unity の Volume） |
| `EditorViewport`（エンジン） | Scene ビューのカメラの位置・視野角 | ◯（エディタの設定） |
| `AudioSystem`（エンジン） | 音量 Master・BGM・SE | ◯（ゲーム全体で1つ） |
| `Particles.cpp`（ゲーム） | エミッタの発生位置・ブレンド | ✕ → **Component に**（位置は Transform で持つ。前の MonsterBall と同じ形。パーティクルを Component にするとき） |
| `GameScene` の `RegisterGV` / `ApplyGV`（ゲーム） | 中身が空 | 消してよい |
| `GameScene.h` の `lightDirection_`・`lightIntensity_`・`kPointLightNum`（ゲーム） | どこからも使われていない（ライトは Component になった） | 消してよい |

見分け方（Reference.md の6章・16章と同じ）：**同じ物が何個もある・場所がある・シーンに置く物 → Component。ゲームに1つだけ・シーンをまたぐ・エディタの設定 → GlobalVariables（Parameters ウィンドウ）。**

`GlobalVariables.h/.cpp` を消すと、Parameters ウィンドウ・その保存（`Parameters/*.json`）・Undo も無くなるので、消さない。名前だけ言えば、クラスは `GlobalVariables`、ウィンドウとフォルダは `Parameters` で呼び方がずれている（揃えるなら `Parameters` に寄せる。急がない）。

### 2. `EntityManager::CalcWorldMatrix` は正しい名前か → 正しい（そのままでよい）

- ソースは `CalcWorldMatrix` で合っている（質問の「Clac」は打ち間違い）。
- このエンジンでは **Calc＝「その場で計算して返す」**、**Get・メンバ＝「覚えてある値を返す」**で使い分けている（`Camera::CalcWVP`・`EntityManager::CalcDepth`・`Skybox::CalcCubeSize`・`Renderer::CalcQuadNormal`）。
  - `TransformComponent::worldMatrix` … `UpdateTransforms` がフレームに1回計算して覚えた値。軽いが、同じフレームの中で値を変えた後は古い。
  - `CalcWorldMatrix(handle)` … 呼ぶたびに親をたどって計算する。少し重いが、いつも今の値。
- 「毎回計算する」ことが名前から分かるので、`GetWorldMatrix` にしない方がよい（Get だと軽いと思って、毎フレーム何百回も呼んでしまう）。Unity の `transform.localToWorldMatrix` にあたる。

### 3. Audio と Sound が混ざっているのはどうか → 決まりがあれば問題ない。1つだけ外れている

混ざっていること自体は珍しくない（Unreal も、素材は SoundWave・SoundCue、鳴らす側は AudioComponent）。大事なのは使い分けの決まりで、今の名前はほぼ次の決まりに沿っている：

**Sound＝音の素材（ファイル・波形。`ModelManager`・`TextureManager` と同じく「素材の名前＋Manager」）、Audio＝鳴らす仕組み（Component・System・Inspector のカテゴリ）**

| 名前 | 何か | 決まりに沿っているか |
|---|---|---|
| `SoundManager`・`Sound/` フォルダ | 音の素材を読んで持つ（＋XAudio2 で鳴らす） | ◯（`ModelManager`・`TextureManager` と同じ形） |
| `AssetType::Sound` | 素材の種類 | ◯ |
| `AudioSourceComponent`・`AudioSystem` | 鳴らす Component と、それを集めて動かす System | ◯（Unity も AudioSource） |
| Inspector のカテゴリ `Audio`・Parameters の `Audio` タブ | 鳴らす仕組みの設定 | ◯ |
| **`SoundBus`** | **音量つまみの通り道（鳴らす仕組み）** | **✕ → `AudioBus` がよい** |

- おすすめ：**`SoundBus` → `AudioBus`（ファイルも `AudioBus.h`）に変えて、この決まりを Reference.md に書く**。シーンファイルに保存されるのは値の名前（`"BGM"`・`"SE"`）で、型の名前ではないので、変えても前のファイルはそのまま読める。直す所は型名の置き換えだけ（エンジンの数ファイル）。
- 急がない。次の md（[SceneFlow.md](SceneFlow.md) の SF0）で「小さな整理」としてまとめて出す。

### 4. SoundManager をリファクタリングした方がいいか（〜Internal は要るか）→ Internal は要らない。直す価値はあるが急がない

**〜Internal がある理由**：static 関数には `this` が無いので、メンバ変数に触るには毎回 `instance_->` と書く必要がある。それを避けるために「static の入口（`Play`）→ ふつうのメンバ関数（`PlayInternal`）」の2段にしている。エンジンでは `DirectXCommon`・`RenderContext`・`SceneRenderer`・`InputManager` も同じ形。

**この形の困る所**：

- 関数が2倍になる（SoundManager は入口と Internal の組が13）。
- private の中身（XAudio2 の型）がヘッダに全部出るので、`SoundManager.h` を読むと `xaudio2.h` も読む。**Component 用に `SoundBus.h` を別に分けたのは、このせい**。

**新しい形はもうエンジンにある**：`AudioSystem`・`CameraSystem`・`CollisionSystem` は、状態を cpp の中の名前の無い namespace に `struct` で置き、ヘッダには static 関数だけを書いている（Internal も `instance_` も無い）。SoundManager をこの形にするなら、一緒に直したい物：

| 直すこと | よくなること |
|---|---|
| Internal をやめて、状態を cpp の中の `struct` へ | 関数が半分。ヘッダから `xaudio2.h`・`wrl.h` が消える（軽くなる） |
| 波形の `new BYTE[]` / `delete[]` → `std::vector<BYTE>` | 解放し忘れが起きない（CLAUDE.md の決まり） |
| `instance_` の `new` / `delete` → `std::unique_ptr` | 同上 |
| 使われていない `SoundConfig` 版の関数（8個。エンジンもゲームも呼んでいない）を消す | ヘッダが短くなる |
| WAV を読むための構造体（`ChunkHeader` など）を cpp へ | 使う人に見せなくてよい物を隠す |
| （任意）素材ハンドルと再生ハンドルを別の型に | 今はどちらも `uint32_t` なので、`Stop(soundHandle)` と取り違えてもビルドが通る |

- **急がない理由**：今バグは無く、次の作業（シーンの切り替え・アニメーション）では SoundManager を触らない。やるときは1つの独立した作業にする（呼ぶ側は `SoundManager::Play(...)` のままなので、ゲームは直さなくてよい）。
- 他の4つ（`DirectXCommon` など）も同じ形なので、**「大きく触るときに新しい形へ」**を決まりにする（全部を一度に変えない）。

### 5. `GetMousePositionX/Y`（クライアント座標）と `GetMousePositionInGame`（そうではない）→ 名前で区別はできているが、揃える方がよい

- 今の動きは正しい（`InGame` が付いているので取り違えにくい）。
- ただ、`GetMousePositionX/Y` は **エディタでは Hierarchy などのパネルの分だけずれる**（ウィンドウ全体での位置なので）。製品版ではたまたまゲーム画面と同じになるので、**ゲームが使うと「エディタでだけずれる」バグになる**（Reference.md 18章の最後の行）。今この2つを使っているのは `InputManager` の中だけ。
- おすすめ：`GetMousePositionX()`・`GetMousePositionY()` の2つを **`GetMousePositionInWindow()`（`Vector2`、ピクセル）1つにまとめる**。`InWindow` と `InGame` の対になり、戻り値も両方 `Vector2` に揃う。「ゲームは `InGame` を使う。`InWindow` はエディタ・エンジン用」とコメントに書く。
- `GetMouseDeltaX/Y`（動いた量）は、どこで測っても同じなのでそのままでよい。
- これも SceneFlow.md の SF0 にまとめて出す。

### 6. ポストエフェクトを Game・Scene の両方に出した方がいいか → 出してよい。ただし Lens は Scene では外す

- 今は `EditorViewport::Render` で、Scene ビューだけ `RenderToTexture(..., false)`（効果なし）にしている。
- **仕組みはもう対応している**：ブルームとレンズの定数バッファは「1フレームに何回でも」使える枠になっていて（ブルーム32回分・レンズ8回分）、1つのビューで使うのはブルーム8回・レンズ1回。2つのビューで使っても足りる。重くなるのはエディタだけ（製品版は Game の絵しか描かない）。
- Unity も、Scene ビューの上のボタンで効果の表示を切り替えられる（既定は表示）。
- **Lens（歪み・色収差・周辺を暗く）は Scene ビューでは外すのがおすすめ**：絵を歪ませると、その上に ImGui で描くギズモ（ライトのアイコンなど）と、物の見えている位置がずれる。クリックで選ぶ位置もずれる。
- やること（小さい）：Scene ビューに「Effects」のチェック（既定 ON）を置いて `RenderToTexture` に渡す。`PostProcess::Composite` に「レンズを掛けるか」を足し（レンズは切っていてもコピーとして毎回通るので、`kLensEnabled` と一緒に見るだけ）、Scene ビューでは false。
- これも SceneFlow.md の SF0 にまとめて出す。
