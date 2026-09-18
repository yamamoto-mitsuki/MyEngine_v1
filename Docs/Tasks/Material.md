# マテリアル系の作業

## このファイルについて
- 物の見た目の設定（色、UV、光沢、金属感、切り抜きなど）に関する作業を書く。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。「なぜ」は「決めたこと」の表へ移す。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階で行う。

---

## 予定

| Step | 作業 | 状態 |
|---|---|---|
| **1** | **`alphaCutoff`（切り抜き）を足す ＋ `ModelConfig::material` を実際に使う** | **反映済み**（現行コードと照合。実行確認済みの記録はLight.md参照） |
| 2 | **glTF（PBR）に対応する**：baseColor / metallic / roughness とそのテクスチャ、UVの向き、`.glb` の埋め込みテクスチャ | これから（PBRを使うならここが先） |
| 3 | マテリアルをアセットとして持つ（今は描画のたびに `Material3dData` を組み立てている） | これから（Inspectorの作業と一緒） |
| 4 | 法線マップを実際にシェーダーで使う | 将来（PBRの精度を上げるとき） |

### glTFについて（2026-09-18に調べたこと）
- `ModelManager::Load` は中で **assimp** を使っているので、**拡張子を `.gltf` / `.glb` にすればそのまま読める**（関数名やコメントがOBJ前提なだけ）。`AI_MATKEY_METALLIC_FACTOR` / `AI_MATKEY_ROUGHNESS_FACTOR` も既に読んでいる。
- ただし、glTFで正しく出すには次の3つが引っかかる。

| 引っかかる所 | 中身 | 直し方 |
|---|---|---|
| UVの向き | `aiProcess_FlipUVs` を常に付けている。OBJ（UVの原点が左下）には必要だが、glTF（原点が左上でDirectXと同じ）に付けるとテクスチャが上下逆になる | 拡張子を見て切り替える |
| テクスチャの種類 | 今読んでいるのは `aiTextureType_DIFFUSE` / `AMBIENT` / `SPECULAR` / `HEIGHT`。glTFは `BASE_COLOR` / `METALNESS` / `DIFFUSE_ROUGHNESS` / `NORMALS` に入る | 両方見て、あった方を使う |
| `.glb` の埋め込みテクスチャ | テクスチャがファイルではなくモデルの中に埋まっている（`aiScene::mTextures`）。今の `TextureManager::Load` はファイルパスしか受け取れない | メモリから読む経路を `TextureManager` に足す。`.gltf` ＋ テクスチャが別ファイルなら、今のままで動く |

- 名前も後で直す（`LoadObjFile` → `LoadModelFile`、`MtlMaterial` → `ModelMaterial` など）。このファイルで「MTL」と書いてある所は「モデルファイルのマテリアル（assimpが読んだもの）」の意味。

---

## 今のマテリアルの流れ（2026-09-18時点）

1. ゲーム側が `Renderer::ModelConfig` / `Rect3dConfig` などに色・UV・テクスチャを入れて描画を頼む
2. `Renderer` が `Material3dData`（GPUに送る形）を組み立てる
   - モデル：`MakeModelMaterial`。モデルファイルのマテリアル（assimpが読んだもの。OBJなら `.mtl`、glTFなら materials）に値があればそれを使う
   - Primitive（板・球など）：`MakeDefaultModelMaterial`。決まった値を入れる
3. `RenderContext::DrawMesh` が `FrameUploadBuffer` に書いて、`gMaterial`（`b20`）として結ぶ
4. 各ピクセルシェーダーが `gMaterial` を読む

- `Material3dData` は `MeshRequest` に入って描画リクエストと一緒に運ばれる。**マテリアルという「もの」はまだ無く、毎回作っている**。

---

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| 切り抜き（カットアウト）は、シェーダーを分けずにマテリアルの値（`alphaCutoff`）で切り替える（Step 1） | シェーダーとPSOの組み合わせが増えない。今のシェーダーもすでに `discard` を持っているので、増える負担も無い。Unityの Alpha Clipping と同じ考え方 |
| `alphaCutoff` が0なら「アルファがちょうど0のピクセルだけ捨てる」＝今までと同じ動き（Step 1） | 既にある描画の見た目を変えない。`if (texColor.a <= gMaterial.alphaCutoff)` と書けば、0のときは今までの `== 0.0` と同じになる |
| `Material3dData` の `padA`（`ambient` の後ろ）を `alphaCutoff` にする（Step 1） | 全部のシェーダーの `Material` 構造体が `ambient` の後ろに `padA` を持っているので、1行の書き換えで済む。構造体の大きさも変わらない |
| 使っていなかった `ModelConfig::material`（`MaterialParams`）を、MTLが無いときの既定値として使う（Step 1） | 宣言だけあって使われていなかった。ゲーム側から光沢や自己発光を指定できるようになる |
| マテリアルを「アセット」にするのはInspectorの作業と一緒にやる（Step 2） | 今は描画のたびに作っているので、Inspectorで編集する対象（実体）が無い。Entity / Inspectorの作りと合わせて決める |

