#include "MyEngine/Render/ModelManager.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Render/DirectXCommon.h"
#include "MyEngine/Render/RenderContext.h"
#include "MyEngine/Render/ShaderStructs.h"
#include "MyEngine/Render/TextureManager.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>

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

//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
uint32_t ModelManager::Load(const std::string& objFilePath) {
	auto& inst = GetInstance();

	// ===== 重複チェック =====
	auto cached = inst.pathToHandle_.find(objFilePath);
	if (cached != inst.pathToHandle_.end()) {
		return cached->second;
	}

	// ===== ディレクトリパスとファイル名を分離 =====
	std::filesystem::path path(objFilePath);
	std::string directoryPath = path.parent_path().string();
	std::string filename = path.filename().string();

	// ===== OBJ読み込み =====
	ModelData modelData = LoadObjFile(directoryPath, filename);

	// ===== マテリアルのテクスチャをTextureManagerでロード =====
	for (auto& [name, mat] : modelData.materialMap) {
		if (!mat.textureFilePath.empty()) {
			mat.srvIndex = TextureManager::Load(mat.textureFilePath);
		}
	}

	// ===== ハンドル割り当てと登録 =====
	uint32_t handle = inst.modelsKey_++;
	inst.models_[handle] = std::move(modelData);
	inst.pathToHandle_[objFilePath] = handle;

	return handle;
}

// ===== 描画リクエストの追加 =====
void ModelManager::DrawModel(const ModelConfig& config) { GetInstance().requests_.push_back(config); }

//======================================================================================================
// 描画リクエストをすべて発行する
//======================================================================================================
void ModelManager::Flush3d(const std::wstring& windowTitle) {
	auto& inst = GetInstance();

	for (const ModelConfig& req : inst.requests_) {
		if (req.windowTitle != windowTitle && req.windowTitle != L"")
			continue;
		if (req.shadingModel != ShadingModel::Unlit) {
			assert(req.directionalLight != nullptr && "ShadingModel::Unlit以外には光源を設置してください");
		}

		auto modelIt = inst.models_.find(req.handle);
		if (modelIt == inst.models_.end())
			continue;
		const ModelData& modelData = modelIt->second;

		// ===== 色変換（0xRRGGBBAA → float4）=====
		float r = static_cast<float>((req.color >> 24) & 0xFF) / 255.0f;
		float g = static_cast<float>((req.color >> 16) & 0xFF) / 255.0f;
		float b = static_cast<float>((req.color >> 8) & 0xFF) / 255.0f;
		float a = static_cast<float>(req.color & 0xFF) / 255.0f;

		// ===== WVP・ワールド行列を計算 =====
		Matrix4x4 worldMatrix = MakeAffineMatrix(req.transform.scale, req.transform.rotation, req.transform.translation);
		Matrix4x4 wvpMatrix = req.camera ? req.camera->CalcWVP(worldMatrix) : worldMatrix;

		// ===== 各メッシュを描画 =====
		for (const MeshData& mesh : modelData.meshes) {
			// 連番インデックス生成（頂点がそのまま連番なのでインデックスも連番）
			std::vector<uint32_t> indices(mesh.vertices.size());
			for (uint32_t i = 0; i < static_cast<uint32_t>(mesh.vertices.size()); ++i) {
				indices[i] = i;
			}

			// ===== マテリアルを取得 =====
			const MaterialData* mat = nullptr;
			auto matIt = modelData.materialMap.find(mesh.materialName);
			if (matIt != modelData.materialMap.end()) {
				mat = &matIt->second;
			}

			// ===== カメラを取得 =====
			CameraDataCB camCB;
			camCB.worldPosition = req.camera ? req.camera->GetTranslation() : Vector3{0.0f, 0.0f, 0.0f};

			// ===== ModelMaterialCBを構築 =====
			ModelMaterialCB matCB;
			const MaterialOverride& ov = req.materialOverride;
			matCB.color = {r, g, b, a};
			matCB.uvTransform = MakeUVTransformMatrix(req.uvTransform);
			if (mat) {
				matCB.ambient = mat->ambient;
				matCB.diffuse = mat->diffuse;
				matCB.specular = mat->specular;
				matCB.shininess = mat->shininess;
				matCB.emissive = mat->emissive;
				matCB.color.w *= mat->dissolve;
			} else {
				// マテリアルが見つからない場合のデフォルト値
				matCB.ambient = {0.2f, 0.2f, 0.2f};
				matCB.diffuse = {1.0f, 1.0f, 1.0f};
				matCB.specular = {0.0f, 0.0f, 0.0f};
				matCB.shininess = 32.0f;
				matCB.emissive = {0.0f, 0.0f, 0.0f};
			}

			// overrideフラグが立っている項目だけ上書き
			if (ov.overrideAmbient) {
				matCB.ambient = ov.ambient;
			}
			if (ov.overrideDiffuse) {
				matCB.diffuse = ov.diffuse;
			}
			if (ov.overrideSpecular) {
				matCB.specular = ov.specular;
			}
			if (ov.overrideShininess) {
				matCB.shininess = ov.shininess;
			}
			if (ov.overrideEmissive) {
				matCB.emissive = ov.emissive;
			}
			if (ov.overrideDissolve) {
				matCB.color.w = ov.dissolve;
			}

			// ShadingModel::Unlitのときは diffuseとambientを白にする
			if (req.shadingModel == ShadingModel::Unlit) {
				matCB.diffuse = {1.0f, 1.0f, 1.0f};
				matCB.ambient = {1.0f, 1.0f, 1.0f};
			}

			RenderContext::DrawModelDesc desc;
			desc.vertices = mesh.vertices;
			desc.indices = indices;
			desc.material = matCB;
			desc.matrices.wvpMatrix = wvpMatrix;
			desc.matrices.worldMatrix = worldMatrix;
			desc.cameraData = camCB;
			desc.material.textureIndex = (req.srvHandle != 0) ? req.srvHandle : (mat ? mat->srvIndex : 0);
			desc.directionalLight = req.directionalLight;
			RenderContext::SetShadingModel(req.shadingModel);
			RenderContext::DrawModel(desc);
		}
	}
}

