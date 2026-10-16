#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Gizmos/GizmoLines.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

namespace {
constexpr float kPI = std::numbers::pi_v<float>;
constexpr uint32_t kCircleDivision = 24;       // 球の円を何本の線で描くか
constexpr uint32_t kIdleColor = 0x66FF66FF;    // 当たっていない Collider の線の色（緑）
constexpr uint32_t kTriggerColor = 0x66CCFFFF; // 当たっていない Is Trigger の Collider の線の色（水色。すり抜ける物と見分ける）
constexpr uint32_t kHitColor = 0xFF4444FF;     // 当たっている Collider の線の色（赤）
constexpr float kNoFadeStart = 10000.0f;       // 距離でフェードさせないための値（LightGizmoと同じ）
constexpr float kNoFadeEnd = 20000.0f;

// ===== 判定に使う形 =====

/// <summary>
/// 重なっている2つ。first が小さい方（同じ組を「AとB」「BとA」の2回数えないように、並びをそろえる）
/// </summary>
struct HitPair {
	Handle<Entity> first;
	Handle<Entity> second;
	bool isTrigger = false; // どちらかが Is Trigger（途中で切り替えたら、前の当たりは Exit・新しい当たりは Enter になる）
};

/// <summary>
/// CollisionSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct CollisionState {
	std::vector<WorldCollider> colliders; // このフレームに判定した形（ギズモもこれを描く＝見えている線と判定が同じ）
	std::vector<HitPair> currentPairs;    // このフレームに重なっていた組（並べてある）
	std::vector<HitPair> previousPairs;   // 前のフレームに重なっていた組（並べてある）
	std::vector<CollisionHit> hits;       // 当たりの一覧（self の順）
	Renderer::LineListConfig gizmoLines;  // ギズモの線の置き場（配列のメモリを使い回す）
	bool showGizmos = true;               // 線を出すか
};
std::unique_ptr<CollisionState> state;

// Handle の並び順（番号 → 世代）。組の並びをそろえるのと、二分探索に使う
bool HandleLess(const Handle<Entity>& a, const Handle<Entity>& b) { return std::tie(a.index, a.generation) < std::tie(b.index, b.generation); }

// 組の並び順（first → second → isTrigger）
bool PairLess(const HitPair& a, const HitPair& b) {
	if (a.first != b.first) {
		return HandleLess(a.first, b.first);
	}
	if (a.second != b.second) {
		return HandleLess(a.second, b.second);
	}
	return a.isTrigger < b.isTrigger;
}

// 2つのEntityから、並びをそろえた組を作る
HitPair MakePair(Handle<Entity> a, Handle<Entity> b, bool isTrigger) { return HandleLess(a, b) ? HitPair{a, b, isTrigger} : HitPair{b, a, isTrigger}; }

// Entityの大きさ（ワールド行列の1〜3行目の長さ＝ローカルのX・Y・Z軸がどれだけ伸びているか。親の大きさも入っている）
Vector3 GetWorldScale(const Matrix4x4& m) {
	return {
	    Length(Vector3{m.m[0][0], m.m[0][1], m.m[0][2]}),
	    Length(Vector3{m.m[1][0], m.m[1][1], m.m[1][2]}),
	    Length(Vector3{m.m[2][0], m.m[2][1], m.m[2][2]}),
	};
}

// 形を distance だけ太らせる（球は半径を、箱は6つの面を外へ）
WorldCollider Inflate(const WorldCollider& collider, float distance) {
	WorldCollider result = collider;
	const Vector3 grow = {distance, distance, distance};
	result.sphere.radius += distance;
	result.box = {collider.box.min - grow, collider.box.max + grow};
	return result;
}

// 2つが重なっているか（形の組み合わせで、使う関数を選ぶ）
bool IsOverlapping(const WorldCollider& a, const WorldCollider& b) {
	const bool aIsSphere = (a.shape == ColliderShape::Sphere);
	const bool bIsSphere = (b.shape == ColliderShape::Sphere);
	if (aIsSphere && bIsSphere) {
		return Collision::SphereSphere(a.sphere, b.sphere);
	}
	if (!aIsSphere && !bIsSphere) {
		return Collision::AABBAABB(a.box, b.box);
	}
	// 球と箱（どちらが球でも、球を先にして渡す）
	return aIsSphere ? Collision::SphereAABB(a.sphere, b.box) : Collision::SphereAABB(b.sphere, a.box);
}

