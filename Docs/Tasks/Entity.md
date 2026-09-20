# Entity：Componentの保管と寿命

## 読む順番（2026-09-21 更新）

**今エラーが出ているなら、まず [F-16](#f-16-出ているエラーの直し方2026-09-21) を見る。** 原因は2つで、どちらも数行で直る。

| 順 | 場所 | 内容 |
|---|---|---|
| 1 | **F-16**（手順Fの中） | 今出ている45件のエラーの直し方。①F-7（Editor3ファイルの5行）をまだやっていない ②`ComponentUI.h` の `max`（`FLT_MAX` にする）。あなたの実ファイルで再現して確認済み |
| 2 | **F-17** | F-11(4)（vcxprojの行）が何をしているのか。**飛ばしてもいい**（代わりのやり方あり） |
| 3 | **F-18 / F-19** | よくある質問（日本語カテゴリ、Transformは要るのか、ゲーム全体のフラグの置き方、敵を倒す書き方）と、出てくるSTL・テンプレートの用語の地図 |
| 4 | 手順F の F-1〜F-14 | 書き味レイヤー本体。`COMPONENT` / `SYSTEM` / `ui.Field` / サブカテゴリ / 自動登録 |

写経済み：手順A〜E、[Light.md](Light.md) Step 6・Step 6.1、[Editor.md](Editor.md)「直したこと（2026-09-20）」、手順F の F-1〜F-11(3)。

読み物（写経は要らない）：

- 「これからの設計：ゲーム固有Componentの書き方」＝手順Fの考え方（なぜこの形にしたか）
- 「ドラッグ＆ドロップ・C#・マルチスレッドの優先順位」＝2026-09-20 の相談の答え。ビルド時間の実測値もここ
- 「レビュー結果（2026-09-19）」と手順A〜E（写経済み）、さらに下のCodexの手順1〜8（記録）

---

## レビュー結果（2026-09-19）

### 確かめたこと

- 今のソースのまま（Codexの手順1〜6を写した状態）で、エンジン全77ファイルを `/W4` の Debug / Release でコンパイル → エラー0、新しい警告0。ゲーム側4ファイルも Debug / Release で通る
- 本物の `EntityManager`・`ComponentStorage`・`EditorHistory` を使ったテストで、予約の規則と Undo を確認（下の「確認したこと」に結果）

### 正しい所（このままでよい）

| 所 | 見た内容 |
|---|---|
| `ComponentStorage` | 予約のまとめ方（追加→削除＝無し、削除→追加＝置き換え、二重追加は最初の値）、世代込みのキー（同じスロットに作り直しても古い予約が付かない）、末尾と入れ替える削除と索引の付け替え、破棄で予約も消す。4000回の乱数の操作を、別に持った期待値と毎回比べて全部一致 |
| `EntityManager` | `RegisterComponent` の二重登録は何もしない、`Get` / `RequestAdd` / `RequestRemove` / `IsAddPending` / `ForEach`、Transformは必ず付いて外せない、`FlushDestroy` で登録された全型から消す |
| `TypedComponentEditor` | 実体の出し入れを EntityManager に直結したので、各Editorは表示（`DrawComponent`）だけ書けばよくなった |
| `EditorHistory` の手順6 | Component全体を写す方式。仕様の変更として正しく動く（下の「注意」） |

### 直す所

| # | 今の問題 | 起きること | 直し方 |
|---|---|---|---|
| 1 | ソースは `ComponentCategory::GamePlay`、Entity.md の例は `Gameplay` | 手順7の `HealthEditor` をそのまま写すとコンパイルエラー | `Gameplay` にそろえる（手順A） |
| 2 | Inspectorの区画が「登録した順」に並ぶ | ゲームのシーンの `Initialize` は `ImGuiManager::Initialize`（エンジンのEditorを登録する所）より**先に**走る（`WindowManager::AddWindow` の中の順番）。ゲームのEditorを登録すると、Transformより上に出る | 登録表をカテゴリ順に並べる（手順A） |
| 3 | 削除のUndo・コピーの写しを「Editorの登録表」から集めている | Editorを登録していない型（コードからだけ付けた `Velocity` など）は、**削除→Undo・複製・コピーで消える** | 写しを EntityManager の全部の型から取る（手順B・C） |
| 4 | `ForEach` の間は、どの型でも `Create` が禁止（`iterationDepth_`） | ゲームのSystemで「`ForEach<Spawner>` の中で敵を作る」とアサートで止まる。よく書く形 | 止めるのは「回している型の配列が増減するとき」だけにする（手順B） |

### 注意（直さないが知っておくこと）

- **手順6の仕様**：Undoは「Component全体」を編集開始時に戻す。Play中にゲームが同じComponentを書き換えていると、それも一緒に戻る。例：手順Eの `Spin` で回っている MonsterBall の Position を**Play中に**変えて Undo → Rotation も編集開始時に戻る（少しカクッとする）。Stop中（Editing）はゲームの `Update` が止まっているので起きない
- Transformで「ドラッグして元の値にぴったり戻した」ときは、空のUndoが1件できる（`worldMatrix` が前のフレームの値のまま比べられるため）。Ctrl+Zが1回空振りするだけ
- 互換用に残していた `GetTransform` / `GetModelRenderer` / `RequestAddModelRenderer` などは、使っている所が3か所（`UpdateTransforms`・`ModelRenderSystem`・ゲームの `GameScene`）だけなので、今回そろえて消す（手順B・D・E）
- `ARCHITECTURE.md` の最後の「まだソースへ反映していない」は古くなった（手順A）

---

## A. カテゴリ名と、Inspectorの並び順

### A-1. `MyEngine/Editor/Inspector/ComponentEditor.h`

enum の最後の1行を直す（Lightingのコメントも、Step 6が終わるので外す）。

```cpp
	Lighting,    // ライト
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
	Gameplay,    // ゲーム固有のデータ
};
```

enum の上の説明は「`/// Add Componentのメニューの分け方。Inspectorの区画もこの順に並ぶ`」に。
`ComponentEditor` の説明の3行目と、`ComponentEditorRegistry` の説明を次に置き換える。

```cpp
/// <para>新しいComponentを作ったら、EntityManager::RegisterComponent&lt;T&gt;() で置き場所を作り、
/// TypedComponentEditorを継承したクラスを ComponentEditorRegistry::Register する</para>
```

```cpp
/// <summary>
/// ComponentEditorの登録表。カテゴリ順（同じカテゴリの中は登録した順）に並べて持つ
/// <para>ゲームのシーンはエンジンのEditorより先に登録することがある（ImGuiManagerの初期化より前にシーンが作られる）。
/// それでもTransformが一番上に来るように、登録した順ではなくカテゴリで並べる</para>
/// </summary>
```

### A-2. `MyEngine/Editor/Inspector/ComponentEditor.cpp`（ファイル全体を差し替え）

`push_back` の代わりに、同じカテゴリの最後の後ろへ入れる。`enum class` 同士は `<` で比べられる（書いた順の値になる）。

```cpp
#include "ComponentEditor.h"

#include <algorithm>
#include <string_view>

namespace {
std::vector<std::unique_ptr<ComponentEditor>> editors; // カテゴリ順（同じカテゴリの中は登録した順）
}

void ComponentEditorRegistry::Register(std::unique_ptr<ComponentEditor> editor) {
	for (const std::unique_ptr<ComponentEditor>& registered : editors) {
		if (std::string_view(registered->GetName()) == editor->GetName()) {
			return;
		}
	}
	// 同じカテゴリの最後の後ろに入れる（Coreが必ず先頭、Gameplayが最後になる）
	const ComponentCategory category = editor->GetCategory();
	const auto position = std::upper_bound(editors.begin(), editors.end(), category, [](ComponentCategory value, const std::unique_ptr<ComponentEditor>& registered) { return value < registered->GetCategory(); });
	editors.insert(position, std::move(editor));
}

const std::vector<std::unique_ptr<ComponentEditor>>& ComponentEditorRegistry::GetAll() { return editors; }
```

### A-3. `Docs/ARCHITECTURE.md` の最後の3行

```
現在（2026-09-19）: Transform・ModelRenderer・ライト（Light.md Step 6の後）は、EntityManagerの型別の連続配列（ComponentStorage<T>）で管理している。段階2の終わり〜段階3の入り口。Editorの登録表（ComponentEditorRegistry）はRuntimeの実体管理とは別。
ゲーム固有のComponentも EntityManager::RegisterComponent<T>() で同じように扱える。手順は [Tasks/Entity.md](Tasks/Entity.md)。
Entityの名前・親子をIDから分離することや、複数World・マルチスレッド化は今回の対象に含めない。
```

---

## B. `ComponentStorage` と `EntityManager`

### 何が変わるか

| 変更 | 理由 |
|---|---|
| `ComponentSnapshot`（型＋バイト列）と、`IComponentStorage` に `GetSize` / `Find` / `SetNowBytes` を追加 | 型を知らなくても「持っているComponentを全部写す・戻す」ができる。EditorHistory（手順C）が、Editorの登録表ではなくこちらから写す |
| `EntityManager::CaptureComponents` / `RestoreComponents` を追加 | 上の窓口を全部の型について回す。将来のシーン保存・プレハブ（Instantiate）にも使える形 |
| 「回している」印を EntityManager 全体の1個（`iterationDepth_`）から、**型ごと**（`ComponentStorage::iterating_`）に | `ForEach<Spawner>` の中で `Create` してもよくなる。止めるのは、今回している型の配列が増減する操作（その型の Flush・破棄・`SetNow`）だけ |
| `EnsureStructuralChangesAllowed` → `EnsureInitialized` | 残った役目は「Initializeを呼んだか」の確認だけなので、名前を合わせた |
| `IsActiveInHierarchy` を追加 | 自分と親が全部有効か。ModelRenderSystem・ライト・ゲームのSystemが同じ判定を使う（UnityのactiveInHierarchy） |
| 互換用の `GetTransform` など5つを削除 | 新しいComponentに専用の関数を増やさない、という手順3の方針どおり。呼んでいる所は手順B・D・Eで `Get<T>` などに直す |
| 消えていたコメントを戻した | Codexの版で説明のコメントがほぼ消えていたので |

`ForEach` の中でしてよいこと・いけないことは、`EntityManager.h` の `ForEach` の上に書いた。

```
 ForEach<SpinComponent>( … ) の中で
   ○ 値を書き換える　　○ RequestAdd / RequestRemove / Destroy（予約）　　○ Create（Transformの配列が伸びる）
   × FlushComponentChanges / FlushDestroy（Spinの配列が増減する）
 ForEach<TransformComponent>( … ) の中で
   × Create（今回しているTransformの配列が伸びて、回している途中の要素がずれる）→ アサート
```

`Create` した後は、それより前に `Get` で受け取ったTransformのポインタを使わない（配列が伸びて場所が変わることがある）。これは前からの規則（ARCHITECTURE.md 原則1）。

### B-1. `MyEngine/Entity/ComponentStorage.h`（ファイル全体を差し替え）

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Diagnostics/MyAssert.h"

// 前方宣言
struct Entity;


/// <summary>
/// Component1つ分の写し（型を知らずに持てる形。Undo・コピー・「Componentを付けて作る」用）
/// <para>同じ実行の中だけで使う。type_index やバイト列をファイルに保存しない（型の番号も並びも実行ごとに変わりうる）</para>
/// </summary>
struct ComponentSnapshot {
	std::type_index type = typeid(void); // どの型か
	std::vector<std::byte> bytes;        // 中身をそのまま写したもの

	// 値から写しを作る
	template<class T> static ComponentSnapshot Make(const T& value) {
		static_assert(std::is_trivially_copyable_v<T>, "Componentはmemcpyで写せる型にしてください（ARCHITECTURE.md 原則2）");
		const auto* begin = reinterpret_cast<const std::byte*>(&value);
		return {typeid(T), std::vector<std::byte>(begin, begin + sizeof(T))};
	}
};


/// <summary>
/// 型を知らなくても呼べる窓口。EntityManagerは全部の型をこれでまとめて扱う
/// </summary>
class IComponentStorage {
public:
	virtual ~IComponentStorage() = default;
	virtual void Flush(bool (*isAlive)(Handle<Entity>)) = 0; // 予約を反映する（フレームの境目）
	virtual void DestroyEntity(Handle<Entity> entity) = 0;   // そのEntityの実体と予約を消す（フレームの境目）

	// ===== 型を知らずに中身を写す・戻す（Undo・コピー用）=====
	virtual size_t GetSize() const = 0;
	virtual const void* Find(Handle<Entity> entity) const = 0;              // 持っていなければnullptr
	virtual void SetNowBytes(Handle<Entity> entity, const void* bytes) = 0; // 無ければ作り、あれば上書き（フレームの境目）
};


/// <summary>
/// 型Tのところを、型別の連続した配列で持つ（ARCHITECTURE.md 段階3の「型別の連続配列」）
/// <para>追加・取り外しは予約して、Flushでまとめて反映する</para>
/// </summary>
template<class T>
class ComponentStorage final : public IComponentStorage {
	static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
	static_assert(std::is_class_v<T> && std::is_trivially_copyable_v<T>);
	static_assert(std::is_default_constructible_v<T> && std::is_copy_assignable_v<T>);

public:
	struct Entry {
		Handle<Entity> entity;
		T value;
	};

	T* Get(Handle<Entity> entity) {
		const auto it = indices_.find(Key(entity));
		return it == indices_.end() ? nullptr : &entries_[it->second].value;
	}

	bool RequestAdd(Handle<Entity> entity, const T& initial) {
		const uint64_t key = Key(entity);
		const auto pending = pending_.find(key);
		// 予約後の状態で判断する。削除予約の後なら、現在実体があっても追加できる。
		const bool willExist = pending != pending_.end() ? pending->second.has_value() : indices_.contains(key);
		if (willExist) {
			return false;
		}
		pending_.insert_or_assign(key, initial);
		return true;
	}

	bool RequestRemove(Handle<Entity> entity) {
		const uint64_t key = Key(entity);
		const auto pending = pending_.find(key);
		const bool willExist = pending != pending_.end() ? pending->second.has_value() : indices_.contains(key);
		if (!willExist) {
			return false;
		}
		pending_.insert_or_assign(key, std::nullopt);
		return true;
	}

	bool IsAddPending(Handle<Entity> entity) const {
		const auto it = pending_.find(Key(entity));
		return it != pending_.end() && it->second.has_value();
	}

	void Flush(bool (*isAlive)(Handle<Entity>)) override {
		AssertNotIterating();
		for (const auto& [key, value] : pending_) {
			const Handle<Entity> entity = FromKey(key);
			if (!isAlive(entity)) {
				continue; // 削除済み・世代が違う予約は捨てる
			}
			if (value) {
				SetNow(entity, *value);
			} else {
				RemoveNow(entity);
			}
		}
		pending_.clear();
	}

	void DestroyEntity(Handle<Entity> entity) override {
		AssertNotIterating();
		pending_.erase(Key(entity)); // 実体がまだ無い追加予約も消す
		RemoveNow(entity);
	}

	size_t GetSize() const override { return sizeof(T); }

	const void* Find(Handle<Entity> entity) const override {
		const auto it = indices_.find(Key(entity));
		return it == indices_.end() ? nullptr : &entries_[it->second].value;
	}

	void SetNowBytes(Handle<Entity> entity, const void* bytes) override {
		T value{};
		std::memcpy(&value, bytes, sizeof(T)); // バイト列からTへ戻す
		SetNow(entity, value);
	}

	// 以下はEntityManagerから、フレームの境目でのみ使う。
	// Entity生成時の必須Transformと、予約反映の共通処理。
	void SetNow(Handle<Entity> entity, const T& value) {
		AssertNotIterating();
		if (T* existing = Get(entity)) {
			*existing = value;
			return;
		}
		const size_t index = entries_.size();
		entries_.push_back({entity, value});
		try {
			indices_.emplace(Key(entity), index);
		} catch (...) {
			entries_.pop_back(); // 索引の確保に失敗しても片方だけ増やさない
			throw;
		}
	}

	// 持っている物を全部回す。回している間、この型の配列は増減させない（予約はしてよい）
	template<class F>
	void ForEach(F&& function) {
		IterationScope scope(iterating_);
		for (Entry& entry : entries_) {
			function(entry.entity, entry.value);
		}
	}

private:
	// ForEachの間だけ数を増やす。例外で抜けても必ず戻す
	class IterationScope {
	public:
		explicit IterationScope(uint32_t& count) : count_(count) { ++count_; }
		~IterationScope() { --count_; }
		IterationScope(const IterationScope&) = delete;
		IterationScope& operator=(const IterationScope&) = delete;

	private:
		uint32_t& count_;
	};

	static uint64_t Key(Handle<Entity> entity) { return (static_cast<uint64_t>(entity.generation) << 32) | entity.index; }
	static Handle<Entity> FromKey(uint64_t key) { return {static_cast<uint32_t>(key), static_cast<uint32_t>(key >> 32)}; }

	// 回している配列を増減させると、回している途中の要素がずれる・消える
	void AssertNotIterating() const { MY_ASSERT_MSG(iterating_ == 0, "ForEachで回している型の配列は、回し終わるまで増減できません（RequestAdd / RequestRemove で予約してください）"); }

	void RemoveNow(Handle<Entity> entity) {
		const auto it = indices_.find(Key(entity));
		if (it == indices_.end()) {
			return;
		}
		const size_t index = it->second;
		const size_t last = entries_.size() - 1;
		if (index != last) {
			entries_[index] = std::move(entries_[last]);
			indices_.at(Key(entries_[index].entity)) = index;
		}
		entries_.pop_back();
		indices_.erase(it);
	}

	std::vector<Entry> entries_;                   // 生きている実体だけ。削除は末尾と交換するので順序は不定。
	std::unordered_map<uint64_t, size_t> indices_; // Entityの世代込みキー → 配列位置
	// valueあり＝追加（削除→追加なら置換）、nullopt＝削除。
	std::unordered_map<uint64_t, std::optional<T>> pending_;
	uint32_t iterating_ = 0; // ForEachの途中なら1以上
};
```

### B-2. `MyEngine/Entity/EntityManager.h`（ファイル全体を差し替え）

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Entity/ComponentStorage.h"
#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Entity/ModelRendererComponent.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// Entityと、全種類のComponentの管理
/// <para>Componentは型ごとの ComponentStorage&lt;T&gt; に入れる。ゲーム固有の型も RegisterComponent&lt;T&gt;() すれば同じように使える</para>
/// <para>ARCHITECTURE.md の更新順序のうち、2（ワールド行列）と6（破棄）を担当する</para>
/// </summary>
class EntityManager {
public:
	static void Initialize();
	static void Release();

	// ===== 生成・破棄 =====
	// Entityを作る。TransformComponentはその場で付く（作った直後から Get<TransformComponent> で取れる）
	static Handle<Entity> Create(const std::string& name = "Entity", Handle<Entity> parent = {});
	// 番号（EntityId）を指定して作る。Undoで消したEntityを戻すとき・将来のシーン読み込み用
	static Handle<Entity> CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent);
	// まだ誰も使っていない番号を1つもらう
	static EntityId NewId();
	// 破棄を予約する。実際に消えるのはフレームの最後（子も一緒に消える）
	static void Destroy(Handle<Entity> handle);

	// ===== Entityの取得（受け取ったポインタは使い捨てにする。ARCHITECTURE.md 原則1）=====
	static Entity* Get(Handle<Entity> handle);
	static bool IsAlive(Handle<Entity> handle);
	static Handle<Entity> FindById(EntityId id); // 無ければ無効なHandle（0を渡しても無効なHandle＝root扱いにできる）
	static size_t GetCount();
	static SlotMap<Entity>& GetAll() { return instance_->entities_; }
	// 自分と、親を全部たどって全部が有効ならtrue（UnityのactiveInHierarchy）。Systemはこれがfalseなら処理しない
	static bool IsActiveInHierarchy(Handle<Entity> handle);

	// ===== 親子 =====
	// 親を付け替える。自分の子孫を親にしようとしたときは何もしない（輪になるのを防ぐ）
	static void SetParent(Handle<Entity> child, Handle<Entity> parent);
	// 親をたどって、handleがancestorの子孫かを調べる（handle自身は含まない）
	static bool IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor);
	// 子のHandleを out に詰める（outは使い回す）
	static void GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
	// 親を持たないEntity（一番上）のHandleを out に詰める
	static void GetRoots(std::vector<Handle<Entity>>& out);

	// ===== Component（型ごと）=====
	// 型を登録する。Initializeの後、その型を使う前に呼ぶ。同じ型の再登録は何もしない
	// Editorの無いReleaseでも呼ぶ（Inspectorへの登録＝ComponentEditorRegistryとは別）
	template<class T> static void RegisterComponent() {
		static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
		EnsureInitialized();
		const std::type_index key(typeid(T));
		if (!instance_->components_.contains(key)) {
			instance_->components_.emplace(key, std::make_unique<ComponentStorage<T>>());
		}
	}

	// 反映済みの実体。予約中・未登録・未追加・無効なEntityならnullptr
	template<class T> static T* Get(Handle<Entity> handle) {
		if (!IsAlive(handle)) {
			return nullptr;
		}
		auto* storage = FindStorage<T>();
		return storage ? storage->Get(handle) : nullptr;
	}

	// 追加を予約する。実際に付くのは次の FlushComponentChanges（それまで Get はnullptr）
	template<class T> static bool RequestAdd(Handle<Entity> handle, const T& initial = {}) {
		if (!IsAlive(handle)) {
			return false;
		}
		return RequireStorage<T>().RequestAdd(handle, initial);
	}

	// 必須Transformの削除をRuntime側でも防ぐ。UIだけで禁止して終わりにしない。
	template<class T> static constexpr bool IsRemovable() { return !std::is_same_v<T, TransformComponent>; }

	// 取り外しを予約する。実際に外れるのは次の FlushComponentChanges
	template<class T> static bool RequestRemove(Handle<Entity> handle) {
		if constexpr (!IsRemovable<T>()) {
			return false;
		} else {
			if (!IsAlive(handle)) {
				return false;
			}
			return RequireStorage<T>().RequestRemove(handle);
		}
	}

	template<class T> static bool IsAddPending(Handle<Entity> handle) {
		auto* storage = FindStorage<T>();
		return IsAlive(handle) && storage && storage->IsAddPending(handle);
	}

	// Tを持っているEntityだけを回す。function(Handle<Entity> entity, T& component)
	// 中でしてよいこと：値の変更、追加・取り外し・破棄の予約、Create（ただしT＝Transformのときは不可）
	// 中でしてはいけないこと：Tの配列が増減する操作（Flush系）。受け取った参照を外へ持ち出さない
	template<class T, class F> static void ForEach(F&& function) {
		if (auto* storage = FindStorage<T>()) {
			storage->ForEach(std::forward<F>(function));
		}
	}

	// ===== Componentの写し（Undo・コピー用。登録されている全部の型を、型を知らずに扱う）=====
	// handleが持っているComponentを全部 out に写す
	static void CaptureComponents(Handle<Entity> handle, std::vector<ComponentSnapshot>& out);
	// 写しを戻す。持っていない物は足し、持っている物（Transformなど）は上書きする。フレームの境目で呼ぶ
	static void RestoreComponents(Handle<Entity> handle, const std::vector<ComponentSnapshot>& components);

	// ===== フレーム更新 =====
	// 更新の最初に1回呼ぶ。Componentの追加・取り外しの予約をまとめて反映する
	static void FlushComponentChanges();
	// 更新順序2：ワールド行列を親→子の順で計算する。シーンのUpdateの後に呼ぶ
	static void UpdateTransforms();
	// 更新順序6：破棄予約をまとめて反映する。フレームの最後に呼ぶ
	static void FlushDestroy();

private:
	static constexpr uint32_t kMaxParentDepth = 64; // 親をたどる回数の上限（万一輪になっても止まるように）
	static EntityManager* instance_;

	static void EnsureInitialized();

	template<class T> static ComponentStorage<T>* FindStorage() {
		// const T等で同じtypeidから異なるStorageへキャストしない。
		static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
		return static_cast<ComponentStorage<T>*>(FindStorage(typeid(T)));
	}
	static IComponentStorage* FindStorage(std::type_index type);

	template<class T> static ComponentStorage<T>& RequireStorage() {
		auto* storage = FindStorage<T>();
		MY_ASSERT_MSG(storage != nullptr, "RegisterComponent<T>()を先に呼んでください");
		return *storage;
	}

	// 破棄予約されたEntityと、その子孫を outに集める
	void CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
	// 親を何回たどるとrootに着くか
	uint32_t CalcDepth(Handle<Entity> handle);

	SlotMap<Entity> entities_;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> components_; // 型 → その型の置き場所
	std::unordered_map<EntityId, Handle<Entity>> idToHandle_;                            // 番号 → Handle（生きているEntityだけ）
	EntityId nextId_ = 1;                                                                // 次に配る番号（0は「無し」なので1から）
	std::vector<Handle<Entity>> pendingDestroy_;                                         // 破棄予約
	// 毎フレーム使う作業用の配列（確保し直さないようにメンバで持つ）
	std::vector<std::pair<uint32_t, Handle<Entity>>> updateOrder_; // (深さ, Handle)
	std::vector<Handle<Entity>> destroyWork_;
};
```

### B-3. `MyEngine/Entity/EntityManager.cpp`（ファイル全体を差し替え）

```cpp
#include "EntityManager.h"

#include <algorithm>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"

// 静的メンバ変数
EntityManager* EntityManager::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
void EntityManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new EntityManager();
	// エンジンのComponent。ライトは LightManager::Initialize、ゲーム固有の型はゲームが登録する
	RegisterComponent<TransformComponent>();
	RegisterComponent<ModelRendererComponent>();
	LogManager::Log("Initialized");
}

void EntityManager::Release() {
	EnsureInitialized();
	delete instance_; // 全Storageと、その予約もunique_ptrが解放する
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// 生成・破棄
//=============================================================================
Handle<Entity> EntityManager::Create(const std::string& name, Handle<Entity> parent) {
	return CreateWithId(NewId(), name, parent); // 新しい番号を配って作る
}

Handle<Entity> EntityManager::CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent) {
	EnsureInitialized();
	MY_ASSERT_MSG(id != 0 && !FindById(id).IsValid(), "EntityIdが0か、もう使われています");
	const Handle<Entity> handle = instance_->entities_.Create();
	Entity* entity = instance_->entities_.Get(handle);
	entity->name = name;
	entity->id = id;
	entity->self = handle;
	instance_->idToHandle_[id] = handle;
	// 番号を指定して作ったときも、この先配る番号とぶつからないようにする
	instance_->nextId_ = (std::max)(instance_->nextId_, id + 1);
	// 全Entityの必須データ。作った直後から取れるように、予約せずその場で作る
	// （ForEach<TransformComponent> の途中ならここでアサートが出る）
	RequireStorage<TransformComponent>().SetNow(handle, TransformComponent{});
	// 親は SetParent を通す（輪になっていないかを見るため）
	if (parent.IsValid()) {
		SetParent(handle, parent);
	}
	return handle;
}

EntityId EntityManager::NewId() { return instance_->nextId_++; }

void EntityManager::Destroy(Handle<Entity> handle) { instance_->pendingDestroy_.push_back(handle); }


//=============================================================================
// 取得
//=============================================================================
Entity* EntityManager::Get(Handle<Entity> handle) { return instance_->entities_.Get(handle); }

bool EntityManager::IsAlive(Handle<Entity> handle) { return instance_ && instance_->entities_.IsAlive(handle); }

Handle<Entity> EntityManager::FindById(EntityId id) {
	auto it = instance_->idToHandle_.find(id);
	return (it != instance_->idToHandle_.end()) ? it->second : Handle<Entity>{};
}

size_t EntityManager::GetCount() { return instance_->entities_.Size(); }

bool EntityManager::IsActiveInHierarchy(Handle<Entity> handle) {
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		const Entity* entity = instance_->entities_.Get(current);
		if (!entity || !entity->isActive) {
			return false;
		}
		if (!entity->parent.IsValid()) {
			return true; // 一番上まで全部有効だった
		}
		current = entity->parent;
	}
	return false; // 深すぎる（輪になっている）
}

IComponentStorage* EntityManager::FindStorage(std::type_index type) {
	if (!instance_) {
		return nullptr;
	}
	const auto it = instance_->components_.find(type);
	return it == instance_->components_.end() ? nullptr : it->second.get();
}


//=============================================================================
// 親子
//=============================================================================
void EntityManager::SetParent(Handle<Entity> child, Handle<Entity> parent) {
	Entity* entity = instance_->entities_.Get(child);
	if (!entity) {
		return;
	}
	// 自分自身、または自分の子孫を親にすると、親子が輪になって無限ループする
	if (child == parent || IsDescendantOf(parent, child)) {
		LogManager::Warning("親子が輪になる付け替えは無視しました");
		return;
	}
	entity->parent = parent;
}

bool EntityManager::IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor) {
	if (!ancestor.IsValid()) {
		return false;
	}
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = instance_->entities_.Get(current);
		if (!entity) {
			return false;
		}
		if (entity->parent == ancestor) {
			return true;
		}
		current = entity->parent;
	}
	return false;
}

void EntityManager::GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (entity.parent == handle) {
			out.push_back(entity.self);
		}
	}
}

void EntityManager::GetRoots(std::vector<Handle<Entity>>& out) {
	out.clear();
	for (const Entity& entity : instance_->entities_) {
		if (!entity.parent.IsValid()) {
			out.push_back(entity.self);
		}
	}
}


//=============================================================================
// Componentの写し（Undo・コピー用）
//=============================================================================
void EntityManager::CaptureComponents(Handle<Entity> handle, std::vector<ComponentSnapshot>& out) {
	out.clear();
	if (!IsAlive(handle)) {
		return;
	}
	// Inspectorに出していない（Editorを登録していない）型も含めて、持っている物を全部写す
	for (const auto& [type, storage] : instance_->components_) {
		if (const void* component = storage->Find(handle)) {
			const auto* bytes = static_cast<const std::byte*>(component);
			out.push_back({type, std::vector<std::byte>(bytes, bytes + storage->GetSize())});
		}
	}
}

void EntityManager::RestoreComponents(Handle<Entity> handle, const std::vector<ComponentSnapshot>& components) {
	EnsureInitialized();
	if (!IsAlive(handle)) {
		return;
	}
	for (const ComponentSnapshot& component : components) {
		IComponentStorage* storage = FindStorage(component.type);
		MY_ASSERT_MSG(storage != nullptr && storage->GetSize() == component.bytes.size(), "写したComponentの型が登録されていません");
		storage->SetNowBytes(handle, component.bytes.data());
	}
}


//=============================================================================
// 予約の反映
//=============================================================================
void EntityManager::FlushComponentChanges() {
	EnsureInitialized();
	for (auto& [type, storage] : instance_->components_) {
		storage->Flush(&EntityManager::IsAlive);
	}
}


//=============================================================================
// 更新順序2：ワールド行列（親 → 子）
//=============================================================================
void EntityManager::UpdateTransforms() {
	EntityManager& self = *instance_;

	// --- 深さ（親をたどる回数）を数えて、浅い順に並べる ---
	// 親の行列が先に確定していないと、子の行列が1フレーム遅れてしまう
	self.updateOrder_.clear();
	for (const Entity& entity : self.entities_) {
		self.updateOrder_.emplace_back(self.CalcDepth(entity.self), entity.self);
	}
	std::sort(self.updateOrder_.begin(), self.updateOrder_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

	// --- 浅い順に、自分の行列を作って親の行列を掛ける ---
	for (const auto& [depth, handle] : self.updateOrder_) {
		const Entity* entity = self.entities_.Get(handle);
		TransformComponent* transform = Get<TransformComponent>(handle);
		if (!entity || !transform) {
			continue;
		}
		// 自分の分（大きさ → 回転 → 位置）
		transform->worldMatrix = MakeAffineMatrix(transform->scale, transform->rotation, transform->translation);
		// 親がいれば、親のワールド行列を掛ける（親は先に確定している）
		if (const TransformComponent* parentTransform = Get<TransformComponent>(entity->parent)) {
			transform->worldMatrix = transform->worldMatrix * parentTransform->worldMatrix;
		}
	}
}

uint32_t EntityManager::CalcDepth(Handle<Entity> handle) {
	uint32_t depth = 0;
	Handle<Entity> current = handle;
	for (uint32_t i = 0; i < kMaxParentDepth; ++i) {
		Entity* entity = entities_.Get(current);
		if (!entity || !entity->parent.IsValid()) {
			break;
		}
		++depth;
		current = entity->parent;
	}
	return depth;
}


//=============================================================================
// 更新順序6：破棄
//=============================================================================
void EntityManager::FlushDestroy() {
	EnsureInitialized();
	EntityManager& self = *instance_;
	if (self.pendingDestroy_.empty()) {
		return;
	}
	// 親を消したら子も消す。消しながら探すと崩れるので、先に全部集める
	self.destroyWork_.clear();
	for (Handle<Entity> handle : self.pendingDestroy_) {
		self.CollectDescendants(handle, self.destroyWork_);
	}
	self.pendingDestroy_.clear();

	for (Handle<Entity> handle : self.destroyWork_) {
		Entity* entity = self.entities_.Get(handle);
		if (!entity) {
			continue; // 親と一緒に2回集まったときは、2回目は消えている
		}
		// ゲームが登録した型も含め、実体と未反映の予約をすべて消す
		for (auto& [type, storage] : self.components_) {
			storage->DestroyEntity(handle);
		}
		self.idToHandle_.erase(entity->id);
		self.entities_.Destroy(handle);
	}
}

void EntityManager::CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out) {
	if (!entities_.IsAlive(handle)) {
		return;
	}
	out.push_back(handle);
	// 子を探して、その子孫も集める
	for (const Entity& entity : entities_) {
		if (entity.parent == handle) {
			CollectDescendants(entity.self, out);
		}
	}
}

void EntityManager::EnsureInitialized() { MY_ASSERT_MSG(instance_ != nullptr, "EntityManager::Initialize()を先に呼んでください"); }
```

`ComponentStorage.cpp`（中身は `#include` 1行だけ）はそのまま残してよい。

---

## C. `EditorHistory`：写しを EntityManager 経由に

### C-1. `MyEngine/Editor/History/EditorHistory.h`

include に2行足す。

```cpp
#pragma once
#include <string>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Entity/ComponentStorage.h"
#include "MyEngine/Entity/Entity.h"
```

`RequestCreate` の宣言を置き換える（Light.md Step 6の Create > Light で使う）。

```cpp
	// parentが無効ならroot。componentsを渡すと、それを付けた状態で作る（Transformを渡すと初期位置・向きになる）
	static void RequestCreate(const std::string& name, Handle<Entity> parent, std::vector<ComponentSnapshot> components = {});
```

`RecordComponentChange` の上のコメントは、手順6の後の意味に合わせる。

```cpp
	// Inspectorが描く前と描いた後の中身を渡す。変わっていたら、Component全体の「編集前」と「編集後」を覚える
```

### C-2. `MyEngine/Editor/History/EditorHistory.cpp`

**① `struct ComponentData` を消し、`EntityData` を置き換える**

```cpp
struct EntityData {
	EntityId id = 0;
	EntityId parent = 0; // 0ならroot
	std::string name;
	bool isActive = true;
	std::vector<ComponentSnapshot> components; // 持っているComponent全部（Inspectorに出していない型も含む）
};
```

**② `CaptureOne` を置き換える**（登録表を回す `for` の代わりに1行）

```cpp
// Entity1つ分。EntityManagerに登録されている全種類のComponentを見て、持っている物を写す
EntityData CaptureOne(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	EntityData data;
	data.id = entity->id;
	data.parent = IdOf(entity->parent);
	data.name = entity->name;
	data.isActive = entity->isActive;
	EntityManager::CaptureComponents(handle, data.components);
	return data;
}
```

**③ `Restore` の2つ目の `for` と、その後の `FlushComponentChanges` を置き換える**

```cpp
	for (const EntityData& data : tree) {
		Handle<Entity> handle = EntityManager::CreateWithId(data.id, data.name, EntityManager::FindById(data.parent));
		EntityManager::Get(handle)->isActive = data.isActive;
		// 予約せずその場で付ける（Flushの中＝フレームの境目なので安全）。Transformのように作った時点で持っている物は上書き
		EntityManager::RestoreComponents(handle, data.components);
	}
	HierarchyWindow::SetSelected(EntityManager::FindById(root.id));
	return true;
}
```

予約（`RequestAdd`）ではなく、その場で付ける（`SetNowBytes`）。Undoは `EditorHistory::Flush`（フレームの境目）の中で動くので安全。Transformのように作った時点で持っている物は上書きになる。

**④ `ApplyEdits` の上のコメント**

```cpp
// Component全体を書き戻す（undoならbefore、redoならafter）
```

**⑤ `RequestCreate` を置き換える**

```cpp
void EditorHistory::RequestCreate(const std::string& name, Handle<Entity> parent, std::vector<ComponentSnapshot> components) {
	if (parent.IsValid() && !EntityManager::IsAlive(parent)) {
		return;
	}
	requests.push_back([name, parentId = IdOf(parent), components = std::move(components)] {
		EntityData data;
		data.id = EntityManager::NewId();
		data.parent = parentId;
		data.name = name;
		data.components = components; // 空なら、作った時点のTransform（初期値）だけ
		AddTree({data});
	});
}
```

これで `EditorHistory.cpp` は `ComponentEditorRegistry` を使わなくなる（`#include "MyEngine/Editor/Inspector/ComponentEditor.h"` は、Add/Remove Componentと編集の記録で `ComponentEditor` を使うので残す）。

---

## D. `MyEngine/Entity/ModelRenderSystem.cpp`（ファイル全体を差し替え）

全Entityを回して「ModelRendererを持っているか」を1つずつ調べるのをやめ、`ForEach<ModelRendererComponent>` で持っている物だけを回す。親をたどる判定は `IsActiveInHierarchy` と `IsDescendantOf` に置き換えたので、`IsDrawableInRoot` は消える。

```cpp
#include "ModelRenderSystem.h"

#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/IBL/IBLEnvironment.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"


//=============================================================================
// ModelRendererComponentの描画
//=============================================================================
void ModelRenderSystem::Draw(Handle<Entity> root, Camera* camera, IBLEnvironment* environment, const std::wstring& windowTitle) {
	if (!camera || windowTitle.empty()) {
		return;
	}

	// ModelRendererComponentを持つEntityだけを回す（全Entityの中から探さない）
	EntityManager::ForEach<ModelRendererComponent>([&](Handle<Entity> entity, const ModelRendererComponent& render) {
		if (!render.enabled || render.modelHandle == 0) {
			return;
		}
		// 自分か親のどれかが無効なら描かない
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		// rootを指定したときは、root自身とその子孫だけ（rootが無効なHandleなら全部が対象）
		if (root.IsValid() && entity != root && !EntityManager::IsDescendantOf(entity, root)) {
			return;
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		if (!transform || !ModelManager::GetModelAsset(render.modelHandle)) {
			return;
		}
		if (render.shadingType == ShadingType::PBR && (!environment || environment->GetParametersAddress() == 0)) {
			return;
		}

		Renderer::ModelConfig config{};
		config.modelHandle = render.modelHandle;
		config.textureHandle = render.textureHandle;
		config.color = render.color;
		config.uvTransform = render.uvTransform;
		config.material = render.material;
		config.shadingType = render.shadingType;
		config.blendMode = render.blendMode;
		config.rasterizerType = render.rasterizerType;
		config.depthMode = render.depthMode;
		config.billboard = render.billboard;
		config.worldMatrix = &transform->worldMatrix;
		config.camera = camera;
		config.env = environment;
		config.windowTitle = windowTitle;
		Renderer::DrawModel(config);
	});
}
```

ここまででエンジンはビルドが通る（ゲームは手順Eまで写してから）。

---

## E. ゲーム側：ゲーム固有Component `Spin` で確かめる

`Health` の例は数字が変わるだけで見て分からないので、**Play中に回り続ける** `SpinComponent` にした。ゲーム固有Componentの作り方の見本も兼ねる。

| 何を | どこに |
|---|---|
| データ（`SpinComponent`） | `GameComponents.h`。エンジンは中身を知らない |
| 置き場所の登録（`RegisterComponent`） | `GameComponents::Register`。Releaseでも必要 |
| Inspectorの表示（`SpinEditor`） | `GameComponents.cpp` の中、`#ifdef USE_IMGUI` だけ |
| 処理（System） | `GameComponents::Update`。`ForEach<SpinComponent>` で持っている物だけ回す |

### E-1. 新規：`CG3_Project/GameComponents.h`

```cpp
#pragma once
#include <MyEngine/Math/Vector3.h>

// ゲーム固有のComponent（エンジンは中身を知らない）
// 置き場所はEntityManager、Inspectorの表示はGameComponents.cppのEditorが担当する


/// <summary>
/// 回り続ける（Play中だけ。Stopすると元の向きに戻る）
/// </summary>
struct SpinComponent {
	Vector3 speed = {0.0f, 90.0f, 0.0f}; // 1秒に回る角度（度）。軸ごと
};


namespace GameComponents {
/// <summary>
/// ゲーム固有のComponentを登録する。シーンのInitializeの最初に呼ぶ（2回目からは何もしない）
/// </summary>
void Register();

/// <summary>
/// ゲーム固有のComponentの処理（System）。シーンのUpdateで呼ぶ
/// </summary>
void Update(float deltaTime);
} // namespace GameComponents
```

### E-2. 新規：`CG3_Project/GameComponents.cpp`

```cpp
#include "GameComponents.h"

#include <numbers>

#include <MyEngine/Entity/EntityManager.h>

#ifdef USE_IMGUI
#include <memory>

#include <MyEngine/Editor/Inspector/ComponentEditor.h>
#include <externals/imgui/imgui.h>
#endif

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン

#ifdef USE_IMGUI
/// <summary>
/// SpinComponentのInspector（Add Component → Gameplay → Spin で付けられる）
/// </summary>
class SpinEditor final : public TypedComponentEditor<SpinComponent> {
public:
	const char* GetName() const override { return "Spin"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Gameplay; }

protected:
	void DrawComponent(SpinComponent& spin) const override { ImGui::DragFloat3("Speed (deg/s)", &spin.speed.x, 1.0f); }
};
#endif
} // namespace


//=============================================================================
// 登録
//=============================================================================
void GameComponents::Register() {
	// 置き場所（Releaseでも必要）
	EntityManager::RegisterComponent<SpinComponent>();
#ifdef USE_IMGUI
	// Inspectorでの表示（エディタだけ）
	ComponentEditorRegistry::Register(std::make_unique<SpinEditor>());
#endif
}


//=============================================================================
// 更新
//=============================================================================
void GameComponents::Update(float deltaTime) {
	// SpinComponentを持っているEntityだけを回す
	EntityManager::ForEach<SpinComponent>([deltaTime](Handle<Entity> entity, const SpinComponent& spin) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (TransformComponent* transform = EntityManager::Get<TransformComponent>(entity)) {
			transform->rotation += spin.speed * (kDegToRad * deltaTime);
		}
	});
}
```

2つともゲームのプロジェクト（CG3_Project）に追加する（ソリューションエクスプローラーで右クリック → 追加 → 既存の項目）。

### E-3. `CG3_Project/GameScene.cpp`

**① include**（先頭の `#include "GameScene.h"` の下）

```cpp
#include "GameScene.h"
#include "GameComponents.h"
```

`#include <MyEngine/Time/Time.h>` も足す（`Time::GetDeltaTime` 用。`Scene/Skybox.h` と `UI/GlobalVariables.h` の間）。

**② `Initialize` の最初**（カメラより前）

```cpp
void GameScene::Initialize() {
	// ゲーム固有のComponentを登録する（Entityに付ける前に。Stopで作り直すたびに呼ばれるが、2回目からは何もしない）
	GameComponents::Register();

	// カメラ
```

**③ モンスターボールを作る所**（`RequestAddModelRenderer` は手順Bで消えたので `RequestAdd<T>` に）

```cpp
	// モンスターボール（前の MonsterBall クラスの代わり。位置や向きはInspectorで触る）
	ModelRendererComponent monsterBall;
	monsterBall.modelHandle = ModelManager::Load("resources/monsterBall/monsterBall.gltf");
	monsterBall.shadingType = ShadingType::PBR;
	const Handle<Entity> ball = EntityManager::Create("MonsterBall", sceneRoot_);
	EntityManager::RequestAdd<ModelRendererComponent>(ball, monsterBall);
	EntityManager::RequestAdd<SpinComponent>(ball, SpinComponent{{0.0f, 45.0f, 0.0f}}); // Play中だけ回る（ゲーム固有Componentの確認用）
```

**④ `Update` の「ゲームオブジェクト」**

```cpp
	// --- ゲームオブジェクト ---
	particles_->Update();
	stage_->Update();
	GameComponents::Update(Time::GetDeltaTime()); // ゲーム固有のComponent（Spinなど）
```

### E-4. ビルドの前に

- エンジンをビルドすると、ヘッダーがゲーム側（`CG3_Project/MyEngine/include`）へ配られる。今回ファイルの名前は変わらないので、古いヘッダーを消す必要は無い
- エンジン → ゲームの順に、Debug / Release 両方ビルドする

### E-5. 実行して確認すること

1. 今まで通りMonsterBallが描かれる（`ModelRenderSystem` を書き直したので）。Hierarchyで親の `GameScene` のチェックを外すと消える
2. MonsterBallを選ぶと、Inspectorに **Transform → Model Renderer → Spin** の順に出る（Spinはゲームが先に登録しているが、カテゴリ順なので一番下）
3. **Play** を押すとMonsterBallがY軸で回る。**Stop** で元の向きに戻る（シーンの作り直し）。Spinの Speed を変えると速さが変わる
4. 別のEntityに Add Component → **Gameplay** → **Spin** が付けられる。Remove Component → Undo で値ごと戻る
5. MonsterBallを **Delete → Ctrl+Z** で、Model Renderer と Spin が値ごと戻る。**Ctrl+D** で複製するとSpinも付いてくる
6. Editorの無い型も戻ることの確認（任意）：`GameComponents::Register` の `ComponentEditorRegistry::Register` の行を一時的にコメントにする → Inspectorに Spin が出なくなる → MonsterBall を Delete → Ctrl+Z → Play で**まだ回る**（手順Bの前は、ここでSpinが消えていた）。確認したら戻す

### 確認したこと（2026-09-19、Claude）

- 手順A〜Eまで写した状態：エンジン77ファイル・ゲーム5ファイルを `/W4` の Debug / Release でコンパイル。エラー0、新しい警告0
- Light.md Step 6まで写した状態：エンジン78ファイル・ゲーム5ファイル、同じくエラー0・新しい警告0
- 本物の `EntityManager` / `ComponentStorage` / `EditorHistory` / `ComponentEditorRegistry` / `LightManager` / `ModelRenderSystem` / `GameComponents` を使うテスト（GPU・ImGui・ログだけ差し替え）で **39,307項目すべて通過**。確かめた主なもの：
  - ゲームのEditorを先に登録しても、並びは Transform → Point Light → Health
  - 予約の規則（二重追加・追加→削除・削除→追加・Transformは外せない・破棄した後の同じスロットに古い予約が付かない）
  - `ForEach<Health>` の中で `Create` と `RequestAdd<Health>` ができる
  - 4000回の乱数の操作で、実体と期待値が毎回一致
  - Editorの無い型（Velocity）が、親子ごとの削除→Undo・Redo・複製で値ごと戻る／付いてくる。複製した後の値は別々
  - Inspectorの編集10フレーム分が1件のUndoになる。Remove Component → Undo で値ごと戻る
  - `ModelRenderSystem`：親が無効なら描かない、root を指定すればその下だけ
  - Spin：0.5秒で45度回る。無効なEntityは回らない
- 画面の見た目・実際のマウス操作は確かめていない（上の「実行して確認すること」で見てほしい）

---

## これからの設計：ゲーム固有Componentの書き方（2026-09-20 の相談）

> **決まった（2026-09-20）。** この節は「なぜこの形にしたか」の記録。写す物は下の**手順F**にある。
> 変えた点：カテゴリの中の並びを「登録した順」から「名前順」にした（ファイルが増えても並びが変わらないように）。

### 今の書き方の面倒な所（指摘されたこと）

1. `ImGui::DragFloat3(...)` を直接書く。エディタの都合（ポインタ・速さ・最小最大の順番）を毎回思い出さないといけない
2. ゲーム固有Componentのカテゴリは `Gameplay` で決まりなのに、毎回書く
3. カテゴリの下にサブカテゴリが無い（Gameplayが増えると一列に並ぶ）
4. `Update` に `ForEach` と `if (Get<TransformComponent>(...))` の決まり文句が要る。書きたいのは中の1行だけ
5. 登録（`RegisterComponent` と `ComponentEditorRegistry::Register`）を自分で書く

### 案：ファイル1つ＝Component1つ。「データ・見せ方・処理」だけ書く

```cpp
// CG3_Project/Source/Components/Spin.h
#pragma once
#include <MyEngine/Component/GameComponent.h>

// ① データ
struct Spin {
	Vector3 speed = {0.0f, 90.0f, 0.0f}; // 1秒に回る角度（度）
};

// ② Inspectorでの見せ方。ImGuiは出てこない
//    カテゴリは Gameplay で固定。"Movement" はその下のサブカテゴリ（省略してもよい）
COMPONENT(Spin, "Movement") {
	ui.Field("Speed (deg/s)", value.speed);                      // 型で見た目が決まる（Vector3ならDragFloat3）
	ui.Field("Speed Y", value.speed.y, Range(-720.0f, 720.0f));  // 範囲を付けるとスライダー
}

// ③ 毎フレームの処理。引数の型を見て「Spinと Transform の両方を持つEntity」だけ呼ばれる
void SpinUpdate(Spin& spin, TransformComponent& transform, float dt) {
	transform.rotation += spin.speed * (kDegToRad * dt);
}
SYSTEM(SpinUpdate);
```

これで、`EntityManager::RegisterComponent` も `ComponentEditorRegistry::Register` も `GameComponents::Update` も書かなくてよくなる（今の `GameComponent.h` / `.cpp` は、Componentごとのファイルに分かれて消える）。

### 中で何が起きるか

| 書く物 | 仕組み |
|---|---|
| `COMPONENT(Spin, "Movement") { … }` | `void Describe_Spin(ComponentUI& ui, Spin& value)` という関数の定義になり、同時に `static ComponentRegistrar<Spin> registrar_Spin(…)` が1つ作られる。この変数の初期化は `main` より前に走るので、そこで「登録待ちの表」に並び、`EntityManager::Initialize` の後にまとめて登録される |
| `ui.Field(名前, 値)` | 型ごとに `ImGui::DragFloat3` などを呼ぶだけの薄い包み（`float` / `Vector3` / `bool` / `int` / enum / 色）。**ここに「保存・読み込み」も足せる**のが大きい |
| `SYSTEM(SpinUpdate)` | 関数の引数の型（`Spin` と `TransformComponent`）をテンプレートで取り出し、「両方持っていて、親まで有効なEntity」を回す処理を作って、毎フレームの呼び出し表に登録する。`dt` は最後の引数が `float` なら渡す |

C++ならではの注意：静的初期化での自動登録が効くのは**そのファイルがリンクされるとき**だけ。ゲーム（exe）のcppは必ずリンクされるので効くが、エンジン（.lib）の中のファイルは、誰も参照していないとリンカに捨てられて登録も消える。なのでエンジン側のComponentは今までどおり `InspectorWindow::Initialize` で明示的に登録する。

### サブカテゴリ

`ComponentCategory` のenumをやめて、**文字列の道**（`"Gameplay/Movement"`、`"Rendering3D"`）にする。Add Componentのメニューは `/` で分けて入れ子にする。並び順は、先頭の決まった順（Core → Rendering3D → … → Gameplay）を表で持ち、その中はアルファベット順。

### 「ファイルを作って、ドラッグ＆ドロップで付ける」はどうか

**C++では、ファイルを作っただけでは使えない。** UnityのC#はスクリプトを実行中に読み直せるが、C++はビルドし直して起動し直さないと、新しい型はプログラムの中に存在しない（Unrealも同じで、C++クラスを足したらビルドが要る）。
なので現実的にはこうなる。

1. Projectパネル（`Source/Components/` の一覧）で右クリック → **Create C++ Component** → 名前を入れる
2. ひな形（上の①②③が書いてある）を作って、Visual Studio で開く
3. 中身を書く → **ビルドし直して起動し直す**
4. それ以降、そのファイルを Inspector にドラッグ＆ドロップすると付けられる（Add Component から選ぶのと同じこと）

つまり「作ってすぐドラッグ＆ドロップ」はできない。値打ちがあるのは「ひな形を作ってくれる」ことと「ファイル一覧が見える」ことで、**付ける操作そのものは Add Component とほぼ同じ**。
ファイルを増やすのを楽にするだけなら、vcxprojに `Source\Components\*.h` `*.cpp` のワイルドカードを1行書いておけば、**ファイルを置くだけでビルドに入る**（プロジェクトを触らなくてよい）。これは今すぐできる。

### おすすめの順番（私の意見）

| 順 | やること | 理由・規模 |
|---|---|---|
| 1 | **書き味レイヤー**（①②③・サブカテゴリ・自動登録） | 一番効く。これから作るゲームのComponentが全部これで書ける。私の見積もりで1バッチ（写経は半日くらい） |
| 2 | **シリアライズ（シーンの保存・読み込み）** | ②の `ui.Field` の並びが、そのまま保存する項目の並びになる（1で作ると2が安く付く）。Play→Stopで編集が消えない、プレハブ、起動時のシーン読み込みにつながる。エディタとして一番大きい穴。保存ファイルに版番号を付けておけば、後でTransformを変えても読める |
| 3 | **クオータニオン** | Inspectorの表示は度のまま、中身だけ変える（ジンバルロックが消える、回転の補間ができる）。2で版番号を付けておけば、古い保存ファイルも読める |
| 4 | Projectパネル＋ひな形作成＋ドラッグ＆ドロップ | 1〜3の後。ファイル一覧は、モデル・テクスチャを選ぶのにも使えるようになる |
| 5 | Input / Sound | 今あるもので動いているので、必要になってから（「ジャンプ」などの名前で入力を取る層、`AudioSourceComponent`） |

---

## ドラッグ＆ドロップ・C#・マルチスレッドの優先順位（2026-09-20 の相談）

### まず、C#が速いのは「起動」ではなく「直してから見るまで」

- C++より速いのは**スクリプトを直して結果を見るまで**の時間。C#（.NET）は実行中にアセンブリを読み直せるので、Unityは再起動せずに反映できる。
- 起動そのものはC#の方が**遅い**（JITとGCの用意がある）。だから商用エンジンは「重い処理＝C++／書き換えが多い処理＝スクリプト」に分けている。
- つまりC#を入れる値打ちはドラッグ＆ドロップではなく、**ビルドと再起動が要らなくなること**。

### C#を載せるとどうなるか（正直な見積もり）

| 要る物 | 中身 |
|---|---|
| .NETの埋め込み | CoreCLR か Mono を自分のexeの中で起動する |
| 橋渡し（バインディング） | C#から `EntityManager` / `TransformComponent` / `InputManager` を触れるようにする。関数ごとに手書きか自動生成 |
| 型の対応 | `Vector3` などのメモリ配置合わせ、文字列・配列の受け渡し |
| GCとの同居 | C#側が持っている物をGCに消されないよう固定する。C++側の寿命と噛み合わせる |
| 読み直し | 読み込み文脈（AssemblyLoadContext）を作り直して、持っている状態を移し替える。**ここが一番むずかしい** |
| デバッグ | Visual StudioでC#側にブレークポイントを張れるようにする |
| ビルド | C#プロジェクトのビルドをエディタから走らせる |

**これだけで数ヶ月かかる。しかもその間ゲームは1歩も進まない。** Unity・Unrealが専任チームで作っている部分なので、**今はやらない方がいい**と思う。

### 同じ効果を、はるかに安く手に入れる順番

1. **シリアライズ**（次の予定）… 起動し直しても同じ状態に戻る。**体感の8割はこれ。** Unityで再生前に戻れるのも、結局は保存があるから
2. **ビルドを速くする（PCH）**… 実測で1ファイル 2.5秒 → 0.2秒（下の表）
3. **エディタからビルド＋自動起動（進捗バー付き）**… ボタン1つで「ビルド → 終わったら起動 → さっきのシーンを開く」。10秒で戻ってこられる
4. どうしてもホットリロードしたいなら、C#より**C++のDLL差し替え**の方が現実的（ゲームのコードだけDLLにして実行中に入れ替える）。ただしvtableとstatic変数で事故りやすい。既製品では Live++（有料）がこれをやっている

### で、ドラッグ＆ドロップは？

- 「Projectパネルからファイルをドラッグして付ける」**操作そのものはビルドと関係なく作れる**（Add Componentと同じ処理をドロップで呼ぶだけ。半バッチくらい）
- ただし**新しく作ったファイルは、ビルドして起動し直すまでメニューにもD&Dにも出てこない**。これはC++の性質で、C#を入れない限り変わらない
- なので「作ってすぐ付けられる」は諦めて、「**ビルド → 起動 → 付ける、が10秒で回る**」を目指すのが現実的

### マルチスレッドとの優先順位（私のおすすめ）

| 順 | やること | なぜ |
|---|---|---|
| 1 | **シリアライズ** | 無いと作ったシーンが消える。エディタとして一番大きい穴 |
| 2 | **クオータニオン** | 回転の破綻を先に潰す。保存に版番号を付けておけば後から入れ替えられる |
| 3 | **ビルドの高速化（PCH）＋エディタからビルド（進捗バー）** | 毎日効く。PCHは半バッチ |
| 4 | Projectパネル＋ドラッグ＆ドロップ | 1〜3の後。ファイル一覧はモデル・テクスチャを選ぶのにも使える |
| 5 | マルチスレッド | **今は要らない。まず測る**（Profilerがある）。60fpsで困っていないなら入れる意味がない。入れるなら「アセット読み込みだけ別スレッド」（画面が固まらなくなる・範囲が小さい・事故りにくい）。UpdateやDrawの並列化は、データの持ち方まで含めた大工事なので、ゲームの形が決まってから |
| 99 | C#スクリプティング | やらない。やるなら就職してからの勉強として |

補足：DirectX12は元々コマンドリストを複数スレッドで作れる作りなので、将来やるなら「描画コマンドの並列生成」が本筋。ただし**今どこが遅いのかを測っていない**ので、測る前に手を出すと高確率で無駄になる。

### ビルド時間の実測（2026-09-20、このPC、16スレッド）

| 何を | Debug | Release |
|---|---|---|
| エンジン全部（80ファイル、`/MP`） | 27.8秒 | 43.9秒 |
| ゲーム全部（5ファイル） | 6.0秒 | 6.0秒 |
| **Componentを1つ直したとき（1ファイル）** | **約2.5秒** | 同じくらい |

ゲームは**エンジンのlibを作り直さない**（`MyEngine/lib` にあるものを使う）ので、Componentを触るだけなら「1ファイル 2.5秒 ＋ リンク数秒」で終わる。

1ファイル2.5秒の中身を測ると：

| 何に使っているか | 時間 |
|---|---|
| `GameComponent.h` から先のヘッダを読む | 2.4秒 |
| `Spin.cpp` 自身の中身 | 0.2秒 |

**ほぼ全部ヘッダの読み込み。** 犯人は `MyAssert.h` → `LogManager.h` → spdlog/fmt（テンプレートが巨大）。
→ **PCHを入れると 2.5秒 → 0.2秒**（同じファイル・同じ環境で実測）。Componentが50個になっても「1つ直して数秒」を保てる。

### ビルド中の進捗バー

できる。MSBuildを子プロセスで動かして、出てくる行を読むだけ。

- `msbuild CG3_Project.sln /p:Configuration=Debug /m /nologo /v:m` を `CreateProcess` ＋パイプで起動する
- `cl` は**コンパイルしたファイル名を1行ずつ出す**ので、`○○.cpp` の行を数えれば `3 / 12` が作れる（Unrealの `[3/12]` と同じやり方）
- 全体の数は、始める前に「objより新しいcpp」を数えれば近い値が出る。**正確でなくてよい**（UnityやUnrealのバーも正確ではない）
- 終わったら終了コードを見て、0なら `CG3_Project.exe` を起動、0以外ならログの `error` 行だけをエディタに出す

これは「エディタからビルド」機能とセットで、上の優先順位の3番。

---

## 手順F：書き味レイヤー（COMPONENT / SYSTEM）

> **2026-09-20 に書き、2026-09-21 に直した（F-16）。** コンパイル（エンジン80ファイル＋ゲーム、Debug/Release）と動作テスト（44項目）は通してある。
> 画面の見た目は確かめていないので、写したら「F-12 確認すること」を見てほしい。

### 何が変わるか

ゲーム固有Componentを作るとき、今はこう書いている（`GameComponent.h` ＋ `GameComponent.cpp` の2ファイル、約60行）。

```cpp
struct SpinComponent { Vector3 speed = {0.0f, 90.0f, 0.0f}; };
namespace GameComponents { void Register(); void Update(float deltaTime); }
// .cpp 側
class SpinEditor final : public TypedComponentEditor<SpinComponent> {
    const char* GetName() const override { return "Spin"; }
    ComponentCategory GetCategory() const override { return ComponentCategory::Gameplay; }
    void DrawComponent(SpinComponent& spin) const override { ImGui::DragFloat3("Speed (deg/s)", &spin.speed.x, 1.0f); }
};
void GameComponents::Register() {
    EntityManager::RegisterComponent<SpinComponent>();
    ComponentEditorRegistry::Register(std::make_unique<SpinEditor>());
}
void GameComponents::Update(float deltaTime) {
    EntityManager::ForEach<SpinComponent>([deltaTime](Handle<Entity> entity, const SpinComponent& spin) {
        if (!EntityManager::IsActiveInHierarchy(entity)) { return; }
        if (TransformComponent* transform = EntityManager::Get<TransformComponent>(entity)) {
            transform->rotation += spin.speed * (kDegToRad * deltaTime);
        }
    });
}
// さらに GameScene::Initialize で Register()、GameScene::Update で Update() を呼ぶ
```

手順Fの後はこうなる（`Source/Components/Spin.cpp` 1ファイル、17行。**シーン側に足す物は無い**）。

```cpp
#include <MyEngine/Component/GameComponent.h>

// ① データ
struct Spin {
	Vector3 speed = {0.0f, 90.0f, 0.0f}; // 1秒に回る角度（度）。軸ごと
};

// ② Inspectorでの見せ方（カテゴリは Gameplay で固定。"Movement" はその下のサブカテゴリ）
COMPONENT(Spin, "Movement") {
	ui.Field("Speed (deg/s)", value.speed, Tip("1秒に回る角度。軸ごと"));
}

// ③ 毎フレームの処理。Spin と Transform の両方を持つEntityだけ呼ばれる
void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) {
	transform.rotation += spin.speed * (kDegToRad * deltaTime);
}
SYSTEM(SpinUpdate);
```

`RegisterComponent` も `ComponentEditorRegistry::Register` も `ForEach` も `IsActiveInHierarchy` も `ImGui` も `#ifdef USE_IMGUI` も書かない。**ファイルを `Source/Components/` に置くだけで、Add Componentのメニューに出る。**

### 仕組み（読まなくても写せるが、知っておくと直せる）

| 書く物 | 中で何が起きるか |
|---|---|
| `COMPONENT(Spin, "Movement") { … }` | `inline void Describe_Spin(ComponentUI& ui, Spin& value)` の定義になり、同時に `inline const ComponentRegistrar<Spin> registrarOf_Spin(…)` が1つ作られる。この変数が作られるのは `main` より前なので、その場では登録できない（EntityManagerがまだ無い）。なので**登録待ちの表**に並び、`Engine::Initialize` の中で `GameComponentRegistry::ApplyAll()` がまとめて登録する |
| `ui.Field(名前, 値)` | 型ごとに `ImGui::DragFloat3` などを呼ぶだけの薄い包み。**ImGuiを知っているのは `ComponentUI.cpp` だけ**。Editorの無いビルドでは何もしない実装に差し替わる（だから `#ifdef` が要らない） |
| `SYSTEM(SpinUpdate)` | 関数の引数の型（`Spin&`・`TransformComponent&`・`float`）をテンプレートで取り出し、「全部持っていて・親までたどって有効なEntity」を回す処理を作って、毎フレームの表に並べる。最後の引数が `float` なら `deltaTime` を渡す |

**C++の制限（大事）：** この自動登録が効くのは**exeに直接入るファイル（.obj）だけ**。同じコードを静的ライブラリ（.lib）に入れると、誰も参照していないのでリンカがファイルごと捨て、登録も消える。実際に確かめた結果：

| リンクのしかた | 結果 |
|---|---|
| `parts.obj` を直接リンク | `Component=1 System=1`（登録される） |
| 同じコードを `parts.lib` に入れてリンク | `Component=0 System=0`（**消える**） |

だから**ゲームのComponentは `CG3_Project` のプロジェクトに直接入れる**（`Source/Components/*.cpp`）。エンジン（.lib）側のComponentは今までどおり `InspectorWindow::Initialize` で明示的に登録する。

### F-1. 新規：`MyEngine/Component/ComponentUI.h`

`MyEngine/Component/` フォルダを作って、この4ファイルを置く（F-1〜F-4）。

```cpp
#pragma once
#include <cfloat>

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


/// <summary>
/// Componentの中身の見せ方を書くための道具。ゲーム側にImGuiが出てこないようにするための薄い包み
/// <para>実体は差し替えられるようにしてある（今はInspectorに描く係だけ。後で「保存する係」「読み込む係」を足せば、
/// COMPONENT(...) に書いた並びがそのままセーブデータの並びになる）</para>
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

	// ===== 色（0〜1。色見本を押すと色を選べる）=====
	virtual void ColorField(const char* label, Vector3& rgb) = 0;
	virtual void ColorField(const char* label, Vector4& rgba) = 0;

	// ===== 飾り =====
	virtual void Label(const char* text) = 0;               // 灰色の説明文
	virtual void Separator(const char* text = nullptr) = 0; // 区切り線（文字を入れると見出しになる）
	virtual void Space() = 0;                               // 1行あける
};

// Inspectorに描く係（エンジンが1つだけ持っている。ゲームからは触らない）
ComponentUI& GetInspectorUI();
```

### F-2. 新規：`MyEngine/Component/ComponentUI.cpp`

ImGuiを知っているのはこのファイルだけ。Editorの無いビルド（Release）では、何もしない方の実装が使われる。

```cpp
#include "MyEngine/Component/ComponentUI.h"

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>

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
	void ColorField(const char*, Vector3&) override {}
	void ColorField(const char*, Vector4&) override {}
	void Label(const char*) override {}
	void Separator(const char*) override {}
	void Space() override {}
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

### F-3. 新規：`MyEngine/Component/GameComponent.h`

`COMPONENT` と `SYSTEM` の中身。ゲーム側はこのヘッダ1つをインクルードするだけでよくなる。

```cpp
#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <numbers>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Entity/TransformComponent.h"

// ゲーム固有Componentを書くための道具。ゲーム側はこのヘッダ1つだけをインクルードする
//
//   1ファイル＝1Component。書くのは「① データ ② 見せ方 ③ 処理」の3つだけ
//
//   struct Spin { Vector3 speed = {0.0f, 90.0f, 0.0f}; };            // ① データ
//   COMPONENT(Spin, "Movement") { ui.Field("Speed", value.speed); }  // ② 見せ方
//   void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) { ... }  // ③ 処理
//   SYSTEM(SpinUpdate);

// 度 ↔ ラジアン（TransformComponent::rotation はラジアン）
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;


//=============================================================================
// COMPONENT(...) の中身を Inspector の区画につなぐ
//=============================================================================

/// <summary>
/// COMPONENT(...) に書いた「見せ方」を使うComponentEditor。ゲームは直接触らない
/// </summary>
template<class T> class DescribedComponentEditor final : public TypedComponentEditor<T> {
public:
	using DescribeFunction = void (*)(ComponentUI& ui, T& value);

	DescribedComponentEditor(const char* name, std::string category, DescribeFunction describe) : name_(name), category_(std::move(category)), describe_(describe) {}

	const char* GetName() const override { return name_; }
	const char* GetCategory() const override { return category_.c_str(); }

protected:
	void DrawComponent(T& value) const override { describe_(GetInspectorUI(), value); }

private:
	const char* name_;          // COMPONENT(型名) の型名がそのまま表示名になる
	std::string category_;      // "Gameplay" か "Gameplay/サブカテゴリ"
	DescribeFunction describe_; // ②に書いた見せ方
};

// "Movement" から "Gameplay/Movement" を作る（空なら "Gameplay"）
inline std::string MakeGameplayCategory(const char* subCategory) {
	std::string path = "Gameplay";
	if (subCategory != nullptr && subCategory[0] != 0) {
		path += "/";
		path += subCategory;
	}
	return path;
}


//=============================================================================
// 登録待ちの表（static変数が作られるのはmainより前なので、その場では登録できない）
//=============================================================================
namespace ComponentRegistryDetail {
// Componentの登録を1つ予約する
void AddPending(std::function<void()> apply);
// Systemを1つ登録する
void AddSystem(const char* name, std::function<void(float)> run);
} // namespace ComponentRegistryDetail


/// <summary>
/// COMPONENT(...) が1つ作る物。作られた時点では「登録待ちの表」に並ぶだけ
/// </summary>
template<class T> class ComponentRegistrar {
public:
	ComponentRegistrar(const char* name, const char* subCategory, void (*describe)(ComponentUI&, T&)) {
		ComponentRegistryDetail::AddPending([name, subCategory, describe]() {
			EntityManager::RegisterComponent<T>(); // 置き場所
			ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<T>>(name, MakeGameplayCategory(subCategory), describe)); // Inspector
		});
	}
};


