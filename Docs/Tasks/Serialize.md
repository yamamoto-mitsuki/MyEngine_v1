# Serialize：シーンの保存・読み込みと Play / Stop

## このファイルについて

- シーンの保存・読み込み（シリアライズ）と、それに合わせて作り直した Play / Stop・シーン切り替えの作業を書く。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。「なぜ」は「決めたこと」の表へ移す。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階。
- ブランチは `feature/scene-serialize`（`develop` の 71cb636 から、2026-09-21 に作成）。

| 読む順番 | 何が書いてあるか |
|---|---|
| [1. 次に何をやるか](#1-次に何をやるか2026-09-21-の判断) | シリアライズを先にした理由と、この後の順番 |
| [2. S1で何が変わるか](#2-s1-で何が変わるか) | 操作の前後の違い・ファイルの形・仕組み |
| [3. S1の手順](#3-s1-の手順パート-ag) | 写経するコード（パートA〜G） |
| [4. 確認すること](#4-確認すること) | 写したあとに実行して見ること |
| [6. 決めたこと](#6-決めたこと) | 理由の一覧 |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| **S1** | **シリアライズ（保存・読み込み・Play/Stopの復元・シーン切り替え）** | **写経待ち（2026-09-21 に書き、2026-09-24 にレビューして直した）** |
| S2 | ColliderComponent ＋ CollisionSystem | これから |
| S3 | AudioSourceComponent ＋ System | これから |
| S4 | PCH（ゲームのComponentのビルドを速く） | これから |
| S5 | 小さなゲームを1本通す | これから |

---

## 1. 次に何をやるか（2026-09-21 の判断）

### 結論

**シリアライズ（S1）を先にやる。** Collision・Audio はその後。**Input は Component にしない。**

決め方：Claude が「シリアライズ先行」「ゲーム用Component先行」「どちらにも寄らない技術リード」の3つの立場で、それぞれコードの事実（ファイル:行）を根拠に主張させ、別の審査役がコードを読み直して判定した。3つとも「Play/Stop で編集が消える問題を先に直す」点は一致した。

### 決め手（重い順）

1. **今は Stop だけでなく、Play を押した瞬間にも編集が消える。**
   `SceneManager::Play()` は停止中なら `RequestReload()` を呼び、`ReloadImmediate()` が「Finalize → 新しいシーン → Initialize」でシーンをコードから作り直している。Inspector で Collider の大きさを合わせても、Play を押すと消える。**Collider を先に作ると、Inspector で調整するという一番の使い道が成り立たない**（結局 `GameScene::Initialize` にコードで書くことになり、後でファイルへ移す二度手間になる）。
2. **Play / Stop の戻り方が、Entity の置き場所で違う。**
   `sceneRoot_` の下は作り直されるが、Hierarchy の一番上に作った物は `Finalize` で消されないので、Play 中に動いた結果のまま残る。この上に動く Component（Collider・Audio）を足すと、デバッグのたびに状態がずれる。
3. **アセットの番号はファイルに書けない。**
   `modelHandle` は読み込んだ順の番号で、次に起動したときは別のモデルを指す。音（AudioSource）も同じ形になる。「Component は番号を持ち、保存するときだけパスに直す」を先に決めておけば、Audio はそれをなぞるだけで済む。
4. **Release でシーンを読むには「型の名前 → 読み書き」の表が Runtime 側に要る。**
   今エンジンの型の名前（"Transform" など）を持っているのは Editor だけ（`InspectorWindow::Initialize`。Debug でしか呼ばれない）。Component を増やす前に作っておく。
5. **挙げた「Scene」は、シーンファイルそのもの。**
   シーンの切り替え自体は `IScene::NextScene` で既にできる。足りないのは「シーンがデータであること」＝シリアライズ。
6. **今が一番安い。** ゲームのシーンは Entity 3つ・ゲームの Component 1つ。Component やコードで組んだ配置が増えるほど、後でファイルへ移す量が増える。

### 負けた側（ゲーム用Component先行）の一番強い論点と、その答え

- **論点：** 当たり判定はゲーム側では正しく書けない。SYSTEM とシーンの Update は `UpdateTransforms` より前に走るので、`worldMatrix` が1フレーム古い（子の `translation` は親から見た値）。エンジンに「判定する場所」が要るのに、使わないと見つからない。
- **答え：** その場所は S2 で作る（`UpdateTransforms` の直後に CollisionSystem）。S1 は写経1〜2回分なので、遅れるのは1週間ほど。逆に Collider を先に作ると、上の決め手1のとおり調整した値が Play で消える。

### Input は Component にしない

- Entity ごとに持つデータが無い（ForEach で回す物が無い）。SYSTEM の中から `InputManager` の static 関数を呼べば足りる。Unity でも入力は大抵全体で1つ。
- 直す価値のある穴は1つ：パッド系の関数は、キーボードと違って「ImGuiに文字を打っている間は無視する」確認をしていない。パッドを使うとき（S5）に数行で足す。
- `"Jump"` のような名前で入力を取る仕組みは全体の設定なので、必要になってから。

### この後の順番

| 順 | 作業 | ひとこと |
|---|---|---|
| **S1** | **シリアライズ（このファイル）** | 保存・読み込み・Play/Stop の復元・シーン切り替え |
| S2 | ColliderComponent ＋ CollisionSystem | 球 / AABB、中心のずれ・大きさ・isTrigger。`UpdateTransforms` の直後に判定して「当たったペアの一覧」に書き、ゲームの SYSTEM が読む。Scene ビューに緑の線。押し戻しはしない |
| S3 | AudioSourceComponent ＋ System | 音はパスで保存（S1 の AssetField に Sound を足す）。`SoundManager` に番号→パスと StopAll を足し、Stop・Pause・Entity の破棄で音も止める（今は Pause 中も鳴り、Stop/Play で BGM が重なる） |
| S4 | PCH | ゲームの Component を量産する直前に。1ファイル 2.5秒 → 0.2秒の見込み（Entity.md の実測） |
| S5 | 小さなゲームを1本通す | 自機・弾・敵、当たったら消えて音、BGM、NextScene でリザルト。配置はエディタで行い、シーンファイルに保存 |
| 次 | プレハブ・シーンを開く | プレハブ＝「Entity の部分木の保存」。シーンを開くには「名前 → シーンの作り方」の表が要る |
| 次 | クオータニオン | 必要になってから。ファイルの版を2に上げ、古い版のオイラー角を読み替える |

---

## 2. S1 で何が変わるか

### 操作の前と後

| 操作 | 今 | S1 の後 |
|---|---|---|
| 停止中に編集して **Play** | シーンが作り直されて、編集が消える | **編集したまま始まる** |
| **Stop** | コードで作り直す（`sceneRoot_` の下だけ。一番上に作った物は Play 中の変化が残る） | **Play を押した瞬間の状態へ、全部戻る**（同じ EntityId・同じ並び・選んでいた Entity も） |
| **Restart** | 再生中も停止中も、コードから作り直す | 再生中だけ押せる。Play を押した瞬間からやり直す |
| **Save** | 無い | Control ウィンドウの **Save** か **Ctrl+S**（停止中だけ）。`resources/scenes/GameScene.scene.json` に書く |
| 起動 | `GameScene::Initialize` のコードで作る | ファイルがあればファイルから。無ければ `CreateDefaultEntities()`（最初の配置） |
| Release | 同上 | 同じファイルから起動する |
| シーン切り替え | `NextScene()` の直後、その場で | 次のフレームの頭で。切り替え先のファイルから作る。今まで何もしていなかった `RequestNextScene` も効く |
| ウィンドウを閉じる | シーンの `Finalize` が呼ばれていなかった | 呼ばれる |
| 別の Entity への参照 | `Handle<Entity>` を持つしかない（Inspector に出せない。Stop・削除の Undo・保存のたびに無効になる） | **`EntityRef`**（EntityId で覚える）。`ui.Field` に渡すだけで、Hierarchy から**ドラッグ＆ドロップ**で入れられる。Stop・削除の Undo・コピー・再起動の後も同じ相手 |
| Hierarchy のクリック | 押した瞬間に選ぶ | **離したときに選ぶ**（押したままドラッグすると、選び直さずに Inspector の参照欄まで運べる） |
| enum | ゲームの Component に書けない | `ui.Field` に渡すだけでコンボ。保存は名前 |
| モデル・テクスチャ | ゲームの Component に書けない | `ui.AssetField`。resources 以下から選ぶ。保存はパス |
| Model Renderer のテクスチャ | 選べない | 選べる（`(none)` ならモデルのマテリアルのテクスチャ）。1x1 の画像（`white1x1.png`）を読むと落ちていたのも直した |
| シーンのコードから Entity を探す | 無い | `EntityManager::FindByName("MonsterBall")`（Handle は Play・Stop のたびに変わるので、`Initialize` の中で探す） |

### ファイルの形（本物のコードで保存した実例）

GameScene の最初の配置（平行光源・MonsterBall）で、MonsterBall に Spin を付けて保存するとこうなる。

```json
{
	"version": 1,
	"entities": [
		{
			"id": 1,
			"name": "Directional Light",
			"parent": 0,
			"active": true,
			"components": {
				"Transform": {
					"translation": [0.0, 3.0, 0.0],
					"rotation": [0.87266463, -0.5235988, 0.0],
					"scale": [1.0, 1.0, 1.0]
				},
				"Directional Light": {
					"color": [1.0, 1.0, 1.0],
					"intensity": 1.0,
					"showIcon": true,
					"showRange": true
				}
			}
		},
		{
			"id": 2,
			"name": "MonsterBall",
			"parent": 0,
			"active": true,
			"components": {
				"Transform": {
					"translation": [0.0, 0.0, 0.0],
					"rotation": [0.0, 0.0, 0.0],
					"scale": [1.0, 1.0, 1.0]
				},
				"Model Renderer": {
					"enabled": true,
					"model": "resources/monsterBall/monsterBall.gltf",
					"texture": "",
					"shadingType": "PBR",
					"blendMode": "Normal",
					"rasterizerType": "SolidBack",
					"depthMode": "TestWrite",
					"billboard": "None",
					"color": [1.0, 1.0, 1.0, 1.0],
					"ambient": [0.2, 0.2, 0.2],
					"diffuse": [1.0, 1.0, 1.0],
					"specular": [0.0, 0.0, 0.0],
					"emissive": [0.0, 0.0, 0.0],
					"shininess": 32.0,
					"metallic": 0.0,
					"roughness": 0.5,
					"alphaCutoff": 0.0,
					"uvTranslation": [0.0, 0.0, 0.0],
					"uvRotation": [0.0, 0.0, 0.0],
					"uvScale": [1.0, 1.0, 1.0]
				},
				"Spin": {
					"Speed (deg/s)": [0.0, 90.0, 0.0]
				}
			}
		}
	]
}
```

| 決まり | 理由 |
|---|---|
| 先頭に `"version": 1` | 値の意味を変える（回転をクオータニオンにする等）ときに上げて、古い版を読み替える |
| 親は `"parent"` に親の **EntityId**（0は親なし） | Handle は実行ごと・作り直すごとに変わる。EntityId は変わらない |
| Component は **型の名前 → 項目** | `type_index` やバイト列は実行ごとに変わるので書けない（`ComponentStorage.h` の注意書きのとおり） |
| 項目の名前は **`ui.Field` のラベル** | `COMPONENT(...)` に書いた並びが、そのまま保存項目になる（ComponentUI を作ったときの狙い） |
| エンジンの型の項目名はメンバ変数の名前（`translation` など） | エンジンの型は Inspector を手書きしているので、保存の名前だけ `EngineComponents.cpp` で決める |
| enum は**名前**（`"PBR"`） | 途中に選択肢を足しても・並べ替えても読める |
| モデル・テクスチャは**パス** | 番号は読み込んだ順で決まる |
| 小数は **float の最短表記**（`0.1`） | 普通の nlohmann::json は double なので `0.10000000149011612` と書かれてしまう |
| 数だけの配列は1行 | Transform が3行で読める。git の差分も見やすい |
| `worldMatrix` は書かない | 毎フレーム計算する結果 |
| `ui.Field` に書いていない項目は保存されない | 実行中だけの値（タイマーなど）はわざと書かなければよい |

### 仕組み（読まなくても写せるが、知っておくと直せる）

#### 書く係・読む係

`COMPONENT(Spin, ...) { ui.Field("Speed", value.speed); }` の中身は「見せ方の関数」`Describe_Spin(ComponentUI& ui, Spin& value)` になっている。**`ui` の中身を差し替えると、同じ関数が3つの仕事をする。**

| `ui` の中身 | `ui.Field("Speed", value.speed)` 1行で起きること | 置き場所 |
|---|---|---|
| Inspector に描く係 | `ImGui::DragFloat3("Speed", ...)` | `ComponentUI.cpp` |
| **書く係**（`JsonWriterUI`） | `json["Speed"] = [x, y, z]` | `ComponentSerializer.cpp` |
| **読む係**（`JsonReaderUI`） | `json` に `"Speed"` があれば `value.speed` に入れる。無ければ初期値のまま | `ComponentSerializer.cpp` |

だから、ゲームの Component は **何も足さなくても保存される**。エンジンの型（Transform・Model Renderer・ライト3種）だけは Inspector を ImGui で手書きしているので、保存用の見せ方の関数を `EngineComponents.cpp` に別に書いた。

**型の名前 → 読み書きの表**（`ComponentSerializer`）は、Editor の表（`ComponentEditorRegistry`）とは別物。Release でもシーンを読むのでこちらが要る。`COMPONENT(...)` は両方に登録し、エンジンの型は `Engine::Initialize` で `ComponentSerializer::RegisterEngineComponents()` が登録する。

#### 読み込みは3周

| 周 | すること | 理由 |
|---|---|---|
| 1 | 全部の Entity を、書いてある **EntityId のまま**作る（親はまだ付けない） | ファイルの中で子が親より先に書かれていても困らない |
| 2 | 親子をつなぐ | 全員そろっているので、順番に関係なく親が見つかる。輪になる親子は `SetParent` が断る |
| 3 | Component を付ける（予約せず、その場で） | 全員そろっているので、Component の中の `EntityRef` の相手が本当に居るかをここで確かめられる（居なければ外して警告） |

Component は `T value{}`（初期値）から始めて、ファイルにある項目だけを上書きする。だから **Component に項目を足しても、古いファイルはそのまま読める**（足した項目は初期値になる）。

#### Play / Stop の流れ

```
[Play を押す]（UIの中。まだ何も変えない）
   ↓ 次のフレームの頭（FlushComponentChanges・EditorHistory::Flush の後。予約は全部反映済み）
退避：全 Entity を JSON の文字列にしてメモリに持つ（ファイルと同じ中身）
   ↓
作り直す：古いシーンの Finalize → 全 Entity を今すぐ消す → 退避した JSON から作る → Initialize
   ↓
再生中（シーンの Update・SYSTEM が回る。動いても消しても作ってもよい）
   ↓
[Stop を押す] → 次のフレームの頭で、退避した JSON からもう一度作り直す → 退避を捨てる
```

- **Stop で戻る物 ＝ Save で保存される物。** 退避とファイルは同じ書き方なので、「Stop したら値が消えた」なら、その値は `ui.Field` に書き忘れている（保存もされない）とすぐ分かる。
- 作り直すと Handle は変わるが、EntityId は同じまま。Component の中の参照（`EntityRef`）も EntityId で覚えているので、Stop の後も同じ相手を指す。Hierarchy で選んでいた Entity も EntityId で選び直す。
- Undo の履歴は作り直すたびに捨てる（今までと同じ）。コピーした物は残る。
- 保存と作り直しを同じフレームで頼まれたら、作り直しが先（Stop と Ctrl+S を続けて押しても、Play 中の状態は保存されない）。

#### 別の Entity への参照は `EntityRef`（Handle ではなく EntityId）

Component に `Handle<Entity>` を持たせると、相手が作り直されるたび（Play / Stop・削除の Undo・再起動）に世代が変わって、**同じ相手なのに無効**になる。`EntityRef` は EntityId だけを持つので、どれをまたいでも同じ相手を指す。

```cpp
struct Homing {
	EntityRef target; // 追う相手
	float speed = 5.0f;
};
COMPONENT(Homing, "Movement") { ui.Field("Target", value.target); } // Hierarchy から Entity をドラッグして入れる

void HomingMove(Homing& homing, TransformComponent& transform, float deltaTime) {
	const TransformComponent* target = EntityManager::Get<TransformComponent>(homing.target); // EntityRef のまま取れる。居なければnullptr
	if (target == nullptr) {
		return;
	}
	// ...
}
SYSTEM(HomingMove);
```

| 書き方 | 意味 |
|---|---|
| `EntityManager::Get<T>(ref)` | 相手の Component（居なければ `nullptr`） |
| `EntityManager::Find(ref)` | 今の Handle（居なければ無効な Handle） |
| `EntityManager::RefOf(handle)` | Handle から参照を作る（コードで入れるとき） |
| `ref.IsSet()` | 何か入っているか（相手が消えていても、番号が入っていれば true。Inspector には `(missing)` と出る） |

- 相手を消して Undo で戻すと、同じ EntityId で戻るので、参照もまた効く。
- 保存するときに相手が居なければ `0` で書く（次の起動で同じ番号が別の Entity に配られても、そちらを指さないように）。読み込んだファイルの相手が居なければ外して警告する。
- シーンのクラスのメンバに覚えるなら `Handle` でもよいが、`Initialize` の中で探し直すこと（`EntityManager::FindByName("Player")`。`CreateDefaultEntities` で覚えた Handle は、Play・Stop・次の起動では無効）。

#### 「全部の Entity がシーンの物」

シーンファイルを使うシーン（`GetSceneFile()` が `nullptr` 以外）は、**EntityManager の中の全部の Entity を自分の物として扱う**。シーンに入るときも出るときも全部消す。だから `sceneRoot_` とその `Destroy` は要らなくなり、Hierarchy の一番上に作った物も保存・復元される。

`GetSceneFile()` を書かないシーンは、今までどおり自分で作って自分で消す（前の書き方のシーンも壊れない）。

**シーンファイルを使うシーンの `Initialize` で Entity を作ってはいけない。** ファイルや退避から作った物に毎回足されて、Play / Stop のたびに1つずつ増える（Save するとファイルにも残る）。最初の配置は `CreateDefaultEntities` に書き、後はエディタで置いて Save する。`Initialize` の前後で Entity の数が変わっていたら、Log に警告が出る。

シーンファイルを読めなかったとき（壊れている・git のマージで `<<<<<<<` が残った・新しい版で保存された）は、空のシーンで始まり、元のファイルを **`〇〇.scene.json.broken`** に写しておく（そのまま Save を2回押すと、本体も `.bak` も空で上書きされてしまうので）。

#### 保存のキーとラベルの変更

- 保存の名前は `ui.Field` のラベル。**ラベルを変えると、前に保存した値は読めなくなる。** 読み込み時に「ファイルの項目 "Speed" を読む所がありません」と警告が出る。
- 表示だけ変えたいときは **`"新しい表示###前のラベル"`** と書く。ImGui の決まり（`"表示###ID"` は表示が変わっても同じ ID）と同じで、`###` の後ろが保存の名前になる。

```cpp
ui.Field("速さ（度/秒）###Speed (deg/s)", value.speed); // 表示は「速さ（度/秒）」、保存の名前は前と同じ
```

- 同じ Component の中に同じ名前が2つあると、保存で後の方が勝つ（警告が出る。ImGui でも ID がぶつかるので、どのみち分ける）。
- 警告（知らない Component・知らない項目・形の違う値・見つからないファイル）は、同じ文を1回だけ Log ウィンドウに出す。

---

## 3. S1 の手順（パート A〜G）

> **2026-09-21 に書き、2026-09-24 にレビューして直した。** 下のコードは全部、scratchpad に作ったエンジンとゲームの写しに入れて、コンパイル（エンジン83ファイル＋ゲーム、Debug / Release、`/W4`）でエラー0・新しい警告0、動作テスト172項目全通過を確かめてある（「5. 検証したこと」）。画面の見た目と実際の操作はまだ確かめていないので、写したら「4. 確認すること」を見てほしい。

エンジン側のパート（A〜E）は互いに独立していて、どの順に写してもよい（ただしビルドが通るのは全部写してから）。**F（ゲーム側）はエンジンをビルドした後に**写す（G の2）。**新しいファイルは5つ**（エンジン）、変わるファイルは21（エンジン19・ゲーム2）。

| パート | 中身 | ファイル |
|---|---|---|
| A | アセットの番号 → パス、1x1 の画像で落ちるのを直す | `ModelManager.h/.cpp`、`TextureManager.h/.cpp`（数行ずつ） |
| B | `EntityRef`・`FindByName`、`ui.Field` で enum・Entity参照・アセットを書けるように、Hierarchy の選び方 | `Entity.h`・`EntityManager.h/.cpp`（数行ずつ）、`ComponentUI.h/.cpp`（全体）、`HierarchyWindow.h`（1か所）・`.cpp`（4か所）、`ModelRendererEditor.h/.cpp`（全体） |
| C | Component の書く係・読む係と、型の名前の表 | 新規 `ComponentSerializer.h/.cpp`・`EngineComponents.cpp`、`GameComponent.h`（2行）、`Engine.cpp`（2か所） |
| D | シーン全体の保存・読み込み | 新規 `SceneSerializer.h/.cpp` |
| E | Play / Stop / Save / シーン切り替え | `IScene.h`・`SceneManager.h/.cpp`（全体）、`WindowManager.cpp`（3か所） |
| F | ゲーム側 | `GameScene.h/.cpp` |
| G | プロジェクトに追加・ビルド | vcxproj / filters |

---

### A. アセットの番号 → パス

モデルもテクスチャも「パス → 番号」の表は持っているが、逆に引く関数が無い。保存するときに番号をパスに直すために足す。表は1つのまま、全部を見て探す（数十個なので遅くない。表を2つにすると、読み込みと解放の両方で2つをそろえ続けることになる）。

#### A-1. `MyEngine/Graphics/Model/ModelManager.h`

`GetModelName` の宣言のすぐ下に2行足す。

```cpp
	// ハンドルから表示名を取得（見つからなければ空）
	static const std::string& GetModelName(uint32_t modelHandle);
	// ハンドルから読み込んだときのパスを取得（見つからなければ空）。シーンの保存でパスに直すのに使う
	static const std::string& GetModelPath(uint32_t modelHandle);
```

#### A-2. `MyEngine/Graphics/Model/ModelManager.cpp`

`GetModelName` の実装のすぐ下に足す。

```cpp
// ===== 読み込んだときのパス取得（番号 → パスの逆引き。モデルの数は多くないので、全部を見て探す）=====
const std::string& ModelManager::GetModelPath(uint32_t modelHandle) {
	for (const auto& [path, handle] : GetInstance().pathToHandle_) {
		if (handle == modelHandle) {
			return path;
		}
	}
	static const std::string emptyString;
	return emptyString;
}
```

#### A-3. `MyEngine/Graphics/Texture/TextureManager.h`

ゲッターの `GetTextureData` の下に2行足す。

```cpp
	static const TextureData* GetTextureData(uint32_t srvIndex);
	// 番号から読み込んだときのパスを取得（見つからなければ空）。シーンの保存でパスに直すのに使う
	static const std::string& GetTexturePath(uint32_t srvIndex);
```

#### A-4. `MyEngine/Graphics/Texture/TextureManager.cpp`

ファイルの一番下（`GetTextureSize` の後）に足す。

```cpp
// 番号 → パスの逆引き（textures_ のキーがパス）
const std::string& TextureManager::GetTexturePath(uint32_t srvIndex) {
	for (const auto& [path, data] : instance_->textures_) {
		if (data.srvIndex == srvIndex) {
			return path;
		}
	}
	static const std::string emptyString;
	return emptyString;
}
```

#### A-5. `MyEngine/Graphics/Texture/TextureManager.cpp`（`LoadTextureFromFile` の中）

1x1 の画像（`white1x1.png` など）を読むと、`GenerateMipMaps` が `E_INVALIDARG` を返して assert で止まっていた（1x1 はもう小さくできないので、DirectXTex がミップマップを作れない）。今まではコードで読んでいなかったので表に出なかったが、B でテクスチャを Inspector から選べるようにすると、単色にしたくて選ぶ人が必ず出る。`// ミップマップの生成` のすぐ上に足す。

```cpp
	// 1x1 の画像はこれ以上小さくできない（GenerateMipMaps は E_INVALIDARG を返す）ので、ミップマップを作らずにそのまま使う
	if (image.GetMetadata().width == 1 && image.GetMetadata().height == 1) {
		return image;
	}
	// ミップマップの生成
	DirectX::ScratchImage mipImages{};
```

---

### B. `EntityRef`、`ui.Field` で enum・Entity参照・アセット、Hierarchy の選び方

ゲームの Component に書ける物が増える。

```cpp
struct Homing {
	EntityRef target;           // 追う相手（EntityId で覚える）
	float speed = 5.0f;
	HomingMode mode = HomingMode::Chase; // enum
	uint32_t trailModel = 0;    // モデル
};
COMPONENT(Homing, "Movement") {
	ui.Field("Target", value.target);   // Hierarchy から Entity をドラッグして入れる。右クリックで外す
	ui.Field("Speed", value.speed, AtLeast(0.0f));
	ui.Field("Mode", value.mode);       // enum はコンボになる（名前は magic_enum が作る）
	ui.AssetField("Trail Model", value.trailModel, AssetType::Model); // resources 以下のモデルから選ぶ
}
```

- **enum** は `template` の `Field` が受ける。`int&` にも `float&` にも渡せない型なので、ほかの `Field` とぶつからない。**今の値と違う選択肢を選んだときだけ書き戻す**（magic_enum が知らない値を、触っていないのに先頭の選択肢へ変えてしまわないように）。
- **Entity参照**（`EntityRef`）の欄は「相手の名前を出した台」。相手が消えていると `(missing)`。Hierarchy がドラッグで運んでいる物の種類の名前（`"ENTITY_HANDLE"`）を `HierarchyWindow.h` に出して、Inspector 側でも同じ名前で受け取る。運ばれてくるのは Handle なので、`RefOf` で EntityId に直して覚える。
- **Hierarchy の選択は「離したとき、ドラッグしていなければ」** に変える。押した瞬間に選ぶと、B を Inspector の参照欄へ運ぼうとした瞬間に Inspector の中身が B に切り替わり、A の参照欄が消えて落とす先が無くなる（ImGui を実際に動かして確かめた）。
- Hierarchy の各行の ImGui の ID を Handle の番号から **EntityId** に変える。Play / Stop で作り直すと Handle の番号が逆順に振り直されて、開いた・閉じた状態が別の Entity に移ってしまうため。
- **アセット** の欄は、resources と MyEngine/Resources の下を1回だけ掘って、その種類の拡張子のファイルを並べる。`(Refresh)` で掘り直す。選んだ瞬間に読み込む（Inspector を描くのは描画のコマンドを積む前なので、テクスチャの読み込みがコマンドリストを閉じても大丈夫。今までの Model Renderer のモデル選択と同じ）。
- Model Renderer の自前のモデル選択（ファイルを掘る処理とコンボ、約70行）は、この `AssetField` に置き換えた。テクスチャも選べるようになる。

#### B-0. `MyEngine/Entity/Entity.h`・`EntityManager.h/.cpp`（`EntityRef` と `FindByName`）

(1) `Entity.h`：`using EntityId = ...;` のすぐ下に足す。

```cpp
/// <summary>
/// 別のEntityへの参照（Componentに持たせる用）
/// <para>Handleではなく EntityId で覚えるので、保存・Play/Stop・削除のUndo・コピーをまたいでも同じ相手を指す
/// （Handleは作り直すたびに変わる）。使うときは EntityManager::Get&lt;T&gt;(ref) / EntityManager::Find(ref)</para>
/// </summary>
struct EntityRef {
	EntityId id = 0; // 0は「無し」

	bool IsSet() const { return id != 0; }
	bool operator==(const EntityRef&) const = default;
};
```

(2) `EntityManager.h`：`FindById` の宣言のすぐ下に足す。

```cpp
	static Handle<Entity> FindById(EntityId id); // 無ければ無効なHandle（0を渡しても無効なHandle＝root扱いにできる）
	static Handle<Entity> Find(EntityRef ref) { return FindById(ref.id); } // 参照から今のHandleを引く（居なければ無効なHandle）
	static EntityRef RefOf(Handle<Entity> handle);                        // Handleから参照を作る（Componentに覚えさせるとき）
	// 名前で探す。同じ名前が複数あれば最初の1つ、無ければ無効なHandle
	// シーンの Initialize で、シーンファイルから作られたEntityを探すのに使う（Handleは Play / Stop のたびに変わる）
	static Handle<Entity> FindByName(const std::string& name);
```

(3) `EntityManager.h`：`template<class T> static T* Get(Handle<Entity> handle)` の関数のすぐ下に足す。

```cpp
	// 参照（EntityRef）から取る。相手が居なければnullptr
	template<class T> static T* Get(EntityRef ref) { return Get<T>(Find(ref)); }
```

(4) `EntityManager.cpp`：`FindById` の実装のすぐ下に足す。

```cpp
EntityRef EntityManager::RefOf(Handle<Entity> handle) {
	const Entity* entity = Get(handle);
	return {entity ? entity->id : 0};
}

Handle<Entity> EntityManager::FindByName(const std::string& name) {
	for (const Entity& entity : instance_->entities_) {
		if (entity.name == name) {
			return entity.self;
		}
	}
	return {};
}
```

#### B-1. `MyEngine/Component/ComponentUI.h`（ファイル全体を差し替え）

```cpp
#pragma once
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"

/// <summary>
/// 項目の見せ方の指定。省略できる（Range(...) や Tip(...) で作る）
/// </summary>
struct FieldStyle {
	float min = 0.0f;              // 下限（min < max のときだけ効く）
	float max = 0.0f;              // 上限
	float dragSpeed = 0.0f;        // つまんで動かす速さ（0なら型ごとの既定）
	bool slider = false;           // true: スライダー、false: つまんで動かす
	const char* tooltip = nullptr; // マウスを乗せたときに出す説明

	// つなげて書ける（Range(0.0f, 10.0f).Tip("説明") のように）
	FieldStyle Tip(const char* text) const {
		FieldStyle copy = *this;
		copy.tooltip = text;
		return copy;
	}
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

// 下限だけ決める（つまんで動かす。負の値にしたくない項目に）
inline FieldStyle AtLeast(float min) {
	FieldStyle style;
	style.min = min;
	style.max = FLT_MAX; // windows.h の max マクロに邪魔されないよう、numeric_limits ではなく cfloat のFLT_MAXを使う
	return style;
}

// つまんで動かす速さを決める
inline FieldStyle Drag(float speed) {
	FieldStyle style;
	style.dragSpeed = speed;
	return style;
}

// 説明だけ付ける
inline FieldStyle Tip(const char* text) {
	FieldStyle style;
	style.tooltip = text;
	return style;
}

// ファイルから読み込む物（アセット）の種類。AssetField で使う
enum class AssetType {
	Model,   // ModelManager::Load の番号
	Texture, // TextureManager::Load の番号
};

// アセットの番号からパスを引く（未選択・知らない番号なら空）。保存とInspectorの表示で使う
const std::string& GetAssetPath(AssetType type, uint32_t handle);
// パスのファイルを読み込んで番号をもらう（空・ファイルが無いなら0＝未選択）
uint32_t LoadAsset(AssetType type, const std::string& path);

/// <summary>
/// Componentの中身の見せ方を書くための道具。ゲーム側にImGuiが出てこないようにするための薄い包み
/// <para>実体は差し替えられる。Inspectorに描く係・ファイルへ書く係・ファイルから読む係の3つがあり、
/// COMPONENT(...) に書いた並びが、そのまま「Inspectorの見た目」と「セーブデータの項目」になる（ComponentSerializer.cpp）</para>
/// <para>保存するときの項目の名前は label。label を変えると、前に保存した値は読めなくなる（読み込み時に警告が出る）</para>
/// </summary>
class ComponentUI {
public:
	virtual ~ComponentUI() = default;

	// ===== 値を1つ見せる（型で見た目が決まる）=====
	virtual void Field(const char* label, float& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, int& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, bool& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector2& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector3& value, FieldStyle style = {}) = 0;
	virtual void Field(const char* label, Vector4& value, FieldStyle style = {}) = 0;

	// ===== 別のEntityへの参照（InspectorではHierarchyからドラッグ＆ドロップで入れる。保存はEntityIdで）=====
	virtual void Field(const char* label, EntityRef& value, FieldStyle style = {}) = 0;

	// ===== enum（選択肢の名前は magic_enum が型から作る。保存も名前で行うので、並びを変えても読める）=====
	template<class E>
	    requires std::is_enum_v<E>
	void Field(const char* label, E& value, FieldStyle style = {}) {
		constexpr auto names = magic_enum::enum_names<E>(); // 選択肢の名前（宣言の順）
		const auto current = magic_enum::enum_index(value); // 今の値が何番目か（magic_enumが知らない値なら無し）
		size_t index = current.value_or(0);
		const size_t before = index;
		EnumField(label, index, names, style);
		// 変わったときだけ書き戻す（知らない値を、触っていないのに先頭の選択肢へ変えてしまわないように）
		if (index != before && index < names.size()) {
			value = magic_enum::enum_value<E>(index);
		}
	}

	// ===== アセット（モデル・テクスチャの番号。Inspectorではファイルを選ぶ。保存はパスで）=====
	virtual void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle style = {}) = 0;

	// ===== 色（0〜1。色見本を押すと色を選べる）=====
	virtual void ColorField(const char* label, Vector3& rgb) = 0;
	virtual void ColorField(const char* label, Vector4& rgba) = 0;

	// ===== 飾り（保存には関係しない）=====
	virtual void Label(const char* text) = 0;               // 灰色の説明文
	virtual void Separator(const char* text = nullptr) = 0; // 区切り線（文字を入れると見出しになる）
	virtual void Space() = 0;                               // 1行あける

protected:
	// enumの中身。何番目の選択肢かで受け渡す（names は選択肢の名前）
	virtual void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle style) = 0;
};

// Inspectorに描く係（エンジンが1つだけ持っている。ゲームからは触らない）
ComponentUI& GetInspectorUI();
```

#### B-2. `MyEngine/Component/ComponentUI.cpp`（ファイル全体を差し替え）

アセットの番号 ↔ パス（`GetAssetPath` / `LoadAsset`）は、Inspector と保存・読み込みの両方で使うので `#ifdef USE_IMGUI` の外に置く。**無いファイルを `ModelManager::Load` に渡すと中の assert で止まる（Release でも abort）ので、先に `exists` で確かめる。**

```cpp
#include "MyEngine/Component/ComponentUI.h"

#include <filesystem>
#include <format>
#include <string>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

//=============================================================================
// アセットの番号 ↔ パス（Inspector・保存・読み込みで共通）
//=============================================================================
const std::string& GetAssetPath(AssetType type, uint32_t handle) {
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	if (handle == 0) {
		return kEmpty;
	}
	switch (type) {
	case AssetType::Model:
		return ModelManager::GetModelPath(handle);
	case AssetType::Texture:
		return TextureManager::GetTexturePath(handle);
	}
	return kEmpty;
}

uint32_t LoadAsset(AssetType type, const std::string& path) {
	if (path.empty()) {
		return 0; // 未選択
	}
	// 無いファイルを読ませるとManagerの中で止まるので、先に確かめる
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		LogManager::Warning(std::format("アセットが見つかりません（未選択として扱います）: {}", path));
		return 0;
	}
	switch (type) {
	case AssetType::Model:
		return ModelManager::Load(path);
	case AssetType::Texture:
		return TextureManager::Load(path);
	}
	return 0;
}

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
// 範囲が決まっているか
bool HasRange(const FieldStyle& style) { return style.min < style.max; }

// つまんで動かす速さ（指定が無ければ型ごとの既定）
float DragSpeed(const FieldStyle& style, float defaultSpeed) { return style.dragSpeed > 0.0f ? style.dragSpeed : defaultSpeed; }

// 直前の項目にマウスを乗せたら説明を出す
void DrawTooltip(const FieldStyle& style) {
	if (style.tooltip != nullptr && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", style.tooltip);
	}
}

// ===== アセットの一覧（フォルダを掘るのは、最初に開いたときと Refresh のときだけ）=====
constexpr const char* kAssetSearchRoots[] = {"resources", "MyEngine/Resources"}; // 探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr std::string_view kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};
constexpr std::string_view kTextureExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".hdr"};

// その種類の拡張子か（大文字でも通るように小文字へ直して比べる）
bool IsAssetFile(const std::filesystem::path& path, AssetType type) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const std::span<const std::string_view> candidates = (type == AssetType::Model) ? std::span<const std::string_view>(kModelExtensions) : std::span<const std::string_view>(kTextureExtensions);
	return std::find(candidates.begin(), candidates.end(), extension) != candidates.end();
}

// その種類のファイルのパス一覧（区切りは / にそろえる。ModelManagerのキャッシュのキーと同じ形にするため）
const std::vector<std::string>& AssetFiles(AssetType type, bool refresh) {
	static std::vector<std::string> files[2]; // [AssetType] ごとの一覧
	static bool scanned[2] = {false, false};
	const size_t slot = static_cast<size_t>(type);
	if (scanned[slot] && !refresh) {
		return files[slot];
	}
	files[slot].clear();
	for (const char* root : kAssetSearchRoots) {
		std::error_code error; // 例外ではなくエラーコードで受ける（フォルダが無くても止まらない）
		if (!std::filesystem::exists(root, error)) {
			continue;
		}
		const auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsAssetFile(entry.path(), type)) {
				files[slot].push_back(entry.path().generic_string());
			}
		}
	}
	std::sort(files[slot].begin(), files[slot].end());
	scanned[slot] = true;
	return files[slot];
}

/// <summary>
/// Inspectorに描く係。ImGuiを知っているのはここだけ
/// </summary>
class InspectorComponentUI final : public ComponentUI {
public:
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

	void Field(const char* label, int& value, FieldStyle style) override {
		const int min = static_cast<int>(style.min);
		const int max = static_cast<int>(style.max);
		if (style.slider && HasRange(style)) {
			ImGui::SliderInt(label, &value, min, max);
		} else if (HasRange(style)) {
			ImGui::DragInt(label, &value, DragSpeed(style, 1.0f), min, max);
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
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat2(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat2(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector3& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat3(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat3(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	void Field(const char* label, Vector4& value, FieldStyle style) override {
		if (style.slider && HasRange(style)) {
			ImGui::SliderFloat4(label, &value.x, style.min, style.max);
		} else if (HasRange(style)) {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f), style.min, style.max);
		} else {
			ImGui::DragFloat4(label, &value.x, DragSpeed(style, 0.01f));
		}
		DrawTooltip(style);
	}

	// 相手の名前を出した台。HierarchyからEntityをドラッグして落とすと入る。右クリックで外す
	void Field(const char* label, EntityRef& value, FieldStyle style) override {
		const Entity* target = EntityManager::Get(EntityManager::Find(value));
		// 番号はあるのに居ない＝相手が消えている（消した操作をUndoすると、同じ番号で戻ってまた指す）
		const std::string preview = target ? target->name : (value.IsSet() ? "(missing)" : "(none)");
		ImGui::PushID(label); // 同じ区画に参照が2つあっても、台のIDがぶつからないように
		ImGui::Button(preview.c_str(), ImVec2(ImGui::CalcItemWidth(), 0.0f)); // 押しても何もしない
		DrawTooltip(style);
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HierarchyWindow::kDragDropType)) {
				// Hierarchyが運んでくるのはHandle。覚えるのは作り直しても変わらない番号
				value = EntityManager::RefOf(*static_cast<const Handle<Entity>*>(payload->Data));
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("menu")) {
			if (ImGui::MenuItem("Clear")) {
				value = {};
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		// 他の項目と同じく、ラベルは右に出す（"表示###キー" の ## から後ろは、ImGuiのほかの項目と同じく出さない）
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(label, std::strstr(label, "##")); // 見つからなければnullptr＝最後まで出す
	}

	// resources以下の、その種類のファイルから選ぶ
	void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle style) override {
		const std::string& path = GetAssetPath(type, handle);
		const char* preview = (handle == 0) ? "(none)" : (path.empty() ? "(unknown)" : path.c_str());
		if (ImGui::BeginCombo(label, preview)) {
			const bool refresh = ImGui::Selectable("(Refresh)"); // フォルダにファイルを増やしたとき用
			if (ImGui::Selectable("(none)", handle == 0)) {
				handle = 0;
			}
			for (const std::string& file : AssetFiles(type, refresh)) {
				if (ImGui::Selectable(file.c_str(), file == path)) {
					handle = LoadAsset(type, file); // 同じパスならキャッシュが返るので、選び直しても読み込み直さない
				}
			}
			ImGui::EndCombo();
		}
		DrawTooltip(style);
	}

	void ColorField(const char* label, Vector3& rgb) override { ImGui::ColorEdit3(label, &rgb.x); }
	void ColorField(const char* label, Vector4& rgba) override { ImGui::ColorEdit4(label, &rgba.x); }

	void Label(const char* text) override { ImGui::TextDisabled("%s", text); }
	void Separator(const char* text) override {
		if (text != nullptr) {
			ImGui::SeparatorText(text);
		} else {
			ImGui::Separator();
		}
	}
	void Space() override { ImGui::Spacing(); }

protected:
	void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle style) override {
		// string_view は末尾に0があるとは限らないので、ImGuiに渡す前に std::string にする
		const std::string current = index < names.size() ? std::string(names[index]) : std::string("?");
		if (ImGui::BeginCombo(label, current.c_str())) {
			for (size_t i = 0; i < names.size(); ++i) {
				const std::string name(names[i]);
				if (ImGui::Selectable(name.c_str(), i == index)) {
					index = i;
				}
				if (i == index) {
					ImGui::SetItemDefaultFocus(); // 開いたときに今の選択へスクロールする
				}
			}
			ImGui::EndCombo();
		}
		DrawTooltip(style);
	}
};
} // namespace

#else // USE_IMGUI

namespace {
/// <summary>
/// Editorの無いビルド（Release）用。Componentの見せ方は書かれているが、描く相手が居ないので何もしない
/// </summary>
class InspectorComponentUI final : public ComponentUI {
public:
	void Field(const char*, float&, FieldStyle) override {}
	void Field(const char*, int&, FieldStyle) override {}
	void Field(const char*, bool&, FieldStyle) override {}
	void Field(const char*, Vector2&, FieldStyle) override {}
	void Field(const char*, Vector3&, FieldStyle) override {}
	void Field(const char*, Vector4&, FieldStyle) override {}
	void Field(const char*, EntityRef&, FieldStyle) override {}
	void AssetField(const char*, uint32_t&, AssetType, FieldStyle) override {}
	void ColorField(const char*, Vector3&) override {}
	void ColorField(const char*, Vector4&) override {}
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}

protected:
	void EnumField(const char*, size_t&, std::span<const std::string_view>, FieldStyle) override {}
};
} // namespace

#endif // USE_IMGUI

//=============================================================================
// Inspectorに描く係を取り出す
//=============================================================================
ComponentUI& GetInspectorUI() {
	static InspectorComponentUI ui; // 最初に呼ばれたときに1つだけ作られる
	return ui;
}
```

#### B-3. `MyEngine/Editor/Windows/HierarchyWindow.h`

`class HierarchyWindow {` の `public:` のすぐ下に足す。

```cpp
class HierarchyWindow {
public:
	// Entityをドラッグで運ぶときの種類の名前（HierarchyからInspectorの参照欄へ落とすときも同じ名前で受け取る）
	static constexpr const char* kDragDropType = "ENTITY_HANDLE";

```

#### B-4. `MyEngine/Editor/Windows/HierarchyWindow.cpp`（4か所）

(1) 無名の namespace の先頭にある次の1行を**消す**（B-3 でクラスの中へ移した）。

```cpp
constexpr const char* kDragDropType = "ENTITY_HANDLE"; // ドラッグで運ぶものの種類の名前
```

(2) すぐ下の `AcceptEntityDrop` の中は無名の namespace の関数（クラスの外）なので、名前の前に `HierarchyWindow::` を付ける。

```cpp
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HierarchyWindow::kDragDropType)) {
```

`DrawEntityNode` の中の `ImGui::SetDragDropPayload(kDragDropType, ...)` はクラスの関数の中なので、そのままでよい。

(3) `DrawEntityNode` の中の `PushID` の行と、その上のコメント1行（`// Handleの中身をIDにする…`）の計2行を、次の3行に置き換える（Handle の番号から EntityId に変える）。

```cpp
	// EntityIdをIDにする（名前が同じEntityがあってもぶつからない）
	// Handleの番号だと、Play / Stop で作り直したときに振り直されて、開いた・閉じた状態が別のEntityに移ってしまう
	ImGui::PushID(static_cast<int>(entity->id));
```

(4) そのすぐ下の「クリックで選択」の `const bool clicked = ...` の1行を、下のコードの2〜5行目（コメント2行・コード2行）に置き換える（1行目の見出しのコメントと、最後の `if (clicked && !ImGui::IsItemToggledOpen())` は今あるものと同じ）。

```cpp
		// --- クリックで選択（右クリックでも選ぶ）、ダブルクリックで名前の変更 ---
		// 左クリックは「離したとき、ドラッグしていなければ」選ぶ。押した瞬間に選ぶと、Inspectorの参照欄へドラッグする前に
		// Inspectorの中身がドラッグしているEntityに切り替わってしまい、落とす先が消える
		const bool clickedLeft = ImGui::IsItemDeactivated() && ImGui::GetDragDropPayload() == nullptr;
		const bool clicked = clickedLeft || ImGui::IsItemClicked(ImGuiMouseButton_Right);
		if (clicked && !ImGui::IsItemToggledOpen()) {
```

`IsItemDeactivated()` は「押していた項目から、マウスを離した」フレームで true。ドラッグして離したときは、まだ運んでいる物（`GetDragDropPayload()`）があるので選ばない。

#### B-5. `MyEngine/Editor/Inspector/ModelRendererEditor.h`（ファイル全体を差し替え）

`DrawModelPicker` を消しただけ。

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
	const char* GetCategory() const override { return "Rendering3D"; }

protected:
	void DrawComponent(ModelRendererComponent& render) const override;
};
```

#### B-6. `MyEngine/Editor/Inspector/ModelRendererEditor.cpp`（ファイル全体を差し替え）

```cpp
#include "ModelRendererEditor.h"

#include <externals/imgui/imgui.h>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"


//=============================================================================
// Inspectorの中身
//=============================================================================
void ModelRendererEditor::DrawComponent(ModelRendererComponent& render) const {
	ImGui::Checkbox("Enabled", &render.enabled);

	// --- どのモデル・テクスチャを使うか（resources以下のファイルから選ぶ。ComponentUIのアセット欄と同じ物）---
	ComponentUI& ui = GetInspectorUI();
	ui.AssetField("Model", render.modelHandle, AssetType::Model);
	ui.AssetField("Texture", render.textureHandle, AssetType::Texture, Tip("(none) なら、モデルのマテリアルのテクスチャを使う"));

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
		render.color = (static_cast<uint32_t>(rgba[0] * 255.0f + 0.5f) << 24) | (static_cast<uint32_t>(rgba[1] * 255.0f + 0.5f) << 16) | (static_cast<uint32_t>(rgba[2] * 255.0f + 0.5f) << 8) |
		               static_cast<uint32_t>(rgba[3] * 255.0f + 0.5f);
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
```

---

### C. Component の書く係・読む係と、型の名前の表

#### C-1. 新規：`MyEngine/Component/ComponentSerializer.h`

`SceneJson` は nlohmann の json の「キーを書いた順に並べる・小数を float で持つ」版。ヘッダでは `json_fwd.hpp`（前方宣言だけの軽いヘッダ）しか読まないので、ゲームの Component のビルドは重くならない。

`Register<T>` の中の2つのラムダが「型を知っている部分」で、あとは型を知らずに表で回す（`IComponentStorage` と同じ考え方）。

```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <externals/nlohmann/json_fwd.hpp>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Entity/EntityManager.h"

// シーンファイルに使うJSONの型
// ・キーは書いた順に並ぶ（ordered_map。ui.Field の順番のままファイルに出る）
// ・小数は float のまま書く（普通の nlohmann::json は double なので、0.1f が 0.10000000149011612 と書かれてしまう）
using SceneJson = nlohmann::basic_json<nlohmann::ordered_map, std::vector, std::string, bool, std::int64_t, std::uint64_t, float>;


/// <summary>
/// Componentの型ごとの「保存する項目」の表。型の名前（ファイルに書く名前）と、見せ方の関数（COMPONENTの中身と同じ形）を持つ
/// <para>見せ方の関数に「書く係」「読む係」の ComponentUI を渡すと、ui.Field の並びがそのままJSONの項目になる</para>
/// <para>Editorの登録表（ComponentEditorRegistry）とは別物。Releaseでもシーンを読むのに要る</para>
/// </summary>
class ComponentSerializer {
public:
	/// <summary>
	/// 型を登録する。name はファイルに書く名前（変えると、前に保存したファイルのその型が読めなくなる）
	/// </summary>
	template<class T> static void Register(const char* name, void (*describe)(ComponentUI&, T&)) {
		RegisterErased(
		    name,
		    // --- 書く：持っていれば、写しに対して見せ方の関数を呼ぶ（関数は T& を受け取るので、本物を書き換えないように写す）---
		    [describe](Handle<Entity> entity, ComponentUI& writer) {
			    const T* component = EntityManager::Get<T>(entity);
			    if (component == nullptr) {
				    return false; // 持っていない
			    }
			    T copy = *component;
			    describe(writer, copy);
			    return true;
		    },
		    // --- 読む：初期値から始めて、ファイルにある項目だけ上書きし、予約せずその場で付ける（Transformのように持っていれば上書き）---
		    [describe](Handle<Entity> entity, ComponentUI& reader) {
			    T value{};
			    describe(reader, value);
			    EntityManager::RestoreComponents(entity, {ComponentSnapshot::Make(value)});
		    });
	}

	// Entityが持っているComponentを全部 out に書く（{"型の名前": {"項目": 値, ...}, ...}）。警告は warnings に集める（同じ文を何度も出さないため）
	static void Save(Handle<Entity> entity, SceneJson& out, std::set<std::string>& warnings);
	// in に書いてあるComponentを全部付ける。知らない型の名前は警告して飛ばす。フレームの境目でだけ呼ぶ
	static void Load(Handle<Entity> entity, const SceneJson& in, std::set<std::string>& warnings);

	// エンジンのComponent（Transform・ModelRenderer・ライト）を登録する。定義は EngineComponents.cpp
	static void RegisterEngineComponents();

private:
	using SaveFunction = std::function<bool(Handle<Entity>, ComponentUI&)>; // 持っていなければfalse
	using LoadFunction = std::function<void(Handle<Entity>, ComponentUI&)>;

	// 型を消した形で表に入れる（名前順に並べる）
	static void RegisterErased(const char* name, SaveFunction save, LoadFunction load);
};
```

#### C-2. 新規：`MyEngine/Component/ComponentSerializer.cpp`

```cpp
#include "ComponentSerializer.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Diagnostics/LogManager.h"

namespace {
//=============================================================================
// 登録表
//=============================================================================
struct Entry {
	std::string name;                                       // ファイルに書く型の名前
	std::function<bool(Handle<Entity>, ComponentUI&)> save; // 持っていれば書いてtrue
	std::function<void(Handle<Entity>, ComponentUI&)> load; // 読んでその場で付ける
};

// 名前順に並べて持つ。関数の中のstaticなので、ゲームのCOMPONENT（mainより前に作られる）との初期化の順番に左右されない
std::vector<Entry>& Entries() {
	static std::vector<Entry> entries;
	return entries;
}

// ファイルに書く順番。Transform を先頭に（Inspectorと同じ）、あとは名前順
// 登録した順番（＝リンクの順番）に左右されないので、ビルドし直してもファイルの並びが変わらない
bool IsBefore(std::string_view a, std::string_view b) {
	const bool aIsTransform = (a == "Transform");
	const bool bIsTransform = (b == "Transform");
	if (aIsTransform != bIsTransform) {
		return aIsTransform;
	}
	return a < b;
}

const Entry* FindEntry(std::string_view name) {
	for (const Entry& entry : Entries()) {
		if (entry.name == name) {
			return &entry;
		}
	}
	return nullptr;
}

// 保存に使う項目の名前。"表示名###キー" と書いてあれば ### より後ろ、無ければ label そのもの
// ImGuiの決まり（"表示###ID" は表示が変わっても同じID）と同じ。表示名を後から変えたくなったら
// "新しい表示名###前の名前" と書けば、前に保存したファイルもそのまま読める
std::string KeyOf(const char* label) {
	const std::string_view text(label);
	const size_t mark = text.find("###");
	return std::string(mark == std::string_view::npos ? text : text.substr(mark + 3));
}

//=============================================================================
// 書く係（見せ方の関数に渡すと、ui.Field の1行ごとにJSONの項目を1つ書く）
//=============================================================================
class JsonWriterUI final : public ComponentUI {
public:
	JsonWriterUI(SceneJson& out, std::string_view typeName, std::set<std::string>& warnings) : out_(out), typeName_(typeName), warnings_(warnings) {}

	void Field(const char* label, float& value, FieldStyle) override { Put(label, value); }
	void Field(const char* label, int& value, FieldStyle) override { Put(label, value); }
	void Field(const char* label, bool& value, FieldStyle) override { Put(label, value); }
	void Field(const char* label, Vector2& value, FieldStyle) override { Put(label, SceneJson::array({value.x, value.y})); }
	void Field(const char* label, Vector3& value, FieldStyle) override { Put(label, SceneJson::array({value.x, value.y, value.z})); }
	void Field(const char* label, Vector4& value, FieldStyle) override { Put(label, SceneJson::array({value.x, value.y, value.z, value.w})); }

	// 相手の EntityId を書く。相手がもう居なければ0（次に起動したとき、同じ番号が別のEntityに配られることがあるので）
	void Field(const char* label, EntityRef& value, FieldStyle) override {
		const bool isAlive = EntityManager::Find(value).IsValid();
		Put(label, isAlive ? value.id : EntityId{0});
	}

	// アセットはパスで書く（番号は読み込んだ順で決まるので、次に起動したときは別の物を指す）
	void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle) override {
		const std::string& path = GetAssetPath(type, handle);
		if (handle != 0 && path.empty()) {
			warnings_.insert(std::format("{}: \"{}\" はファイルから読んだ物ではないので保存できません（未選択として書きます）", typeName_, KeyOf(label)));
		}
		Put(label, path);
	}

	void ColorField(const char* label, Vector3& rgb) override { Field(label, rgb, {}); }
	void ColorField(const char* label, Vector4& rgba) override { Field(label, rgba, {}); }

	// 飾りは保存しない
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}

protected:
	// enumは名前で書く（途中に選択肢を足しても、並べ替えても読めるように）
	void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle) override {
		if (index < names.size()) {
			Put(label, std::string(names[index]));
		}
	}

private:
	template<class Value> void Put(const char* label, Value&& value) {
		const std::string key = KeyOf(label);
		if (out_.contains(key)) {
			warnings_.insert(std::format("{}: 項目の名前 \"{}\" が2つあります。後の方で上書きされます（\"表示名###別の名前\" で分けられます）", typeName_, key));
		}
		out_[key] = std::forward<Value>(value);
	}

	SceneJson& out_;
	std::string_view typeName_;         // 警告に出す型の名前
	std::set<std::string>& warnings_;   // 警告（同じ文は1回だけ出すように、集めてから出す）
};

//=============================================================================
// 読む係（ui.Field の1行ごとに、同じ名前の項目を探して値を入れる。無ければ初期値のまま）
//=============================================================================
class JsonReaderUI final : public ComponentUI {
public:
	JsonReaderUI(const SceneJson& in, std::string_view typeName, std::set<std::string>& warnings) : in_(in), typeName_(typeName), warnings_(warnings) {}

	void Field(const char* label, float& value, FieldStyle) override {
		if (const SceneJson* item = Find(label, &SceneJson::is_number)) {
			value = item->get<float>();
		}
	}
	void Field(const char* label, int& value, FieldStyle) override {
		if (const SceneJson* item = Find(label, &SceneJson::is_number_integer)) {
			value = item->get<int>();
		}
	}
	void Field(const char* label, bool& value, FieldStyle) override {
		if (const SceneJson* item = Find(label, &SceneJson::is_boolean)) {
			value = item->get<bool>();
		}
	}
	void Field(const char* label, Vector2& value, FieldStyle) override { ReadFloats(label, &value.x, 2); }
	void Field(const char* label, Vector3& value, FieldStyle) override { ReadFloats(label, &value.x, 3); }
	void Field(const char* label, Vector4& value, FieldStyle) override { ReadFloats(label, &value.x, 4); }

	// 相手の EntityId を読む（読み込みは「全Entityを作ってからComponent」の順なので、相手が居るならもう作られている）
	void Field(const char* label, EntityRef& value, FieldStyle) override {
		const SceneJson* item = Find(label, &SceneJson::is_number_unsigned);
		if (item == nullptr) {
			return;
		}
		const EntityId id = item->get<EntityId>();
		if (id != 0 && !EntityManager::FindById(id).IsValid()) {
			warnings_.insert(std::format("{}: \"{}\" の相手（EntityId {}）がシーンに無いので外しました", typeName_, KeyOf(label), id));
			value = {};
			return;
		}
		value.id = id;
	}

	void AssetField(const char* label, uint32_t& handle, AssetType type, FieldStyle) override {
		const SceneJson* item = Find(label, &SceneJson::is_string);
		if (item == nullptr) {
			return;
		}
		// LoadAsset も見つからなければ知らせてくれるが、同じアセットを何個ものEntityが指していると同じ文が何度も出る。
		// ここで先に確かめて warnings_（同じ文は1回だけ）に入れることで、1回にまとめる
		const std::string& path = item->get_ref<const std::string&>();
		std::error_code error;
		if (!path.empty() && !std::filesystem::exists(path, error)) {
			warnings_.insert(std::format("{}: \"{}\" のアセットが見つかりません（未選択にします）: {}", typeName_, KeyOf(label), path));
			handle = 0;
			return;
		}
		handle = LoadAsset(type, path); // 空なら0（未選択）
	}

	void ColorField(const char* label, Vector3& rgb) override { ReadFloats(label, &rgb.x, 3); }
	void ColorField(const char* label, Vector4& rgba) override { ReadFloats(label, &rgba.x, 4); }

	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}

	// ファイルにあったのに誰も読まなかった項目を知らせる（ラベルを変えた・項目を消したときに気づけるように）
	void WarnUnused() const {
		for (auto it = in_.begin(); it != in_.end(); ++it) {
			if (std::find(used_.begin(), used_.end(), it.key()) == used_.end()) {
				warnings_.insert(std::format("{}: ファイルの項目 \"{}\" を読む所がありません（ラベルを変えたなら \"新しい表示名###{}\" と書くと読めます。このまま保存すると消えます）", typeName_, it.key(), it.key()));
			}
		}
	}

protected:
	void EnumField(const char* label, size_t& index, std::span<const std::string_view> names, FieldStyle) override {
		const SceneJson* item = Find(label, &SceneJson::is_string);
		if (item == nullptr) {
			return;
		}
		const std::string& name = item->get_ref<const std::string&>();
		const auto found = std::find(names.begin(), names.end(), name);
		if (found == names.end()) {
			warnings_.insert(std::format("{}: \"{}\" = \"{}\" は知らない選択肢です（初期値のままにします）", typeName_, KeyOf(label), name));
			return;
		}
		index = static_cast<size_t>(found - names.begin());
	}

private:
	using TypeCheck = bool (SceneJson::*)() const noexcept; // is_number などの「形を調べる関数」

	// 項目を探す。無ければnullptr（後から足した項目は、古いファイルには無い＝初期値のまま）。形が違えば警告してnullptr
	const SceneJson* Find(const char* label, TypeCheck isExpected) {
		const std::string key = KeyOf(label);
		const auto it = in_.find(key); // const の json に [] で無いキーを引くと止まるので、必ず find を使う
		if (it == in_.end()) {
			return nullptr;
		}
		used_.push_back(key);
		if (!((*it).*isExpected)()) {
			warnings_.insert(std::format("{}: \"{}\" の値の形が違うので、初期値のままにします", typeName_, key));
			return nullptr;
		}
		return &*it;
	}

	// [x, y, z] のような数の並びを読む。数が合わなければ警告して読まない
	void ReadFloats(const char* label, float* out, size_t count) {
		const SceneJson* item = Find(label, &SceneJson::is_array);
		if (item == nullptr) {
			return;
		}
		const bool allNumbers = std::all_of(item->begin(), item->end(), [](const SceneJson& element) { return element.is_number(); });
		if (item->size() != count || !allNumbers) {
			warnings_.insert(std::format("{}: \"{}\" は数が{}個の配列にしてください（初期値のままにします）", typeName_, KeyOf(label), count));
			return;
		}
		for (size_t i = 0; i < count; ++i) {
			out[i] = (*item)[i].get<float>();
		}
	}

	const SceneJson& in_;
	std::string_view typeName_;       // 警告に出す型の名前
	std::set<std::string>& warnings_; // 警告
	std::vector<std::string> used_;   // 読んだ項目の名前
};
} // namespace


//=============================================================================
// 登録
//=============================================================================
void ComponentSerializer::RegisterErased(const char* name, SaveFunction save, LoadFunction load) {
	std::vector<Entry>& entries = Entries();
	// 決まった順番の場所に入れる（IsBefore）
	const auto position = std::lower_bound(entries.begin(), entries.end(), std::string_view(name), [](const Entry& entry, std::string_view key) { return IsBefore(entry.name, key); });
	if (position != entries.end() && position->name == name) {
		LogManager::Error(std::format("Componentの名前 \"{}\" が2つ登録されました。後の方は保存されません", name));
		return;
	}
	entries.insert(position, Entry{name, std::move(save), std::move(load)});
}

//=============================================================================
// 書く・読む
//=============================================================================
void ComponentSerializer::Save(Handle<Entity> entity, SceneJson& out, std::set<std::string>& warnings) {
	for (const Entry& entry : Entries()) {
		// 中身は手元で作ってから入れる（ordered_map は中身が vector なので、out[...] の参照を持ったまま別の項目を足すと参照が壊れる）
		SceneJson fields = SceneJson::object();
		JsonWriterUI writer(fields, entry.name, warnings);
		if (entry.save(entity, writer)) {
			out[entry.name] = std::move(fields);
		}
	}
}

void ComponentSerializer::Load(Handle<Entity> entity, const SceneJson& in, std::set<std::string>& warnings) {
	for (auto it = in.begin(); it != in.end(); ++it) {
		const Entry* entry = FindEntry(it.key());
		if (entry == nullptr) {
			warnings.insert(std::format("知らないComponent \"{}\" を飛ばしました（名前を変えた・消した型は、このまま保存すると消えます）", it.key()));
			continue;
		}
		if (!it->is_object()) {
			warnings.insert(std::format("\"{}\" の中身が {{ }} の形ではないので飛ばしました", it.key()));
			continue;
		}
		JsonReaderUI reader(*it, entry->name, warnings);
		entry->load(entity, reader);
		reader.WarnUnused();
	}
}
```

**読み方のポイント**

- `Find` の `TypeCheck` は「`SceneJson` のメンバ関数へのポインタ」。`&SceneJson::is_number` を渡すと `((*it).*isExpected)()` で `it->is_number()` を呼んだのと同じになる。形を調べる関数を引数で選べるようにしている。
- `const` の json に `[]` で無いキーを引くと nlohmann の中の assert で止まるので、読む係は必ず `find` を使う。
- 警告は `std::set` に集めて、最後に1回ずつ出す（同じ Component を100個持っていても、同じ文は1回）。

#### C-3. 新規：`MyEngine/Component/EngineComponents.cpp`

エンジンの型の保存項目。**エンジンの Component に項目を足したら、ここにも1行足す**（足し忘れると、その項目は保存されず、Stop で初期値に戻る）。

```cpp
#include "MyEngine/Component/ComponentSerializer.h"

#include <algorithm>
#include <cstdint>

#include "MyEngine/Entity/ModelRendererComponent.h"
#include "MyEngine/Entity/TransformComponent.h"
#include "MyEngine/Light/LightComponent.h"

// エンジンのComponentの「保存する項目」。書き方は COMPONENT(...) の中身と同じ
// Inspectorは Editor/Inspector の手書きのEditorのまま（度で見せる・Resetボタンなど、細かく作り込んでいるため）
// ここは保存の名前を決めるだけなので、ラベルはメンバ変数の名前にそろえる
// ★ Componentに項目を足したら、ここにも1行足す（足し忘れると、その項目は保存されず、Stopで初期値に戻る）

namespace {
//=============================================================================
// Transform
//=============================================================================
void DescribeTransform(ComponentUI& ui, TransformComponent& value) {
	ui.Field("translation", value.translation);
	ui.Field("rotation", value.rotation); // ラジアン（X→Y→Zの順）。度に直して書くと、保存するたびに誤差が乗る
	ui.Field("scale", value.scale);
	// worldMatrix は毎フレーム計算する結果なので書かない
}

//=============================================================================
// ModelRenderer
//=============================================================================
// 0xRRGGBBAA → 0〜1の4つ（色の Field は Vector4 なので、保存するときだけ写す）
Vector4 UnpackColor(uint32_t rgba) {
	return {
	    static_cast<float>((rgba >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(rgba & 0xFF) / 255.0f,
	};
}

// 0〜1の4つ → 0xRRGGBBAA（8bitなので、写して戻しても値は変わらない）
uint32_t PackColor(const Vector4& color) {
	// ファイルを手で書き換えて1を超えても、隣の色にはみ出さないように0〜1に収める
	const auto toByte = [](float value) { return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return (toByte(color.x) << 24) | (toByte(color.y) << 16) | (toByte(color.z) << 8) | toByte(color.w);
}

void DescribeModelRenderer(ComponentUI& ui, ModelRendererComponent& value) {
	ui.Field("enabled", value.enabled);
	// --- 使うファイル（パスで書く。番号は起動するたびに変わる）---
	ui.AssetField("model", value.modelHandle, AssetType::Model);
	ui.AssetField("texture", value.textureHandle, AssetType::Texture); // 未選択＝モデルのマテリアルのテクスチャ
	// --- 描画の設定（enumは名前で書く）---
	ui.Field("shadingType", value.shadingType);
	ui.Field("blendMode", value.blendMode);
	ui.Field("rasterizerType", value.rasterizerType);
	ui.Field("depthMode", value.depthMode);
	ui.Field("billboard", value.billboard);
	// --- 色 ---
	Vector4 color = UnpackColor(value.color);
	ui.ColorField("color", color);
	value.color = PackColor(color);
	// --- マテリアル ---
	ui.ColorField("ambient", value.material.ambient);
	ui.ColorField("diffuse", value.material.diffuse);
	ui.ColorField("specular", value.material.specular);
	ui.ColorField("emissive", value.material.emissive);
	ui.Field("shininess", value.material.shininess);
	ui.Field("metallic", value.material.metallic);
	ui.Field("roughness", value.material.roughness);
	ui.Field("alphaCutoff", value.material.alphaCutoff);
	// --- UV ---
	ui.Field("uvTranslation", value.uvTransform.translation);
	ui.Field("uvRotation", value.uvTransform.rotation);
	ui.Field("uvScale", value.uvTransform.scale);
}

//=============================================================================
// ライト
//=============================================================================
// ギズモの表示（3種類で共通）
void DescribeGizmo(ComponentUI& ui, LightGizmoFlags& gizmo) {
	ui.Field("showIcon", gizmo.showIcon);
	ui.Field("showRange", gizmo.showRange);
}

void DescribeDirectionalLight(ComponentUI& ui, DirectionalLightComponent& value) {
	ui.ColorField("color", value.color); // 見た目の色（sRGB）。リニアへの変換は LightSystem が集めるときに行う
	ui.Field("intensity", value.intensity);
	DescribeGizmo(ui, value.gizmo);
}

void DescribePointLight(ComponentUI& ui, PointLightComponent& value) {
	ui.ColorField("color", value.color);
	ui.Field("intensity", value.intensity);
	ui.Field("radius", value.radius);
	ui.Field("decay", value.decay);
	DescribeGizmo(ui, value.gizmo);
}

void DescribeSpotLight(ComponentUI& ui, SpotLightComponent& value) {
	ui.ColorField("color", value.color);
	ui.Field("intensity", value.intensity);
	ui.Field("range", value.range);
	ui.Field("decay", value.decay);
	ui.Field("outerAngle", value.outerAngle); // 度
	ui.Field("innerAngle", value.innerAngle); // 度
	DescribeGizmo(ui, value.gizmo);
}
} // namespace


//=============================================================================
// エンジンのComponentを登録する（Engine::Initialize から1回。Releaseでも呼ぶ）
// 名前はInspectorの見出しと同じにしておく（ファイルを開いたときに、どのComponentか分かるように）
//=============================================================================
void ComponentSerializer::RegisterEngineComponents() {
	Register<TransformComponent>("Transform", &DescribeTransform);
	Register<ModelRendererComponent>("Model Renderer", &DescribeModelRenderer);
	Register<DirectionalLightComponent>("Directional Light", &DescribeDirectionalLight);
	Register<PointLightComponent>("Point Light", &DescribePointLight);
	Register<SpotLightComponent>("Spot Light", &DescribeSpotLight);
}
```

#### C-4. `MyEngine/Component/GameComponent.h`（2か所）

(1) インクルードに1行足す。

```cpp
#include "MyEngine/Component/ComponentSerializer.h"
#include "MyEngine/Component/ComponentUI.h"
```

(2) `ComponentRegistrar` のコンストラクタの中、`RegisterComponent<T>()` の下に1行足す。

```cpp
		ComponentRegistryDetail::AddPending([name, subCategory, describe]() {
			EntityManager::RegisterComponent<T>();                                                                                               // 置き場所
			ComponentSerializer::Register<T>(name, describe);                                                                                    // 保存・読み込み（Releaseでも要る）
			ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<T>>(name, MakeGameplayCategory(subCategory), describe)); // Inspector
		});
```

#### C-5. `MyEngine/Engine.cpp`（2か所）

(1) インクルード（`// Component` の所）に1行足す。

```cpp
// Component
#include "MyEngine/Component/ComponentSerializer.h"
#include "MyEngine/Component/GameComponent.h"
```

(2) `Initialize` の中、`LightSystem::Initialize();` のすぐ下に1行足す。

```cpp
	LightSystem::Initialize(); // ライトのComponentをEntityManagerに登録するので、EntityManagerの後
	ComponentSerializer::RegisterEngineComponents(); // Transform・ModelRenderer・ライトの保存項目（Releaseでもシーンを読むのに要る）
	GameComponentRegistry::ApplyAll(); // COMPONENT(...) で書いたゲーム固有Componentを登録（登録待ちの表を空にする）
```

最初のシーンは、この後の `windowManager_.AddWindow` の中で作られる（そこでシーンファイルを読む）。だから型の登録はそれより前にする。

---

### D. シーン全体の保存・読み込み

#### D-1. 新規：`MyEngine/Scene/SceneSerializer.h`

```cpp
#pragma once
#include <string>

// シーンファイルを読んだ結果
enum class SceneLoadResult {
	Loaded,   // 読めた
	NotFound, // ファイルが無い（まだ一度も保存していないシーン）
	Failed,   // ファイルはあるが読めない（壊れている・新しい版のエンジンで保存された）
};


/// <summary>
/// 全Entity（名前・有効・親子・Component）をJSONにする / JSONから作る
/// <para>Componentの中身は ComponentSerializer に登録した型だけが書かれる（COMPONENT(...) の型とエンジンの型は全部入っている）</para>
/// <para>読み込みはフレームの境目（SceneManager::Update の頭）でだけ行う。今あるEntityは消さないので、先に DestroyAllNow を呼ぶ</para>
/// </summary>
class SceneSerializer {
public:
	// ファイルの形の版。値の意味を変えたら（回転をクオータニオンにする等）上げて、古い版を読み替える処理を足す
	static constexpr int kVersion = 1;

	// ===== ファイル =====
	// フォルダが無ければ作る。前のファイルは「〇〇.bak」として1つだけ残す
	static bool SaveFile(const std::string& path);
	static SceneLoadResult LoadFile(const std::string& path);

	// ===== 文字列（Playを押した瞬間の退避に使う。ファイルと同じ中身）=====
	static std::string SaveToText();
	static bool LoadFromText(const std::string& text);

	// 全Entityを今すぐ消す（予約ではない。同じEntityIdで作り直すため）。フレームの境目でだけ呼ぶ
	static void DestroyAllNow();
};
```

#### D-2. 新規：`MyEngine/Scene/SceneSerializer.cpp`

```cpp
#include "SceneSerializer.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

#include <externals/nlohmann/json.hpp>

#include "MyEngine/Component/ComponentSerializer.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
// 集めた警告を1回ずつ出す（同じComponentを100個持っていても、同じ文は1回だけ）
void FlushWarnings(const std::set<std::string>& warnings) {
	for (const std::string& warning : warnings) {
		LogManager::Warning(warning);
	}
}

//=============================================================================
// 全Entity → JSON
//=============================================================================
SceneJson SaveAll() {
	std::set<std::string> warnings;
	SceneJson entities = SceneJson::array();
	// 並んでいる順（Hierarchyに出る順）のまま書く。読むときも同じ順に作るので、並びが変わらない
	for (const Entity& entity : EntityManager::GetAll()) {
		const Entity* parent = EntityManager::Get(entity.parent);
		SceneJson components = SceneJson::object();
		ComponentSerializer::Save(entity.self, components, warnings);

		// 1つのEntityの中身は手元で組み立ててから入れる（ordered_map は途中で参照を持つと壊れるため）
		SceneJson item = SceneJson::object();
		item["id"] = entity.id;
		item["name"] = entity.name;
		item["parent"] = parent ? parent->id : EntityId{0}; // 親は EntityId で書く（0は親なし）
		item["active"] = entity.isActive;
		item["components"] = std::move(components);
		entities.push_back(std::move(item));
	}
	FlushWarnings(warnings);

	SceneJson root = SceneJson::object();
	root["version"] = SceneSerializer::kVersion;
	root["entities"] = std::move(entities);
	return root;
}

//=============================================================================
// JSON → Entity（型が違う値を value() で読むと例外が出るので、呼ぶ側で受ける）
//=============================================================================
bool LoadAll(const SceneJson& root) {
	// --- 版を確かめる（新しい版は、どこが変わったか分からないので読まない）---
	const int version = root.value("version", 0);
	if (version < 1 || version > SceneSerializer::kVersion) {
		LogManager::Error(std::format("このエンジンでは読めない版です（ファイル: {} / エンジン: {}）", version, SceneSerializer::kVersion));
		return false;
	}
	// 版を上げたら、ここで古い版のJSONを今の形に直してから読む（例: if (version < 2) { 回転をクオータニオンに直す }）
	const auto entitiesIt = root.find("entities");
	if (entitiesIt == root.end() || !entitiesIt->is_array()) {
		LogManager::Error("\"entities\" がありません");
		return false;
	}
	const SceneJson& entities = *entitiesIt;
	std::set<std::string> warnings;

	// ===== 1周目：Entityを作る（親はまだ付けない。ファイルの並びが崩れていても困らないように、全部そろってから付ける）=====
	std::vector<Handle<Entity>> created(entities.size()); // 作れなかった所は無効なHandleのまま
	for (size_t i = 0; i < entities.size(); ++i) {
		const SceneJson& item = entities[i];
		const EntityId id = item.value("id", EntityId{0});
		const std::string name = item.value("name", std::string("Entity"));
		if (id == 0 || EntityManager::FindById(id).IsValid()) {
			warnings.insert(std::format("EntityIdが0か、2つ目の同じ番号なので飛ばしました: {}", name));
			continue;
		}
		created[i] = EntityManager::CreateWithId(id, name, {});
		EntityManager::Get(created[i])->isActive = item.value("active", true);
	}

	// ===== 2周目：親子をつなぐ =====
	for (size_t i = 0; i < entities.size(); ++i) {
		const EntityId parentId = entities[i].value("parent", EntityId{0});
		if (!created[i].IsValid() || parentId == 0) {
			continue;
		}
		const Handle<Entity> parent = EntityManager::FindById(parentId);
		if (!parent.IsValid()) {
			warnings.insert(std::format("親の番号 {} が見つからないので、一番上に置きました", parentId));
			continue;
		}
		EntityManager::SetParent(created[i], parent); // 輪になる親子は SetParent が断る
	}

	// ===== 3周目：Componentを付ける（全Entityがそろった後なので、Componentの中のEntityへの参照も引ける）=====
	for (size_t i = 0; i < entities.size(); ++i) {
		const auto componentsIt = entities[i].find("components");
		if (!created[i].IsValid() || componentsIt == entities[i].end() || !componentsIt->is_object()) {
			continue;
		}
		ComponentSerializer::Load(created[i], *componentsIt, warnings);
	}
	FlushWarnings(warnings);
	return true;
}

// 読み込みの入口。失敗したら作りかけのEntityを消して、何も無かったことにする
bool LoadSafely(const SceneJson& root) {
	try {
		if (LoadAll(root)) {
			return true;
		}
	} catch (const SceneJson::exception& exception) {
		// "id": "abc" のように、形の違う値があったとき
		LogManager::Error(std::format("シーンの読み込みに失敗しました: {}", exception.what()));
	}
	SceneSerializer::DestroyAllNow();
	return false;
}

// 1つの値を1行の文字列にする（名前に壊れた文字が混じっていても、例外で落とさず置き換える）
std::string ToLine(const SceneJson& json) { return json.dump(-1, ' ', false, SceneJson::error_handler_t::replace); }

// ファイル用に、人が読める形で書く。タブで字下げし、数だけの配列（Vector3 など）は1行にまとめる
// （nlohmann の dump で字下げすると1要素1行になり、Transform1つで15行になってしまう）
void WriteReadable(const SceneJson& json, int depth, std::string& out) {
	const std::string indent(static_cast<size_t>(depth) + 1, '\t'); // 中身の字下げ
	const std::string closeIndent(static_cast<size_t>(depth), '\t'); // 閉じかっこの字下げ
	const bool isNumberArray = json.is_array() && std::all_of(json.begin(), json.end(), [](const SceneJson& element) { return element.is_number(); });

	if (json.is_object() && !json.empty()) {
		// --- { "キー": 値, ... } は1項目1行 ---
		out += "{\n";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += indent + ToLine(SceneJson(it.key())) + ": ";
			WriteReadable(*it, depth + 1, out);
			out += (std::next(it) == json.end()) ? "\n" : ",\n";
		}
		out += closeIndent + "}";
	} else if (isNumberArray) {
		// --- [1.0, 2.0, 3.0] は1行 ---
		out += "[";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += ToLine(*it);
			out += (std::next(it) == json.end()) ? "" : ", ";
		}
		out += "]";
	} else if (json.is_array() && !json.empty()) {
		// --- それ以外の配列（Entityの並びなど）は1要素ずつ ---
		out += "[\n";
		for (auto it = json.begin(); it != json.end(); ++it) {
			out += indent;
			WriteReadable(*it, depth + 1, out);
			out += (std::next(it) == json.end()) ? "\n" : ",\n";
		}
		out += closeIndent + "]";
	} else {
		out += ToLine(json); // 数・文字・true/false・空の {} []
	}
}
} // namespace


//=============================================================================
// ファイル
//=============================================================================
bool SceneSerializer::SaveFile(const std::string& path) {
	const std::filesystem::path filePath(path);
	std::error_code error; // 例外ではなくエラーコードで受ける
	std::filesystem::create_directories(filePath.parent_path(), error);
	// 前のファイルを1つだけ残す（書いている途中で落ちても、1つ前には戻れるように）
	if (std::filesystem::exists(filePath, error)) {
		std::filesystem::copy_file(filePath, filePath.string() + ".bak", std::filesystem::copy_options::overwrite_existing, error);
	}
	std::ofstream stream(filePath);
	if (!stream) {
		LogManager::Error(std::format("シーンファイルを開けませんでした: {}", path));
		return false;
	}
	std::string text;
	WriteReadable(SaveAll(), 0, text); // gitの差分で見やすいように、タブで字下げして書く
	stream << text << '\n';
	stream.close();
	if (stream.fail()) {
		LogManager::Error(std::format("シーンファイルを書き切れませんでした（1つ前は .bak にあります）: {}", path));
		return false;
	}
	LogManager::Log(std::format("シーンを保存しました: {}", path));
	return true;
}

SceneLoadResult SceneSerializer::LoadFile(const std::string& path) {
	std::error_code error;
	if (!std::filesystem::exists(path, error)) {
		return SceneLoadResult::NotFound;
	}
	std::ifstream stream(path);
	const SceneJson root = SceneJson::parse(stream, nullptr, false); // false: 壊れていても例外を投げず、discarded を返す
	stream.close();
	const bool isLoaded = !root.is_discarded() && root.is_object() && LoadSafely(root);
	if (!isLoaded) {
		// 読めなかったファイルを別の名前で残す（このまま Save を2回押すと、本体も .bak も空のシーンで上書きされてしまうので）
		std::filesystem::copy_file(path, path + ".broken", std::filesystem::copy_options::overwrite_existing, error);
		LogManager::Error(std::format("シーンファイルを読めませんでした（壊れている・新しい版で保存された）。{}.broken に写しました", path));
		return SceneLoadResult::Failed;
	}
	LogManager::Log(std::format("シーンを読み込みました: {}（{}個）", path, EntityManager::GetCount()));
	return SceneLoadResult::Loaded;
}

//=============================================================================
// 文字列
//=============================================================================
std::string SceneSerializer::SaveToText() { return ToLine(SaveAll()); } // 1行に詰める（人が読まないので）

bool SceneSerializer::LoadFromText(const std::string& text) {
	const SceneJson root = SceneJson::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		LogManager::Error("退避したシーンが読めませんでした");
		return false;
	}
	return LoadSafely(root);
}

//=============================================================================
// 全Entityを今すぐ消す
//=============================================================================
void SceneSerializer::DestroyAllNow() {
	std::vector<Handle<Entity>> roots;
	EntityManager::GetRoots(roots);
	for (Handle<Entity> root : roots) {
		EntityManager::Destroy(root); // 子も一緒に消える
	}
	EntityManager::FlushDestroy(); // 予約をその場で反映する（フレームの境目なので安全。EditorHistoryのUndoと同じ使い方）
}
```

**読み方のポイント**

- `SceneJson` の `value("id", EntityId{0})` は「キーが無ければ既定値」だが、**キーがあって型が違うと例外を投げる**（`"id": "abc"` など）。なので読み込み全体を `LoadSafely` の `try` で包み、失敗したら作りかけの Entity を消して「何も無かった」ことにする。
- ordered_map（キーを書いた順に並べる表）は中身が vector なので、`json["a"]` の参照を持ったまま `json["b"]` を足すと参照が壊れる。だから1つの Entity の中身は手元の変数で組み立ててから入れている。
- `DestroyAllNow` は `Destroy`（予約）の後にすぐ `FlushDestroy` を呼ぶ。同じ EntityId で作り直すので、フレームの最後まで待てない（`CreateWithId` は使用中の番号だと assert で止まる）。呼んでよいのはフレームの境目だけ（Undo の `Remove` と同じ使い方）。

---

### E. Play / Stop / Save / シーン切り替え

#### E-1. `MyEngine/Scene/IScene.h`（ファイル全体を差し替え）

足したのは `GetSceneFile` と `CreateDefaultEntities` の2つと、コメント。

```cpp
#pragma once
#include <string>
#include <memory>
#include <functional>

// 前方宣言
class Camera;

/// <summary>
/// シーン基底クラス
/// <para>シーンファイルを使うシーン（GetSceneFile が nullptr 以外）では、Entityはエンジンが作って消す。
/// 呼ばれる順番は「前のシーンの Finalize → 全Entityを消す → Entityを作る（Playの退避 / シーンファイル / CreateDefaultEntities）→ Initialize」</para>
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
	virtual Camera* GetCamera() { return nullptr; }
	virtual std::unique_ptr<IScene> NextScene() { return nullptr; }

	// ===== シーンファイル（Docs/Tasks/Serialize.md）=====
	// Entityを保存するファイル（例: "resources/scenes/GameScene.scene.json"）
	// nullptr ならファイルを使わない（今まで通り、Initializeで作ってFinalizeで消す）
	virtual const char* GetSceneFile() const { return nullptr; }
	// シーンファイルがまだ無いときだけ、Initialize の前に呼ばれる。最初の配置をコードで作る
	// （Saveするとファイルができるので、次からは呼ばれない。ファイルを消すと、またこの配置から始まる）
	// ここで作ったEntityのHandleをメンバに覚えない（Play・Stop・次の起動ではこの関数が呼ばれず、Handleも変わる）。
	// 覚えたいときは Initialize の中で EntityManager::FindByName などで探す
	virtual void CreateDefaultEntities() {}

	// WindowManagerが自動でセットする
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }
	const std::wstring& GetWindowTitle() const { return windowTitle_; }

protected:
	std::wstring windowTitle_ = L"Title";
};

// シーンの作り方。Stop / Restart で作り直すときにも使う
using SceneFactory = std::function<std::unique_ptr<IScene>()>;
```

#### E-2. `MyEngine/Scene/SceneManager.h`（ファイル全体を差し替え）

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
/// </summary>
class SceneManager {
public:
	void Initialize();
	void Update();
	void Draw();
	void Finalize(); // アプリの終了時に1回。今のシーンの Finalize を呼ぶ

	// ===== 再生コントロール =====
	void Play();  // 停止中なら、今のEntityを退避してから再生
	void Pause(); // 一時停止 / 再開
	void Stop();  // 停止して、Playを押した瞬間の状態へ戻す
	// 作り直す。再生中はPlayを押した瞬間の状態から、停止中はシーンファイルから（保存していない編集は消える）
	void RequestReload() { isReloadRequested_ = true; }
	// シーンファイルに保存する（停止中だけ。実際に書くのは次のフレームの頭）
	void RequestSave() { isSaveRequested_ = true; }

	// ===== ゲッター =====
	IScene* GetCurrentScene() { return currentScene_.get(); }
	PlayState GetPlayState() const { return playState_; }
	const char* GetSceneFile() const { return currentScene_ ? currentScene_->GetSceneFile() : nullptr; } // シーンファイルを使わないならnullptr

	// ===== セッター =====
	void SetScene(std::unique_ptr<IScene> currentScene) { currentScene_ = std::move(currentScene); }
	void RequestNextScene(std::unique_ptr<IScene> next) { nextScene_ = std::move(next); } // 次のフレームの頭で切り替える
	void SetSceneFactory(std::function<std::unique_ptr<IScene>()> factory) { sceneFactory_ = std::move(factory); }
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }

private:
	// 最初のシーンを作り直す（Play・Stop・Restart）
	void ReloadImmediate();
	// シーンを入れ替える。useSnapshot なら、Playを押した瞬間に退避したEntityを使う
	void ChangeScene(std::unique_ptr<IScene> next, bool useSnapshot);
	// 新しいシーンのEntityを作る（退避 → シーンファイル → CreateDefaultEntities の順に探す）
	void LoadEntities(bool useSnapshot);
	// シーンファイルに書く
	void SaveImmediate();

	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> nextScene_; // 次のフレームの頭で切り替えるシーン
	SceneFactory sceneFactory_;
	std::wstring windowTitle_;
#ifdef USE_IMGUI
	PlayState playState_ = PlayState::Editing; // エディタでは停止状態から始める
#else
	PlayState playState_ = PlayState::Playing; // Release は即座に動かす
#endif
	bool isReloadRequested_ = false;
	bool isSnapshotRequested_ = false; // 次に作り直す前に、今のEntityを退避する（Playを押した）
	bool isSaveRequested_ = false;
	std::string playSnapshot_;         // Playを押した瞬間の全Entity（シーンファイルと同じJSON）。空なら退避なし
};
```

#### E-3. `MyEngine/Scene/SceneManager.cpp`（ファイル全体を差し替え）

```cpp
#include "SceneManager.h"

#include <format>
#include <functional>

#include "MyEngine/Component/GameComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/Time/Time.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#endif

void SceneManager::Initialize() {
	MY_ASSERT_MSG(sceneFactory_ != nullptr, "SetSceneFactory()でシーンの作り方を登録してください");
	ChangeScene(sceneFactory_(), false);
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");

	// ===== フレームの頭：Entityを作り直す・保存するのはここだけ =====
	// 前のフレームの予約（Componentの追加・Undoなど）は反映済みで、まだ誰も ForEach で回していない
	if (isReloadRequested_) {
		isReloadRequested_ = false;
		ReloadImmediate();
	}
	if (nextScene_) {
		ChangeScene(std::move(nextScene_), false); // 前のフレームで NextScene() が返したシーン
	}
	// 保存は作り直しの後（Stop と Save を同じフレームで頼まれても、戻した後の状態を書く）
	if (isSaveRequested_) {
		isSaveRequested_ = false;
		SaveImmediate();
	}

	// 停止中・一時停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ == PlayState::Editing) {
		return;
	}
	// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
	Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
	currentScene_->Update();
	GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）

	// シーン遷移は予約だけ（Entityの入れ替えは、次のフレームの頭で行う）
	if (std::unique_ptr<IScene> next = currentScene_->NextScene()) {
		nextScene_ = std::move(next);
	}
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
}

void SceneManager::Pause() {
	if (playState_ == PlayState::Playing) {
		playState_ = PlayState::Paused;
	} else if (playState_ == PlayState::Paused) {
		playState_ = PlayState::Playing;
	}
}

void SceneManager::Stop() {
	if (playState_ == PlayState::Editing) {
		return; // 停止中に押しても作り直さない（保存していない編集を消さないように）
	}
	playState_ = PlayState::Editing;
	RequestReload(); // Playを押した瞬間の状態へ戻す
}

//======================================================================================================
// 作り直す・入れ替える（フレームの頭でだけ呼ぶ）
//======================================================================================================
void SceneManager::ReloadImmediate() {
	// Playを押した直後なら、作り直す前に今のEntityを退避する（フレームの頭なので、見えている通りに写せる）
	if (isSnapshotRequested_) {
		isSnapshotRequested_ = false;
		if (GetSceneFile() != nullptr) {
			playSnapshot_ = SceneSerializer::SaveToText();
		}
	}
	// 再生中に頼まれたシーン遷移は捨てる（最初のシーンに戻るので、その遷移はもう関係ない）
	// これが残っていると、Stopを押したフレームにそのまま次のシーンへ飛んでしまう
	nextScene_.reset();
	// Stop・Restartで最初のシーンに戻る（タイトル → ゲームと切り替わった後でも、Playを押したシーンへ戻る）
	ChangeScene(sceneFactory_(), true);
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
		LogManager::Log(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります: {}", file));
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

**読み方のポイント**

- 形を変える処理（保存・作り直し・切り替え）は全部 `Update` の頭に集めた。ここは `FlushComponentChanges` と `EditorHistory::Flush` の直後なので、UI で頼んだ操作（Add Component・Undo など）が全部反映済みで、まだ誰も `ForEach` で回していない。
- `ChangeScene` の中の `FlushComponentChanges` は、`CreateDefaultEntities` で `RequestAdd`（予約）した Component を、`Initialize` を呼ぶ前に付けておくため。これで `Initialize` の中で `Get<T>` が取れる。
- 保存を予約（`RequestSave`）にしたのも同じ理由。Ctrl+D と Ctrl+S を同じフレームで押しても、複製した物まで保存される。保存は作り直しの**後**に行うので、Stop と Ctrl+S が同じフレームでも、戻した後の状態が書かれる。
- `Initialize` の前後で Entity の数を比べ、増えていたら警告する（シーンファイルを使うシーンの `Initialize` で Entity を作ると、Play / Stop のたびに増えるため）。
- `NextScene()` が返したシーンは `nextScene_` に入れておき、次のフレームの頭で切り替える（切り替えも Entity を全部作り直すので、フレームの頭にそろえる）。Stop すると、切り替わった後でも **Play を押したシーンの、押した瞬間の状態**へ戻る（Unity と同じ）。
- 作り直す（Stop・Restart）ときは、`nextScene_` に溜まっている切り替えの予約を捨てる（`ReloadImmediate` の `nextScene_.reset()`）。Play 中の最後のフレームでシーン遷移が決まった直後に Stop を押すと、戻したその同じフレームで次のシーンへ飛んでしまうため。
- `Finalize()` は今まで宣言だけで中身が無く、シーンの `Finalize` はアプリを閉じても一度も呼ばれていなかった。E-4 の (2) でウィンドウを閉じたときに呼ぶ。

#### E-4. `MyEngine/Window/WindowManager.cpp`（3か所）

(1) `AddWindow` の最後にある次の部分を、**その直前の空行1行も含めて消す**。`sceneFactory` は上で `SetSceneFactory(std::move(sceneFactory))` に渡した後なので空になっていて、この `if` は一度も通っていなかった（もし通ると、シーンを2つ作って1つ目を片付けずに捨てる）。

```cpp
	if (sceneFactory) {
		auto scene = sceneFactory();
		scene->SetWindowTitle(config.title);
		scene->Initialize();
		windows_.back().sceneManager->SetScene(std::move(scene));
	} else {
		LogManager::Log("window.get()->GetTitle()ウィンドウでシーンの指定なし");
	}
```

代わりにコメントを1行残しておく。

```cpp
	LogManager::Log(std::format("AddWindow: title={} hwnd={}", ConvertString(config.title), (void*)windows_.back().window->GetHWND()));
	// シーンは上の sceneManager->Initialize() が作っている（ここでもう1つ作らない）
}
```

(2) `ProcessMessage` の中、閉じたウィンドウを消す所で、先にシーンの `Finalize` を呼ぶ。×で閉じると、ここで `SceneManager` ごと消えて main のループが終わるので、ここで呼ばないと一度も呼ばれない。

```cpp
		if (!it->window->ProcessMessage()) {
			// ウィンドウが閉じられたら、シーンの後片付けをしてから削除（×で閉じるのが普通の終わり方なので、ここで呼ばないとFinalizeが一度も走らない）
			if (it->sceneManager) {
				it->sceneManager->Finalize();
			}
			it = windows_.erase(it);
			continue;
		}
```

(3) `DrawPlayToolbar` の、`Restart` ボタンの1行上にある `ImGui::SameLine();` から、関数の最後の `}` までを差し替える。

```cpp
	ImGui::SameLine();
	// 停止中の作り直しは「シーンファイルから読み直す」になり、保存していない編集が消えるので押せなくする
	ImGui::BeginDisabled(state == PlayState::Editing);
	if (ImGui::Button("Restart")) {
		sceneManager->RequestReload(); // 再生中：Playを押した瞬間の状態からやり直す
	}
	ImGui::EndDisabled();

	// --- シーンの保存（停止中だけ。Ctrl+S でも）---
	const char* sceneFile = sceneManager->GetSceneFile();
	const bool canSave = (state == PlayState::Editing) && (sceneFile != nullptr);
	ImGui::SameLine();
	ImGui::BeginDisabled(!canSave);
	if (ImGui::Button("Save")) {
		sceneManager->RequestSave();
	}
	ImGui::EndDisabled();
	// 文字の入力中は、入力欄のほうを優先する（名前の変更中に Ctrl+S を押しても保存しない）
	const bool isTyping = ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive();
	if (canSave && !isTyping && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
		sceneManager->RequestSave();
	}

	const char* label = (state == PlayState::Playing) ? "Playing" : (state == PlayState::Paused) ? "Paused" : "Editing";
	ImGui::SameLine();
	ImGui::Text("   [%s]", label);
	// どのファイルに保存されるか
	ImGui::TextDisabled("Scene: %s", sceneFile ? sceneFile : "(シーンファイルを使っていない)");
	ImGui::End();
}
```

---

### F. ゲーム側：`GameScene` をシーンファイルに対応させる

`Initialize` の中の Entity を作るコードを `CreateDefaultEntities` へ移し、`sceneRoot_` をやめる。**カメラ・IBL・Stage・Particles は Entity ではないので、今までどおり `Initialize` で作る**（保存されない。Entity にするのは後の作業）。

#### F-1. `CG3_Project/GameScene.h`

(1) 使わなくなるインクルードを2行消す。

```cpp
#include <MyEngine/Core/Handle.h>
#include <MyEngine/Entity/Entity.h>
```

(2) `GetCamera` の下に足す。

```cpp
	Camera* GetCamera() override { return camera_; }

	// シーンファイル（Entityはここに保存され、次からはここから作られる）
	const char* GetSceneFile() const override { return "resources/scenes/GameScene.scene.json"; }
	// シーンファイルがまだ無いときだけ呼ばれる最初の配置
	void CreateDefaultEntities() override;
```

(3) 一番下の `sceneRoot_` とそのコメントを消す。

```cpp
	// このシーンが作ったEntityをまとめる親。Finalizeでこれを消すと、子も全部消える
	Handle<Entity> sceneRoot_;
```

#### F-2. `CG3_Project/GameScene.cpp`

`// 終了処理` の見出しから `Initialize` の終わり（`GlobalVariables::GetInstance()->LoadFiles();` の次の `}`）までを、次で差し替える。

```cpp
//==========================================
// 終了処理
//==========================================
void GameScene::Finalize() {
	// Entityはエンジンが消すので、ここでは Initialize で作った物（Entityではない物）だけを片付ける
	delete camera_;
	delete stage_;
	delete particles_;

	camera_ = nullptr;
	stage_ = nullptr;
	particles_ = nullptr;
}

//==========================================
// 最初の配置（シーンファイルがまだ無いときだけ呼ばれる）
//==========================================
void GameScene::CreateDefaultEntities() {
	// 平行光源（ライトもEntityに付けるComponent。向きはTransformの回転で、+Zの向きに照らす）
	// 置き場所・向きはUnityの最初のシーンと同じ（位置は照らし方に関係なく、ギズモを描く場所なだけ）
	const Handle<Entity> sun = EntityManager::Create("Directional Light");
	TransformComponent* sunTransform = EntityManager::Get<TransformComponent>(sun); // Transformは作った直後から取れる
	sunTransform->translation = {0.0f, 3.0f, 0.0f};
	sunTransform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f};
	EntityManager::RequestAdd<DirectionalLightComponent>(sun);

	// モンスターボール（位置や向きはInspectorで触る）
	ModelRendererComponent monsterBall;
	monsterBall.modelHandle = ModelManager::Load("resources/monsterBall/monsterBall.gltf");
	monsterBall.shadingType = ShadingType::PBR;
	const Handle<Entity> ball = EntityManager::Create("MonsterBall");
	EntityManager::RequestAdd<ModelRendererComponent>(ball, monsterBall);
}

//==========================================
// 初期化（Entityがそろった後に呼ばれる）
//==========================================
void GameScene::Initialize() {
	// カメラ
	camera_ = new Camera();
	camera_->Initialize(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
	camera_->SetTranslation({0.0f, 0.0f, -30.0f});

	// 環境光（IBL）。PBRのモデルはこれが無いと描かれない
	environment_.MakeFromHDR(TextureManager::Load("resources/monsterBall/sky.hdr"));

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
```

- `sceneRoot_`（"GameScene" という親）はやめた。Unity のシーンと同じく、一番上に並ぶ。まとめたいときは Hierarchy で空の Entity を作って、その下にドラッグすればよい（それもファイルに保存される）。
- 初めて起動すると、ファイルが無いので `CreateDefaultEntities` の配置で始まる。Save するとファイルができ、それ以降は `CreateDefaultEntities` は呼ばれない。**ファイルを消すと、またこの配置から始まる。**

---

### G. プロジェクトに追加・ビルド

1. Visual Studio の `MyEngine_v1` プロジェクトに新しい5ファイルを追加する（「既存の項目を追加」）。
   - `MyEngine/Component/ComponentSerializer.h`・`ComponentSerializer.cpp`・`EngineComponents.cpp`（今の `Component` の他のファイルと同じ場所）
   - `MyEngine/Scene/SceneSerializer.h`・`SceneSerializer.cpp`（フィルタは `Scene`）
   - **足し忘れても、エンジンのビルドは通ってしまう**（静的ライブラリなので）。エラーが出るのはゲームのリンクで、`MyEngine_v1.lib(Engine.obj) : error LNK2019: ... ComponentSerializer::RegisterEngineComponents` のように、ゲーム側が悪いように見える。そう出たら、ここを見直す。
2. エンジンをビルド（Debug と Release）。ゲーム側に配るヘッダは、ビルド後の xcopy がフォルダごと拾うので何もしなくてよい。**F（ゲーム側）はエンジンをビルドした後に**写す（それまでは配られたヘッダが古く、`GetSceneFile() const override` に赤線が出る）。
3. ゲームをビルド（Debug と Release）。
4. ゲーム側の git：`resources/scenes/GameScene.scene.json` は**コミットする**（シーンのデータそのもの。提出物にも入れる）。`.bak`（1つ前）と `.broken`（読めなかったときの写し）は `.gitignore` に足す。

```gitignore
# シーンファイルの1つ前（Saveのたびに作られる）と、読めなかったときの写し
*.scene.json.bak
*.scene.json.broken
```

---

## 4. 確認すること

**エディタ（Debug）**

1. 起動する。Log に「シーンファイルが無いので、CreateDefaultEntities() で作ります」と出て、今までと同じ絵（MonsterBall と平行光源）。Hierarchy は「GameScene」の親が無くなり、2つが一番上に並ぶ。
2. MonsterBall を選んで Position を動かし、Add Component → Gameplay → Movement → Spin を付ける。**Play** → 動かした位置で回る（前はここで Spin が消えていた）。
3. **Stop** → Play を押す直前の位置・Spin 付きに戻る。回っていた分は戻る。MonsterBall を選んだままになっている。
4. Hierarchy の＋で Entity を作ってから Play → Play 中にそれを消したり動かしたりする → Stop → 元どおり。Play → Stop を10回繰り返しても Entity の数が増えない。
5. 再生中は Control ウィンドウの **Save** が灰色で押せない。**Restart** は再生中だけ押せて、Play を押した瞬間からやり直す。
6. 停止中に **Ctrl+S**（または Save）→ Log に「シーンを保存しました」。`resources/scenes/GameScene.scene.json` を開くと「2. ファイルの形」のようになっている。Hierarchy で名前を変えている最中の Ctrl+S では保存されない。
7. アプリを閉じて起動し直す → 保存した配置・Spin 付きで始まる。Log に「シーンを読み込みました」と「CreateDefaultEntities() は呼んでいません」。
8. Model Renderer の Inspector に **Texture** の欄が増えている。`uvChecker.png` を選ぶと貼り替わる。`white1x1.png` を選んでも落ちない。`(none)` に戻すとモデルのテクスチャに戻る。Model の欄も今までどおり選べる。
9. もう一度 Save すると `GameScene.scene.json.bak` ができる（1つ前）。
10. **参照欄**：ゲーム側に確認用の Component を1つ作り（下。`CG3_Project` のプロジェクトの `Source\Components` に、Spin.cpp と同じように追加してビルド）、MonsterBall に付ける。Inspector に Target 欄が出ている状態で、**Hierarchy の Directional Light を押したまま Target 欄まで運んで離す** → 欄に「Directional Light」と出る（MonsterBall を選んだまま）。右クリック → Clear で `(none)`。
    入れた状態で Directional Light を選んで Delete → MonsterBall を選び直すと欄が `(missing)` → Ctrl+Z → また「Directional Light」。Play → Stop・Save → 再起動でも同じ相手のまま。
11. Hierarchy で親子にした Entity の矢印を畳んでから Play → Stop → 畳んだ状態のまま（前は別の Entity と入れ替わった）。

```cpp
// CG3_Project/Source/Components/LookTarget.cpp（確認用。消してよい）
#include <MyEngine/Component/GameComponent.h>

struct LookTarget {
	EntityRef target; // 見る相手
};
COMPONENT(LookTarget, "Test") { ui.Field("Target", value.target, Tip("Hierarchy から Entity をドラッグ")); }
```

**Release**

12. Release でビルドして起動 → 同じファイルから、同じ配置で始まる（エディタが無くても読める）。

**わざと壊して確かめる（任意）**

13. `GameScene.scene.json` の `"model"` のパスを無いファイルに書き換えて起動 → 落ちずに Log に「アセットが見つかりません」。MonsterBall は描かれない（Inspector で選び直せる）。
14. Spin の `"Speed (deg/s)"` を `"Speed"` に書き換えて起動 → 「ファイルの項目 "Speed" を読む所がありません」と警告、Spin は初期値。
15. ファイルの途中を消して JSON として壊す → 「シーンファイルを読めませんでした」「空のシーンで始めます」。`GameScene.scene.json.broken` に壊れたままの中身が残っている。直すか `.bak` を戻して起動し直す。
16. `GameScene::Initialize` に `EntityManager::Create("Test");` を1行足して起動 → Log に「Initialize で Entity を作っています」の警告（確かめたら消す）。

---

## 5. 検証したこと（2026-09-21 / レビューと直し 2026-09-24、Claude）

- **コンパイル**：scratchpad にエンジン全体とゲームの写しを作り、このファイルのコードを入れて VS 18（v145）の cl で `/std:c++latest /W4 /utf-8`。エンジン83ファイル・ゲーム（`GameScene`・`Stage`・`Particles`・`main`・`Spin`＋確認用 `Homing`）を Debug（`USE_IMGUI`）/ Release で、**エラー0・新しい警告0**（元からある警告は変わらず）。
- **動作テスト 172項目 全通過**：本物の `EntityManager` / `ComponentSerializer` / `EngineComponents` / `SceneSerializer` / `SceneManager` / `COMPONENT` / `SYSTEM` を使い、モデル・テクスチャの Manager とログだけ差し替えたテストを作って実行した。
  - 保存 → 全部消す → 読み込みで、EntityId・名前・有効・親子・Hierarchy の並び・Transform（float がビットまで同じ）・Model Renderer の全項目・ライト2種を同じ Entity に・ゲーム Component（enum・アセット・`###` のキー・`EntityRef` が同じ相手）が元どおり。もう一度書くと同じ文字列。ファイル経由でも同じ中身。Transform が各 Entity の先頭に書かれる。
  - `EntityRef`：相手を消して同じ EntityId で作り直す（削除の Undo と同じ）と、また同じ相手を指す。相手が居ない間に保存すると `0` で書かれる。ファイルの相手が居なければ外して警告。`FindByName`。
  - 読めなかったファイルは `.broken` に写され、その後 Save を2回しても残る。`Initialize` で Entity を作ると警告。Stop と Save を同じフレームで頼むと、Play 前の状態が保存される。`Finalize` は2回呼んでも1回だけ。
  - 壊れた JSON・新しい版・`"id": "abc"`（例外を受けて作りかけを消す）・知らない Component・知らない項目・形の違う値・知らない選択肢・無いファイル・重複 ID・無い親・子が親より先に書かれた並び、を全部落ちずに読み、警告は同じ文を1回だけ出す。
  - `SceneManager`：ファイルが無い起動で `CreateDefaultEntities`（予約した Component が `Initialize` の時点で付いている）、停止中の編集 → Save → **Play で編集が残る** → Play 中に動かす・作る・消す・SYSTEM が値を変える → 再生中の Save は断る → Restart で Play 時点へ（再生は続く）→ Stop で Play 時点へ（SYSTEM が変えた値も戻る、参照も同じ相手）、保存していない編集も Play → Stop で消えない、NextScene は次のフレームで切り替わり前のシーンの Entity が消える、切り替え後の Stop で最初のシーンの Play 時点へ、`RequestNextScene` が効く、`Finalize` が呼ばれる、作り直した `SceneManager` がファイルから読む（保存していない値は入っていない）。
- **Hierarchy から参照欄へのドラッグ＆ドロップ**：Hierarchy と Inspector の該当コードを写した小さな ImGui（1.92.9）のプログラムを画面無しで動かし、マウスの押す・動かす・離すを入れて確かめた。直す前は「B を押した瞬間に Inspector が B に切り替わり、A の参照欄に落とせない」、直した後は「A を選んだまま B を運べて、A.target = B」、普通のクリックでは今まで通り選ばれる。
- **確かめていないこと**：画面の見た目（Inspector の参照欄・アセット欄の見え方、Control ウィンドウ）、本物のエディタでのマウス操作、IBL などのGPU資源を作り直したときの見た目、複数ウィンドウ。

### レビューで見つけて直したこと

書いたコードを、いったん別の目（写経する人の目・ライフサイクル・読み書き・エディタ・C++）で読み直した。見つけて直した物と、その確かめ方。

| 見つけた物 | 放っておくと | 直し方 | 確かめ方 |
|---|---|---|---|
| Hierarchy は**押した瞬間**に選ぶ | Inspector の参照欄へドラッグしようとすると、運んでいる途中で Inspector の中身が相手に切り替わり、落とす先が消える | 左クリックは「離したとき、ドラッグしていなければ」選ぶ | ImGui（1.92.9）を画面無しで動かし、押す・動かす・離すを入れて、直す前は落とせない・直した後は `A.target = B` になることを確かめた |
| Component が `Handle<Entity>` で相手を覚えていた | Play / Stop・削除の Undo・コピー・再起動のたびに世代が変わり、同じ相手なのに無効になる | `EntityRef`（EntityId で覚える）に変えた | テスト（消して同じ番号で戻すと、また同じ相手） |
| 1x1 の画像（`white1x1.png`）でミップマップ生成が `E_INVALIDARG` | Inspector でその画像を選ぶと assert で止まる | 1段しか作れない画像はミップ無しで読む | 実物の 1x1 png で確認 |
| 読めないファイルを Save で2回上書きできた | 本体も `.bak` も空のシーンになり、元のデータが完全に消える | 読めなかったファイルは `.broken` に写してから空のシーンで始める | テスト（Save を2回しても `.broken` が残る） |
| ×で閉じるとシーンの `Finalize` が一度も呼ばれない | カメラなどが解放されないまま終わる | `ProcessMessage` でウィンドウを消す直前に呼ぶ | コードの通り道を確認（`Finalize` は2回呼んでも1回だけ、はテスト） |
| Stop と Ctrl+S が同じフレーム | 再生中に動いた後の状態がファイルに残る | 保存を作り直しの**後**にした | テスト（`999` がファイルに入らない） |
| Play / Stop で TreeNode の開閉が別の Entity に移る | Handle の番号が作り直しで逆順に振り直されるため | 行の ImGui の ID を EntityId にした | コードで確認 |
| `CreateDefaultEntities` で覚えた `Handle` を使い回していた | 2回目以降（ファイルから読んだとき）は無効 | `FindByName` を足し、`Initialize` で探し直す書き方に直した（数が増えたら警告も出す） | テスト |
| 見つからないアセットの警告が、指している数だけ出る | 同じ文が Log を埋めて、ほかの警告が見えなくなる | 読む係が先に `exists` を見て、ほかの警告と同じ「同じ文は1回だけ」の箱に入れる | テスト（50個の Entity が同じ無いファイルを指しても警告は1回） |
| `NextScene()` の予約が残ったまま Stop できる | シーン遷移が決まった直後に Stop を押すと、戻したその同じフレームで次のシーンへ飛ぶ | 作り直し（Stop・Restart）のときに予約を捨てる | テスト（直しを外すと落ちることも確かめた） |

このファイルのコードは、上の検証を通した写しから**そのまま貼り出している**（1行も手で書き写していない）ので、ここに載っているコードとビルドして動かしたコードは同じ物。

---

## 6. 決めたこと

| 決めたこと | 理由 |
|---|---|
| 次の作業はシリアライズ（S1）。Collider・Audio はその後、Input は Component にしない | 「1. 次に何をやるか」。Play でも編集が消える今の状態では、Inspector で調整する Component を足しても値が残らない |
| 形式は JSON（nlohmann）。1シーン1ファイル、`resources/scenes/<シーン名>.scene.json` | 人が読めて git の差分で見られる。`.scene.json` にしておくと、将来の Project パネルでシーンだと見分けられる |
| JSON は「キーを書いた順」「小数は float」の版（`SceneJson`） | 普通の nlohmann::json はキーを文字順に並べ替え、float を double にして `0.10000000149011612` と書く |
| Component の項目は `ui.Field` の並びそのもの（書く係・読む係を ComponentUI の差し替えで作る） | COMPONENT を書けば保存も済む。項目の一覧が1か所なので足し忘れない |
| 保存の名前は `ui.Field` のラベル。`"表示###名前"` なら `###` の後ろ | ImGui と同じ決まり。表示だけ変えたいときの逃げ道 |
| エンジンの型の保存項目は `EngineComponents.cpp` に別に書く。Inspector は手書きのまま | Transform は度で見せる・Reset・行列の表示、Model Renderer はツリー表示など、ComponentUI で書けない見た目がある。Inspector もこの関数に寄せるのは後で（未解決） |
| 型の名前の表（`ComponentSerializer`）は Editor の表と別に、Runtime 側に置く | Release でもシーンを読む。Editor の表は Debug でしか埋まらない |
| アセットは Component に番号を持ち、保存するときだけパスに直す | Component に文字列は持てない（ARCHITECTURE.md 原則2）。固定長の `char[260]` だと Component が5倍近く大きくなり、Undo の写しも毎回その大きさになる |
| enum は名前で保存 | 途中に選択肢を足しても・並べ替えても読める。数だと黙って別の選択肢になる |
| 別の Entity への参照は `EntityRef`（EntityId だけを持つ）。`Handle<Entity>` は `ui.Field` に渡せない | Handle は作り直すたび（Play/Stop・削除の Undo・再起動）に世代が変わり、同じ相手でも無効になる。EntityId ならどれをまたいでも同じ相手。trivially copyable のままなので Undo のバイト比較もそのまま効く |
| 保存するときに相手が居ない `EntityRef` は 0 で書く。読み込んだ相手が居なければ外して警告 | 番号は再起動で配り直されることがあるので、居ない番号を残すと、いつか別の Entity を指す |
| Hierarchy の左クリックは「離したとき、ドラッグしていなければ」選ぶ。行の ImGui の ID は EntityId | 押した瞬間に選ぶと、Inspector の参照欄へ運ぶ前に Inspector の中身が変わる。Handle の番号は作り直すと逆順に振り直され、開閉状態が別の Entity に移る |
| シーンのクラスが覚える Handle は `Initialize` で探す（`FindByName`） | `CreateDefaultEntities` は最初の1回しか呼ばれず、Handle は Play/Stop のたびに変わる |
| 回転はラジアンのまま保存する | 度に直すと保存するたびに変換の誤差が乗る |
| 読み込みは「作る → 親子 → Component」の3周 | ファイルの並びに頼らない。Component の中の参照も引ける |
| 書いていない項目は初期値。知らない項目・型は警告して飛ばす（同じ文は1回） | 項目を足しても古いファイルが読める。ラベルを変えたことに気づける |
| 無いアセットは0（未選択）にして警告 | `ModelManager::Load` は無いファイルで assert（Release でも abort）。先に `exists` で確かめる |
| 読み込みの途中で例外が出たら、作りかけを全部消して「読めなかった」にする | 半分だけ読めたシーンで保存すると、ファイルが半分になる |
| 保存のたびに1つ前を `.bak` に残す | 書いている途中で落ちても、1つ前には戻れる |
| 読めなかったファイルは `.broken` に写して、空のシーンで始める | そのまま Save を2回押すと、本体も `.bak` も空で上書きされて元のデータが消える（レビューで実際に再現） |
| シーンファイルを使うシーンの `Initialize` で Entity が増えたら警告する | 前の書き方（Initialize で作る）のままだと、Play / Stop のたびに1つずつ増え、Save でファイルにも残る。エンジンは黙っていたので気づけない |
| 保存は作り直し（Play/Stop/Restart・シーン切り替え）の後に行う | Stop と Ctrl+S を同じフレームで頼まれても、Play 中の状態を書かない |
| シーンファイルを使うシーンは、EntityManager の全 Entity を自分の物として扱う（入るときも出るときも全部消す） | `sceneRoot_` の下だけ戻り、一番上の物は戻らない、という食い違いを無くす。Hierarchy で作った物も保存される |
| `GetSceneFile()` を書かないシーンは今までどおり | 前の書き方のシーンを壊さない |
| Entity の出どころは「Play の退避 → ファイル → `CreateDefaultEntities`」の順。そろってから `Initialize` | `Initialize` の中で Entity を探せる（Unity の Awake/Start と同じ順番）。`Initialize` は Entity ではない物（カメラ・IBL）を作る |
| Play を押したら、次のフレームの頭で全 Entity を JSON で退避し、Stop で戻す | Unity と同じ。退避とファイルが同じ書き方なので「Stop で戻る物 ＝ 保存される物」。バイト列の写しだと、Component の中の Handle が作り直しで無効になる |
| Play でも作り直す（退避した物から） | 今と同じ動き。`Initialize` が毎回同じ条件で呼ばれ、退避の読み書きが Play のたびに試される |
| Restart は再生中だけ押せる | 停止中の作り直しは「ファイルから読み直す」になり、保存していない編集が消える |
| 保存は停止中だけ。予約して次のフレームの頭で書く | 再生中の状態をファイルに残さない（Unity と同じ）。UI で頼んだ操作が全部反映されてから写す |
| シーンの切り替えも予約して、次のフレームの頭で | Entity を全部作り直すので、Play/Stop と同じ場所にそろえる。`RequestNextScene` もここで効くようにした |
| Stop は、シーンが切り替わった後でも「Play を押したシーン」の押した瞬間へ戻る | Unity と同じ。`sceneFactory_` は切り替えで変えない |
| 作り直しの前後で、選んでいた Entity を EntityId で選び直す | Stop のたびに選択が外れると、Inspector で値を見比べられない |
| ウィンドウを閉じたとき（`WindowManager::ProcessMessage` で消す直前）にシーンの `Finalize` を呼ぶ | 今まで一度も呼ばれておらず、カメラなどが解放されていなかった。×で閉じると main のループはそのまま終わるので、`Engine::Finalize` で呼ぼうとしても、もうシーンが無い |
| Model Renderer のモデル選択は `AssetField` に一本化し、テクスチャも選べるようにした | 同じ「ファイルを掘って選ぶ」処理を2か所に持たない |
| 1x1 の画像はミップマップを作らずに読む | DirectXTex の `GenerateMipMaps` は1段しか作れない画像で `E_INVALIDARG` を返し、assert で止まっていた。テクスチャを Inspector で選べるようにしたので表に出る |

---

## 7. 未解決・後回し

| 何を | なぜ後か / どうするか |
|---|---|
| カメラ・IBL（HDR のパス）・天球・Stage・Particles の保存 | Entity ではないので保存されない。CameraComponent、シーンの設定（IBL・天球）をファイルの `"settings"` に書く、Stage を ModelRenderer 付きの Entity にする、ParticleEmitterComponent、の順で後から |
| エンジンの型の Inspector も Describe 関数に寄せる | 今は「保存項目」と「Inspector」を別に書いているので、項目を足すと2か所直す。Light と Model Renderer は ComponentUI だけで書けそう。Transform は度の表示とクオータニオン化のときにまとめて |
| 保存していない印（`*`）と、終了・Revert の確認 | EditorHistory の「保存した時点の位置」と比べれば出せる |
| Play をまたいで Undo の履歴を残す | Stop で戻る状態は Play 直前と同じ EntityId・同じ値なので、履歴をしまっておけば戻せる |
| シーンを開く（別のシーンを編集する）・プレハブ | 「名前 → シーンの作り方」の表が要る。プレハブは Entity の部分木の保存＋EntityId の振り直し（Paste と同じ） |
| 知らない Component の中身を取っておく | 今は警告して、次の保存で消える。Component のファイルを一時的にビルドから外すと、そのデータが消える |
| 型の名前・項目の名前の付け替え表 | 今は `###` で項目の表示だけ変えられる。型の名前を変えたくなったら「旧名 → 新名」の表を足す |
| 複数ウィンドウ | EntityManager は1つなので、シーンファイルを使うシーンが全部消すと、別のウィンドウの Entity も消える。今はウィンドウ1つ |
| 日本語を含むパス | エンジン全体で文字コードの扱いがそろっていない（`std::filesystem::path(std::string)` は Shift-JIS として読む）。当面アセットのパスは英数字だけ |
