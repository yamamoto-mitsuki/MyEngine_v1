#pragma once
#include "MyEngine/Components/Camera/CameraFollowComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

// 前方宣言
class ComponentUI;

/// <summary>
/// Camera Follow の中身（CameraSystem から使う）
/// </summary>
namespace CameraFollow {
// Inspector とシーンファイルの並び
void Describe(ComponentUI& ui, CameraFollowComponent& value);

// カメラ1つ分の追いかけ。Transform を動かしたら true
bool Update(Handle<Entity> entity, CameraFollowComponent& follow, float deltaTime);

// なめらかに近づけるときの、1フレームに進む割合（残りの差 × これ だけ進む）。smoothTime が 0 なら 1（すぐ合わせる）
float CalcSmoothRate(float deltaTime, float smoothTime);
} // namespace CameraFollow