//=============================================================================
// SYSTEM(...) の中身（関数の引数の型から、回すComponentを決める）
//=============================================================================
namespace ComponentSystemDetail {

// 型の並びを持ち運ぶための箱
template<class... Ts> struct TypeList {};

// 最後の引数の型（引数の無い関数は登録できない）
template<class... Args> struct LastArg;
template<class T> struct LastArg<T> {
	using type = T;
};
template<class T, class... Rest> struct LastArg<T, Rest...> : LastArg<Rest...> {};

// tupleの Offset 番目から Index... 個を取り出して TypeList にする
template<class Tuple, size_t Offset, class Sequence> struct MakeTypeList;
template<class Tuple, size_t Offset, size_t... Index> struct MakeTypeList<Tuple, Offset, std::index_sequence<Index...>> {
	using type = TypeList<std::tuple_element_t<Offset + Index, Tuple>...>;
};

/// <summary>
/// 関数の引数を「Entity」「必要なComponent」「deltaTime」に分ける
/// <para>先頭が Handle&lt;Entity&gt; ならEntityを渡す。最後が float なら deltaTime を渡す。間は全部Component</para>
/// </summary>
template<class... Args> struct SystemArgs {
	static constexpr size_t count = sizeof...(Args);
	static_assert(count >= 1, "SYSTEMの関数には、Componentの参照を1つ以上書いてください");

	// 番兵の int を足して、引数が少なくても添字がはみ出さないようにする
	using AllWithGuard = std::tuple<Args..., int>;
	static constexpr bool hasEntity = std::is_same_v<std::tuple_element_t<0, AllWithGuard>, Handle<Entity>>;
	static constexpr bool hasDeltaTime = std::is_same_v<typename LastArg<Args...>::type, float>;
	static constexpr size_t componentCount = count - (hasEntity ? 1 : 0) - (hasDeltaTime ? 1 : 0);
	static_assert(componentCount >= 1, "SYSTEMの関数には、Componentの参照を1つ以上書いてください（Handleとfloatだけでは回せません）");

	using Components = typename MakeTypeList<std::tuple<Args...>, hasEntity ? 1 : 0, std::make_index_sequence<componentCount>>::type;
};

/// <summary>
/// First と Rest... を全部持っていて、親までたどって有効なEntityだけを回す
/// </summary>
template<bool HasEntity, bool HasDeltaTime, class Function, class First, class... Rest> void RunSystem(Function function, float deltaTime) {
	using FirstComponent = std::remove_cvref_t<First>;
	EntityManager::ForEach<FirstComponent>([&](Handle<Entity> entity, FirstComponent& first) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		// 残りのComponentを集める。1つでも持っていなければ呼ばない
		const std::tuple<std::remove_cvref_t<Rest>*...> rest(EntityManager::Get<std::remove_cvref_t<Rest>>(entity)...);
		const bool hasAll = std::apply([](auto*... component) { return ((component != nullptr) && ...); }, rest);
		if (!hasAll) {
			return;
		}
		std::apply(
		    [&](auto*... component) {
			    if constexpr (HasEntity && HasDeltaTime) {
				    function(entity, first, *component..., deltaTime);
			    } else if constexpr (HasEntity) {
				    function(entity, first, *component...);
			    } else if constexpr (HasDeltaTime) {
				    function(first, *component..., deltaTime);
			    } else {
				    function(first, *component...);
			    }
		    },
		    rest);
	});
}

