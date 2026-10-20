#include "PhysicsSystem.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Components/ColliderComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Systems/CollisionSystem.h"
#include "MyEngine/Systems/ElasticSurfaceSystem.h"
#include "MyEngine/Time/EngineTime.h"
#include "MyEngine/UI/GlobalVariables.h"

namespace {
constexpr float kMaxStepTime = 1.0f / 120.0f;            // 1回に進める時間の上限（速い物が薄い壁をすり抜けにくいように、細かく分けて動かす）
constexpr float kMinMass = 0.01f;                        // 重さの下限（0で割らないように）
constexpr float kGroundCos = 0.7f;                       // 押し返された向きと「上」の間が、約45度より小さければ地面に乗っている
constexpr const char* kGroupName = "Physics";            // Settings の見出し（Save すると Parameters/Physics/World.json）
constexpr const char* kCategoryName = "World";           // その中の分け方
constexpr const char* kGravityName = "Gravity";          // 項目の名前
constexpr Vector3 kDefaultGravity = {0.0f, -9.8f, 0.0f}; // 重力の既定（地球と同じ。Unity も同じ）

/// <summary>
/// 動かす Rigidbody 1つ分（Update の中だけで使う）
/// </summary>
struct Body {
	Handle<Entity> entity;                   // 持ち主
	RigidbodyComponent* rigidbody = nullptr; // Update の中だけ持つ（その間、Component の配列は増えも減りもしない）
	int solid = -1;                          // solids の何番目が自分の形か（形が無ければ -1。落ちるだけで何にもぶつからない）
	Vector3 moved = {0.0f, 0.0f, 0.0f};      // このフレームに動いた量（ワールド座標）
	Collision::AxisLock lock;                // 動かさない軸（Freeze Position）
};

/// <summary>
/// ぶつかる形1つ分（Is Trigger でない Collider。Update の中だけで使う）
/// </summary>
struct Solid {
	WorldCollider shape; // 今の形（Rigidbody が動いたら、一緒にずらす）
	int body = -1;       // 動く Rigidbody の形なら bodies の何番目か（壁・床・Is Kinematic は -1）
};

/// <summary>
/// PhysicsSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct PhysicsState {
	std::vector<Body> bodies;  // 動かす Rigidbody（毎フレーム入れ直す。配列のメモリは使い回す）
	std::vector<Solid> solids; // ぶつかる形（同上）
};
std::unique_ptr<PhysicsState> state;

//=============================================================================
// 小さな道具
//=============================================================================
// 形を move だけずらす（物理では回さないので、ずらすだけでよい）
void Translate(WorldCollider& shape, const Vector3& move) {
	shape.sphere.center += move;
	shape.box.min += move;
	shape.box.max += move;
	shape.capsule.start += move;
	shape.capsule.end += move;
}

// Rigidbody 同士がぶつかったとき、self が動く割合（0〜1。残りは相手が動く）。重さの逆の割合（軽い方がたくさん動く）
//   self が動く割合 ＝ 相手の重さ ÷ 2つの重さの和（同じ重さなら半分ずつ。相手が9倍重ければ、自分が 0.9 動く）
float CalcSelfShare(const RigidbodyComponent& self, const RigidbodyComponent& other) {
	const float selfMass = (std::max)(self.mass, kMinMass);
	const float otherMass = (std::max)(other.mass, kMinMass);
	return otherMass / (selfMass + otherMass);
}

// Rigidbody を move だけ動かす（形も一緒に）
void MoveBody(Body& body, const Vector3& move) {
	body.moved += move;
	if (body.solid >= 0) {
		Translate(state->solids[body.solid].shape, move);
	}
}

//=============================================================================
// 集める
//=============================================================================
// ===== 動かす Rigidbody（有効な Entity の、Is Kinematic でない物）=====
void CollectBodies() {
	std::vector<Body>& bodies = state->bodies;
	bodies.clear();
	EntityManager::ForEach<RigidbodyComponent>([&bodies](Handle<Entity> entity, RigidbodyComponent& rigidbody) {
		if (rigidbody.isKinematic || !EntityManager::IsActiveInHierarchy(entity)) {
			return; // 物理で動かさない（Is Kinematic の物は、ほかの Rigidbody から見ると壁と同じ）
		}
		Body body;
		body.entity = entity;
		body.rigidbody = &rigidbody;
		body.lock = {rigidbody.freezePositionX, rigidbody.freezePositionY, rigidbody.freezePositionZ};
		bodies.push_back(body);
	});
}

