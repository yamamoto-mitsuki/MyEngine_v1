#pragma once
#include <vector>

#include "MyEngine/Components/AnimatorComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Matrix4x4.h"

// 前方宣言
class AnimationPlayer;


/// <summary>
/// AnimatorComponentを持つEntityのアニメーションを進めて、ModelRendererのノード行列を作る
/// <para>Entityごとの AnimationPlayer（今の時刻・ノードの行列）はここが持つ。Componentはデータだけ</para>
/// <para>再生中は時刻を進める。停止中（Editing）は、InspectorのPreviewTimeのポーズを見せる</para>
/// </summary>
class AnimationSystem {
public:
	// AnimatorComponentを登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ。
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::UpdateTransform の前に呼ぶ
	/// <para>消えた・外した Animatorを一覧から外し、始まったばかりの Animatorを再生し、
	/// 時刻を進めて、ノードの行列を作り直す</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false）</param>
	/// <param name="deltaTime">経過秒（Time::GetDeltaTime。一時停止中は0なので、時刻は進まない）</param>
	static void Update(bool isGameRunning, float deltaTime);

	/// <summary>
	/// そのEntityのノード行列
	/// </summary>
	/// <param name="entity"></param>
	/// <returns>Animator が無い・無効・まだ作っていないなら nullptr（モデルに入っている形で描く）</returns>
	static const std::vector<Matrix4x4>* GetNodeMatrices(Handle<Entity> entity);


	// ===== ゲームから使う =====
	// そのEntityのアニメーションを最初から再生する。Animatorが無い・アニメーションが未選択なら false
	static bool Play(Handle<Entity> entity);
	// 止めて最初の時刻に戻す
	static void Stop(Handle<Entity> entity);
	// 再生中か（ループしないアニメが最後まで行ったら false）
	static bool IsPlaying(Handle<Entity> entity);
	// 細かい操作（SetTime・SetPauseなど）をしたい用。無ければ nullptr
	// 返したポインタはそのフレームの中だけ使う（Animatorを足す・消すと、一覧の場所が変わる）
	static AnimationPlayer* GetPlayer(Handle<Entity> entity);

};
