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
	Vector3 normal = {0.0f, 0.0f, 0.0f}; // second から first へ押し出す向き（長さ1）。並び順（PairLess）には使わない
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

// 2つのEntityから、並びをそろえた組を作る。normal は b から a へ押し出す向き（入れ替えたら、向きも逆にする）
HitPair MakePair(Handle<Entity> a, Handle<Entity> b, bool isTrigger, const Vector3& normal) { return HandleLess(a, b) ? HitPair{a, b, isTrigger, normal} : HitPair{b, a, isTrigger, -normal}; }

// Entityの大きさ（ワールド行列の1〜3行目の長さ＝ローカルのX・Y・Z軸がどれだけ伸びているか。親の大きさも入っている）
Vector3 GetWorldScale(const Matrix4x4& m) {
	return {
	    Length(Vector3{m.m[0][0], m.m[0][1], m.m[0][2]}),
	    Length(Vector3{m.m[1][0], m.m[1][1], m.m[1][2]}),
	    Length(Vector3{m.m[2][0], m.m[2][1], m.m[2][2]}),
	};
}

// 形を distance だけ太らせる（球・カプセルは半径を、箱は6つの面を外へ）
WorldCollider Inflate(const WorldCollider& collider, float distance) {
	WorldCollider result = collider;
	const Vector3 grow = {distance, distance, distance};
	result.sphere.radius += distance;
	result.box = {collider.box.min - grow, collider.box.max + grow};
	result.capsule.radius += distance;
	return result;
}

// カプセルを線分の上のpointに置いた同じ半径の球にする
WorldCollider ToSphere(const WorldCollider& capsule, const Vector3& point) {
	WorldCollider result = capsule;
	result.shape = ColliderShape::Sphere;
	result.sphere = {point, capsule.capsule.radius};
	return result;
}

// カプセルの線分の上で、相手（球・箱）に一番近い点。箱に入っていれば、一番深く入っている点
Vector3 NearestPointOnCapsule(const Capsule& capsule, const WorldCollider& other) {
	if (other.shape == ColliderShape::Sphere) {
		return Collision::ClosestPointOnSegment(other.sphere.center, capsule.start, capsule.end);
	}
	return Collision::DeepestPointOnSegmentInAABB(capsule.start, capsule.end, other.box);
}

// 2つが重なっているか（形の組み合わせで、使う関数を選ぶ）
bool IsOverlapping(const WorldCollider& a, const WorldCollider& b) {
	Vector3 push;
	return CollisionSystem::CalcPushOut(a, b, {}, &push);
}

// 2つが kContactOffset より近いか（触れているだけの物も「当たっている」にする）。近ければ、b から a へ押し出す向き（長さ1）を normal に入れる
// Rigid body は壁や床から押し戻されて「ちょうど触れた所」で止まるので、重なりだけを見ると、乗っている床との当たりが毎フレーム切れたりつながったりする
bool CalcContact(const WorldCollider& a, const WorldCollider& b, Vector3* normal) {
	const float half = CollisionSystem::kContactOffset * 0.5f; // 両方を半分ずつ太らせる
	Vector3 push;
	if (!CollisionSystem::CalcPushOut(Inflate(a, half), Inflate(b, half), {}, &push)) {
		return false;
	}
	*normal = Normalize(push);
	return true;
}

