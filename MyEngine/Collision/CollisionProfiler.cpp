#include "CollisionProfiler.h"
#include "MyEngine/Debug/MyAssert.h"
#include "MyEngine/Debug/ImGuiManager.h"
#include "MyEngine/Log/LogManager.h"

// 静的メンバ変数
CollisionProfiler *CollisionProfiler::instance_;

void CollisionProfiler::Initialize() { 
	MY_ASSERT_MSG(instance_ == nullptr, "2回以上初期化されています");
	instance_ = new CollisionProfiler();
}

void CollisionProfiler::Release() { 
	MY_ASSERT_MSG(instance_ != nullptr, "2回以上初期化されています");
	delete instance_; 
	instance_ = nullptr;
}

void CollisionProfiler::Record(const char* funcName, double elapsedMs) {
	auto& s = instance_->stats_[funcName];
	s.callCount++;
	s.totalMs += elapsedMs;
	instance_->totalCalls_++;
}

void CollisionProfiler::Reset() { 
	instance_->stats_.clear();
	instance_->totalCalls_ = 0;
}

uint64_t CollisionProfiler::GetTotalCalls() { return instance_->totalCalls_; }

const std::unordered_map<std::string, CollisionProfiler::FunctionStats>& CollisionProfiler::GetAllStats() { return instance_->stats_; }

#ifdef USE_IMGUI
void CollisionProfiler::DebugPrint() {
	for (const auto& [name, s] : instance_->stats_) {
		char buf[64];
		// 変換
		snprintf(buf, sizeof(buf), "%llu calls  avg:%.4f ms", static_cast<unsigned long long>(s.callCount), s.AvgMs());
		ImGuiManager::SetDebugValue(DebugCategory::Collision, name, std::string(buf));
	}
	ImGuiManager::SetDebugValue(DebugCategory::Collision, "Total", instance_->totalCalls_);
}
#endif