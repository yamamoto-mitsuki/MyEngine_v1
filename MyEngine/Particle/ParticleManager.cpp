#include "MyEngine/Particle/ParticleManager.h"

#include <numbers>

#include "MyEngine/Diagnostics/MyAssert.h";
#include "MyEngine/Diagnostics/LogManager.h";
#include "MyEngine/Time/Time.h"
#include "MyEngine/Camera/Camera.h"
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

	LogManager::Log("Initialized");
}


//=============================================================================
// 更新
//=============================================================================
void ParticleManager::Update() { 
	auto& inst = *instance_; 
	const float deltaTime = Time::GetDeltaTime();
	
	// ===== ル―プして更新 =====
	inst.instanceCount_ = 0;
	for (std::list<Particle>::iterator p = inst.particles_.begin(); p != inst.particles_.end(); ++p) {
		// バッファ容量を超えた分は描画しない
		if (inst.instanceCount_ >= kMaxParticles) {
			LogManager::Warning("particles >= kMaxParticles");
			break;
		}
		// 生存期間の過ぎたものを消す
		if ((*p).lifeTime <= (*p).currentTime) {
			p = inst.particles_.erase(p);
			continue;
		}

		// --- Particle更新 ---
		(*p).currentTime += deltaTime; // 時間増加
		(*p).transform.translation += (*p).velocity * deltaTime; // 座標更新                                          // 座標更新
		// ビルボード計算
		Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
		Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, inst.camera_->GetViewMatrix());
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
		// ワールド座標に
		Matrix4x4 world = MathUtility::MakeScaleMatrix((*p).transform.scale) * billboardMatrix * MathUtility::MakeTranslateMatrix((*p).transform.translation);

		// --- Mapに書き込み ---
		ParticleData& data = inst.mappedInstances_[inst.instanceCount_];
		data.wvp = inst.camera_ ? inst.camera_->CalcWVP(world) : world;
		data.world = world;
		data.color = (*p).color;
		data.color.w = 1.0f - ((*p).currentTime / (*p).lifeTime);
		++inst.instanceCount_;
	}
}