// ===== ぶつかる形（有効な Entity の、有効で Is Trigger でない Collider）。Rigidbodyの形なら、つないでおく =====
void CollectSolids() {
	std::vector<Solid>& solids = state->solids;
	std::vector<Body>& bodies = state->bodies;
	solids.clear();
	EntityManager::ForEach<ColliderComponent>([&solids, &bodies](Handle<Entity> entity, const ColliderComponent& collider) {
		if (!collider.enabled || collider.isTrigger || !EntityManager::IsActiveInHierarchy(entity)) {
			return; // すり抜ける物は押し戻さない
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		if (transform == nullptr) {
			return;
		}
		Solid solid;
		solid.shape = CollisionSystem::CalcWorldCollider(entity, collider, *transform);
		// 動かす Rigidbody の形か（Rigidbody は数が少ないので、順に探せば足りる）
		for (size_t i = 0; i < bodies.size(); ++i) {
			if (bodies[i].entity == entity) {
				solid.body = static_cast<int>(i);
				bodies[i].solid = static_cast<int>(solids.size());
				break;
			}
		}
		solids.push_back(solid);
	});
}

//=============================================================================
// 動かす（重力・空気抵抗で速さを変えて、速さの分だけ進める）
//=============================================================================
void Integrate(Body& body, const Vector3& gravity, float stepTime) {
	RigidbodyComponent& rigidbody = *body.rigidbody;
	if (rigidbody.useGravity) {
		rigidbody.velocity += gravity * (rigidbody.gravityScale * stepTime);
	}
	// 空気抵抗：1秒に drag の割合くらい遅くなる（Unity と同じ近似。dragが0なら変わらない）
	if (rigidbody.drag > 0.0f) {
		rigidbody.velocity *= 1.0f / (1.0f + rigidbody.drag * stepTime);
	}
	// 止めた軸の速さは0（Freeze Position）
	rigidbody.velocity = Collision::RemoveLockedAxes(rigidbody.velocity, body.lock);
	MoveBody(body, rigidbody.velocity * stepTime);
}

//=============================================================================
// 弾む面に押し返される（力で速さを変えるだけ。動かすのは次の Integrate）
//=============================================================================
void PushByElasticSurfaces(Body& body, const Vector3& up, float stepTime) {
	if (body.solid < 0) {
		return; // 形が無い（何にも乗れない）
	}
	RigidbodyComponent& rigidbody = *body.rigidbody;
	const Vector3 force = ElasticSurfaceSystem::CalcContactForce(state->solids[body.solid].shape, rigidbody.velocity.y);
	if (LengthSq(force) == 0.0f) {
		return; // 面に入っていない
	}
	// F = ma → 速さの変化 ＝ F ÷ 重さ × 時間（止めた軸には動かさない）
	const float mass = (std::max)(rigidbody.mass, kMinMass);
	rigidbody.velocity += Collision::RemoveLockedAxes(force * (stepTime / mass), body.lock);
	if (Dot(Normalize(force), up) >= kGroundCos) {
		rigidbody.isGrounded = true; // 下から押し返された＝面の上に乗っている
	}
}

//=============================================================================
// 押し戻す（重なった形から一番短い距離で外へ出し、ぶつかった向きの速さを0にする）
//=============================================================================
void Resolve(size_t bodyIndex, const Vector3& up) {
	Body& body = state->bodies[bodyIndex];
	if (body.solid < 0) {
		return; // 形が無い（何にもぶつからない）
	}
	std::vector<Solid>& solids = state->solids;
	RigidbodyComponent& rigidbody = *body.rigidbody;
	for (size_t i = 0; i < solids.size(); ++i) {
		const Solid& other = solids[i];
		// --- 早期リターン ---
		// 自分の形・親子の形とはぶつからない（自分の子に付けた Collider（持っている剣など）に押し出され続けないように）
		if (static_cast<int>(i) == body.solid || EntityManager::IsRelated(body.entity, other.shape.entity)) {
			continue;
		}
		// Rigidbody 同士で、どちらかが Collide With Rigidbodies を外していたら、すり抜ける（押し合わない）
		if (other.body >= 0 && (!rigidbody.collideWithRigidbodies || !state->bodies[other.body].rigidbody->collideWithRigidbodies)) {
			continue;
		}
		Vector3 push;
		if (!CollisionSystem::CalcPushOut(solids[body.solid].shape, other.shape, body.lock, &push)) {
			continue; // 重なっていない
		}
		const Vector3 normal = Normalize(push); // 相手から押し返された向き

		if (other.body >= 0) {
			// --- Rigidbody 同士：重さの逆の割合で押し合い、近づく速さも同じ割合で分ける（くっついて同じ速さになる。跳ね返らない）---
			Body& otherBody = state->bodies[other.body];
			const float selfShare = CalcSelfShare(rigidbody, *otherBody.rigidbody); // 自分が動く割合
			const float otherShare = 1.0f - selfShare;                              // 相手が動く割合
			MoveBody(body, push * selfShare);
			MoveBody(otherBody, Collision::RemoveLockedAxes(push * -otherShare, otherBody.lock));
			const float approach = Dot(rigidbody.velocity - otherBody.rigidbody->velocity, normal); // マイナスなら近づいている
			if (approach < 0.0f) {
				rigidbody.velocity -= normal * (approach * selfShare);
				otherBody.rigidbody->velocity += normal * (approach * otherShare);
			}
			if (Dot(-normal, up) >= kGroundCos) {
				otherBody.rigidbody->isGrounded = true; // 相手が自分の上に乗っている
			}
		} else {
			// --- 壁・床（Rigidbody が無い・Is Kinematic）：自分だけ押し戻し、壁へ向かう速さを0にする ---
			// 壁に沿った速さは残るので、壁をこすりながら進める（床の上を歩ける）
			MoveBody(body, push);
			const float approach = Dot(rigidbody.velocity, normal);
			if (approach < 0.0f) {
				rigidbody.velocity -= normal * approach;
			}
		}
		if (Dot(normal, up) >= kGroundCos) {
			rigidbody.isGrounded = true; // 下から押し返された＝何かの上に乗っている
		}
	}
}

//=============================================================================
// 動いた量を Transform へ書く
//=============================================================================
// translation は親から見た位置なので、親がいれば、親から見た量に直してから足す
void ApplyMove(const Body& body) {
	TransformComponent* transform = EntityManager::Get<TransformComponent>(body.entity);
	const Entity* entity = EntityManager::Get(body.entity);
	if (transform == nullptr || entity == nullptr) {
		return;
	}
	Vector3 move = body.moved;
	if (const TransformComponent* parent = EntityManager::Get<TransformComponent>(entity->parent)) {
		const Vector3 local = TransformNormal(body.moved, Inverse(parent->worldMatrix));
		// 親の大きさが0だと逆行列が作れず、数でなくなる。そのときは、そのまま足す
		if (std::isfinite(local.x) && std::isfinite(local.y) && std::isfinite(local.z)) {
			move = local;
		}
	}
	transform->translation += move;
}

//=============================================================================
// Inspector と 保存
//=============================================================================
// Rigidbody の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Collider と同じ書き方）
void DescribeRigidbody(ComponentUI& ui, RigidbodyComponent& value) {
	ui.Field("Mass###mass", value.mass, AtLeast(kMinMass).Tip("重さ（kg）。Elastic Surface（弾む面）の上で、重いほど深く沈める"));
	ui.Field("Use Gravity###useGravity", value.useGravity, Tip("重力で落ちる（向きと強さは Settings → Physics）"));
	ui.Field("Gravity Scale###gravityScale", value.gravityScale, Tip("重力の倍率（2 で2倍速く落ちる）"));
	ui.Field("Drag###drag", value.drag, AtLeast(0.0f).Tip("空気抵抗（大きいほど早く止まる。0 なら止まらない）"));
	ui.Field("Is Kinematic###isKinematic", value.isKinematic, Tip("物理で動かさない（スクリプトで動かす床など）。ほかの Rigidbody からは壁に見える"));
	ui.Field("Collide With Rigidbodies###collideWithRigidbodies", value.collideWithRigidbodies, Tip("ほかの Rigidbody と押し合う（重さの逆の割合で）。外すと、ほかの Rigidbody とはすり抜ける（壁・床にはぶつかる。当たりの知らせ OnCollision〜 は来る。当たると吹き飛ぶプレイヤーなど）"));
	ui.Separator("Constraints");
	ui.Field("Freeze Position X###freezePositionX", value.freezePositionX);
	ui.Field("Freeze Position Y###freezePositionY", value.freezePositionY, Tip("見下ろしのゲームなら止める"));
	ui.Field("Freeze Position Z###freezePositionZ", value.freezePositionZ, Tip("横から見るゲームなら止める"));
	ui.Separator("Info");
	ui.Field("Velocity###velocity", value.velocity, Tip("今の速さ（実行中に変わる。停止中に入れた値は、Play を押したときの速さになる）"));
	ui.Label(value.isGrounded ? "Is Grounded: 何かの上に乗っている" : "Is Grounded: 乗っていない");
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void PhysicsSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<PhysicsState>();

	// Rigidbody の置き場所・保存・Inspector を登録する。見せ方は DescribeRigidbody 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<RigidbodyComponent>();
	ComponentSerializer::Register<RigidbodyComponent>("Rigidbody", &DescribeRigidbody);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<RigidbodyComponent>>("Rigidbody", "Physics", &DescribeRigidbody));

	// 重力を Settings に出す（Save すると Parameters/Physics/World.json に残り、次の起動で読む）
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->Group(kGroupName).Category(kCategoryName).Add(kGravityName, kDefaultGravity);
	gv->LoadGroup(kGroupName);
}

