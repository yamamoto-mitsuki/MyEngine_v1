#define NOMINMAX
#ifdef USE_IMGUI
#include "ProjectWindow.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <format>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/Editor/Assets/AssetThumbnails.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Editor/Windows/CodeWindow.h"
#include "MyEngine/Editor/Windows/SceneDialogs.h"
#include "MyEngine/Editor/Inspector/AssetInspector.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Physics/PhysicMaterialManager.h"
#include "MyEngine/Scene/PrefabManager.h"
#include "MyEngine/Scene/SceneManager.h"
#include "MyEngine/String/ConvertString.h"
#include "MyEngine/String/FileNameRule.h"

// 静的メンバ変数
std::filesystem::path ProjectWindow::currentFolder_ = "resources";
std::filesystem::path ProjectWindow::selected_;
std::filesystem::path ProjectWindow::renaming_;
std::string ProjectWindow::renameBuffer_;
bool ProjectWindow::renameFocusRequest_ = false;
std::filesystem::path ProjectWindow::deleteTarget_;
bool ProjectWindow::deleteRequest_ = false;
float ProjectWindow::iconSize_ = 64.0f;
std::vector<ProjectWindow::Folder> ProjectWindow::rootFolders_;
std::vector<ProjectWindow::Item> ProjectWindow::items_;
double ProjectWindow::lastScanTime_ = -1000.0;
bool ProjectWindow::rescanRequest_ = true;
bool ProjectWindow::revealRequest_ = false;