// ===== 描画リクエストをクリア =====
void ModelManager::ClearRequests() { GetInstance().requests_.clear(); }

//======================================================================================================
// OBJファイルを読み込む
//======================================================================================================
ModelManager::ModelData ModelManager::LoadObjFile(const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;
	std::vector<Vector4> positions; // 頂点位置
	std::vector<Vector3> normals;   // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open() && "OBJファイルが開けませんでした");

	MeshData* currentMesh = nullptr;

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
			modelData.materialMap = LoadMaterialTemplateFile(directoryPath, mtlFilename);

		} else if (identifier == "usemtl") {
			// ===== マテリアルが変わったら新しいメッシュを開始 =====
			std::string materialName;
			s >> materialName;
			modelData.meshes.push_back(MeshData{});
			currentMesh = &modelData.meshes.back();
			currentMesh->materialName = materialName;
			vertexSmoothGroups.push_back({});

		} else if (identifier == "f") {
			if (!currentMesh) {
				modelData.meshes.push_back(MeshData{});
				currentMesh = &modelData.meshes.back();
				vertexSmoothGroups.push_back({});
			}
			// 頂点を全部読む
			std::vector<VertexData3D> faceVertices;
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
	for (size_t meshIdx = 0; meshIdx < modelData.meshes.size(); ++meshIdx) {
		MeshData& mesh = modelData.meshes[meshIdx];
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

	return modelData;
}

//======================================================================================================
// MTLファイルを読み込む
//======================================================================================================
std::map<std::string, ModelManager::MaterialData> ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {

	std::map<std::string, MaterialData> materialMap;
	MaterialData* currentMaterial = nullptr;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open() && "MTLファイルが開けませんでした");

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
			materialMap[materialName] = MaterialData{};
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