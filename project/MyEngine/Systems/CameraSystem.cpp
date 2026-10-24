#include "CameraSystem.h"

#include <cmath>
#include <memory>
#include <algorithm>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Components/RigidbodyComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"

namespace {
constexpr float kAspectRatio = 1280.0f / 720.0f;       // 縦横比（Game ビューの描画先・スプライトの基準と同じ 16:9）
constexpr float kMinFovY = 1.0f;                       // 視野角の下限（度）
constexpr float kMaxFovY = 179.0f;                     // 視野角の上限（度）。180度以上は透視投影で作れない
const Vector3 kDefaultPosition = {0.0f, 0.0f, -10.0f}; // Camera Component が無いときの位置（原点を正面から見る）

/// <summary>
/// CameraSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct CameraState {
	// ゲームの絵を撮るカメラ（GetMainCamera はいつもこれを返す）
	// shared_ptr にするのは、パーティクル（ParticleManager）が weak_ptr で「見ているだけ」にできるように
	std::shared_ptr<Camera> camera = std::make_shared<Camera>();
	Handle<Entity> mainEntity;      // 今使っている Camera Component の Entity
	bool hasLoggedNoCamera = false; // 「Camera Component が無い」を知らせたか（続けて何度も出さない）
};
std::unique_ptr<CameraState> state;

// 写り方をカメラへ（視野角は度で持っているので、ラジアンに直して渡す）
void ApplyProjection(const CameraComponent& component) {
	const float fovY = std::clamp(component.fovY, kMinFovY, kMaxFovY) * kDegToRad;
	const float nearZ = (std::max)(component.nearZ, 0.001f);
	const float farZ = (std::max)(component.farZ, nearZ + 0.01f); // far が near より手前だと、何も映らない
	state->camera->Initialize(fovY, kAspectRatio, nearZ, farZ);    // Initialize の中で行列も作り直す
}

// Camera Component が無いときの、既定のカメラ
void UseDefaultCamera() {
	state->camera->SetTranslation(kDefaultPosition);
	state->camera->SetRotation({0.0f, 0.0f, 0.0f});
	state->camera->SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(CameraComponent{});
}

// Camera の見せ方。Inspector とシーンファイルの両方がこの並びを使う（保存の名前はメンバ名）
void DescribeCamera(ComponentUI& ui, CameraComponent& value) {
	ui.Field("Field Of View###fovY", value.fovY, Range(kMinFovY, kMaxFovY).Tip("縦の視野角（度）。小さいほど望遠"));
	ui.Field("Near###nearZ", value.nearZ, AtLeast(0.001f).Tip("これより近い物は映らない"));
	ui.Field("Far###farZ", value.farZ, AtLeast(0.01f).Tip("これより遠い物は映らない"));
	ui.Field("Priority###priority", value.priority, Tip("有効な Camera が複数あるとき、大きい方を使う（切り替えはこの値か、Entity の有効・無効で）"));
}