namespace {
const std::filesystem::path kResourceFolder = "resources"; // 出すのはゲームのフォルダだけ（エンジンの MyEngine/Resources/ は出さない）
const std::filesystem::path kSourceFolder = "Source";      // ゲームのコード（中の物は Visual Studio のプロジェクトに名前で書いてある）
const std::array<std::filesystem::path, 2> kRootFolders = {kResourceFolder, kSourceFolder}; // 出すのはゲームのフォルダだけ（エンジンの MyEngine/Resources/ は出さない）
constexpr double kRescanSeconds = 1.0;                     // ファイルを読み直す間隔（エクスプローラーで足した物も出るように）
constexpr float kMinIconSize = 16.0f;                      // アイコンの大きさの下限
constexpr float kMaxIconSize = 128.0f;                     // 上限（小さい絵の大きさ AssetThumbnails::kThumbnailSize と同じ）
constexpr float kListModeSize = 24.0f;                     // これ以下なら、1行ずつの並びにする
constexpr float kCellPadding = 4.0f;                       // アイコンの並びの1つ分の、内側の余白
constexpr float kFolderListWidth = 180.0f;                 // 左の木の最初の幅（境目をドラッグで変えられる）
constexpr std::string_view kHiddenExtensions[] = {".bak"}; // 出さないファイル（シーンの保存で残る1つ前のファイル）

// 大文字・小文字を区別しない名前の比べ方（並べる順）
bool NameLess(const std::string& a, const std::string& b) {
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
}

// path が folder の中にあるか（folder そのものも含む）
bool IsInside(const std::filesystem::path& path, const std::filesystem::path& folder) {
	auto it = path.begin();
	for (const std::filesystem::path& part : folder) {
		if (it == path.end() || *it != part) {
			return false;
		}
		++it;
	}
	return true;
}

// path が、出しているフォルダ（resources/ か Source/）の中にあるか
bool IsInsideRoots(const std::filesystem::path& path) {
	return std::any_of(kRootFolders.begin(), kRootFolders.end(), [&path](const std::filesystem::path& root) { return IsInside(path, root); });
}

// path が、出しているフォルダそのものか（左の木の一番上）
bool IsRootFolder(const std::filesystem::path& path) { return std::find(kRootFolders.begin(), kRootFolders.end(), path) != kRootFolders.end(); }

// ファイルの名前から種類を決める
AssetIcon IconOf(const std::string& fileName) {
	if (fileName.ends_with(kSceneFileExtension)) {
		return AssetIcon::Scene;
	}
	if (IsAssetFile(fileName, AssetType::Model)) {
		return AssetIcon::Model;
	}
	if (IsAssetFile(fileName, AssetType::Texture)) {
		return AssetIcon::Texture;
	}
	if (IsAssetFile(fileName, AssetType::Sound)) {
		return AssetIcon::Sound;
	}
	if (CodeWindow::IsCodeFile(fileName)) {
		return AssetIcon::Code;
	}
	return AssetIcon::File;
}

// 出さないファイルか
bool IsHidden(const std::string& fileName) {
	return std::any_of(std::begin(kHiddenExtensions), std::end(kHiddenExtensions), [&](std::string_view extension) { return fileName.ends_with(extension); });
}

// 名前を変えるときに打つ部分（シーンは .scene.json の前、ほかのファイルは拡張子の前、フォルダは全部）
std::string EditableName(const std::filesystem::path& path, AssetIcon icon, bool isFolder) {
	const std::string fileName = ToUtf8(path.filename());
	if (isFolder) {
		return fileName;
	}
	if (icon == AssetIcon::Scene) {
		return fileName.substr(0, fileName.size() - std::string_view(kSceneFileExtension).size());
	}
	return ToUtf8(path.stem());
}

// 名前を newName に変えた後のパス（拡張子はそのまま）
std::filesystem::path RenamedPath(const std::filesystem::path& path, AssetIcon icon, bool isFolder, const std::string& newName) {
	if (isFolder) {
		return path.parent_path() / newName;
	}
	if (icon == AssetIcon::Scene) {
		return path.parent_path() / (newName + kSceneFileExtension);
	}
	return path.parent_path() / (newName + path.extension().string());
}

// シーンの名前で開けるシーンファイルか（resources/scenes/ の直下にある物だけ。LoadScene も名前からこの場所を探す）
bool IsOpenableScene(const std::filesystem::path& path, AssetIcon icon, const std::string& name) { return icon == AssetIcon::Scene && path == std::filesystem::path(MakeSceneFilePath(name)); }

// folder の中で、まだ使われていない名前（NewFolder・NewFolder1・NewFolder2 …）
std::filesystem::path UniquePath(const std::filesystem::path& folder, const std::string& baseName) {
	std::error_code error;
	for (int number = 0;; ++number) {
		const std::filesystem::path path = folder / (number == 0 ? baseName : baseName + std::to_string(number));
		if (!std::filesystem::exists(path, error)) {
			return path;
		}
	}
}

// まだ使われていないシーンの名前（NewScene・NewScene1 …）
std::string UniqueSceneName(const std::string& baseName) {
	for (int number = 0;; ++number) {
		const std::string name = (number == 0) ? baseName : baseName + std::to_string(number);
		if (!SceneManager::HasScene(name)) {
			return name;
		}
	}
}

// ごみ箱へ移す（Windows のごみ箱から元に戻せる）
bool MoveToRecycleBin(const std::filesystem::path& path) {
	std::error_code error;
	// SHFileOperation は「\0\0」で終わる絶対パスを受け取る（何個もまとめて渡せる形のため。std::wstring の終わりの \0 と足して2つ）
	std::wstring from = std::filesystem::absolute(path, error).make_preferred().wstring();
	from.push_back(L'\0');
	SHFILEOPSTRUCTW operation{};
	operation.wFunc = FO_DELETE;
	operation.pFrom = from.c_str();
	operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI; // ごみ箱へ・確かめるのはこちらで済んだ・進み具合を出さない
	if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted) {
		LogManager::Warning(std::format("ごみ箱へ移せませんでした: {}", ToUtf8(path)));
		return false;
	}
	LogManager::Log(std::format("ごみ箱へ移しました: {}", ToUtf8(path)));
	return true;
}

