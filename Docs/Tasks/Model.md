# モデル読み込みの作業（assimp / glTF / ノード階層）

## このファイルについて
- モデルファイルの読み込み（assimp）と、その中の階層構造・アニメーションに関する作業を書く。
- マテリアルの中身（色・PBRの値・切り抜き）は `Material.md`、Entityの階層は `Editor.md`。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。「なぜ」は「決めたこと」の表へ移す。

---

## 予定

| Step | 作業 | 状態 |
|---|---|---|
| **1** | **ノード階層（`aiNode`）を読んで描画に反映する ＋ glTFを読む準備（UVの向き、baseColor）** | 実行確認OK（学校の課題もこれ） |
| **1a** | **glTFのテクスチャが上下逆になるのを直す（`aiProcess_FlipUVs` を全形式に付ける）** | **コード反映済み**（936cdcfで全形式FlipUVsを確認。実行確認は要確認） |
| 2 | モデルのノード階層を、Entityの階層として取り込む（`Editor.md` Step 3のRenderComponentの後） | これから |
| 3 | `.glb` の埋め込みテクスチャに対応する（`TextureManager` にメモリから読む経路を足す） | これから |
| 4 | スキンアニメーション（ボーン、`aiAnimation`） | 将来 |

---

## 今の読み込みの流れ（2026-09-18時点）

1. `ModelManager::Load(パス)` → `LoadObjFile`（名前はOBJだが中身はassimpなので何でも読める）
2. assimpの `aiScene` から
   - マテリアル（`aiMaterial`）→ `MtlMaterial`（名前で引ける表）
   - メッシュ（`aiMesh`）→ `SubMesh`（1メッシュ＝1マテリアル。頂点とインデックス）
   - **ノード（`aiNode`）→ Step 1で追加**
3. `MakeMeshBuffer` で頂点・インデックスをGPU専用メモリ（DEFAULTヒープ）へ転送（`UploadContext`）
4. 描画は `Renderer::DrawModel` が、メッシュごとに `MeshRequest` を積む

- 頂点は読み込み時に **xを反転**している（assimpは右手系、このエンジンは左手系）。`aiProcess_FlipWindingOrder` とセットで三角形の裏表を合わせている。

---

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| ノードの木は「配列 ＋ インデックス」で持つ（`std::vector<Node>`、0番が根）（Step 1） | ポインタで木を作ると、コピーや保存のときに困る。配列なら順番に全部回せて、ARCHITECTURE.md 原則3（まとめて処理）とも相性が良い |
| ノードの行列は読み込み時に「根からの行列」まで計算して持つ（Step 1） | アニメーションが無いうちは毎フレーム計算する必要がない。描画時は `ノードの行列 × 置いた場所の行列` を渡すだけで済む |
| 頂点にノードの行列を焼き込まない（Step 1） | 同じメッシュを複数のノードが使っている場合（インスタンス）に壊れる。将来アニメーションでノードが動くようになったときも、焼き込みだと作り直しになる |
| assimpの行列は「転置」＋「x反転」で変換する（Step 1） | assimpは列ベクトル・行優先、このエンジンは行ベクトルなので転置が必要。さらに頂点をx反転して読んでいるので、行列も同じ空間に直さないと位置がずれる |
| UVの上下反転（`aiProcess_FlipUVs`）は**形式に関係なく全部に付ける**（Step 1a で修正） | glTFの「ファイル」はUVの原点が左上（DirectXと同じ）なので一度は外したが、**assimpのglTF2インポーターが読み込む時点で `1-v` して左下原点に揃えてしまう**。assimpから出てくるUVはOBJでもglTFでも左下原点なので、DirectXに合わせる反転はどの形式でも必要。実測で確認済み（Step 1a に測定結果） |
| テクスチャは `DIFFUSE` と `BASE_COLOR` の両方を見る（Step 1） | glTFのbaseColorは `aiTextureType_BASE_COLOR` に入る。OBJ / FBXは `DIFFUSE`。あった方を使えば両方読める |

---

## Step 1：ノード階層を読む ＋ glTFを読む準備

### 目的
1. `aiNode` の木を `ModelAsset::nodes`（配列）に写し、**ノードの行列を描画に反映する**。
   - 今はノードを見ていないので、「モデルファイルの中で子オブジェクトをずらして置いてある」形のモデルが、全部原点に重なって出てしまう。
