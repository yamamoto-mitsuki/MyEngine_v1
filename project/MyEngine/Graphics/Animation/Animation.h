#pragma once
#include <map>
#include <string>
#include <vector>
#include "Vector3.h"
#include "Quaternion.h"

// ===== キーフレーム =====
template <typename T> 
struct Keyframe {
	T value;    // 値
	float time; // 時刻（秒）
};
using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;


// ===== ノードごとのAnimation =====
// SRT1つあたりのテンプレート
template<typename T> 
struct AnimationCurve {
	std::vector<Keyframe<T>> keyframes;
};

struct NodeAnimation {
	AnimationCurve<Vector3> scale;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> translate;
};


// ===== Animation =====
struct Animation {
	std::string name; // アニメーションの名前
	float duration;   // 全体の尺（秒）
	std::map<std::string, NodeAnimation> nodeAnimations; // NodeAnimationの集合
};