// エクスプローラーで、その物を選んだ状態で開く
void RevealInExplorer(const std::filesystem::path& path) {
	std::error_code error;
	const std::wstring arguments = L"/select,\"" + std::filesystem::absolute(path, error).make_preferred().wstring() + L"\"";
	ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

// エクスプローラーで、フォルダの中を開く
void OpenInExplorer(const std::filesystem::path& folder) {
	std::error_code error;
	const std::wstring target = std::filesystem::absolute(folder, error).make_preferred().wstring();
	ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// 名前を w の幅に収めて描く（収まれば真ん中、収まらなければ左に寄せて、はみ出した所は切る）
void DrawClippedText(ImDrawList* drawList, const std::string& text, float x, float y, float width) {
	const float textWidth = ImGui::CalcTextSize(text.c_str()).x;
	const float textX = (textWidth <= width) ? x + (width - textWidth) * 0.5f : x;
	const ImVec4 clip(x, y, x + width, y + ImGui::GetTextLineHeight());
	drawList->AddText(nullptr, 0.0f, ImVec2(textX, y), ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, 0.0f, &clip);
}
} // namespace

//=============================================================================
// 描画
//=============================================================================
void ProjectWindow::Draw() {
	ImGui::SetNextWindowSize(ImVec2(800.0f, 300.0f), ImGuiCond_FirstUseEver); // 初めて出すときの大きさ（後は imgui.ini に覚えられる）
	if (!ImGui::Begin("Project")) {
		ImGui::End();
		return; // 隠れている間はファイルを読まない
	}

	// --- ファイルを読み直す（頼まれたとき・1秒ごと）---
	if (rescanRequest_ || ImGui::GetTime() - lastScanTime_ > kRescanSeconds) {
		Rescan();
	}
	if (rootFolders_.empty()) {
		ImGui::TextDisabled("resources/ フォルダも Source/ フォルダもありません（ゲームのプロジェクトのフォルダで起動してください）");
		ImGui::End();
		return;
	}

	// --- 左右に分ける（境目はドラッグで動かせる）---
	if (ImGui::BeginTable("##projectLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("folders", ImGuiTableColumnFlags_WidthFixed, kFolderListWidth);
		ImGui::TableSetupColumn("items", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		// ===== 左：フォルダの木 =====
		ImGui::TableNextColumn();
		if (ImGui::BeginChild("##folderTree")) {
			for (const Folder& root : rootFolders_) {
				DrawFolderTree(root);
			}
			revealRequest_ = false;
		}
		ImGui::EndChild();

		// ===== 右：今のフォルダの中身 =====
		ImGui::TableNextColumn();
		DrawToolbar();
		ImGui::Separator();
		// 一番下の1行（選んでいる物のパス）の分を残して、残りを中身にする
		if (ImGui::BeginChild("##items", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()))) {
			DrawItems();
			HandleEmptySpace();
		}
		ImGui::EndChild();
		ImGui::TextDisabled("%s", ToUtf8(selected_.empty() ? currentFolder_ : selected_).c_str());
		ImGui::EndTable();
	}
	HandleShortcuts();

	// --- ごみ箱へ移すか聞く（メニューの中から開くと入れ子になるので、ここで開く）---
	if (deleteRequest_) {
		ImGui::OpenPopup("Delete Asset");
		deleteRequest_ = false;
	}
	DrawDeleteDialog();
	ImGui::End();
}

//=============================================================================
// 読む
//=============================================================================
void ProjectWindow::Rescan() {
	lastScanTime_ = ImGui::GetTime();
	rescanRequest_ = false;
	// 左の木（あるフォルダだけ）
	std::error_code error;
	rootFolders_.clear();
	for (const std::filesystem::path& root : kRootFolders) {
		if (std::filesystem::is_directory(root, error)) {
			rootFolders_.push_back(ScanFolderTree(root));
		}
	}
	if (rootFolders_.empty()) {
		items_.clear();
		return;
	}
	// 見ていたフォルダが消えていたら（名前を変えた・エクスプローラーで消した）、一番上のフォルダに戻る
	if (!std::filesystem::is_directory(currentFolder_, error) || !IsInsideRoots(currentFolder_)) {
		currentFolder_ = rootFolders_.front().path;
	}
	items_ = ScanItems(currentFolder_);
	// 名前を変えている物が無くなっていたら、やめる
	if (!renaming_.empty() && std::none_of(items_.begin(), items_.end(), [](const Item& item) { return item.path == renaming_; })) {
		renaming_.clear();
	}
}

ProjectWindow::Folder ProjectWindow::ScanFolderTree(const std::filesystem::path& path) {
	Folder folder;
	folder.path = path;
	folder.name = ToUtf8(path.filename());
	std::error_code error;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, error)) {
		if (entry.is_directory(error)) {
			folder.children.push_back(ScanFolderTree(entry.path()));
		}
	}
	std::sort(folder.children.begin(), folder.children.end(), [](const Folder& a, const Folder& b) { return NameLess(a.name, b.name); });
	return folder;
}

