#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector4.h"

// 画面に重ねる絵（HUD・ゲージ・ボタン・フェードの幕）。Entityに付けるComponent（カテゴリ：UI）
// 位置は Transform の translation の x・y を「画面のピクセル」として読む（1280×720 の画面のつもりで置く。ウィンドウの大きさが変わっても同じ割合で出る）
// Y は上が＋（3D と同じ）。回転は rotation の z、大きさには scale の x・y も掛かる。親子にすると一緒に動く（UI をまとめる親を作るとよい）
// 3D空間に置く板は Sprite Renderer（SpriteRendererComponent）を使う

/// <summary>
/// 画面のどこから位置を数えるか（Unity の Anchor）
/// </summary>
enum class ScreenAnchor {
	TopLeft,     // 左上
	Top,         // 上の真ん中
	TopRight,    // 右上
	Left,        // 左の真ん中
	Center,      // 真ん中
	Right,       // 右の真ん中
	BottomLeft,  // 左下
	Bottom,      // 下の真ん中
	BottomRight, // 右下
};

/// <summary>
/// 画面に重ねる絵（Unity の UI の Image）
/// </summary>
struct ImageComponent {
	bool enabled = true;                        // false の間は描かない（マウスも当たらない）
	uint32_t textureHandle = 0;                 // 画像（TextureManager::Load の番号。0 なら白い四角。黒くすればフェードの幕）。保存はパスで
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};   // 掛ける色（RGBA。0〜1。a を動かすとフェード）
	Vector2 size = {100.0f, 100.0f};            // 大きさ（ピクセル）。Inspector の Set Native Size で画像（1コマ）と同じ大きさになる
	ScreenAnchor anchor = ScreenAnchor::Center; // 画面のどこから位置を数えるか（TopLeft なら、左上の角から右へ x・上へ y）
	Vector2 pivot = {0.5f, 0.5f};               // 絵のどこを位置に合わせるか（0,0=左下 0,1=左上 1,1=右上）
	int columns = 1;                            // 画像を横に何コマに分けるか（コマを並べた画像）
	int rows = 1;                               // 縦に何コマに分けるか
	int frame = 0;                              // 何コマ目を出すか（左上が 0。右へ 1, 2 …、行の終わりで次の行へ）
	bool flipX = false;                         // 左右反転
	bool flipY = false;                         // 上下反転
	float fill = 1.0f;                          // 左から何割まで出すか（0〜1。体力・インクのゲージに）
	int order = 0;                              // 重ねる順（大きいほど手前。同じなら先に作った物が奥）
	BlendMode blend = BlendMode::Normal;        // 重ね方（Normal：透明な所が透ける / Add：足して光る）
};

static_assert(std::is_trivially_copyable_v<ImageComponent>);