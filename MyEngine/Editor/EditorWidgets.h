#pragma once
#include <string>
#include <type_traits>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>


/// <summary>
/// Hierarchy・Inspectorなど、エディタのウィンドウで共通に使うImGuiの部品
/// <para>ImGuiManagerは USE_IMGUI の中にしか無いので、Releaseでもコンパイルされるウィンドウから使える場所に置く</para>
/// </summary>
namespace EditorWidgets {

// std::string の長さを、ImGuiが必要な分だけ伸ばす（ImGui公式の misc/cpp/imgui_stdlib と同じ仕組み）
inline int ResizeStringCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		auto* text = static_cast<std::string*>(data->UserData);
		text->resize(static_cast<size_t>(data->BufTextLen));
		data->Buf = text->data();
	}
	return 0;
}

/// <summary>
/// std::string をそのまま編集する入力欄。長さの上限が無いので、日本語でも途中で切れない
/// </summary>
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags | ImGuiInputTextFlags_CallbackResize, ResizeStringCallback, &text);
}

/// <summary>
/// enumをコンボボックスで選ばせる。選択肢の名前はmagic_enumが型から作る
/// </summary>
template<typename E> bool EnumCombo(const char* label, E& value) {
	static_assert(std::is_enum_v<E>, "EnumCombo は enum 専用です");
	constexpr auto names = magic_enum::enum_names<E>();
	size_t current = magic_enum::enum_index(value).value_or(0);
	bool changed = false;
	std::string currentName(names[current]);
	if (ImGui::BeginCombo(label, currentName.c_str())) {
		for (size_t i = 0; i < names.size(); ++i) {
			std::string name(names[i]);
			bool isSelected = (i == current);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
				value = magic_enum::enum_value<E>(i);
				changed = true;
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus(); // 開いたときに今の選択へスクロールする
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

} // namespace EditorWidgets