std::vector<ProjectWindow::Item> ProjectWindow::ScanItems(const std::filesystem::path& folder) {
	std::vector<Item> folders;
	std::vector<Item> files;
	std::error_code error;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, error)) {
		const std::string fileName = ToUtf8(entry.path().filename());
		Item item;
		item.path = entry.path();
		item.isFolder = entry.is_directory(error);
		if (item.isFolder) {
			item.name = fileName;
			item.icon = AssetIcon::Folder;
			folders.push_back(std::move(item));
			continue;
		}
		if (IsHidden(fileName)) {
			continue;
		}
		item.icon = IconOf(fileName);
		// 種類が分かる物は拡張子を出さない（アイコンで分かる。Unity と同じ）。分からない物・コードは拡張子まで出す（plane.mtl と plane.blend、Coin.h と Coin.cpp を見分けられるように）
		item.name = (item.icon == AssetIcon::File || item.icon == AssetIcon::Code) ? fileName : EditableName(item.path, item.icon, false);
		item.writeTime = entry.last_write_time(error);
		files.push_back(std::move(item));
	}
	// フォルダが先、その後にファイル。それぞれ名前の順
	auto byName = [](const Item& a, const Item& b) { return NameLess(a.name, b.name); };
	std::sort(folders.begin(), folders.end(), byName);
	std::sort(files.begin(), files.end(), byName);
	folders.insert(folders.end(), files.begin(), files.end());
	return folders;
}

//=============================================================================
// 見出し（今のフォルダの場所・アイコンの大きさ）
//=============================================================================
void ProjectWindow::DrawToolbar() {
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端

	// --- 今のフォルダの場所（resources / monsterBall）。押すとそのフォルダへ ---
	const std::filesystem::path folder = currentFolder_; // 押すと currentFolder_ が変わるので、写してから回す
	std::filesystem::path partial;
	for (const std::filesystem::path& part : folder) {
		const bool isFirst = partial.empty();
		partial /= part;
		if (!isFirst) {
			ImGui::SameLine(0.0f, 2.0f);
			ImGui::TextDisabled("/");
			ImGui::SameLine(0.0f, 2.0f);
		}
		ImGui::PushID(ToUtf8(partial).c_str());
		if (ImGui::SmallButton(ToUtf8(part).c_str())) {
			OpenFolder(partial);
		}
		ImGui::PopID();
	}

	// --- 右端：アイコンの大きさ（小さくすると1行ずつの並び）---
	constexpr float kDragWidth = 90.0f;
	ImGui::SameLine(right - kDragWidth);
	ImGui::SetNextItemWidth(kDragWidth);
	ImGui::DragFloat("##iconSize", &iconSize_, 0.5f, kMinIconSize, kMaxIconSize, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SetItemTooltip("アイコンの大きさ（%.0f 以下で1行ずつの並び）", kListModeSize);
}

//=============================================================================
// 左の木
//=============================================================================
void ProjectWindow::DrawFolderTree(const Folder& folder) {
	const bool isCurrent = (folder.path == currentFolder_);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (folder.children.empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (isCurrent) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (IsRootFolder(folder.path)) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}
	// 右でフォルダの中へ入ったら、そこまでの親を開いて見えるようにする
	if (revealRequest_ && !isCurrent && IsInside(currentFolder_, folder.path)) {
		ImGui::SetNextItemOpen(true);
	}

	ImGui::PushID(ToUtf8(folder.path).c_str());
	const bool isOpen = ImGui::TreeNodeEx("##folder", flags);
	// 押したらそのフォルダの中身を右に出す（矢印を押したときは開く・閉じるだけ）
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		OpenFolder(folder.path);
	}
	// 矢印の右に、アイコンと名前
	ImGui::SameLine();
	const float iconSide = ImGui::GetTextLineHeight();
	const ImVec2 iconMin = ImGui::GetCursorScreenPos();
	AssetIcons::Draw(ImGui::GetWindowDrawList(), isCurrent ? AssetIcon::FolderOpen : AssetIcon::Folder, iconMin, ImVec2(iconMin.x + iconSide, iconMin.y + iconSide));
	ImGui::Dummy(ImVec2(iconSide, iconSide));
	ImGui::SameLine();
	ImGui::TextUnformatted(folder.name.c_str());

	if (isOpen) {
		for (const Folder& child : folder.children) {
			DrawFolderTree(child);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

//=============================================================================
// 右の中身
//=============================================================================
void ProjectWindow::DrawItems() {
	if (items_.empty()) {
		ImGui::TextDisabled("空のフォルダです（右クリック → Create）");
		return;
	}
	// --- 1行ずつの並び ---
	if (iconSize_ <= kListModeSize) {
		for (const Item& item : items_) {
			DrawListItem(item);
		}
		return;
	}
	// --- アイコンの並び（入るだけ横に並べて、はみ出したら次の行）---
	const float cellWidth = iconSize_ + kCellPadding * 2.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cellWidth + spacing)));
	for (size_t i = 0; i < items_.size(); ++i) {
		if (i % static_cast<size_t>(columns) != 0) {
			ImGui::SameLine();
		}
		DrawGridItem(items_[i], cellWidth);
	}
}

