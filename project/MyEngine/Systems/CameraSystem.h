#pragma once
#include <memory>

#include "MyEngine/Components/CameraComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

// 前方宣言
class Camera;


/// <summary>
/// CameraComponent を持つEntityから、ゲームの絵を撮るカメラを決める
/// <para>カメラ（Camera クラス）はここが1つだけ持ち、毎フレーム、有効な Camera Component の中で priority が一番大きい物の
/// 位置・向き（Transform）と写り方をそこへ写す。シーンの GetCamera() は、上書きしなければこのカメラを返す</para>
/// </summary>
class CameraSystem {
public:
	// CameraComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	// 毎フレーム1回、EntityManager::UpdateTransforms の後に呼ぶ（親の下に置いたカメラも、そのフレームの位置で撮るため）
	static void Update();

	// ゲームの絵を撮るカメラ。Camera Component が1つも無いときは、既定の位置（(0, 0, -10) から +Z を向く）のカメラ
	// nullptr にはならない。借りるだけ（その場で使う）ならこちら
	static Camera* GetMainCamera();
	// 同じカメラを shared_ptr で返す。フレームをまたいで覚えておきたい物は、これを weak_ptr にして持つ
	// （CameraSystem::Release でカメラが消えても、weak_ptr なら「もう無い」と分かる）
	static std::shared_ptr<Camera> GetSharedMainCamera();
	// 今使っている Camera Component の Entity（無ければ無効な Handle）
	static Handle<Entity> GetMainCameraEntity();
};