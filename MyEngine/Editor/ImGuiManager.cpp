#define NOMINMAX
#ifdef USE_IMGUI
#include "MyEngine/Editor/ImGuiManager.h"

#include <cstdio>
#include <format>

#include <externals/imgui/imgui.h>
#include <externals/imgui/imgui_impl_dx12.h>
#include <externals/imgui/imgui_impl_win32.h>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Editor/Profiler.h"
#include "MyEngine/UI/GlobalVariables.h"
#include "MyEngine/Window/Win32Window.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
ImGuiManager* ImGuiManager::instance_ = nullptr;


namespace {
	// バーの色をつける
	void DrawProfilerBar(const char* label, float ms, float budgetMs) { 
		float frac = (budgetMs > 0.0f) ? ms / budgetMs : 0.0f;
	    ImVec4 col = (frac < 0.8f)     ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f)  // 緑：余裕
	               : (frac < 1.0f)     ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)  // 黄：ギリ
	                                   : ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // 赤：超過
	    char buf[32];
	    snprintf(buf, sizeof(buf), "%.3f ms", ms);
	    ImGui::Text("%-11s", label);
	    ImGui::SameLine();
	    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
	    ImGui::ProgressBar(std::min(frac, 1.0f), ImVec2(-1, 0), buf); // -1=残り幅いっぱい
	    ImGui::PopStyleColor();
	}
} // namespace

//=============================================================================
// 初期化
//=============================================================================
void ImGuiManager::Initialize(Win32Window* window) {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回呼んでいます");
	instance_ = new ImGuiManager();
	instance_->hwnd_ = window->GetHWND();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	// Docking有効化
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// アプリ外にマウスカーソルを飛ばさない
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	// ウィンドウ配置をimgui.iniに自動保存
	io.IniFilename = "imgui.ini";

	// 日本語フォントの読み込み
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	ImGui_ImplWin32_Init(window->GetHWND());
	// SRVDescriptorHeapのスロット0をImGui用に使う
	ID3D12DescriptorHeap* srvHeap = DirectXCommon::GetSRVDescriptorHeap();
	ImGui_ImplDX12_Init(
	    DirectXCommon::GetDevice(), DirectXCommon::kSwapChainBufferCount, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap, srvHeap->GetCPUDescriptorHandleForHeapStart(),
	    srvHeap->GetGPUDescriptorHandleForHeapStart());
	io.Fonts->Build();

	// 前回保存したスタイルを自動で読み込む（ファイルがなければスキップ）
	instance_->LoadStyle("imgui_style.ini");

	LogManager::Log("Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void ImGuiManager::Release() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にRelease()が呼ばれています");
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}

