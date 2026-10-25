#include "AnimationSystem.h"

#include <cfloat>
#include <format>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <string_view>

#include "MyEngine/Components/TransformComponent.h"  
#include "MyEngine/Components/ModelRendererComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Editor/Gizmos/GizmoLines.h" 
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/Animation/AnimationManager.h"
#include "MyEngine/Graphics/Animation/AnimationPlayer.h"
#include "MyEngine/Graphics/Model/ModelManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h" 

namespace {
// --- 骨のギズモ ---
constexpr Vector4 kJointColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 骨（Joint）の球の色（白）
constexpr Vector4 kBoneColor = {1.0f, 0.8f, 0.2f, 1.0f};  // 親子をつなぐ線の色（黄色）
constexpr float kJointRadiusRate = 0.003f;   // 球の半径＝骨全体の大きさ（対角線）× これ（モデルの大きさが違っても同じくらいに見える）
constexpr float kMinJointRadius = 0.001f;    // 球の半径の下限（骨が1本だけ・全部同じ位置のとき）
constexpr uint32_t kJointCircleDivision = 8; // 球の円を何本の線で描くか（骨は数が多いので粗くする）
constexpr float kNoFadeStart = 10000.0f;     // 距離でフェードさせないための値（Collider・ライトのギズモと同じ）
constexpr float kNoFadeEnd = 20000.0f;

/// <summary>
/// Animator1つ分の実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct AnimatorState {
	Handle<Entity> entity;        // 持ち主
	uint32_t animationHandle = 0; // 再生しているアニメーションの番号
	int clipIndex = 0;            // 再生しているのはファイルの中の何番目か
	AnimationPlayer player;       // 今の時刻・ノードの行列
};

/// <summary>
/// AnimationSystem が持つもの（Initializeで作り、Releaseで消す）
/// </summary>
struct AnimationState {
	std::vector<AnimatorState> animators; // 見つけたAnimatorの一覧。止まっていても持つ
	bool wasGameRunning = false; // 前のフレームでゲームが動いていたか

	// --- 骨のギズモ ---
	Renderer::LineListConfig gizmoLines; // ギズモの線の置き場（配列のメモリを使い回す）
	std::vector<Vector3> nodePositions;  // ノードごとのワールドの位置（1体分）
	bool showGizmos = true;              // 線を出すか
};
std::unique_ptr<AnimationState> state;

// ===== Entityの状態を探す（無ければ nullptr）=====
AnimatorState* FindAnimator(Handle<Entity> entity) { 
	const auto it = std::ranges::find(state->animators, entity, &AnimatorState::entity);
	return (it != state->animators.end()) ? &*it : nullptr;
}

// ===== Entityの状態を探す（無ければ nullptr）=====
AnimatorState& FindOrAddAnimator(Handle<Entity> entity) { 
	if (AnimatorState* animator = FindAnimator(entity)) {
		return *animator;
	}
	state->animators.push_back({entity});
	return state->animators.back();
}

// ===== ファイルの中に、その番号のアニメーションがあるか（無い番号で GetAnimation を呼ぶと、警告が毎フレーム出るので先に確かめる =====
bool HasClip(uint32_t animationHandle, int clipIndex) {
	const uint32_t count = AnimationManager::GetAnimationCount(animationHandle);
	return clipIndex >= 0 && static_cast<uint32_t>(clipIndex) < count;
}

// ===== Componentで選んでいるアニメーションを最初の時刻から持ち直す =====
void Restart(AnimatorState& animator, const AnimatorComponent& component, bool play) {
	animator.animationHandle = component.animationHandle;
	animator.clipIndex = component.clipIndex;
	// 未選択・無い番号: 空の AnimationPlayerにする（ノードの行列が空 → モデルに入っている形で描く）
	if (!HasClip(component.animationHandle, component.clipIndex)) {
		animator.player = AnimationPlayer{};
		return;
	}
	animator.player.Play(component.animationHandle, static_cast<uint32_t>(component.clipIndex), component.loop);
	if (!play) {
		animator.player.Stop(); // アニメーションは覚えたまま、最初の時刻で止まる
	}
}

