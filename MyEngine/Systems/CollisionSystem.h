#pragma once
#include <span>
#include <vector>
#include <functional>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Geometry/Ray.h"
#include "MyEngine/Math/Geometry/Capsule.h"
#include "MyEngine/Components/ColliderComponent.h"

// 前方宣言
class Camera;


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
	Handle<Entity> self;  // 自分
	Handle<Entity> other; // 相手（Exit のときは、相手がもう消えていることがある。そのときは EntityManager::Get が nullptr を返す）
	HitPhase phase;       // 当たり始め・当たっている・離れた
};

// どのEntityを調べるか（true を返したEntityだけ調べる）。空なら全部
// レイヤーの代わりに「相手が持っている Component」で絞る：[](Handle<Entity> e) { return EntityManager::Get<Wall>(e) != nullptr; }
using CollisionFilter = std::function<bool(Handle<Entity>)>;


/// <summary>
/// ColliderComponent を持つEntity同士の重なりを調べて、「当たりの一覧」を作る
/// <para>押し戻しはしない。ゲームは SYSTEM の中で GetHits を読んで、消す・音を鳴らすなどを決める</para>
/// <para>1フレームの順番：SYSTEM（前のフレームの当たりを読む）→ ワールド行列 → 破棄の反映 → ここで判定 → 描画</para>
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
	/// </summary>
	static void Update();

	/// <summary>
	/// このEntityの当たり（self がこのEntityの物だけ）。無ければ空
	/// <para>中身は次の Update まで変わらない（SYSTEM の中で読む分には気にしなくてよい）</para>
	/// </summary>
	static std::span<const CollisionHit> GetHits(Handle<Entity> entity);

	// ===== その場で調べる（SYSTEM の中で、何回呼んでもよい）=====
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
	/// <param name="capsule">動いた跡（start → end を半径 radius で太らせた形）</param>
	/// <param name="out">触れたEntityが入る（最初に空にする。並びは決まっていない）</param>
	/// <param name="filter">調べる相手を絞る（空なら全部）</param>
	static void SweepSphere(const Capsule& capsule, std::vector<Handle<Entity>>& out, const CollisionFilter& filter = {});

	// 全部の当たり（1組につき、両方から見た2件が入る。self の順に並んでいる）
	static std::span<const CollisionHit> GetAllHits();

	// Collider の線を描く（エディタ用。当たっている物は色を変える）
	static void DrawGizmos(Camera* camera);

	// 線を出すか（メニューバーの View から切り替える）
	static bool& GetShowGizmos();
};