// TypeList を開いて RunSystem に渡す
template<bool HasEntity, bool HasDeltaTime, class Function, class... Components> void RunSystemFromList(Function function, float deltaTime, TypeList<Components...>) {
	RunSystem<HasEntity, HasDeltaTime, Function, Components...>(function, deltaTime);
}

} // namespace ComponentSystemDetail


/// <summary>
/// SYSTEM(...) が1つ作る物。作られた時点では毎フレームの呼び出し表に並ぶだけ
/// </summary>
class SystemRegistrar {
public:
	template<class... Args> SystemRegistrar(const char* name, void (*function)(Args...)) {
		using Info = ComponentSystemDetail::SystemArgs<Args...>;
		ComponentRegistryDetail::AddSystem(
		    name, [function](float deltaTime) { ComponentSystemDetail::RunSystemFromList<Info::hasEntity, Info::hasDeltaTime>(function, deltaTime, typename Info::Components{}); });
	}
};


/// <summary>
/// COMPONENT / SYSTEM で書いた物をまとめて扱う。呼ぶのはエンジンだけ
/// </summary>
class GameComponentRegistry {
public:
	// 登録待ちを全部登録する。EntityManager::Initialize の後に1回呼ぶ
	static void ApplyAll();
	// SYSTEM(...) で登録した処理を全部呼ぶ。シーンのUpdateの後に呼ぶ
	static void UpdateAll(float deltaTime);
	// 登録された数（確認用）
	static size_t GetComponentCount();
	static size_t GetSystemCount();
};


