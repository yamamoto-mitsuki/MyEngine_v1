#pragma once
#include "MyEngine/Components/TriggerActivatorComponent.h"

/// <summary>
/// Trigger Activator を登録し、範囲に入った・出たを見て、Target を出す・消す（フェードも）
/// <para>入ったかどうかは、同じ Entity の Collider の当たり（CollisionSystem::GetHits）で決める。
/// Enter・Exit の知らせではなく「今、中にいるか」を毎フレーム調べて、前のフレームと比べる（相手が消えても、出たことになる）</para>
/// <para>フェードは Target の Alpha Group の Alpha を動かす。描くときに Image・Text・Sprite Renderer が、親まで掛けて使う（SpriteRenderSystem::CalcGroupAlpha）</para>
/// </summary>
class TriggerActivatorSystem {
public:
	// TriggerActivatorComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 入った・出たを見て Target を出す・消し、フェードを進める（再生中だけ。停止中は何もしない）
	/// <para>CollisionSystem::Update の後に呼ぶ（そのフレームの当たりで決めるので、入ったフレームに出始める）</para>
	/// </summary>
	/// <param name="isGameRunning">再生中か（一時停止中も true。そのときは deltaTime が 0 なので、フェードは止まる）</param>
	/// <param name="deltaTime">経過秒（Time::GetDeltaTime）</param>
	static void Update(bool isGameRunning, float deltaTime);
};