---

## Step 1：`alphaCutoff`（切り抜き）を足す ＋ `ModelConfig::material` を使う

### 目的
1. マテリアルに `alphaCutoff` を足し、「アルファがこれ以下のピクセルを捨てる」（切り抜き）ができるようにする。
2. 使われていなかった `ModelConfig::material`（`MaterialParams`）を、MTLに値が無いときの既定値として使う。
3. Primitive（板・球など）の設定にも `alphaCutoff` を足す（アイコンなどに使えるようにする）。

### 切り抜きと半透明の違い（`Light.md` Step 5.1aの続き）
| | 半透明（今のアイコン） | 切り抜き（このStepでできるようになる） |
|---|---|---|
| 深度 | 書かない（`TestNoWrite`） | 書く（`TestWrite`） |
| 並べ替え | 必要（奥から手前へ） | 要らない |
| 縁 | なめらか | ギザギザ |
| 向いているもの | 数が少ないもの、ガラス、煙 | 木の葉、金網、フェンスなど数が多いもの |

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Graphics/Pipeline/RenderStates.h` | `MaterialParams` に `alphaCutoff` を追加 |
| ② | `MyEngine/Graphics/Pipeline/ShaderConstants.h` | `Material3dData` の `padA` を `alphaCutoff` に |
| ③ | `MyEngine/Shader/Data/Model/` の6つの `.PS.shader`（`Unlit` / `Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / `PBR`） | `Material` 構造体の `padA` を `alphaCutoff` に。`discard` の条件を変える |
| ④ | `MyEngine/Graphics/Renderer/Renderer.h` | Primitiveの6つのConfigに `alphaCutoff` を追加。`MakeModelMaterial` の宣言 |
| ⑤ | `MyEngine/Graphics/Renderer/Renderer.cpp` | `MakeDefaultModelMaterial` / `MakeModelMaterial` / `PushMesh` / `DrawModel` |

> 確認済み（2026-09-18）：エンジンの全67個の `.cpp` をDebug / Releaseでコンパイルして通る（変更したファイルの警告は0個）。6つのPSをDXCでコンパイルして通る（HLSL 2018 / 2021）。`alphaCutoff` が定数バッファの92バイト目（C++の `ambient` の直後）に来ていることも確認。

---

### ① `MyEngine/Graphics/Pipeline/RenderStates.h`

**追加する**：`MaterialParams` の最後（`roughness` の下）
```cpp
	float alphaCutoff = 0.0f;              // これ以下のアルファのピクセルを捨てる（切り抜き。0で今まで通り）
```

変更後の全体
```cpp
struct MaterialParams {
	Vector3 ambient = {0.2f, 0.2f, 0.2f};  // Ka: 環境光色
	Vector3 diffuse = {1.0f, 1.0f, 1.0f};  // Kd: 拡散反射色
	Vector3 specular = {0.0f, 0.0f, 0.0f}; // Ks: 鏡面反射色
	Vector3 emissive = {0.0f, 0.0f, 0.0f}; // Ke: 自己発光色
	float shininess = 32.0f;               // Ns: 鏡面反射指数
	float metallic = 0.0f;                 // 0=非金属 1=金属
	float roughness = 0.5f;                // 0=鏡面 1=完全拡散
	float alphaCutoff = 0.0f;              // これ以下のアルファのピクセルを捨てる（切り抜き。0で今まで通り）
};
```

### ② `MyEngine/Graphics/Pipeline/ShaderConstants.h`

**変更する**：`Material3dData` の `ambient` の下の `padA`
```cpp
	float alphaCutoff = 0.0f;  // これ以下のアルファのピクセルを捨てる（0なら「ちょうど0」だけ捨てる＝今まで通り）
```

