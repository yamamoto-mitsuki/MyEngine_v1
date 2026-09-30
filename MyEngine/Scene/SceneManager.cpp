#include "SceneManager.h"

#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <string_view>
#include <utility>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Scene/DefaultScene.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/Systems/AudioSystem.h"
#include "MyEngine/Time/Time.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#endif

// 静的メンバ変数
SceneManager* SceneManager::updating_ = nullptr;
SceneManager* SceneManager::main_ = nullptr;

namespace {
// 名前 → 作り方 の表
// REGISTER_SCENE は main より前（static 変数が作られるとき）に呼ばれる。表をふつうの static 変数にすると、
// 表の方がまだ作られていないことがあるので、関数の中の static にする（初めて呼ばれたときに必ず作られる）
std::map<std::string, SceneFactory>& SceneTable() {
	static std::map<std::string, SceneFactory> table;
	return table;
}

// シーンファイルの場所から名前を作る（"resources/scenes/Title.scene.json" → "Title"）
std::string SceneNameFromFile(std::string_view file) {
	const size_t slash = file.find_last_of("/\\");
	std::string_view name = (slash == std::string_view::npos) ? file : file.substr(slash + 1);
	const std::string_view extension = kSceneFileExtension;
	if (name.ends_with(extension)) {
		name.remove_suffix(extension.size());
	}
	return std::string(name);
}
} // namespace

//======================================================================================================
// シーンの登録（REGISTER_SCENE）
//======================================================================================================
void SceneRegistry::Register(const char* name, SceneFactory factory) { SceneTable()[name] = std::move(factory); }

SceneFactory SceneRegistry::Find(const std::string& name) {
	const auto it = SceneTable().find(name);
	return (it != SceneTable().end()) ? it->second : nullptr;
}

//======================================================================================================
// 初期化・更新・描画・終了
//======================================================================================================
void SceneManager::Initialize(SceneFactory openScene) {
	MY_ASSERT_MSG(openScene != nullptr, "最初のシーンが渡されていません（Engine::Initialize の2つ目の引数）");
	// Update の外で LoadScene を呼ばれたときは、最初のウィンドウのシーンを切り替える
	if (main_ == nullptr) {
		main_ = this;
	}
	openSceneFactory_ = std::move(openScene);
	ChangeScene(openSceneFactory_(), false);
	openSceneName_ = currentScene_->GetSceneName();
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");
	// この Update の間に呼ばれた LoadScene（シーンの Update・SYSTEM）は、このウィンドウのシーンを切り替える
	updating_ = this;

	// ===== フレームの頭：Entityを作り直す・保存するのはここだけ =====
	// 前のフレームの予約（Componentの追加・Undoなど）は反映済みで、まだ誰も ForEach で回していない
	if (isReloadRequested_) {
		isReloadRequested_ = false;
		ReloadImmediate();
	}
	if (nextScene_) {
		ChangeScene(std::move(nextScene_), false); // 前のフレームで NextScene() が返したシーン
	}
	if (!nextSceneName_.empty()) {
		// 前のフレームで LoadScene に頼まれたシーン（頼まれた名前は空に戻す）
		ChangeScene(CreateScene(std::exchange(nextSceneName_, std::string())), false);
	}
	// 保存は作り直しの後（Stop と Save を同じフレームで頼まれても、戻した後の状態を書く）
	if (isSaveRequested_) {
		isSaveRequested_ = false;
		SaveImmediate();
	}

	// 停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ != PlayState::Editing) {
		// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
		Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
		currentScene_->Update();
		GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）

		// シーン遷移は予約だけ（Entityの入れ替えは、次のフレームの頭で行う）
		if (std::unique_ptr<IScene> next = currentScene_->NextScene()) {
			nextScene_ = std::move(next);
		}
	}
	updating_ = nullptr;
}

void SceneManager::Draw() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、描画できませんでした");
	currentScene_->Draw();
}

void SceneManager::Finalize() {
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset(); // 2回 Finalize しないように、ここで捨てる
	}
	// 閉じたウィンドウへ LoadScene を送らないように
	if (main_ == this) {
		main_ = nullptr;
	}
}

