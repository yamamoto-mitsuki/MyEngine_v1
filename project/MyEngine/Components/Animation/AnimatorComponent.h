#pragma once
#include <cstdint>
#include <type_traits>

// アニメーションを再生するためのデータ。Entityに付けるComponent（カテゴリ: Animation）
// 同じEntityのModel Rendererのノードを動かす（Model Rendererが無ければ何もしない）
// 今の時刻・ノードの行列（AnimationPlayer）は持たない。AnimationSystem が持つ
// ここに入れると、コピー・Undo・保存で丸ごと写されてしまう

struct AnimatorComponent {
	uint32_t animationHandle = 0; // AnimationManager::Loadの番号。保存はパス
	int clipIndex = 0;            // ファイルの中の何番目のアニメーションか（普通は0）
	float speed = 1.0f;           // 1で等速、2で倍速、負で逆再生
	bool loop = true;             // ループするか
	bool playOnStart = true;      // Playを押したとき（シーンが始まったとき）に再生する
	float previewTime = 0.0f;     // 停止中（Editing）に見せる時刻（秒）。Inspectorでつまんでポーズを確かめる
	bool enabled = true;          // false なら動かさない（Model Renderer はモデルに入っている形で描く）
};

static_assert(std::is_trivially_copyable_v<AnimatorComponent>);