# MyEngine リファレンス（2026-09-21 時点）

**今のエンジンで「ゲームを作るとき」に使う物だけをまとめた。** 設計の理由は [ARCHITECTURE.md](ARCHITECTURE.md)、作業手順と写経コードは [Tasks/](Tasks/) にある。

> **ブランチ `feature/scene-serialize` のこのファイルは、シーンの保存（[Tasks/Serialize.md](Tasks/Serialize.md) の S1）を入れた後の書き方になっている。** S1 を写し終えるまでは、2〜6章・8章・9章・17〜19章の「シーンファイル」「Play/Stop」「`CreateDefaultEntities`」「`EntityRef`・`FindByName`」「`ui.Field` の enum・Entity参照・アセット」はまだ使えない。

各項目は「**できること → 良い例 → だめな例**」の順。だめな例は実際にやりがちな物だけを載せた。

| 探している物 | 行き先 |
|---|---|
| ゲームを起動する形 | [1. 全体の形](#1-全体の形) |
| Entityを作る・消す | [3. Entity](#3-entity) |
| Componentを付ける・取る | [4. Component](#4-component) |
| 自分のComponentを書く | [5. ゲーム固有Component](#5-ゲーム固有componentcomponent--system) |
| **敵が複数いるときのフラグの持ち方** | [6. 状態をどこに置くか](#6-状態をどこに置くか) |
| 位置・回転・親子 | [7. Transform](#7-transform) |
| モデルを出す | [8. 描画](#8-描画) |
| ライト | [9. ライト](#9-ライト) |
| カメラ | [10. カメラ](#10-カメラ) |
| キー・マウス・パッド | [11. 入力](#11-入力) |
| 音 | [12. 音](#12-音) |
| 経過時間・ポーズ | [13. 時間](#13-時間) |
| 乱数・当たり判定・数学 | [14. 数学と当たり判定](#14-数学と当たり判定) |
| パーティクル | [15. パーティクル](#15-パーティクル) |
| ログ・調整項目 | [16. ログ・アサート・調整項目](#16-ログアサート調整項目) |
| **シーンの保存・Play / Stop で何が戻るか** | [2. ゲームの入口](#2-ゲームの入口) / [17. エディタ](#17-エディタ) |
| エディタの使い方 | [17. エディタ](#17-エディタ) |
| やりがちな失敗の一覧 | [18. だめな例まとめ](#18-だめな例まとめ) |
| まだ無い物 | [19. まだ無い物](#19-まだ無い物) |

---

## 1. 全体の形

```
main.cpp                  ゲームの入口。Engine::Initialize してループを回すだけ
  └ Engine               ウィンドウ・DirectX12・各Managerの初期化と、フレームの開始/終了
      └ WindowManager    ウィンドウごとの更新と描画。Editorのウィンドウもここ
          └ SceneManager 再生状態（Play / Pause / Stop）とシーンの作り直し
              └ IScene   ゲームが書くシーン（GameScene）
                  └ Entity + Component   置いてある物と、その中身のデータ
                       └ System          Componentを回して動かす処理
```

**エンジンは「データの置き場所」と「毎フレームの段取り」を持ち、ゲームは「データの中身」と「処理」を書く。**

### 1フレームの順番

| 順 | 何が起きるか | 書く場所 |
|---|---|---|
| 1 | `Engine::BeginFrame`（時間の更新、ImGuiの開始） | エンジン |
| 2 | `EntityManager::FlushComponentChanges`（前フレームに予約したComponentの追加・削除を反映） | エンジン |
| 3 | `EditorHistory::Flush`（Undo・削除・貼り付けの予約を実行） | エンジン |
| 4 | `SceneManager::Update`：頭で作り直し（Play / Stop / Restart）・シーン切り替え・保存 → **`IScene::Update`**（停止中は呼ばれない） | **ゲーム** |
| 5 | **`GameComponentRegistry::UpdateAll`（`SYSTEM(...)` で書いた処理）** | **ゲーム** |
| 6 | `EntityManager::UpdateTransforms`（親→子の順に `worldMatrix` を計算） | エンジン |
| 7 | `ParticleManager::Update` / `LightSystem::Update`（ライトを集めてGPUへ） | エンジン |
| 8 | `EntityManager::FlushDestroy`（`Destroy` の予約を反映。子も一緒に消える） | エンジン |
| 9 | `WindowManager::DrawAll` → **`IScene::Draw`** → `ModelRenderSystem::Draw` → ポストエフェクト | **ゲーム** |
| 10 | `Engine::EndFrame`（Present） | エンジン |

**覚えておくこと：**

- **追加・削除は必ず「予約 → 次のフレームの頭で反映」**。`ForEach` で回している最中に配列が動かないようにするため
- `worldMatrix` は 6 で計算されるので、**Updateの中で読むと1フレーム前の値**
- 停止中（Stop）は 4 の `IScene::Update` と 5 が飛ぶ（4 の頭の作り直し・保存は動く）。Pause中は `Time::GetDeltaTime()` が 0 になる

---

## 2. ゲームの入口

`main.cpp` は基本これだけ。触るのはウィンドウの設定と最初のシーンだけ。

```cpp
#include <MyEngine/Engine.h>
#include "GameScene.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	WindowConfig wc;
	wc.title = L"CG3";
	wc.width = 1280;
	wc.height = 720;
#ifdef _DEBUG
	wc.isImGui = true; // Editorを出すか
#endif
	Engine::Initialize(wc, [] { return std::make_unique<GameScene>(); });

	while (Engine::ProcessMessage()) {
		Engine::BeginFrame();
		Engine::GetWindowManager()->UpdateAll();
		Engine::GetWindowManager()->DrawAll();
		Engine::EndFrame();
	}
	Engine::Finalize();
	return 0;
}
```

シーンは `IScene` を継承して書く。**Entity はシーンファイルに保存され、エンジンが作って消す**ので、シーンのクラスが書くのは「Entity ではない物」（カメラ・IBL など）だけ。

```cpp
class GameScene : public IScene {
public:
	void Initialize() override; // Entityがそろった後に呼ばれる。カメラ・IBLなど、Entityではない物を作る
	void Update() override;     // 毎フレーム（停止中は呼ばれない）
	void Draw() override;       // 描画コマンドを積む
	void Finalize() override;   // Initializeで作った物を片付ける（Entityはエンジンが消す）
	Camera* GetCamera() override { return camera_; }

	// Entityを保存するファイル
	const char* GetSceneFile() const override { return "resources/scenes/GameScene.scene.json"; }
	// ファイルがまだ無いときだけ呼ばれる、最初の配置
	void CreateDefaultEntities() override;
};
```

シーンが作られる順番（起動・Play・Stop・シーン切り替えのたびに同じ）：

1. 前のシーンの `Finalize`
2. **全部の Entity を消す**
3. Entity を作る：Play を押した瞬間の退避があればそれ → 無ければシーンファイル → それも無ければ `CreateDefaultEntities()`
4. 予約した Component を付ける（`CreateDefaultEntities` で `RequestAdd` した物も、ここで付く）
5. `Initialize`（Entity はもうそろっているので、中で探してよい）

| 置き場所 | 保存されるか | 例 |
|---|---|---|
| Entity と、その Component の `ui.Field` に書いた項目 | **される**（Save でファイルへ。Play → Stop でも戻る） | 位置、モデル、ライト、敵のHP |
| `Initialize` で作った物（シーンのクラスのメンバ） | されない（毎回 `Initialize` で作り直す） | カメラ、IBL、パーティクルのグループ |
| Component の中でも `ui.Field` に書いていない項目 | されない（読み込むと初期値） | 実行中だけのタイマー |

> `GetSceneFile()` を書かないシーンは前の書き方のまま（Entity を `Initialize` で作って `Finalize` で消す）。新しく書くシーンはシーンファイルを使うこと。

---

## 3. Entity

Entityは「場面に置いてある物1つ」。名前と有効フラグと親子関係だけを持ち、中身は全部Componentで足す。

```cpp
// 作る（親を省略すると一番上）
const Handle<Entity> root = EntityManager::Create("GameScene");
const Handle<Entity> ball = EntityManager::Create("MonsterBall", root);

// 消す（予約。子も一緒に消える）
EntityManager::Destroy(ball);

// 生きているか
if (EntityManager::IsAlive(ball)) { … }

// 名前・有効フラグを触る（Entity自体の情報）
if (Entity* entity = EntityManager::Get(ball)) {
	entity->name = "Boss";
	entity->isActive = false;
}

// 親を付け替える／子を集める
EntityManager::SetParent(ball, root);
std::vector<Handle<Entity>> children;
EntityManager::GetChildren(root, children);
```

| やりたいこと | 書き方 |
|---|---|
| 自分と親を全部たどって有効か | `EntityManager::IsActiveInHierarchy(handle)` |
| 消えても変わらない番号がほしい | `EntityManager::Get(h)->id`（`EntityId`）。`FindById(id)` で引き直す |
| Componentに別のEntityを覚えさせたい | `EntityRef`（`EntityManager::RefOf(h)` で作り、`EntityManager::Get<T>(ref)` / `Find(ref)` で使う） |
| 名前で探したい（シーンの `Initialize` で） | `EntityManager::FindByName("Player")`（同じ名前が複数なら最初の1つ） |
| 数を知る | `EntityManager::GetCount()` |

### 良い例：フレームをまたぐときは `Handle` で覚える

```cpp
class GameScene : public IScene {
private:
	Handle<Entity> player_; // Handleなら持っていてよい（ただの番号2つ）
};
```

### だめな例：ポインタを持ち続ける

```cpp
class GameScene : public IScene {
private:
	Entity* player_;              // ✗ 次のフレームには別の物を指しているかもしれない
	TransformComponent* transform_; // ✗ Componentの配列は詰め直されるので特に危険
};
```

**取ったポインタはその場で使い捨てる。** 配列は要素が消えると後ろから詰められるので、持ち続けたポインタは別のEntityの物になる。

---

## 4. Component

Componentは「Entityに付けるデータ」。型ごとに `ComponentStorage<T>` という連続した配列に入る。

```cpp
// 付ける（予約。実際に付くのは次のフレームの頭）
EntityManager::RequestAdd<ModelRendererComponent>(ball, renderer);
EntityManager::RequestAdd<Spin>(ball); // 初期値でよければ値は省略できる

// 取る（持っていなければ nullptr）
if (ModelRendererComponent* renderer = EntityManager::Get<ModelRendererComponent>(ball)) {
	renderer->color = 0xFF0000FF;
}

// 外す（予約）
EntityManager::RequestRemove<Spin>(ball);

// 全部回す（その型を持っているEntityだけ）
EntityManager::ForEach<Spin>([](Handle<Entity> entity, Spin& spin) { … });
```

| 関数 | いつ効くか |
|---|---|
| `RequestAdd<T>` / `RequestRemove<T>` | **次のフレームの頭**（それまで `Get<T>` は `nullptr` のまま） |
| `IsAddPending<T>` | 追加を予約済みか |
| `Get<T>` | すぐ。反映済みの物だけ |
| `Destroy` | フレームの最後 |

### Componentに入れてよい物

```cpp
// ○ 良い例
struct Bullet {
	Vector3 velocity = {0.0f, 0.0f, 10.0f};
	float lifeTime = 3.0f;
	int damage = 1;
	bool piercing = false;
	EntityRef owner;        // 別のEntityへの参照は EntityRef（EntityIdで覚えるので、保存・Stop・Undoをまたいでも同じ相手）
};
```

```cpp
// ✗ だめな例
struct Bullet {
	std::string name;             // ✗ 可変長（ヒープを使う）
	std::vector<Vector3> path;    // ✗ 可変長
	Entity* owner;                // ✗ 生ポインタ
	ID3D12Resource* buffer;       // ✗ GPUリソース
	virtual void Update();        // ✗ 仮想関数（vtableポインタが入る）
};
```

理由：**Undoとコピーは中身を丸ごとバイト列で写している**ので、ヒープを指す物が入っていると壊れる。`static_assert(std::is_trivially_copyable_v<T>)` でコンパイル時に止まる。
文字やリストがどうしても要るときは相談すること（今は仕組みが無い）。

### だめな例：`ForEach` の最中にその型を増減させる

```cpp
EntityManager::ForEach<Enemy>([](Handle<Entity> entity, Enemy& enemy) {
	EntityManager::RequestAdd<Enemy>(other);  // ✗ assertで止まる（回している配列を動かせない）
});
```

**予約（`RequestAdd` / `RequestRemove` / `Destroy`）は安全**。止まるのは、その場で配列を動かす操作だけ。

---

## 5. ゲーム固有Component（COMPONENT / SYSTEM）

**1ファイル＝1Component。** `CG3_Project/Source/Components/` に `.cpp` を置いて、VSのプロジェクトに追加する。書くのは「①データ ②見せ方 ③処理」だけ。

```cpp
#include <MyEngine/Component/GameComponent.h>

// ① データ
struct Spin {
	Vector3 speed = {0.0f, 90.0f, 0.0f}; // 1秒に回る角度（度）
};

// ② Inspectorでの見せ方（カテゴリは Gameplay 固定。"Movement" はその下）
COMPONENT(Spin, "Movement") {
	ui.Field("Speed (deg/s)", value.speed, Tip("1秒に回る角度"));
}

// ③ 毎フレームの処理（両方持っているEntityだけ呼ばれる）
void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) {
	transform.rotation += spin.speed * (kDegToRad * deltaTime);
}
SYSTEM(SpinUpdate);
```

これだけで `EntityManager::RegisterComponent` も Inspector への登録も毎フレームの呼び出しも済む。詳しい一覧は [Tasks/Entity.md](Tasks/Entity.md) の **F-14**。

### `ui.Field` で書けるもの（抜粋）

| 書き方 | 見た目 |
|---|---|
| `ui.Field("HP", value.hp)` | 数字（`float` / `int` / `bool` / `Vector2` / `Vector3` / `Vector4`） |
| `ui.Field("HP", value.hp, Range(0.0f, 100.0f))` | スライダー |
| `ui.Field("Radius", value.r, AtLeast(0.0f))` | 0より小さくできない |
| `ui.Field("HP", value.hp, Tip("体力"))` | マウスを乗せると説明 |
| `ui.ColorField("Color", value.color)` | 色見本 |
| `ui.Field("Mode", value.mode)` | **enum** はコンボ（名前は magic_enum が作る） |
| `ui.Field("Target", value.target)` | **`EntityRef`**（別の Entity への参照）は相手の名前の台。Hierarchy から Entity をドラッグして入れる。右クリックで外す。相手が消えていると `(missing)` |
| `ui.AssetField("Model", value.model, AssetType::Model)` | **モデル・テクスチャの番号**（`uint32_t`）。resources 以下のファイルから選ぶ |
| `ui.Separator("見出し")` / `ui.Label("説明")` / `ui.Space()` | 飾り（保存はされない） |

### `ui.Field` に書いた物は、そのまま保存される

- **保存の名前はラベル**（`"HP"`）。enum は名前、`EntityRef` は EntityId（相手が居なければ 0）、アセットはパスで書かれる。
- **ラベルを変えると、前に保存した値が読めなくなる**（Log に「ファイルの項目 "HP" を読む所がありません」と出る）。表示だけ変えたいときは `"体力###HP"` と書く（`###` の後ろが保存の名前。ImGui の決まりと同じ）。
- `ui.Field` に書かなかった項目は保存されない（読み込むと初期値）。実行中だけの値はわざと書かなければよい。
- Component に項目を足しても、前のファイルはそのまま読める（足した項目は初期値になる）。
- 見せ方の関数の中に処理を書かない。Inspector に描くときだけでなく、保存・読み込みでも呼ばれる。

### `SYSTEM` の引数（＝「この型を全部持つEntityだけ呼ぶ」という注文書）

| 書き方 | 意味 |
|---|---|
| `void F(Spin& spin)` | `Spin` を持つ全部 |
| `void F(Spin& spin, TransformComponent& transform)` | **両方**持つEntityだけ |
| `void F(Spin& spin, float deltaTime)` | 最後の `float` は経過秒 |
| `void F(Handle<Entity> entity, Enemy& enemy)` | 先頭の `Handle<Entity>` は**そのEntity自身**（消したいとき） |
| `void F(const Spin& spin, Health& health)` | 読むだけなら `const&` |

### だめな例

```cpp
// ✗ 1ファイルに2つのComponentを詰める（どこに何があるか分からなくなる）
// ✗ 引数を値渡しにする（コンパイルエラー）
void SpinUpdate(Spin spin, TransformComponent transform) { … }

// ✗ Componentのファイルを静的ライブラリ（エンジン側）に入れる
//    → 誰も参照していないのでリンカに捨てられ、Add Componentに出てこない

// ✗ System同士を直接呼ぶ
void PlayerMove(...) { EnemyUpdate(...); } // 呼ぶ順番に縛られて壊れやすくなる
```

---

## 6. 状態をどこに置くか

**これが一番よく迷う所。** 置き場所は3つあって、使い分けは「**誰の状態か**」で決まる。

| 置き場所 | 誰の状態か | 例 |
|---|---|---|
| ① **Componentの中** | **その物1つだけ**の状態 | 敵のHP、弾の速度、プレイヤーの残機 |
| ② **ゲームに1つだけの入れ物** | ゲーム全体の状態 | スコア、倒した数、残り時間、今のステージ |
| ③ **タグComponent（付いているかどうか）** | 状態の切り替え | 倒された、無敵中、狙われている |

### 敵が複数いるとき（よくある勘違い）

**「敵ごとに倒したかのフラグ」はComponentに入れる。** Componentは**Entityごとに1個ずつ**用意されるので、`Enemy` を10体に付ければ `Enemy` の実体も10個ある。グローバル変数にするのは**全体で1個しかない物だけ**。

```cpp
// ○ 良い例
// Source/Components/Enemy.cpp
struct Enemy {
	int hp = 3;          // ← 敵ごとに1個。10体いれば10個ある
	bool isDefeated = false;
	float respawnTimer = 0.0f;
};

COMPONENT(Enemy, "Enemy") {
	ui.Field("HP", value.hp, Range(0.0f, 20.0f));
	ui.Field("Defeated", value.isDefeated);
}

// 倒されたかの判定は「その敵の中」で完結する
void EnemyDeath(Handle<Entity> entity, Enemy& enemy) {
	if (enemy.hp > 0 || enemy.isDefeated) {
		return;
	}
	enemy.isDefeated = true;
	++gGameState.killCount;         // ← 「全体で何体倒したか」だけがグローバル
	gGameState.speedBonus += 0.5f;
	EntityManager::Destroy(entity); // 自分を消す
}
SYSTEM(EnemyDeath);
```

```cpp
// Source/GameState.h  ← ゲーム全体の状態（1個しかない物だけ）
#pragma once
struct GameState {
	int killCount = 0;
	float speedBonus = 0.0f;
	bool isGameOver = false;
};
inline GameState gGameState; // inline を付けると、どのcppから見ても「同じ1個」
```

```cpp
// ✗ だめな例：敵ごとの状態をグローバルに置く
struct GameState {
	bool enemyDefeated;          // ✗ どの敵の話？ 敵が2体になった瞬間に破綻する
	bool enemy1Defeated;         // ✗ 敵が増えるたびに変数と if が増える
	bool enemy2Defeated;
	std::vector<bool> defeated;  // ✗ 番号と敵の対応を自分で管理することになる（消えたらずれる）
};
```

**Componentは「Entityごとの変数」そのもの。** 配列の管理はエンジンがやるので、自分で `vector<Enemy>` を持つ必要はない。

### 全体から「敵がまだ何体いるか」を知りたい

```cpp
// ○ 良い例：数えるのはSystemか、シーンのUpdateで
int aliveCount = 0;
EntityManager::ForEach<Enemy>([&](Handle<Entity> entity, Enemy& enemy) {
	if (EntityManager::IsActiveInHierarchy(entity) && !enemy.isDefeated) {
		++aliveCount;
	}
});
if (aliveCount == 0) {
	gGameState.isGameOver = false; // クリア処理へ
}
```

### 特定の1体を覚えておきたい（ボスなど）

```cpp
// ○ 良い例：Handle か EntityId で覚える
class GameScene : public IScene {
private:
	Handle<Entity> boss_;  // 同じ実行中ならこれでよい
	EntityId bossId_ = 0;  // 消して作り直しても追いたいならこっち（FindById で引く）
};
```

> シーンファイルを使うシーンでは、**Play・Stop・シーン切り替えのたびに Entity が作り直される**（Handle は変わり、EntityId は同じ）。シーンのクラスも作り直されて `Initialize` がまた呼ばれるので、メンバの Handle は `Initialize` の中で探し直す。探す目印には、空のタグ Component（`struct Boss {};`）を付けて `ForEach<Boss>` で見つけるのが楽。

```cpp
// ✗ だめな例
Enemy* boss_;  // ✗ 配列が詰め直されると別の敵を指す
```

### 別のEntityの状態を見たい（追尾など）

```cpp
// ○ 良い例：相手を EntityRef でComponentに持って、その場で取る
struct Homing {
	EntityRef target; // EntityId で覚える（Handle と違い、Stop・削除の Undo・再起動の後も同じ相手）
	float speed = 5.0f;
};

// Inspector で相手を選べるようにする（Hierarchy からドラッグ＆ドロップ）
// コードで入れるときは homing.target = EntityManager::RefOf(handle);
COMPONENT(Homing, "Movement") {
	ui.Field("Target", value.target);
	ui.Field("Speed", value.speed, AtLeast(0.0f));
}

void HomingMove(Homing& homing, TransformComponent& transform, float deltaTime) {
	const TransformComponent* targetTransform = EntityManager::Get<TransformComponent>(homing.target); // EntityRef のまま取れる
	if (targetTransform == nullptr) {
		return; // 相手はもう居ない（入っていない）
	}
	const Vector3 toTarget = Normalize(targetTransform->translation - transform.translation);
	transform.translation += toTarget * (homing.speed * deltaTime);
}
SYSTEM(HomingMove);
```

### System同士のつなぎ方

**直接呼び合わない。「片方が書いて、片方が読む」にする。** そうすると呼ぶ順番が変わっても壊れない。

```cpp
// ○ 良い例：印（タグComponent）を付けて、別のSystemが拾う
struct Dead {}; // 中身は空でよい
COMPONENT(Dead, "Enemy") { ui.Label("倒された印"); }

void EnemyDeath(Handle<Entity> entity, Enemy& enemy) {
	if (enemy.hp <= 0) {
		EntityManager::RequestAdd<Dead>(entity); // 印を付けるだけ
	}
}
SYSTEM(EnemyDeath);

void DeadEffect(Handle<Entity> entity, Dead&, TransformComponent& transform) {
	transform.scale = transform.scale * 0.9f; // 縮んで消える演出
	if (transform.scale.x < 0.05f) {
		EntityManager::Destroy(entity);
	}
}
SYSTEM(DeadEffect);
```

---

## 7. Transform

全Entityが必ず1つ持つ。**作った直後から `Get` できる**（予約ではない）。

```cpp
TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
transform->translation = {0.0f, 3.0f, 0.0f};
transform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f}; // ← ラジアン
transform->scale = {2.0f, 2.0f, 2.0f};
```

| 項目 | 中身 |
|---|---|
| `translation` / `rotation` / `scale` | **親から見た**位置・回転・大きさ。`rotation` は**ラジアン**（InspectorとGlobalVariablesの表示だけ度） |
| `worldMatrix` | 親の行列まで掛けた結果。**エンジンが毎フレーム計算する。自分で書き込まない** |
| `GetWorldPosition(transform)` | ワールド座標（行列の4行目） |
| `GetWorldForward(transform)` | 前（ローカル +Z）がワールドでどちらを向いているか |

```cpp
// ✗ だめな例
transform->rotation.y += 90.0f;            // ✗ 度で足している（ラジアンなので約5157周する）
transform->worldMatrix = MakeIdentity4x4(); // ✗ 自分で書いても次のフレームに上書きされる
```

> **`worldMatrix` はフレームの中盤（`UpdateTransforms`）で計算される。** シーンの `Update` で読むと1フレーム前の値。位置を正確に使いたいときは `translation` から計算するか、`Draw` で読む。

---

## 8. 描画

### モデルを出す

**一番早いのはエディタで置くこと**：Hierarchy の＋で Entity を作り、Add Component → Rendering3D → Model Renderer、Model の欄でファイルを選んで Save。

コードで置くとき（`CreateDefaultEntities` の中。シーンファイルがまだ無いときの最初の配置）：

```cpp
// ① モデルを読む（返るのは番号。同じパスは2回読まれない）
const uint32_t modelHandle = ModelManager::Load("resources/monsterBall/monsterBall.gltf");

// ② Entityに ModelRendererComponent を付ける
ModelRendererComponent renderer;
renderer.modelHandle = modelHandle;
renderer.shadingType = ShadingType::PBR;
const Handle<Entity> ball = EntityManager::Create("MonsterBall");
EntityManager::RequestAdd<ModelRendererComponent>(ball, renderer);
```

あとは `ModelRenderSystem` が毎フレーム描く。**ゲーム側で `Renderer::DrawModel` を呼ぶ必要はない。** 保存するとモデルはパスで書かれ、次の起動で読み直される。

| 項目 | 意味 |
|---|---|
| `modelHandle` | `ModelManager::Load` の戻り値。0は未選択 |
| `textureHandle` | 0ならモデルのマテリアルのテクスチャを使う（Inspector の Texture 欄で選べる） |
| `color` | 0xRRGGBBAA の掛け算 |
| `shadingType` | `Unlit` / `Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / **`PBR`** |
| `blendMode` | `Normal` / `Add` / `Subtract` / `Multiply` / `Screen` |
| `rasterizerType` | `SolidBack`（通常） / `SolidNone`（両面） |
| `depthMode` | `TestWrite`（不透明） / 半透明は書き込みを切る |
| `billboard` | `None` / `Full` / `AxisY`（カメラの方を向く） |
| `enabled` | falseで描かない |

> **PBRはIBL（環境光）が無いと真っ黒になる。** シーンが `IBLEnvironment` を持って `MakeFromHDR` しておくこと（`GameScene` がやっている）。

### 図形・スプライト

Entityにしないその場限りの描画は `Renderer` を直接呼ぶ（`Draw()` の中で）。

```cpp
Renderer::SphereConfig config;
config.transform.translation = {0.0f, 1.0f, 0.0f};
config.transform.scale = {0.5f, 0.5f, 0.5f};
config.color = 0xFF0000FF;
config.shadingType = ShadingType::Lambert;
Renderer::DrawSphere(config);
```

どの `***Config` も `transform`（拡縮・回転・移動）・`color`・`textureHandle`・`shadingType`・`blendMode` を同じ名前で持つ。

`DrawModel` / `DrawSprite` / `DrawTriangle` / `DrawSphere` / `DrawAABB` / `DrawOBB` / `DrawLines` / `DrawRect3d` / `DrawQuad2d` / `DrawQuad3d` / `DrawParticle` がある。

```cpp
// ✗ だめな例
void GameScene::Update() {
	Renderer::DrawSphere(config); // ✗ 描画は Draw() で積む（Updateで積むと停止中に消える）
}
```

---

## 9. ライト

**ライトもEntityに付けるComponent。** 位置と向きは付けたEntityのTransformから取る（向き＝ローカルの +Z）。

```cpp
const Handle<Entity> sun = EntityManager::Create("Directional Light"); // CreateDefaultEntities の中で（エディタで置くなら Create → Light）
TransformComponent* transform = EntityManager::Get<TransformComponent>(sun);
transform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f};
EntityManager::RequestAdd<DirectionalLightComponent>(sun);
```

| 種類 | 主な項目 |
|---|---|
| `DirectionalLightComponent` | `color` / `intensity`。**有効な物のうち最初の1つだけ**が使われる（2つ以上あると警告） |
| `PointLightComponent` | `color` / `intensity` / `radius` / `decay` |
| `SpotLightComponent` | `color` / `intensity` / `range` / `decay` / `outerAngle` / `innerAngle`（度、89まで） |

```cpp
// ✗ だめな例
struct DirectionalLightComponent { Vector3 direction; }; // ✗ 向きはTransformから取る（二重管理になる）
```

Hierarchyの **Create → Light** から作るのが早い（位置と向きの初期値もUnityと同じに入る）。

---

## 10. カメラ

```cpp
camera_ = new Camera();
camera_->Initialize(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f); // fovY(ラジアン), アスペクト, near, far
camera_->SetTranslation({0.0f, 0.0f, -30.0f});
```

シーンが `GetCamera()` で返したカメラを、エンジンが描画とパーティクルに使う。

```cpp
// ✗ だめな例
void GameScene::Update() {
	ParticleManager::SetCamera(camera_); // ✗ 要らない（エンジンが毎フレーム GetCamera から取る）
}
```

Sceneビューのカメラ（デバッグカメラ）は別物で、ゲームのカメラには影響しない。

---

## 11. 入力

```cpp
if (InputManager::IsKeyTriggered(DIK_SPACE)) { … } // 押した瞬間
if (InputManager::IsKeyPressed(DIK_W))      { … } // 押している間
if (InputManager::IsKeyReleased(DIK_SPACE)) { … } // 離した瞬間

const float x = InputManager::GetLeftStickX();     // -1 〜 1（デッドゾーン処理済み）
if (InputManager::IsButtonTriggered(XINPUT_GAMEPAD_A)) { … }

const long dx = InputManager::GetMouseDeltaX();
```

キーの名前は DirectInput の `DIK_A` `DIK_SPACE` `DIK_LSHIFT` など。

```cpp
// ✗ だめな例
if (GetAsyncKeyState(VK_SPACE)) { … } // ✗ Windowsを直接叩くとImGuiに文字を打っている間も反応する
```

---

## 12. 音

```cpp
const uint32_t bgm = SoundManager::Load("resources/bgm.wav");   // 読む（番号が返る）
const uint32_t play = SoundManager::Play(bgm, true, 0.5f);      // 鳴らす（ループ, 音量）
SoundManager::SetVolume(play, 0.2f);
SoundManager::Stop(play);
```

`Load` の戻り値が**音のデータ**、`Play` の戻り値が**鳴っている1つ**。止めたり音量を変えたりするのは後者。

```cpp
// ✗ だめな例
void GameScene::Update() {
	const uint32_t se = SoundManager::Load("resources/se.wav"); // ✗ 毎フレーム読む（Loadは Initialize で1回）
	SoundManager::Play(se, false);
}
```

---

## 13. 時間

```cpp
const float deltaTime = Time::GetDeltaTime();         // 前フレームからの秒（TimeScaleが掛かる）
const float raw = Time::GetUnscaleDeltaTime();        // 掛かっていない方（UIの演出などに）
Time::SetTimeScale(0.5f);                             // スロー
```

Pauseは `TimeScale = 0`。**`deltaTime` を掛けて動かしていれば、ポーズ処理を自分で書かなくても止まる。**

```cpp
// ✗ だめな例
transform->translation.z += 0.1f;                    // ✗ フレームレートで速度が変わる／ポーズで止まらない
// ○
transform->translation.z += 5.0f * Time::GetDeltaTime();
```

---

## 14. 数学と当たり判定

```cpp
#include <MyEngine/Math/MathIncludes.h>

const float r = RandomEngine::GetFloat(0.0f, 1.0f);
const int n = RandomEngine::GetInt(0, 10);
const Vector3 v = RandomEngine::GetInsideUnitSphere();

const Vector3 a = Normalize(b - c);
const float len = Length(v);
const float t = MathUtility::Clamp(x, 0.0f, 1.0f);
const Vector3 p = MathUtility::Lerp(from, to, 0.5f);
```

当たり判定は形どうしの関数。**`Collision::` を付けて呼ぶ**（`#include <MyEngine/Collision/CollisionIncludes.h>`）。

```cpp
const Sphere sphere{GetWorldPosition(*transform), 1.0f};   // 中心, 半径
const AABB box{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}; // 最小点, 最大点
if (Collision::SphereAABB(sphere, box)) { … }

const Ray ray{origin, direction}; // 始点, 向き
float t = 0.0f;
if (Collision::RaySphere(ray, sphere, &t)) { … } // 当たった距離が t に入る
```

`SphereSphere` / `AABBAABB` / `SphereAABB` / `RaySphere` / `RayAABB` / `RayPlane` / `IsPointInSphere` / `IsPointInAABB` / `ClosestPointOnAABB` / `RayPoint`。

> **物理エンジンは無い。** 重力も押し戻しも自分で書く（当たったら位置を戻す、速度を反転する、など）。

---

## 15. パーティクル

```cpp
// グループを作る（Initializeで1回）
ParticleGroupSetting setting;
setting.textureHandle = TextureManager::Load("resources/particle.png");
groupHandle_ = ParticleManager::CreateGroup(setting);

// 出す
Particle p;
p.transform.translation = {0.0f, 1.0f, 0.0f};
p.velocity = RandomEngine::GetInsideUnitSphere();
p.lifeTime = 1.0f;
ParticleManager::Register(groupHandle_, p);
```

1グループ最大4096個。カメラはエンジンが毎フレーム渡す。

---

## 16. ログ・アサート・調整項目

```cpp
LogManager::Log("Push!");            // logs/日付.log に残る
LogManager::Warning("敵が多すぎる");
LogManager::Error("モデルが読めない");

MY_ASSERT_MSG(handle != 0, "モデルを読み込んでから呼んでください"); // Debugだけ。通知が出てログにも残る
```

数値の調整は `GlobalVariables`（Parametersウィンドウで触って、jsonに保存される）。**登録する関数と、読み戻す関数の2つを書く**のが決まり。

```cpp
// 登録（Initializeで1回）。Group → Category → Add をつなげて書く
void GameScene::RegisterGV() {
	auto gv = GlobalVariables::GetInstance()->Group("GameScene");
	gv.Category("Player").Add("Speed", playerSpeed_).Add("JumpPower", jumpPower_);
}

// 読み戻す（Updateで毎フレーム）
void GameScene::ApplyGV() {
	auto* gv = GlobalVariables::GetInstance();
	playerSpeed_ = gv->Get<float>("GameScene", "Player", "Speed");
	jumpPower_ = gv->Get<float>("GameScene", "Player", "JumpPower");
}
```

`Add` は**登録済みなら値を上書きしない**（jsonから読んだ値が生き残る）。`Category("Enemy").Category("Move")` と重ねると階層になり、`Get<float>("GameScene", "Enemy/Move", "Speed")` で引ける。

> **Inspectorで触れる物はComponentに入れる方がよい。** `GlobalVariables` は「Entityに紐づかない全体の設定」向け。

---

## 17. エディタ

| 操作 | 何ができるか |
|---|---|
| **Hierarchy** | Entityの一覧。右上の＋で作る、右クリックでコピー/貼り付け/削除、ダブルクリック（F2）で名前変更、ドラッグで親子 |
| **Inspector** | 選んだEntityのComponentを編集。下の **+ Add Component** で足す、見出しの右クリックで外す |
| **Scene / Game** | Sceneは編集用（デバッグカメラ・ギズモ）、Gameは製品の絵 |
| **Play / Pause / Stop** | Playで `Update` が回り出す（**停止中に編集した状態のまま始まる**）。**Stopで Play を押した瞬間の状態へ戻る**（Play 中に動いた・作った・消した物は全部戻る。選んでいた Entity もそのまま） |
| **Restart** | 再生中だけ押せる。Play を押した瞬間からやり直す |
| **Save（Ctrl+S）** | Control ウィンドウ。停止中だけ。シーンファイル（`resources/scenes/〇〇.scene.json`）に書く。1つ前は `.bak` に残る |
| **Undo / Redo** | Ctrl+Z / Ctrl+Y。作成・削除・名前・有効・親子・Componentの追加削除・Inspectorの値変更が戻る。Play・Stop・シーン切り替えで履歴は消える |
| **View → Gizmos** | ライトのアイコンと範囲の表示切り替え |

> **Save しないで終了すると、停止中の編集は消える**（Play → Stop では消えない）。Play 中の変更は Stop で戻るので、Play 中に見つけた良い値は、Stop してから入れ直して Save する（Unity と同じ）。

---

## 18. だめな例まとめ

| だめな書き方 | 何が起きるか | 正しくは |
|---|---|---|
| `Entity*` や `TransformComponent*` をメンバに持つ | 配列が詰め直されて別の物を指す | `Handle<Entity>` か `EntityId` で持つ |
| Componentに `std::string` / `std::vector` / ポインタ / 仮想関数 | Undo・コピーが壊れる（`static_assert` で止まる） | 固定長の値だけにする |
| `ForEach<T>` の中でその型を増減させる | assertで止まる | 予約（`RequestAdd` / `RequestRemove` / `Destroy`）を使う |
| `RequestAdd` の直後に `Get` | `nullptr` が返る | 次のフレームで取る。初期値は `RequestAdd` に渡す |
| 敵ごとの状態をグローバル変数に置く | 2体目で破綻する | Componentに入れる（Entityごとに1個ある） |
| `rotation` に度を入れる | とんでもない回転になる | ラジアン（`* kDegToRad`） |
| `worldMatrix` に自分で書き込む | 次のフレームに上書きされる | `translation` / `rotation` / `scale` を変える |
| `deltaTime` を掛けずに動かす | フレームレートで速度が変わる。ポーズで止まらない | `* Time::GetDeltaTime()` |
| `Update` で `Renderer::Draw***` を呼ぶ | 停止中に消える・順番が崩れる | `Draw()` の中で積む |
| 毎フレーム `ModelManager::Load` / `SoundManager::Load` | 無駄に遅くなる | `Initialize` で1回 |
| Componentのファイルを静的ライブラリに入れる | リンカに捨てられて登録が消える | ゲーム（exe）のプロジェクトに入れる |
| ヘッダで `std::numeric_limits<T>::max()` | `windows.h` の `max` マクロと衝突 | `<cfloat>` の `FLT_MAX` など |
| シーンファイルを使うシーンの `Initialize` で Entity を作る | ファイルから読んだ物と二重になる（Play・Stop のたびに増える） | 最初の配置は `CreateDefaultEntities` に書く。後はエディタで置いて Save |
| `ui.Field` のラベルを変える | 前に保存した値が読めず、初期値に戻る | `"新しい表示###前のラベル"` と書く |
| 残したい値を `ui.Field` に書き忘れる | 保存されず、Stop・再起動で初期値に戻る | Stop して値が戻ったら書き忘れ。`ui.Field` に足す |
| 見せ方の関数（`COMPONENT(...) { }` の中）に処理を書く | 保存・読み込みのときにも実行される | 処理は `SYSTEM` に書く |
| Componentに `Handle<Entity>` で相手を覚える | Play / Stop・削除の Undo・再起動のたびに無効になる（`ui.Field` にも渡せない） | `EntityRef` で覚える |
| `CreateDefaultEntities` で作った Entity の Handle をメンバに覚える | Play の後・2回目の起動では無効（その関数が呼ばれない） | `Initialize` の中で `FindByName` などで探す |
| System同士を直接呼ぶ | 呼ぶ順番に縛られる | 片方が書いて片方が読む（タグComponentか全体の状態を経由） |

---

## 19. まだ無い物

**作っていない物を正直に書いておく（設計の穴ではなく、順番の問題）。**

| 無い物 | 今どうするか | 予定 |
|---|---|---|
| カメラ・IBL・天球の保存 | Entity ではないので、`Initialize` のコードで作る | CameraComponent・シーンの設定として後で |
| 当たり判定の Component（Collider） | 当たり判定の関数（`Collision::`）を自分で呼ぶ | **次にやる**（Serialize.md の S2） |
| 音の Component（AudioSource） | `SoundManager` を直接呼ぶ | S3 |
| クオータニオン | オイラー角（ラジアン）。真上を向くと破綻する | 必要になってから（ファイルの版を上げて読み替える） |
| 物理エンジン | 当たり判定の関数だけ。押し戻しは自分で書く | 未定（PhysXは大きい） |
| プレハブ・別のシーンを開く | 同じEntityを作る関数を自分で書く。編集できるのは最初のシーンだけ | シリアライズの続きとして後で |
| アニメーション | 無い（モデルはノード階層まで） | 未定 |
| 2DのUIレイアウト | `Renderer::DrawSprite` に座標を直接渡す | 未定 |
| Componentの文字列データ | 入れられない（固定長のみ） | 必要になったら |
| Projectパネル（ファイル一覧） | VSでファイルを作る | 後で |
| ビルドの高速化（PCH） | 1ファイル約2.5秒 | S4（0.2秒になる見込み） |

---

## 関連

- [ARCHITECTURE.md](ARCHITECTURE.md) — なぜこの設計か（生ポインタ禁止、データと振る舞いの分離）
- [Tasks/Entity.md](Tasks/Entity.md) — Entity / Component の作り方と、書き味レイヤーの全コード
- [Tasks/Serialize.md](Tasks/Serialize.md) — シーンの保存・読み込み、Play / Stop の仕組み、次の作業の順番
- [Tasks/Editor.md](Tasks/Editor.md) — Editorの責務、Undoの仕組み、決めたことの一覧
- [Tasks/Light.md](Tasks/Light.md) — ライトの整理の経緯
- [Tasks/Model.md](Tasks/Model.md) / [Tasks/Material.md](Tasks/Material.md) / [Tasks/PostEffect.md](Tasks/PostEffect.md) / [Tasks/FrameLoop.md](Tasks/FrameLoop.md) / [Tasks/Compute.md](Tasks/Compute.md)
