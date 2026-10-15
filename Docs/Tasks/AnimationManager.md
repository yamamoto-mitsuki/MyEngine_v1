# AnimationManager：glTF のアニメーションを読み込んで、番号で持つ

## このファイルについて

- 2026-10-14 の依頼：「読み込み・番号管理のコードを教えて。md ファイルに」。
- [Animation.md の6章](Animation.md#6-an2-1animationmanagergltf-のアニメーションを読み込んで番号で持つ2026-10-14) と同じコード。こちらは **読み込みと番号管理だけを、仕組みの説明付きで** 書いた（なぜ番号管理にするかは Animation.md 6.1）。
- コードはエンジンを Development・Release でビルドして 0エラーを確かめた物。アニメ入りの glTF がリポジトリに無いので、**読み込みの実行はまだ**（7章で確かめる）。
- 今の `AnimationManager.h`（Passkey まで書いた物）の続きから書く。
- **2026-10-14 追記**：パスを `std::filesystem::path` で受け取るように変えた（[2章の最後](#パスは-stdfilesystempath-で受け取る2026-10-14-追記)）。5章・6章のコードも差し替え済み。

| 章 | 中身 |
|---|---|
| [1. 全体の形](#1-全体の形) | 何をどこに持つか（図） |
| [2. 番号管理の仕組み](#2-番号管理の仕組み) | 3つの入れ物と、Load の流れ |
| [3. 読み込みの仕組み](#3-読み込みの仕組みassimp--animation) | assimp の中身と、エンジンの形への直し方 |
| [4. Animation.h に足す所](#4-animationh-に足す所) | 初期値と name |
| [5. AnimationManager.h（全部）](#5-animationmanagerh全部) | |
| [6. AnimationManager.cpp（全部）](#6-animationmanagercpp全部) | |
| [7. 登録と確認](#7-登録と確認) | vcxproj・Engine.cpp・動いたかの見方 |

---

## 1. 全体の形

```
ゲーム側                      AnimationManager（シングルトン）
                              ┌────────────────────────────────────────────┐
Load("walk.gltf") ──────────▶ │ pathToHandle_ : "walk.gltf" → 1            │ ← 同じパスはここで見つかって、読み直さない
        ◀──── 1 ─────────────│                                            │
                              │ files_ : 1 → AnimationFile                 │
GetAnimation(1, 0) ─────────▶ │            ├ path = "walk.gltf"            │
        ◀── const Animation* │            └ animations[0] = 歩き          │
                              │                       [1] = 走り（あれば）  │
                              │ nextHandle_ = 2（次に配る番号）            │
                              └────────────────────────────────────────────┘
```

- **使う側が持つのは番号（`uint32_t`）だけ。** 中身（キーの山）は AnimationManager が1つだけ持ち、何体で使っても共有する。
- 番号は **ファイルに1つ**。1つの glTF に「待機」「歩き」など複数入っていることがあるので、中は `index`（0, 1, 2…）で選ぶ。

---

## 2. 番号管理の仕組み

### 持つ物（3つ）

| メンバ | 型 | 役目 |
|---|---|---|
| `files_` | `std::unordered_map<uint32_t, AnimationFile>` | **番号 → 中身**。`GetAnimation` で引く |
| `pathToHandle_` | `std::unordered_map<std::string, uint32_t>` | **パスの文字 → 番号**。同じファイルを2回読まないため（キーは `ToPathKey`（ConvertString.h）でそろえた文字。下の「パスは filesystem::path で受け取る」） |
| `nextHandle_` | `uint32_t`（1から） | 次に配る番号。配るたびに +1。**0 は「失敗」の印**にするので使わない |

- `unordered_map`：キーから値をすぐ引ける入れ物（中で並べ替えないので `std::map` より速い。順番は要らないのでこちら）。
- 番号を `vector` の添字にしない理由：後で「1つだけ消す（Unload）」を足したときに、ほかの番号がずれないように。ModelManager・SoundManager も同じ形。

### Load の流れ

```
Load(path)
 ├─ ⓪ key = ToPathKey(path)（書き方をそろえた文字にする。ConvertString.h の共通の関数）
 ├─ ① pathToHandle_ に key がある？ → あればその番号を返して終わり（読まない）
 ├─ ② LoadAnimationFile(path) でファイルを読む → 空なら 0（失敗）を返して終わり
 ├─ ③ handle = nextHandle_++（今の値を使ってから +1）
 ├─ ④ files_[handle] に path と animations を入れる
 ├─ ⑤ pathToHandle_[key] = handle
 └─ ⑥ handle を返す
```

- 失敗しても **止めない（assert しない）で 0 を返す**：SoundManager と同じ。Inspector などで読めないファイルを選んでも、エディタが落ちないように。0 を `GetAnimation` に渡すと nullptr が返り、Log に警告が出るので気づける。
- `std::move(animations)`：読んだ中身をコピーせずに `files_` へ渡す（キーの数が多いとコピーは重い）。

### 取り出す

- `GetAnimation(handle, index)`：`files_` から探して、`animations[index]` のアドレスを返す。無ければ nullptr。
- `const Animation*` で返すのは、使う側が中身を書き換えられないように（みんなで共有している物なので）。
- **返ったポインタは、Release まで同じ物を指す**（`files_` に後から足しても、中の `vector` の中身は動かない）。ただし、毎フレーム `GetAnimation` し直しても速いので、覚えておくなら番号の方を覚える。

### パスは `std::filesystem::path` で受け取る（2026-10-14 追記）

`Load` の引数を `std::string` から `std::filesystem::path` にした。`GetPath` は、ほかの Manager の `GetXxxPath` とそろえて **`std::string`（そろえた UTF-8 の文字）を返す**（シーンファイルに保存する・Inspector に出す文字なので）。キーを作る `ToPathKey` と、UTF-8 の文字を path にする `ToPath` は ConvertString.h に置いた（[PathLoad.md](PathLoad.md)。4つの Manager で同じ物を使う）。

| 何が良くなるか | 中身 |
|---|---|
| 同じファイルを同じ物として扱える | `"a/b.gltf"`・`"a\b.gltf"`・`"a/./b.gltf"`・`"a/c/../b.gltf"` を、全部同じキー `"a/b.gltf"` にそろえてから `pathToHandle_` を引く（`ToPathKey`）。`std::string` のままだと、書き方が違うだけで別のファイルとして2回読んでしまう |
| 日本語のパスで壊れない | assimp には `ToUtf8(path)`（エンジンにもうある関数。区切りを `/` にした UTF-8 の文字）で渡す。`path::string()` は日本語の Windows では Shift-JIS になって、assimp が読めない |

- **呼ぶ側は今まで通り文字で書いてよい**：`AnimationManager::Load("resources/models/a.gltf")` のように書くと、文字から path が自動で作られる（暗黙の変換）。
- `lexically_normal()`：`.` や `..` を消して、パスを一番短い形にする。**ファイルを見に行かず、文字だけで直す**（ファイルが無くても動く。`std::filesystem::canonical` はファイルを見に行き、無いと例外になるので使わない）。
- `pathToHandle_` のキーは `std::string`（`ToPathKey` で作った UTF-8 の文字）のまま。`std::filesystem::path` 同士の `==` は Windows で大文字・小文字の扱いなどが分かりにくいので、そろえた文字で比べる方が確実。
- **注意**：中身が UTF-8 の `std::string`（ImGui の入力欄・シーンファイルから読んだ文字など）を、そのまま `Load` に渡すと、`std::string` → path の変換が「Shift-JIS の文字」として読むので、日本語が化ける。そういう文字は `ToPath(utf8String)`（ConvertString.h）で path にしてから渡す。英数字だけのパスなら気にしなくてよい。

---

## 3. 読み込みの仕組み（assimp → Animation）

### assimp の中身

```
aiScene
 └ mAnimations[i]          … aiAnimation（1本のアニメ。mNumAnimations 本）
     ├ mName               … 名前（Blender のアクション名など）
     ├ mDuration           … 長さ（tick）
     ├ mTicksPerSecond     … 1秒が何 tick か
     └ mChannels[c]        … aiNodeAnim（ノード1つ分の動き。mNumChannels 個）
         ├ mNodeName       … どのノードか（ModelManager の Node::name と同じ）
         ├ mPositionKeys[] … 位置のキー（mTime と mValue(x,y,z)）
         ├ mRotationKeys[] … 回転のキー（mTime と mValue(x,y,z,w)）
         └ mScalingKeys[]  … 大きさのキー
```

エンジンの形との対応：`aiAnimation` → `Animation`、`aiNodeAnim` → `NodeAnimation`、各キーの配列 → `AnimationCurve`、キー1つ → `Keyframe`。

### 直すこと（3つ）

1. **時間を秒に**：assimp の時間は「tick」という単位。`秒 = tick ÷ mTicksPerSecond`。`mTicksPerSecond` が 0 のファイルもあるので、そのときは assimp の決まりの 25 を使う。glTF は元が秒なので、assimp が 1000（ミリ秒）にしてくることが多いが、割れば同じく秒になる。`duration` も同じく割る。
2. **向きを ModelManager にそろえる**：ModelManager は読むときに x を反転している（右手系 → 左手系）。キーも同じにしないと、動きが左右逆になる。

   | キー | 直し方 |
   |---|---|
   | 位置 | `(-x, y, z)` |
   | 回転 | `(x, -y, -z, w)`（x を反転した世界では、回る向きが変わるので y と z を反転） |
   | 大きさ | そのまま（向きに関係ない） |
3. **ノード名で入れる**：`animation.nodeAnimations[ノード名]`。`std::map` の `[]` は「無ければ空の物を作ってから返す」ので、探す → 無ければ足す、が1行で書ける。

- 読み込みのオプションは `0`：ModelManager が付けている `aiProcess_Triangulate` などは頂点を直すための物で、アニメには関係ない。
- `Assimp::Importer` は関数を抜けると、読んだ物を全部消す。だから **関数の中でエンジンの形（`Animation`）に写し終えてから** 返す（`aiScene` のポインタを外に持ち出さない）。

---

## 4. Animation.h に足す所

初期値の無いメンバに初期値を書く（無いと、何が入っているか分からない値から始まる）。`Animation` に名前を足す。

```cpp
// ===== キーフレーム =====
template <typename T> 
struct Keyframe {
	T value{};         // 値
	float time = 0.0f; // 時刻（秒）
};
```

```cpp
// ===== Animation =====
struct Animation {
	std::string name;       // アニメーションの名前（Blender のアクション名など。1ファイルに複数あるとき見分ける）
	float duration = 0.0f;  // 全体の尺（秒）
	std::map<std::string, NodeAnimation> nodeAnimations; // NodeAnimationの集合
};
```

---

## 5. AnimationManager.h（全部）

今の AnimationManager.h を、これに置き換える（Passkey の所はそのまま）。

```cpp
#pragma once
#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "Animation.h"


/// <summary>
/// アニメーションファイル（glTF など）を読み込み、番号（ハンドル）で管理する
/// <para>同じパスを2回 Load しても読み直さず、同じ番号を返す（ModelManager・SoundManager と同じ形）</para>
/// <para>1つのファイルに複数のアニメーションが入っていることがあるので、番号は「ファイル」に付け、中のアニメーションは index で選ぶ</para>
/// </summary>
class AnimationManager {
public:
	static constexpr uint32_t kInvalidHandle = 0; // 読み込みに失敗したときの番号

	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class AnimationManager;
	};
	explicit AnimationManager(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	// コピー禁止
	AnimationManager(const AnimationManager&) = delete;
	AnimationManager& operator=(const AnimationManager&) = delete;

	// 初期化・解放
	static void Initialize();
	static void Release();

	/// <summary>
	/// アニメーションファイルを読み込む。同じパスならキャッシュから返す
	/// </summary>
	/// <param name="filePath">ファイルのパス（例: "resources/models/AnimatedCube/AnimatedCube.gltf"）。"a/b" と "a\b"、"a/./b" も同じファイルとして扱う</param>
	/// <returns>ハンドル。読めなかった・アニメーションが無いときは kInvalidHandle</returns>
	static uint32_t Load(const std::filesystem::path& filePath);

	/// <summary>
	/// アニメーションを取得する
	/// </summary>
	/// <param name="handle">Load が返したハンドル</param>
	/// <param name="index">ファイルの中の何番目か（ふつうは0）</param>
	/// <returns>見つからなければ nullptr</returns>
	static const Animation* GetAnimation(uint32_t handle, uint32_t index = 0);

	// ファイルの中のアニメーションの数（見つからなければ0）
	static uint32_t GetAnimationCount(uint32_t handle);
	// 読み込んだときのパス（ToPathKey でそろえた UTF-8 の文字。見つからなければ空）。後でシーンに保存するときに、番号をパスに直すのに使う
	static const std::string& GetPath(uint32_t handle);

private:
	// ===== 1ファイル分 =====
	struct AnimationFile {
		std::string path;                  // 読み込んだときのパス（ToPathKey でそろえた UTF-8 の文字）
		std::vector<Animation> animations; // ファイルの中のアニメーション全部
	};

	/// <summary>
	/// assimp でファイルを読み、中のアニメーションを全部 Animation に直す（エンジンの向き・秒に直す）
	/// </summary>
	/// <returns>読めなかったときは空</returns>
	static std::vector<Animation> LoadAnimationFile(const std::filesystem::path& filePath);

	// ハンドルからファイルを探す（無ければ nullptr）
	static const AnimationFile* Find(uint32_t handle);

	// シングルトン
	friend struct std::default_delete<AnimationManager>;
	~AnimationManager() = default;
	static std::unique_ptr<AnimationManager> instance_;

	std::unordered_map<uint32_t, AnimationFile> files_;      // ハンドル → 1ファイル分
	// パスの文字（ToPathKey で作る）→ ハンドル（同じファイルを2回読まない）
	std::unordered_map<std::string, uint32_t> pathToHandle_;
	uint32_t nextHandle_ = 1;                                // 次に配る番号（0 は失敗の印なので1から）
};
```

---

## 6. AnimationManager.cpp（全部）

新しく作る（`Graphics/Animation/AnimationManager.cpp`）。

```cpp
#include "AnimationManager.h"

#include <format>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/String/ConvertString.h"

// 静的メンバ変数
std::unique_ptr<AnimationManager> AnimationManager::instance_ = nullptr;

namespace {
// mTicksPerSecond が 0 のファイル（書き出したソフトが入れていない）のときに使う値。assimp の決まりで 25
constexpr double kDefaultTicksPerSecond = 25.0;
} // namespace


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void AnimationManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼び出されています");
	instance_ = std::make_unique<AnimationManager>(ConstructorKey());
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void AnimationManager::Release() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にRelease()が呼ばれています");
	instance_.reset();
	LogManager::Log("Released");
}


//=============================================================================
// 読み込み
//=============================================================================
uint32_t AnimationManager::Load(const std::filesystem::path& filePath) {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()を先に呼んでください");
	// --- 読み込み済みなら同じ番号 ---
	const std::string key = ToPathKey(filePath); // 書き方をそろえた文字（"a\b"・"a/./b" も "a/b"）
	auto cached = instance_->pathToHandle_.find(key);
	if (cached != instance_->pathToHandle_.end()) {
		return cached->second;
	}

	// --- ファイルから読む ---
	std::vector<Animation> animations = LoadAnimationFile(filePath);
	if (animations.empty()) {
		// 止めずに失敗の番号を返す（SoundManager と同じ。読めないファイルを選んでも落ちないように）
		return kInvalidHandle;
	}

	// --- 番号を配って登録 ---
	const uint32_t handle = instance_->nextHandle_++;
	AnimationFile& file = instance_->files_[handle];
	file.path = key; // GetPath で返す・保存に使うので、そろえた文字で持つ
	file.animations = std::move(animations);
	instance_->pathToHandle_[key] = handle;
	LogManager::Log(std::format("読み込み完了 \"{}\" -> handle={} アニメーション数={}", key, handle, file.animations.size()));
	return handle;
}


//=============================================================================
// assimp → Animation
//=============================================================================
std::vector<Animation> AnimationManager::LoadAnimationFile(const std::filesystem::path& filePath) {
	Assimp::Importer importer;
	// assimp には UTF-8 の文字で渡す（Windows の assimp は UTF-8 として読む。path::string() は Shift-JIS になるので使わない）
	const std::string utf8Path = ToUtf8(filePath);
	// アニメーションだけ使うので、頂点を直すオプション（Triangulate など）は要らない
	const aiScene* scene = importer.ReadFile(utf8Path.c_str(), 0);
	if (scene == nullptr) {
		LogManager::Error(std::format("読み込み失敗 \"{}\": {}", utf8Path, importer.GetErrorString()));
		return {};
	}
	if (scene->mNumAnimations == 0) {
		LogManager::Error(std::format("アニメーションが入っていません \"{}\"", utf8Path));
		return {};
	}

	std::vector<Animation> animations;
	animations.reserve(scene->mNumAnimations);
	for (uint32_t animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex) {
		const aiAnimation* source = scene->mAnimations[animationIndex];
		// assimp の時間は「tick」。秒 = tick ÷ (1秒あたりの tick 数)
		const double ticksPerSecond = (source->mTicksPerSecond != 0.0) ? source->mTicksPerSecond : kDefaultTicksPerSecond;

		Animation animation;
		animation.name = source->mName.C_Str();
		animation.duration = static_cast<float>(source->mDuration / ticksPerSecond);

		// --- チャンネル（ノード1つ分の動き）---
		for (uint32_t channelIndex = 0; channelIndex < source->mNumChannels; ++channelIndex) {
			const aiNodeAnim* channel = source->mChannels[channelIndex];
			// ノード名で引けるように入れる（ModelManager::Node::name と同じ名前）
			NodeAnimation& nodeAnimation = animation.nodeAnimations[channel->mNodeName.C_Str()];

			// 位置：ModelManager が頂点・行列の x を反転しているので、ここでも x を反転する
			nodeAnimation.translate.keyframes.reserve(channel->mNumPositionKeys);
			for (uint32_t keyIndex = 0; keyIndex < channel->mNumPositionKeys; ++keyIndex) {
				const aiVectorKey& key = channel->mPositionKeys[keyIndex];
				Keyframe<Vector3> keyframe;
				keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);
				keyframe.value = {-key.mValue.x, key.mValue.y, key.mValue.z};
				nodeAnimation.translate.keyframes.push_back(keyframe);
			}

			// 回転：x を反転した世界では、回転の向きが変わるので y と z を反転する（w と x はそのまま）
			nodeAnimation.rotate.keyframes.reserve(channel->mNumRotationKeys);
			for (uint32_t keyIndex = 0; keyIndex < channel->mNumRotationKeys; ++keyIndex) {
				const aiQuatKey& key = channel->mRotationKeys[keyIndex];
				Keyframe<Quaternion> keyframe;
				keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);
				keyframe.value = {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w};
				nodeAnimation.rotate.keyframes.push_back(keyframe);
			}

			// 大きさ：向きに関係ないのでそのまま
			nodeAnimation.scale.keyframes.reserve(channel->mNumScalingKeys);
			for (uint32_t keyIndex = 0; keyIndex < channel->mNumScalingKeys; ++keyIndex) {
				const aiVectorKey& key = channel->mScalingKeys[keyIndex];
				Keyframe<Vector3> keyframe;
				keyframe.time = static_cast<float>(key.mTime / ticksPerSecond);
				keyframe.value = {key.mValue.x, key.mValue.y, key.mValue.z};
				nodeAnimation.scale.keyframes.push_back(keyframe);
			}
		}
		animations.push_back(std::move(animation));
	}
	return animations;
}


//=============================================================================
// 取得
//=============================================================================
const AnimationManager::AnimationFile* AnimationManager::Find(uint32_t handle) {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()を先に呼んでください");
	auto it = instance_->files_.find(handle);
	if (it == instance_->files_.end()) {
		return nullptr;
	}
	return &it->second;
}

const Animation* AnimationManager::GetAnimation(uint32_t handle, uint32_t index) {
	const AnimationFile* file = Find(handle);
	if (file == nullptr || index >= file->animations.size()) {
		LogManager::Warning(std::format("アニメーションが見つかりません handle={} index={}", handle, index));
		return nullptr;
	}
	return &file->animations[index];
}

uint32_t AnimationManager::GetAnimationCount(uint32_t handle) {
	const AnimationFile* file = Find(handle);
	return file ? static_cast<uint32_t>(file->animations.size()) : 0;
}

const std::string& AnimationManager::GetPath(uint32_t handle) {
	const AnimationFile* file = Find(handle);
	if (file == nullptr) {
		static const std::string emptyString;
		return emptyString;
	}
	return file->path;
}
```

- `#include "AnimationManager.h"`：自分の .h はファイル名だけで書ける（MyEngine.props で `Graphics/Animation` も探す場所に入っている）。
- `namespace { ... }`（名前の無い namespace）：中の物はこの .cpp の中だけで使える（ほかのファイルの同じ名前とぶつからない）。

---

## 7. 登録と確認

### 登録

**MyEngine_v1.vcxproj**：ソリューションエクスプローラーで `Graphics/Animation` のフィルタに `AnimationManager.cpp` を追加する（手で書くなら `<ClCompile Include="MyEngine\Graphics\Animation\AnimationManager.cpp" />`）。

**Engine.cpp**：

```cpp
#include "MyEngine/Graphics/Animation/AnimationManager.h"
```

```cpp
	ModelManager::Initialize();
	AnimationManager::Initialize();
```

```cpp
	// --- 今まで Release を呼んでいなかった物（作りっぱなしで、終わるときに消していなかった）---
	AnimationManager::Release();
	IBLBaker::Finalize();
```

（CPU のデータだけで GPU の物を持たないので、消す順番はどこでもよい。シーンが消えた後に置いた）

### 確認

1. アニメ入りの glTF を `resources/` に置く（例：Khronos の glTF サンプルの `AnimatedCube`、学校の配布物）。
2. シーンの `Initialize` などで呼ぶ：

```cpp
uint32_t handle = AnimationManager::Load("resources/models/AnimatedCube/AnimatedCube.gltf");
uint32_t again = AnimationManager::Load("resources/models/AnimatedCube/AnimatedCube.gltf");
const Animation* animation = AnimationManager::GetAnimation(handle);
```

3. 見ること：
   - Log ウィンドウに `読み込み完了 ... handle=1 アニメーション数=1` が **1回だけ** 出る（2回目の Load は読み直さない）。
   - `handle` と `again` が同じ番号。
   - ブレークポイントで `animation` の中を見て、`duration`（秒）がそれらしい（AnimatedCube なら数秒）、`nodeAnimations` にノード名が入っていて、キーの `time` が 0〜`duration` の間に並んでいる。
   - わざと無いパスを Load すると、止まらずに 0 が返って、Log に `読み込み失敗` が出る。

次（時刻 t の値を取り出す補間・再生）は [Animation.md 6.6](Animation.md#66-次再生)。