void ProjectWindow::DrawGridItem(const Item& item, float cellWidth) {
	const bool isRenaming = (item.path == renaming_);
	const float textHeight = ImGui::GetTextLineHeight();
	const ImVec2 cellSize(cellWidth, kCellPadding + iconSize_ + kCellPadding + textHeight + kCellPadding);

	ImGui::PushID(ToUtf8(item.path).c_str());
	ImGui::BeginGroup();
	const ImVec2 cellMin = ImGui::GetCursorScreenPos();
	// 押せる四角（選ぶ・ダブルクリック・右クリック。マウスが乗る・選ぶと色が付く）。名前の変更中は、下に入力欄を重ねる
	ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
	if (isRenaming) {
		flags |= ImGuiSelectableFlags_AllowOverlap;
	}
	ImGui::Selectable("##cell", item.path == selected_, flags, cellSize);
	HandleItemInput(item);

	// アイコン（上の真ん中）
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 iconMin(cellMin.x + (cellWidth - iconSize_) * 0.5f, cellMin.y + kCellPadding);
	DrawItemIcon(drawList, item, iconMin, ImVec2(iconMin.x + iconSize_, iconMin.y + iconSize_));

	// 名前（下）。変更中は入力欄
	const float textY = iconMin.y + iconSize_ + kCellPadding;
	if (isRenaming) {
		ImGui::SetCursorScreenPos(ImVec2(cellMin.x, textY - ImGui::GetStyle().FramePadding.y));
		ImGui::SetNextItemWidth(cellWidth);
		DrawRenameField(item);
	} else {
		DrawClippedText(drawList, LabelOf(item), cellMin.x, textY, cellWidth);
	}
	ImGui::EndGroup();
	ImGui::PopID();
}

void ProjectWindow::DrawListItem(const Item& item) {
	const bool isRenaming = (item.path == renaming_);
	const float rowHeight = ImGui::GetFrameHeight();

	ImGui::PushID(ToUtf8(item.path).c_str());
	const ImVec2 rowMin = ImGui::GetCursorScreenPos();
	ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
	if (isRenaming) {
		flags |= ImGuiSelectableFlags_AllowOverlap;
	}
	ImGui::Selectable("##row", item.path == selected_, flags, ImVec2(0.0f, rowHeight)); // 幅0＝横いっぱい
	HandleItemInput(item);

	// 左にアイコン、右に名前
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float iconSide = rowHeight - 2.0f;
	DrawItemIcon(drawList, item, ImVec2(rowMin.x, rowMin.y + 1.0f), ImVec2(rowMin.x + iconSide, rowMin.y + 1.0f + iconSide));
	const float textX = rowMin.x + iconSide + ImGui::GetStyle().ItemInnerSpacing.x;
	if (isRenaming) {
		ImGui::SetCursorScreenPos(ImVec2(textX, rowMin.y));
		ImGui::SetNextItemWidth(-FLT_MIN);
		DrawRenameField(item);
	} else {
		const float textY = rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
		drawList->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), LabelOf(item).c_str());
	}
	ImGui::PopID();
}

void ProjectWindow::DrawItemIcon(ImDrawList* drawList, const Item& item, const ImVec2& min, const ImVec2& max) {
	// 見えていない所は描かない（小さい絵も作らない）
	if (!ImGui::IsRectVisible(min, max)) {
		return;
	}
	// 画像ファイルは、その画像の小さい絵（縦横比を保って、四角の真ん中に）
	if (item.icon == AssetIcon::Texture) {
		ImTextureID texture = ImTextureID_Invalid;
		ImVec2 size;
		if (AssetThumbnails::Get(ToUtf8(item.path), item.writeTime, texture, size)) {
			const ImVec2 box(max.x - min.x, max.y - min.y);
			const float scale = (std::min)(box.x / size.x, box.y / size.y);
			const ImVec2 drawSize(size.x * scale, size.y * scale);
			const ImVec2 drawMin(min.x + (box.x - drawSize.x) * 0.5f, min.y + (box.y - drawSize.y) * 0.5f);
			drawList->AddImage(texture, drawMin, ImVec2(drawMin.x + drawSize.x, drawMin.y + drawSize.y));
			return;
		}
		// まだ作っていない（次のフレーム以降に作る）・作れなかった → アイコンで代わりに描く
	}
	AssetIcons::Draw(drawList, item.icon, min, max);
}

