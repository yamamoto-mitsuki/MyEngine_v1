#pragma once
#include <string>
#include <memory>
#include <functional>

// 前方宣言
class Camera;


/// <summary>
/// シーン基底クラス
/// <para>シーンファイルを使うシーン（GetSceneFile が nullptr 以外）では、Entityはエンジンが作って消す。
/// 呼ばれる順番は「前のシーンの Finalize → 全Entityを消す → Entityを作る（Playの退避 / シーンファイル / CreateDefaultEntities）→ Initialize」</para>
/// </summary>
class IScene {
public:
	virtual ~IScene() = default;
	// Entityがそろった後に呼ばれる。カメラ・IBLなど、Entityではない物を作る
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	// Initializeで作った物（Entityではない物）を片付ける。Entityはエンジンが消すので、ここでは消さなくてよい
	virtual void Finalize() = 0;
	virtual Camera* GetCamera() { return nullptr; }
	virtual std::unique_ptr<IScene> NextScene() { return nullptr; }

	// ===== シーンファイル（Docs/Tasks/Serialize.md）=====
	// Entityを保存するファイル（例: "resources/scenes/GameScene.scene.json"）
	// nullptr ならファイルを使わない（今まで通り、Initializeで作ってFinalizeで消す）
	virtual const char* GetSceneFile() const { return nullptr; }
	// シーンファイルがまだ無いときだけ、Initialize の前に呼ばれる。最初の配置をコードで作る
	// （Saveするとファイルができるので、次からは呼ばれない。ファイルを消すと、またこの配置から始まる）
	// ここで作ったEntityのHandleをメンバに覚えない（Play・Stop・次の起動ではこの関数が呼ばれず、Handleも変わる）。
	// 覚えたいときは Initialize の中で EntityManager::FindByName などで探す
	virtual void CreateDefaultEntities() {}

	// WindowManagerが自動でセットする
	void SetWindowTitle(const std::wstring& title) { windowTitle_ = title; }
	const std::wstring& GetWindowTitle() const { return windowTitle_; }

protected:
	std::wstring windowTitle_ = L"Title";
};

// シーンの作り方。Stop / Restart で作り直すときにも使う
using SceneFactory = std::function<std::unique_ptr<IScene>()>;