//=============================================================================
// ゲーム側が書くのはこの2つだけ
//=============================================================================

/// <summary>
/// Componentの見せ方を書く。COMPONENT(型名) または COMPONENT(型名, "サブカテゴリ")
/// <para>中では ui（見せ方の道具）と value（Componentの中身）が使える。カテゴリは Gameplay で固定</para>
/// </summary>
// [[maybe_unused]] は「使わなくても警告しないで」の印。中身の無いComponent（印だけのタグ）でも警告が出ないようにしている
#define COMPONENT(Type, ...)                                                                             \
	inline void Describe_##Type([[maybe_unused]] ComponentUI& ui, [[maybe_unused]] Type& value);         \
	inline const ComponentRegistrar<Type> registrarOf_##Type(#Type, "" __VA_ARGS__, &Describe_##Type);   \
	inline void Describe_##Type([[maybe_unused]] ComponentUI& ui, [[maybe_unused]] Type& value)

/// <summary>
/// 毎フレームの処理を登録する。関数の引数に書いたComponentを全部持つEntityだけが呼ばれる
/// <para>先頭の引数を Handle&lt;Entity&gt; にするとそのEntityが入る（消したいとき用）。最後の引数を float にすると deltaTime が入る</para>
/// </summary>
#define SYSTEM(function) inline const SystemRegistrar registrarOfSystem_##function(#function, &function)
```

### F-4. 新規：`MyEngine/Component/GameComponent.cpp`

```cpp
#include "MyEngine/Component/GameComponent.h"

