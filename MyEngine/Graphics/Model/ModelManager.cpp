#include "MyEngine/Graphics/Model/ModelManager.h"

#include <map>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/GPU/UploadContext.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"


// モデルの頂点インデックス登録の際、頂点情報の重複を避けるためのハッシュ
namespace {
struct VertexKey {
	int32_t px, py, pz, u, v, nx, ny, nz;
	bool operator==(const VertexKey& o) const { return px == o.px && py == o.py && pz == o.pz && u == o.u && v == o.v && nx == o.nx && ny == o.ny && nz == o.nz; }
};

struct VertexKeyHash {
	size_t operator()(const VertexKey& k) const {
		size_t h = 14695981039346656037ull; // FNV-1a
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&k);
		for (size_t i = 0; i < sizeof(VertexKey); ++i) {
			h = (h ^ p[i]) * 1099511628211ull;
		}
		return h;
	}
};
} // namespace

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
	ModelAsset model = modelCheck->second;

	// --- マテリアル ---
	auto materialCheck = model.materialMap.find(name);
	// 存在している名前か
	if (materialCheck == model.materialMap.end()) {
		LogManager::Error(std::format("{}: not found", name));
		MY_ASSERT_MSG(false, "存在しないマテリアル名を参照しています");
	}
	MtlMaterial material = materialCheck->second;

	return &material;
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
		MakeMeshBuffer(mesh);
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
	ModelAsset ModelAsset;
	std::vector<Vector4> positions; // 頂点位置
	std::vector<Vector3> normals;   // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	MY_ASSERT_MSG(file.is_open(), "OBJファイルが開けませんでした");

	SubMesh* currentMesh = nullptr;

	// ===== スムーズシェードグループの管理 =====
	// 管理番号(s 0はフラットシェーディング)
	int currentSmoothGroup = 0;
	// 頂点がどのスムーズグループに属するか記録する
	// メッシュインデックス、頂点インデックスからグループ番号
	std::vector<std::vector<int>> vertexSmoothGroups;

	while (std::getline(file, line)) {
		// コメント・空行をスキップ
		if (line.empty() || line[0] == '#') {
			continue;
		}

		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			// ===== 頂点位置 =====
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			// ===== テクスチャ座標 =====
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			// ===== 法線 =====
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);

		} else if (identifier == "s") {
			std::string value;
			s >> value;
			if (value == "off" || value == "0") {
				// フラットシェーディング
				currentSmoothGroup = 0;
			} else {
				// スムーズシェードグループ番号
				currentSmoothGroup = std::stoi(value);
			}

		} else if (identifier == "mtllib") {
			// ===== MTLファイルを読み込む =====
			std::string mtlFilename;
			s >> mtlFilename;
			ModelAsset.materialMap = LoadMaterialTemplateFile(directoryPath, mtlFilename);

		} else if (identifier == "usemtl") {
			// ===== マテリアルが変わったら新しいメッシュを開始 =====
			std::string materialName;
			s >> materialName;
			ModelAsset.meshes.push_back(SubMesh{});
			currentMesh = &ModelAsset.meshes.back();
			currentMesh->materialName = materialName;
			vertexSmoothGroups.push_back({});

		} else if (identifier == "f") {
			if (!currentMesh) {
				ModelAsset.meshes.push_back(SubMesh{});
				currentMesh = &ModelAsset.meshes.back();
				vertexSmoothGroups.push_back({});
			}
			// 頂点を全部読む
			std::vector<Vertex3dData> faceVertices;
			std::string vertexDefinition;

			while (s >> vertexDefinition) {
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = {0, 0, 0};
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					if (!index.empty()) {
						elementIndices[element] = std::stoi(index);
					}
				}
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = elementIndices[1] > 0 ? texcoords[elementIndices[1] - 1] : Vector2{0.0f, 0.0f};
				Vector3 normal = elementIndices[2] > 0 ? normals[elementIndices[2] - 1] : Vector3{0.0f, 1.0f, 0.0f};
				faceVertices.push_back({position, texcoord, normal});
			}

			// ポリゴンを三角形に分割
			for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
				// 巻き順逆転
				currentMesh->vertices.push_back(faceVertices[i + 1]);
				vertexSmoothGroups.back().push_back(currentSmoothGroup);
				currentMesh->vertices.push_back(faceVertices[i]);
				vertexSmoothGroups.back().push_back(currentSmoothGroup);
				currentMesh->vertices.push_back(faceVertices[0]);
				vertexSmoothGroups.back().push_back(currentSmoothGroup);
			}
		}
	}

	// ===== スムーズシェーディング処理 =====
	// スムーズグループ番号が1以上の頂点は、同じ位置・同じグループの頂点の法線を平均化する
	for (size_t meshIdx = 0; meshIdx < ModelAsset.meshes.size(); ++meshIdx) {
		SubMesh& mesh = ModelAsset.meshes[meshIdx];
		const std::vector<int>& groups = vertexSmoothGroups[meshIdx];

		// 法線の合計と個数を記録
		struct NormalAccum {
			Vector3 sum = {0.0f, 0.0f, 0.0f};
			int count = 0;
		};
		std::unordered_map<std::string, NormalAccum> accumMap;

		auto makeKey = [](const Vector4& p, int group) {
			// 小数点以下4桁で丸めてキーにする（浮動小数点の誤差対策）
			auto fmt = [](float f) { return std::to_string(static_cast<int>(std::round(f * 10000.0f))); };
			return fmt(p.x) + "," + fmt(p.y) + "," + fmt(p.z) + "," + std::to_string(group);
		};

		// 法線を蓄積
		for (size_t i = 0; i < mesh.vertices.size(); ++i) {
			int group = groups[i];
			if (group == 0) {
				continue;
			}

			std::string key = makeKey(mesh.vertices[i].position, group);
			accumMap[key].sum.x += mesh.vertices[i].normal.x;
			accumMap[key].sum.y += mesh.vertices[i].normal.y;
			accumMap[key].sum.z += mesh.vertices[i].normal.z;
			accumMap[key].count++;
		}

		// 平均化して正規化
		for (size_t i = 0; i < mesh.vertices.size(); ++i) {
			int group = groups[i];
			if (group == 0) {
				continue;
			}

			std::string key = makeKey(mesh.vertices[i].position, group);
			auto it = accumMap.find(key);
			if (it == accumMap.end()) {
				continue;
			}

			const NormalAccum& accum = it->second;
			Vector3 avg = {
			    accum.sum.x / accum.count,
			    accum.sum.y / accum.count,
			    accum.sum.z / accum.count,
			};

			// 正規化
			float len = std::sqrt(avg.x * avg.x + avg.y * avg.y + avg.z * avg.z);
			if (len > 0.0001f) {
				mesh.vertices[i].normal = {avg.x / len, avg.y / len, avg.z / len};
			}
		}
	}

	// ===== 重複を排除して、頂点インデックスを生成 =====
	auto quantize = [](float f) { return static_cast<int32_t>(std::lround(f * 10000.0f)); }; // 1/10000で丸めて誤差を対策
	for (SubMesh& mesh : ModelAsset.meshes) {
		std::unordered_map<VertexKey, uint32_t, VertexKeyHash> unique; // キーが何番目の頂点か
		std::vector<Vertex3dData> verts; // 重複排除後の頂点
		std::vector<uint32_t> indices;   // 頂点インデックス
		verts.reserve(mesh.vertices.size());
		indices.reserve(mesh.vertices.size());

		for (const Vertex3dData& v : mesh.vertices) {
			VertexKey key{quantize(v.position.x), quantize(v.position.y), quantize(v.position.z), quantize(v.texcoord.x),
			              quantize(v.texcoord.y), quantize(v.normal.x),   quantize(v.normal.y),   quantize(v.normal.z)};
			auto it = unique.find(key);

			if (it != unique.end()) {
				indices.push_back(it->second);
			} else {
				uint32_t idx = static_cast<uint32_t>(verts.size());
				unique.emplace(key, idx);
				verts.push_back(v);
				indices.push_back(idx);
			}
		}
		mesh.vertices = std::move(verts);
		mesh.indices = std::move(indices);
	}
	return ModelAsset;
}

