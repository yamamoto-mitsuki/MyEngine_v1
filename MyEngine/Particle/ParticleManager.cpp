#include "MyEngine/Particle/ParticleManager.h"

#include <numbers>

#include "MyEngine/Diagnostics/MyAssert.h";
#include "MyEngine/Diagnostics/LogManager.h";
#include "MyEngine/Time/Time.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
ParticleManager* ParticleManager::instance_ = nullptr;


//=============================================================================
// 初期化
//=============================================================================
void ParticleManager::Initialize() { 
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new ParticleManager(); 

	// ===== StructuredBuffer(Resource) =====
	instance_->instanceBuffer_ = DirectXCommon::CreateUploadBuffer(sizeof(ParticleData) * kMaxParticles);
	instance_->instanceBuffer_->SetName(L"ParticleManager_InstanceBuffer");
	instance_->instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&instance_->mappedInstances_));

	// ===== SRV =====
	// Desc
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = kMaxParticles;
	srvDesc.Buffer.StructureByteStride = sizeof(ParticleData);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	// 作成
	instance_->srvSlot_ = DirectXCommon::AllocateSRVSlot();
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = DirectXCommon::GetCPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(), 
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, instance_->srvSlot_);
	instance_->srvHandleGPU_ = DirectXCommon::GetGPUDescriptorHandle(DirectXCommon::GetSRVDescriptorHeap(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, instance_->srvSlot_);
	DirectXCommon::GetDevice()->CreateShaderResourceView(instance_->instanceBuffer_.Get(), &srvDesc, cpu);

	// ===== 全パーティクル共有のQuad =====
	const VertexParticleData vertices[4] = {
	    {{-0.5f, +0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 左上
	    {{+0.5f, +0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 右上
	    {{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
	    {{+0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
	};
	const uint32_t indices[6] = {0, 1, 2, 1, 3, 2};
	// 頂点バッファ
	instance_->vertexBuffer_ = DirectXCommon::CreateUploadBuffer(sizeof(vertices));
	instance_->vertexBuffer_->SetName(L"ParticleManager_VB");
	void* mapped = nullptr;
	instance_->vertexBuffer_->Map(0, nullptr, &mapped);
	memcpy(mapped, vertices, sizeof(vertices));
	instance_->vertexBuffer_->Unmap(0, nullptr);
	instance_->vbv_.BufferLocation = instance_->vertexBuffer_->GetGPUVirtualAddress();
	instance_->vbv_.SizeInBytes = sizeof(vertices);
	instance_->vbv_.StrideInBytes = sizeof(VertexParticleData);
	// インデックスバッファ
	instance_->indexBuffer_ = DirectXCommon::CreateUploadBuffer(sizeof(indices));
	instance_->indexBuffer_->SetName(L"ParticleManager_IB");
	instance_->indexBuffer_->Map(0, nullptr, &mapped);
	memcpy(mapped, indices, sizeof(indices));
	instance_->indexBuffer_->Unmap(0, nullptr);
	instance_->ibv_.BufferLocation = instance_->indexBuffer_->GetGPUVirtualAddress();
	instance_->ibv_.SizeInBytes = sizeof(indices);
	instance_->ibv_.Format = DXGI_FORMAT_R32_UINT;

	LogManager::Log("Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void ParticleManager::Release() {
	if (!instance_) {
		return;
	}

	if (instance_->mappedInstances_) {
		instance_->instanceBuffer_->Unmap(0, nullptr);
	}
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// パーティクルを1個登録
//=============================================================================
void ParticleManager::Register(const Particle& p) { 
	MY_ASSERT_MSG(instance_, "Initialize()を先に読んでください");
	instance_->particles_.push_back(p);
}


//=============================================================================
// 更新
//=============================================================================
void ParticleManager::Update() { 
	auto& inst = *instance_;
	const float deltaTime = Time::GetDeltaTime();
	// カメラ未設定のとき
	if (!inst.camera_) {
		inst.instanceCount_ = 0;
		LogManager::Warning("Camera Not Found");
		return;
	}

	// ビルボード行列は全粒共通なのでループの外で1回だけ作る
	// カメラのワールド行列 = Viewの逆行列。平行移動を消して回転だけ残す
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
	Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, Inverse(inst.camera_->GetViewMatrix()));
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	// ===== ル―プして更新 =====
	inst.instanceCount_ = 0;
	for (std::list<Particle>::iterator p = inst.particles_.begin(); p != inst.particles_.end();) {
		// バッファ容量を超えた分は描画しない
		if (inst.instanceCount_ >= kMaxParticles) {
			LogManager::Warning("particles >= kMaxParticles");
			break;
		}

		// 時間を進め、寿命切れは削除
		(*p).currentTime += deltaTime;
		if ((*p).lifeTime <= (*p).currentTime) {
			p = inst.particles_.erase(p);
			continue;
		}

		// --- Particle更新 ---
		(*p).transform.translation += (*p).velocity * deltaTime; // 座標更新
		Matrix4x4 world = MathUtility::MakeScaleMatrix(p->transform.scale) * billboardMatrix * MathUtility::MakeTranslateMatrix(p->transform.translation);

		// --- Mapに書き込み ---
		ParticleData& data = inst.mappedInstances_[inst.instanceCount_];
		data.wvp = inst.camera_ ? inst.camera_->CalcWVP(world) : world;
		data.world = world;
		data.color = (*p).color;
		data.color.w = 1.0f - ((*p).currentTime / (*p).lifeTime);
		++inst.instanceCount_;
	}
}

//=============================================================================
// 描画
//=============================================================================
void ParticleManager::Draw() { 
	auto& inst = *instance_;
	
	// 早期リターン
	if (inst.instanceCount_ == 0) {
		return;
	}


}