#include <vector>

namespace {
// 登録待ちの表。関数の中のstaticで持つので、他のstatic変数との初期化の順番に左右されない
std::vector<std::function<void()>>& PendingComponents() {
	static std::vector<std::function<void()>> pending;
	return pending;
}

// 毎フレーム呼ぶ処理の表
struct RegisteredSystem {
	const char* name;               // SYSTEM(関数名) の関数名（確認用）
	std::function<void(float)> run; // Entityを回して関数を呼ぶ処理
};
std::vector<RegisteredSystem>& Systems() {
	static std::vector<RegisteredSystem> systems;
	return systems;
}

size_t appliedComponentCount = 0; // 登録済みのComponentの数
} // namespace


//=============================================================================
// COMPONENT / SYSTEM から呼ばれる（mainより前）
//=============================================================================
void ComponentRegistryDetail::AddPending(std::function<void()> apply) { PendingComponents().push_back(std::move(apply)); }

void ComponentRegistryDetail::AddSystem(const char* name, std::function<void(float)> run) { Systems().push_back({name, std::move(run)}); }


//=============================================================================
// 登録待ちをまとめて登録する（EntityManager::Initialize の後）
//=============================================================================
void GameComponentRegistry::ApplyAll() {
	// 表を取り出してから回す（2回呼ばれても二重に登録しない）
	std::vector<std::function<void()>> pending;
	pending.swap(PendingComponents());
	for (const std::function<void()>& apply : pending) {
		apply();
	}
	appliedComponentCount += pending.size();
}


//=============================================================================
// SYSTEM(...) で登録した処理を全部呼ぶ
//=============================================================================
void GameComponentRegistry::UpdateAll(float deltaTime) {
	for (const RegisteredSystem& system : Systems()) {
		system.run(deltaTime);
	}
}


//=============================================================================
// 数（確認用）
//=============================================================================
size_t GameComponentRegistry::GetComponentCount() { return appliedComponentCount; }

size_t GameComponentRegistry::GetSystemCount() { return Systems().size(); }
```

### F-5. `MyEngine/Editor/Inspector/ComponentEditor.h`（2か所）

**(1) `enum class ComponentCategory` を丸ごと消して、コメントに置き換える**（カテゴリは文字列の道になるので、列挙型は要らなくなる）。

消すのはここ。

```cpp
/// <summary>
/// Add Componentのメニューの分け方。Inspectorの区画もこの順に並ぶ
/// </summary>
enum class ComponentCategory {
	Core,        // Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
	Rendering3D, // 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
	Rendering2D, // 2D（将来のSpriteRenderer）
	Lighting,    // ライト
	Effects,     // パーティクルなど
	Physics,     // 当たり判定
	Audio,       // 音
	Gameplay,    // ゲーム固有のデータ
};
```

代わりにこう書く。

```cpp
// Add Componentのメニューの分け方。"/" で区切ると、その下にさらにカテゴリを作れる（"Gameplay/Movement" など）
//
//   Core        Transformなど、全Entityが必ず持つもの（Add Componentには出ない）
//   Rendering3D 3Dモデル（ModelRenderer、将来はSkinnedModelRenderer）
//   Rendering2D 2D（将来のSpriteRenderer）
//   Lighting    ライト
//   Effects     パーティクルなど
//   Physics     当たり判定
//   Audio       音
//   Gameplay    ゲーム固有のデータ（COMPONENT(...) で書いた物は全部ここに入る）
//
// 上の段の並び順は ComponentEditor.cpp の kTopLevelOrder が決める。そこに無い名前は後ろに回る
```

**(2) `GetCategory` の戻り値を文字列にする**（`ComponentEditor` クラスの中）。

```cpp
	// ===== 種類の情報 =====
	virtual const char* GetName() const = 0;         // Inspectorの見出し・Add Componentの表示名
	virtual const char* GetCategory() const = 0;     // Add Componentでどのカテゴリに出すか（"Gameplay/Movement" のように "/" で入れ子）
	virtual bool IsOptional() const { return true; } // Add・Removeの対象か（Transformのように必ず持つものはfalse）
```

ついでに、下の方にある2つの説明コメントも実態に合わせる（動きには関係ない）。

- `/// ComponentEditorの登録表。カテゴリ順（同じカテゴリの中は登録した順）に並べて持つ` → `…（同じカテゴリの中は名前順）に並べて持つ`
- `/// ComponentEditorの登録表。Inspectorは登録した順に区画を並べる` → `/// ComponentEditorの登録表。Inspectorの区画も Add Component のメニューも、この並びをそのまま使う`

### F-6. `MyEngine/Editor/Inspector/ComponentEditor.cpp`（ファイル全体を差し替え）

並べ方が「列挙型の順」から「上の段の順 → パスの文字順 → 名前順」になる。
`"Gameplay"` が `"Gameplay/Movement"` より前に来るので、**各段で「そこで終わる物」が必ず先に並ぶ**。これがメニューを入れ子にできる理由。

```cpp
#include "ComponentEditor.h"

#include <algorithm>
#include <iterator>
#include <string_view>

namespace {
std::vector<std::unique_ptr<ComponentEditor>> editors; // カテゴリ順（同じカテゴリの中は名前順）

// Add Componentの、上の段の並び順。ここに無い名前は後ろに回る
constexpr std::string_view kTopLevelOrder[] = {"Core", "Rendering3D", "Rendering2D", "Lighting", "Effects", "Physics", "Audio", "Gameplay"};

// "Gameplay/Movement" の上の段は "Gameplay"
std::string_view TopLevelOf(std::string_view path) {
	const size_t slash = path.find('/');
	return slash == std::string_view::npos ? path : path.substr(0, slash);
}

// 上の段が何番目か（知らない名前は一番後ろ）
size_t TopLevelOrderOf(std::string_view path) {
	const std::string_view topLevel = TopLevelOf(path);
	for (size_t i = 0; i < std::size(kTopLevelOrder); ++i) {
		if (kTopLevelOrder[i] == topLevel) {
			return i;
		}
	}
	return std::size(kTopLevelOrder);
}

// aがbより前に来るか。上の段の順 → カテゴリのパスの文字順 → 表示名の文字順
// （"Gameplay" は "Gameplay/Movement" より前。だから各段で「そこで終わる物」が先に並ぶ）
// 登録した順に頼らないので、ファイルが増えても並びが変わらない
bool IsBefore(const ComponentEditor& a, const ComponentEditor& b) {
	const size_t orderA = TopLevelOrderOf(a.GetCategory());
	const size_t orderB = TopLevelOrderOf(b.GetCategory());
	if (orderA != orderB) {
		return orderA < orderB;
	}
	const std::string_view categoryA(a.GetCategory());
	const std::string_view categoryB(b.GetCategory());
	if (categoryA != categoryB) {
		return categoryA < categoryB;
	}
	return std::string_view(a.GetName()) < std::string_view(b.GetName());
}
} // namespace

void ComponentEditorRegistry::Register(std::unique_ptr<ComponentEditor> editor) {
	for (const std::unique_ptr<ComponentEditor>& registered : editors) {
		if (std::string_view(registered->GetName()) == editor->GetName()) {
			return;
		}
	}
	// 並びの決まった場所に入れる（Coreが必ず先頭、Gameplayが最後になる）
	const auto position = std::find_if(editors.begin(), editors.end(), [&editor](const std::unique_ptr<ComponentEditor>& registered) { return IsBefore(*editor, *registered); });
	editors.insert(position, std::move(editor));
}

const std::vector<std::unique_ptr<ComponentEditor>>& ComponentEditorRegistry::GetAll() { return editors; }
```

### F-7. エンジンのEditor3ファイル（5か所。`ComponentCategory::` を文字列にするだけ）

| ファイル | 直す行 |
|---|---|
| `Editor/Inspector/TransformEditor.h` | `const char* GetCategory() const override { return "Core"; }` |
| `Editor/Inspector/ModelRendererEditor.h` | `const char* GetCategory() const override { return "Rendering3D"; }` |
| `Editor/Inspector/LightEditor.h` | 3クラス全部 `const char* GetCategory() const override { return "Lighting"; }` |

### F-8. `MyEngine/Editor/Windows/InspectorWindow.cpp`（3か所）

**(1) インクルード**：`magic_enum` を使わなくなるので消し、`<string_view>` を足す。

```cpp
#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>   // ← 足す
#include <vector>

#include <externals/imgui/imgui.h>
// #include <externals/magic_enum/magic_enum.hpp>  ← この行を消す
```

**(2) ファイルの先頭の `namespace { … }` を全部差し替える**（`IsListed` の引数が変わり、メニューを組み立てる関数が増える）。

```cpp

namespace {
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）

// Add Componentに出すか（外せる種類で、検索にも合う）
bool IsListed(const ComponentEditor& editor, const ImGuiTextFilter& filter) { return editor.IsOptional() && filter.PassFilter(editor.GetName()); }

// カテゴリのパスの depth 段目の名前（"Gameplay/Movement" の1段目は "Movement"）。そこで終わっていれば空
std::string_view CategorySegment(std::string_view path, size_t depth) {
	size_t start = 0;
	for (size_t i = 0; i < depth; ++i) {
		const size_t slash = path.find('/', start);
		if (slash == std::string_view::npos) {
			return {}; // これより下の段は無い
		}
		start = slash + 1;
	}
	const size_t slash = path.find('/', start);
	return path.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
}

// Componentを1つ、押せる項目として出す
void DrawAddComponentItem(Handle<Entity> handle, const ComponentEditor& editor) {
	// すでに持っている・追加を予約済みなら、灰色にして押せなくする
	ImGui::BeginDisabled(editor.Get(handle) || editor.IsAddPending(handle));
	if (ImGui::Selectable(editor.GetName())) { // Selectableを押すとポップアップは自動で閉じる
		EditorHistory::RequestAddComponent(handle, editor);
	}
	ImGui::EndDisabled();
}

// [begin, end) は同じ上位カテゴリの並び。depth段目の名前で切って、入れ子のメニューにする
void DrawAddComponentLevel(Handle<Entity> handle, const std::vector<const ComponentEditor*>& items, size_t begin, size_t end, size_t depth, const ImGuiTextFilter& filter) {
	size_t index = begin;

	// パスがこの段で終わっている物（"Gameplay" 直下など）を先に並べる。並びの決まりで、これらは必ず先頭に集まっている
	while (index < end && CategorySegment(items[index]->GetCategory(), depth).empty()) {
		DrawAddComponentItem(handle, *items[index]);
		++index;
	}

	// 残りを、同じ名前のかたまりごとに入れ子にする
	while (index < end) {
		const std::string name(CategorySegment(items[index]->GetCategory(), depth));
		size_t groupEnd = index;
		while (groupEnd < end && CategorySegment(items[groupEnd]->GetCategory(), depth) == name) {
			++groupEnd;
		}
		// 検索中は、見つかったカテゴリを全部開いて見せる
		if (filter.IsActive()) {
			ImGui::SetNextItemOpen(true);
		}
		if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			DrawAddComponentLevel(handle, items, index, groupEnd, depth + 1, filter);
			ImGui::TreePop();
		}
		index = groupEnd;
	}
}
} // namespace
```

**(3) `DrawAddComponent` の後半（`// --- カテゴリ（押すと下に開く）…` から関数の最後まで）を差し替える。** 30行あった二重ループが、並べ替え済みの表を渡すだけになる。

```cpp
	// --- カテゴリ（押すと下に開く）→ その中のComponent ---
	// 登録表はカテゴリ順に並んでいるので、同じカテゴリの物は必ず続いて並んでいる（だから範囲で切って入れ子にできる）
	std::vector<const ComponentEditor*> items;
	for (const std::unique_ptr<ComponentEditor>& editor : ComponentEditorRegistry::GetAll()) {
		if (IsListed(*editor, filter)) {
			items.push_back(editor.get());
		}
	}
	DrawAddComponentLevel(handle, items, 0, items.size(), 0, filter);

	ImGui::EndPopup();
}
```

### F-9. エンジンに2行つなぐ

**(1) `MyEngine/Engine.cpp`** … インクルードを1つ足して（`// Particle` の後、`// Entity` の前あたり）、

```cpp
// Component
#include "MyEngine/Component/GameComponent.h"
```

`Initialize` の中、`LightSystem::Initialize();` のすぐ下に1行足す。

```cpp
	EntityManager::Initialize();
	LightSystem::Initialize();         // ライトのComponentをEntityManagerに登録するので、EntityManagerの後
	GameComponentRegistry::ApplyAll(); // COMPONENT(...) で書いたゲーム固有Componentを登録（登録待ちの表を空にする）
```

**(2) `MyEngine/Scene/SceneManager.cpp`** … インクルードを1つ足して、

```cpp
#include "MyEngine/Component/GameComponent.h"
```

`Update()` の `currentScene_->Update();` のすぐ下に1行足す。

```cpp
	currentScene_->Update();
	GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）
```

ここに置く理由：

- 停止中（`PlayState::Editing`）は `Update()` の手前で `return` しているので、**Stop中はSystemも動かない**（今と同じ）
- 一時停止中は `Time::SetTimeScale(0)` なので `deltaTime` が0になり、**Pauseで止まる**（Systemの中に特別な判定を書かなくてよい）
- `EntityManager::UpdateTransforms()` はこの後（`WindowManager::UpdateAll`）なので、Systemが動かした `rotation` はそのフレームの描画に間に合う

### F-10. エンジンのプロジェクトに4ファイルを追加する

1. Visual Studio の `MyEngine_v1` プロジェクトに、F-1〜F-4 の4ファイルを追加（フィルタは `Component` を新しく作るときれい）
2. **ゲーム側に渡すヘッダのコピーは何もしなくてよい**（vcxprojを見て確認した）。ビルド後の `xcopy /Y /S /I "$(ProjectDir)MyEngine\*.h" "$(SolutionDir)MyEngine\include\MyEngine\"` がフォルダごと拾うので、`ComponentUI.h` と `GameComponent.h` は自動で `MyEngine/include/MyEngine/Component/` に入る
3. エンジンをビルドして `MyEngine_v1.lib` を作り直す

### F-11. ゲーム側：`Spin` を新しい形に置き換える

**(1) 新規：`CG3_Project/Source/Components/Spin.cpp`**（フォルダも新しく作る）

```cpp
#include <MyEngine/Component/GameComponent.h>

// ① データ
struct Spin {
	Vector3 speed = {0.0f, 90.0f, 0.0f}; // 1秒に回る角度（度）。軸ごと
};

// ② Inspectorでの見せ方（カテゴリは Gameplay で固定。"Movement" はその下のサブカテゴリ）
COMPONENT(Spin, "Movement") {
	ui.Field("Speed (deg/s)", value.speed, Tip("1秒に回る角度。軸ごと"));
}

// ③ 毎フレームの処理。Spin と Transform の両方を持つEntityだけ呼ばれる
void SpinUpdate(Spin& spin, TransformComponent& transform, float deltaTime) {
	transform.rotation += spin.speed * (kDegToRad * deltaTime);
}
SYSTEM(SpinUpdate);
```

**(2) 消す：`CG3_Project/GameComponent.h` と `CG3_Project/GameComponent.cpp`**（プロジェクトからも外す）

**(3) `CG3_Project/GameScene.cpp` から3か所消す**

```cpp
#include "GameComponent.h"                              // ← 消す

	// ゲーム固有のComponentを登録する（Entityに付ける前に。Stopで作り直すたびに呼ばれるが、2回目からは何もしない）
	GameComponents::Register();                         // ← 消す（Initializeの先頭）

	EntityManager::RequestAdd<SpinComponent>(            // ← この4行を消す
	    ball, SpinComponent{
	              {0.0f, 45.0f, 0.0f}
    }); // Play中だけ回る（ゲーム固有Componentの確認用）

	GameComponents::Update(Time::GetDeltaTime());        // ← 消す（Updateの中）
```

`RequestAdd<SpinComponent>` を消すので、**MonsterBallは最初はSpinを持っていない**。Inspectorの Add Component → Gameplay → Movement → Spin で付けて確かめる。
（コードから付けたいときは、`struct Spin` だけを `Source/Components/Spin.h` に切り出して、`GameScene.cpp` でそれをインクルードすればよい。`COMPONENT` と `SYSTEM` は `.cpp` に残す。）

**(4) `Spin.cpp` をプロジェクトに入れる**（入れないとコンパイルされず、Add Componentに出てこない）

**一番簡単なのは、VSで `CG3_Project` を右クリック → 追加 → 既存の項目 → `Source\Components\Spin.cpp`。これだけでよい。**
毎回VSを触るのが面倒なら、代わりに `CG3_Project.vcxproj` をメモ帳で開いて `<ItemGroup>` の中にこう書くと、そのフォルダのcppが全部自動で入る（利点と癖は **F-17** に書いた）。

```xml
  <ItemGroup>
    <ClCompile Include="Source\Components\*.cpp" />
    <ClInclude Include="Source\Components\*.h" />
  </ItemGroup>
```

> **注意：** Componentのファイルは必ず `CG3_Project` プロジェクト（exe）に入れる。静的ライブラリに入れると登録が消える（上の「仕組み」の表）。

### F-12. 確認すること