// 2つが kContactOffset より近いか（触れているだけの物も「当たっている」にする）
// Rigid body は壁や床から押し戻されて「ちょうど触れた所」で止まるので、重なりだけを見ると、乗っている床との当たりが毎フレーム切れたりつながったりする
bool IsInContact(const WorldCollider& a, const WorldCollider& b) {
	const float half = CollisionSystem::kContactOffset * 0.5f; // 両方を半分ずつ太らせる
	return IsOverlapping(Inflate(a, half), Inflate(b, half));
}

// 1組を「両方から見た2件」にして足す。消えたEntityから見た分は足さない（読む人がいない）
void AddHits(std::vector<CollisionHit>& hits, const HitPair& pair, HitPhase phase) {
	if (EntityManager::IsAlive(pair.first)) {
		hits.push_back({pair.first, pair.second, phase, pair.isTrigger});
	}
	if (EntityManager::IsAlive(pair.second)) {
		hits.push_back({pair.second, pair.first, phase, pair.isTrigger});
	}
}

// このEntityが、今重なっているか（当たり始め・当たっている。離れた＝Exit は入れない）
bool IsTouching(Handle<Entity> entity) {
	for (const CollisionHit& hit : CollisionSystem::GetHits(entity)) {
		if (hit.phase != HitPhase::Exit) {
			return true;
		}
	}
	return false;
}

// ===== 線（ギズモ）=====
void PushBox(std::vector<Renderer::LineSegment>& lines, const AABB& box, uint32_t color) {
	// 8つの角。番号 i の 1・2・4 のビットで、x・y・z に min と max のどちらを使うかを決める
	Vector3 corners[8];
	for (uint32_t i = 0; i < 8; ++i) {
		corners[i] = {
		    (i & 1) ? box.max.x : box.min.x,
		    (i & 2) ? box.max.y : box.min.y,
		    (i & 4) ? box.max.z : box.min.z,
		};
	}
	// ビットが1つだけ違う角どうしを結ぶと、辺になる（1つの軸に4本 × 3軸 ＝ 12本）
	for (uint32_t i = 0; i < 8; ++i) {
		for (uint32_t bit = 1; bit < 8; bit <<= 1) {
			if ((i & bit) == 0) {
				lines.push_back({corners[i], corners[i | bit], color});
			}
		}
	}
}

// ===== Inspector と 保存 =====

// Collider の見せ方。Inspector とシーンファイルの両方がこの並びを使う（COMPONENT(...) の中身と同じ書き方）
// 保存の名前はエンジンの他の型にそろえてメンバ名にし、Inspector には「表示###保存の名前」で読みやすい名前を出す
void DescribeCollider(ComponentUI& ui, ColliderComponent& value) {
	ui.Field("Enabled###enabled", value.enabled);
	ui.Field("Is Trigger###isTrigger", value.isTrigger, Tip("すり抜ける（Rigidbody を押し戻さない）。Script には OnTrigger〜、外すと OnCollision〜"));
	ui.Field("Shape###shape", value.shape);
	ui.Field("Center###center", value.center, Tip("Entityの位置からのずれ（Entityの回転と大きさも掛かる）"));
	// 形に合う大きさだけを見せる（保存も、使っている方だけが書かれる）
	if (value.shape == ColliderShape::Sphere) {
		ui.Field("Radius###radius", value.radius, AtLeast(0.0f).Tip("Entityの大きさの、一番大きい軸を掛けて判定する"));
	} else {
		ui.Field("Size###size", value.size, AtLeast(0.0f).Tip("幅・高さ・奥行き。Entityを回しても、箱は回らない"));
	}
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void CollisionSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<CollisionState>();
	// 線は距離でフェードさせない
	state->gizmoLines.fadeStartDistance = kNoFadeStart;
	state->gizmoLines.fadeEndDistance = kNoFadeEnd;

	// Collider の置き場所・保存・Inspector を登録する。見せ方は DescribeCollider 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<ColliderComponent>();
	ComponentSerializer::Register<ColliderComponent>("Collider", &DescribeCollider);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<ColliderComponent>>("Collider", "Physics", &DescribeCollider));
}

void CollisionSystem::Release() { state.reset(); }

