#include "MyEngine/Graphics/Model/ModelManager.h"

#include <map>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
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


// ===== インスタンス取得 =====
ModelManager& ModelManager::GetInstance() {
	static ModelManager instance;
	return instance;
}

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
		LogManager::Error(std::format("serach name[{}] bytes={}", name, name.size()));
		for (auto& [key, _] : model.materialMap) {
			LogManager::Error(std::format("have key[{}] bytes={}", key, key.size()));
		}

		LogManager::Error(std::format("{}: not found", name));
		MY_ASSERT_MSG(false, "存在しないマテリアル名を参照しています");
		return nullptr;
	}
	return &materialCheck->second;
}


//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
uint32_t ModelManager::Load(const std::string& objFilePath) {
	auto& inst = GetInstance();
	// 重複チェック
	auto cached = inst.pathToHandle_.find(objFilePath);
	if (cached != inst.pathToHandle_.end()) {
		return cached->second;
	}

	// ディレクトリパスとファイル名を分離
	std::filesystem::path path(objFilePath);
	std::string directoryPath = path.parent_path().string();
	std::string filename = path.filename().string();
	// OBJ読み込み
	ModelAsset ModelAsset = LoadObjFile(directoryPath, filename);
	// マテリアルのテクスチャをTextureManagerでロード
	for (auto& [name, mat] : ModelAsset.materialMap) {
		if (!mat.textureFilePath.empty()) {
			mat.srvIndex = TextureManager::Load(mat.textureFilePath);
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
	inst.pathToHandle_[objFilePath] = handle;

	return handle;
}


//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
ModelManager::ModelAsset ModelManager::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	Assimp::Importer importer;
	// --- 読み込み ---
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices 
		 | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
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
		// テクスチャ（パスはモデルファイルからの相対なのでdirectoryPathを前置）
		aiString texPath;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
			mtl.textureFilePath = directoryPath + "/" + texPath.C_Str();
		}
		if (material->GetTexture(aiTextureType_AMBIENT, 0, &texPath) == AI_SUCCESS) {
			mtl.ambientTexFilePath = directoryPath + "/" + texPath.C_Str();
		}
		if (material->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
			mtl.specularTexFilePath = directoryPath + "/" + texPath.C_Str();
		}
		// OBJのmap_bump/bumpはassimpではHEIGHTに分類される
		if (material->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
			mtl.bumpTexFilePath = directoryPath + "/" + texPath.C_Str();
		}

		modelAsset.materialMap[mtl.name] = mtl;
	}

	// --- Mesh解析 ---
	// assimpは「1メッシュ=1マテリアル」に分割済みなので、aiMeshをそのままSubMeshにする
	modelAsset.meshes.reserve(scene->mNumMeshes);
	// ループ
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		MY_ASSERT_MSG(mesh->HasNormals(), filename + ": 法線がないMeshは非対応です");
		MY_ASSERT_MSG(mesh->HasTextureCoords(0), filename + ": TexcoordがないMeshは非対応です");
		// SubMesh
		SubMesh subMesh;
		subMesh.materialName = materialNames[mesh->mMaterialIndex];
		subMesh.vertices.reserve(mesh->mNumVertices); // 頂点。JoinIdenticalVerticesで重複排除済みなのでそのままコピーできる
		// Meshの中身（Face）の解析を行う
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
			Vertex3dData vertex;
			// 右手系→左手系はx反転で対処（FlipWindingOrderとセット）
			vertex.position = {-position.x, position.y, position.z, 1.0f};
			vertex.normal = {-normal.x, normal.y, normal.z};
			vertex.texcoord = {texcoord.x, texcoord.y};
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

	return modelAsset;
}


//======================================================================================================
// メッシュのCPU頂点を GPU常駐バッファへ転送する（ロード時1回だけ）
//======================================================================================================
void ModelManager::MakeMeshBuffer(SubMesh& mesh, const std::string& modelName) {
	if (mesh.vertices.empty() || mesh.indices.empty()) {
		return;
	}
	const size_t vbSize = sizeof(Vertex3dData) * mesh.vertices.size();
	const size_t ibSize = sizeof(uint32_t) * mesh.indices.size();
	// 頂点バッファ
	mesh.vertexBuffer = DirectXCommon::CreateDefaultBuffer(vbSize);
	UploadContext::QueueUpload(mesh.vertexBuffer.Get(), mesh.vertices.data(), vbSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	// インデックスバッファ
	mesh.indexBuffer = DirectXCommon::CreateDefaultBuffer(ibSize);
	UploadContext::QueueUpload(mesh.indexBuffer.Get(), mesh.indices.data(), ibSize, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	
	// バッファに名前をつける
	std::wstring base = ConvertString(modelName + "/" + mesh.materialName);
	mesh.vertexBuffer->SetName((base + L"_VB").c_str());
	mesh.indexBuffer->SetName((base + L"_IB").c_str());

	// 頂点バッファビュー
	mesh.vbv.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
	mesh.vbv.SizeInBytes = static_cast<UINT>(vbSize);
	mesh.vbv.StrideInBytes = sizeof(Vertex3dData);
	// インデックスバッファビュー
	mesh.ibv.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
	mesh.ibv.SizeInBytes = static_cast<UINT>(ibSize);
	mesh.ibv.Format = DXGI_FORMAT_R32_UINT;

	mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
}