1. **Add Componentのメニューが入れ子になっている**：`+ Add Component` → `Gameplay` を開くと、中に `Movement` があり、その中に `Spin` がある
2. MonsterBallに `Spin` を付けると、Inspectorに `Spin` の区画が出て、`Speed (deg/s)` が3つの数字で出る。マウスを乗せると説明が出る
3. **Play で回る**。Stop で元に戻る（Playを押すとシーンが作り直されるので、付けたSpinも消える。これは今の仕様。シーンの保存を作れば直る）
4. Undo（Ctrl+Z）で `Spin` の追加と、`Speed` の変更が戻る
5. カテゴリの並びが `Core → Rendering3D → Rendering2D → Lighting → … → Gameplay` のままで、Inspectorの区画の順も変わっていない
6. 検索欄に `spi` と打つと、`Gameplay` と `Movement` が自動で開いて `Spin` が出る
7. **Releaseビルドが通り、動く**（Componentの見せ方を書いていても、ImGuiが無いビルドで問題が無いこと）

### F-13. これから新しいComponentを作るときの手順（覚えるのはこれだけ）

1. `CG3_Project/Source/Components/` に `名前.cpp` を作る
2. 中に `#include <MyEngine/Component/GameComponent.h>` と「①データ ②`COMPONENT` ③処理＋`SYSTEM`」を書く
3. ビルドして起動 → Add Component に出てくる

### F-14. 書き方リファレンス

**`ui.Field` が受け取れる型**

| 型 | 見た目 |
|---|---|
| `float` | つまんで動かす数字（`DragFloat`） |
| `int` | つまんで動かす整数 |
| `bool` | チェックボックス |
| `Vector2` / `Vector3` / `Vector4` | 2〜4個の数字 |

**見せ方の指定（省略できる。つなげて書ける）**

| 書き方 | 意味 |
|---|---|
| `ui.Field("HP", value.hp)` | そのまま |
| `ui.Field("HP", value.hp, Range(0.0f, 100.0f))` | 0〜100のスライダー |
| `ui.Field("Radius", value.radius, AtLeast(0.0f))` | 0より小さくできない |
| `ui.Field("Speed", value.speed, Drag(0.1f))` | つまんだときの動く速さ |
| `ui.Field("HP", value.hp, Tip("体力"))` | マウスを乗せると説明が出る |
| `ui.Field("Bar", value.bar, Range(1.0f, 200.0f).Tip("大きさ"))` | つなげる |

**色と飾り**

| 書き方 | 意味 |
|---|---|
| `ui.ColorField("Color", value.color)` | 色見本を押すと色を選べる（`Vector3`＝RGB、`Vector4`＝RGBA） |
| `ui.Separator("見出し")` | 区切り線（文字を入れると見出しになる） |
| `ui.Label("説明")` | 灰色の説明文 |
| `ui.Space()` | 1行あける |

**カテゴリ**

| 書き方 | Add Componentでの場所 |
|---|---|
| `COMPONENT(Health)` | Gameplay → Health |
| `COMPONENT(Spin, "Movement")` | Gameplay → Movement → Spin |
| `COMPONENT(BossPhase, "Enemy/Boss")` | Gameplay → Enemy → Boss → BossPhase |

**`SYSTEM` の引数の決まり**

| 書き方 | 意味 |
|---|---|
| `void F(Spin& spin)` | `Spin` を持つEntity全部 |
| `void F(Spin& spin, TransformComponent& transform)` | **両方**持つEntityだけ（片方しか無いEntityは呼ばれない） |
| `void F(Spin& spin, float deltaTime)` | 最後の `float` は経過秒（`deltaTime`） |
| `void F(const Spin& spin, Health& health)` | 読むだけなら `const&`。書き換えるなら `&` |
| `void F(Handle<Entity> entity, Enemy& enemy, float deltaTime)` | 先頭の `Handle<Entity>` は**そのEntity自身**。`EntityManager::Destroy(entity)` で自分を消せる（2026-09-21に追加） |

- 親までたどって有効なEntityだけが呼ばれる（`isActive` が切れていたらとばす）
- 回している最中に `EntityManager::RequestAdd` / `RequestRemove` / `Destroy` を呼んでも安全（予約なので、実際に効くのは次のフレーム）
- `SYSTEM` は1ファイルに何個書いてもよい

**やってはいけないこと**

| だめな書き方 | なぜ |
|---|---|
| Componentの中に `std::string` や `std::vector`、ポインタを持つ | Undoとコピーが中身を丸ごとバイトで写すので壊れる（`static_assert` で止まる）。文字やリストが必要になったら相談して |
| `COMPONENT` を書かずに `EntityManager::RegisterComponent` だけ呼ぶ | Inspectorに出ない。逆に `COMPONENT` を書けば両方やってくれる |
| Componentのファイルを静的ライブラリに入れる | リンカに捨てられて登録が消える |
| `SYSTEM` の関数の引数を値渡し（`Spin spin`）にする | コンパイルエラーになる（参照で受け取る） |

### F-15. 検証したこと（2026-09-20 ／ 2026-09-21 追記、Claude）

- **コンパイル**：エンジン80ファイル（新規4・変更6）＋ゲーム（`GameScene.cpp`・`Spin.cpp`・変わり種の確認用ファイル・`Stage.cpp`・`Particles.cpp`・`main.cpp`）を Debug / Release の両方で。**エラー0、新しい警告0**（`/W4`）
- **（2026-09-21）あなたが写経した実ファイルをコピーして同じエラーを再現**し、F-16の直しを当ててから Debug / Release ともエラー0になることを確認した
- **（2026-09-21）`windows.h` を先に読むファイルで `ComponentUI.h` が壊れること**を再現し、`FLT_MAX` にして直ることを確認した
- **動作テスト44項目、失敗0**（本物の `EntityManager` / `ComponentStorage` / `ComponentEditorRegistry` / `GameComponentRegistry` / `ComponentUI` を使い、ログと通知だけスタブ）
  - `COMPONENT` で書いた6つが登録され、`ApplyAll` を2回呼んでも増えない
  - **（2026-09-21）先頭で `Handle<Entity>` を受け取るSystemが、回っている最中に自分を消せる**（消えるのは `FlushDestroy` の後。消えた分は次のフレームから呼ばれない）
  - **誰もインクルードしていない別ファイル（`parts.cpp`）のComponentも、置くだけで登録される**
  - 並び順：`Core → Rendering3D → Rendering2D → Lighting → Gameplay → Gameplay/Enemy/Boss → Gameplay/Movement`。同じカテゴリの中は名前順。登録した順に左右されない
  - 同じカテゴリの物が必ず続いて並んでいる（メニューを入れ子にできる条件）
  - `Spin` が1秒で90度・0.5秒で45度回る。無効なEntity、親が無効な子は回らない。有効に戻すと回り出す
  - `deltaTime` を取らないSystem、`const&` で受けるSystem、2つのComponentを要求するSystemが正しく呼ばれる（片方だけ持つEntityは呼ばれない）
  - 予約中（Flush前）はSystemから見えない。外した後は呼ばれない。消したEntityがあっても落ちない
  - Editorの `Draw` を通しても値が変わらない（ImGuiが無いビルドの通り道）
- **Add Componentのメニューの組み立て**：12個のカテゴリ（`Gameplay` 直下・`Gameplay/Enemy`・`Gameplay/Enemy/Boss`・`Gameplay/Movement` 3つ・知らないカテゴリ）を入れて、出来上がる入れ子が期待どおりか文字で照合。**一致**
- **.libに入れると消えること**を実際に確認（`parts.obj` 直リンク＝登録される／`parts.lib` 経由＝消える）
- 画面の見た目・実際のマウス操作は確かめていない（F-12を見てほしい）

### F-16. 出ているエラーの直し方（2026-09-21）

**あなたの実ファイルを私の環境にコピーして、同じエラーを再現してから直した。** 原因は2つ。

#### (1) 45件のエラーの正体は `max` ではなく、**F-7 をやっていないこと**

F-5 で `ComponentEditor.h` から `enum class ComponentCategory` を消したのに、3つのEditorがまだ `ComponentCategory::Core` などと書いている。無くなった型を使っているので、こう出る。

```
TransformEditor.h(12): error C3646: 'GetCategory': 不明なオーバーライド指定子です
TransformEditor.h(12): error C2059: 構文エラー: '('
TransformEditor.h(12): error C2334: '{' の前に予期しないトークンがありました。関数の本体は無視されます
```

**エラーが出ている場所は5行だけ**（下の表）。それを読み込むcppの数だけ同じエラーが繰り返されるので数十件に見える。

| ファイル | 今こう書いてある | こう直す |
|---|---|---|
| `Editor/Inspector/TransformEditor.h` 12行目 | `ComponentCategory GetCategory() const override { return ComponentCategory::Core; }` | `const char* GetCategory() const override { return "Core"; }` |
| `Editor/Inspector/ModelRendererEditor.h` 12行目 | `… return ComponentCategory::Rendering3D; }` | `const char* GetCategory() const override { return "Rendering3D"; }` |
| `Editor/Inspector/LightEditor.h` 14・26・38行目（3クラス分） | `… return ComponentCategory::Lighting; }` | `const char* GetCategory() const override { return "Lighting"; }` |

**この5行を直すとエラーは全部消える**（あなたのファイルで確認済み。Debug / Release ともエラー0）。

#### (2) `ComponentUI.h` の `max` は本当に問題だった（私のミス）

`AtLeast` の中に `std::numeric_limits<float>::max()` と書いたのが悪かった。`windows.h` が **`max` という名前のマクロ**を作るので、`windows.h` を先に読んだファイルでは `max()` がマクロとして展開されて壊れる。

```
ComponentUI.h(45): warning C4003: 関数に似たマクロ呼び出し 'max' の引数が不足しています
ComponentUI.h(45): error C2589: '(': スコープ解決演算子 (::) の右側にあるトークンは使えません
```

**`#define NOMINMAX` を `ComponentUI.h` に書いても効かない。** NOMINMAXは「`windows.h` を読むより前」に決まっていないと意味がなく、`ComponentUI.h` に着いた時点では `windows.h` はもう読み終わっているから（エンジンは `Engine.h` や各cppの先頭で `#define NOMINMAX` しているが、それが効かない読み込み順のファイルがある）。

直し方は**マクロに頼らないこと**。`ComponentUI.h` の2か所を直して、足した `#define NOMINMAX` の行は消す。

```cpp
#pragma once
#include <cfloat>   // ← <limits> をこれに変える
```

```cpp
// 下限だけ決める（つまんで動かす。負の値にしたくない項目に）
inline FieldStyle AtLeast(float min) {
	FieldStyle style;
	style.min = min;
	style.max = FLT_MAX; // windows.h の max マクロに邪魔されないよう、numeric_limits ではなく cfloat のFLT_MAXを使う
	return style;
}
```

`FLT_MAX` は「floatで表せる一番大きい数」。これもマクロだが、`max` という名前ではないので衝突しない。
**確認**：`windows.h` を先に読んでから `ComponentUI.h` を読むファイルを作って、直す前はエラー、直した後は通ることを実際に見た。

#### (3) ついでに `GameComponent.h` を2か所直した

- **`SYSTEM` の関数が、先頭で `Handle<Entity>` を受け取れるようにした。** 「敵を倒したら自分を消す」が書けなかったため（下のQ&Aを見て）
- **`COMPONENT` の中身が `value` を使わなくても警告が出ないようにした**（`[[maybe_unused]]`）。`struct Dead {};` のような**印だけのComponent**を書くと `/W4` で `warning C4100` が出ていた

**`GameComponent.h` は F-3 のコードブロックをまるごと貼り直すのが早い**（変わったのは後半だけだが、探すより速い）。

---

### F-17. F-11(4)（vcxproj の行）は何をしているのか

Visual Studio は「このプロジェクトでコンパイルするファイル」を `.vcxproj` の中に**1行ずつ**持っている。

```xml
<ItemGroup>
  <ClCompile Include="GameScene.cpp" />
  <ClCompile Include="main.cpp" />
  <ClCompile Include="Particles.cpp" />
  <ClCompile Include="Stage.cpp" />
</ItemGroup>
```

`Source/Components/Spin.cpp` を作っても、**この表に載っていなければコンパイルされない**。コンパイルされない → objにならない → `COMPONENT` の登録も走らない → **Add Componentに出てこない**。エラーも警告も出ないので、一番気づきにくい失敗の仕方をする。

やり方は2つあって、**どちらでも動く**。

**(a) 1つずつ足す（今回はこっちでいい）**

ソリューションエクスプローラーで `CG3_Project` を右クリック → **追加 → 既存の項目** → `Source\Components\Spin.cpp` を選ぶ。VSが上の `<ClCompile Include="Source\Components\Spin.cpp" />` を自動で書き足す。**F-11(4) を飛ばして、これだけやれば動く。**

**(b) フォルダごと拾う（F-11(4)）**

```xml
  <ItemGroup>
    <ClCompile Include="Source\Components\*.cpp" />
    <ClInclude Include="Source\Components\*.h" />
  </ItemGroup>
```

`*` は「何でもいい」という意味で、**そのフォルダに置いたcppが全部自動で入る**。Componentを増やすたびにVSを触らなくてよくなるのが利点。ただし癖がある。

- VSの「新しい項目の追加」で作ると、VSが**明示的な行も足してしまい二重に入る**ことがある（そのときは vcxproj から明示行を手で消す）
- エクスプローラーで直接作ったファイルがソリューションエクスプローラーに出るのは、**プロジェクトを開き直した後**

なので (b) にするなら「**Componentのファイルはエクスプローラーで作る（VSからは作らない）**」と決めると気持ちよく回る。

**おすすめ：今は (a)。Componentが5個・10個と増えて面倒になったら (b) に変える。**

---

### F-18. よくある質問（2026-09-21）

#### Q. カテゴリ名（`"Movement"` の所）は日本語にできる？

**できる。** `COMPONENT(Spin, "移動")` と書けば、Add Component の中が「移動」になる。エンジンもゲームも `/utf-8` でビルドしていて、ImGuiのフォントもメイリオなので、そのまま出る。`"移動/空を飛ぶ"` のように入れ子にもできる。

注意が2つ。

- **並び順は文字コード順**。ひらがな・カタカナは五十音順に並ぶが、**漢字は五十音順にならない**（Unicodeの番号順になる）。順番を決めたいときは `"1 移動"` `"2 戦闘"` のように頭に数字を付けるのが手っ取り早い
- **Componentの名前（`Spin` の所）は型名がそのまま出る**ので日本語にはできない（C++の識別子だから）。Inspectorに「回転」と出したいなら、表示名を別に渡せるようにする必要がある。`COMPONENT(Spin, "移動")` に3つ目を足すだけなので、欲しければ言ってくれれば次のバッチで入れる

#### Q. `TransformComponent` は絶対に要る？

**要らない。** `SYSTEM` の関数の引数は「**この型を全部持っているEntityだけ呼んでくれ**」という注文書で、`TransformComponent` を書いたのは「回すために回転を触りたいから」でしかない。

```cpp
struct Timer { float remain = 3.0f; };
COMPONENT(Timer, "Gimmick") { ui.Field("Remain", value.remain, AtLeast(0.0f)); }

void TimerTick(Timer& timer, float deltaTime) { timer.remain -= deltaTime; } // Transformは要らない
SYSTEM(TimerTick);
```

ただし**全EntityがTransformを持っている**ので、書いても取りこぼしは起きない（`Transform` を注文しても、除外されるEntityは無い）。

#### Q. 「敵を倒したら移動速度が上がる」みたいな、ゲーム全体のフラグはどこに置くの？

**全部をComponentにする必要はない。** 置き場所は3つある。

| 置き場所 | いつ使うか | 例 |
|---|---|---|
| ① Componentの中 | **その物だけ**の状態 | `Enemy::hp`、`Player::speed` |
| ② ゲームに1つだけの入れ物 | スコア、倒した数、進行状況 | `gGameState.killCount` |
| ③ Componentが付いているかどうか（タグ） | 状態の切り替え | `Dead` が付いたら別のSystemが拾う |

②の一番簡単な形（Componentにしない。ふつうのC++）：

```cpp
// CG3_Project/Source/GameState.h
#pragma once

// ゲーム全体で1つだけの状態
struct GameState {
	int killCount = 0;        // 倒した数
	float speedBonus = 0.0f;  // 倒すほど速くなる分
};

inline GameState gGameState; // inline を付けると、どのcppから見ても「同じ1個」になる
```

敵の側：

```cpp
// CG3_Project/Source/Components/Enemy.cpp
#include <MyEngine/Component/GameComponent.h>
#include "GameState.h"

struct Enemy {
	int hp = 3;
};

COMPONENT(Enemy, "Enemy") {
	ui.Field("HP", value.hp, Range(0.0f, 20.0f));
}

// 先頭の Handle<Entity> で「自分」を受け取れるので、自分を消せる
void EnemyDeath(Handle<Entity> entity, Enemy& enemy) {
	if (enemy.hp > 0) {
		return;
	}
	++gGameState.killCount;        // ← ゲーム全体のフラグを更新
	gGameState.speedBonus += 0.5f;
	EntityManager::Destroy(entity); // 予約。実際に消えるのはフレームの最後
}
SYSTEM(EnemyDeath);
```

プレイヤーの側：

```cpp
// CG3_Project/Source/Components/Player.cpp
#include <MyEngine/Component/GameComponent.h>
#include "GameState.h"

struct Player {
	float baseSpeed = 5.0f;
};

COMPONENT(Player, "Player") {
	ui.Field("Base Speed", value.baseSpeed, AtLeast(0.0f));
}

void PlayerMove(Player& player, TransformComponent& transform, float deltaTime) {
	const float speed = player.baseSpeed + gGameState.speedBonus; // ← 倒すほど速くなる
	transform.translation.z += speed * deltaTime;
}
SYSTEM(PlayerMove);
```

ファイルが分かれていても `GameState.h` をインクルードするだけでつながる。
**大事なのは、System同士が直接呼び合わないこと。** 片方が書いて、片方が読むだけにしておくと、どちらが先に動いても壊れない（今はSystemの実行順は登録順だが、そこに頼らない書き方にしておくと後で困らない）。

Inspectorで見たい・触りたいなら、②もComponentにして、Entityを1つだけ作って付ける（**上の `inline GameState gGameState;` とどちらか一方にする**。同じ名前の型を2つ作らない）：

```cpp
// Source/Components/GameState.cpp
struct GameState {
	int killCount = 0;
	float speedBonus = 0.0f;
};

COMPONENT(GameState, "System") {
	ui.Field("Kill Count", value.killCount);
	ui.Field("Speed Bonus", value.speedBonus, AtLeast(0.0f));
}

// 1個しか無い物を取り出す道具（見つからなければ nullptr）
GameState* FindGameState() {
	GameState* found = nullptr;
	EntityManager::ForEach<GameState>([&](Handle<Entity>, GameState& state) {
		if (found == nullptr) {
			found = &state;
		}
	});
	return found;
}
```