//=============================================================================
// Camera Follow（カメラの追いかけ）
//=============================================================================
// Camera Follow の見せ方（実行中だけの値は並べない＝保存しない）
void DescribeCameraFollow(ComponentUI& ui, CameraFollowComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと追わない（置いた位置のまま）"));
	ui.Field("Target###target", value.target, Tip("追う Entity（Hierarchy からドラッグ）。再生中だけ追う。動かすのはカメラの X・Y だけ（Z と向きは置いたまま）"));
	ui.Field("Mode###mode", value.mode, Tip("追い方。Center：追う物を真ん中に / LookAhead：進む向きの先を見せる（速さ × Time。止まると真ん中に戻る）"));
	// --- 選んだ追い方の値だけを見せる（保存も、使っている方だけが書かれる。Collider の Shape と同じ）---
	switch (value.mode) {
	case CameraFollowMode::Center:
		break; // Center だけの値は無い
	case CameraFollowMode::LookAhead:
		ui.Field("Look Ahead Time###lookAheadTime", value.lookAheadTime, AtLeast(0.0f).Tip("何秒先を見せるか（先読み ＝ 追う物の速さ × これ）。X 0.4 なら 7m/s で 2.8m 先。Y を 0 にすると、跳んでも上下に先読みしない"));
		ui.Field("Look Ahead Max###lookAheadMax", value.lookAheadMax, AtLeast(0.0f).Tip("先読みの上限（m）。吹っ飛びのような速い動きで、先へ行きすぎないように"));
		ui.Field("Look Ahead Smooth Time###lookAheadSmoothTime", value.lookAheadSmoothTime, AtLeast(0.0f).Tip("先読みが変わるまでの時間の目安（秒）。速さが急に変わっても（振り返る・止まる）、画面がガクッと動かない。0 ならすぐ変わる"));
		break;
	}
	ui.Separator("Follow");
	ui.Field("Offset###offset", value.offset, Tip("追う物から、画面の真ん中をどれだけずらすか（m）。Y を上げると、足元が広く見える"));
	ui.Field("Dead Zone###deadZone", value.deadZone, AtLeast(0.0f).Tip("遊び（m）。画面の真ん中から左右・上下にこの範囲で動いても、カメラは動かない（跳ぶたびに画面が上下しないように）"));
	ui.Field("Ground Snap Y###groundSnapY", value.groundSnapY, Tip("追う物が地面にいる間は、上下の遊びを使わずに合わせる（跳んでも画面は揺れず、段に乗ると上がる・降りると下がる）。追う物に Rigidbody が要る"));
	ui.Field("Smooth Time###smoothTime", value.smoothTime, AtLeast(0.0f).Tip("追いつくまでの時間の目安（秒）。大きいほどゆっくり。0 ならすぐ合わせる"));
	ui.Field("Max Lag###maxLag", value.maxLag, AtLeast(0.0f).Tip("遅れの上限（m）。速く動いても、追う物が画面の真ん中からこれより離れない"));
	ui.Separator("Bounds");
	ui.Field("Use Bounds###useBounds", value.useBounds, Tip("画面の端が、Min〜Max の外を写さない（ステージの端でカメラが止まる）"));
	ui.Field("Min###boundsMin", value.boundsMin, Tip("写してよい範囲の左下（ワールドの X・Y）。追う物の奥行きで測る"));
	ui.Field("Max###boundsMax", value.boundsMax, Tip("写してよい範囲の右上（ワールドの X・Y）"));
}

// ===== なめらかに近づけるときの、1フレームに進む割合（残りの差 × これ だけ進む）=====
// smoothTime が 0 なら 1（すぐ合わせる）
float CalcSmoothRate(float deltaTime, float smoothTime) { return (smoothTime > 0.0f) ? 1.0f - std::exp(-deltaTime / smoothTime) : 1.0f; }

// ===== 追い方で決まる、focus のずれ（先読み）の目標 =====
Vector2 CalcWantedLookAhead(const CameraFollowComponent& follow, const Vector2& velocity) {
	switch (follow.mode) {
	case CameraFollowMode::Center:
		return {0.0f, 0.0f}; // ずらさない（LookAhead から切り替えたときは、0 へなめらかに戻る）
	case CameraFollowMode::LookAhead:
		// 速さ × 何秒先か（t = p + α・v の α・v）。速いほど先を見せ、止まると 0（真ん中に戻る）。上限で縛る
		return {
		    std::clamp(velocity.x * follow.lookAheadTime.x, -follow.lookAheadMax.x, follow.lookAheadMax.x),
		    std::clamp(velocity.y * follow.lookAheadTime.y, -follow.lookAheadMax.y, follow.lookAheadMax.y),
		};
	}
	MY_ASSERT_MSG(false, "知らない CameraFollowMode です");
	return {0.0f, 0.0f};
}

// ===== 1つの軸で、遊びの外へ出た分だけ真ん中を動かす（focus が「真ん中 ± 遊び」の中なら、真ん中のまま）=====
float ApplyDeadZone(float center, float focus, float deadZone) {
	if (focus > center + deadZone) {
		return focus - deadZone; // 右（上）へはみ出した：はみ出した分だけ追う
	}
	if (focus < center - deadZone) {
		return focus + deadZone; // 左（下）へはみ出した
	}
	return center; // 遊びの中：動かない
}

// ===== 1つの軸で、画面の端が min〜max の外を写さないように、真ん中を寄せる =====
// halfView は画面の半分の幅（高さ）。範囲が画面より狭いときは、どこに置いてもはみ出すので、範囲の真ん中にする
float ApplyBounds(float center, float halfView, float min, float max) {
	const float low = min + halfView;  // 真ん中がこれより小さいと、画面の左（下）の端が min の外を写す
	const float high = max - halfView; // 真ん中がこれより大きいと、画面の右（上）の端が max の外を写す
	if (low > high) {
		return (min + max) * 0.5f;
	}
	return std::clamp(center, low, high);
}