// ===== Inspector と 保存 =====
// Animatorの見せ方。Inspectorとシーンファイルの両方がこの並びを使う
// 保存の名前はメンバ名にし、Inspector には「表示###保存の名前」で読みやすい名前を出す
void DescribeAnimator(ComponentUI& ui, AnimatorComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("オフにすると動かさない（モデルに入っている形で描く）"));
	ui.AssetField("Animation###animation", value.animationHandle, AssetType::Animation, Tip("resources の下の .gltf / .glb / .fbx（アニメーションが入っている物）"));

	// --- クリップ：名前で選ぶ・名前で保存する ---
	// Component は文字を持てないので、中は番号（clipIndex）のまま。保存・Inspector のときだけ名前に直す
	const uint32_t clipCount = AnimationManager::GetAnimationCount(value.animationHandle); // 未選択なら0
	std::vector<std::string_view> clipNames;
	clipNames.reserve(clipCount);
	for (uint32_t index = 0; index < clipCount; ++index) {
		clipNames.push_back(AnimationManager::GetAnimation(value.animationHandle, index)->name); // 名前の文字は AnimationManager が持っている
	}
	if (!clipNames.empty()) {
		// 読み込みのときは、AssetField（1つ上）でファイルを読んだ後なので、名前の一覧はもうできている
		size_t clipIndex = HasClip(value.animationHandle, value.clipIndex) ? static_cast<size_t>(value.clipIndex) : 0;
		ui.ChoiceField("Clip###clip", clipIndex, clipNames, Tip("ファイルの中のどのアニメーションか（保存は名前）"));
		value.clipIndex = static_cast<int>(clipIndex);
	}

	// --- 選んでいるアニメーションの長さ（飾りなので保存されない） ---
	const Animation* clip = HasClip(value.animationHandle, value.clipIndex) ? AnimationManager::GetAnimation(value.animationHandle, static_cast<uint32_t>(value.clipIndex)) : nullptr;
	if (clip != nullptr) {
		const std::string info = std::format("{:.2f} 秒 ／ このファイルに {} 個", clip->duration, clipCount);
		ui.Label(info.c_str());
	}

	ui.Field("Speed###speed", value.speed, Drag(0.01f).Tip("1で等速、2で倍速、負で逆再生"));
	ui.Field("Loop###loop", value.loop, Tip("ループするか"));
	ui.Field("Play On Start###playOnStart", value.playOnStart, Tip("Play を押したとき（シーンが始まったとき）に再生する。オフならゲームから AnimationSystem::Play"));

	// --- 停止中のプレビュー ---
	ui.Separator("Preview");
	const float duration = (clip != nullptr) ? clip->duration : 0.0f;
	const FieldStyle timeStyle = (duration > 0.0f) ? Range(0.0f, duration) : AtLeast(0.0f);
	ui.Field("Preview Time###previewTime", value.previewTime, timeStyle.Tip("停止中に見せる時刻（秒）。つまんで動かすとポーズが変わる"));
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void AnimationSystem::Initialize() { 
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<AnimationState>();
	state->gizmoLines.fadeStartDistance = kNoFadeStart;
	state->gizmoLines.fadeEndDistance = kNoFadeEnd;
	state->gizmoLines.depthMode = DepthMode::Disable;

	// Animatorの置き場所・保存・保存・Inspectorを登録
	EntityManager::RegisterComponent<AnimatorComponent>();
	ComponentSerializer::Register<AnimatorComponent>("Animator", &DescribeAnimator);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<AnimatorComponent>>("Animator", "Animation", &DescribeAnimator));
}

void AnimationSystem::Release() { state.reset(); }


