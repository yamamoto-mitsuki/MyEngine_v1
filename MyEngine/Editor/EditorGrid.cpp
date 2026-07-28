#include "EditorGrid.h"
#include "MyEngine/Camera/CameraIncludes.h"

void EditorGrid::Initialize() { 
	modelHandle_ = ModelManager::Load("MyEngine/Resources/Model/Grid.obj");
	model_.modelHandle = modelHandle_;
}

void EditorGrid::Update() {

}

void EditorGrid::Draw(const Camera* camera) {

}