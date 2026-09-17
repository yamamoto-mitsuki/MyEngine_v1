#pragma once
#include <string>
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Entity/Entity.h"
#include "MyEngine/Entity/TransformComponent.h"


/// <summary>
/// Entityの管理。実体はここだけが持ち、外にはHandleを渡す（LightManagerと同じ形）
/// <para>ARCHITECTURE.md の更新順序のうち、2（ワールド行列）と6（破棄）を担当する</para>
/// </summary>
class EntityManager {
public:
	static void Initialize();
	static void Release();

	// ===== 生成・破棄 =====
	/// <summary>
	/// Entityを作る。TransformComponentも一緒に作られる
	/// </summary>
	/// <param name="name">ヒエラルキーに出す名前</param>
	/// <param name="parent">親。省略すると一番上に置く</param>
	static Handle<Entity> Create(const std::string& name = "Entity", Handle<Entity> parent = {});

	/// <summary>
	/// 破棄を予約する。実際に消えるのはフレームの最後（子も一緒に消える）
	/// </summary>
	static void Destroy(Handle<Entity> handle);

	// ===== 取得（受け取ったポインタは使い捨てにする。ARCHITECTURE.md 原則1） =====
	static Entity* Get(Handle<Entity> handle);
	static TransformComponent* GetTransform(Handle<Entity> handle);
	static bool IsAlive(Handle<Entity> handle);

	// ===== 親子 =====
	/// <summary>
	/// 親を付け替える。自分の子孫を親にしようとしたときは何もしない（輪になるのを防ぐ）
	/// </summary>
	static void SetParent(Handle<Entity> child, Handle<Entity> parent);

	/// <summary>
	/// 親をたどって、handleがancestorの子孫かを調べる
	/// </summary>
	static bool IsDescendantOf(Handle<Entity> handle, Handle<Entity> ancestor);

	/// <summary>
	/// 子のHandleを out に詰める（毎フレーム呼ぶので、outは使い回す）
	/// </summary>
	static void GetChildren(Handle<Entity> handle, std::vector<Handle<Entity>>& out);

	/// <summary>
	/// 親を持たないEntity（一番上）のHandleを out に詰める
	/// </summary>
	static void GetRoots(std::vector<Handle<Entity>>& out);

	// 追加を予約する。実体ができるのは次のFlushComponentChanges。
	// 戻り値のポインタを即座に使う方式にはしない。
	static void RequestAddRender(Handle<Entity> handle);
	static bool IsRenderAddPending(Handle<Entity> handle);
	static RenderComponent* GetRender(Handle<Entity> handle);

	// Update冒頭で1回呼ぶ。SlotMapの変更はここへまとめる。
	static void FlushComponentChanges();


	// ===== フレーム更新 =====
	/// <summary>
	/// 更新順序2：ワールド行列を親→子の順で計算する。シーンのUpdateの後に呼ぶ
	/// </summary>
	static void UpdateTransforms();

	/// <summary>
	/// 更新順序6：破棄予約をまとめて反映する。フレームの最後に呼ぶ
	/// </summary>
	static void FlushDestroy();

	// ===== 一覧（ヒエラルキー用） =====
	static size_t GetCount();
	static SlotMap<Entity>& GetAll() { return instance_->entities_; }

private:
	static constexpr uint32_t kMaxParentDepth = 64; // 親をたどる回数の上限（万一輪になっても止まるように）

	static EntityManager* instance_;

	// 破棄予約されたEntityと、その子孫を outに集める
	void CollectDescendants(Handle<Entity> handle, std::vector<Handle<Entity>>& out);
	// 親を何回たどるとrootに着くか
	uint32_t CalcDepth(Handle<Entity> handle);

	SlotMap<Entity> entities_;
	SlotMap<TransformComponent> transforms_;
	SlotMap<RenderComponent> renders_;
	std::vector<Handle<Entity>> pendingAddRender_;
	std::vector<Handle<Entity>> pendingDestroy_; // 破棄予約
	// 毎フレーム使う作業用の配列（確保し直さないようにメンバで持つ）
	std::vector<std::pair<uint32_t, Handle<Entity>>> updateOrder_; // (深さ, Handle)
	std::vector<Handle<Entity>> destroyWork_;
};