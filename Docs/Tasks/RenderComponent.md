# RenderComponent（Editor Step 3）の写経手順

## 作業ルール

- コードはこのMarkdownに書き、ユーザーが実装ファイルへ写経する。
- 作業ブランチは `codex/component-render-guide`。Pushは禁止。
- 実装済み・ビルド済み・実行確認済みを分けて記録する。
- 既存の作業途中の変更は残す。完了した手順や設計理由も残す。

## 進める順番

| 区切り | 内容 | 状態 |
|---|---|---|
| 3a | データ、追加予約、取得、Entityと一緒の破棄 | 写経手順作成。未反映・未ビルド |
| 3b | 描画処理、カメラ・IBLの受け渡し | 次回 |
| 3c | Inspector、Add Component、モデル選択 | 3bの後 |
| 3d | ゲーム側のMonsterBallをEntityへ移行 | IBLを含めた動作確認後 |

今回は **3aだけ**。この区切りを写経しても、まだモデルは画面に出ない。
`Editor.md` Step 3全体の完了は3dまで確認してからとする。

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| 実体はEntityManagerの `SlotMap<RenderComponent>` が所有し、EntityはHandleを持つ | 既存のTransformと同じ方式。Componentのデータを型別にまとめる |
| `Renderer::ModelConfig` をComponentに丸ごと持たせない | Camera・IBL・行列のポインタと `std::wstring` が入っている。Componentは値とIDを持ち、描画処理が一時的なConfigへ変換する |
| 追加は予約し、次のUpdate冒頭にまとめて反映する | InspectorやSystemの処理中にSlotMapを増やして、取得済みポインタを無効化しないため |
| 1 EntityにつきRenderComponentは1個 | 複数メッシュは既存のモデルアセットが持つ。ノードのEntity化は `Model.md` Step 2で扱う |
| IBLの所有方法は3bで整理する。今回はIBLポインタを足さない | 現在のIBLEnvironmentはGPUリソースを所有し、Handle管理されていない。先に生ポインタを保存して済ませると設計原則に反する |

## Step 3a：データと寿命の管理

### ① `MyEngine/Entity/RenderComponent.h` を新規作成

Visual Studioのエンジンプロジェクトにもヘッダーとして追加する。
`.vcxproj` / `.filters` はVisual Studioの「追加」で更新してよい。

```cpp
#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/Transform.h"


// 描画に必要なデータだけを持つ。処理とGPUリソースは持たない。
struct RenderComponent {
	uint32_t modelHandle = 0;   // 0は未選択（ModelManagerの採番は1から）
	uint32_t textureHandle = 0; // 0ならモデルのマテリアルのテクスチャを使う
	uint32_t color = 0xFFFFFFFF;
	Transform uvTransform;
	MaterialParams material;
	ShadingType shadingType = ShadingType::Unlit;
	BlendMode blendMode = BlendMode::Normal;
	RasterizerType rasterizerType = RasterizerType::SolidBack;
	DepthMode depthMode = DepthMode::TestWrite;
	bool enabled = true;
};

static_assert(std::is_trivially_copyable_v<RenderComponent>);
```

`static_assert` はコピーが単純なデータであることの補助チェック。
生ポインタもこの検査には通るので、「ポインタを持たない」規則そのものは設計時に確認する。
位置・回転・拡縮は既存のTransformComponentが持つため、ここには重複して持たせない。

### ② `MyEngine/Entity/Entity.h`

自作ヘッダーのincludeへ追加する。

```cpp
#include "MyEngine/Entity/RenderComponent.h"
```

`Handle<TransformComponent> transform;` の直後へ追加する。

```cpp
	Handle<RenderComponent> render; // 未追加なら無効なHandle
```

### ③ `MyEngine/Entity/EntityManager.h`

`public:` の取得関数付近へ追加する。

```cpp
	// 追加を予約する。実体ができるのは次のFlushComponentChanges。
	// 戻り値のポインタを即座に使う方式にはしない。
	static void RequestAddRender(Handle<Entity> handle);
	static bool IsRenderAddPending(Handle<Entity> handle);
	static RenderComponent* GetRender(Handle<Entity> handle);

	// Update冒頭で1回呼ぶ。SlotMapの変更はここへまとめる。
	static void FlushComponentChanges();
```

`private:` の `SlotMap<TransformComponent> transforms_;` の直後へ追加する。

```cpp
	SlotMap<RenderComponent> renders_;
	std::vector<Handle<Entity>> pendingAddRender_;
```

### ④ `MyEngine/Entity/EntityManager.cpp`

次の4関数を、取得関数のまとまりの後へ追加する。
`<algorithm>` は既にincludeされているので追加不要。

