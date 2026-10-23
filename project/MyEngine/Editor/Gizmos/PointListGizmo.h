#pragma once
#include <span>

#include <externals/imgui/imgui.h>

#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"

// 前方宣言
class Camera;
class ComponentEditor;

/// <summary>
/// Scene ビューで編集する点の並び（Polygon Shape の外周・Arrow Strip の線）。点は Entity から見た X・Y
/// <para>持ち主（PolygonShapeGizmo・ArrowStripGizmo）が、毎フレーム Component から作って渡す</para>
/// </summary>
struct PointList {
	std::span<Vector2> storage;              // 点の置き場所（全部。足すと、後ろの点を1つずつずらす）
	int* count = nullptr;                    // 使っている点の数（足す・消すと変わる）
	int minCount = 2;                        // これより少なくは消せない
	bool isClosed = false;                   // 最後の点から最初の点へも辺がある（多角形）
	float planeZ = 0.0f;                     // 点を動かす面（Entity から見た Z）
	bool isValid = true;                     // false なら、辺を赤く描く（形がおかしい）
	std::span<const Vector3> curve;          // 点の間を通る線（Entity から見た座標。あれば、辺の代わりにこれを太く描く）
	const ComponentEditor* editor = nullptr; // Undo の写しを取る（Inspector と同じ ComponentEditor::Save）
	const void* component = nullptr;         // 写しを取る Component（点を持っている物）
};


/// <summary>
/// Scene ビューで、点を動かす・足す・消す（形・線の編集の共通部分）
/// <para>操作：■ をドラッグ（0.25m に吸い付く。Ctrl で吸い付かない）／辺の真ん中の ＋ を押すと点を足す（そのままドラッグできる）／
/// 点を選んで Delete で消す（minCount より少なくはできない）／Esc で編集を終わる</para>
/// <para>1回のドラッグ・足す・消すで、1件の履歴になる（Ctrl+Z で戻せる）</para>
/// <para>選んだ点などの状態は1つだけ持つ。別の物を渡した・前のフレームに呼ばれなかったら、状態を捨てる</para>
/// </summary>
class PointListGizmo {
public:
	/// <summary>
	/// Scene ビューの絵の上に描いて、マウス・キーで編集する。持ち主の Draw から毎フレーム1回
	/// </summary>
	/// <param name="entity">点を持っている Entity（Undo の記録に使う）</param>
	/// <param name="list">編集する点</param>
	/// <param name="world">Entity のワールド行列</param>
	/// <param name="camera">Scene ビューのカメラ</param>
	/// <param name="imageMin">絵の左上（画面の座標）</param>
	/// <param name="imageSize">絵の大きさ</param>
	/// <param name="isHovered">マウスが絵の上にあるか</param>
	/// <returns>編集を続けるか（Esc を押したら false。持ち主は編集中の印を消す）</returns>
	static bool Draw(Handle<Entity> entity, const PointList& list, const Matrix4x4& world, const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered);
};