//=============================================================================
// 判定
//=============================================================================
void CollisionSystem::Update() {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");

	// ===== 1. 判定に使う形を集める（有効なEntityの、有効な Collider だけ）=====
	state->colliders.clear();
	EntityManager::ForEach<ColliderComponent>([](Handle<Entity> entity, const ColliderComponent& collider) {
		if (!collider.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		if (transform == nullptr) {
			return;
		}
		state->colliders.push_back(CalcWorldCollider(entity, collider, *transform));
	});

	// ===== 2. 全部の組を調べる（総当たり。n個なら n×(n-1)/2 回。数百個まではこれで足りる）=====
	// 触れているだけの物も当たっている扱い（IsInContact）。どちらかが Is Trigger なら、すり抜ける当たり
	// 親子の Collider 同士は当たらない（自分の子に付けた剣の Collider で、自分に当たり続けないように）
	std::vector<HitPair>& current = state->currentPairs;
	current.clear();
	const std::vector<WorldCollider>& colliders = state->colliders;
	for (size_t i = 0; i < colliders.size(); ++i) {
		for (size_t j = i + 1; j < colliders.size(); ++j) {
			if (IsInContact(colliders[i], colliders[j]) && !EntityManager::IsRelated(colliders[i].entity, colliders[j].entity)) {
				current.push_back(MakePair(colliders[i].entity, colliders[j].entity, colliders[i].isTrigger || colliders[j].isTrigger));
			}
		}
	}
	std::ranges::sort(current, PairLess);

	// ===== 3. 前のフレームの組と見比べて、当たり始め・当たっている・離れた に分ける =====
	// どちらの列も同じ順に並べてあるので、先頭から1つずつ見比べれば「両方にある」「片方にしか無い」が分かる
	const std::vector<HitPair>& previous = state->previousPairs;
	std::vector<CollisionHit>& hits = state->hits;
	hits.clear();
	size_t c = 0; // current の何番目を見ているか
	size_t p = 0; // previous の何番目を見ているか
	while (c < current.size() || p < previous.size()) {
		if (p == previous.size() || (c < current.size() && PairLess(current[c], previous[p]))) {
			// 今だけにある ＝ 当たり始めた
			AddHits(hits, current[c], HitPhase::Enter);
			++c;
		} else if (c == current.size() || PairLess(previous[p], current[c])) {
			// 前だけにある ＝ 離れた（Colliderを外した・無効にした・Entityを消した、も含む）
			AddHits(hits, previous[p], HitPhase::Exit);
			++p;
		} else {
			// 両方にある ＝ 当たったまま
			AddHits(hits, current[c], HitPhase::Stay);
			++c;
			++p;
		}
	}
	// 自分（self）の順に並べ直す。GetHits が、自分の分だけを二分探索で切り出せるように
	std::ranges::stable_sort(hits, HandleLess, &CollisionHit::self);

	// 今回の組を、次のフレームの「前」にする（入れ替えるだけ。配列のメモリはどちらも使い回す）
	std::swap(state->previousPairs, state->currentPairs);
}

//=============================================================================
// 当たりの一覧
//=============================================================================
std::span<const CollisionHit> CollisionSystem::GetHits(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	// self の順に並んでいるので、自分の分は1か所にまとまっている。その範囲（最初と最後）を二分探索で探す
	const auto range = std::ranges::equal_range(state->hits, entity, HandleLess, &CollisionHit::self);
	return {range.begin(), range.end()};
}

std::span<const CollisionHit> CollisionSystem::GetAllHits() {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	return state->hits;
}

//=============================================================================
// Collider をワールド座標の形にする（判定・押し戻しで共通）
//=============================================================================
WorldCollider CollisionSystem::CalcWorldCollider(Handle<Entity> entity, const ColliderComponent& collider, const TransformComponent& transform) {
	WorldCollider result;
	result.entity = entity;
	result.shape = collider.shape;
	result.isTrigger = collider.isTrigger;
	// 中心：ずれはEntityのローカル座標なので、ワールド行列で変換する（回転と大きさが掛かってから、Entityの位置が足される）
	const Vector3 center = TransformPoint(collider.center, transform.worldMatrix);
	const Vector3 scale = GetWorldScale(transform.worldMatrix);
	if (collider.shape == ColliderShape::Sphere) {
		// 球のまま大きくするために、一番大きい軸の大きさを掛ける（std::max は windows.h の max マクロとぶつかるので使わない）
		float maxScale = scale.x;
		if (scale.y > maxScale) {
			maxScale = scale.y;
		}
		if (scale.z > maxScale) {
			maxScale = scale.z;
		}
		result.sphere = {center, std::abs(collider.radius) * maxScale};
	} else {
		// 箱は「中心から面までの距離（大きさの半分）」を軸ごとに出す。回転は使わない（軸にそろった箱）
		const Vector3 half = {
		    std::abs(collider.size.x) * scale.x * 0.5f,
		    std::abs(collider.size.y) * scale.y * 0.5f,
		    std::abs(collider.size.z) * scale.z * 0.5f,
		};
		result.box = {center - half, center + half};
	}
	return result;
}

//=============================================================================
// その場で調べる（レイキャスト・スイープ）
//=============================================================================
namespace {
// 調べる相手か（生きていて、絞り込みに合う）
// state->colliders は前のフレームに集めた物なので、作り直しで消えたEntityが残っていることがある
bool IsQueryTarget(const WorldCollider& collider, const CollisionFilter& filter) {
	if (!EntityManager::IsAlive(collider.entity)) {
		return false;
	}
	return !filter || filter(collider.entity);
}

// レイ（向きは長さ1）が Collider に当たるか。当たったら、始点からの距離を t に入れる
bool RayCollider(const Ray& ray, const WorldCollider& collider, float& t) {
	if (collider.shape == ColliderShape::Box) {
		return Collision::RayAABB(ray, collider.box, &t); // 始点が箱の中なら t=0
	}
	// 球：RaySphere は「近い方の交点」を返すので、始点が中のとき・球が後ろにあるときは t がマイナスになる
	if (Collision::IsPointInSphere(ray.origin, collider.sphere)) {
		t = 0.0f;
		return true;
	}
	return Collision::RaySphere(ray, collider.sphere, &t) && t >= 0.0f;
}
} // namespace

bool CollisionSystem::Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, const CollisionFilter& filter) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	// 向きを長さ1にする（t がそのまま距離になるように）
	const float length = Length(ray.direction);
	if (length < 1e-6f) {
		return false; // 向きが無い
	}
	const Ray unitRay = {ray.origin, ray.direction / length};

	// 全部の Collider を調べて、一番近い物を残す（総当たり。Update と同じく数百個まではこれで足りる）
	bool isHit = false;
	float nearest = maxDistance;
	for (const WorldCollider& collider : state->colliders) {
		if (!IsQueryTarget(collider, filter)) {
			continue;
		}
		float t = 0.0f;
		if (!RayCollider(unitRay, collider, t) || t > nearest) {
			continue;
		}
		nearest = t;
		isHit = true;
		hit.entity = collider.entity;
	}
	if (isHit) {
		hit.distance = nearest;
		hit.point = Collision::RayPoint(unitRay, nearest);
	}
	return isHit;
}

