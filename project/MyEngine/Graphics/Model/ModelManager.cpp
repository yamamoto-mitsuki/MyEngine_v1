#include "MyEngine/Graphics/Model/ModelManager.h"

#include <map>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <filesystem>

#include <pix.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

//======================================================================================================
// assimpの行列 → エンジンの行列
// ・assimpは「列ベクトル（変換後 = 行列 × 頂点）」で行優先。エンジンは「行ベクトル（変換後 = 頂点 × 行列）」なので転置する
// ・頂点を読むときにxを反転している（右手系→左手系）ので、行列も x を反転した空間へ直す
//   （x反転の行列で前後から挟む形。結果として「片方の添字だけがx」の成分の符号が反転する）
//======================================================================================================
static Matrix4x4 ConvertAssimpMatrix(const aiMatrix4x4& matrix) {
	Matrix4x4 result{};
	// 転置しながら入れる
	result.m[0][0] = matrix.a1;
	result.m[0][1] = matrix.b1;
	result.m[0][2] = matrix.c1;
	result.m[0][3] = matrix.d1;
	result.m[1][0] = matrix.a2;
	result.m[1][1] = matrix.b2;
	result.m[1][2] = matrix.c2;
	result.m[1][3] = matrix.d2;
	result.m[2][0] = matrix.a3;
	result.m[2][1] = matrix.b3;
	result.m[2][2] = matrix.c3;
	result.m[2][3] = matrix.d3;
	result.m[3][0] = matrix.a4;
	result.m[3][1] = matrix.b4;
	result.m[3][2] = matrix.c4;
	result.m[3][3] = matrix.d4;
	// x反転（位置のxと、xが混ざる回転成分の符号が変わる）
	result.m[0][1] = -result.m[0][1];
	result.m[0][2] = -result.m[0][2];
	result.m[0][3] = -result.m[0][3];
	result.m[1][0] = -result.m[1][0];
	result.m[2][0] = -result.m[2][0];
	result.m[3][0] = -result.m[3][0];
	return result;
}


// ===== インスタンス取得 =====
ModelManager& ModelManager::GetInstance() {
	static ModelManager instance;
	return instance;
}

//======================================================================================================
// 初期化 / 解放
//======================================================================================================
// ===== 初期化 =====
void ModelManager::Initialize() {}

// ===== 解放 =====
void ModelManager::Release() {
	auto& inst = GetInstance();
	inst.models_.clear();
	inst.pathToHandle_.clear();
	inst.modelsKey_ = 1;
}

// ===== 取得 =====
// モデル
const ModelManager::ModelAsset* ModelManager::GetModelAsset(uint32_t handle) {
	auto it = GetInstance().models_.find(handle);
	if (it != GetInstance().models_.end()) {
		return &it->second;
	}

	LogManager::Warning("ModelAssetが見つかりませんでした");
	return nullptr;
}

// モデルのマテリアル
ModelManager::MtlMaterial* ModelManager::GetMtlMaterial(uint32_t handle, const std::string name) { 
	// --- 検索対象のモデル ---
	auto modelCheck = GetInstance().models_.find(handle); 
	// 存在しているmodelHandleか
	if (modelCheck == GetInstance().models_.end()) {
		LogManager::Error(std::format("{}: not found", handle));
		MY_ASSERT_MSG(false, "存在しないmodelHandleを参照しています");
	}
	ModelAsset& model = modelCheck->second;

	// --- マテリアル ---
	auto materialCheck = model.materialMap.find(name);
	// 存在している名前か
	if (materialCheck == model.materialMap.end()) {
		LogManager::Error(std::format("search name[{}] bytes={}", name, name.size()));
		for (auto& [key, _] : model.materialMap) {
			LogManager::Error(std::format("have key[{}] bytes={}", key, key.size()));
		}

		LogManager::Error(std::format("{}: not found", name));
		MY_ASSERT_MSG(false, "存在しないマテリアル名を参照しています");
		return nullptr;
	}
	return &materialCheck->second;
}

