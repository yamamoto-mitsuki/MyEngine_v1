# PathLoad：読み込みのパスを std::filesystem::path にする

## このファイルについて

- 2026-10-14 の依頼：レビューの指摘「`ModelManager.cpp` で `directoryPath + "/" + filename` と文字の `+` でつないでいる。`directoryPath` が空だと先頭に `/` が付いて読めない。`std::filesystem::path` の演算子でつなぐのを検討して」を受けて、「**読み込み系で string のやつを全部変更しよう。ConvertString も変更箇所に**」。
- 先に結論：
  - **4つの Manager（Model・Texture・Sound・Animation）の `Load` を `const std::filesystem::path&` で受け取る**ようにした。パスをつなぐ所は path の `/` にした（指摘の所）。
  - **ConvertString に関数を2つ足す**：`ToPath`（UTF-8 の文字 → path）と `ToPathKey`（path → キャッシュのキー）。
  - シーンファイル・Inspector から来るパス（UTF-8 の文字）は、**`LoadAsset` で1回だけ `ToPath` を通して** Manager に渡す。
  - `GetModelPath`・`GetTexturePath`・`GetSoundPath`・`GetPath` は **今まで通り `std::string`（UTF-8 の文字）を返す**（シーンファイルに保存する・Inspector に出す文字なので）。
