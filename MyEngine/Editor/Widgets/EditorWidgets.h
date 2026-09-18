#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/Widgets/ImeInput.h"

/// <summary>
/// Hierarchy・Inspectorなど、エディタのウィンドウで共通に使うImGuiの部品
/// <para>ImGuiManagerは USE_IMGUI の中にしか無いので、Releaseでもコンパイルされるウィンドウから使える場所に置く</para>
/// </summary>
namespace EditorWidgets {

// std::string版InputTextのコールバック
inline int InputTextCallback(ImGuiInputTextCallbackData* data) {
	// 文字が増えて入れ物が足りなくなったら、std::stringを伸ばして付け替える（ImGui公式の misc/cpp/imgui_stdlib と同じ仕組み）
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		auto* text = static_cast<std::string*>(data->UserData);
		text->resize(static_cast<size_t>(data->BufTextLen));
		data->Buf = text->data();
	}
	// 日本語の変換が始まったら、選んでいた文字を先に消す（メモ帳などと同じ。変換中の文字がその場所に出る）
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
		if (ImeInput::IsComposing() && data->HasSelection()) {
			const int start = (std::min)(data->SelectionStart, data->SelectionEnd);
			data->DeleteChars(start, std::abs(data->SelectionEnd - data->SelectionStart));
		}
	}
	return 0;
}

/// <summary>
/// std::string をそのまま編集する入力欄。長さの上限が無いので、日本語でも途中で切れない
/// </summary>
inline bool InputText(const char* label, std::string& text, ImGuiInputTextFlags flags = 0) {
	flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways;
	return ImGui::InputText(label, text.data(), text.capacity() + 1, flags, InputTextCallback, &text);
}

/// <summary>
/// 大きな「＋」付きのボタン。labelの「##」より前が＋の右に出る文字（"##create" なら＋だけ）
/// <para>sizeの考え方はImGui::Buttonと同じ（0なら中身に合わせる、-FLT_MINなら横幅いっぱい）</para>
/// </summary>
inline bool PlusButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f)) {
	const ImGuiStyle& style = ImGui::GetStyle();
	const char* textEnd = std::strstr(label, "##");
	if (!textEnd) {
		textEnd = label + std::strlen(label);
	}
	const bool hasText = (textEnd != label);
	const float iconSize = ImGui::GetFontSize() * 0.7f; // ＋の縦横の長さ
	if (size.x == 0.0f) {
		const float textWidth = hasText ? style.ItemInnerSpacing.x + ImGui::CalcTextSize(label, textEnd).x : 0.0f;
		size.x = style.FramePadding.x * 2.0f + iconSize + textWidth;
	}

	// ボタンそのものは文字無しで作り、上に＋と文字を自分で描く
	ImGui::PushID(label);
	const bool pressed = ImGui::Button("##plusButton", size);
	ImGui::PopID();

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
	const float half = iconSize * 0.5f;
	const float thickness = (std::max)(2.0f, ImGui::GetFontSize() * 0.12f); // 線の太さ（文字より太くして目立たせる）
	// 文字があれば左寄せ、無ければ真ん中に＋を描く
	const float centerX = hasText ? min.x + style.FramePadding.x + half : (min.x + max.x) * 0.5f;
	const float centerY = (min.y + max.y) * 0.5f;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddLine(ImVec2(centerX - half, centerY), ImVec2(centerX + half, centerY), color, thickness);
	drawList->AddLine(ImVec2(centerX, centerY - half), ImVec2(centerX, centerY + half), color, thickness);
	if (hasText) {
		const ImVec2 textPos(centerX + half + style.ItemInnerSpacing.x, centerY - ImGui::GetFontSize() * 0.5f);
		drawList->AddText(textPos, color, label, textEnd);
	}
	return pressed;
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