//=============================================================================
// フレームの開始処理
//=============================================================================
void ImGuiManager::Begin() {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	// DockSpace自体がDockされるのを防ぐ
	ImGuiWindowFlags dockFlags = ImGuiWindowFlags_MenuBar |    // メニューバーを表示
	                             ImGuiWindowFlags_NoDocking |  // DockSpace自体はDockされない
	                             ImGuiWindowFlags_NoTitleBar | // タイトルバーなし
	                             ImGuiWindowFlags_NoCollapse | // 折りたたみなし
	                             ImGuiWindowFlags_NoResize |   // リサイズなし（画面全体に合わせる）
	                             ImGuiWindowFlags_NoMove |     // 移動なし
	                             ImGuiWindowFlags_NoNavFocus;  // ナビゲーションフォーカスなし
	// 画面全体に広げる
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpaceWindow", nullptr, dockFlags);
	ImGui::PopStyleVar(3);

	// ===== メニューバー =====
	if (ImGui::BeginMenuBar()) {
		// --- File --- 
		if (ImGui::BeginMenu("File")) {
			// jsonから全シーンデータを再読み込み
			if (ImGui::MenuItem("Reload All Variables")) {
				GlobalVariables::GetInstance()->LoadFiles();
			}
			// ImGuiウィンドウ配置をimgui.iniに書き出し
			if (ImGui::MenuItem("Save Layout")) {
				ImGui::SaveIniSettingsToDisk("imgui.ini");
			}
			ImGui::EndMenu();
		}

		// --- Edit ---
		if (ImGui::BeginMenu("Edit")) {
			ImGui::EndMenu();
		}

		// --- Setting ---
		if (ImGui::BeginMenu("Setting")) {
			// 色・サイズ・角丸等を自由に調整できる。
			if (ImGui::BeginMenu("Style")) {
				ImGui::ShowStyleEditor();
				ImGui::EndMenu();
			}
			// フォントの切り替えとテーマプリセットの選択ができる。
			if (ImGui::BeginMenu("Font")) {
				ImGui::ShowFontSelector("Font");
				ImGui::ShowStyleSelector("Theme Preset");
				ImGui::EndMenu();
			}

			ImGui::Separator(); // 横線

			// 現在のスタイル（色・Alpha・サイズ系）を imgui_style.ini に保存
			if (ImGui::MenuItem("Save Style")) {
				instance_->SaveStyle("imgui_style.ini");
			}
			// imgui_style.ini からスタイルを読み込んで即時反映
			if (ImGui::MenuItem("Load Style")) {
				instance_->LoadStyle("imgui_style.ini");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// DockSpaceを登録する
	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	ImGui::End();

	// ===== Logの出力 =====
	LogManager::DrawImGuiWindow();

	// ===== 調整項目の更新 =====
	GlobalVariables::GetInstance()->Update();

	// ===== プロファイラの描画 =====
	Profiler::Draw();
}

//=============================================================================
// 描画リクエストの追加
//=============================================================================
void ImGuiManager::AddDrawRequest(std::function<void()> drawFunc) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	instance_->requests_.push_back(std::move(drawFunc));
}

//=============================================================================
// 積まれた描画リクエストを全て実行する
//=============================================================================
void ImGuiManager::ProcessRequests() {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	for (const auto& func : instance_->requests_) {
		func();
	}
}

//=============================================================================
// 描画データを確定させる
//=============================================================================
void ImGuiManager::Render() {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	ImGui::Render();
}

//=============================================================================
// 確定した描画データをコマンドリストに積む
//=============================================================================
void ImGuiManager::RenderDrawData() {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	ID3D12DescriptorHeap* ppHeaps[] = {DirectXCommon::GetSRVDescriptorHeap()};
	DirectXCommon::GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectXCommon::GetCommandList());
}

//=============================================================================
// 全リクエストをクリアする
//=============================================================================
void ImGuiManager::ClearRequests() {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	instance_->requests_.clear();
}

//=============================================================================
// スタイルをiniファイルに保存
//=============================================================================
void ImGuiManager::SaveStyle(const std::string& path) {
	ImGuiStyle& s = ImGui::GetStyle();
	FILE* f = nullptr;
	fopen_s(&f, path.c_str(), "wt");
	if (!f) {
		LogManager::Warning("ファイルを開けませんでした: " + path);
		return;
	}
	// Alpha
	fprintf(f, "Alpha=%.3f\n", s.Alpha);
	// サイズ系パラメータ
	fprintf(f, "WindowPaddingX=%.3f\n", s.WindowPadding.x);
	fprintf(f, "WindowPaddingY=%.3f\n", s.WindowPadding.y);
	fprintf(f, "WindowRounding=%.3f\n", s.WindowRounding);
	fprintf(f, "WindowBorderSize=%.3f\n", s.WindowBorderSize);
	fprintf(f, "FramePaddingX=%.3f\n", s.FramePadding.x);
	fprintf(f, "FramePaddingY=%.3f\n", s.FramePadding.y);
	fprintf(f, "FrameRounding=%.3f\n", s.FrameRounding);
	fprintf(f, "FrameBorderSize=%.3f\n", s.FrameBorderSize);
	fprintf(f, "ItemSpacingX=%.3f\n", s.ItemSpacing.x);
	fprintf(f, "ItemSpacingY=%.3f\n", s.ItemSpacing.y);
	fprintf(f, "ItemInnerSpacingX=%.3f\n", s.ItemInnerSpacing.x);
	fprintf(f, "ItemInnerSpacingY=%.3f\n", s.ItemInnerSpacing.y);
	fprintf(f, "IndentSpacing=%.3f\n", s.IndentSpacing);
	fprintf(f, "ScrollbarSize=%.3f\n", s.ScrollbarSize);
	fprintf(f, "ScrollbarRounding=%.3f\n", s.ScrollbarRounding);
	fprintf(f, "GrabMinSize=%.3f\n", s.GrabMinSize);
	fprintf(f, "GrabRounding=%.3f\n", s.GrabRounding);
	fprintf(f, "TabRounding=%.3f\n", s.TabRounding);
	fprintf(f, "PopupRounding=%.3f\n", s.PopupRounding);
	fprintf(f, "ChildRounding=%.3f\n", s.ChildRounding);
	// 色（全ImGuiCol_COUNT個）
	for (int i = 0; i < ImGuiCol_COUNT; i++) {
		const ImVec4& c = s.Colors[i];
		fprintf(f, "Color_%d=%.3f,%.3f,%.3f,%.3f\n", i, c.x, c.y, c.z, c.w);
	}

	fclose(f);
	LogManager::Log("SaveStyle: " + path);
}

//=============================================================================
// スタイルをiniファイルから読み込む
//=============================================================================
void ImGuiManager::LoadStyle(const std::string& path) {
	ImGuiStyle& s = ImGui::GetStyle();
	FILE* f = nullptr;
	fopen_s(&f, path.c_str(), "rt");
	if (!f) {
		// 初回起動時はファイルがなくてもOK
		LogManager::Warning("File not found (first launch): " + path);
		return;
	}

	char line[256];
	float v0, v1, v2, v3;
	while (fgets(line, sizeof(line), f)) {
		// Alpha
		if (sscanf_s(line, "Alpha=%f", &v0) == 1) {
			s.Alpha = v0;
		}
		// サイズ系パラメータ
		if (sscanf_s(line, "WindowPaddingX=%f", &v0) == 1) {
			s.WindowPadding.x = v0;
		}
		if (sscanf_s(line, "WindowPaddingY=%f", &v0) == 1) {
			s.WindowPadding.y = v0;
		}
		if (sscanf_s(line, "WindowRounding=%f", &v0) == 1) {
			s.WindowRounding = v0;
		}
		if (sscanf_s(line, "WindowBorderSize=%f", &v0) == 1) {
			s.WindowBorderSize = v0;
		}
		if (sscanf_s(line, "FramePaddingX=%f", &v0) == 1) {
			s.FramePadding.x = v0;
		}
		if (sscanf_s(line, "FramePaddingY=%f", &v0) == 1) {
			s.FramePadding.y = v0;
		}
		if (sscanf_s(line, "FrameRounding=%f", &v0) == 1) {
			s.FrameRounding = v0;
		}
		if (sscanf_s(line, "FrameBorderSize=%f", &v0) == 1) {
			s.FrameBorderSize = v0;
		}
		if (sscanf_s(line, "ItemSpacingX=%f", &v0) == 1) {
			s.ItemSpacing.x = v0;
		}
		if (sscanf_s(line, "ItemSpacingY=%f", &v0) == 1) {
			s.ItemSpacing.y = v0;
		}
		if (sscanf_s(line, "ItemInnerSpacingX=%f", &v0) == 1) {
			s.ItemInnerSpacing.x = v0;
		}
		if (sscanf_s(line, "ItemInnerSpacingY=%f", &v0) == 1) {
			s.ItemInnerSpacing.y = v0;
		}
		if (sscanf_s(line, "IndentSpacing=%f", &v0) == 1) {
			s.IndentSpacing = v0;
		}
		if (sscanf_s(line, "ScrollbarSize=%f", &v0) == 1) {
			s.ScrollbarSize = v0;
		}
		if (sscanf_s(line, "ScrollbarRounding=%f", &v0) == 1) {
			s.ScrollbarRounding = v0;
		}
		if (sscanf_s(line, "GrabMinSize=%f", &v0) == 1) {
			s.GrabMinSize = v0;
		}
		if (sscanf_s(line, "GrabRounding=%f", &v0) == 1) {
			s.GrabRounding = v0;
		}
		if (sscanf_s(line, "TabRounding=%f", &v0) == 1) {
			s.TabRounding = v0;
		}
		if (sscanf_s(line, "PopupRounding=%f", &v0) == 1) {
			s.PopupRounding = v0;
		}
		if (sscanf_s(line, "ChildRounding=%f", &v0) == 1) {
			s.ChildRounding = v0;
		}

		// 色
		int idx;
		if (sscanf_s(line, "Color_%d=%f,%f,%f,%f", &idx, &v0, &v1, &v2, &v3) == 5) {
			if (idx >= 0 && idx < ImGuiCol_COUNT) {
				s.Colors[idx] = ImVec4(v0, v1, v2, v3);
			}
		}
	}

	fclose(f);
	LogManager::Log("LoadStyle: " + path);
}
#endif