この5行が何度も出てくるようなら、エンジンに `EntityManager::FindFirst<T>()` を足す（そのときは言って）。

③のタグの例（Componentを付けること自体が「印」）：

```cpp
struct Dead {}; // 中身は空でよい
COMPONENT(Dead, "Enemy") { ui.Label("倒された印"); }

// 倒した側は印を付けるだけ
void EnemyDeath(Handle<Entity> entity, Enemy& enemy) {
	if (enemy.hp <= 0) {
		EntityManager::RequestAdd<Dead>(entity);
	}
}
// 別のSystemが、次のフレームに拾って演出する
void DeadEffect(Handle<Entity> entity, Dead&, TransformComponent& transform) {
	transform.scale = transform.scale * 0.9f;
	if (transform.scale.x < 0.05f) {
		EntityManager::Destroy(entity);
	}
}
```

#### Q. 既存の `GameScene` や `Stage` クラスはどうなる？

**そのままでいい。** この仕組みは「Entityに付ける部品」を書きやすくしただけで、普通のC++クラスを禁止していない。シーン全体の段取り（ステージを作る、カメラを置く、ゲームオーバー判定）は今までどおり `GameScene` に書いて、その中から `gGameState` を読めばよい。

#### Q. Systemの中から他のEntityを触りたい

今までどおり `EntityManager` を直接呼べる。

```cpp
void Homing(Homing& homing, TransformComponent& transform, float deltaTime) {
	// 相手のHandleをComponentに覚えておく（EntityIdでもよい）
	if (TransformComponent* target = EntityManager::Get<TransformComponent>(homing.target)) {
		// target->translation に向かって進む
	}
}
```

`Handle<Entity>` はComponentの中に持ってよい（ただの数字2つなので `is_trivially_copyable` を壊さない）。

---

### F-19. 出てくるSTL・テンプレートの用語（読み飛ばし用の地図）

**まず前提：`GameComponent.h` の中身は読まなくていい。** 使うのは `COMPONENT` と `SYSTEM` と `ui.Field` の3つだけ。中で使っている道具は、以下の「何のためにあるか」だけ分かれば十分。

| 出てくる物 | 何のためにあるか |
|---|---|
| `std::vector<T>` | 可変長の配列。登録待ちの表・Systemの表 |
| `std::unique_ptr<T>` | 「持ち主が1人だけ」のポインタ。持ち主が消えると中身も消える（deleteを書かなくていい） |
| `std::function<void(float)>` | **関数を変数に入れる箱**。`SYSTEM` が作った処理を表に並べておくために使う |
| `std::string` / `std::string_view` | 文字列 / 文字列の**見るだけの窓**（コピーしない。速い代わりに元が消えると無効） |
| 関数の中の `static` | その関数が**最初に呼ばれたとき**に1回だけ作られる変数。起動時の初期化の順番に左右されないので、登録待ちの表に使っている |
| `inline` 変数 | ヘッダに書いても「全体で1個」になる変数。`COMPONENT` が作る登録係はこれ |
| `template<class T>` | 型を後から決められる型・関数。`ComponentStorage<T>` など |
| `template<class... Args>`（パラメータパック） | **引数が何個でもよい**テンプレート。`SYSTEM` が関数の引数を数える所で使う |
| `std::tuple<A, B, C>` | 違う型をまとめて1個にした入れ物（構造体の名前なし版） |
| `std::apply(f, tuple)` | tupleの中身をばらして関数に渡す |
| `std::index_sequence<0,1,2>` | 「0,1,2番目」という**番号の並び**を型として持つ道具。tupleから何番目を取り出すかを決めるのに使う |
| `std::is_same_v<A, B>` | AとBが同じ型か（コンパイル時に決まる `true` / `false`） |
| `std::remove_cvref_t<T>` | `const Spin&` → `Spin` のように、`const` と `&` を取り払う |
| `if constexpr (…)` | **コンパイル時に**分岐する。偽の方は機械語にならない（`deltaTime` を渡す/渡さないの切り替え） |
| `(… && …)`（畳み込み式） | パックの全部に `&&` を掛ける。「残りのComponentを全部持っているか」の判定 |
| 部分特殊化（`struct LastArg<T, Rest...>`） | 「この形の型が来たときは、こう扱う」という**型のパターンマッチ**。引数の並びから最後の1個を取り出すのに使っている |

**どこに何が書いてあるか**

| 場所 | 中身 | 読む必要 |
|---|---|---|
| `ComponentUI.h` 前半（`FieldStyle`・`Range`・`AtLeast`） | 見せ方の指定 | ○（使う） |
| `ComponentUI.h` 後半（`class ComponentUI`） | `ui.Field` の一覧表 | ○（どの型が書けるか見るとき） |
| `ComponentUI.cpp` | ImGuiを呼ぶ所 | △（見た目を変えたくなったら） |
| `GameComponent.h` 前半（`DescribedComponentEditor`・`ComponentRegistrar`） | 登録のしくみ | △ |
| `GameComponent.h` 中ほど（`namespace ComponentSystemDetail`） | **引数の型を数える所。ここが一番むずかしい** | **×（読まなくていい）** |
| `GameComponent.h` 末尾（`#define COMPONENT` / `#define SYSTEM`） | 書く形そのもの | ○ |
| `GameComponent.cpp` | 登録待ちの表とSystemの表 | △ |

`ComponentSystemDetail` が何をしているかを一言で言うと、**`void SpinUpdate(Spin&, TransformComponent&, float)` という書き方から「Spin と TransformComponent を持つEntityを回して、最後にdeltaTimeを渡す」という手順を、コンパイル時に組み立てている**。手で書くと毎回こうなる物を、型から自動で作っているだけ。

```cpp
// SYSTEM(SpinUpdate) が組み立てているのは、要するにこれ
EntityManager::ForEach<Spin>([deltaTime](Handle<Entity> entity, Spin& spin) {
	if (!EntityManager::IsActiveInHierarchy(entity)) { return; }
	TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	if (transform == nullptr) { return; }
	SpinUpdate(spin, *transform, deltaTime);
});
```


---

> ここから下は、Codexが書いた手順1〜8（2026-09-18）。**手順1〜6は写経済み**（未コミット）。手順7の `Health` の例は、上の手順Eの `Spin` に置き換えた（写す必要は無い）。記録として残す。

## 今回の位置づけ

基準コミットは `e04d804`。作業ブランチは `feature/component-storage`（developから作成）、Push禁止。
**このファイルは写経用。エンジンのソースはまだ変更していない。** 1〜6を写してからビルドする。
Editorの操作とは独立したRuntimeの管理基盤という大きな区切りなので、この話題をEntity.mdにまとめる。

`TypedComponentEditor<T>` がゲーム固有Componentにも使える、という理解は正しい。
ただし、これは「Tの編集方法」を提供する窓口。現在の `GetComponent` / `RequestAddComponent` の継承先を見ると、保管先は継承先が用意している。
今回はその保管先を共通化する。InspectorやUndoに `Health` / `Enemy` などの分岐は追加しない。

| 責務 | 今回の担当 |
|---|---|
| データの定義 | ゲーム側の `HealthComponent` など |
| 型ごとの保管・予約・破棄 | `ComponentStorage<T>`。エンジン側の共通テンプレート |
| Entityの寿命と全Storageの連携 | `EntityManager` |
| Inspectorの見た目 | ゲーム側の `TypedComponentEditor<T>` 継承クラス |
| 何を利用するか | ゲームの初期化でRuntime登録、EditorビルドではInspector登録も行う |
| 毎フレームの処理 | ゲーム側Systemから `EntityManager::ForEach<T>` |

### 先に決める規約

- 1 Entityにつき同じ型は1個。Componentの実体は型別の連続配列に置く。
- `Get<T>` は反映済みの実体を返す。予約中の値は返さない。未登録・未追加・無効なEntityならnullptr。
- `RequestAdd<T>` / `RequestRemove<T>` は予約のみ。`FlushComponentChanges` で反映する。
- 同じEntity・同じ型への予約は「最後に要求された状態」にまとめる。追加→削除なら無し、削除→追加なら新しい初期値。二重追加は最初の値を保つ。
- 別の型どうしの反映順序には依存しない。依存処理は全型の反映が終わってからSystemで行う。
- 予約も保管先も **Entityのindexとgenerationの両方** を使う。Undoで同じEntityIdが戻っても、古い予約は引き継がない。
- Entity削除では全型の実体と予約を消す。親を消したときは子も対象。
- Transformだけは全Entityに必須。削除不可。専用の寿命規則はこの1種類に限定する。
- 取得したポインタは構造変更までの一時利用。フレームをまたいで保持しない。`ForEach` 中は予約できるがCreate・Flush・登録・Releaseは禁止する。
- 現在の単一EntityManager・メインスレッド向け。複数World、スレッド安全、DLLの動的アンロード、Save形式まで完成したとは扱わない。

`std::type_index` は実行中だけの型検索用。`typeid(T).name()` や `hash_code()` をSaveに書いてはいけない。
Saveには別途安定した型ID・資産ID・形式バージョンを用意する。

## 1. 新規：MyEngine/Entity/ComponentStorage.h

Entityの一覧にComponentの型を列挙しないため、型を知らなくても「予約反映」「Entity破棄」を呼べる基底を置く。
実体へのアクセスはテンプレート側でTのまま行う。Component自身にvirtual関数は付けない。

<!-- file: MyEngine/Entity/ComponentStorage.h -->
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MyEngine/Core/Handle.h"

struct Entity;

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void Flush(bool (*isAlive)(Handle<Entity>)) = 0;
    virtual void DestroyEntity(Handle<Entity> entity) = 0;
};

template<class T>
class ComponentStorage final : public IComponentStorage {
    static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
    static_assert(std::is_class_v<T> && std::is_trivially_copyable_v<T>);
    static_assert(std::is_default_constructible_v<T> && std::is_copy_assignable_v<T>);

public:
    struct Entry {
        Handle<Entity> entity;
        T value;
    };

    T* Get(Handle<Entity> entity) {
        const auto it = indices_.find(Key(entity));
        return it == indices_.end() ? nullptr : &entries_[it->second].value;
    }

    bool RequestAdd(Handle<Entity> entity, const T& initial) {
        const uint64_t key = Key(entity);
        const auto pending = pending_.find(key);
        // 予約後の状態で判断する。削除予約の後なら、現在実体があっても追加できる。
        const bool willExist = pending != pending_.end()
            ? pending->second.has_value() : indices_.contains(key);
        if (willExist) {
            return false;
        }
        pending_.insert_or_assign(key, initial);
        return true;
    }

    bool RequestRemove(Handle<Entity> entity) {
        const uint64_t key = Key(entity);
        const auto pending = pending_.find(key);
        const bool willExist = pending != pending_.end()
            ? pending->second.has_value() : indices_.contains(key);
        if (!willExist) {
            return false;
        }
        pending_.insert_or_assign(key, std::nullopt);
        return true;
    }

    bool IsAddPending(Handle<Entity> entity) const {
        const auto it = pending_.find(Key(entity));
        return it != pending_.end() && it->second.has_value();
    }

    void Flush(bool (*isAlive)(Handle<Entity>)) override {
        for (const auto& [key, value] : pending_) {
            const Handle<Entity> entity = FromKey(key);
            if (!isAlive(entity)) {
                continue; // 削除済み・世代が違う予約は捨てる
            }
            if (value) {
                SetNow(entity, *value);
            } else {
                RemoveNow(entity);
            }
        }
        pending_.clear();
    }

    void DestroyEntity(Handle<Entity> entity) override {
        pending_.erase(Key(entity)); // 実体がまだ無い追加予約も消す
        RemoveNow(entity);
    }

    // 以下はEntityManagerから、フレーム境界でのみ使う。
    // Entity生成時の必須Transformと、予約反映の共通処理。
    void SetNow(Handle<Entity> entity, const T& value) {
        if (T* existing = Get(entity)) {
            *existing = value;
            return;
        }
        const size_t index = entries_.size();
        entries_.push_back({entity, value});
        try {
            indices_.emplace(Key(entity), index);
        } catch (...) {
            entries_.pop_back(); // 索引の確保に失敗しても片方だけ増やさない
            throw;
        }
    }

    template<class F>
    void ForEach(F&& function) {
        for (Entry& entry : entries_) {
            function(entry.entity, entry.value);
        }
    }

private:
    static uint64_t Key(Handle<Entity> entity) {
        return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
    }
    static Handle<Entity> FromKey(uint64_t key) {
        return {static_cast<uint32_t>(key), static_cast<uint32_t>(key >> 32)};
    }

    void RemoveNow(Handle<Entity> entity) {
        const auto it = indices_.find(Key(entity));
        if (it == indices_.end()) {
            return;
        }
        const size_t index = it->second;
        const size_t last = entries_.size() - 1;
        if (index != last) {
            entries_[index] = std::move(entries_[last]);
            indices_.at(Key(entries_[index].entity)) = index;
        }
        entries_.pop_back();
        indices_.erase(it);
    }

    std::vector<Entry> entries_; // 生きている実体だけ。削除は末尾と交換するので順序は不定。
    std::unordered_map<uint64_t, size_t> indices_; // Entityの世代込みキー → 配列位置
    // valueあり＝追加（削除→追加なら置換）、nullopt＝削除。
    std::unordered_map<uint64_t, std::optional<T>> pending_;
};
```

## 2. MyEngine/Entity/Entity.h を置き換える

Componentへの個別HandleをEntityに増やし続けない。名前・親子・IDは今のまま残す。
ゲーム側で `entity->transform` / `entity->render` を直接参照している場合は、`EntityManager::GetTransform` / `GetModelRenderer` に変更する。今回確認したエンジン内の参照は手順4で置き換わる。

<!-- file: MyEngine/Entity/Entity.h -->
```cpp
#pragma once
#include <cstdint>
#include <string>

#include "MyEngine/Core/Handle.h"

using EntityId = uint64_t; // Undoで復元する番号。0は無し。実行中Handleとは別物。

struct Entity {
    std::string name = "Entity";
    EntityId id = 0;
    Handle<Entity> self;
    Handle<Entity> parent;
    bool isActive = true;
};
```

## 3. MyEngine/Entity/EntityManager.h を置き換える

既存のModelRenderer専用APIは互換ラッパーとして残す。既存ゲームの呼び出しを一度に壊さず、保管の実体だけを共通化する。
今後のComponentには専用ラッパーを増やさない。

<!-- file: MyEngine/Entity/EntityManager.h -->
```cpp
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Entity/ComponentStorage.h"
#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Entity/ModelRendererComponent.h"
#include "MyEngine/Entity/TransformComponent.h"

class EntityManager {
public:
    static void Initialize();
    static void Release();

    static Handle<Entity> Create(const std::string& name = "Entity", Handle<Entity> parent = {});
    static Handle<Entity> CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent);
    static EntityId NewId();
    static void Destroy(Handle<Entity> handle); // 予約。子孫もFlushDestroyで消す。

    static Entity* Get(Handle<Entity> handle);
    static bool IsAlive(Handle<Entity> handle);
    static Handle<Entity> FindById(EntityId id);
    static size_t GetCount();
    static SlotMap<Entity>& GetAll() { return instance_->entities_; }

    // Initialize後、ゲーム/シーンの初期化で登録する。同じ型の再登録は何もしない。
    // Editorが無いReleaseでも呼ぶ。Inspectorの登録とは独立している。
    template<class T>
    static void RegisterComponent() {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
        EnsureStructuralChangesAllowed();
        const std::type_index key(typeid(T));
        if (!instance_->components_.contains(key)) {
            instance_->components_.emplace(key, std::make_unique<ComponentStorage<T>>());
        }
    }

    template<class T>
    static T* Get(Handle<Entity> handle) {
        if (!IsAlive(handle)) {
            return nullptr;
        }
        auto* storage = FindStorage<T>();
        return storage ? storage->Get(handle) : nullptr;
    }

    template<class T>
    static bool RequestAdd(Handle<Entity> handle, const T& initial = {}) {
        if (!IsAlive(handle)) {
            return false;
        }
        return RequireStorage<T>().RequestAdd(handle, initial);
    }

    // 必須Transformの削除をRuntime側でも防ぐ。UIだけで禁止して終わりにしない。
    template<class T>
    static constexpr bool IsRemovable() { return !std::is_same_v<T, TransformComponent>; }

    template<class T>
    static bool RequestRemove(Handle<Entity> handle) {
        if constexpr (!IsRemovable<T>()) {
            return false;
        } else {
            if (!IsAlive(handle)) {
                return false;
            }
            return RequireStorage<T>().RequestRemove(handle);
        }
    }

    template<class T>
    static bool IsAddPending(Handle<Entity> handle) {
        auto* storage = FindStorage<T>();
        return IsAlive(handle) && storage && storage->IsAddPending(handle);
    }

    // Entity全件を検索せず、その型を持つものだけを走査する。
    // function(entityHandle, component)内では値変更・追加削除の予約ができる。
    // 構造変更は走査後のフレーム境界。参照は呼び出しの外へ保存しない。
    template<class T, class F>
    static void ForEach(F&& function) {
        auto* storage = FindStorage<T>();
        if (!storage) {
            return;
        }
        IterationScope scope(*instance_);
        storage->ForEach(std::forward<F>(function));
    }

    static void FlushComponentChanges();
    static void FlushDestroy();
    static void UpdateTransforms();

    static void SetParent(Handle<Entity> child, Handle<Entity> parent);
    static bool IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor);
    static void GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
    static void GetRoots(std::vector<Handle<Entity>>& out);

    // 移行用。中身は共通APIへ委譲し、専用配列は持たない。
    static TransformComponent* GetTransform(Handle<Entity> h) { return Get<TransformComponent>(h); }
    static ModelRendererComponent* GetModelRenderer(Handle<Entity> h) { return Get<ModelRendererComponent>(h); }
    static void RequestAddModelRenderer(Handle<Entity> h, const ModelRendererComponent& value = {}) {
        RequestAdd<ModelRendererComponent>(h, value);
    }
    static void RequestRemoveModelRenderer(Handle<Entity> h) { RequestRemove<ModelRendererComponent>(h); }
    static bool IsModelRendererAddPending(Handle<Entity> h) { return IsAddPending<ModelRendererComponent>(h); }