// ===== モデル名取得 =====
const std::string& ModelManager::GetModelName(uint32_t modelHandle) {
	auto it = GetInstance().models_.find(modelHandle);
	if (it != GetInstance().models_.end()) {
		return it->second.name;
	}
	static const std::string emptyString;
	return emptyString;
}

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


//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
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

	// GPU常駐バッファを構築（転送を予約）
	for (SubMesh& mesh : ModelAsset.meshes) {
		MakeMeshBuffer(mesh, filename);
	}
	// 予約した転送をまとめて実行
	UploadContext::Flush();
	// ハンドル割り当てと登録
	uint32_t handle = inst.modelsKey_++;
	inst.models_[handle] = std::move(ModelAsset);
	inst.pathToHandle_[key] = handle;

	return handle;
}


//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
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
	MY_ASSERT_MSG(scene != nullptr, std::string("読み込み失敗: ") + importer.GetErrorString());
	MY_ASSERT_MSG(scene->HasMeshes(), "メッシュを見つけられませんでした");

	ModelAsset modelAsset;
	// --- Material解析 ---
	// assimpはメッシュごとにマテリアル番号（mMaterialIndex）しか持たないので、番号→名前の表を作っておき、後でSubMesh::materialNameにいれる
	std::vector<std::string> materialNames(scene->mNumMaterials);
	// ループ
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		MtlMaterial mtl;
		// 名前（objのnewmtl名。ない場合assimpが"DefaultMaterial"を作る）
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		mtl.name = name.C_Str();
		materialNames[materialIndex] = mtl.name;
		// 環境光などの色（見つからない場合、MtlMaterialのデフォルト値）
		aiColor3D color;
		if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
			mtl.ambient = {color.r, color.g, color.b};
		}
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
			mtl.diffuse = {color.r, color.g, color.b};
		}
		if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
			mtl.specular = {color.r, color.g, color.b};
		}
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
			mtl.emissive = {color.r, color.g, color.b};
		}
		// スカラー
		material->Get(AI_MATKEY_SHININESS, mtl.shininess);
		material->Get(AI_MATKEY_OPACITY, mtl.dissolve);
		material->Get(AI_MATKEY_METALLIC_FACTOR, mtl.metallic);
		material->Get(AI_MATKEY_ROUGHNESS_FACTOR, mtl.roughness);
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

		modelAsset.materialMap[mtl.name] = mtl;
	}

	// --- Mesh解析 ---
	// assimpは「1メッシュ=1マテリアル」に分割済みなので、aiMeshをそのままSubMeshにする
	modelAsset.meshes.reserve(scene->mNumMeshes);
	// ループ
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		// 法線・UVが無いモデルもあるので、無ければ既定値で補う
		const bool hasNormals = mesh->HasNormals();
		const bool hasTexcoords = mesh->HasTextureCoords(0);
		if (!hasNormals) {
			LogManager::Warning(filename + ": 法線がないMeshです。(0,1,0)で補完します");
		}
		if (!hasTexcoords) {
			LogManager::Warning(filename + ": TexcoordがないMeshです。(0,0)で補完します");
		}
		// SubMesh
		SubMesh subMesh;
		subMesh.materialName = materialNames[mesh->mMaterialIndex];
		subMesh.isSkinned = mesh->HasBones(); // 骨を持つメッシュ（gltfのskin）
		subMesh.vertices.reserve(mesh->mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			Vertex3dData vertex;
			// 右手系→左手系はx反転で対処（FlipWindingOrderとセット）
			vertex.position = {-position.x, position.y, position.z, 1.0f};
			// 法線
			if (hasNormals) {
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				vertex.normal = {-normal.x, normal.y, normal.z};
			} else {
				vertex.normal = {0.0f, 1.0f, 0.0f};
			}
			// UV
			if (hasTexcoords) {
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				vertex.texcoord = {texcoord.x, texcoord.y};
			} else {
				vertex.texcoord = {0.0f, 0.0f};
			}
			subMesh.vertices.push_back(vertex);
		}
		// 頂点インデックス。Faceの中身をそのまま積む
		subMesh.indices.reserve(mesh->mNumFaces * 3);
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			MY_ASSERT_MSG(face.mNumIndices == 3, "ポリゴンは三角形のみサポートしています");
			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				subMesh.indices.push_back(face.mIndices[element]);
			}
		}
		modelAsset.meshes.push_back(std::move(subMesh));
	}

	// --- Node解析（モデルファイルの中の階層構造） ---
	// 根からたどって、ノードごとの行列とメッシュ番号を配列に写す
	LoadNode(scene->mRootNode, kInvalidNode, MakeIdentity4x4(), modelAsset);
	// スキン（骨の印・逆バインド行列・頂点ごとの骨と重み）
	LoadSkin(scene, modelAsset);
	return modelAsset;
}