//======================================================================================================
// シーンの切り替え（ゲームのコードから）
//======================================================================================================
bool SceneManager::LoadScene(const std::string& name) {
	SceneManager* target = (updating_ != nullptr) ? updating_ : main_;
	if (target == nullptr) {
		LogManager::Warning(std::format("LoadScene(\"{}\")：ウィンドウがまだ無いので切り替えられません", name));
		return false;
	}
	// 停止中はゲームのコードが動かないので、ふつうは来ない（来るのはシーンの Initialize から呼んだとき）
	// エディタで開いているシーンを、ゲームのコードが勝手に替えないようにする
	if (target->playState_ == PlayState::Editing) {
		LogManager::Log(std::format("LoadScene(\"{}\")：停止中なので切り替えません（Play 中だけ効きます）", name));
		return false;
	}
	if (!HasScene(name)) {
		LogManager::Warning(std::format("LoadScene(\"{}\")：シーンがありません（{} も REGISTER_SCENE({}) もありません）", name, MakeSceneFilePath(name), name));
		return false;
	}
	target->nextSceneName_ = name; // 同じフレームに何回も呼ばれたら、最後に頼まれたシーンへ
	return true;
}

bool SceneManager::HasScene(const std::string& name) {
	if (name.empty()) {
		return false;
	}
	if (SceneRegistry::Find(name) != nullptr) {
		return true; // クラスがあれば、ファイルが無くても CreateDefaultEntities から作れる
	}
	std::error_code error; // 調べられなかったときも例外を出さず「無い」にする
	return std::filesystem::exists(MakeSceneFilePath(name), error);
}

std::unique_ptr<IScene> SceneManager::CreateScene(const std::string& name) {
	// REGISTER_SCENE したクラスがあればそれ、無ければ Entity だけで動くエンジンのシーン
	const SceneFactory factory = SceneRegistry::Find(name);
	std::unique_ptr<IScene> scene = factory ? factory() : std::make_unique<DefaultScene>();
	scene->SetSceneName(name);
	return scene;
}

//======================================================================================================
// 再生コントロール
//======================================================================================================
void SceneManager::Play() {
	if (playState_ == PlayState::Editing) {
		isSnapshotRequested_ = true; // 作り直す直前に、編集した状態を退避する（Stopでここへ戻す）
		RequestReload();             // 退避した物から作り直して、最初から再生する
	}
	playState_ = PlayState::Playing;
	AudioSystem::SetPaused(false); // 一時停止中に Play を呼ばれても、音が止まったままにならないように
}

void SceneManager::Pause() {
	if (playState_ == PlayState::Playing) {
		playState_ = PlayState::Paused;
	} else if (playState_ == PlayState::Paused) {
		playState_ = PlayState::Playing;
	}
	AudioSystem::SetPaused(playState_ == PlayState::Paused); // 時間と一緒に音も止める / 続きから鳴らす
}

void SceneManager::Stop() {
	if (playState_ == PlayState::Editing) {
		return; // 停止中に押しても作り直さない（保存していない編集を消さないように）
	}
	playState_ = PlayState::Editing;
	// 音はすぐに止める（作り直しは次のフレームの頭。一時停止を先に解くと、止めるまでの1フレームだけ鳴ってしまう）
	AudioSystem::StopAll();
	AudioSystem::SetPaused(false);
	RequestReload(); // Playを押した瞬間の状態へ戻す
}

