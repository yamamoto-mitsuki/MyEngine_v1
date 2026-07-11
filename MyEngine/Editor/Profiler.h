#pragma once
#include <format>
#include <string>
#include <vector>
#include <type_traits>

namespace ProfCategory {
inline const std::string CPU = "CPU";
inline const std::string GPU = "GPU";
inline const std::string Memory = "Memory";
inline const std::string Physics = "Physics";
inline const std::string Frame = "Frame";
}; // namespace ProfCategory


/// <summary>
/// Profilerに登録、表示
/// </summary>
class Profiler {
public:
	// --- 名前登録時に間違えないようにするためのキー ---
	// CPUグループ
	struct CPUGroup {
		static inline std::string Times = "Times";
	};
	// --- 表示方法 ---
	enum class DisplayType {
		Value, // 数値表示
		Bar,   // バー表示
		Plot   // グラフ表示
	};

	// --- 表示項目 ---
	struct Entry {
		std::string label;                            // 名前
		std::string text;                             // Value用の表示文字列
		std::string unit;                             // ms/MB などの単位
		float value = 0.0f;                           // Bar/Plotの現在地
		float maxValue = 1.0f;                        // Bar/Plotの上限
		std::vector<float> history;                   // Plot用の履歴
		DisplayType displayType = DisplayType::Value; // 表示タイプ
	};
	// グループ情報
	struct GroupData {
		std::string name;
		std::vector<Entry> entries;
	};
	// カテゴリ情報
	struct CategoryData {
		std::string name;
		std::vector<GroupData> groups;
	};

	// ===== グループ =====
	class GroupProxy {
	public:
		explicit GroupProxy(GroupData* g) : groupData_(g) {};

		GroupProxy& Text(const std::string& label, const std::string& value);
		GroupProxy& Bar(const std::string& label, float value, float maxValue, const std::string& unit = "");
		GroupProxy& Plot(const std::string& label, float value, float maxValue = 0.0f);

		template<class T> GroupProxy& Value(const std::string& label, T v, const std::string& uint = "") {
			std::string s = std::is_floating_point_v<T> ? std::format("{:.2f}", double(v)) : std::to_string(v);
			if (!uint.empty()) {
				s += " " + uint;
			}
			return Text(label, s);
		}

	private:
		Entry& FindOrAdd(const std::string& label);
		GroupData* groupData_;
	};

	// ===== カテゴリ =====
	class CategoryProxy {
	public:
		explicit CategoryProxy(CategoryData* c) : categoryData_(c) {}
		GroupProxy Group(const std::string& name);

		// --- グループ無しで直接書く（内部で名前なしの既定グループへ）---
		template<class T> 
		CategoryProxy& Value(const std::string& label, T v, const std::string& unit = "") {
			Group("").Value(label, v, unit);
			return *this;
		}
		CategoryProxy& Text(const std::string& label, const std::string& value) {
			Group("").Text(label, value);
			return *this;
		}
		CategoryProxy& Bar(const std::string& label, float v, float maxV, const std::string& unit = "") {
			Group("").Bar(label, v, maxV, unit);
			return *this;
		}
		CategoryProxy& Plot(const std::string& label, float v, float maxV = 0.0f) {
			Group("").Plot(label, v, maxV);
			return *this;
		}

	private:
		CategoryData* categoryData_;
	};

	static CategoryProxy Category(const std::string& name);
	static void Draw(bool* open = nullptr);
	static void Clear();

private:
	static inline std::vector<CategoryData> categories_;
};
