#include "MyEngine/Particle/ParticleManager.h"

#include "MyEngine/Diagnostics/MyAssert.h";
#include "MyEngine/Diagnostics/LogManager.h";
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
	// 
}