std::string ProjectWindow::LabelOf(const Item& item) {
	// 開いているシーンに保存していない変更があれば、名前に * を付ける（Control ウィンドウと同じ）
	const SceneManager* sceneManager = SceneManager::GetMain();
	if (sceneManager && sceneManager->IsDirty() && item.name == sceneManager->GetOpenSceneName() && IsOpenableScene(item.path, item.icon, item.name)) {
		return item.name + " *";
	}
	return item.name;
}

//=============================================================================
// 項目の上でのクリック
//=============================================================================
void ProjectWindow::HandleItemInput(const Item& item) {
	// --- 左・右クリックで選ぶ ---
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		selected_ = item.path;
		// Inspector で直せるファイル（Physic Material など）なら、Inspector に出す（Entity の選択は外す。Unity と同じ）
		if (!item.isFolder && AssetInspector::CanEdit(item.path)) {
			AssetInspector::Select(item.path);
			HierarchyWindow::SetSelected({});
		}
	}
	// --- ダブルクリックで開く（フォルダは中へ、シーンはエディタで開く）---
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		Open(item);
	}
	// --- マウスを乗せたらファイルの名前（拡張子まで。名前が長くて切れていても分かる）---
	ImGui::SetItemTooltip("%s", ToUtf8(item.path.filename()).c_str());
	// --- 右クリックのメニュー ---
	if (ImGui::BeginPopupContextItem("itemMenu")) {
		DrawItemMenu(item);
		ImGui::EndPopup();
	}
}

void ProjectWindow::DrawItemMenu(const Item& item) {
	// --- 開く ---
	if (item.isFolder) {
		if (ImGui::MenuItem("Open")) {
			Open(item);
		}
	} else if (item.icon == AssetIcon::Scene) {
		const SceneManager* sceneManager = SceneManager::GetMain();
		const bool canOpen = IsOpenableScene(item.path, item.icon, item.name) && sceneManager && sceneManager->GetPlayState() == PlayState::Editing;
		if (ImGui::MenuItem("Open", nullptr, false, canOpen)) {
			Open(item);
		}
		ImGui::SetItemTooltip("停止中に、resources/scenes/ のシーンだけ開けます");
	} else if (item.icon == AssetIcon::Code) {
		// コードはエンジンの中で読む（Code ウィンドウ）か、Visual Studio で直す
		if (ImGui::MenuItem("Open in Code Window")) {
			Open(item);
		}
		if (ImGui::MenuItem("Open in Visual Studio")) {
			CodeWindow::OpenExternal(item.path);
		}
	} else if (IsAssetFile(ToUtf8(item.path), AssetType::Prefab)) {
		// Prefab はシーンに置ける（原点に。次のフレームの頭にできる。Undo には入らないので、要らなければ Hierarchy で消す）
		if (ImGui::MenuItem("Place in Scene")) {
			if (const uint32_t prefab = LoadAsset(AssetType::Prefab, ToUtf8(item.path)); prefab != 0) {
				PrefabManager::RequestPlace(prefab);
			}
		}
		ImGui::SetItemTooltip("原点に置く。直して Hierarchy の右クリック → Save As Prefab で、Prefab を上書きできる");
	}
	ImGui::Separator();

	// --- 名前を変える・消す（開いているシーンと、それが入っているフォルダ・Source/ の中の物はできない）---
	const bool isLocked = IsLocked(item);
	if (ImGui::MenuItem("Rename", "F2", false, !isLocked)) {
		StartRename(item);
	}
	if (ImGui::MenuItem("Delete", "Del", false, !isLocked)) {
		deleteTarget_ = item.path;
		deleteRequest_ = true;
	}
	if (isLocked && IsInside(item.path, kSourceFolder)) {
		ImGui::TextDisabled("コードは Visual Studio で名前を変える・消す（プロジェクトにファイルの名前が書いてあるので）");
	} else if (isLocked) {
		ImGui::TextDisabled("開いているシーンは、名前を変える・消すができません");
	} else if (item.icon != AssetIcon::Scene) {
		ImGui::TextDisabled("名前を変える・消すと、使っているシーンからは見つからなくなります");
	}
	ImGui::Separator();

	// --- そのほか ---
	if (ImGui::MenuItem("Copy Path")) {
		ImGui::SetClipboardText(ToUtf8(item.path).c_str()); // Inspector やコードに貼れるように、resources/ から始まるパスで
	}
	if (ImGui::MenuItem("Show in Explorer")) {
		RevealInExplorer(item.path);
	}
	ImGui::Separator();
	DrawCreateMenu();
}