```cpp
void EntityManager::RequestAddRender(Handle<Entity> handle) {
	if (!IsAlive(handle) || GetRender(handle) || IsRenderAddPending(handle)) {
		return;
	}
	instance_->pendingAddRender_.push_back(handle);
}

bool EntityManager::IsRenderAddPending(Handle<Entity> handle) {
	const auto& pending = instance_->pendingAddRender_;
	return std::find(pending.begin(), pending.end(), handle) != pending.end();
}

RenderComponent* EntityManager::GetRender(Handle<Entity> handle) {
	Entity* entity = instance_->entities_.Get(handle);
	if (!entity) {
		return nullptr;
	}
	return instance_->renders_.Get(entity->render);
}

void EntityManager::FlushComponentChanges() {
	EntityManager& self = *instance_;
	for (Handle<Entity> handle : self.pendingAddRender_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity || self.renders_.IsAlive(entity->render)) {
			continue;
		}
		entity->render = self.renders_.Create();
	}
	self.pendingAddRender_.clear();
}
```

続いて `FlushDestroy()` の中のこの行を探す。

```cpp
		self.transforms_.Destroy(entity->transform); // Componentも一緒に消す
```

その **直前** に追加する。

```cpp
		self.renders_.Destroy(entity->render);
```

無効なHandleに対する `SlotMap::Destroy` は何もしないので、RenderComponentが無いEntityもそのまま破棄できる。
親を破棄すると既存の `CollectDescendants` が子孫を集めるので、子のRenderComponentも同じループで破棄される。

### ⑤ `MyEngine/Window/WindowManager.cpp`

`WindowManager::UpdateAll()` の **関数本体の先頭**、ウィンドウやシーンの更新より前へ追加する。

```cpp
	EntityManager::FlushComponentChanges(); // 前フレームに予約したComponentを追加
```

既存の `EntityManager::UpdateTransforms()` と `FlushDestroy()` の位置はこのStepでは変えない。
`EntityManager.h` は既にincludeされている。

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

### 確認すること

1. エンジンのDebug / Releaseビルドが通る。
2. ゲームのビルドが通り、既存の描画・Hierarchy・Inspectorが変わらず動く。
3. 追加処理の確認は、EntityManager初期化後、描画やUpdateの走査をしていないテスト箇所で以下を一時的に実行する。確認後はテスト用コードを外す。

```cpp
const Handle<Entity> testEntity = EntityManager::Create("Render lifetime test");
EntityManager::RequestAddRender(testEntity);
EntityManager::RequestAddRender(testEntity); // 重複予約しても1個だけ
MY_ASSERT_MSG(EntityManager::GetRender(testEntity) == nullptr, "予約だけでは追加しない");
MY_ASSERT_MSG(EntityManager::IsRenderAddPending(testEntity), "追加が予約されている");
EntityManager::FlushComponentChanges();
MY_ASSERT_MSG(EntityManager::GetRender(testEntity) != nullptr, "反映後は取得できる");
MY_ASSERT_MSG(!EntityManager::IsRenderAddPending(testEntity), "予約は消えている");

EntityManager::Destroy(testEntity);
EntityManager::FlushDestroy();
MY_ASSERT_MSG(EntityManager::GetRender(testEntity) == nullptr, "Entityと一緒に破棄される");

const Handle<Entity> cancelled = EntityManager::Create("Cancelled render test");
EntityManager::RequestAddRender(cancelled);
EntityManager::Destroy(cancelled);
EntityManager::FlushDestroy();
const Handle<Entity> replacement = EntityManager::Create("Replacement test");
EntityManager::FlushComponentChanges();
MY_ASSERT_MSG(EntityManager::GetRender(replacement) == nullptr, "古い予約を新Entityへ適用しない");
EntityManager::Destroy(replacement);
EntityManager::FlushDestroy();
```

テストを置くファイルには `MyEngine/Entity/EntityManager.h` と
`MyEngine/Diagnostics/MyAssert.h` のincludeが必要。
`MY_ASSERT_MSG` の評価を無効化する構成では判定できないため、この動作確認はDebugで行う。
本番のゲームコードから `FlushComponentChanges` / `FlushDestroy` を呼ぶ使い方にはしない。

### 次回の確認事項

- 3bではCameraとIBLを描画時に解決する方法、シーン・ウィンドウの所属、`isActive` の親子への伝播を決める。
- 3cではモデルの読み込みタイミングも確認する。Inspector内からのロードがGPU転送のフレーム処理と衝突しないことを確認してからコードにする。
- 既存Entityの即時CreateやTransform更新順序と、ARCHITECTURE.mdの規約との差は残っている。このStepで全体が規約対応済みになったとは扱わない。