// ===== カメラ1つ分の追いかけ。動かしたら true =====
bool UpdateFollowOne(Handle<Entity> entity, CameraFollowComponent& follow, float deltaTime) {
	// --- 早期リターン ---
	const Handle<Entity> targetEntity = EntityManager::Find(follow.target);
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	const TransformComponent* targetTransform = EntityManager::Get<TransformComponent>(targetEntity);
	if (transform == nullptr || targetTransform == nullptr) {
		return false; // 追う物が入っていない・もう居ない
	}
	const Vector3 cameraPosition = GetWorldPosition(*transform);
	const Vector3 targetPosition = GetWorldPosition(*targetTransform);
	const Vector2 target = {targetPosition.x, targetPosition.y};

	// --- 1. 先読み（追い方で決まる focus のずれ）---
	// 速さは位置の変化から出す（Rigidbody が無い物・スクリプトで動かす物でも使える。壁に止められている間は 0）
	// 目標の先読みへ、指数でなめらかに近づける（速さはフレームごとにガタつくので、そのままだと画面が震える）
	if (follow.isFollowing) {
		const Vector2 velocity = (target - follow.lastTargetPosition) / deltaTime;
		const Vector2 wanted = CalcWantedLookAhead(follow, velocity);
		follow.currentLookAhead += (wanted - follow.currentLookAhead) * CalcSmoothRate(deltaTime, follow.lookAheadSmoothTime);
	}
	follow.lastTargetPosition = target;

	// --- 2. 画面の真ん中に置きたい点（focus）---
	const Vector2 focus = target + follow.offset + follow.currentLookAhead;

	// --- 3. 新しい真ん中 ---
	Vector2 center = {cameraPosition.x, cameraPosition.y};
	if (!follow.isFollowing) {
		// 追いかけ始め（Play・シーンを読んだ直後）：置いた位置から流れてこないように、すぐ focus へ
		center = focus;
		follow.isFollowing = true;
	} else {
		// 遊びの外へ出た分だけ追う。Y は、追う物が地面にいる間は遊びを使わずに合わせる（Ground Snap Y）
		// 遊びだけだと、跳んで遊びの上へはみ出した分カメラが上がり、着地しても遊びの中なので戻らない（跳ぶたびに少しずつずれていく）
		const RigidbodyComponent* targetBody = EntityManager::Get<RigidbodyComponent>(targetEntity);
		const bool isSnapY = follow.groundSnapY && targetBody != nullptr && targetBody->isGrounded;
		const Vector2 wanted = {ApplyDeadZone(center.x, focus.x, follow.deadZone.x), isSnapY ? focus.y : ApplyDeadZone(center.y, focus.y, follow.deadZone.y)};
		// なめらかに近づける（指数補間。1フレームに「残りの差 × t」だけ進む）
		center += (wanted - center) * CalcSmoothRate(deltaTime, follow.smoothTime);
		// 遅れすぎないように縛る（吹っ飛びのような速い動きでも、追う物が画面から出ない）
		center.x = std::clamp(center.x, focus.x - follow.maxLag.x, focus.x + follow.maxLag.x);
		center.y = std::clamp(center.y, focus.y - follow.maxLag.y, focus.y + follow.maxLag.y);
	}

	// --- 4. ステージの端で止める（画面の端が、範囲の外を写さない）---
	if (follow.useBounds) {
		// 追う物の奥行きでの、画面の半分の高さ・幅（カメラは +Z を向いている前提）。半分の高さ ＝ 距離 × tan(縦の視野角の半分)
		const CameraComponent* camera = EntityManager::Get<CameraComponent>(entity);
		const float fovY = std::clamp(camera ? camera->fovY : CameraComponent{}.fovY, kMinFovY, kMaxFovY) * kDegToRad;
		const float halfHeight = std::abs(targetPosition.z - cameraPosition.z) * std::tan(fovY * 0.5f);
		const float halfWidth = halfHeight * kAspectRatio;
		center.x = ApplyBounds(center.x, halfWidth, follow.boundsMin.x, follow.boundsMax.x);
		center.y = ApplyBounds(center.y, halfHeight, follow.boundsMin.y, follow.boundsMax.y);
	}

	// --- 5. 動いた量を Transform へ（親の下に置いたカメラでも、ワールドで動いた量だけ動く）---
	const Vector3 move = {center.x - cameraPosition.x, center.y - cameraPosition.y, 0.0f};
	if (LengthSq(move) == 0.0f) {
		return false;
	}
	EntityManager::TranslateWorld(entity, move);
	return true;
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void CameraSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<CameraState>();
	UseDefaultCamera(); // 最初の Update より前（シーンの Initialize）に使われても、ちゃんとしたカメラを返せるように

	EntityManager::RegisterComponent<CameraComponent>();
	ComponentSerializer::Register<CameraComponent>("Camera", &DescribeCamera);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraComponent>>("Camera", "Camera", &DescribeCamera));

	EntityManager::RegisterComponent<CameraFollowComponent>();
	ComponentSerializer::Register<CameraFollowComponent>("Camera Follow", &DescribeCameraFollow);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<CameraFollowComponent>>("Camera Follow", "Camera", &DescribeCameraFollow));
}

void CameraSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
bool CameraSystem::UpdateFollow(float deltaTime) {
	if (deltaTime <= 0.0f) {
		return false; // 一時停止・ヒットストップ中（追う物も止まっている）
	}
	bool isMoved = false;
	EntityManager::ForEach<CameraFollowComponent>([&isMoved, deltaTime](Handle<Entity> entity, CameraFollowComponent& follow) {
		if (!follow.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (UpdateFollowOne(entity, follow, deltaTime)) {
			isMoved = true;
		}
	});
	return isMoved;
}

void CameraSystem::Update() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");

	// ===== 1. 使う Camera を選ぶ（有効な物の中で priority が一番大きい物。同じなら先に見つけた物）=====
	Handle<Entity> best;
	CameraComponent bestComponent;
	EntityManager::ForEach<CameraComponent>([&](Handle<Entity> entity, const CameraComponent& component) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (!best.IsValid() || component.priority > bestComponent.priority) {
			best = entity;
			bestComponent = component;
		}
	});
	state->mainEntity = best;

	// ===== 2. 無ければ既定のカメラ =====
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(best);
	if (transform == nullptr) {
		if (!state->hasLoggedNoCamera) {
			LogManager::Log("Camera Component が無いので、既定のカメラ（(0, 0, -10) から +Z を向く）で撮ります。Hierarchy の ＋ → Camera で置けます");
			state->hasLoggedNoCamera = true;
		}
		UseDefaultCamera();
		return;
	}
	state->hasLoggedNoCamera = false;

	// ===== 3. 位置・向きはワールド行列から（親の下に置いても、見えている場所・向きから撮る）=====
	// 大きさは写さない（ビュー行列に大きさが混ざると、絵が伸び縮みする）
	const Transform world = DecomposeAffineMatrix(transform->worldMatrix);
	state->camera->SetTranslation(world.translation);
	state->camera->SetRotation(world.rotation);
	state->camera->SetScale({1.0f, 1.0f, 1.0f});
	ApplyProjection(bestComponent);
}

//=============================================================================
// 取得
//=============================================================================
Camera* CameraSystem::GetMainCamera() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return state->camera.get();
}

std::shared_ptr<Camera> CameraSystem::GetSharedMainCamera() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return state->camera;
}

Handle<Entity> CameraSystem::GetMainCameraEntity() {
	MY_ASSERT_MSG(state != nullptr, "CameraSystem::Initialize()を先に呼んでください");
	return state->mainEntity;
}

const TransformComponent* CameraSystem::GetFollowTargetTransform() {
	// Target の Entity → Transform（どこかで無ければ nullptr）
	return EntityManager::Get<TransformComponent>(GetFollowTargetEntity());
}

Handle<Entity> CameraSystem::GetFollowTargetEntity() {
	// メインカメラの Camera Follow → その Target の Entity（どこかで無ければ無効な Handle）
	const CameraFollowComponent* follow = EntityManager::Get<CameraFollowComponent>(GetMainCameraEntity());
	return (follow != nullptr) ? EntityManager::Find(follow->target) : Handle<Entity>{};
}