void ProjectWindow::DrawCreateMenu() {
	if (ImGui::BeginMenu("Create")) {
		if (ImGui::MenuItem("Folder")) {
			CreateFolder();
		}
		if (ImGui::MenuItem("Scene")) {
			CreateScene();
		}
		ImGui::SetItemTooltip("シーンは resources/scenes/ に作ります（シーンは名前で探すので、置き場所は1つ）");
		if (ImGui::MenuItem("Physic Material")) {
			CreatePhysicMaterial();
		}
		ImGui::SetItemTooltip("今のフォルダに作ります（跳ね返り・摩擦。Collider の Physic Material で選ぶ）");
		ImGui::EndMenu();
	}
}

//=============================================================================
// 何も無い所
//=============================================================================
void ProjectWindow::HandleEmptySpace() {
	// --- 何も無い所をクリックしたら、選ぶのをやめる ---
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		selected_.clear();
	}
	// --- 何も無い所の右クリック（項目の上では項目のメニューが出る）---
	if (ImGui::BeginPopupContextWindow("emptySpaceMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		DrawCreateMenu();
		ImGui::Separator();
		if (ImGui::MenuItem("Show in Explorer")) {
			OpenInExplorer(currentFolder_);
		}
		if (ImGui::MenuItem("Refresh")) {
			rescanRequest_ = true;
		}
		ImGui::EndPopup();
	}
}

//=============================================================================
// ショートカット（Project ウィンドウを触っているときだけ）
//=============================================================================
void ProjectWindow::HandleShortcuts() {
	// 名前の入力中・ほかの入力中は、入力欄のほうを優先する
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || !renaming_.empty() || ImGui::GetIO().WantTextInput) {
		return;
	}
	const Item* item = FindSelected();
	if (item == nullptr || IsLocked(*item)) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
		StartRename(*item);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		deleteTarget_ = item->path;
		deleteRequest_ = true;
	}
}

//=============================================================================
// 開く・作る
//=============================================================================
void ProjectWindow::Open(const Item& item) {
	if (item.isFolder) {
		OpenFolder(item.path);
		return;
	}
	// シーンはエディタで開く（保存していない変更があれば、先に聞く）
	if (IsOpenableScene(item.path, item.icon, item.name)) {
		SceneDialogs::RequestOpen(item.name);
	}
	// コードは Code ウィンドウで読む
	if (item.icon == AssetIcon::Code) {
		CodeWindow::Open(item.path);
	}
	// そのほかのファイルは、今は何もしない（後で：Inspector へドラッグ）
}

void ProjectWindow::OpenFolder(const std::filesystem::path& folder) {
	currentFolder_ = folder;
	selected_.clear();
	renaming_.clear();
	rescanRequest_ = true; // 中身は次のフレームの頭で読む（今は中身の一覧を回している途中かもしれないので）
	revealRequest_ = true;
}

void ProjectWindow::CreateFolder() {
	const std::filesystem::path path = UniquePath(currentFolder_, "NewFolder");
	std::error_code error;
	if (!std::filesystem::create_directory(path, error)) {
		LogManager::Warning(std::format("フォルダを作れませんでした: {}", ToUtf8(path)));
		return;
	}
	// 作ったフォルダを選んで、すぐに名前を打てるようにする（Unity と同じ）
	selected_ = path;
	renaming_ = path;
	renameBuffer_ = ToUtf8(path.filename());
	renameFocusRequest_ = true;
	rescanRequest_ = true;
}

void ProjectWindow::CreateScene() {
	const std::string name = UniqueSceneName("NewScene");
	if (!SceneManager::CreateSceneFile(name)) {
		return;
	}
	// resources/scenes/ を開いて、作ったシーンの名前をすぐに打てるようにする
	const std::filesystem::path path = MakeSceneFilePath(name);
	OpenFolder(path.parent_path());
	selected_ = path;
	renaming_ = path;
	renameBuffer_ = name;
	renameFocusRequest_ = true;
}