private:
    static constexpr uint32_t kMaxParentDepth = 64;
    static EntityManager* instance_;
    static void EnsureStructuralChangesAllowed();

    // コールバックが例外で抜けても、走査中の印を必ず戻す。
    class IterationScope {
    public:
        explicit IterationScope(EntityManager& manager) : manager_(manager) { ++manager_.iterationDepth_; }
        ~IterationScope() { --manager_.iterationDepth_; }
        IterationScope(const IterationScope&) = delete;
        IterationScope& operator=(const IterationScope&) = delete;
    private:
        EntityManager& manager_;
    };

    template<class T>
    static ComponentStorage<T>* FindStorage() {
        // const T等で同じtypeidから異なるStorageへキャストしない。
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
        if (!instance_) {
            return nullptr;
        }
        const auto it = instance_->components_.find(std::type_index(typeid(T)));
        return it == instance_->components_.end()
            ? nullptr : static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template<class T>
    static ComponentStorage<T>& RequireStorage() {
        auto* storage = FindStorage<T>();
        MY_ASSERT_MSG(storage != nullptr, "RegisterComponent<T>()を先に呼んでください");
        return *storage;
    }

    void CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
    uint32_t CalcDepth(Handle<Entity> handle);

    SlotMap<Entity> entities_;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> components_;
    std::unordered_map<EntityId, Handle<Entity>> idToHandle_;
    EntityId nextId_ = 1;
    size_t iterationDepth_ = 0;
    std::vector<Handle<Entity>> pendingDestroy_;
    std::vector<std::pair<uint32_t, Handle<Entity>>> updateOrder_;
    std::vector<Handle<Entity>> destroyWork_;
};
```

## 4. MyEngine/Entity/EntityManager.cpp の変更

### 4-A. Initialize / Release を置き換え、検査関数を追加

<!-- replace: MyEngine/Entity/EntityManager.cpp | void EntityManager::Initialize() -->
```cpp
void EntityManager::Initialize() {
    MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
    instance_ = new EntityManager();
    RegisterComponent<TransformComponent>();
    RegisterComponent<ModelRendererComponent>();
    LogManager::Log("Initialized");
}
```

<!-- replace: MyEngine/Entity/EntityManager.cpp | void EntityManager::Release() -->
```cpp
void EntityManager::Release() {
    EnsureStructuralChangesAllowed();
    delete instance_; // 全Storageと、その予約もunique_ptrが解放する
    instance_ = nullptr;
    LogManager::Log("Released");
}
```

<!-- append: MyEngine/Entity/EntityManager.cpp -->
```cpp
void EntityManager::EnsureStructuralChangesAllowed() {
    MY_ASSERT_MSG(instance_ != nullptr, "EntityManager::Initialize()を先に呼んでください");
    MY_ASSERT_MSG(instance_->iterationDepth_ == 0, "ForEachの途中で構造を変えず、予約してください");
}
```

### 4-B. CreateWithId と IsAlive を置き換える

<!-- replace: MyEngine/Entity/EntityManager.cpp | Handle<Entity> EntityManager::CreateWithId -->
```cpp
Handle<Entity> EntityManager::CreateWithId(EntityId id, const std::string& name, Handle<Entity> parent) {
    EnsureStructuralChangesAllowed();
    MY_ASSERT_MSG(id != 0 && !FindById(id).IsValid(), "EntityIdが0か、もう使われています");
    const Handle<Entity> handle = instance_->entities_.Create();
    Entity* entity = instance_->entities_.Get(handle);
    entity->name = name;
    entity->id = id;
    entity->self = handle;
    instance_->idToHandle_[id] = handle;
    instance_->nextId_ = (std::max)(instance_->nextId_, id + 1);
    // 全Entityの必須データ。Create自体がフレーム境界なので即時に作る。
    RequireStorage<TransformComponent>().SetNow(handle, TransformComponent{});
    if (parent.IsValid()) {
        SetParent(handle, parent);
    }
    return handle;
}
```

<!-- replace: MyEngine/Entity/EntityManager.cpp | bool EntityManager::IsAlive -->
```cpp
bool EntityManager::IsAlive(Handle<Entity> handle) {
    return instance_ && instance_->entities_.IsAlive(handle);
}
```

### 4-C. 古い専用関数を削除し、Flushを置き換える

次の5関数の定義はcppから削除する。hに互換ラッパーを書いたため、残すと二重定義になる。

- `GetTransform`
- `RequestAddModelRenderer`
- `RequestRemoveModelRenderer`
- `IsModelRendererAddPending`
- `GetModelRenderer`

<!-- replace: MyEngine/Entity/EntityManager.cpp | void EntityManager::FlushComponentChanges() -->
```cpp
void EntityManager::FlushComponentChanges() {
    EnsureStructuralChangesAllowed();
    for (auto& [type, storage] : instance_->components_) {
        storage->Flush(&EntityManager::IsAlive);
    }
}
```

### 4-D. UpdateTransforms の1行を変更

`self.transforms_.Get(entity->transform)` を `Get<TransformComponent>(handle)` に置き換える。
残りの親子順・行列計算はそのまま。

### 4-E. FlushDestroy を置き換える

<!-- replace: MyEngine/Entity/EntityManager.cpp | void EntityManager::FlushDestroy() -->
```cpp
void EntityManager::FlushDestroy() {
    EnsureStructuralChangesAllowed();
    EntityManager& self = *instance_;
    if (self.pendingDestroy_.empty()) {
        return;
    }
    self.destroyWork_.clear();
    for (Handle<Entity> handle : self.pendingDestroy_) {
        self.CollectDescendants(handle, self.destroyWork_);
    }
    self.pendingDestroy_.clear();

    for (Handle<Entity> handle : self.destroyWork_) {
        Entity* entity = self.entities_.Get(handle);
        if (!entity) {
            continue;
        }
        // ゲームが登録した未知の型も含め、実体と未反映の予約をすべて消す。
        for (auto& [type, storage] : self.components_) {
            storage->DestroyEntity(handle);
        }
        self.idToHandle_.erase(entity->id);
        self.entities_.Destroy(handle);
    }
}
```

## 5. TypedComponentEditorを共通の保管先へつなぐ

### 5-A. ComponentEditor.h

`#include "MyEngine/Entity/EntityManager.h"` を追加する。
`ComponentCategory` の末尾に `Gameplay, // ゲーム固有のデータ` を追加する。
`TypedComponentEditor` の定義だけを次へ置き換える（`ComponentEditor` と `ComponentEditorRegistry` は残す）。
`ComponentEditor` の説明コメントも「Runtimeに型を登録し、EditorではTypedComponentEditorの派生クラスを登録する」に更新する。

<!-- replace-class: MyEngine/Editor/Inspector/ComponentEditor.h | template<class T> -->
```cpp
template<class T>
class TypedComponentEditor : public ComponentEditor {
    // これはバイトコピーできることの検査。ポインタを含まないことまでは検査できない。
    static_assert(std::is_trivially_copyable_v<T>);

public:
    size_t GetSize() const final { return sizeof(T); }
    bool IsOptional() const final { return EntityManager::IsRemovable<T>(); }
    void* Get(Handle<Entity> handle) const final { return EntityManager::Get<T>(handle); }
    bool IsAddPending(Handle<Entity> handle) const final { return EntityManager::IsAddPending<T>(handle); }
    void RequestAdd(Handle<Entity> handle, const void* initial) const final {
        T value{};
        if (initial) {
            std::memcpy(&value, initial, sizeof(T));
        }
        EntityManager::RequestAdd<T>(handle, value);
    }
    void RequestRemove(Handle<Entity> handle) const final { EntityManager::RequestRemove<T>(handle); }
    void Draw(void* component) const final { DrawComponent(*static_cast<T*>(component)); }

protected:
    virtual void DrawComponent(T& component) const = 0;
};
```

`TypedComponentEditor` は今回からEntityManager標準Storage用になる。LightManagerなど独自の保管先を使う段階では、無理にここへキャストせず、基底の `ComponentEditor` を直接継承するか、実体の管理を移行してから使う。

### 5-B. 既存のEditorから不要な橋渡しを消す

| ファイル | 消す宣言・定義 |
|---|---|
| TransformEditor.h | `IsOptional`、`GetComponent` |
| TransformEditor.cpp | `TransformEditor::GetComponent` |
| ModelRendererEditor.h | `IsAddPending`、`RequestRemove`、`GetComponent`、`RequestAddComponent` |
| ModelRendererEditor.cpp | 上の4関数の定義 |

`GetName` / `GetCategory` / `DrawComponent` / `DrawModelPicker` は変更しない。
`InspectorWindow` の描画処理や `EditorHistory` の型別分岐を追加する必要はない。

## 6. Undoの復元単位をComponent全体に揃える

現在の `RecordComponentChange` は差分を4バイトずつ広げて戻す。これは任意のTの意味を知らない処理なので、double・uint64_tなどの一部だけを戻したり、隣のboolも巻き込んだりする可能性がある。
フィールドを中央に列挙する方式へ戻さず、**Component全体を1つの編集単位として記録・復元する**。

この変更後の意味は明確にする：操作開始時のComponent全体がbefore、最後にInspectorで変更した時点の全体がafter。Undo時は全体を戻すので、同じComponentにゲームが書いた別の値も戻る。
編集設定とゲーム中だけの状態を独立して戻したいときは、別Component／System側の状態に分ける。これは「変更した項目だけ戻す」とは異なる仕様。
TransformのworldMatrixは次の `UpdateTransforms` で再計算される。これを保存データとして扱わない。

`EditorHistory.cpp` の以下4か所だけを変更する。他の履歴・コピー・復元処理はそのまま。
併せて、`ApplyEdits` の直前のコメントを「Component全体を書き戻す（undoならbefore、redoならafter）」へ、`EditorHistory.h` の `RecordComponentChange` のコメントを「Inspectorの前後を比較し、変更があればComponent全体の前後を記録する」へ更新する。

### 6-A. ComponentEdit

<!-- replace-struct: MyEngine/Editor/History/EditorHistory.cpp | struct ComponentEdit -->
```cpp
struct ComponentEdit {
    EntityId entity = 0;
    const ComponentEditor* editor = nullptr;
    std::vector<std::byte> before; // 編集開始時のComponent全体
    std::vector<std::byte> after;  // 最後にInspectorで変更した時点のComponent全体
};
```

### 6-B. ApplyEdits

<!-- replace: MyEngine/Editor/History/EditorHistory.cpp | bool ApplyEdits -->
```cpp
bool ApplyEdits(const std::vector<ComponentEdit>& edits, bool undo) {
    // 一部だけ復元して失敗しないよう、先に全対象を確認する。
    for (const ComponentEdit& edit : edits) {
        if (!edit.editor->Get(EntityManager::FindById(edit.entity))) {
            return false;
        }
    }
    for (const ComponentEdit& edit : edits) {
        void* target = edit.editor->Get(EntityManager::FindById(edit.entity));
        const auto& source = undo ? edit.before : edit.after;
        std::memcpy(target, source.data(), source.size());
    }
    return true;
}
```

### 6-C. RecordComponentChange

<!-- replace: MyEngine/Editor/History/EditorHistory.cpp | void EditorHistory::RecordComponentChange -->
```cpp
void EditorHistory::RecordComponentChange(Handle<Entity> handle, const ComponentEditor& editor, const void* before, const void* after) {
    const size_t size = editor.GetSize();
    if (std::memcmp(before, after, size) == 0) {
        return;
    }
    const EntityId id = IdOf(handle);
    auto edit = std::find_if(pendingEdits.begin(), pendingEdits.end(), [&](const ComponentEdit& e) {
        return e.entity == id && e.editor == &editor;
    });
    const auto* oldBytes = static_cast<const std::byte*>(before);
    const auto* newBytes = static_cast<const std::byte*>(after);
    if (edit == pendingEdits.end()) {
        pendingEdits.push_back({id, &editor,
            std::vector<std::byte>(oldBytes, oldBytes + size),
            std::vector<std::byte>(newBytes, newBytes + size)});
    } else {
        edit->after.assign(newBytes, newBytes + size); // beforeは操作開始時のまま
    }
}
```

### 6-D. CommitComponentChanges

<!-- replace: MyEngine/Editor/History/EditorHistory.cpp | void EditorHistory::CommitComponentChanges() -->
```cpp
void EditorHistory::CommitComponentChanges() {
    if (pendingEdits.empty()) {
        return;
    }
    std::vector<ComponentEdit> edits = std::move(pendingEdits);
    pendingEdits.clear();
    std::erase_if(edits, [](const ComponentEdit& edit) { return edit.before == edit.after; });
    if (!edits.empty()) {
        requests.push_back([edits] {
            Append({[edits] { return ApplyEdits(edits, true); }, [edits] { return ApplyEdits(edits, false); }});
        });
    }
}
```

注意：memcmpはパディングも比較するので、同じ意味の値でも余分な履歴ができる可能性は残る。描画関数ではComponent全体を作り直さず、編集されたメンバへ書き込む。
今のバイトスナップショットは同一実行内・同一型定義専用。Save形式やDLLリロード用には使わない。

## 7. ゲーム固有Componentで確認する

これはエンジン内へ追加するクラスではない。ゲームプロジェクトに次の2つを作る。

### HealthComponent.h（ゲーム側）

<!-- example: HealthComponent.h -->
```cpp
#pragma once

// 設定・状態をどこまで同じUndo単位にするかはゲーム側で決める。
// 独立して扱う現在HPと初期HPが必要なら、別Componentへ分ける。
struct HealthComponent {
    int hitPoints = 100;
    int maxHitPoints = 100;
};
```

### HealthEditor.h（ゲーム側・USE_IMGUI時だけincludeする）

<!-- example: HealthEditor.h -->
```cpp
#pragma once
#include <externals/imgui/imgui.h>

#include "HealthComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"

class HealthEditor final : public TypedComponentEditor<HealthComponent> {
public:
    const char* GetName() const override { return "Game/Health"; }
    ComponentCategory GetCategory() const override { return ComponentCategory::Gameplay; }

protected:
    void DrawComponent(HealthComponent& value) const override {
        ImGui::DragInt("HP", &value.hitPoints, 1.0f, 0, value.maxHitPoints);
        ImGui::DragInt("Max HP", &value.maxHitPoints, 1.0f, 1, 10000);
    }
};
```

### 登録と使用

ゲームのシーンcppで `HealthComponent.h` と `EntityManager.h` をincludeする。
`#ifdef USE_IMGUI` 内で `<memory>` と `HealthEditor.h` をincludeする。
シーンの `Initialize` 冒頭（EntityManager初期化後・Inspector描画前）で次を呼ぶ。
同じ登録をStop/Restartで呼び直してよい。Inspector名はプロジェクト内で一意にする（現在のRegistryは名前で重複判定）。

```cpp
EntityManager::RegisterComponent<HealthComponent>();
#ifdef USE_IMGUI
ComponentEditorRegistry::Register(std::make_unique<HealthEditor>());
#endif
```

UIからAdd Component → Gameplay → Game/Healthで追加できる。
コードからも使える。既存のsceneRoot配下に作り、シーンFinalizeで親を破棄する規約は続ける。

```cpp
// Initializeなど、Entityを作ってよいタイミングで行う。
const auto player = EntityManager::Create("Player", sceneRoot);
EntityManager::RequestAdd<HealthComponent>(player, HealthComponent{80, 100});
// この時点のGet<HealthComponent>(player)はnullptr。次のフレーム境界で追加される。
```

SystemのUpdateでは `EntityManager::ForEach<HealthComponent>([](Handle<Entity> entity, HealthComponent& health) { /* ... */ });` と走査できる。
ForEachは所有Entityも渡すので、必要なTransformなどを `Get<TransformComponent>(entity)` で取得する。
isActive・シーン所属での絞り込みは自動ではない。System側で判定する。

**Runtime登録だけではEditorのコピー対象にはならない。** 現在のEditorHistoryはEditor登録表からコピー対象を集める。
編集・複製したいゲームのデータにはEditorも登録する。ゲーム実行時だけの作業データはSystemで作り直す。
Component内に別Entityへの参照を持つ場合、Undo復元にはEntityIdを使う。複製時の「参照先も複製先へ付け替える」機能は未実装で、値は元の参照先のままコピーされる。今回のHealth例には参照を持たせていない。

## 8. ビルドと実行確認

新規 `ComponentStorage.h` をVSプロジェクトのEntityフォルダへ追加する（cpp追加は無し）。
エンジンをDebug / Releaseでビルドし、更新した全ヘッダーとlibをゲーム側へ配布してから、ゲームも両構成でリビルドする。
古いEntity.hと新しいlibを混在させない。Releaseでも `RegisterComponent` を呼び、ImGui用include・登録だけを条件付きにする。

確認する順番：

1. 既存のTransform・ModelRendererの表示、描画、追加／取り外し、親子行列が変わらない。
2. Game/Healthが追加できる。数値を編集→Undo→Redo。ドラッグは1操作にまとまる。
3. Healthを取り外し→Undoで値ごと戻る。子を持つEntityの削除→Undoでも戻る。
4. Health付きEntityのコピー／貼り付けで値が独立する。貼り付け先の編集で元が変わらない。
5. 追加→削除、削除→追加、二重追加を同じフレームで予約して規約どおりになる。
6. 追加予約を持つEntityを削除→同じスロットへ別Entityを作っても、古い予約が付かない。
7. Stop/RestartでRuntime登録・Inspector登録が重複しない。ReleaseでImGui無しでも追加・取得・削除できる。

### 自動検証の記録

2026-09-18、このmdのコードを検証用フォルダへ抽出し、記載した削除・置換も適用して確認した。実際のソースとプロジェクトファイルは変更していない。

- MSVC 14.51、C++latest、/W4：エンジン全76 cpp＋HealthEditor例をDebug / Releaseで構文チェックし、両方エラー0。これは実ゲーム全体のリンク・起動確認ではない。
- 抽出後のEntityManager・EditorHistory・ComponentEditorRegistry・既存の行列実装を静的ライブラリにし、別のテストcppからゲーム固有型を登録してリンク・実行。Debug / Releaseとも **23,187チェック通過**。
- 二重登録、未反映の追加、二重追加、追加→削除、削除→追加、末尾交換後の索引、型の独立性、ForEach中の予約と構造変更禁止、例外後の走査ガード解放を確認。
- 親子削除と予約の破棄、同じスロット・同じEntityIdで復元した後への古い予約混入防止、64バイト境界の型、既存ModelRenderer APIと親子行列を確認。
- 独自Componentの追加削除Undo、ドラッグのまとめ、64ビット整数・doubleの全体復元、子孫の削除Undo、古い編集履歴の再適用、コピー後の値の独立性を確認。
- 4,000回のランダムな追加・削除・破棄・再生成・Flushを、独立した期待状態と比較。

テスト実行時はログ・通知付きassertだけをテスト用へ差し替え、GPUとImGuiの描画は動かしていない。構文チェックは実ヘッダーを使用。
ゲームの描画・IME・実際のマウス操作は、上の手動確認を行う。

## この後

この段階は「任意のデータComponentを共通管理できる土台」。Entity自体のID化・複数World・保存までを一度に完成させる段階ではない。
まず上の動作確認。その後、Light.md Step 6でライトをこの管理へつなぐ。ライトの所有者を二重に作らず、LightManagerとの責務を整理してから写経コードを用意する。
Save・Hierarchyの整理用フォルダ・起動ローダーはその後の独立した設計項目として扱う。
