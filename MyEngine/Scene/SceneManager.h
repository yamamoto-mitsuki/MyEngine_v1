#pragma once
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Scene/IScene.h"
#include <functional>
#include <memory>
#include <string>

// 実行状態
enum class PlayState {
	Editing, // 停止中。Updateを回さない
	Playing, // 再生中
	Paused,  // 一時停止
};


/// <summary>
/// シーン一括管理クラス
/// <para>Entityを作り直す・シーンを切り替える・保存するのは、全部 Update の頭（フレームの境目）で行う</para>
/// <para>「開いているシーン」＝エディタで編集しているシーン。Play はここから始まり、Stop でここ（Playを押した瞬間の状態）へ戻る</para>
/// </summary>
class SceneManager {
public:
	// 最初のシーンを作る。openScene は「開いているシーン」の作り方（Stop・Restart で作り直すときにも使う）
	void Initialize(SceneFactory openScene);
	void Update();
	void Draw();
	void Finalize(); // アプリの終了時に1回。今のシーンの Finalize を呼ぶ

	// ===== シーンの切り替え（ゲームのコードから。どこからでも呼べる）=====
	// 次のフレームの頭で、name のシーン（resources/scenes/<name>.scene.json）へ切り替える。切り替わるのは今 Update しているウィンドウ
	// 再生中だけ効く（停止中は何もしない）。シーンが無ければ Log に警告して false（切り替えない）
	static bool LoadScene(const std::string& name);
	// name のシーンがあるか（シーンファイルがある、または REGISTER_SCENE で登録した名前）
	static bool HasScene(const std::string& name);
	// name のシーンを作る（REGISTER_SCENE した名前ならそのクラス、無ければ DefaultScene）。Entity はまだ作らない
	static std::unique_ptr<IScene> CreateScene(const std::string& name);

	// ===== 再生コントロール =====
	void Play();  // 停止中なら、今のEntityを退避してから再生
	void Pause(); // 一時停止 / 再開
	void Stop();  // 停止して、開いているシーンの「Playを押した瞬間の状態」へ戻す
	// 作り直す。再生中はPlayを押した瞬間の状態から、停止中はシーンファイルから（保存していない編集は消える）
	void RequestReload() { isReloadRequested_ = true; }
	// シーンファイルに保存する（停止中だけ。実際に書くのは次のフレームの頭）
	void RequestSave() { isSaveRequested_ = true; }

	// ===== ゲッター =====
	IScene* GetCurrentScene() { return currentScene_.get(); }
	PlayState GetPlayState() const { return playState_; }
	const char* GetSceneFile() const { return currentScene_ ? currentScene_->GetSceneFile() : nullptr; } // シーンファイルを使わないならnullptr
	// 開いているシーンの名前（Playを押したシーン。再生中に切り替わっても、これは変わらない）
	const std::string& GetOpenSceneName() const { return openSceneName_; }

	// ===== セッター =====
	void RequestNextScene(std::unique_ptr<IScene> next) { nextScene_ = std::move(next); } // 次のフレームの頭で切り替える
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }

private:
	// 開いているシーンを作り直す（Play・Stop・Restart）
	void ReloadImmediate();
	// シーンを入れ替える。useSnapshot なら、Playを押した瞬間に退避したEntityを使う
	void ChangeScene(std::unique_ptr<IScene> next, bool useSnapshot);
	// 新しいシーンのEntityを作る（退避 → シーンファイル → CreateDefaultEntities の順に探す）
	void LoadEntities(bool useSnapshot);
	// シーンファイルに書く
	void SaveImmediate();

	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> nextScene_; // 次のフレームの頭で切り替えるシーン（NextScene() が返した物）
	std::string nextSceneName_;         // 次のフレームの頭で切り替えるシーンの名前（LoadScene で頼まれた物）
	SceneFactory openSceneFactory_;     // 開いているシーンの作り方（Stop・Restart で作り直す）
	std::string openSceneName_;         // 開いているシーンの名前
	std::wstring windowTitle_;
#ifdef USE_IMGUI
	PlayState playState_ = PlayState::Editing; // エディタでは停止状態から始める
#else
	PlayState playState_ = PlayState::Playing; // Release は即座に動かす
#endif
	bool isReloadRequested_ = false;
	bool isSnapshotRequested_ = false; // 次に作り直す前に、今のEntityを退避する（Playを押した）
	bool isSaveRequested_ = false;
	std::string playSnapshot_; // Playを押した瞬間の全Entity（シーンファイルと同じJSON）。空なら退避なし

	// ===== LoadScene の行き先（static 関数からは this が無いので、ここで覚える）=====
	static SceneManager* updating_; // 今 Update しているシーンマネージャ（Update の中だけ入っている）
	static SceneManager* main_;     // 最初に作ったシーンマネージャ（Update の外で LoadScene を呼ばれたときの行き先）
};