// 1組を「両方から見た2件」にして足す。消えたEntityから見た分は足さない（読む人がいない）
void AddHits(std::vector<CollisionHit>& hits, const HitPair& pair, HitPhase phase) {
	if (EntityManager::IsAlive(pair.first)) {
		hits.push_back({pair.first, pair.second, phase, pair.isTrigger, pair.normal});
	}
	if (EntityManager::IsAlive(pair.second)) {
		hits.push_back({pair.second, pair.first, phase, pair.isTrigger, -pair.normal});
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
	switch (value.shape) {
	case ColliderShape::Sphere:
		ui.Field("Radius###radius", value.radius, AtLeast(0.0f).Tip("Entityの大きさの、一番大きい軸を掛けて判定する"));
		break;
	case ColliderShape::Box:
		ui.Field("Size###size", value.size, AtLeast(0.0f).Tip("幅・高さ・奥行き。Entityを回しても、箱は回らない"));
		break;
	case ColliderShape::Capsule:
		ui.Field("Radius###radius", value.radius, AtLeast(0.0f).Tip("Entityの大きさの、X と Z の大きい方を掛けて判定する"));
		ui.Field("Height## #height ", value.height, AtLeast(0.0f).Tip("下の端から上の端まで（半球も入れる）。Entity の Y の大きさを掛ける。半径の2倍より小さいと球になる。Entityを回しても、カプセルは立ったまま"));
		break;
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
			if (EntityManager::IsRelated(colliders[i].entity, colliders[j].entity)) {
				continue;
			}
			Vector3 normal; // j から i へ押し出す向き
			if (CalcContact(colliders[i], colliders[j], &normal)) {
				current.push_back(MakePair(colliders[i].entity, colliders[j].entity, colliders[i].isTrigger || colliders[j].isTrigger, normal));
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
	switch (collider.shape) {
	case ColliderShape::Sphere: {
		// 球のまま大きくするために、一番大きい軸の大きさを掛ける（(std::max) と括弧で囲むと、windows.h の max マクロにならない）
		const float maxScale = (std::max)({scale.x, scale.y, scale.z});
		result.sphere = {center, std::abs(collider.radius) * maxScale};
		break;
	}
	case ColliderShape::Box: {
		// 箱は「中心から面までの距離（大きさの半分）」を軸ごとに出す。回転は使わない（軸にそろった箱）
		const Vector3 half = {
		    std::abs(collider.size.x) * scale.x * 0.5f,
		    std::abs(collider.size.y) * scale.y * 0.5f,
		    std::abs(collider.size.z) * scale.z * 0.5f,
		};
		result.box = {center - half, center + half};
		break;
	}
	case ColliderShape::Capsule: {
		// 半径には横（X と Z）の大きい方、高さには Y の大きさを掛ける。回転は使わない（いつも立ったまま）
		const float radius = std::abs(collider.radius) * (std::max)(scale.x, scale.z);
		const float height = std::abs(collider.height) * scale.y;
		// 線分の長さの半分 ＝ 高さの半分から、端の半球の分（半径）を引いた物。高さが半径の2倍より小さければ0（球になる）
		const float halfSegment = (std::max)(height * 0.5f - radius, 0.0f);
		const Vector3 offset = {0.0f, halfSegment, 0.0f};
		result.capsule = {center - offset, center + offset, radius};
		break;
	}
	}
	return result;
}

//=============================================================================
// カプセルを球に置き換える（判定・押し戻しで共通）
//=============================================================================
void CollisionSystem::ReplaceCapsulesWithSpheres(WorldCollider& a, WorldCollider& b) {
	const bool aIsCapsule = (a.shape == ColliderShape::Capsule);
	const bool bIsCapsule = (b.shape == ColliderShape::Capsule);
	// --- 両方カプセル：2本の線分の、お互いに一番近い点 ---
	if (aIsCapsule && bIsCapsule) {
		Vector3 onA;
		Vector3 onB;
		Collision::ClosestPointsBetweenSegments(a.capsule.start, a.capsule.end, b.capsule.start, b.capsule.end, &onA, &onB);
		a = ToSphere(a, onA);
		b = ToSphere(b, onB);
		return;
	}
	// --- 片方だけ：相手（球・箱）に一番近い点 ---
	if (aIsCapsule) {
		a = ToSphere(a, NearestPointOnCapsule(a.capsule, b));
	}
	if (bIsCapsule) {
		b = ToSphere(b, NearestPointOnCapsule(b.capsule, a));
	}
}

//=============================================================================
// 押し出すずれ（判定・当たりの向き・押し戻しで共通）
//=============================================================================
bool CollisionSystem::CalcPushOut(WorldCollider a, WorldCollider b, Collision::AxisLock lock, Vector3* push) {
	// カプセルは、相手に一番近い所の球に置き換える（その球を出す量だけ、カプセルも出す）。ここから下は球と箱だけ
	ReplaceCapsulesWithSpheres(a, b);
	// 形の組み合わせ（a × b）で、使う関数を選ぶ
	switch (a.shape) {
	case ColliderShape::Sphere:
		switch (b.shape) {
		case ColliderShape::Sphere:
			return Collision::PushOutSphereSphere(a.sphere, b.sphere, push, lock);
		case ColliderShape::Box:
			return Collision::PushOutSphereAABB(a.sphere, b.box, push, lock);
		case ColliderShape::Capsule:
			break; // 置き換えたので来ない
		}
		break;
	case ColliderShape::Box:
		switch (b.shape) {
		case ColliderShape::Sphere:
			return Collision::PushOutAABBSphere(a.box, b.sphere, push, lock);
		case ColliderShape::Box:
			return Collision::PushOutAABBAABB(a.box, b.box, push, lock);
		case ColliderShape::Capsule:
			break; // 置き換えたので来ない
		}
		break;
	case ColliderShape::Capsule:
		break; // 置き換えたので来ない
	}
	MY_ASSERT_MSG(false, "カプセルが球に置き換わっていません");
	return false;
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

// ===== レイ（向きは長さ1）が Collider に当たるか。当たったら、始点からの距離を t に入れる =====
bool RayCollider(const Ray& ray, const WorldCollider& collider, float& t) {
	switch (collider.shape) {
	case ColliderShape::Sphere:
		// RaySphere は「近い方の交点」を返すので、始点が中のとき・球が後ろにあるときは t がマイナスになる
		if (Collision::IsPointInSphere(ray.origin, collider.sphere)) {
			t = 0.0f;
			return true;
		}
		return Collision::RaySphere(ray, collider.sphere, &t) && t >= 0.0f;
	case ColliderShape::Box:
		return Collision::RayAABB(ray, collider.box, &t); // 始点が箱の中なら t=0
	case ColliderShape::Capsule:
		return Collision::RayCapsule(ray, collider.capsule, &t); // 始点がカプセルの中なら t=0
	}
	return false;
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
	// 動いた跡をカプセルの Collider にして、Update と同じ IsOverlapping で調べる（相手が球・箱・カプセルのどれでも同じ書き方）
	WorldCollider sweep;
	sweep.shape = ColliderShape::Capsule;
	sweep.capsule = capsule;
	for (const WorldCollider& collider : state->colliders) {
		if (!IsQueryTarget(collider, filter)) {
			continue;
		}
		if (IsOverlapping(sweep, collider)) {
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
		switch (collider.shape) {
		case ColliderShape::Sphere:
			GizmoLines::PushWireSphere(gizmo.lines, collider.sphere.center, collider.sphere.radius, color);
			break;
		case ColliderShape::Box:
			PushBox(gizmo.lines, collider.box, color);
			break;
		case ColliderShape::Capsule:
			GizmoLines::PushWireCapsule(gizmo.lines, collider.capsule.start, collider.capsule.end, collider.capsule.radius, color);
			break;
		}
	}
	Renderer::DrawLines(gizmo); // 線が0本なら中で何もしない
	gizmo.camera = nullptr;     // フレームをまたいで持たない
}

bool& CollisionSystem::GetShowGizmos() {
	MY_ASSERT_MSG(state != nullptr, "CollisionSystem::Initialize()を先に呼んでください");
	return state->showGizmos;
}