void PhysicsSystem::Release() { state.reset(); }


//=============================================================================
// 毎フレーム（再生中だけ）
//=============================================================================
bool PhysicsSystem::Update(float deltaTime) {
	MY_ASSERT_MSG(state != nullptr, "PhysicsSystem::Initialize()を先に呼んでください");
	if (deltaTime <= 0.0f) {
		return false; // 一時停止・ヒットストップ中（速さも重力も変えない）
	}

	// ===== 1. 集める（弾む面・動かす Rigidbody・ぶつかる形）=====
	// 弾む面は Rigidbody が無くても揺れ続けるので、面があれば先へ進む
	const float surfaceStepTime = ElasticSurfaceSystem::BeginPhysics(); // 面が無ければ0
	const bool hasSurfaces = (surfaceStepTime > 0.0f);
	CollectBodies();
	if (state->bodies.empty() && !hasSurfaces) {
		return false;
	}
	CollectSolids();

	// ===== 2. 細かく分けて「面に押し返される → 面を進める → 動かす → 押し戻す」を繰り返す =====
	const Vector3 gravity = GetGravity();
	// 地面に乗っているかを決める「上」（重力の反対。重力が無ければ +Y）
	const Vector3 up = (LengthSq(gravity) > 1e-8f) ? Normalize(-gravity) : Vector3{0.0f, 1.0f, 0.0f};
	// 1歩の長さの上限：弾む面があれば、面が要求する長さ（張りの強い膜は、1歩が長いと暴れる）
	const float maxStepTime = hasSurfaces ? (std::min)(kMaxStepTime, surfaceStepTime) : kMaxStepTime;
	const int maxSteps = static_cast<int>(std::ceil(Time::kMaxDeltaTime / maxStepTime)); // deltaTime が上限でも、1歩が maxStepTime を超えない回数
	const int steps = std::clamp(static_cast<int>(std::ceil(deltaTime / maxStepTime)), 1, maxSteps);
	const float stepTime = deltaTime / static_cast<float>(steps);
	for (Body& body : state->bodies) {
		body.rigidbody->isGrounded = false; // このフレームに乗っていれば、Resolve・PushByElasticSurfaces が true にする
	}
	for (int step = 0; step < steps; ++step) {
		// --- 弾む面：物が面を押し、面に押し返される（同じ1歩の中で、面も物も進める。別々に進めると突き抜ける・暴れる）---
		if (hasSurfaces) {
			for (Body& body : state->bodies) {
				PushByElasticSurfaces(body, up, stepTime);
			}
			ElasticSurfaceSystem::Step(stepTime);
		}
		// --- 動かす → 押し戻す ---
		for (size_t i = 0; i < state->bodies.size(); ++i) {
			Integrate(state->bodies[i], gravity, stepTime);
			Resolve(i, up);
		}
	}

	// ===== 3. 動いた量を Transform へ =====
	bool isMoved = false;
	for (const Body& body : state->bodies) {
		if (LengthSq(body.moved) == 0.0f) {
			continue;
		}
		ApplyMove(body);
		isMoved = true;
	}
	return isMoved;
}

//=============================================================================
// 重力
//=============================================================================
Vector3 PhysicsSystem::GetGravity() { return GlobalVariables::GetInstance()->Get<Vector3>(kGroupName, kCategoryName, kGravityName); }

void PhysicsSystem::SetGravity(const Vector3& gravity) { GlobalVariables::GetInstance()->Set<Vector3>(kGroupName, kCategoryName, kGravityName, gravity); }