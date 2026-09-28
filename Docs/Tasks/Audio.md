# Audio：音の Component（S3）と、Component・System のフォルダの整理

## このファイルについて

- Serialize.md の順番の **S3（AudioSourceComponent ＋ System）** と、その前にやる **Component・System のフォルダの整理**、それから「置き場所が決まっている物は、ずれるとどうなるか」の答えを書く。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。「なぜ」は「決めたこと」の表へ移す。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階。
- ブランチは `feature/audio`（**Collision をコミットしてから** `feature/collision` の先に切る。手順は A-1）。

| 読む順番 | 何が書いてあるか |
|---|---|
| [1. フォルダの整理](#1-フォルダの整理何をどこに置くか) | 新しい置き場所と、名前で場所が決まる決まり |
| [2. 置き場所がずれるとどうなるか](#2-置き場所が決まっている物がずれるとどうなるか) | 質問への答え。ファイルを表示する仕組み（Projectパネル）で解決するか |
| [3. 音で何が変わるか](#3-音audiosourceで何が変わるか) | できること・ゲームでの書き方・仕組み |
| [4. 手順](#4-手順パート-ad) | 写経するコード（パートA〜D） |
| [5. 確認すること](#5-確認すること) | 写したあとに実行して見ること |
| [7. 決めたこと](#7-決めたこと) | 理由の一覧 |

---

## 進捗

| Step | 作業 | 状態 |
|---|---|---|
| **A** | **フォルダの整理（`ECS/`・`Components/`・`Systems/`）と、ゲームへのヘッダの写し方（robocopy）** | **写経待ち（2026-09-27）** |
| **B** | **AudioSourceComponent ＋ AudioSystem（S3）** | **写経待ち（2026-09-27）** |
| C | 置き場所を覚えなくていいように（Inspector に「Component を書いたファイル」と「アセットを探す場所」を出す） | 写経待ち |
| D | ゲーム側：確認用の音と Component | 写経待ち |

**A を最初に**（B・C のコードは新しい置き場所で書いてある）。B と C はどちらからでもよい。D は B の後。

---

## 1. フォルダの整理：何をどこに置くか

### 今の困りごと

`〜Component` が `Component/` にあったり `Entity/`・`Light/`・`Collision/` にあったりする。しかも `Component/` に入っているのは Component ではなく、Component を書くための仕組み（`ComponentUI`・`GameComponent` など）。`〜System` も `Entity/`・`Light/`・`Collision/` に散らばっている。

原因は「機能ごとのフォルダ（`Light/` にライトの全部）」で始めたのに、Transform・ModelRenderer のように**どの機能にも入らない物**が出てきて、とりあえず `Entity/` に置いたこと。

### 新しい形（パートA）

**名前の終わりで、置き場所が決まる**ようにする。

```
MyEngine/
├ ECS/          Entity と Component の「仕組み」。中身の型は持たない
│                 Entity.h・EntityManager・ComponentStorage          … 入れ物
│                 ComponentUI・ComponentSerializer・GameComponent    … 書き方の道具と保存
├ Components/   Component（データだけ）と、エンジンの Component の保存の項目
│                 TransformComponent.h・ModelRendererComponent.h・LightComponent.h・ColliderComponent.h
│                 AudioSourceComponent.h（パートB）・EngineComponents.cpp
├ Systems/      System（Component を読んで処理する係）
│                 ModelRenderSystem・LightSystem・CollisionSystem・AudioSystem（パートB）
├ Editor/
│   ├ Gizmos/     LightGizmo（エディタで見える線とアイコン）
│   └ Inspector/  TransformEditor など（Inspector の区画。今のまま）
├ Collision/    当たり判定の計算だけ（Collision:: の関数・CollisionProfiler）
├ Sound/        SoundManager（音を読む・鳴らす。Component のことは知らない）
└ Camera/・Graphics/・Scene/・Math/ など     今のまま
```

| 名前 | 置き場所 | 例 |
|---|---|---|
| `〜Component` | `Components/` | `TransformComponent.h`・`AudioSourceComponent.h` |
| `〜System` | `Systems/` | `CollisionSystem`・`AudioSystem` |
| `〜Editor`（Inspector の区画） | `Editor/Inspector/` | `LightEditor` |
| `〜Gizmo`（エディタの線・アイコン） | `Editor/Gizmos/` | `LightGizmo` |
| Entity と Component の仕組み（`Entity`・`Component〜`・`GameComponent`） | `ECS/` | `EntityManager`・`ComponentUI` |
| `〜Manager`（ファイルを読む・実体を持つ） | その分野のフォルダ | `ModelManager`（`Graphics/Model/`）・`SoundManager`（`Sound/`） |

無くなる物：`Entity/`・`Component/`・`Light/` のフォルダと、`Light/LightIncludes.h`（`Engine.cpp` しか使っていなかった）。

### なぜ「機能ごと」ではなく「種類ごと」にしたか

- 機能ごと（`Light/` に LightComponent と LightSystem）もよくある形。でも「どの機能にも入らない物」が必ず出てきて、今回のように迷子になる。
- 種類ごとなら、**名前を見れば場所が決まる**。覚えるのは上の表だけ。Inspector の区画（`〜Editor`）はもう `Editor/Inspector/` に種類で分かれていたので、それにそろえた。
- `Component/`（仕組み）と `Components/`（データ）が並ぶと紛らわしいので、仕組みの方を `ECS/`（Entity・Component・System の略。ARCHITECTURE.md でも使っている言葉）にした。
- 困ること：ライト1つが `Components/`・`Systems/`・`Editor/` の3か所に分かれる。名前の頭（`Light〜`）がそろっているので、VS の「すべて検索」（Ctrl+,）で `Light` と打てば全部出る。

### ゲーム側（`Source/Components/`）は今のまま

ゲームの Component は「**1ファイル＝データ＋見せ方＋処理**」のまま（Entity.md で決めた形）。エンジンと違って分けない。ゲームの Component は小さく、1つ直すときに3つとも触ることが多いので、1ファイルの方が速い。`Source/Components/` は約束で、仕組みの決まりではない（2章）。

### ゲームに配るヘッダの写し方も直す（xcopy → robocopy）

エンジンのビルド後のイベントは、ヘッダを `xcopy` で `CG3_Project/MyEngine/include/` に写している。**xcopy は写すだけで、エンジンから消えたファイルをゲーム側から消さない。** フォルダを動かすと、古い場所のヘッダがゲーム側に残り、ゲームの `#include <MyEngine/Entity/EntityManager.h>` が**古い写しを読んで、エラーにならずにビルドが通ってしまう**（中身は古いまま。後でエンジンを直しても反映されない）。前にもライトの整理で消した `PointLight.h` がゲーム側に残っていて、`GameScene.h` がそれを読み続けていた。

`robocopy /PURGE` にすると、エンジンに無いヘッダはゲーム側からも消える。**古いパスは必ずコンパイルエラー（C1083）になるので、直し忘れに気づける。**

---

## 2. 「置き場所が決まっている物」がずれるとどうなるか

質問への答え。置き場所が決まっている物は4種類あって、**ずれたときの壊れ方が違う**。

| 置き場所が決まっている物 | 誰が決めているか | ずれるとどうなるか | 覚えなくていいようにするには |
|---|---|---|---|
| **ゲームの Component のファイル**（`Source/Components/`） | 誰も決めていない（約束） | **どこに置いても動く。** 登録は `COMPONENT(...)` が起動のときにやるので、フォルダは関係ない。困るのは**プロジェクトに入れ忘れたとき**だけ（コンパイルされないので Add Component に出ない。エラーも出ない） | パートC：Inspector の見出しにマウスを乗せると、**そのComponentを書いたファイル**が出る |
| **アセットを探す場所**（`resources/` と `MyEngine/Resources/`） | `ComponentUI.cpp` の `kAssetSearchRoots`（1か所） | その外に置くと、Inspector の一覧に出ない。**下のフォルダの深さ・名前は自由**（`resources/sounds/se/hit.wav` でもよい） | パートC：一覧の一番上に「探す場所」を出す。場所を変えたいときは `kAssetSearchRoots` の1行を直す |
| **使っているアセットのパス**（scene.json に書かれる） | Save したとき | ファイルを動かす・名前を変えると、次に開いたとき「アセットが見つかりません（未選択にします）」の警告が出て、そのComponentは未選択になる。**そのまま Save すると、そのパスは消える**（1つ前のファイルは `.bak` に残る） | 動かしたら、エディタで選び直す。数が多ければ scene.json はテキストなので、エディタを閉じて検索・置換で直せる |
| **エンジンのヘッダ**（`MyEngine/ECS/EntityManager.h` など） | このファイルのパートA | ゲームの `#include` がコンパイルエラー（C1083）になる。**robocopy にする前は、古い写しを読んでエラーにならなかった**（1章） | 名前で場所が決まる（1章の表）。直し忘れはエラーで分かる |

ほかに、エンジンの中で直に書いているパスが少しある（`MyEngine/Resources/Textures/white.png` など7か所、`MyEngine/Shader/Data`）。これはエンジンの作りなので、ゲームを作るときに覚える必要は無い。また、`COMPONENT(Spin, ...)` の型の名前は scene.json のキーになるので、**型の名前を変えると、前に保存した分は「知らない型」として読み飛ばされる**（8章）。

### 「ファイルを表示する仕組み（Projectパネル）を作れば問題ないのか」

**半分は解決する。**

- **解決すること：どこに何があるかを覚えなくてよくなる。** 見て選ぶ・ドラッグで入れる、ができるので、パスを打つことが無くなる。実は Inspector のアセットの選択（コンボ）は、もう小さな Projectパネルになっている（`resources/` の下を全部掘って出している）。今回それに Sound を足し、一番上に「探す場所」を出す。
- **解決しないこと：エクスプローラでファイルを動かしたとき、パスが切れること。** これは「見えるかどうか」ではなく、**参照をパスで持っている**ことの問題。Unity はこれを2つで解いている。
  1. **エディタの中で動かすと、参照も書き換える**（Projectパネルでドラッグして動かすと、シーンの中のパスも直る）
  2. **ファイルごとに番号（GUID）を振って、参照は番号で持つ**（`.meta` ファイル。エクスプローラで動かしても、`.meta` と一緒なら切れない）

このエンジンでの順番：

1. **今（このファイル）**：名前で場所が決まる規則＋「どこにあるか」を見せる（パートC）。見つからないアセットは警告して未選択にする（S1 の作り）。
2. **Projectパネルを作るとき**（予定の「Projectパネル＋ドラッグ＆ドロップ」）：パネルの中で動かす・名前を変えると、開いているシーンのパスも書き換える（上の 1）。これで「エディタの中で動かせば切れない」になる。
3. **GUID（`.meta`）**：プレハブのように「ファイルからファイルを指す」物が増えて、書き換えきれなくなったら。今はシーンファイル1つなので要らない。

---

## 3. 音（AudioSource）で何が変わるか

### できること

| 操作 | どうなるか |
|---|---|
| Inspector の **Add Component → Audio → Audio Source** | 音源が付く |
| **Sound** | `resources/` と `MyEngine/Resources/` の下の `.wav` / `.mp3` から選ぶ（Model・Texture と同じ選び方） |
| **Volume** / **Pitch** | 鳴っている間に動かすと、その場で変わる。Pitch は 0.5〜2（1オクターブ下〜上） |
| **Loop** | 最後まで鳴ったら頭から（BGM）。鳴っている途中で変えたときは、次に鳴らしたときから効く |
| **Play On Start** | Play を押したとき（Release では起動したとき）に鳴る |
| 停止中（Editing） | **鳴らない**（置いて、設定するだけ） |
| **Pause** / **Resume** | その位置で止まり、続きから鳴る。シーンの `Initialize` で `SoundManager` から直接鳴らした音も止まる（前は Pause しても鳴り続けていた） |
| **Stop** | その場で全部止まる |
| Play / Stop・Restart を繰り返す | シーンの `Initialize` で直接鳴らした BGM も毎回止まる（前は止める人がいなかったので、1往復ごとに1つずつ重なっていった） |
| Entity を**消す**・**無効にする**（親も）・**Audio Source を外す** | その音が止まる |
| Save | パスで保存される（`"sound": "resources/sounds/bgm.wav"`）。Undo も効く |
| ゲームのコード | `AudioSystem::Play(entity)` / `Stop` / `IsPlaying` / `PlayOneShot` |

### ゲームでの書き方

**BGM はコードが要らない。** Entity を作って Audio Source を付け、Sound を選び、Loop と Play On Start に印を付けて Save するだけ。

効果音は、鳴らしたいときに `SYSTEM` の中で呼ぶ（このまま `Source/Components/` に置いてビルドできることを確かめてある）。

```cpp
// 使い方の例（Audio.md・Reference.md に載せるコードのコンパイル確認用）
#include <MyEngine/ECS/GameComponent.h>

// ===== 弾：まっすぐ進む =====
struct Bullet {
	Vector3 velocity = {0.0f, 0.0f, 10.0f}; // 1秒に進む量
};
COMPONENT(Bullet, "Shooter") { ui.Field("Velocity", value.velocity); }

void BulletMove(Bullet& bullet, TransformComponent& transform, float deltaTime) { transform.translation += bullet.velocity * deltaTime; }
SYSTEM(BulletMove);

// ===== 敵：弾が当たると音を鳴らしてHPが減り、0で消える =====
struct Enemy {
	int hp = 3;
};
COMPONENT(Enemy, "Shooter") { ui.Field("HP", value.hp); }

void EnemyHit(Handle<Entity> entity, Enemy& enemy) {
	for (const CollisionHit& hit : CollisionSystem::GetHits(entity)) {
		if (hit.phase != HitPhase::Enter || EntityManager::Get<Bullet>(hit.other) == nullptr) {
			continue;
		}
		EntityManager::Destroy(hit.other); // 弾は消す
		--enemy.hp;
		// 敵に付けた Audio Source の音を重ねて鳴らす（Play On Start は外しておく）。この後で敵を消しても、音は最後まで鳴る
		AudioSystem::PlayOneShot(entity);
	}
	if (enemy.hp <= 0) {
		EntityManager::Destroy(entity);
	}
}
SYSTEM(EnemyHit);

// ===== ゴール：触れたらファンファーレを1回鳴らし、鳴り終わったら次へ =====
struct Goal {
	bool reached = false;  // 触れた
	bool finished = false; // ファンファーレが鳴り終わった
};
COMPONENT(Goal, "Shooter") {}

void GoalUpdate(Handle<Entity> entity, Goal& goal) {
	if (!goal.reached) {
		for (const CollisionHit& hit : CollisionSystem::GetHits(entity)) {
			if (hit.phase == HitPhase::Enter) {
				goal.reached = true;
				AudioSystem::Play(entity); // ゴールに付けた Audio Source（Play On Start は外しておく）
			}
		}
		return;
	}
	// 鳴り終わったら1回だけ（ここでシーンを切り替えるなど）
	if (!goal.finished && !AudioSystem::IsPlaying(entity)) {
		goal.finished = true;
	}
}
SYSTEM(GoalUpdate);
```

- **すぐ消す Entity の音は `PlayOneShot`。** `Play` の音は、その Entity を消すと止まる（敵を倒した音が、倒した瞬間に切れる）。`PlayOneShot` は重ねて鳴らし、持ち主が消えても最後まで鳴る。
- **コードで鳴らす音は、Play On Start の印を外しておく。** 付いていると、Play を押した瞬間にも鳴る。
- **同じ音を何個もの Entity に付けても、読み込みは1回**（`SoundManager` がパスで覚えている）。
- `IsPlaying` は「鳴り終わったか」を知るのに使える（上のゴールの例）。一時停止中は `true`。
- 例の `Bullet`・`Enemy` は Collision.md の例と同じ名前。両方をゲームに入れるなら、片方だけにする（同じ名前の Component は2つ作れない）。
- 相手が何者かは Component で見分ける（Collision.md と同じ）。音を鳴らす係の Entity を別に作ってもよい（`EntityRef` で覚えて `AudioSystem::Play(EntityManager::Find(ref))`）。

### 仕組み（読まなくても写せるが、知っておくと直せる）

#### 1フレームの中の順番

| 順 | 何が起きるか |
|---|---|
| 1 | `SoundManager::CleanupSourceVoices`（鳴り終わった音を片付ける。`Engine::BeginFrame`） |
| 2 | ゲームの `SYSTEM`（ここで `AudioSystem::Play` などを呼ぶ。その場で鳴る） |
| 3 | `EntityManager::FlushDestroy`（`Destroy` の予約を反映） |
| 4 | `CollisionSystem::Update` |
| 5 | **`AudioSystem::Update`**（消えた・外した・無効の音を止める → 初めて見つけた AudioSource を Play On Start で鳴らす → Inspector で変えた音量・ピッチを伝える） |

消えた Entity の音を同じフレームで止めたいので、`FlushDestroy` の後にした。

#### 鳴っている音の番号は AudioSystem が持つ

`SoundManager::Play` が返す「鳴っている1つ」の番号（再生ハンドル）は、**Component に入れず `AudioSystem.cpp` の中の一覧に持つ**（Entity と番号の組）。Component に入れると：

- Entity をコピー（Ctrl+D）すると、2つの AudioSource が同じ音を指す。片方を消すと、もう片方の音も止まる。
- 削除の Undo・Play / Stop の作り直しで、もう無い番号が戻ってくる。

Component にあるのは「何を・どう鳴らすか」（データ）だけで、「今鳴っているか」は System が持つ。Collider の「当たりの一覧」と同じ考え方（ARCHITECTURE.md のデータと処理の分離）。

一覧には**鳴っていない AudioSource も入れておく**。「この AudioSource はもう始まった」の印で、これが無いと Play On Start の音が鳴り終わるたびに鳴り直してしまう。

#### Play / Pause / Stop と音

| 操作 | SceneManager | 音 |
|---|---|---|
| Play | `AudioSystem::SetPaused(false)`。次のフレームの頭で作り直す（`ReloadImmediate`） | 作り直す前に **`StopAll`**（全部止める）→ 作り直した Entity の Play On Start が鳴る |
| Pause | `AudioSystem::SetPaused(true)` | 全部の音がその位置で止まる。一時停止中に鳴らした音は、Resume まで待つ |
| Resume | `AudioSystem::SetPaused(false)` | 続きから |
| Stop | その場で `StopAll` → `SetPaused(false)` | 全部止まる。停止中の `AudioSystem::Update` は AudioSource を鳴らさない |
| Restart | 作り直す前に `StopAll` | 一時停止中の Restart なら、一時停止のまま |

- 止めるのは `SoundManager` の**全部の音**（AudioSource の音も、シーンの `Initialize` が直接鳴らした音も）。Play / Stop のたびにシーンが作り直されるので、前のシーンの音は全部要らない。
- **次のシーンへの切り替え（`NextScene`）では全部は止めない。** AudioSource の音は、Entity が消えるので止まる。`SoundManager` で直接鳴らした音は、ゲームが止めるまで鳴り続ける（タイトルからゲームへ BGM を続けたいときに使える）。
- Stop で止めるのを作り直し（次のフレーム）まで待たないのは、一時停止中の Stop で「一時停止を解く → 作り直しで止める」の間の1フレームだけ音が出てしまうから。

---

## 4. 手順（パート A〜D）

> **2026-09-27 に書いた。** 下のコードは全部、scratchpad に作ったエンジンとゲームの写し（Collision を写した今の本体と同じ物）に入れて、コンパイル（エンジン85ファイル＋ゲーム、Debug / Release、`/W4`）でエラー0・新しい警告0、フォルダ整理のスクリプトを git の写しで実行、本物の XAudio2 で音を鳴らす動作テスト 109項目全通過を確かめてある（「6. 検証したこと」）。画面の見た目と実際の操作・聞こえ方はまだ確かめていないので、写したら「5. 確認すること」を見てほしい。

| パート | 中身 | ファイル |
|---|---|---|
| A | フォルダの整理・ヘッダの写し方 | スクリプト1つ（24ファイルを動かし、`#include` とプロジェクトを書き換える）、ビルド後のイベント、ゲーム側の `#include` |
| B | 音 | 新規 `AudioSourceComponent.h`・`AudioSystem.h/.cpp`、`SoundManager.h/.cpp`、`ComponentUI.h/.cpp`、`GameComponent.h`、`SceneManager.cpp`、`WindowManager.cpp`、`Engine.cpp` |
| C | 置き場所を覚えなくていいように | `ComponentEditor.h`（1行）、`GameComponent.h`、`InspectorWindow.cpp`、`ComponentUI.cpp` |
| D | ゲーム側の確認用 | 音のファイル2つ、`Source/Components/AudioKeyTest.cpp` |

---

### A. フォルダの整理

#### A-1. 準備

1. **Collision をコミットする**（まだなら）。フォルダの整理は変更が多いので、それだけで1コミットにしておくと、後で追いやすい。
2. ブランチを切る。

```bash
git switch -c feature/audio
```

3. **Visual Studio を閉じる。** 開いたままだと、スクリプトが書き換えたプロジェクトファイルを VS が古い内容で上書きすることがある。

#### A-2. スクリプトでファイルを動かす

24ファイルを動かして、`#include "MyEngine/..."` とプロジェクトファイル（`.vcxproj` / `.filters`）の中のパスを書き換える。手で動かすと `#include` の書き換えが50か所近くあり、直し忘れやすいので、スクリプトにした（中で何をするかは先頭のコメントのとおり。動かす一覧は `MOVES` の表）。

下を `MyEngine_v1` のリポジトリの一番上（`MyEngine_v1.vcxproj` がある所）に `reorganize.sh` という名前で保存する（**文字コードは UTF-8、改行はどちらでもよい**）。

```bash
#!/usr/bin/env bash
# MyEngine: move Component / System files into ECS/, Components/, Systems/ (Docs/Tasks/Audio.md part A)
# Run in Git Bash at the repository root (the folder that has MyEngine_v1.vcxproj):
#   bash reorganize.sh
# What it does:
#   1. moves the files (git mv; plain mv for files git does not track yet)
#   2. rewrites #include "MyEngine/..." in MyEngine/**/*.h, *.cpp
#   3. rewrites the paths in MyEngine_v1.vcxproj and MyEngine_v1.vcxproj.filters
# It does not touch the game project, Docs, or the post-build event.
# sed -b: keep CRLF line endings (Git Bash sed would turn them into LF without it)
set -euo pipefail

if [ ! -f MyEngine_v1.vcxproj ] || [ ! -d MyEngine ]; then
	echo "Run this at the repository root (the folder that has MyEngine_v1.vcxproj)." >&2
	exit 1
fi

# old path : new path (under MyEngine/)
MOVES=(
	"Entity/Entity.h:ECS/Entity.h"
	"Entity/EntityManager.h:ECS/EntityManager.h"
	"Entity/EntityManager.cpp:ECS/EntityManager.cpp"
	"Entity/ComponentStorage.h:ECS/ComponentStorage.h"
	"Entity/ComponentStorage.cpp:ECS/ComponentStorage.cpp"
	"Component/ComponentUI.h:ECS/ComponentUI.h"
	"Component/ComponentUI.cpp:ECS/ComponentUI.cpp"
	"Component/ComponentSerializer.h:ECS/ComponentSerializer.h"
	"Component/ComponentSerializer.cpp:ECS/ComponentSerializer.cpp"
	"Component/GameComponent.h:ECS/GameComponent.h"
	"Component/GameComponent.cpp:ECS/GameComponent.cpp"
	"Entity/TransformComponent.h:Components/TransformComponent.h"
	"Entity/ModelRendererComponent.h:Components/ModelRendererComponent.h"
	"Light/LightComponent.h:Components/LightComponent.h"
	"Collision/ColliderComponent.h:Components/ColliderComponent.h"
	"Component/EngineComponents.cpp:Components/EngineComponents.cpp"
	"Entity/ModelRenderSystem.h:Systems/ModelRenderSystem.h"
	"Entity/ModelRenderSystem.cpp:Systems/ModelRenderSystem.cpp"
	"Light/LightSystem.h:Systems/LightSystem.h"
	"Light/LightSystem.cpp:Systems/LightSystem.cpp"
	"Collision/CollisionSystem.h:Systems/CollisionSystem.h"
	"Collision/CollisionSystem.cpp:Systems/CollisionSystem.cpp"
	"Light/LightGizmo.h:Editor/Gizmos/LightGizmo.h"
	"Light/LightGizmo.cpp:Editor/Gizmos/LightGizmo.cpp"
)
# only included by Engine.cpp; the Light folder goes away
REMOVED="Light/LightIncludes.h"

# ===== check first (stop before changing anything) =====
for m in "${MOVES[@]}"; do
	from="MyEngine/${m%%:*}"
	to="MyEngine/${m##*:}"
	[ -f "$from" ] || { echo "not found: $from (already moved?)" >&2; exit 1; }
	[ ! -e "$to" ] || { echo "already exists: $to" >&2; exit 1; }
done

# git mv for tracked files, mv for new files (ColliderComponent.h etc. before the Collision commit)
move() {
	if git ls-files --error-unmatch "$1" > /dev/null 2>&1; then
		git mv "$1" "$2"
	else
		mv "$1" "$2"
	fi
}

# "MyEngine/Entity/Entity.h" -> "MyEngine\\Entity\\Entity\.h" (for sed: literal backslashes and dots)
to_backslash_pattern() {
	local s="${1//\//\\\\}"
	echo "${s//./\\.}"
}
to_backslash_text() {
	echo "${1//\//\\\\}"
}

mkdir -p MyEngine/ECS MyEngine/Components MyEngine/Systems MyEngine/Editor/Gizmos

# ===== move and rewrite =====
for m in "${MOVES[@]}"; do
	from="MyEngine/${m%%:*}"
	to="MyEngine/${m##*:}"
	move "$from" "$to"
	# includes (forward slashes)
	for file in $(grep -rlF --include=*.h --include=*.cpp "\"$from\"" MyEngine || true); do
		sed -b -i "s|\"${from//./\\.}\"|\"$to\"|g" "$file"
	done
	# project files (backslashes)
	sed -b -i "s|$(to_backslash_pattern "$from")|$(to_backslash_text "$to")|g" MyEngine_v1.vcxproj MyEngine_v1.vcxproj.filters
	echo "moved: $from -> $to"
done

# ===== LightIncludes.h =====
removed="MyEngine/$REMOVED"
if [ -f "$removed" ]; then
	if git ls-files --error-unmatch "$removed" > /dev/null 2>&1; then
		git rm -qf "$removed" # the include rewrite above touched it, so -f
	else
		rm "$removed"
	fi
	sed -b -i "/$(to_backslash_pattern "$removed")/d" MyEngine_v1.vcxproj MyEngine_v1.vcxproj.filters
	# Engine.cpp used it for LightSystem and LightGizmo
	sed -b -i "s|#include \"MyEngine/Light/LightIncludes\.h\"|#include \"MyEngine/Systems/LightSystem.h\"\r\n#include \"MyEngine/Editor/Gizmos/LightGizmo.h\"|" MyEngine/Engine.cpp
	echo "removed: $removed"
fi

# empty folders left behind (git does not keep them anyway)
for dir in MyEngine/Entity MyEngine/Component MyEngine/Light; do
	if [ -d "$dir" ] && [ -z "$(ls -A "$dir")" ]; then
		rmdir "$dir"
		echo "removed empty folder: $dir"
	fi
done

# ===== anything left that still points at an old place? =====
left=$(grep -rnE "\"MyEngine/(Entity|Component|Light)/|\"MyEngine/Collision/(ColliderComponent|CollisionSystem)\.h\"" MyEngine --include=*.h --include=*.cpp || true)
if [ -n "$left" ]; then
	echo "---- still old paths (fix by hand) ----"
	echo "$left"
	exit 1
fi
echo "done. Next: Audio.md A-3 (post-build event), then build the engine."
```

Git Bash を開いて（エクスプローラでリポジトリのフォルダを右クリック → Open Git Bash here。無ければ PowerShell から `& "C:\Program Files\Git\bin\bash.exe"`）、実行する。

```bash
bash reorganize.sh
```

`moved: MyEngine/Entity/Entity.h -> MyEngine/ECS/Entity.h` のような行が24行と `removed: ...` が出て、最後に `done.` と出れば成功。**`not found` / `already exists` で止まったときは、何も変わっていない**（先に全部確かめてから動かす）。終わったら `reorganize.sh` は消してよい。

- `git status` で見ると、動かしたファイルは `renamed`（R）になる（`git mv` で動かしているので、履歴もつながる）。Collision の3ファイルをまだコミットしていなかった場合は、普通の移動になる（新しいファイルとして出る）。
- スクリプトが書き換えるのは `MyEngine/` の下の `.h` / `.cpp` と、プロジェクトファイルだけ。**ゲーム側・Docs・ビルド後のイベントは触らない**（A-3・A-5 で手で直す）。
- 改行は CRLF のまま（Git Bash の `sed` は、何も付けないと LF にしてしまうので `-b` を付けている）。
- `Engine.cpp` の `#include "MyEngine/Light/LightIncludes.h"` は、`LightSystem.h` と `LightGizmo.h` の2行になる（パートB で `Engine.cpp` の `#include` はまとめて整える）。

#### A-3. ビルド後のイベント（エンジンのプロジェクト）

Visual Studio を開き、プロジェクトの再読み込みを聞かれたら「すべて再読み込み」。`MyEngine_v1` のプロジェクトを右クリック → プロパティ → 上の「構成」を **すべての構成** にして → 構成プロパティ → ビルド イベント → ビルド後のイベント → コマンド ライン を、次の4行に差し替える（**2行目だけが変わる**。xcopy → robocopy）。

```
xcopy /Y /I "$(OutDir)$(TargetName).lib" "$(SolutionDir)MyEngine\lib\$(Configuration)\"
robocopy "$(ProjectDir)MyEngine" "$(SolutionDir)MyEngine\include\MyEngine" *.h /S /PURGE /NFL /NDL /NJH /NJS /NP & if errorlevel 8 (exit /b 1) else (cmd /c "exit /b 0")
xcopy /Y /S /I "$(ProjectDir)MyEngine\Shader\*" "$(SolutionDir)MyEngine\Shader\"
xcopy /Y /S /I "$(ProjectDir)Resources" "$(SolutionDir)MyEngine\Resources\"
```

| 部分 | 意味 |
|---|---|
| `*.h /S` | `.h` だけを、下のフォルダも全部写す（今の xcopy と同じ） |
| `/PURGE` | **エンジンに無い `.h` と、`.h` が1つも無くなったフォルダを、ゲーム側から消す**（今回の目的） |
| `/NFL /NDL /NJH /NJS /NP` | 写したファイルの一覧などを出さない（出力ウィンドウが埋まらないように）。消したファイルは `*EXTRA File` の行で出る |
| `& if errorlevel 8 (exit /b 1) else (cmd /c "exit /b 0")` | 終了コードを直す（下の解説） |

- **道の最後に `\` を付けない。** robocopy は `"...\"` の `\"` を「ここは閉じカッコではない」と読んでしまい、道がおかしくなる。
- ゲーム側の `MyEngine/include/` の中の `.h` 以外の物は消さない（`.h` だけを見比べる）。**ゲームのリポジトリが `MyEngine/include/` も記録しているなら、`git status` に古いヘッダの削除が出る**（正しい変化）。

> **解説：robocopy の終了コード**
>
> ビルド後のイベントは、最後のコマンドの終了コードが 0 以外だと「ビルド失敗」になる。xcopy は成功すると 0 を返すが、**robocopy は成功でも 1（写した）・2（余分な物があった）・3（両方）などを返す**（0〜7 が成功、8 以上が失敗）。そのままだと、写せたのにビルド失敗と出る。
>
> そこで、8 以上なら `exit /b 1`（本当に失敗）、それ以外は `cmd /c "exit /b 0"`（終了コードを 0 にするだけ。イベントは続く）にしている。`if errorlevel 8` は「終了コードが 8 **以上**なら」という意味（cmd の古い書き方）。scratchpad で、古いヘッダと空になったフォルダが消え、`.h` 以外は残り、終了コードが 0 になるのを確かめた。

#### A-4. エンジンをビルド

Debug と Release の両方。**初めの1回は、出力ウィンドウに `*EXTRA File ... Entity\EntityManager.h` のような行がたくさん出る**（ゲーム側の古いヘッダを消した）。2回目からは出ない。

`CG3_Project/MyEngine/include/MyEngine/` を開いて、`ECS`・`Components`・`Systems` があり、**`Entity`・`Component`・`Light` が無い**ことを見る。

#### A-5. ゲーム側の `#include`

ゲームをビルドすると、古いパスが `error C1083: include ファイルを開けません。'MyEngine/Entity/EntityManager.h'` のように出る。次の表で直す（今あるのは `GameScene.cpp` の3行と `Source/Components/Spin.cpp` の1行。Collision.md の `HitLog.cpp` を足していたらそれも）。

| 前 | 後 |
|---|---|
| `<MyEngine/Component/GameComponent.h>` | `<MyEngine/ECS/GameComponent.h>` |
| `<MyEngine/Entity/EntityManager.h>` | `<MyEngine/ECS/EntityManager.h>` |
| `<MyEngine/Entity/ModelRenderSystem.h>` | `<MyEngine/Systems/ModelRenderSystem.h>` |
| `<MyEngine/Light/LightComponent.h>` | `<MyEngine/Components/LightComponent.h>` |
| `<MyEngine/Entity/TransformComponent.h>` など、ほかの `〜Component.h` | `<MyEngine/Components/〜Component.h>` |
| `<MyEngine/Collision/CollisionSystem.h>` など、ほかの `〜System.h` | `<MyEngine/Systems/〜System.h>` |

VS の「フォルダーを指定して置換」（Ctrl+Shift+H）で、`MyEngine/Component/GameComponent.h` → `MyEngine/ECS/GameComponent.h` のように置き換えてもよい。

ゲームをビルド（Debug と Release）して、エラーが無くなればパートAは終わり。**ここで一度コミットしておく**（「フォルダの整理」）。

---

### B. 音（AudioSource）

#### B-1. `MyEngine/Sound/SoundManager.h`（2か所）

(1) `CleanupSourceVoices` の宣言の下（`private:` の上）に、5つ足す。

```cpp
	/// <param name="deltaTime">前フレームからの経過時間</param>
	static void CleanupSourceVoices(float deltaTime);

	// ===== まとめて扱う（AudioSystem と、エディタの Play / Pause / Stop から使う）=====

	/// <summary>
	/// 鳴っている音を全部止めて破棄する（Play / Stop / Restart でシーンを作り直すとき）
	/// <para>AudioSource の音も、Play() で直接鳴らした音も止まる</para>
	/// </summary>
	static void StopAll();

	/// <summary>
	/// 全部の音を一時停止する / 再開する（エディタの Pause）
	/// <para>一時停止の間に Play() した音は、再開するまで鳴らない</para>
	/// </summary>
	static void SetPaused(bool paused);

	// 一時停止中か
	static bool IsPaused();

	/// <summary>
	/// 再生ハンドルの音がまだあるか（鳴り終わった・止めた・0 なら false。一時停止中は true）
	/// </summary>
	static bool IsPlaying(uint32_t playHandle);

	/// <summary>
	/// 素材ハンドルから、読み込んだときのパスを引く（保存と Inspector の表示用。0・知らない番号なら空）
	/// </summary>
	static const std::string& GetSoundPath(uint32_t soundHandle);

private:
```

(2) 一番下のメンバ変数 `nextPlayHandle_` の下に1つ足す。

```cpp
	// 再生ハンドルの発行カウンタ。0は無効なハンドルとするため1からスタートする
	uint32_t nextPlayHandle_ = 1;
	// SetPaused(true) の間は true（この間に Play した音は、Start せずに待たせる）
	bool paused_ = false;
};
```

#### B-2. `MyEngine/Sound/SoundManager.cpp`（6か所）

(1) インクルードに1行足す（`LogManager.h` の下）。

```cpp
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/String/ConvertString.h"
```

(2) `CleanupSourceVoices` の頭の `instance_->UpdateFadeInternal(deltaTime);` を、次で差し替える。

```cpp
	// 一時停止中はフェードも止める（進めると、再開したときに音量が飛ぶ）
	if (!instance_->paused_) {
		instance_->UpdateFadeInternal(deltaTime);
	}
```

(3) `CleanupSourceVoices` の関数の閉じ `}` の下（`// 初期化` の見出しの上）に、5つの関数を足す。

```cpp
//=============================================================================
// 全部止める
//=============================================================================
void SoundManager::StopAll() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("鳴っている音を全部止める 件数={}", instance_->voices_.size()));
	for (auto& [playHandle, voice] : instance_->voices_) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
		}
	}
	instance_->voices_.clear();
	instance_->fadeInfos_.clear();
}

//=============================================================================
// 全部を一時停止 / 再開
//=============================================================================
void SoundManager::SetPaused(bool paused) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	if (instance_->paused_ == paused) {
		return; // 同じ状態なら何もしない
	}
	instance_->paused_ = paused;
	// XAudio2 の Stop は「今の位置で止める」だけで、Start でそこから続きが鳴る（頭に戻るのは DestroyVoice して作り直したとき）
	for (auto& [playHandle, voice] : instance_->voices_) {
		if (!voice) {
			continue;
		}
		if (paused) {
			voice->Stop();
		} else {
			voice->Start();
		}
	}
	LogManager::Log(std::format("{} 件数={}", paused ? "全部の音を一時停止した" : "全部の音を再開した", instance_->voices_.size()));
}

bool SoundManager::IsPaused() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->paused_;
}

//=============================================================================
// まだ鳴っているか
//=============================================================================
bool SoundManager::IsPlaying(uint32_t playHandle) {
	// 毎フレーム呼ばれるのでログなし
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	// 鳴り終わった音は CleanupSourceVoices が一覧から消すので、一覧にあれば「まだある」
	return playHandle != 0 && instance_->voices_.contains(playHandle);
}

//=============================================================================
// 素材ハンドル → パス
//=============================================================================
const std::string& SoundManager::GetSoundPath(uint32_t soundHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	// 「パス → 番号」の表を逆から引く（読み込んだ音の数しか無いので、順に見れば足りる）
	const auto it = std::ranges::find_if(instance_->pathID_, [soundHandle](const auto& entry) { return entry.second == soundHandle; });
	return (it != instance_->pathID_.end()) ? it->first : kEmpty;
}
```

- `GetSoundPath` は、今ある「パス → 番号」の表（`pathID_`）を逆から引いている。表をもう1つ持つより、足す物が少ない。読み込んだ音の数しか無いので、順に見ても遅くない。
- `SetPaused` で使っている XAudio2 の `Stop` は「今の位置で止める」だけ（名前は Stop だが、中身は一時停止）。`Start` でそこから続きが鳴る。今ある `SoundManager::Pause` / `Resume` も同じ使い方をしている。

(4) `LoadInternal` の、キャッシュを見る所からファイルを開く所までを、次で差し替える（`// キャッシュチェック` の下の `if` から、`MFCreateSourceReaderFromURL` の失敗の `}` まで）。変わったのは3つ：キャッシュの知らせを警告からログに、パスの直し方を `ConvertString` に、読めなかったときに止めない。

```cpp
	// キャッシュチェック
	if (pathID_.find(filename) != pathID_.end()) {
		uint32_t cachedID = pathID_[filename];
		// 同じ音を何個ものAudioSourceが使うと毎回ここに来る。ふつうのことなので警告にはしない
		LogManager::Log(std::format("キャッシュ済み filename=\"{}\" -> soundHandle={}", filename, cachedID));
		return cachedID;
	}
	HRESULT hr;
	// UTF-8 のパスを Windows の文字列に直す（TextureManager と同じ。1バイトずつ広げると日本語のパスが読めない）
	std::wstring wFilename = ConvertString(filename);
	// SourceReaderでファイルを開く
	IMFSourceReader* pReader = nullptr;
	hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pReader);
	if (FAILED(hr)) {
		// 止めずに 0（未選択）を返す。Inspector で読めないファイルを選んでも、エディタが落ちないように
		// コードで Load して 0 が返った音を Play すると、Play の assert で止まるので気づける
		LogManager::Error(std::format("ファイルの読み込みに失敗した filename=\"{}\" hr=0x{:08X}", filename, static_cast<uint32_t>(hr)));
		return 0;
	}
```

(5) `PlayInternal` の `pSourceVoice->Start()` の所（`hr = pSourceVoice->Start();` から、その失敗の `}` まで）を、次で差し替える。

```cpp
	// 一時停止中（SetPaused）は Start しない。バッファは積んであるので、再開したときに頭から鳴る
	if (!paused_) {
		hr = pSourceVoice->Start();
		if (FAILED(hr)) {
			LogManager::Error(std::format("Start失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
			pSourceVoice->DestroyVoice();
			return 0;
		}
	}
```

(6) `SetVolumeInternal` の最後の1行（`LogManager::Warning(... の音量を{:.3f}に変更した ...)`）を、`Log` に変える。Inspector で音量をつまんで動かしている間、毎フレーム黄色い警告が出ないように。

```cpp
	voice->SetVolume(volume);
	LogManager::Log(std::format("playHandle={} の音量を{:.3f}に変更した", playHandle, volume)); // ふつうの操作なので警告にはしない
```

#### B-3. 新規：`MyEngine/Components/AudioSourceComponent.h`

```cpp
#pragma once
#include <cstdint>

// 音を鳴らすためのデータ。Entityに付けるComponent（カテゴリ：Audio）
// 鳴っている音の番号（SoundManager の再生ハンドル）は持たない。AudioSystem が持つ
// （ここに入れると、コピー・Undo で2つの AudioSource が同じ音を指してしまう）
// 位置はまだ使わない（離れると小さくなる・左右に振る、は後で。Audio.md「8. 未解決」）

/// <summary>
/// 音源
/// </summary>
struct AudioSourceComponent {
	uint32_t soundHandle = 0; // SoundManager::Load の番号（0は未選択）。保存はパスで
	float volume = 1.0f;      // 0=無音、1=元の大きさ
	float pitch = 1.0f;       // 0.5=1オクターブ下、1=元の高さ、2=1オクターブ上
	bool loop = false;        // 最後まで鳴ったら頭から鳴らし直す（BGM）
	bool playOnStart = true;  // シーンが始まったとき（Play を押したとき）に鳴らす
};
```

#### B-4. 新規：`MyEngine/Systems/AudioSystem.h`

```cpp
#pragma once
#include "MyEngine/Components/AudioSourceComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"


/// <summary>
/// AudioSourceComponent を持つEntityの音を、鳴らす・止める
/// <para>鳴っている音の番号（SoundManager の再生ハンドル）はここが持つ。Componentはデータだけ</para>
/// <para>エディタの停止中は鳴らさない。Entityを消す・無効にする・AudioSource を外すと、その音は止まる</para>
/// <para>一覧などの中身は AudioSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class AudioSystem {
public:
	// AudioSourceComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::FlushDestroy の後に呼ぶ
	/// <para>消えた・外した・無効になった AudioSource の音を止め、始まったばかりの AudioSource を鳴らし（Play On Start）、
	/// Inspector で変えた音量・ピッチを鳴っている音に伝える</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false）</param>
	static void Update(bool isGameRunning);

	// ===== ゲームから（SYSTEM の中で）使う =====

	// そのEntityの AudioSource を頭から鳴らす（鳴っていたら止めてから）。AudioSource が無い・音が未選択なら false
	static bool Play(Handle<Entity> entity);
	// そのEntityの音を止める（鳴っていなければ何もしない）
	static void Stop(Handle<Entity> entity);
	// そのEntityの音が鳴っているか（一時停止中も true。鳴り終わったら false）
	static bool IsPlaying(Handle<Entity> entity);
	// 重ねて鳴らす（効果音）。Entityを消しても最後まで鳴る。AudioSystem::Stop では止まらない（エディタの Stop では止まる）
	static bool PlayOneShot(Handle<Entity> entity);

	// ===== エディタの再生コントロールから使う（SceneManager が呼ぶ）=====

	// 全部の音を止める（AudioSource の音も、SoundManager で直接鳴らした音も）。シーンを作り直す前に呼ぶ
	static void StopAll();
	// 全部の音を一時停止する / 再開する
	static void SetPaused(bool paused);
};
```

#### B-5. 新規：`MyEngine/Systems/AudioSystem.cpp`

```cpp
#include "AudioSystem.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Sound/SoundManager.h"

namespace {
constexpr float kMinPitch = 0.5f; // ピッチの下限（Inspector のスライダーと同じ）
constexpr float kMaxPitch = 2.0f; // ピッチの上限（XAudio2 は、音を作るときの上限＝既定の2倍までしか上げられない）

/// <summary>
/// AudioSource 1つ分の、実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct SourceState {
	Handle<Entity> entity;    // 持ち主
	uint32_t playHandle = 0;  // 鳴っている音（SoundManager の再生ハンドル。0は鳴っていない）
	uint32_t soundHandle = 0; // 鳴らした音の素材（Inspector で音を変えたら止める）
	float volume = 1.0f;      // SoundManager に最後に伝えた音量（変わったときだけ伝える。SoundManager は呼ぶたびにログを出すので）
	float pitch = 1.0f;       // 最後に伝えたピッチ
};

/// <summary>
/// AudioSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct AudioState {
	// 見つけた AudioSource の一覧。鳴っていなくても持つ（「もう始まった」の印。無いと Play On Start が毎フレーム鳴り直す）
	std::vector<SourceState> sources;
};
std::unique_ptr<AudioState> state;

// 範囲に収めた音量・ピッチ（ファイルを手で書き換えて範囲の外になっても、XAudio2 に変な値を渡さない）
float ClampVolume(float volume) { return std::clamp(volume, 0.0f, 1.0f); }
float ClampPitch(float pitch) { return std::clamp(pitch, kMinPitch, kMaxPitch); }

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
	source.playHandle = SoundManager::Play(source.soundHandle, component.loop, source.volume, source.pitch);
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
}

// ===== Inspector と 保存 =====

// AudioSource の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Collider と同じ書き方）
// 保存の名前はメンバ名にし、Inspector には「表示###保存の名前」で読みやすい名前を出す
void DescribeAudioSource(ComponentUI& ui, AudioSourceComponent& value) {
	ui.AssetField("Sound###sound", value.soundHandle, AssetType::Sound, Tip("resources の下の .wav / .mp3"));
	ui.Field("Volume###volume", value.volume, Range(0.0f, 1.0f));
	ui.Field("Pitch###pitch", value.pitch, Range(kMinPitch, kMaxPitch).Tip("0.5で1オクターブ下、2で1オクターブ上"));
	ui.Field("Loop###loop", value.loop, Tip("最後まで鳴ったら頭から（BGM）。鳴っている途中で変えたときは、次に鳴らしたときから効く"));
	ui.Field("Play On Start###playOnStart", value.playOnStart, Tip("Play を押したとき（シーンが始まったとき）に鳴らす"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void AudioSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<AudioState>();

	// AudioSource の置き場所・保存・Inspector を登録する。見せ方は DescribeAudioSource 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<AudioSourceComponent>();
	ComponentSerializer::Register<AudioSourceComponent>("Audio Source", &DescribeAudioSource);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<AudioSourceComponent>>("Audio Source", "Audio", &DescribeAudioSource));
}

// SoundManager はウィンドウと一緒に先に片付いているので、ここでは SoundManager を呼ばない
void AudioSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void AudioSystem::Update(bool isGameRunning) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	std::vector<SourceState>& sources = state->sources;

	// ===== 停止中は鳴らさない =====
	if (!isGameRunning) {
		for (SourceState& source : sources) {
			StopSource(source);
		}
		sources.clear(); // 次に Play したときは、始まりからやり直す
		return;
	}

	// ===== 1. 消えた・外した・無効になった AudioSource の音を止めて、一覧から外す =====
	const auto isGone = [](const SourceState& source) {
		return EntityManager::Get<AudioSourceComponent>(source.entity) == nullptr || !EntityManager::IsActiveInHierarchy(source.entity);
	};
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
	return SoundManager::Play(component->soundHandle, false, ClampVolume(component->volume), ClampPitch(component->pitch)) != 0;
}

//=============================================================================
// エディタの再生コントロールから
//=============================================================================
void AudioSystem::StopAll() {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::StopAll();
	state->sources.clear(); // 作り直した後に見つけた AudioSource は「始まったばかり」になる（Play On Start がまた効く）
}

void AudioSystem::SetPaused(bool paused) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::SetPaused(paused);
}
```

- 形は `CollisionSystem.cpp` と同じ（中身は匿名 namespace の `state` に置き、見せ方は `DescribeAudioSource` 1つで保存と Inspector をまかなう）。
- **SoundManager に何かを頼む前に `IsPlaying` で確かめている**（`StopSource`）。鳴り終わった音や、`StopAll` で止めた音の番号で `SoundManager::Stop` を呼ぶと、「voices_ に存在しません」の警告が出るため。
- 音量・ピッチは**変わったときだけ** `SoundManager` に伝える。`SoundManager::SetVolume` は呼ぶたびにログを書くので、毎フレーム呼ぶとログが流れ続ける。
- `std::clamp` は `windows.h` の `min` / `max` マクロとぶつからない（マクロになっているのは `min` と `max` だけ）。
- `Update` の 1 で、先に止めてから `std::erase_if` で外している。`erase_if` に渡す関数の中で値を書き換える（止める）のは、決まりの上ではしてはいけないことになっているので、2回に分けた。

#### B-6. `MyEngine/ECS/ComponentUI.h` と `ComponentUI.cpp`

`ComponentUI.h`：`AssetType` に1つ足す。

```cpp
enum class AssetType {
	Model,   // ModelManager::Load の番号
	Texture, // TextureManager::Load の番号
	Sound,   // SoundManager::Load の番号（素材ハンドル）
};
```

`ComponentUI.cpp`（4か所）

(1) インクルードに1行足す（`TextureManager.h` の下）。

```cpp
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Sound/SoundManager.h"
```

(2) `GetAssetPath` の `switch` に2行足す（`case AssetType::Texture:` の下）。

```cpp
	case AssetType::Texture:
		return TextureManager::GetTexturePath(handle);
	case AssetType::Sound:
		return SoundManager::GetSoundPath(handle);
	}
```

(3) `LoadAsset` の `switch` に2行足す。

```cpp
	case AssetType::Texture:
		return TextureManager::Load(path);
	case AssetType::Sound:
		return SoundManager::Load(path); // 読めない形式なら0（SoundManager がログに出す）
	}
```

(4) `#ifdef USE_IMGUI` の中の、拡張子の表から `AssetFiles` の頭までを直す。`kSoundExtensions` と `kAssetTypeCount` と `ExtensionsOf` を足し、`IsAssetFile` の1行と `AssetFiles` の2行を変える。

```cpp
constexpr std::string_view kTextureExtensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".hdr"};
constexpr std::string_view kSoundExtensions[] = {".wav", ".mp3"}; // SoundManager（Media Foundation）が読める物
constexpr size_t kAssetTypeCount = magic_enum::enum_count<AssetType>(); // AssetType の数（種類を足しても、ここは直さなくてよい）

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

// その種類の拡張子か（大文字でも通るように小文字へ直して比べる）
bool IsAssetFile(const std::filesystem::path& path, AssetType type) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const std::span<const std::string_view> candidates = ExtensionsOf(type);
	return std::find(candidates.begin(), candidates.end(), extension) != candidates.end();
}

// その種類のファイルのパス一覧（区切りは / にそろえる。ModelManagerのキャッシュのキーと同じ形にするため）
const std::vector<std::string>& AssetFiles(AssetType type, bool refresh) {
	static std::vector<std::string> files[kAssetTypeCount]; // [AssetType] ごとの一覧
	static bool scanned[kAssetTypeCount] = {};              // 全部 false から
	const size_t slot = static_cast<size_t>(type);
```

- 前は `files[2]` と、`Model` でなければ `Texture` と決めつける `? :` だった。3つ目の種類を足すと、音の一覧を作るときに画像の拡張子で探してしまい、配列の外にも書く。`magic_enum::enum_count` で数をもらい、`switch` で選ぶようにした。
- 保存・読み込み（`ComponentSerializer.cpp`）は、`GetAssetPath` / `LoadAsset` を通っているので、何も直さなくてよい。

#### B-7. `MyEngine/ECS/GameComponent.h`（インクルードと説明）

スクリプトがパスだけ書き換えたインクルードを並べ直して `AudioSystem.h` を足し、説明のコメントの一番下に1行足す（ゲームは今までどおりこのヘッダ1つで、`AudioSystem::Play` が使える）。`#include "MyEngine/Systems/CollisionSystem.h"` から説明のコメントの最後の行までを、次で差し替える。

```cpp
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"

// ゲーム固有Componentを書くための道具。ゲーム側はこのヘッダ1つだけをインクルードする
//
//   1ファイル＝1Component。書くのは「① データ ② 見せ方 ③ 処理」の3つだけ
//
//   struct Spin { Vector3 speed = {0.0f, 90.0f, 0.0f}; };            // ① データ
//   COMPONENT(Spin, "Movement") { ui.Field("Speed", value.speed); }  // ② 見せ方
//   void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) { ... }  // ③ 処理
//   SYSTEM(SpinUpdate);
//
//   当たったかどうかは、③の中で CollisionSystem::GetHits(entity) を読む（Collider は Add Component → Physics で付ける）
//   音は、③の中で AudioSystem::Play(entity) / PlayOneShot(entity) を呼ぶ（Audio Source は Add Component → Audio で付ける）
```

#### B-8. `MyEngine/Scene/SceneManager.cpp`（5か所）

(1) インクルードに1行足す（`SceneSerializer.h` の下）。

```cpp
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Time/Time.h"
```

(2)〜(4) `Play` / `Pause` / `Stop` を、次で差し替える（それぞれ最後に音の行を足した）。

```cpp
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
```

(5) `ReloadImmediate` の一番上に2行足す。

```cpp
void SceneManager::ReloadImmediate() {
	// 鳴っている音を全部止める（シーンの Initialize が鳴らした BGM が、Play / Stop / Restart のたびに重なっていかないように）
	AudioSystem::StopAll();
	// Playを押した直後なら、作り直す前に今のEntityを退避する（フレームの頭なので、見えている通りに写せる）
```

- `SoundManager.h` ではなく `AudioSystem.h` をインクルードしているのは、`SoundManager.h` が `xaudio2.h`（と `windows.h`）を読む重いヘッダだから。`AudioSystem::StopAll` / `SetPaused` は `SoundManager` に渡すだけの薄い窓口。

#### B-9. `MyEngine/Window/WindowManager.cpp`（2か所）

(1) インクルード。スクリプトが書き換えた `Systems/` の2行を、3行にする（名前順に並べ直して `AudioSystem.h` を足した）。

```cpp
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/LightSystem.h"
#include "MyEngine/ECS/EntityManager.h"
```

(2) `UpdateAll` の中、`CollisionSystem::Update();` の下に足す。

```cpp
	CollisionSystem::Update();             // 当たり判定（消すと決めたEntityを消した後。結果は次のフレームのSYSTEMが読む）

	// 音（消したEntityの音を止めるので、FlushDestroy の後）。エディタの停止中は AudioSource を鳴らさない
	bool isGameRunning = false;
	for (WindowSet& w : windows_) {
		if (w.sceneManager && w.sceneManager->GetPlayState() != PlayState::Editing) {
			isGameRunning = true; // 再生中・一時停止中
		}
	}
	AudioSystem::Update(isGameRunning);

	InputManager::SetActiveWindow(nullptr); // 入力をリセット
```

#### B-10. `MyEngine/Engine.cpp`（3か所）

(1) インクルード。`// Editor` から `// Particle` の `ParticleManager.h` までを、次で差し替える（スクリプトでパスだけ変わった所を、新しいフォルダの名前で並べ直した。`// Graphics` から下はそのまま）。

```cpp
// Editor
#include "MyEngine/Editor/Gizmos/LightGizmo.h"
#include "MyEngine/Editor/Viewport/EditorOverlay.h"
#include "MyEngine/Editor/Profiler.h"
// Diagnostics
#include "MyEngine/Diagnostics/CrashHandler.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
// UI
#include "MyEngine/UI/GlobalVariables.h"
// Sound
#include "MyEngine/Sound/SoundManager.h"
// ECS（Entity と Component の仕組み）
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
// Systems（Component を読んで処理する係）
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/LightSystem.h"
// Input
#include "MyEngine/Input/InputManager.h"
// Math
#include "MyEngine/Math/MathIncludes.h"
// Collision
#include "MyEngine/Collision/Profiling/CollisionProfiler.h"
// Particle
#include "MyEngine/Particle/ParticleManager.h"
```

(2) `Initialize` の中、`CollisionSystem::Initialize();` の下に1行足す。

```cpp
	CollisionSystem::Initialize(); // ColliderをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
	AudioSystem::Initialize();     // AudioSourceをEntityManagerに登録する（保存・Inspectorも）ので、EntityManagerの後
```

(3) `Finalize` の中、`CollisionSystem::Release();` の下に1行足す。

```cpp
	CollisionSystem::Release(); // シーン（ウィンドウ）の破棄より後にする
	AudioSystem::Release();     // シーン（ウィンドウ）の破棄より後にする（SoundManager はウィンドウと一緒に先に片付いている）
```

#### B-11. プロジェクトに追加・ビルド

1. Visual Studio で、新しい3ファイルを作る（ソリューションエクスプローラーでフォルダを右クリック → 追加 → 新しい項目。「すべてのファイルを表示」なら、ファイルを作ってから右クリック → プロジェクトに含める）。
   - `MyEngine/Components/AudioSourceComponent.h`
   - `MyEngine/Systems/AudioSystem.h`・`AudioSystem.cpp`
   - **足し忘れても、エンジンのビルドは通ってしまう**（静的ライブラリなので）。エラーが出るのはゲームのリンクで、`MyEngine_v1.lib(Engine.obj) : error LNK2019: ... AudioSystem::Initialize` のように出る。
2. エンジンをビルド（Debug と Release）。

---

### C. 置き場所を覚えなくていいように

#### C-1. `MyEngine/Editor/Inspector/ComponentEditor.h`（1行）

`ComponentEditor` の「種類の情報」の最後（`GetSize` の下）に1行足す。

```cpp
	virtual size_t GetSize() const = 0;              // Componentの大きさ（Undo・コピーで中身を丸ごと写すのに使う）
	virtual const char* GetSourceFile() const { return nullptr; } // 書いてあるファイル（COMPONENT(...) の物だけ。分からなければnullptr）
```

#### C-2. `MyEngine/ECS/GameComponent.h`（3か所）

(1) 標準ライブラリのインクルードに1行足す（`<numbers>` の下）。

```cpp
#include <numbers>
#include <source_location>
#include <string>
```

(2) `DescribedComponentEditor` の、コンストラクタから最後のメンバまでを、次で差し替える（引数に `sourceFile` を足し、`GetSourceFile` で返す）。

```cpp
	// sourceFile は書いてあるファイル（Inspector の見出しにマウスを乗せると出る）。分からなければ nullptr
	DescribedComponentEditor(const char* name, std::string category, DescribeFunction describe, const char* sourceFile = nullptr)
	    : name_(name), category_(std::move(category)), describe_(describe), sourceFile_(sourceFile) {}

	const char* GetName() const override { return name_; }
	const char* GetCategory() const override { return category_.c_str(); }
	const char* GetSourceFile() const override { return sourceFile_; }

protected:
	void DrawComponent(T& value) const override { describe_(GetInspectorUI(), value); }

private:
	const char* name_;          // COMPONENT(型名) の型名がそのまま表示名になる
	std::string category_;      // "Gameplay" か "Gameplay/サブカテゴリ"
	DescribeFunction describe_; // ②に書いた見せ方
	const char* sourceFile_;    // COMPONENT(...) を書いたファイル（コンパイラが決めた文字列なので、プログラムの終わりまで消えない）
};
```

(3) `ComponentRegistrar` を、次で差し替える。

```cpp
template<class T> class ComponentRegistrar {
public:
	// where は書かなくてよい。既定の引数は「呼んだ側」で作られるので、COMPONENT(...) を書いたファイルと行が入る
	ComponentRegistrar(const char* name, const char* subCategory, void (*describe)(ComponentUI&, T&), std::source_location where = std::source_location::current()) {
		const char* sourceFile = where.file_name();
		ComponentRegistryDetail::AddPending([name, subCategory, describe, sourceFile]() {
			EntityManager::RegisterComponent<T>();                // 置き場所
			ComponentSerializer::Register<T>(name, describe);     // 保存・読み込み（Releaseでも要る）
			ComponentEditorRegistry::Register(                    // Inspector
			    std::make_unique<DescribedComponentEditor<T>>(name, MakeGameplayCategory(subCategory), describe, sourceFile));
		});
	}
};
```

> **解説：`std::source_location` で「どのファイルに書いたか」を知る**
>
> `std::source_location::current()` は、**それを書いた場所**（ファイル名・行）を返す C++20 の道具（`__FILE__` / `__LINE__` の今の書き方）。これを**引数の既定値**にすると、既定値は「関数を呼んだ側」で作られる決まりなので、呼んだ側の場所が入る。
>
> `COMPONENT(Spin, "Movement")` は、展開すると `Spin.cpp` の中で `ComponentRegistrar<Spin>` を作る（`registrarOf_Spin(...)`）。その呼び出しに `where` を書いていないので、既定値の `current()` が **`Spin.cpp` の、COMPONENT を書いた行**で作られる。ゲーム側は何も書き足さなくてよい。
>
> エンジンの Collider・Audio Source は `DescribedComponentEditor` を直接作っていて、`sourceFile` を渡していないので `nullptr`（出さない）。エンジンの Component は「名前で場所が決まる」（1章）ので、見出しに出さなくても迷わない。

#### C-3. `MyEngine/Editor/Windows/InspectorWindow.cpp`（3か所）

(1) インクルードに1行足す（`<cstddef>` の下）。

```cpp
#include <cstddef>
#include <filesystem>
#include <memory>
```

(2) 先頭の匿名 namespace の、`IsListed` の下に関数を1つ足す。

```cpp
// コンパイラが入れたファイルの場所（C:\...\Source\Components\Spin.cpp）を、今の作業フォルダ（ゲームのプロジェクトのフォルダ）からの相対パスに縮める
// 別のドライブなどで縮められなければ、そのまま返す
std::string ShortenSourcePath(const char* file) {
	std::error_code error; // 例外ではなくエラーコードで受ける
	const std::filesystem::path relative = std::filesystem::relative(file, error);
	return (error || relative.empty()) ? std::string(file) : relative.string();
}
```

(3) `DrawComponent` の中、`CollapsingHeader` の行の下（`// --- 見出しの右クリック ---` の上）に足す。

```cpp
	const bool isOpen = ImGui::CollapsingHeader(editor.GetName(), ImGuiTreeNodeFlags_DefaultOpen);

	// --- 見出しにマウスを乗せると、書いてあるファイルを出す（ゲームのComponentはどこに置いてもよいので、探さなくて済むように）---
	if (const char* file = editor.GetSourceFile(); file != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
		ImGui::SetTooltip("%s", ShortenSourcePath(file).c_str());
	}
```

- `ImGuiHoveredFlags_ForTooltip` は「少し止まってから出す」（マウスを通り過ぎただけでは出ない）。
- 相対パスを作る（ディスクを見る）のは、マウスが乗っている間だけ。`ImGui::SetItemTooltip` を使うと、乗っていなくても毎フレーム引数（`ShortenSourcePath`）を作ってしまうので、`IsItemHovered` で先に確かめている。

#### C-4. `MyEngine/ECS/ComponentUI.cpp`（2か所）

(1) `#ifdef USE_IMGUI` の中の、`kAssetSearchRoots` の行より下（`IsAssetFile` の上あたり）に関数を1つ足す。

```cpp
// 探す場所を1行で（"探す場所: resources/ MyEngine/Resources/（下のフォルダも全部）"）
// kAssetSearchRoots から作るので、場所を変えれば表示も変わる
const std::string& SearchRootsText() {
	static const std::string text = [] {
		std::string result = "探す場所:";
		for (const char* root : kAssetSearchRoots) {
			result += std::format(" {}/", root);
		}
		return result + "（下のフォルダも全部）";
	}();
	return text;
}
```

(2) `AssetField`（Inspector に描く係の方）の `BeginCombo` の中、`(Refresh)` の上に1行足す。

```cpp
		if (ImGui::BeginCombo(label, preview)) {
			ImGui::TextDisabled("%s", SearchRootsText().c_str()); // どこを探しているか（置き場所を覚えていなくても分かるように）
			const bool refresh = ImGui::Selectable("(Refresh)"); // フォルダにファイルを増やしたとき用
```

- `static const std::string text = [] { ... }();` は「最初に呼ばれたときに1回だけラムダを実行して作る」書き方。毎フレーム文字列を組み立てない。

エンジンをビルド（Debug と Release）。

---

### D. ゲーム側の確認用

1. **音のファイルを置く。** 手元に無ければ、Windows に入っている音を写す（`C:\Windows\Media`）。`CG3_Project/resources/sounds/` を作って、`Alarm01.wav`（長め。BGM・Loop の確認用）と `Windows Ding.wav`（短い。効果音の確認用）を写す。どちらもこのエンジンで読めるのを確かめてある。**ファイル名は英数字にする**（日本語の名前は読めない。「8. 未解決」）。
2. 確認用の Component を1つ足す（`Source/Components/` に。確かめたら消してよい）。

```cpp
// CG3_Project/Source/Components/AudioKeyTest.cpp
// 確認用：付けたEntityの Audio Source を、数字キーで鳴らす・止める（確かめたら消してよい）
//   1 … Play（頭から鳴らす）  2 … Stop  3 … PlayOneShot（重ねて鳴らす）
#include <format>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/ECS/GameComponent.h>
#include <MyEngine/Input/InputManager.h>

// 中身の無い、印だけのComponent
struct AudioKeyTest {};
COMPONENT(AudioKeyTest, "Test") {}

void AudioKeyTestUpdate(Handle<Entity> entity, AudioKeyTest&) {
	if (InputManager::IsKeyTriggered(DIK_1)) {
		const bool played = AudioSystem::Play(entity);
		LogManager::Log(std::format("Play: {}", played ? "鳴らした" : "鳴らせない（Audio Source が無い・音が未選択）"));
	}
	if (InputManager::IsKeyTriggered(DIK_2)) {
		AudioSystem::Stop(entity);
		LogManager::Log("Stop");
	}
	if (InputManager::IsKeyTriggered(DIK_3)) {
		AudioSystem::PlayOneShot(entity);
		LogManager::Log("PlayOneShot");
	}
}
SYSTEM(AudioKeyTestUpdate);
```

3. ゲームをビルド（Debug と Release）。

---

## 5. 確認すること

**フォルダの整理（パートA）**

1. `CG3_Project/MyEngine/include/MyEngine/` に `ECS`・`Components`・`Systems` があり、`Entity`・`Component`・`Light` が無い。
2. 起動して、今までどおり動く（Hierarchy・Inspector・Add Component・Collider の線・Play / Stop・Save）。
3. VS のソリューションエクスプローラー（すべてのファイルを表示）で、`MyEngine/ECS`・`Components`・`Systems`・`Editor/Gizmos` が見える。

**音（パートB・D）**

4. Entity を作り、**Add Component → Audio → Audio Source**。Sound のコンボを開くと、一番上に灰色で「探す場所: resources/ MyEngine/Resources/（下のフォルダも全部）」（パートC）、その下に `resources/sounds/Alarm01.wav` などが出る。選ぶ。
5. **停止中は鳴らない。Play を押すと鳴る**（Play On Start）。**Stop を押すとその場で止まる。**
6. Loop に印を付けて Play → `Alarm01.wav` が終わっても頭から鳴り続ける。Play 中に Volume・Pitch のスライダーを動かすと、その場で音が変わる（Log に黄色い警告が流れない）。
7. Play 中に **Pause → 音が止まる。Resume → 止まった所から続く。**
8. **Play → Stop を10回くらい繰り返す。** 音が重ならない（鳴っているのは1つだけ）。
9. Play 中に、その Entity の Active（Inspector の一番上、名前の左のチェック）を外す → 止まる。付け直す → 頭から鳴る（Play On Start がもう一度効く）。
10. Play 中に Entity を消す → 止まる。Ctrl+Z で戻す → また鳴る。Remove Component → 止まる。
11. Play On Start を外した Entity に **AudioKeyTest**（Add Component → Gameplay → Test）を付けて Play → **1** で鳴る、もう一度 **1** で頭から、**2** で止まる、**3** を連打すると重なって鳴る（**3** の音は **2** では止まらず、Stop では止まる）。Pause 中に **1** を押すと、Resume まで鳴らない。
12. Stop してから **Save** → 再起動しても残っている。`GameScene.scene.json` に `"Audio Source": { "sound": "resources/sounds/Alarm01.wav", "volume": 1.0, "pitch": 1.0, "loop": true, "playOnStart": true }` のように書かれている。
13. **置き場所がずれたとき：** アプリを閉じて `Alarm01.wav` の名前を変える → 起動すると Log に「Audio Source: "sound" のアセットが見つかりません（未選択にします）: resources/sounds/Alarm01.wav」が出て、Inspector の Sound は `(none)`。落ちない。**この状態で Save しない**（パスが消える。消してしまったら `GameScene.scene.json.bak` に1つ前がある）。名前を戻して起動し直すと元に戻る。
14. Audio Source の追加・Sound の選び直し・値の変更が **Ctrl+Z** で戻る。
15. **Release** でビルドして起動 → 起動した時点で Play On Start の音が鳴る（Release は再生中から始まる）。

**置き場所を覚えなくていいように（パートC）**

16. Spin（や AudioKeyTest）を付けた Entity の Inspector で、**見出しにマウスを少し乗せる** → `Source\Components\Spin.cpp` のように出る。Transform・Collider・Audio Source の見出しには何も出ない。
17. Model Renderer の Model・Texture のコンボにも「探す場所」の行が出る。

---

## 6. 検証したこと（2026-09-27、Claude）

- **フォルダの整理のスクリプト**：今の本体（Collision を写した状態）と同じファイルを scratchpad に写して git に入れ（Collision の3ファイルはコミットしていない状態も再現）、`reorganize.sh` を実行した。24ファイルが動き（git が追っているファイルは rename、追っていない物は普通の移動）、`LightIncludes.h` が消え、`Entity/`・`Component/`・`Light/` のフォルダが消え、古いパスの `#include` が1つも残らない。**全ファイルの改行が CRLF のまま**（最初の版は Git Bash の `sed` が LF に変えていたのを見つけて `-b` を付けた）。プロジェクトファイルのパスも書き換わる（ビルドはプロジェクトファイルに書いてある一覧からファイルを集めて行ったので、その確認にもなっている）。
- **robocopy**：古い `.h`・`.h` が無くなったフォルダが消える、`.h` 以外は残る、`.h` の無いフォルダは作らない、終了コードが 0 になる、をバッチファイルで確かめた。
- **コンパイル**：VS 18（v145）の cl で `/std:c++latest /permissive- /W4 /utf-8`（`UNICODE`）。
  - 整理だけ（パートA）：エンジン84ファイル Debug（`USE_IMGUI`）/ Release エラー0、警告は整理の前と同じ（Debug 4・Release 7。どれも前からある物）。ゲーム（`GameScene`・`Stage`・`Particles`・`main`・`Spin`）は、A-5 の4行を直してエラー0。直さないと C1083 が2つ出る。
  - 全部（A〜D）：エンジン85ファイル Debug / Release エラー0・新しい警告0。ゲーム（上＋`AudioKeyTest`＋「3. ゲームでの書き方」の例）Debug / Release エラー0・警告0。
- **動作テスト 109項目 全通過**：本物の `SoundManager`（XAudio2・Media Foundation で、このPCのスピーカーに実際に鳴らした）・`EntityManager`・`AudioSystem`・`ComponentSerializer`・`ComponentUI`（Release 側）・`SceneSerializer`・`SceneManager`・`GameComponent` を使い、描画・通知だけ差し替えた。音はテストの中で作ったサイン波の wav（0.3秒・3秒）と、`C:\Windows\Media` の `Alarm01.wav`・`Windows Ding.wav`。
  - 読み込み：同じパスは同じ番号、番号→パス、無いファイル・壊れたファイル（前は assert で止まっていた）・空は 0。空白の入った名前も読める。
  - 停止中は鳴らない → 動き出すと Play On Start で1つ鳴る → 何フレーム回しても1つのまま。停止に戻すと止まり、また動かすと頭から。
  - 音量・ピッチの変更（範囲の外の値も）で止まらない。音を変えると止まり、勝手に鳴り直さない。
  - `Play`（鳴っていたら頭から・数は増えない）・`Stop`（2回でも平気）・未選択は `false`。
  - 無効にする・親を無効にする・外す・消す → 止まる。有効に戻す・付け直す → また鳴る。無効な Entity の `Play` は `false`。
  - 0.3秒の音が鳴り終わると `IsPlaying` が `false`、Play On Start は鳴り直さない。Loop なら鳴り続ける。
  - `PlayOneShot` は重なる・`IsPlaying` に数えない・`Stop` と持ち主の削除では止まらない・`StopAll` で止まる。
  - 一時停止：鳴っている音も、一時停止中に鳴らした音も、時間がたっても鳴り終わらない（止まっている）。再開すると最後まで鳴る。
  - `StopAll` で直接鳴らした音も止まり、次の `Update` で Play On Start がまた効く。
  - 保存 → 読み込みで全項目が元どおり（`"sound":"t/short.wav","volume":0.5,"pitch":1.5,"loop":true,"playOnStart":false`）、未選択は `""`、もう一度書くと同じ文字列。無いファイルを指すシーンを読むと、警告1つで未選択になり、ほかの項目は読める。
  - **SceneManager**：`Initialize` で BGM を直接鳴らし、AudioSource も1つ持つシーンで、Pause / Resume（`SoundManager::IsPaused` が付いたり外れたり）、一時停止中の Stop（その場で0個・一時停止が解ける）、**Play → Stop を5往復しても音の数が 2 → 0 → 1 のまま増えない**、一時停止中の Restart で一時停止のまま。
  - `COMPONENT(...)` を書いたファイルの名前が Inspector の登録表に入る（`audiotest.cpp`）。Audio Source は `nullptr` でカテゴリは `Audio`。
  - 日本語のファイル名：Inspector の一覧と同じ作り方のパスで読むと、読めずに 0（**落ちない**）。
- **確かめていないこと**：画面の見た目（Inspector の欄・ツールチップ・コンボの「探す場所」の行）、本物のエディタでの操作、実際の聞こえ方（テストでは鳴っているかを数で見た）、VS でのビルド後のイベントの動き（コマンドはバッチで確かめた）。

---

## 7. 決めたこと

| 決めたこと | 理由 |
|---|---|
| フォルダは種類で分ける（`ECS/`・`Components/`・`Systems/`）。名前の終わりで場所が決まる | 機能で分けると、どの機能にも入らない物が迷子になる。名前から場所が分かれば、覚えることが表1つで済む |
| 仕組みのフォルダは `ECS/`（`Component/` ではない） | `Components/`（データ）と1文字違いで紛らわしい |
| `LightGizmo` は `Editor/Gizmos/` | ギズモはエディタで見るための物。`Light/` に1つだけ残すより、種類でそろえる |
| `Collision/`（判定の計算）・`Sound/`（SoundManager）はそのまま | Component を知らない「道具」。System から使われる側 |
| ゲーム側は1ファイル＝データ＋見せ方＋処理のまま | ゲームの Component は小さく、3つを一緒に直すことが多い |
| 移動はスクリプト（`git mv`＋`sed -b`） | `#include` の書き換えが約50か所あり、手では漏れる。`git mv` なら履歴もつながる |
| ゲームへのヘッダは robocopy `/PURGE` | xcopy は消えたヘッダを残し、古いパスがエラーにならない（前にも `PointLight.h` で起きた） |
| 鳴っている音の番号は `AudioSystem` が持つ（Component に入れない） | コピー・Undo・作り直しで、2つが同じ音を指したり、無い番号が戻ったりする |
| 停止中は AudioSource を鳴らさない | 置いて設定している間に鳴るとうるさい。Unity も同じ |
| Play / Stop / Restart の作り直しの前に全部の音を止める（直接鳴らした音も） | シーンが作り直されるので、前の音は全部要らない。止めないと BGM が重なっていく |
| 次のシーンへの切り替えでは全部は止めない | AudioSource の音は Entity と一緒に消える。直接鳴らした BGM は続けたいことがある |
| Pause は `SoundManager` 全体を止める（`SetPaused`）。一時停止中に鳴らした音も待たせる | 時間（`Time`）と同じく、ゲーム全体が止まっているように見せる |
| Stop はその場で止める（作り直しを待たない） | 一時停止中の Stop で、1フレームだけ音が出るのを防ぐ |
| `PlayOneShot` は一覧に入れない | 持ち主を消してもすぐ止めたくない音（倒した音）のため |
| 音量・ピッチは変わったときだけ `SoundManager` に伝える | `SoundManager` は呼ぶたびにログを書く |
| ピッチは 0.5〜2 | XAudio2 は音を作るときの上限（既定で2倍）より上げられない。下はスライダーを使いやすく |
| `SoundManager::Load` は読めなくても止めずに 0 を返す | Inspector で読めないファイルを選んでもエディタが落ちない。コードで直接使って 0 を `Play` すると、そこで止まるので気づける |
| `SoundManager` のパスの直し方を `ConvertString`（UTF-8）に | `TextureManager` とそろえる。前の1バイトずつ広げる方法は、英数字以外が必ず壊れる |
| Component を書いたファイルは `std::source_location` の既定の引数で取る | ゲームは何も書き足さなくてよい。`__FILE__` の今の書き方 |
| 見出しのファイル表示はゲームの Component だけ | エンジンの Component は名前で場所が決まる |
| Projectパネル・GUID は今はやらない | 表示だけではパス切れは直らない。Projectパネルを作るときに「中で動かすとパスも書き換える」を一緒に作る |

---

## 8. 未解決・後回し

| 何を | なぜ後か / どうするか |
|---|---|
| 日本語のファイル名 | Inspector の一覧（`AssetFiles`）がパスを `generic_string()`（Windows では Shift-JIS）で作り、エンジンの他の所は UTF-8 のつもりで読むので、日本語の名前は読めない（音は落ちずに未選択。モデル・画像は Manager の中で止まることがある）。また、日本語の入った UTF-8 のパス（シーンファイルを手で書き換えた等）を `LoadAsset` に渡すと、`std::filesystem::exists` の手前のパスの変換で例外が出て落ちる。直すなら、一覧を `generic_u8string()` で UTF-8 にし、`exists` に渡すパスを UTF-8 から作る（`std::filesystem::path(std::u8string(...))`）。今は**ファイル名を英数字にする** |
| 見つからないアセットを Save すると、パスが消える | 読み込みで 0（未選択）にしているため。Projectパネルを作るときに「中で動かせばパスも直る」にする。それまでは動かしたら選び直す（`.bak` に1つ前がある） |
| 3Dの音（離れると小さい・左右に振る） | `SoundManager::SetPan` はある。カメラ（聞く人）の位置との差から音量とパンを決めれば作れる。S5 で必要なら |
| 音の種類ごとの音量（BGM・効果音・全体） | XAudio2 の SubmixVoice を2つ作って、音をどちらかに流す。設定画面を作るとき |
| フェード（`AudioSystem` から） | `SoundManager::FadeOut` / `FadeIn` はある。`AudioSystem::FadeOut(entity, 秒)` を足すだけ。BGM の切り替えで要るとき |
| `SoundManager::FadeOut` で音量0にした音が片付かない | フェードの終わりで `Stop` しているが `DestroyVoice` していないので、`voices_` に残り続ける（`IsPlaying` も `true` のまま）。`StopAll` とアプリの終了では消える。フェードを使うときに直す |
| ゲームが `SoundManager::Pause` で止めた音が、エディタの Resume で鳴り出す | `SetPaused(false)` は全部を `Start` する。1つずつの一時停止を覚えておけば直せる。困ったら |
| 1つの Entity に Audio Source は1つ | 同じ型の Component は1つまで（ComponentStorage の決まり）。2つ鳴らしたいときは子の Entity に付ける |
| エディタで音を試し聞きする（停止中に▶ボタン） | 今は Play しないと聞けない。Audio Source の見出しにボタンを足すか、Projectパネルで |
| Component の型の名前を変えると、保存した分が読めなくなる | scene.json のキーが型の名前（`"Spin"`）だから。別名（前の名前）を `COMPONENT` に書けるようにすれば直せる |
| `WindowManager` の `isGameRunning` はウィンドウの中の1つでも再生中なら `true` | 音は全体で1つなので、ウィンドウごとには分けられない。複数のウィンドウで別々に Play する作りにするときに考える |
