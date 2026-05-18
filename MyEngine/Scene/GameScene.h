#pragma once
#include "MyEngine/Scene/IScene.h"
#include "MyEngine/Render/ModelManager.h"

class GameScene : public IScene {
public:
	~GameScene() override;
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;
	std::unique_ptr<IScene> NextScene() override;

private:
	ModelManager::ModelConfig modelConfig_;
};