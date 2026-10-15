#pragma once
#include <string>
#include <filesystem>

// string->wstring
std::wstring ConvertString(const std::string& str);
// wstring->string
std::string ConvertString(const std::wstring& str);

// パス → UTF-8 の文字（区切りは /）。ImGui・ログ・シーンファイルに出すパスはこれで作る
std::string ToUtf8(const std::filesystem::path& path);

// UTF-8 の文字 → パス。ImGui・シーンファイル・ログの文字（UTF-8）を path にするときはこれを使う
std::filesystem::path ToPath(const std::string& utf8);
// パス → 読み込みのキャッシュのキー
std::string ToPathKey(const std::filesystem::path& path);