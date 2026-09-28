#pragma once
#include <span>

#include "MyEngine/Components/ColliderComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

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
/// 当たり1件。self から見て、other と当たった
/// </summary>
struct CollisionHit {
	Handle<Entity> self;  // 自分
	Handle<Entity> other; // 相手（Exit のときは、相手がもう消えていることがある。そのときは EntityManager::Get が nullptr を返す）
	HitPhase phase;       // 当たり始め・当たっている・離れた
};



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

	// 全部の当たり（1組につき、両方から見た2件が入る。self の順に並んでいる）
	static std::span<const CollisionHit> GetAllHits();

	// Collider の線を描く（エディタ用。当たっている物は色を変える）
	static void DrawGizmos(Camera* camera);

	// 線を出すか（メニューバーの View から切り替える）
	static bool& GetShowGizmos();
};