変更後の全体
```cpp
struct Material3dData {
	Vector4 color;             // 色
	Matrix4x4 uvTransform;     // UV変換行列
	Vector3 ambient;           // Ka: 環境光色
	float alphaCutoff = 0.0f;  // これ以下のアルファのピクセルを捨てる（0なら「ちょうど0」だけ捨てる＝今まで通り）
	Vector3 diffuse;           // Kd: 拡散反射色
	float padB = 0.0f;
	Vector3 specular;          // Ks: 鏡面反射色
	float shininess;           // Ns: 鏡面反射指数
	Vector3 emissive;          // Ke: 自己発光色
	uint32_t textureIndex = 0; // 使用するテクスチャのインデックス（0はテクスチャなし）
	float metallic = 0.0f;     // 0=非金属 1=金属
	float roughness = 0.5f;    // 表面の粗さ（0=鏡面, 1=完全拡散）
	float padC = 0.0f;
	float padD = 0.0f;
};
```

### ③ 6つの `.PS.shader`（`Unlit` / `Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / `PBR`）

**変更する**：`Material` 構造体の `padA`（`ambient` の後ろ）

`Unlit` は1行に並んでいる
```hlsl
    float32_t3   ambient;    float alphaCutoff;
```
ほかの5つは別の行になっている
```hlsl
    float32_t3 ambient;
    float alphaCutoff;
```

**変更する**：テクスチャを読んだ後の `discard` の条件（6つとも）

変更前
```hlsl
    if (texColor.a == 0.0)
    {
        discard;
    }
```
変更後
```hlsl
    if (texColor.a <= gMaterial.alphaCutoff) // 切り抜き（alphaCutoffが0なら、ちょうど0のピクセルだけ捨てる）
    {
        discard;
    }
```
`Lambert` だけ1行で書いてある
```hlsl
    if (texColor.a <= gMaterial.alphaCutoff) { discard; } // 切り抜き（alphaCutoffが0なら、ちょうど0のピクセルだけ捨てる）
```

### ④ `MyEngine/Graphics/Renderer/Renderer.h`

**追加する**：`TriangleConfig` / `SphereConfig` / `Rect3dConfig` / `Quad3dConfig` / `AABBConfig` / `OBBConfig` の6つに1行ずつ（`ShadingType shadingType` の上）
```cpp
		float alphaCutoff = 0.0f;                                  // これ以下のアルファのピクセルを捨てる（切り抜き。0で今まで通り）
```
（`ModelConfig` には足さない。`ModelConfig` は `MaterialParams material;` を持っているので、そちらの `alphaCutoff` を使う）

**変更する**：`MakeModelMaterial` の宣言（`private:` の中）
```cpp
	static Material3dData MakeModelMaterial(const ModelManager::MtlMaterial* mat, uint32_t color, const Transform& uvTransform, const MaterialParams& params);
```

### ⑤ `MyEngine/Graphics/Renderer/Renderer.cpp`

**変更する**：`MakeDefaultModelMaterial`（Primitive用）
```cpp
// ===== Primitive Material作成ヘルパー =====
static Material3dData MakeDefaultModelMaterial(float r, float g, float b, float a, const Transform& uvTransform, float alphaCutoff) {
	Material3dData mat;
	mat.color = {r, g, b, a};
	mat.uvTransform = MakeUVTransformMatrix(uvTransform);
	mat.ambient = {0.0f, 0.0f, 0.0f};
	mat.diffuse = {1.0f, 1.0f, 1.0f};
	mat.specular = {0.0f, 0.0f, 0.0f};
	mat.shininess = 1.0f;
	mat.emissive = {0.0f, 0.0f, 0.0f};
	mat.alphaCutoff = alphaCutoff;
	return mat;
}
```

**変更する**：`MakeModelMaterial`（モデル用）
```cpp
// ===== Model Materialの共通作成部分 =====
Material3dData Renderer::MakeModelMaterial(const ModelManager::MtlMaterial* mat, uint32_t color, const Transform& uvTransform, const MaterialParams& params) {
	// 色変換（0xRRGGBBAA → float4）
	float r = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(color & 0xFF) / 255.0f;
	// マテリアル構築
	Material3dData material;
	material.color = {r, g, b, a};
	material.uvTransform = MakeUVTransformMatrix(uvTransform);
	// 値が設定している場合
	if (mat) {
		material.ambient = mat->ambient;
		material.diffuse = mat->diffuse;
		material.specular = mat->specular;
		material.shininess = mat->shininess;
		material.emissive = mat->emissive;
		material.color.w *= mat->dissolve;
		material.metallic = mat->metallic;
		material.roughness = mat->roughness;
	} else {
		// MTLに該当マテリアルが無いときは、ModelConfigのmaterial（MaterialParams）を使う
		material.ambient = params.ambient;     // Ka: Ambient   環境光（影になっている部分の明るさ）
		material.diffuse = params.diffuse;     // Kd: Diffuse   拡散反射（物体本来の色）
		material.specular = params.specular;   // Ks: Specular  鏡面反射（ハイライトの強さ）
		material.shininess = params.shininess; // Ns: Shininess 光沢（値が大きいほどハイライトが小さく鋭くなる）
		material.emissive = params.emissive;   // Ke: Emissive  自己発光（光源がなくても発光する色）
		material.metallic = params.metallic;
		material.roughness = params.roughness;
	}
	// 切り抜きはMTLに無い設定なので、いつもMaterialParamsから取る
	material.alphaCutoff = params.alphaCutoff;
	return material;
}
```

