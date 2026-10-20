#pragma once
#include <functional>
#include <span>
#include <vector>

#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Components/ColliderComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Geometry/AABB.h"
#include "MyEngine/Math/Geometry/Capsule.h"
#include "MyEngine/Math/Geometry/Ray.h"
#include "MyEngine/Math/Geometry/Sphere.h"

// 前方宣言
class Camera;
struct TransformComponent;


/// <summary>
/// 当たり方の段階
/// </summary>
enum class HitPhase {
	Enter, // 当たり始めた（このフレームから重なった）
	Stay,  // 当たっている（前のフレームから重なったまま）
	Exit,  // 離れた（前のフレームまで重なっていた）
};

/// <summary>
/// レイキャストで当たった物
/// </summary>
struct RaycastHit {
	Handle<Entity> entity; // 当たったEntity
	Vector3 point;         // 当たった位置（ワールド座標）
	float distance = 0.0f; // レイの始点から当たった位置までの距離
};

/// <summary>
/// 当たり1件。self から見て、other と当たった
/// </summary>
struct CollisionHit {
	Handle<Entity> self;    // 自分
	Handle<Entity> other;   // 相手（Exit のときは、相手がもう消えていることがある。そのときは EntityManager::Get が nullptr を返す）
	HitPhase phase;         // 当たり始め・当たっている・離れた
	bool isTrigger = false; // どちらかが Is Trigger（すり抜ける当たり。Script には OnTrigger〜、そうでなければ OnCollision〜）
	Vector3 normal = {0.0f, 0.0f, 0.0f}; // 相手から自分へ押し出す向き（長さ1。ぶつかった面の向き）。Exit は離れる前のフレームの向き
};

/// <summary>
/// ワールド座標に直した Collider の形（CollisionSystem が判定に使い、PhysicsSystem が押し戻しに使う）
/// </summary>
struct WorldCollider {
	Handle<Entity> entity;                       // 持ち主
	ColliderShape shape = ColliderShape::Box;    // 形
	Sphere sphere{};                             // shape が Sphere のとき使う
	AABB box{};                                  // shape が Box のとき使う
	Capsule capsule{};                           // shape が Capsule のとき使う（いつも立っている。start が下の端の球の中心、end が上の端の球の中心）
	bool isTrigger = false;                      // すり抜ける（押し戻さない）
};

// どのEntityを調べるか（true を返したEntityだけ調べる）。空なら全部
// レイヤーの代わりに「相手が持っている Component」で絞る：[](Handle<Entity> e) { return EntityManager::Get<Wall>(e) != nullptr; }
using CollisionFilter = std::function<bool(Handle<Entity>)>;


