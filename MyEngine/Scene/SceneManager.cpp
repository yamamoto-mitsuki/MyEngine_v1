#include "SceneManager.h"

#include <format>
#include <functional>

#include "MyEngine/Component/GameComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Scene/SceneSerializer.h"
#include "MyEngine/Time/Time.h"
#ifdef USE_IMGUI
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#endif


void SceneManager::Initialize() {
	MY_ASSERT_MSG(sceneFactory_ != nullptr, "SetSceneFactory()でシーンの作り方を登録してください");
	ChangeScene(sceneFactory_(), false);
}

void SceneManager::Update() {
	MY_ASSERT_MSG(currentScene_ != nullptr, "シーンが登録されておらず、更新できませんでした");

	// ===== フレームの頭：Entityを作り直す・保存するのはここだけ =====
	// 前のフレームの予約（Componentの追加・Undoなど）は反映済みで、まだ誰も ForEach で回していない
	if (isReloadRequested_) {
		isReloadRequested_ = false;
		ReloadImmediate();
	}
	if (nextScene_) {
		ChangeScene(std::move(nextScene_), false); // 前のフレームで NextScene() が返したシーン
	}
	// 保存は作り直しの後（Stop と Save を同じフレームで頼まれても、戻した後の状態を書く）
	if (isSaveRequested_) {
		isSaveRequested_ = false;
		SaveImmediate();
	}

	// 停止中・一時停止中は更新しない（Drawは回るので画面は出たまま）
	if (playState_ == PlayState::Editing) {
		return;
	}
	// 一時停止は「時間を0にする」。Update は走るので ApplyGV が効く
	Time::SetTimeScale(playState_ == PlayState::Paused ? 0.0f : 1.0f);
	currentScene_->Update();
	GameComponentRegistry::UpdateAll(Time::GetDeltaTime()); // SYSTEM(...) で書いた処理（シーンのUpdateの後、ワールド行列の計算の前）

	// シーン遷移は予約だけ（Entityの入れ替えは、次のフレームの頭で行う）
	if (std::unique_ptr<IScene> next = currentScene_->NextScene()) {
		nextScene_ = std::move(next);
	}
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
}

void SceneManager::Pause() {
	if (playState_ == PlayState::Playing) {
		playState_ = PlayState::Paused;
	} else if (playState_ == PlayState::Paused) {
		playState_ = PlayState::Playing;
	}
}

void SceneManager::Stop() {
	if (playState_ == PlayState::Editing) {
		return; // 停止中に押しても作り直さない（保存していない編集を消さないように）
	}
	playState_ = PlayState::Editing;
	RequestReload(); // Playを押した瞬間の状態へ戻す
}

//======================================================================================================
// 作り直す・入れ替える（フレームの頭でだけ呼ぶ）
//======================================================================================================
void SceneManager::ReloadImmediate() {
	// Playを押した直後なら、作り直す前に今のEntityを退避する（フレームの頭なので、見えている通りに写せる）
	if (isSnapshotRequested_) {
		isSnapshotRequested_ = false;
		if (GetSceneFile() != nullptr) {
			playSnapshot_ = SceneSerializer::SaveToText();
		}
	}
	// 再生中に頼まれたシーン遷移は捨てる（最初のシーンに戻るので、その遷移はもう関係ない）
	// これが残っていると、Stopを押したフレームにそのまま次のシーンへ飛んでしまう
	nextScene_.reset();
	// Stop・Restartで最初のシーンに戻る（タイトル → ゲームと切り替わった後でも、Playを押したシーンへ戻る）
	ChangeScene(sceneFactory_(), true);
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
		LogManager::Log(std::format("シーンファイルが無いので、CreateDefaultEntities() で作ります: {}", file));
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