**変更する**：`PushMesh` の中
```cpp
	req.materialData = MakeDefaultModelMaterial(r, g, b, a, config.uvTransform, config.alphaCutoff);
```

**変更する**：`DrawModel` の中（使っていない色の分解を消して、`config.material` を渡す）

変更前
```cpp
	// 色変換
	float r = static_cast<float>((config.color >> 24) & 0xFF) / 255.0f;
	float g = static_cast<float>((config.color >> 16) & 0xFF) / 255.0f;
	float b = static_cast<float>((config.color >> 8) & 0xFF) / 255.0f;
	float a = static_cast<float>(config.color & 0xFF) / 255.0f;
	// 行列
```
変更後
```cpp
	// 行列（色の分解は MakeModelMaterial の中でやるので、ここでは要らない）
```

変更前
```cpp
		req.materialData = MakeModelMaterial(mat, config.color, config.uvTransform);
```
変更後
```cpp
		req.materialData = MakeModelMaterial(mat, config.color, config.uvTransform, config.material);
```

---

### 解説

**なぜ `padA` を `alphaCutoff` にしたのか**
- HLSLの定数バッファは16バイトの行単位で詰められる。`float32_t3 ambient;` の後ろの4バイトは、行を埋めるためのpaddingで、誰も使っていなかった。
- ここを使えば、**構造体の大きさが変わらない**（＝ほかの値の位置がずれない）。新しい値を後ろに足すと、シェーダーごとに宣言している `Material` 構造体の長さがまちまちなので、全部に padding を足す必要が出てしまう。
- 「paddingを消して意味のある値にする」のは、GPU用構造体でよく使う手。

**なぜ `<=` で比べるのか**
- `alphaCutoff` の既定値は0。`texColor.a <= 0.0` は「アルファがちょうど0のとき」だけ真になるので、**今までの `== 0.0` と同じ動き**になる。
- 切り抜きを使いたいときは `alphaCutoff = 0.5f` のように入れる。アルファが0.5以下のピクセルが消える。

**`MaterialParams` の使い方（MTLとの優先順位）**
- OBJの `.mtl` に値がある → MTLを使う（今まで通り）
- MTLが無い / そのマテリアル名が無い → `ModelConfig::material` を使う（今までは決まった値だった）
- `alphaCutoff` はMTLに無い設定なので、**いつも `ModelConfig::material` から取る**
- 将来マテリアルをアセットにするとき（Step 2）、この優先順位を「アセットの値 → 描画時の上書き」に整理する。

**Primitiveの6つのConfigに1行ずつ足す理由**
- `PushMesh` はテンプレートで、どのConfigでも同じコードが動く。`config.alphaCutoff` を読むには、渡されるConfig全部にその名前が必要。
- `ModelConfig` は `PushMesh` を通らない（`DrawModel` が自分で `MeshRequest` を作る）ので、足さなくてよい。

**アイコンを切り抜きにしたい場合（任意）**
- `LightGizmo::DrawIcon` で次のようにすると、切り抜きになる。
```cpp
	icon.depthMode = DepthMode::TestWrite; // 不透明として描く（深度を書く）
	icon.alphaCutoff = 0.5f;               // アルファ0.5以下を捨てる
```
- 縁がギザギザになるが、並べ替えが要らなくなる。今は半透明のままにしてある（`Light.md` の「決めたこと」）。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 見た目が前と同じ（`alphaCutoff` を設定していないので、今までと同じ動きになるはず）
   - モデルの `ModelConfig::material` の `emissive` や `shininess` を変えると、MTLが無いモデルで見た目が変わる
   - 試しに `LightGizmo::DrawIcon` に上の2行を入れると、アイコンの縁がギザギザになり、深度が書かれる（確認したら戻す）

### 次のStepでやること（ここではやらない）
- Step 2：マテリアルをアセットとして持つ（Inspectorで編集できるようにする）。`Editor.md` の作業と一緒に決める。