- このコードは worktree で全部書き換えて、エンジンを Debug・Development・Release でビルドし 0エラーを確かめた。`ToPath`・`ToPathKey` は小さいテストで動きを確かめた（[1章の表](#確かめたこと)）。**ゲームの実行はまだ**（[7章](#7-確認すること)）。
- ゲーム（AL4_01）側の直しは **要らない**（`Load` を直接呼んでいる所が無い）。

| 章 | 中身 |
|---|---|
| [1. 何が困っていたか・どう直すか](#1-何が困っていたかどう直すか) | 指摘の中身と、日本語のパスの問題 |
| [2. ConvertString](#2-convertstringh--cpp) | `ToPath`・`ToPathKey` を足す |
| [3. TextureManager](#3-texturemanager) | |
| [4. SoundManager](#4-soundmanager) | |
| [5. ModelManager](#5-modelmanager指摘の所) | 指摘の所 |
| [6. LoadAsset・ComponentSerializer](#6-loadassetcomponentserializer) | UTF-8 の文字 → path の境目 |
| [6.5 AnimationManager](#65-animationmanager) | AnimationManager.md の方を直した |
| [7. 確認すること](#7-確認すること) | |

**写す順番**：2 → 3 → 4 → 5 → 6（2 が無いとほかがビルドできない。6 の `LoadAsset` が3つの Manager を呼ぶので、6 は最後）。

---

## 1. 何が困っていたか・どう直すか

### 指摘の中身（つなぎ方）

```cpp
std::string filePath = directoryPath + "/" + filename;
```

| 渡したパス | フォルダ（parent_path） | つないだ結果 | 結果 |
|---|---|---|---|
| `"resources/cube/cube.obj"` | `"resources/cube"` | `"resources/cube/cube.obj"` | 読める |
| `"cube.obj"` | `""`（空） | **`"/cube.obj"`** | **ドライブの一番上（`C:\cube.obj`）を探して失敗** |

テクスチャの4か所（`directoryPath + "/" + texPath.C_Str()`）も同じ。path の `/` なら、空のフォルダには何も足さない・`/` が2つにならない：

```cpp
std::filesystem::path("")               / "cube.obj"  // → "cube.obj"
std::filesystem::path("resources/cube") / "cube.obj"  // → "resources/cube/cube.obj"
std::filesystem::path("resources/cube/")/ "cube.obj"  // → "resources/cube/cube.obj"
```

### もう1つの問題（日本語のパス）

エンジンの文字（ImGui・シーンファイル・ログ）は **UTF-8** で持っている。ところが

```cpp
std::filesystem::path path(utf8の文字); // ← std::string から path を作る
```

と書くと、Windows では **Shift-JIS（日本語の Windows の文字の決まり）として読む**。英数字だけなら同じなので気づかないが、日本語が入ると **化けるか、例外が出て落ちる**（テストで確かめたら例外 `No mapping for the Unicode character exists in the target multi-byte code page.` になった）。

今の ModelManager の `std::filesystem::path path(objFilePath);` がちょうどこれ。

### どう直すか（決まり）

| 場面 | 書き方 |
|---|---|
| Manager の `Load` の引数 | `const std::filesystem::path&` |
| コードに直接書いた英数字のパス | 今まで通り `Load("resources/cube/cube.obj")` でよい（文字から path が自動で作られる） |
| UTF-8 の文字（シーンファイル・Inspector・assimp が返した文字）を path にする | **`ToPath(文字)`** |
| path を UTF-8 の文字にする（ログ・保存・assimp に渡す） | **`ToUtf8(path)`**（前からある） |
| キャッシュのキー（同じファイルか見分ける） | **`ToPathKey(path)`**（`"a\b"`・`"a/./b"` も `"a/b"` にそろう） |
| DirectXTex・Media Foundation・`SetName` に渡す | **`path.c_str()`**（Windows の path の中身は `wchar_t` の文字なので、そのまま渡せる。`ConvertString` が要らなくなる） |
| パスをつなぐ | **path の `/`** |

### 確かめたこと

`ToPath` → `ToPathKey`、`parent_path() / ToPath("tex.png")` の結果：

| 渡した文字 | キー（ToPathKey） | フォルダ / "tex.png" |
|---|---|---|
| `resources/cube/cube.obj` | `resources/cube/cube.obj` | `resources/cube/tex.png` |
| `resources\cube\cube.obj` | `resources/cube/cube.obj` | `resources/cube/tex.png` |
| `resources/./cube/../cube/cube.obj` | `resources/cube/cube.obj` | （そろえる前のフォルダにつながる。読めるので問題ない） |
| `cube.obj` | `cube.obj` | **`tex.png`**（先頭に `/` が付かない） |
| `resources/モデル/箱.obj` | `resources/モデル/箱.obj` | `resources/モデル/tex.png` |
| （比べる用）UTF-8 の `resources/モデル/箱.obj` を `path(std::string)` で直接 | — | **例外で落ちる** |

---

## 2. ConvertString.h / .cpp

### ConvertString.h（`ToUtf8` の下に足す）

```cpp
// UTF-8 の文字 → パス。ImGui・シーンファイル・ログの文字（UTF-8）を path にするときはこれを使う
// std::filesystem::path(utf8の文字) と直接書くと、日本語の Windows では Shift-JIS として読まれて、化けるか例外が出て落ちる
std::filesystem::path ToPath(const std::string& utf8);
// パス → 読み込みのキャッシュのキー（書き方をそろえた UTF-8 の文字）
// "a\b"・"a/./b"・"a/c/../b" も全部 "a/b" になるので、書き方が違っても同じファイルを2回読まない
std::string ToPathKey(const std::filesystem::path& path);
```

### ConvertString.cpp（`ToUtf8` の下に足す）

```cpp
std::filesystem::path ToPath(const std::string& utf8) {
	// UTF-8 → wstring（UTF-16）にしてから path にする。wstring から作る path は、文字の読み方を間違えない
	return std::filesystem::path(ConvertString(utf8));
}

std::string ToPathKey(const std::filesystem::path& path) {
	// lexically_normal：「.」「..」を消して一番短い形にする（ファイルを見に行かず、文字だけで直すので、無いファイルでも動く）
	// ToUtf8：区切りを / にそろえた UTF-8 の文字にする
	return ToUtf8(path.lexically_normal());
}
```

- `lexically_normal()`（あまり使わない STL なので解説）：`std::filesystem::path` の関数。`"a/./b"` → `"a/b"`、`"a/c/../b"` → `"a/b"` のように、**文字だけを見て** 一番短い形にする。似た物に `std::filesystem::canonical` があるが、こちらは実際のファイルを見に行き、ファイルが無いと例外になるので使わない。
- `ToPathKey` を ConvertString に置くのは、4つの Manager が同じキーの作り方を使うため（Manager ごとに書くとコピペになる）。

---

## 3. TextureManager

### TextureManager.h

`#include <string>` の下に `#include <filesystem>`。

```cpp
	/// <param name="filePath">読み込む画像ファイルのパス（UTF-8 の文字から作るときは ToPath を通す）</param>
	/// <returns>登録したテクスチャのSRVスロット番号</returns>
	static uint32_t Load(const std::filesystem::path& filePath);
```

```cpp
	static DirectX::ScratchImage LoadTextureFromFile(const std::filesystem::path& filePath);
```

```cpp
	// 画像のファイルパス（ToPathKey でそろえた UTF-8 の文字）をキー、TextureDataを値とするマップ
	std::unordered_map<std::string, TextureData> textures_;
```

### TextureManager.cpp：Load

```cpp
uint32_t TextureManager::Load(const std::filesystem::path& filePath) {
	MY_ASSERT_MSG(instance_, "Initializeを先に呼んでください");
	// すでに読み込まれているか確認（書き方をそろえたキーで探す）
	const std::string key = ToPathKey(filePath);
	auto it = instance_->textures_.find(key);
	...
	resource->SetName(filePath.c_str()); // 名前をつける（Windows の path の中身は wchar_t の文字なので、そのまま渡せる）
	...
	instance_->textures_[key] = std::move(textureData);

	LogManager::Log(std::format("Loaded: {} -> SRVSlot {}", key, index));
	LogManager::Log(std::format("filePath: {} format: {}", key, (uint32_t)metadata.format));
	return index;
}
```

（`filePath` を使っていた所のうち、**キャッシュとログは `key`**、**ファイルを読む所と SetName は `filePath`**。`...` の所はそのまま）

### TextureManager.cpp：LoadTextureFromFile

```cpp
DirectX::ScratchImage TextureManager::LoadTextureFromFile(const std::filesystem::path& filePath) {
	// テクスチャファイルを読み込む（DirectXTex は wchar_t の文字を受け取るので、path::c_str() をそのまま渡す）
	DirectX::ScratchImage image{};
	HRESULT hr{};

	// HDR画像
	if (filePath.extension() == ".hdr") {
		hr = DirectX::LoadFromHDRFile(filePath.c_str(), nullptr, image);
		...
	}

	// 通常テクスチャ（従来通り：sRGB強制＋mip生成）
	hr = DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

	if (FAILED(hr)) {
		LogManager::Error(std::format("Failed to load texture from file: {}", ToUtf8(filePath)));
		...
```

- `std::wstring wFilePath = ConvertString(filePath);` の行は消す（`filePath.c_str()` がもう wchar_t の文字）。
- `filePath.ends_with(".hdr")` → `filePath.extension() == ".hdr"`（path には `ends_with` が無い。`extension()` は最後の `.` から後ろ）。
- `GetTexturePath` はそのまま（`textures_` のキー＝`ToPathKey` の文字を返す）。

---

## 4. SoundManager

### SoundManager.h

`#include <string>` の下に `#include <filesystem>`。

```cpp
	/// <param name="filePath">ファイルパス（例: "resources/bgm.mp3"）。UTF-8 の文字から作るときは ToPath を通す</param>
	/// <returns>読み込んだ音声の素材ハンドル</returns>
	static uint32_t Load(const std::filesystem::path& filePath);
```

```cpp
	uint32_t LoadInternal(const std::filesystem::path& filePath);
```

### SoundManager.cpp

```cpp
uint32_t SoundManager::Load(const std::filesystem::path& filePath) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->LoadInternal(filePath);
}
```

```cpp
uint32_t SoundManager::LoadInternal(const std::filesystem::path& filePath) {
	// 書き方をそろえたパスの文字（キャッシュのキー・ログ・GetSoundPath で返す文字）
	const std::string filename = ToPathKey(filePath);
	LogManager::Log(std::format("ファイル読み込み要求 filename=\"{}\"", filename));
	...（キャッシュチェックはそのまま。filename を使う）
	HRESULT hr;
	// （「UTF-8 のパスを Windows の文字列に直す」のコメントと wFilename の2行を消す）
	// SourceReaderでファイルを開く
	// Media Foundation の物は COM なので ComPtr で持つ。関数を抜けると（途中の return でも）自動で Release される
	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &reader);
	...（ここから下はそのまま。filename を使っている所はキーの文字なので変えなくてよい）
```

- 関数の頭で `filename`（キーの文字）を作ってしまうので、下のログ・`pathID_` の所は1文字も変えずに済む。

---

## 5. ModelManager（指摘の所）

### ModelManager.h

`#include <vector>` の下に `#include <filesystem>`。

```cpp
	/// <param name="filePath">モデルファイルのパス（例: "resources/cube/cube.obj"）。UTF-8 の文字から作るときは ToPath を通す</param>
	/// <returns>描画時に使うモデルハンドル</returns>
	static uint32_t Load(const std::filesystem::path& filePath);
```

```cpp
	static ModelAsset LoadObjFile(const std::filesystem::path& filePath);
```

- `LoadObjFile` は「フォルダ」と「ファイル名」に分けて渡すのをやめて、**パスを丸ごと渡す**（分けるのは中でやる。分けてつなぎ直していたから、指摘の問題が起きていた）。

### ModelManager.cpp

include に `#include "MyEngine/String/ConvertString.h"` を足す（`TextureManager.h` の下）。

**Load**：

```cpp
uint32_t ModelManager::Load(const std::filesystem::path& filePath) {
	auto& inst = GetInstance();
	// 重複チェック（書き方をそろえたキーで探す）
	const std::string key = ToPathKey(filePath);
	auto cached = inst.pathToHandle_.find(key);
	if (cached != inst.pathToHandle_.end()) {
		return cached->second;
	}

	// ファイル名（ログ・バッファの名前用。UTF-8 の文字）
	const std::string filename = ToUtf8(filePath.filename());
	// 読み込み
	ModelAsset ModelAsset = LoadObjFile(filePath);
	ModelAsset.name = ToUtf8(filePath.stem());
	// マテリアルのテクスチャをTextureManagerでロード（パスは UTF-8 の文字で持っているので ToPath を通す）
	for (auto& [name, mat] : ModelAsset.materialMap) {
		if (!mat.textureFilePath.empty()) {
			mat.srvIndex = TextureManager::Load(ToPath(mat.textureFilePath));
		}
	}
	...（MakeMeshBuffer・UploadContext::Flush はそのまま）
	uint32_t handle = inst.modelsKey_++;
	inst.models_[handle] = std::move(ModelAsset);
	inst.pathToHandle_[key] = handle;

	return handle;
}
```

- 消す行：`std::filesystem::path path(objFilePath);`（1章の日本語の問題）と、`directoryPath`・`filename` を `.string()` で作っていた行。
- `stem()`：拡張子を除いたファイル名（`cube.obj` → `cube`）。

**LoadObjFile の頭**：

```cpp
ModelManager::ModelAsset ModelManager::LoadObjFile(const std::filesystem::path& filePath) {
	Assimp::Importer importer;
	const std::filesystem::path directoryPath = filePath.parent_path(); // テクスチャのパスをつなぐ先（モデルのフォルダ）
	const std::string filename = ToUtf8(filePath.filename());           // ログ用
	// テクスチャのパス（モデルファイルからの相対）を、モデルのフォルダにつなぐ
	// path の / でつなぐと、フォルダが空（"cube.obj" だけ渡した）でも先頭に / が付かない。assimp の文字は UTF-8 なので ToPath を通す
	auto makeTexturePath = [&directoryPath](const aiString& texPath) { return ToUtf8(directoryPath / ToPath(texPath.C_Str())); };
	// --- 読み込み ---
	// assimp には UTF-8 の文字で渡す（path::string() は日本語の Windows では Shift-JIS になって読めない）
	const aiScene* scene = importer.ReadFile(
	    ToUtf8(filePath).c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
```

- `std::string filePath = directoryPath + "/" + filename;` の行は消す（**指摘の所**）。
- `filename` は、下の `LogManager::Warning(filename + ": 法線がない...")` で使っているので作っておく。

**テクスチャの4か所**：

```cpp
		// テクスチャ（パスはモデルファイルからの相対なので、makeTexturePath でモデルのフォルダにつなぐ）
		aiString texPath;
		// glTFはbaseColor、OBJ / FBXはdiffuseに入る。あった方を使う
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS || material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
			mtl.textureFilePath = makeTexturePath(texPath);
		}
		if (material->GetTexture(aiTextureType_AMBIENT, 0, &texPath) == AI_SUCCESS) {
			mtl.ambientTexFilePath = makeTexturePath(texPath);
		}
		if (material->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
			mtl.specularTexFilePath = makeTexturePath(texPath);
		}
		// OBJのmap_bump/bumpはassimpではHEIGHT、glTFはNORMALSに分類される
		if (material->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS || material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
			mtl.bumpTexFilePath = makeTexturePath(texPath);
		}
```

- `makeTexturePath`（ラムダ式）：関数の中で作る小さな関数。同じ「フォルダにつなぐ」を4回書かずに済む（コピペを減らす）。`[&directoryPath]` は「外の `directoryPath` を中で使う」という意味。
- `MtlMaterial::textureFilePath` などは `std::string`（UTF-8 の文字）のまま。TextureManager に渡すときに `ToPath` を通す（上の Load）。

---

## 6. LoadAsset・ComponentSerializer

シーンファイル・Inspector から来るパスは UTF-8 の文字。**ここが「文字 → path」の境目**なので、`LoadAsset` の引数は `std::string` のまま、中で1回だけ `ToPath` を通す。

### ComponentUI.h（コメントだけ）

```cpp
// パスのファイルを読み込んで番号をもらう（空・ファイルが無いなら0＝未選択）。path は UTF-8 の文字（シーンファイル・Inspector の文字）。中で ToPath を通して各 Manager に渡す
uint32_t LoadAsset(AssetType type, const std::string& path);
```

### ComponentUI.cpp

```cpp
uint32_t LoadAsset(AssetType type, const std::string& path) {
	if (path.empty()) {
		return 0; // 未選択
	}
	// シーンファイル・Inspector から来るパスは UTF-8 の文字なので、ここで1回だけ path に直す（日本語のパスでも化けないように）
	const std::filesystem::path filePath = ToPath(path);
	// 無いファイルを読ませるとManagerの中で止まるので、先に確かめる
	std::error_code error;
	if (!std::filesystem::exists(filePath, error)) {
		LogManager::Warning(std::format("アセットが見つかりません（未選択として扱います）: {}", path));
		return 0;
	}
	switch (type) {
	case AssetType::Model:
		return ModelManager::Load(filePath);
	case AssetType::Texture:
		return TextureManager::Load(filePath);
	case AssetType::Sound:
		return SoundManager::Load(filePath); // 読めない形式なら0（SoundManager がログに出す）
	}
	return 0;
}
```

（`ConvertString.h` はもう include してある）

### ComponentSerializer.cpp

include に `#include "MyEngine/String/ConvertString.h"`（`LogManager.h` の下）。

```cpp
		if (!path.empty() && !std::filesystem::exists(ToPath(path), error)) {
```

（シーンファイルから読んだ UTF-8 の文字を、`exists` にそのまま渡していた。1章の問題）

### 変えなくてよい所

| 所 | 理由 |
|---|---|
| `TextureManager::Load("MyEngine/Resources/...")`（LightGizmo・EditorGrid・Skybox・DefaultScene） | 英数字だけのパスを直接書いているので、文字から path が自動で作られて問題ない |
| `AssetIcons.cpp` の `TextureManager::Load(path)` | フォルダ名と enum の名前から作った英数字だけのパス |
| `SceneSerializer::LoadFile` / `SaveFile` | シーンの名前は「英数字と _ - だけ」と決めてある（SceneManager が確かめている）ので、日本語が入らない |

---

## 6.5 AnimationManager

[AnimationManager.md](AnimationManager.md) の方を、ほかの Manager と同じ形に直した：

- 自分で持っていた `MakeKey` をやめて、共通の `ToPathKey` を使う（同じことを2か所に書かない）。
- `GetPath` は `std::string`（`ToPathKey` の文字）を返す（ほかの Manager の `GetXxxPath` とそろえた。保存にそのまま使える）。
- `Load` の引数は `const std::filesystem::path&` のまま。

---

## 7. 確認すること

1. エンジン・ゲームがビルドできる。
2. 今までのシーンを開いて、モデル・テクスチャ・音が今まで通り出る（シーンファイルのパスは `/` 区切りなので、キーは今までと同じ文字になる）。
3. Inspector でモデル・テクスチャ・音を選び直せる。Save → 開き直しても同じ物が出る。
4. （できれば）`resources/テスト/` のような日本語のフォルダに画像を置いて、Inspector から選ぶ → 表示される。今までは、ここで読めない・落ちることがあった。
5. （できれば）モデルを `"cube.obj"` のようにフォルダ無しのパスで Load しても読める（指摘の所）。