//=============================================================================
// 毎フレーム
//=============================================================================
void AnimationSystem::Update(bool isGameRunning, float deltaTime) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	std::vector<AnimatorState>& animators = state->animators;

	// ===== 停止中 =====
	if (isGameRunning != state->wasGameRunning) {
		animators.clear();
		state->wasGameRunning = isGameRunning;
	}

	// ===== 消えた・外した Animatorを一覧から外す =====
	std::erase_if(animators, [](const AnimatorState& animator) { 
		return EntityManager::Get<AnimatorComponent>(animator.entity) == nullptr; 
	});

	// ===== 初めて見つけた Animatorは一覧に足す。時刻を進めて、ノードの行列を作り直す =====
	EntityManager::ForEach<AnimatorComponent>([&animators, isGameRunning, deltaTime](Handle<Entity> entity, const AnimatorComponent& component) {
		if (!component.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return; // 状態は残す（有効に戻したら続きから）
		}

		// --- 一覧から探す。無ければ足す（再生中なら Play on Start）---
		AnimatorState* animator = FindAnimator(entity);
		if (animator == nullptr) {
			animators.push_back({entity});
			animator = &animators.back();
			Restart(*animator, component, isGameRunning && component.playOnStart);
		} else if (component.animationHandle != animator->animationHandle || component.clipIndex != animator->clipIndex) {
			// Inspector・ゲームからアニメーションから選び直した → 新しい方を最初から
			// 再生中のゲームなら、再生していた・Play On Start のときは再生する（未選択から選んでときも動き出すように）
			Restart(*animator, component, isGameRunning && (animator->player.IsPlaying() || component.playOnStart));
		}

		// --- 時刻 ---
		AnimationPlayer& player = animator->player;
		player.SetSpeed(component.speed);
		player.SetLoop(component.loop);
		if (isGameRunning) {
			player.Update(deltaTime); // 一時停止中は deltaTime が0なので進まない
		} else {
			player.SetTime(component.previewTime); // 停止中はInspectorの時刻のポーズ
		}

		// --- ノードの行列（同じEntityのModelRendererのモデル）---
		// Model Renderer が無い・モデルが未選択なら作らない
		const ModelRendererComponent* render = EntityManager::Get<ModelRendererComponent>(entity);
		if (render != nullptr && render->modelHandle != 0) {
			player.ApplyToModel(render->modelHandle);
		}
	});
}

// ===== ノードの行列 =====
const std::vector<Matrix4x4>* AnimationSystem::GetNodeMatrices(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	// --- 早期リターン ---
	// コンポーネントを持っているか
	const AnimatorComponent* component = EntityManager::Get<AnimatorComponent>(entity);
	if (component == nullptr || !component->enabled) {
		return nullptr;
	}
	// アニメーションを持っているか
	const AnimatorState* animator = FindAnimator(entity);
	if (animator == nullptr || animator->player.GetNodeMatrices().empty()) {
		return nullptr;
	}
	return &animator->player.GetNodeMatrices();
}


//=============================================================================
// ゲームから
//=============================================================================
// ===== 最初から再生 =====
bool AnimationSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	const AnimatorComponent* component = EntityManager::Get<AnimatorComponent>(entity);
	// Animatorが無い（RequestAdd した直後は、次のフレームまで付いていない）・無効なEntity
	if (component == nullptr || !EntityManager::IsActiveInHierarchy(entity)) {
		return false;
	}
	// 一覧に入れるので、この後のUpdateがPlay On Startでもう一度からにすることはない
	AnimatorState& animator = FindOrAddAnimator(entity);
	Restart(animator, *component, true);
	return animator.player.IsPlaying();
}

// ===== 止めて最初の時刻に戻す =====
void AnimationSystem::Stop(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	if (AnimatorState* animator = FindAnimator(entity)) {
		animator->player.Stop();
	}
}

// ===== 再生中か（ループしないアニメが最後まで行ったら false）=====
bool AnimationSystem::IsPlaying(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	const AnimatorState* animator = FindAnimator(entity);
	return animator != nullptr && animator->player.IsPlaying();
}