void CollisionSystem::SweepSphere(const Capsule& capsule, std::vector<Handle<Entity>>& out, const CollisionFilter& filter) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	out.clear();
	for (const WorldCollider& collider : state->colliders) {
		if (!IsQueryTarget(collider, filter)) {
			continue;
		}
		const bool isTouching = (collider.shape == ColliderShape::Sphere) ? Collision::CapsuleSphere(capsule, collider.sphere) : Collision::CapsuleAABB(capsule, collider.box);
		if (isTouching) {
			out.push_back(collider.entity);
		}
	}
}

//=============================================================================
// ギズモ
//=============================================================================
void CollisionSystem::DrawGizmos(Camera* camera) {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	if (!state->showGizmos) {
		return;
	}
	Renderer::LineListConfig& gizmo = state->gizmoLines;
	gizmo.lines.clear(); // 中身だけ消す。確保済みのメモリは残るので、次から再確保が起きにくい
	gizmo.camera = camera;
	// 判定に使った形をそのまま描く（Update が集めた物。無効な Collider・無効なEntityは入っていない）
	for (const WorldCollider& collider : state->colliders) {
		const uint32_t idleColor = collider.isTrigger ? kTriggerColor : kIdleColor;
		const uint32_t color = IsTouching(collider.entity) ? kHitColor : idleColor;
		if (collider.shape == ColliderShape::Sphere) {
			GizmoLines::PushWireSphere(gizmo.lines, collider.sphere.center, collider.sphere.radius, color);
		} else {
			PushBox(gizmo.lines, collider.box, color);
		}
	}
	Renderer::DrawLines(gizmo); // 線が0本なら中で何もしない
	gizmo.camera = nullptr;     // フレームをまたいで持たない
}

bool& CollisionSystem::GetShowGizmos() {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	return state->showGizmos;
}