void ProjectWindow::CreatePhysicMaterial() {
	// まだ使われていない名前（NewPhysicMaterial.physmat・NewPhysicMaterial1.physmat …）。拡張子の前に番号を付ける
	std::filesystem::path path;
	std::error_code error;
	for (int number = 0;; ++number) {
		const std::string name = "NewPhysicMaterial" + (number == 0 ? std::string() : std::to_string(number));
		path = currentFolder_ / (name + std::string(PhysicMaterialManager::kFileExtension));
		if (!std::filesystem::exists(path, error)) {
			break;
		}
	}
	if (!PhysicMaterialManager::CreateNewFile(path)) {
		return;
	}
	// 作ったファイルを選んで Inspector に出し、すぐに名前を打てるようにする
	selected_ = path;
	AssetInspector::Select(path);
	HierarchyWindow::SetSelected({});
	renaming_ = path;
	renameBuffer_ = ToUtf8(path.filename());
	renameFocusRequest_ = true;
	rescanRequest_ = true;
}

//=============================================================================
// 名前の変更
//=============================================================================
void ProjectWindow::StartRename(const Item& item) {
	if (IsLocked(item)) {
		return;
	}
	selected_ = item.path;
	renaming_ = item.path;
	renameBuffer_ = EditableName(item.path, item.icon, item.isFolder); // 拡張子は変えない
	renameFocusRequest_ = true;
}

void ProjectWindow::DrawRenameField(const Item& item) {
	// 出した最初のフレームだけ、入力欄にフォーカスを移す（すぐに文字を打てるように）
	const bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全部選んだ状態（そのまま打つと置き換わる）
	const bool entered = EditorWidgets::InputFileName("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	// 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）。Esc で抜けたら ImGui が元に戻すので、変わらない
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		ApplyRename(item);
	}
	// 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる（Hierarchy の名前の変更と同じ）
	bool finished = entered || ImGui::IsItemDeactivated();
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_.clear();
	}
}

void ProjectWindow::ApplyRename(const Item& item) {
	if (renameBuffer_ == EditableName(item.path, item.icon, item.isFolder)) {
		return; // 変わっていない
	}
	if (!IsValidFileName(renameBuffer_)) {
		LogManager::Warning(std::format("名前に使えるのは、英数字と _ - だけです: \"{}\"", renameBuffer_));
		return;
	}
	const std::filesystem::path newPath = RenamedPath(item.path, item.icon, item.isFolder, renameBuffer_);
	std::error_code error;
	// 同じ名前がもうあるならやめる（大文字・小文字だけ変えるときは、Windows では同じファイルなので通す）
	if (std::filesystem::exists(newPath, error) && !std::filesystem::equivalent(item.path, newPath, error)) {
		LogManager::Warning(std::format("同じ名前がもうあります: {}", ToUtf8(newPath)));
		return;
	}
	std::filesystem::rename(item.path, newPath, error);
	if (error) {
		LogManager::Warning(std::format("名前を変えられませんでした: {}（{}）", ToUtf8(item.path), error.message()));
		return;
	}
	LogManager::Log(std::format("名前を変えました: {} → {}", ToUtf8(item.path), ToUtf8(newPath)));
	if (selected_ == item.path) {
		selected_ = newPath;
	}
	rescanRequest_ = true;
}

//=============================================================================
// ごみ箱へ移すか聞く
//=============================================================================
void ProjectWindow::DrawDeleteDialog() {
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		return;
	}
	ImGui::Text("%s をごみ箱へ移しますか？", ToUtf8(deleteTarget_).c_str());
	ImGui::TextDisabled("（Windows のごみ箱から元に戻せます）");
	ImGui::Spacing();
	if (ImGui::Button("Delete")) {
		MoveToRecycleBin(deleteTarget_);
		if (IsInside(selected_, deleteTarget_)) {
			selected_.clear();
		}
		rescanRequest_ = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

//=============================================================================
// 小さな道具
//=============================================================================
bool ProjectWindow::IsLocked(const Item& item) {
	// Source/ の中の物（コードとフォルダ）：Visual Studio のプロジェクトにファイルの名前が書いてあるので、ここで変えるとビルドできなくなる
	if (IsInside(item.path, kSourceFolder)) {
		return true;
	}
	const SceneManager* sceneManager = SceneManager::GetMain();
	if (sceneManager == nullptr || sceneManager->GetOpenSceneName().empty()) {
		return false;
	}
	// 開いているシーンそのもの、またはそれが入っているフォルダ（名前を変えると、保存する先が無くなってしまう）
	const std::filesystem::path openFile = MakeSceneFilePath(sceneManager->GetOpenSceneName());
	return item.path == openFile || (item.isFolder && IsInside(openFile, item.path));
}

const ProjectWindow::Item* ProjectWindow::FindSelected() {
	for (const Item& item : items_) {
		if (item.path == selected_) {
			return &item;
		}
	}
	return nullptr;
}
#endif