// ===== 持っているAnimationPlayerを渡す =====
AnimationPlayer* AnimationSystem::GetPlayer(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	AnimatorState* animator = FindAnimator(entity);
	return (animator != nullptr) ? &animator->player : nullptr;
}


//=============================================================================
// ギズモ（骨）
//=============================================================================
void AnimationSystem::DrawGizmos(Camera* camera) {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	if (!state->showGizmos) {
		return;
	}
	Renderer::LineListConfig& gizmo = state->gizmoLines;
	gizmo.lines.clear(); // 中身だけ消して、メモリを確保
	gizmo.camera = camera;
	std::vector<Vector3>& positions = state->nodePositions;

	EntityManager::ForEach<ModelRendererComponent>([&gizmo, &positions](Handle<Entity> entity, const ModelRendererComponent& render) {
		// --- 早期リターン ---
		if (!render.enabled || render.modelHandle == 0 || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		const ModelManager::ModelAsset* asset = ModelManager::GetModelAsset(render.modelHandle);
		if (transform == nullptr || asset == nullptr) {
			return;
		}
		const std::vector<ModelManager::Node>& nodes = asset->nodes;
		// 骨のないモデルは書かない
		if (std::ranges::none_of(nodes, &ModelManager::Node::isJoint)) {
			return;
		}

		// --- ノードごとのワールドの位置 ---
		// Animator があれば動いているポーズ（ModelRenderSystemが描いている物と同じ行列）、無ければモデルに入っている形
		const std::vector<Matrix4x4>* nodeMatrices = AnimationSystem::GetNodeMatrices(entity);
		const bool useAnimated = (nodeMatrices != nullptr && nodeMatrices->size() == nodes.size());
		positions.resize(nodes.size());
		// 骨全体を囲む箱（箱の大きさを決める）
		Vector3 boundsMin = {FLT_MAX, FLT_MAX, FLT_MAX};
		Vector3 boundsMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			const Matrix4x4& nodeMatrix = useAnimated ? (*nodeMatrices)[nodeIndex] : nodes[nodeIndex].worldMatrix;
			// ノードの原点（0, 0, 0）を、ノードの行列 × Entity のワールド行列で動かした所（行ベクトルなので「ノード × Entity」の順）
			positions[nodeIndex] = TransformPoint({0.0f, 0.0f, 0.0f}, nodeMatrix * transform->worldMatrix);
			if (nodes[nodeIndex].isJoint) {
				const Vector3& p = positions[nodeIndex];
				boundsMin = {(std::min)(boundsMin.x, p.x), (std::min)(boundsMin.y, p.y), (std::min)(boundsMin.z, p.z)};
				boundsMax = {(std::max)(boundsMax.x, p.x), (std::max)(boundsMax.y, p.y), (std::max)(boundsMax.z, p.z)};
			}
		}
		const float radius = (std::max)(Length(boundsMax - boundsMin) * kJointRadiusRate, kMinJointRadius);

		// --- 骨は球、親子は線 ---
		for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
			const ModelManager::Node& node = nodes[nodeIndex];
			if (node.isJoint) {
				GizmoLines::PushWireSphere(gizmo.lines, positions[nodeIndex], radius, kJointColor, kJointCircleDivision);
			}
			// 親も骨か、骨の祖先なら線でつなぐ（骨どうしが兄弟で、親が骨でない Armatureボようなノードでもつながるように）
			if (node.isSkeleton && node.parent != ModelManager::kInvalidNode && nodes[node.parent].isSkeleton) {
				gizmo.lines.push_back({positions[node.parent], positions[nodeIndex], kBoneColor});
			}
		}
	});
	Renderer::DrawLines(gizmo); // 線が0本なら中で何もしない
	gizmo.camera = nullptr;     // フレームをまたいで持たない
}

bool& AnimationSystem::GetShowGizmos() {
	MY_ASSERT_MSG(state != nullptr, "AnimationSystem::Initialize()を先に呼んでください");
	return state->showGizmos;
}