//======================================================================================================
// MTLファイルを読み込む
//======================================================================================================
std::map<std::string, ModelManager::MtlMaterial> ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {

	std::map<std::string, MtlMaterial> materialMap;
	MtlMaterial* currentMaterial = nullptr;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	MY_ASSERT_MSG(file.is_open(), "MTLファイルが開けませんでした");

	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#')
			continue;

		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "newmtl") {
			// ===== 新しいマテリアルを開始 =====
			std::string materialName;
			s >> materialName;
			materialMap[materialName] = MtlMaterial{};
			currentMaterial = &materialMap[materialName];
			currentMaterial->name = materialName;

		} else if (currentMaterial) {
			if (identifier == "Ka") {
				s >> currentMaterial->ambient.x >> currentMaterial->ambient.y >> currentMaterial->ambient.z;
			} else if (identifier == "Kd") {
				s >> currentMaterial->diffuse.x >> currentMaterial->diffuse.y >> currentMaterial->diffuse.z;
			} else if (identifier == "Ks") {
				s >> currentMaterial->specular.x >> currentMaterial->specular.y >> currentMaterial->specular.z;
			} else if (identifier == "Ke") {
				s >> currentMaterial->emissive.x >> currentMaterial->emissive.y >> currentMaterial->emissive.z;
			} else if (identifier == "Ns") {
				s >> currentMaterial->shininess;
			} else if (identifier == "d") {
				s >> currentMaterial->dissolve;
			} else if (identifier == "Tr") {
				// Tr は 1-d（Trが大きいほど透明）
				float tr;
				s >> tr;
				currentMaterial->dissolve = 1.0f - tr;
			} else if (identifier == "illum") {
				s >> currentMaterial->illum;
			} else if (identifier == "map_Kd") {
				std::string texFilename;
				s >> texFilename;
				currentMaterial->textureFilePath = directoryPath + "/" + texFilename;
			} else if (identifier == "map_Ka") {
				std::string texFilename;
				s >> texFilename;
				currentMaterial->ambientTexFilePath = directoryPath + "/" + texFilename;
			} else if (identifier == "map_Ks") {
				std::string texFilename;
				s >> texFilename;
				currentMaterial->specularTexFilePath = directoryPath + "/" + texFilename;
			} else if (identifier == "map_bump" || identifier == "bump") {
				// -bm などのオプションを読み飛ばしてファイル名を取得
				// 例: "map_bump -bm 1.0 normal.png"
				std::string token;
				while (s >> token) {
					if (!token.empty() && token[0] == '-') {
						s >> token; // オプション値を読み飛ばす
					} else {
						currentMaterial->bumpTexFilePath = directoryPath + "/" + token;
						break;
					}
				}
			}
		}
	}

	return materialMap;
}

//======================================================================================================
// メッシュのCPU頂点を GPU常駐バッファへ転送する（ロード時1回だけ）
//======================================================================================================
void ModelManager::MakeMeshBuffer(SubMesh& mesh) {
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