//======================================================================================================
// aiNodeの木を ModelAsset::nodes（配列）へ写す
//======================================================================================================
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


//======================================================================================================
// スキン（骨）を読む
//======================================================================================================
void ModelManager::LoadSkin(const aiScene* scene, ModelAsset& modelAsset) {
	// --- ノード名 → 番号の表 ---
	std::unordered_map<std::string, uint32_t> nameToIndex;
	nameToIndex.reserve(modelAsset.nodes.size());
	for (uint32_t nodeIndex = 0; nodeIndex < modelAsset.nodes.size(); ++nodeIndex) {
		nameToIndex.emplace(modelAsset.nodes[nodeIndex].name, nodeIndex); // 同じ名前が2つあれば、前の方を使う
	}

	// --- メッシュごとの骨（aiBone）---
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh* mesh = scene->mMeshes[meshIndex];
		if (!mesh->HasBones()) {
			continue; // スキンのないメッシュ
		}
		// 頂点ごとの骨と重み（verticesと同じ数・順番。最初は全部重み0）
		SubMesh& subMesh = modelAsset.meshes[meshIndex]; // aiMeshの番号とSubMeshの番号は同じ
		subMesh.influences.assign(subMesh.vertices.size(), VertexInfluence{});

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			const aiBone* bone = mesh->mBones[boneIndex];
			auto found = nameToIndex.find(bone->mName.C_Str());
			if (found == nameToIndex.end()) {
				continue; // 骨の名前のノードがない（壊れたファイル）。この骨は使わない
			}
			const uint32_t jointIndex = found->second; // この骨の番号 = ノードの番号 = 行列パレットの番号
			Node& joint = modelAsset.nodes[jointIndex];

			// ===== 骨の印 =====
			// 自分に骨の印、自分から根までに骨の線を通す印
			// 親は必ず自分より前の番号にいつ。既に印のあるところまで来たら、その先も印が付いているので止める
			joint.isJoint = true;
			for (uint32_t index = jointIndex; index != kInvalidNode && !modelAsset.nodes[index].isSkeleton; 
				index = modelAsset.nodes[index].parent) {
				modelAsset.nodes[index].isSkeleton = true;
			}

			// ===== 逆バインド行列 =====
			// assimpでは mOffsetMatrix（gltfのinverseBindMatrices）。ノードの行列と同じく、転置とx反転で直す
			joint.inverseBindPoseMatrix = ConvertAssimpMatrix(bone->mOffsetMatrix);

			// ===== 頂点ごとの重み =====
			// 骨が「どの頂点を、どれだけ動かすか」を持っているので、頂点の側（influences）へ写し替える
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				const aiVertexWeight& vertexWeight = bone->mWeights[weightIndex];
				if (vertexWeight.mVertexId >= subMesh.influences.size()) {
					continue; // 頂点の番号が範囲外（壊れたファイル）
				}
				AddInfluence(subMesh.influences[vertexWeight.mVertexId], static_cast<int32_t>(jointIndex), vertexWeight.mWeight);
			}
		}
		// 重みの合計を1にそろえる
		for (VertexInfluence& influence : subMesh.influences) {
			NormalizeInfluence(influence);
		}
	}
}