2. glTFを読んだときに、テクスチャが上下逆にならないようにする（→ **Step 1a で結論が変わった**。`aiProcess_FlipUVs` は全形式に付ける）。
3. glTFのbaseColorテクスチャを読めるようにする。

### ノードとメッシュの関係
```
aiScene
 ├ mMeshes[]     … 形（頂点・インデックス）。どこに置くかは持っていない
 └ mRootNode     … 階層。各ノードが「行列」と「使うメッシュの番号」を持つ
     ├ "Body"   行列, mMeshes = { 0 }
     ├ "Arm"    行列, mMeshes = { 1 }
     │   └ "Hand"  行列, mMeshes = { 2 }   ← Armの行列も掛かる
     └ "Wheel"  行列, mMeshes = { 3, 4 }
```
- **メッシュは「形」だけ**で、位置はノードが持つ。だからノードを見ないと、モデルファイル通りの位置に出ない。
- 1つのメッシュを複数のノードが使うこともある（同じ形を別の場所に置く＝インスタンス）。

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Graphics/Model/ModelManager.h` | `Node` 構造体、`ModelAsset::nodes`、`LoadNode` の宣言 |
| ② | `MyEngine/Graphics/Model/ModelManager.cpp` | 行列の変換、`LoadNode`、読み込みフラグ、テクスチャの種類 |
| ③ | `MyEngine/Graphics/Renderer/Renderer.cpp` | `DrawModel` をノード単位で回す |

> 確認済み（2026-09-18）：エンジンの全69個の `.cpp` をDebug / Releaseでコンパイルして通る（`/W4`。変更したファイルの警告は0個）。

---

### ① `MyEngine/Graphics/Model/ModelManager.h`

**追加する**：`class ModelManager {` の `public:` のすぐ下
```cpp
	static constexpr uint32_t kInvalidNode = 0xFFFFFFFF; // 親がいないノード（根）を表す
```

**差し替える**：`ModelAsset` の定義（`Node` を足して、`nodes` を持たせる）
```cpp
	// ===== ノード（モデルファイルの中の階層構造。glTF / FBX / OBJ 共通） =====
	// ポインタで木を作らず、配列とインデックスで持つ（0番が根）
	struct Node {
		std::string name;                       // ノード名（Blenderなどで付けた名前）
		Matrix4x4 localMatrix = MakeIdentity4x4(); // 親から見た行列（エンジンの向きに直したもの）
		Matrix4x4 worldMatrix = MakeIdentity4x4(); // 根からたどって掛け合わせた行列（読み込み時に計算）
		uint32_t parent = kInvalidNode;         // 親のインデックス。根は kInvalidNode
		std::vector<uint32_t> children;         // 子のインデックス
		std::vector<uint32_t> meshIndices;      // このノードが持つメッシュ（ModelAsset::meshes の番号）
	};

	// ===== モデルデータ（複数メッシュ・マテリアル・ノード）=====
	struct ModelAsset {
		std::string name;
		std::vector<SubMesh> meshes;
		std::vector<Node> nodes;                     // 階層構造。0番が根
		std::map<std::string, MtlMaterial> materialMap;
	};
```

**追加する**：`private:` の `LoadObjFile` の宣言の下
```cpp
	/// <summary>
	/// aiNodeの木を ModelAsset::nodes（配列）へ写す。子を再帰でたどる
	/// </summary>
	/// <returns>追加したノードのインデックス</returns>
	static uint32_t LoadNode(const struct aiNode* node, uint32_t parentIndex, const Matrix4x4& parentWorld, ModelAsset& modelAsset);
```

### ② `MyEngine/Graphics/Model/ModelManager.cpp`

**追加する**：includeに2つ ← **Step 1a で不要になった**（拡張子を見るのをやめたので、足していたら消してよい）
```cpp
#include <algorithm>
#include <cctype>
```

**追加する**：`// ===== インスタンス取得 =====` の上
```cpp
//======================================================================================================
// assimpの行列 → エンジンの行列
// ・assimpは「列ベクトル（変換後 = 行列 × 頂点）」で行優先。エンジンは「行ベクトル（変換後 = 頂点 × 行列）」なので転置する
// ・頂点を読むときにxを反転している（右手系→左手系）ので、行列も x を反転した空間へ直す
//   （x反転の行列で前後から挟む形。結果として「片方の添字だけがx」の成分の符号が反転する）
//======================================================================================================
```
```cpp
static Matrix4x4 ConvertAssimpMatrix(const aiMatrix4x4& matrix) {
	Matrix4x4 result{};
	// 転置しながら入れる
	result.m[0][0] = matrix.a1; result.m[0][1] = matrix.b1; result.m[0][2] = matrix.c1; result.m[0][3] = matrix.d1;
	result.m[1][0] = matrix.a2; result.m[1][1] = matrix.b2; result.m[1][2] = matrix.c2; result.m[1][3] = matrix.d2;
	result.m[2][0] = matrix.a3; result.m[2][1] = matrix.b3; result.m[2][2] = matrix.c3; result.m[2][3] = matrix.d3;
	result.m[3][0] = matrix.a4; result.m[3][1] = matrix.b4; result.m[3][2] = matrix.c4; result.m[3][3] = matrix.d4;
	// x反転（位置のxと、xが混ざる回転成分の符号が変わる）
	result.m[0][1] = -result.m[0][1];
	result.m[0][2] = -result.m[0][2];
	result.m[0][3] = -result.m[0][3];
	result.m[1][0] = -result.m[1][0];
	result.m[2][0] = -result.m[2][0];
	result.m[3][0] = -result.m[3][0];
	return result;
}
```

**差し替える**：`LoadObjFile` の読み込みの部分（`importer.ReadFile` の呼び出し）

> ⚠️ ここは一度「glTFのときだけ `aiProcess_FlipUVs` を外す」形で書いたが、**それは間違いだった**（glTFのテクスチャが上下逆になる）。
> 下のコードが直した後の最終形。理由と測定結果は **Step 1a**。拡張子の判定はもう要らないので、`<algorithm>` と `<cctype>` のincludeも足さなくてよい。

```cpp
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
		 | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
```

**差し替える**：ディフューズテクスチャの取得（`aiTextureType_DIFFUSE` の行）
```cpp
		// glTFはbaseColor、OBJ / FBXはdiffuseに入る。あった方を使う
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS || material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
			mtl.textureFilePath = directoryPath + "/" + texPath.C_Str();
		}
```

**差し替える**：バンプ / 法線テクスチャの取得（`aiTextureType_HEIGHT` の行）
```cpp
		// OBJのmap_bump/bumpはassimpではHEIGHT、glTFはNORMALSに分類される
		if (material->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS || material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
			mtl.bumpTexFilePath = directoryPath + "/" + texPath.C_Str();
		}
```

**追加する**：`LoadObjFile` の最後（メッシュのループの後、`return modelAsset;` の上）
```cpp
	// --- Node解析（モデルファイルの中の階層構造） ---
	// 根からたどって、ノードごとの行列とメッシュ番号を配列に写す
	LoadNode(scene->mRootNode, kInvalidNode, MakeIdentity4x4(), modelAsset);
```

**追加する**：`LoadObjFile` の関数の下（`MakeMeshBuffer` の上）
```cpp
//======================================================================================================
// aiNodeの木を ModelAsset::nodes（配列）へ写す
//======================================================================================================
```
```cpp
uint32_t ModelManager::LoadNode(const aiNode* node, uint32_t parentIndex, const Matrix4x4& parentWorld, ModelAsset& modelAsset) {
	// 先に自分の場所を作る（子を足すと配列が伸びるので、参照ではなくインデックスで触る）
	uint32_t index = static_cast<uint32_t>(modelAsset.nodes.size());
	modelAsset.nodes.emplace_back();
	{
		Node& self = modelAsset.nodes[index];
		self.name = node->mName.C_Str();
		self.localMatrix = ConvertAssimpMatrix(node->mTransformation);
		self.worldMatrix = self.localMatrix * parentWorld; // 行ベクトルなので「自分 × 親」の順
		self.parent = parentIndex;
		// このノードが持つメッシュ（aiMeshの番号は ModelAsset::meshes の番号と同じ）
		self.meshIndices.assign(node->mMeshes, node->mMeshes + node->mNumMeshes);
	}
	Matrix4x4 world = modelAsset.nodes[index].worldMatrix;

	// 子をたどる
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		uint32_t added = LoadNode(node->mChildren[childIndex], index, world, modelAsset);
		modelAsset.nodes[index].children.push_back(added);
	}
	return index;
}
```

### ③ `MyEngine/Graphics/Renderer/Renderer.cpp`

**差し替える**：`DrawModel` のメッシュのループ（`for (const ModelManager::SubMesh& mesh : asset->meshes) {` の所）
```cpp
	// ノードの階層をたどり、ノードが持つメッシュごとに1つのMeshRequest
	for (const ModelManager::Node& node : asset->nodes) {
		for (uint32_t meshIndex : node.meshIndices) {
			const ModelManager::SubMesh& mesh = asset->meshes[meshIndex];
			MeshRequest req;
			// 静的ジオメトリ：GPU常駐バッファを指すだけ
			req.isStatic = true;
			req.vbv = mesh.vbv;
			req.ibv = mesh.ibv;
			req.indexCount = mesh.indexCount;
			// マテリアル構築
			const ModelManager::MtlMaterial* mat = ModelManager::GetMtlMaterial(config.modelHandle, mesh.materialName);
			req.materialData = MakeModelMaterial(mat, config.color, config.uvTransform, config.material);
			req.materialData.textureIndex = ResolveTextureIndex((config.textureHandle != 0) ? config.textureHandle : (mat ? mat->srvIndex : 0));
			req.objectTransformData.worldMatrix = node.worldMatrix * worldMatrix; // ノードの行列を挟む
			req.objectTransformData.isBillboard = config.isBillboard;
			req.cameraData.worldPosition = config.camera ? config.camera->GetTranslation() : Vector3{};
			req.iblParamsAddress = config.env ? config.env->GetParametersAddress() : 0;
			// 描画設定
			req.shadingType = config.shadingType;
			req.blendMode = config.blendMode;
			req.rasterizerType = config.rasterizerType;
			req.depthMode = config.depthMode;
			req.windowTitle = config.windowTitle;
			req.debugName = &ModelManager::GetModelName(config.modelHandle);
			req.debugSubName = &mesh.materialName;

			RenderQueue::Request(std::move(req));
		}
	}
}
```

---

### 解説

**なぜ転置するのか**
- assimpは「列ベクトル」の世界。`変換後 = 行列 × 頂点` の形で、移動量は行列の**4列目**に入る。
- このエンジンは「行ベクトル」の世界。`変換後 = 頂点 × 行列`（シェーダーも `mul(position, world)`）で、移動量は**4行目**に入る。
- 同じ変換を別の並べ方で書いているだけなので、**転置**すると移り合う。

**なぜx反転が必要なのか**
- 頂点を読むときに `-position.x` にしている（assimpは右手系、このエンジンは左手系）。つまり頂点は「x反転した空間」に入っている。
- 行列だけ元の空間のままだと、「x反転した頂点」を「元の空間の行列」で動かすことになり、左右が逆に動く（親を右に動かしたのに子が左にずれる、など）。
- x反転の行列 `X`（xだけ −1 の行列）で挟んだ `X × 行列 × X` が、x反転した空間での同じ変換になる。結果として**「片方の添字だけがxの成分」の符号が反転する**だけなので、コードでは6か所の符号を変えている。
- 移動量のxが反転するのも同じ理由（右にずらす → x反転した空間では左）。

**なぜ `自分 × 親` の順に掛けるのか**
- 行ベクトルでは、頂点は左から右へ通っていく：`頂点 × 自分 × 親 × ビュー射影`。
- なので「先に自分の行列、その後に親の行列」の順で並べる。列ベクトルの資料（`親 × 自分`）とは逆になるので、ここは混ざりやすい。
- `EntityManager::UpdateTransforms` も同じ順（`自分 * 親`）。

**読み込み時に「根からの行列」を計算しておく理由**
- ノードが動かない（アニメーションが無い）うちは、毎フレーム計算しても結果が同じ。
- 描画時は `ノードの行列 × ゲームが指定した行列` を渡すだけになる。
- アニメーションを入れるときは、この `worldMatrix` を毎フレーム計算し直す形（Step 4）になる。

**`emplace_back` した直後に参照を取り直している理由**
```cpp
	uint32_t index = static_cast<uint32_t>(modelAsset.nodes.size());
	modelAsset.nodes.emplace_back();
```
- 子を追加すると `nodes` の配列が伸びて、**前に取った参照やポインタが無効になる**（`std::vector` の再確保）。
- なので子をたどる間は参照を持たず、`modelAsset.nodes[index]` と番号で触っている。ヒエラルキーの操作を後回しにしたのと同じ考え方。

**OBJはノードを持っているのか**
- OBJにも階層はある（`o` や `g` がノードになる）が、行列は持たない（頂点が最初からワールド座標）。なのでOBJでは全ノードの行列が単位行列になり、**今までと同じ結果**になる。
- 階層の違いが出るのは glTF / FBX / Blender からの書き出し。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - いま使っているOBJ（grid、モデル）の見た目が前と変わらない
   - 親子でずらして作ったモデル（Blenderで「立方体の子に小さい立方体を離して置く」など）を読むと、**ファイル通りの位置関係で出る**（前は重なっていた）
   - glTFを読むと、テクスチャが上下逆にならない ← **ここが引っかかった。Step 1a で直す**
   - glTFのbaseColorテクスチャが出る（真っ白にならない）
   - PIXで見ると、ノードが持つメッシュの数だけ描画が積まれている

### 次のStepでやること（ここではやらない）
- Step 2：ノード階層をEntityの階層として取り込む（`Editor.md` Step 3のRenderComponentができてから）。
- Step 3：`.glb` の埋め込みテクスチャ。

---

## Step 1a：glTFのテクスチャが上下逆になるのを直す

### 症状
- OBJは正常。**glTFだけテクスチャが上下逆**（monsterBall.gltf なら赤が下、白が上になる）。形・位置は正常。

### 原因
Step 1 で「glTFはUVの原点が左上（DirectXと同じ）だから `aiProcess_FlipUVs` は要らない」と判断したが、**これが間違い**だった。

glTF仕様としては確かに左上原点なのだが、**assimpのglTF2インポーターが読み込む時点で V を `1-v` に反転している**（インポーターの中に「Flip Y coords」という処理がある）。
つまり assimp から受け取るUVは、**OBJだろうがglTFだろうが「左下原点」に揃えられている**。だからDirectX用に上下を反転する処理は、形式に関係なく必ず必要。

**実測**（assimpを直接呼んで、球の上の極（y=+1）の頂点のUVを表示した結果）

| 読み込みフラグ | ファイルの中のV | assimpが返すV | 結果 |
|---|---|---|---|
| `FlipUVs` なし | 0.000（＝画像の上、赤） | **1.000** | 画像の下（白）を拾う → **上下逆** |
| `FlipUVs` あり | 0.000 | **0.000** | 画像の上（赤）を拾う → **正しい** |

- 使った測定方法：assimpだけをリンクした小さなexeを作り、`ReadFile` の後に `mMesh[0]->mTextureCoords[0][いちばんyが大きい頂点]` を表示して、`.gltf` のJSON＋`.bin` から直接読んだ生のUVと比べた。
- ついでに分かったこと：**assimpはノード行列の平行移動を `a4 / b4 / c4`（4列目）に入れて返す**ので、Step 1 の「転置してから入れる」は正しい。

### 直し方（1か所だけ）

**差し替える**：`MyEngine/Graphics/Model/ModelManager.cpp` の `LoadObjFile`、拡張子を見ている5行 ＋ `ReadFile` の行

差し替え前（Step 1 で書いたもの）
```cpp
	// OBJはUVの原点が左下、glTFは左上（DirectXと同じ）。OBJなどのときだけ上下を反転する
	std::string extension = std::filesystem::path(filename).extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const bool isGltf = (extension == ".gltf" || extension == ".glb");
	uint32_t flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipWindingOrder | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;
	if (!isGltf) {
		flags |= aiProcess_FlipUVs;
	}
	const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
```

差し替え後
```cpp
	// assimpはglTFのUVも読み込む時点で上下反転して「左下原点」に揃えるので、DirectXに合わせる反転は形式に関係なく必要
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
		 | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
```

**消してよい**：Step 1 で足した2つのinclude（他で使っていなければ）
```cpp
#include <algorithm>
#include <cctype>
```

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - glTF（monsterBall.gltf など）のテクスチャが正しい向きで出る
   - OBJ（grid、terrain）の見た目が変わっていない

### 覚えておくこと
- **「ファイル形式の仕様」と「ライブラリが返す値」は別物**。assimpのように形式の差を吸収してくれるライブラリは、仕様どおりの値をそのまま渡してくるとは限らない。
- 今回のように見た目で分かる不具合なら、**実際に値を表示して確かめるのが一番速い**。仕様を読んで推測すると、今回のように逆の結論になる。