//======================================================================================================
// 作り直す・入れ替える（フレームの頭でだけ呼ぶ）
//======================================================================================================
void SceneManager::ReloadImmediate() {
	// 鳴っている音を全部止める（シーンの Initialize が鳴らした BGM が、Play / Stop / Restart のたびに重なっていかないように）
	AudioSystem::StopAll();
	// Playを押した直後なら、作り直す前に今のEntityを退避する（フレームの頭なので、見えている通りに写せる）
	if (isSnapshotRequested_) {
		isSnapshotRequested_ = false;
		if (GetSceneFile() != nullptr) {
			playSnapshot_ = SceneSerializer::SaveToText();
		}
	}
	// 再生中に頼まれたシーン遷移は捨てる（開いているシーンに戻るので、その遷移はもう関係ない）
	// これが残っていると、Stopを押したフレームにそのまま次のシーンへ飛んでしまう
	nextScene_.reset();
	nextSceneName_.clear();
	// Stop・Restartで開いているシーンに戻る（タイトル → ゲームと切り替わった後でも、Playを押したシーンへ戻る）
	ChangeScene(openSceneFactory_(), true);
	// 停止に戻ったら退避はもう要らない（次のPlayで取り直す）
	if (playState_ == PlayState::Editing) {
		playSnapshot_.clear();
	}
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> next, bool useSnapshot) {
#ifdef USE_IMGUI
	// 履歴が覚えている相手（EntityId）が作り直されるので、履歴は捨てる
	EditorHistory::Clear();
	// 選んでいたEntityを番号で覚えておく（作り直すとHandleは変わるが、番号は同じ）
	const Entity* selected = EntityManager::Get(HierarchyWindow::GetSelected());
	const EntityId selectedId = selected ? selected->id : 0;
#endif

	// ===== 1. 古いシーンの片付け（カメラ・IBLなど、Entityではない物）=====
	const bool oldUsesFile = currentScene_ && currentScene_->GetSceneFile() != nullptr;
	if (currentScene_) {
		currentScene_->Finalize();
	}
	currentScene_ = std::move(next);
	currentScene_->SetWindowTitle(windowTitle_);
	// 名前の無い前の書き方のシーン（GetSceneFile を上書きしたシーン）は、シーンファイルの名前を名前にする（Control ウィンドウに出す）
	if (currentScene_->GetSceneName().empty() && currentScene_->GetSceneFile() != nullptr) {
		currentScene_->SetSceneName(SceneNameFromFile(currentScene_->GetSceneFile()));
	}

	// ===== 2. Entityを入れ替える =====
	// シーンファイルを使うシーンは、全Entityを自分の物として扱う（出るときも入るときも全部消す）
	// 使わないシーンは今まで通り、自分で作って自分で消す
	const bool newUsesFile = currentScene_->GetSceneFile() != nullptr;
	if (oldUsesFile || newUsesFile) {
		SceneSerializer::DestroyAllNow();
	}
	if (newUsesFile) {
		LoadEntities(useSnapshot);
	}
	EntityManager::FlushComponentChanges(); // CreateDefaultEntities で予約した分もここで付く＝Initialize の時点でそろっている

	// ===== 3. 新しいシーンの準備（Entityがそろった後）=====
	const size_t entityCount = EntityManager::GetCount();
	currentScene_->Initialize();
	// Initialize で Entity を作ると、ファイル・退避から作った物に毎回足されて、Play / Stop のたびに増えていく
	if (newUsesFile && EntityManager::GetCount() != entityCount) {
		LogManager::Warning("シーンファイルを使うシーンの Initialize で Entity を作っています。Play / Stop のたびに増えるので、CreateDefaultEntities へ移してください");
	}

#ifdef USE_IMGUI
	HierarchyWindow::SetSelected(EntityManager::FindById(selectedId)); // 同じ番号のEntityを選び直す（居なければ選択なし）
#endif
}

void SceneManager::LoadEntities(bool useSnapshot) {
	// --- Playを押した瞬間の状態に戻す（Stop・Restart）---
	if (useSnapshot && !playSnapshot_.empty()) {
		SceneSerializer::LoadFromText(playSnapshot_);
		return;
	}
	// --- シーンファイルから作る（起動・シーン切り替え）---
	const char* file = currentScene_->GetSceneFile();
	switch (SceneSerializer::LoadFile(file)) {
	case SceneLoadResult::Loaded:
		// ファイルがある間は、CreateDefaultEntities を直しても反映されない（気づけるようにログに出す）
		LogManager::Log(std::format("CreateDefaultEntities() は呼んでいません（{} を消すと、また呼ばれます）", file));
		break;
	case SceneLoadResult::NotFound:
#ifdef USE_IMGUI
		LogManager::Log(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります（Save するとファイルができます）: {}", file));
#else
		// 製品版でファイルが無いのは、入れ忘れの可能性が高い
		LogManager::Warning(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります（resources に入れ忘れていませんか）: {}", file));
#endif
		currentScene_->CreateDefaultEntities();
		break;
	case SceneLoadResult::Failed:
		// 空のシーンで始まる。読めなかったファイルは「〇〇.broken」に写してあるので、Saveで上書きしても残る
		LogManager::Error(std::format("シーンファイルを読めなかったので、空のシーンで始めます（元のファイルは .broken に残しました）: {}", file));
		break;
	}
}

//======================================================================================================
// 保存
//======================================================================================================
void SceneManager::SaveImmediate() {
	const char* file = GetSceneFile();
	if (file == nullptr) {
		LogManager::Warning("このシーンはシーンファイルを使っていないので保存できません（IScene::GetSceneFile）");
		return;
	}
	// 再生中に保存すると、動いた後の状態がファイルに残ってしまう（Unityも再生中は保存できない）
	// 退避を持っている間＝まだ Play 中の Entity のまま（Stop は次の作り直しで戻る）なので、それも断る
	if (playState_ != PlayState::Editing || !playSnapshot_.empty()) {
		LogManager::Warning("再生中は保存できません。Stopしてから保存してください");
		return;
	}
	SceneSerializer::SaveFile(file);
}