/// <summary>
/// ColliderComponent を持つEntity同士の重なりを調べて、「当たりの一覧」を作る
/// <para>押し戻しはここではしない（Rigidbody を付けた物を PhysicsSystem が押し戻す）。ゲームは Script の OnTrigger〜 / OnCollision〜、
/// または SYSTEM の中で GetHits を読んで、消す・音を鳴らすなどを決める</para>
/// <para>1フレームの順番：SYSTEM（前のフレームの当たりを読む）→ ワールド行列 → Rigidbody の押し戻し → 破棄の反映 → ここで判定 → 描画</para>
/// <para>一覧などの中身は CollisionSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class CollisionSystem {
public:
	// ColliderComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 全部の Collider の重なりを調べて、当たりの一覧を作り直す
	/// <para>EntityManager::FlushDestroy の後に呼ぶ（消すと決めたEntityは、もう当たらない）</para>
	/// <para>触れているだけ（kContactOffset より近い）の物も当たっている扱い（Rigidbody は床にちょうど触れた所で止まるので）</para>
	/// </summary>
	static void Update();

	/// <summary>
	/// このEntityの当たり（self がこのEntityの物だけ）。無ければ空
	/// <para>中身は次の Update まで変わらない（SYSTEM の中で読む分には気にしなくてよい）</para>
	/// </summary>
	static std::span<const CollisionHit> GetHits(Handle<Entity> entity);

	// ===== その場で調べる（SYSTEMの中で、何回呼んでもよい）=====
	// 調べる相手の形は、前のフレームの Update で集めた物（1フレームの順番の 9。動かない壁なら気にしなくてよい）
	// Play・シーン切り替えの直後のフレームは、作り直す前の Entity の形しか無いので、何にも当たらない

	/// <summary>
	/// レイを飛ばして、一番近くで当たった Collider を探す（壁で線を止める・地面を指す）
	/// </summary>
	/// <param name="ray">始点と向き（向きの長さは1でなくてよい。中で長さ1にする）</param>
	/// <param name="maxDistance">どこまで調べるか（始点からの距離）</param>
	/// <param name="hit">当たった物が入る（当たらなければ変えない）</param>
	/// <param name="filter">調べる相手を絞る（空なら全部）</param>
	/// <returns>当たったか。始点が Collider の中なら、距離0で当たる</returns>
	static bool Raycast(const Ray& ray, float maxDistance, RaycastHit& hit, const CollisionFilter& filter = {});

	/// <summary>
	/// 球を start から end まで動かしたときに触れた Collider を全部集める（速く動く物の攻撃。1フレームで飛び越えてもすり抜けない）
	/// </summary>
	/// <param name="capsule">動いた跡（start → end を半径radius で太らせた形）</param>
	/// <param name="out">触れたEntityが入る（最初に空にする。並びは決まっていない）</param>
	/// <param name="filter">調べる相手を絞る（空なら全部）</param>
	static void SweepSphere(const Capsule& capsule, std::vector<Handle<Entity>>& out, const CollisionFilter& filter = {});

	// 全部の当たり（1組につき、両方から見た2件が入る。self の順に並んでいる）
	static std::span<const CollisionHit> GetAllHits();

	/// <summary>
	/// Collider を、ワールド座標の球・箱・カプセルにする（Transform のワールド行列を使うので、UpdateTransforms の後に呼ぶ）
	/// <para>Entityの回転と大きさも掛かる（箱・カプセルは回らない。大きさだけ軸ごとに掛かる）</para>
	/// </summary>
	static WorldCollider CalcWorldCollider(Handle<Entity> entity, const ColliderComponent& collider, const TransformComponent& transform);

	/// <summary>
	/// カプセルを「線分の上で相手に一番近い点に置いた、同じ半径の球」に置き換える（両方カプセルなら両方）。球・箱はそのまま
	/// <para>カプセルは線分を半径で太らせた形なので、重なっているかは、この球と同じになる。球・箱の判定と押し出しをそのまま使える（PhysicsSystem も使う）</para>
	/// </summary>
	static void ReplaceCapsulesWithSpheres(WorldCollider& a, WorldCollider& b);

	/// <summary>
	/// a を b から外へ出すずれ（a に push を足すと、ちょうど触れた所になる）。重なっていなければ false
	/// <para>形の組み合わせ（球・箱・カプセル）で、使う関数を選ぶ。カプセルは ReplaceCapsulesWithSpheres で球にしてから出す</para>
	/// <para>重なりの判定・当たりの向き（CollisionHit::normal）・Rigidbody の押し戻し（PhysicsSystem）が、みんなこれを使う</para>
	/// </summary>
	/// <param name="a">出す方（コピー。カプセルを球に置き換えるので）</param>
	/// <param name="b">出される相手（コピー）</param>
	/// <param name="lock">その軸へは押さない（Freeze Position）。その分、ほかの軸で押す</param>
	/// <param name="push">ずれが入る（重なっていなければ変えない）</param>
	static bool CalcPushOut(WorldCollider a, WorldCollider b, Collision::AxisLock lock, Vector3* push);

	// Collider の線を描く（エディタ用。当たっている物は色を変える）
	static void DrawGizmos(Camera* camera);

	// 線を出すか（メニューバーの View から切り替える）
	static bool& GetShowGizmos();

	// この距離まで近づいたら「当たっている」にする（Unity の Contact Offset と同じ考え）
	static constexpr float kContactOffset = 0.01f;
};