//======================================================================================================
// 頂点に骨を1本足す
//======================================================================================================
void ModelManager::AddInfluence(VertexInfluence& influence, int32_t jointIndex, float weight) {
	if (weight <= 0.0f) {
		return; // 動かさない骨
	}
	// --- 一番重みの小さい枠を探す。空いている枠（重み0）があれば、そこが選ばれる ---
	auto smallest = std::ranges::min_element(influence.weights); // 一番要素の小さい場所を返す
	if (weight <= *smallest) {
		return; // 4本とも埋まっていて、どれよりも軽い骨は捨てる
	}
	// 空き枠に入れる。4本とも埋まっているときは、一番軽い骨と入れ替える
	const size_t slot = static_cast<size_t>(std::distance(influence.weights.begin(), smallest));
	influence.weights[slot] = weight;
	influence.jointIndices[slot] = jointIndex;
}


//======================================================================================================
// 重みの合計を1にそろえる
//======================================================================================================
void ModelManager::NormalizeInfluence(VertexInfluence& influence) { 
	// begin(),end()まで足した合計
	const float sum = std::accumulate(influence.weights.begin(), influence.weights.end(), 0.0f);
	if (sum <= 0.0f) {
		return; // どの骨にも動かされない頂点
	}
	for (float& weight : influence.weights) {
		weight /= sum;
	}
}


//======================================================================================================
// スキンの行列パレット
//======================================================================================================
void ModelManager::MakeMatrixPalette(const ModelAsset& asset, const std::vector<Matrix4x4>* nodeMatrices, 
	std::vector<WellForGPU>& palette) {
	MY_ASSERT_MSG(nodeMatrices == nullptr || nodeMatrices->size() == asset.nodes.size(), "ノードの行列の数がモデルと違います");
	palette.resize(asset.nodes.size()); // 番号はノードの番号（VertexInfluence::jointIndices と同じ）
	// ノード分回す
	for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex) {
		const Node& node = asset.nodes[nodeIndex];
		WellForGPU& well = palette[nodeIndex];
		if (!node.isJoint) {
			// 骨のないノードは、どの頂点からも重みを付けて指されない（空き枠の0番が重みで0を指すだけ）
			// 0を掛けても NaN にならないように、単位行列にしておく
			well.skeletonSpaceMatrix = MakeIdentity4x4();
			well.skeletonSpaceInverseTransposeMatrix = MakeIdentity4x4();
			continue;
		}
		// 今の骨の行列（AnimationPlayer が作ったものか、モデルに入っている形）
		const Matrix4x4& jointMatrix = nodeMatrices ? (*nodeMatrices)[nodeIndex] : node.worldMatrix;
		// バインドポーズの頂点を一度骨から見た位置に戻して（逆バインド行列）、今の骨の行列で置きなおす
		well.skeletonSpaceMatrix = node.inverseBindPoseMatrix * jointMatrix;
		// 法線用 非均一スケールでも面に垂直なままにする
		well.skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(well.skeletonSpaceMatrix));
	}
}


//======================================================================================================
// メッシュのCPU頂点を GPU常駐バッファへ転送する（ロード時1回だけ）
//======================================================================================================
void ModelManager::MakeMeshBuffer(SubMesh& mesh, const std::string& modelName) {
	if (mesh.vertices.empty() || mesh.indices.empty()) {
		return;
	}
	// バッファの名前の頭
	const std::wstring base = ConvertString(modelName + "/" + mesh.materialName);

	// 頂点バッファ（スロット0：形）
	mesh.vbv = UploadContext::CreateVertexBuffer(mesh.vertices, mesh.vertexBuffer, base + L"_VB");
	// スキンの頂点バッファ（スロット1：骨と重み）。スキンのあるメッシュだけ
	if (!mesh.influences.empty()) {
		MY_ASSERT_MSG(mesh.influences.size() == mesh.vertices.size(), "骨と重みの数が頂点の数と違います（同じ頂点番号で2本のバッファを読むので、揃っている必要があります）");
		mesh.influenceVbv = UploadContext::CreateVertexBuffer(mesh.influences, mesh.influenceBuffer, base + L"_Influence");
	}
	// インデックスバッファ
	mesh.ibv = UploadContext::CreateIndexBuffer(mesh.indices, mesh.indexBuffer, base + L"_IB");
	mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
}