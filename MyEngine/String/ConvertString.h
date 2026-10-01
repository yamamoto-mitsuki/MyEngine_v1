#pragma once
#include <string>
#include <filesystem>

// string->wstring
std::wstring ConvertString(const std::string& str);
// wstring->string
std::string ConvertString(const std::wstring& str);
// パス → UTF-8 の文字（区切りは /）。ImGui・ログ・シーンファイルに出すパスはこれで作る
// path::string() / generic_string() は、日本語の Windows では Shift-JIS になる（C++26 で非推奨になった理由もこれ）
std::string ToUtf8(const std::filesystem::path& path);