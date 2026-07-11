#include "MyEngine/Editor/Profiler.h"

#include <cfloat>
#include <algorithm>

#include <externals/imgui/imgui.h>

//=============================================================================
// カテゴリ
//=============================================================================
Profiler::CategoryProxy Profiler::Category(const std::string& name) {
	for (auto& c : categories_) {
		if (c.name == name) {
			return CategoryProxy(&c);
		}
	}
	categories_.push_back({name, {}});
	return CategoryProxy(&categories_.back());
}

//=============================================================================
// グループ
//=============================================================================
Profiler::GroupProxy Profiler::CategoryProxy::Group(const std::string& name) {
	for (auto& g : categoryData_->groups) {
		if (g.name == name) {
			return GroupProxy(&g);
		}
	}
	categoryData_->groups.push_back({name, {}});
	return GroupProxy(&categoryData_->groups.back());
}

//=============================================================================
// 
//=============================================================================
Profiler::Entry& Profiler::GroupProxy::FindOrAdd(const std::string& label) { 
	for (auto& e : groupData_->entries) {
		if (e.label == label) {
			return e;
		}
	}
	groupData_->entries.push_back(Entry{});
	groupData_->entries.back().label = label;
	return groupData_->entries.back();
}

//=============================================================================
// 
//=============================================================================
Profiler::GroupProxy& Profiler::GroupProxy::Text(const std::string& label, const std::string& value) {
	Entry& e = FindOrAdd(label);
	e.displayType = DisplayType::Value;
	e.text = value;
	return *this;
}

//=============================================================================
//
//=============================================================================
Profiler::GroupProxy& Profiler::GroupProxy::Bar(const std::string& label, float value, float maxValue, const std::string& unit) {
	Entry& e = FindOrAdd(label);
	e.displayType = DisplayType::Bar;
	e.value = value;
	e.maxValue = maxValue;
	e.unit = unit;
	return *this;
}

//=============================================================================
//
//=============================================================================
Profiler::GroupProxy& Profiler::GroupProxy::Plot(const std::string& label, float value, float maxValue) {
	Entry& e = FindOrAdd(label);
	e.displayType = DisplayType::Plot;
	e.value = value;
	e.maxValue = maxValue;
	e.history.push_back(value);
	// 直近180フレームのみ表示
	if (e.history.size() > 180) {
		e.history.erase(e.history.begin());
	}
	return *this;
}

//=============================================================================
//
//=============================================================================
void Profiler::Clear() { categories_.clear(); }

// ===== 表示用 =====
namespace {
	ImVec4 BarColor(float frac){
	return frac < 0.8f ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f)  // 緑
	     : frac < 1.0f ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)  // 黄
	                   : ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // 赤
    }

	void DrawEntry(const Profiler::Entry& e) {
		// 表示形式
	    switch (e.displayType) {
		// 数値
	    case Profiler::DisplayType::Value:
		    ImGui::Text("%-16s %s", e.label.c_str(), e.text.c_str());
		    break;
		// バー
	    case Profiler::DisplayType::Bar: {
		    float frac = (e.maxValue > 0.0f) ? e.value / e.maxValue : 0.0f;
		    char buf[64];
		    snprintf(buf, sizeof(buf), "%.2f %s", e.value, e.unit.c_str());
		    ImGui::Text("%-12s", e.label.c_str());
		    ImGui::SameLine();
		    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor(frac));
		    ImGui::ProgressBar(std::min(frac, 1.0f), ImVec2(-1, 0), buf);
		    ImGui::PopStyleColor();
		    break;
	    }
		// グラフ
	    case Profiler::DisplayType::Plot: {
		    float frac = (e.maxValue > 0.0f) ? e.value / e.maxValue : 0.0f;
		    ImGui::Text("%-16s %.2f", e.label.c_str(), e.value);
		    ImGui::PushStyleColor(ImGuiCol_PlotLines, BarColor(frac));
		    ImGui::PlotLines(("##" + e.label).c_str(), e.history.data(), static_cast<int>(e.history.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(-1, 50));
		    ImGui::PopStyleColor();
		    break;
	    }
	    }
	}
} // namespace

//=============================================================================
// 描画
//=============================================================================
void Profiler::Draw(bool* open) {
	// --- 親ハブ + 内部専用ドックスペース ---
	ImGui::Begin("Profiler", open);
	ImGuiID dockId = ImGui::GetID("ProfilerDockSpace");
	ImGui::DockSpace(dockId, ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_None);
	ImGui::End();

	// ===== カテゴリごとに子ウィンドウ） =====
	for (auto& cat : categories_) {
		ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
		ImGui::Begin(cat.name.c_str()); // "CPU" などの窓を開く

		// --- そのカテゴリ内のグループを回す ---
		for (auto& grp : cat.groups) {
			if (grp.name.empty()) {
				// 名前なしグループ = 見出し無しで直接並べる
				for (auto& e : grp.entries) {
					DrawEntry(e);
				}
			} else if (ImGui::TreeNodeEx(grp.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				// 名前ありグループ = Treeで折りたためる
				for (auto& e : grp.entries) {
					DrawEntry(e);
				}
				ImGui::TreePop();
			}
		}

		ImGui::End(); // 子ウィンドウを閉じる
	}
}