#pragma once
#include <d3d12.h>

// 描画したい形
enum class DrawCategory {
	Sprite, // Spriteは深度情報の関係で必ず最後に描画すること
	Model,
	Line,
};

// サンプリング設定
enum class SamplingType { 
	LinearWrap, // 通常描画
	LinearClamp,
	PointClamp,
	ShadowMap,
};

// シェーディング設定
enum class ShadingType {
	Unlit,       // Lightingなし
	Lambert,     // Lambert
	HalfLambert, // Half Lambert
	Phong,       // PhongReflection
	BlinnPhong   // BlinnPhongReflection
};

// ブレンド設定
enum class BlendMode {
	None,     // ブレンド無し
	Normal,   // 通常aブレンド（デフォルト）: Src * SrcA + Dest * (1 - SrcA)
	Add,      // 加算: Src * SrcA + Dest * 1
	Subtract, // 減算: Dest * 1 - Src * SrcA
	Multiply, // 乗算: Src * 0 + Dest * Src
	Screen,   // スクリーン: Src * (1 - Dest) + Dest * 1
};

// ラスタライザ設定
enum RasterizerType {
	SolidBack,  // 通常描画:      背面カリング + 塗りつぶし
	SolidNone,  // 両面描画:      カリングなし
	SolidFront, // シャドウマップ用
	Wireframe,  // デバック用:    ワイヤー（塗りつぶさない）
};

// 深度設定
enum DepthMode {
	TestWrite,  // 深度テストあり・書き込み有: 不透明3D
	TestNoWrite, // 深度テストあり・書き込み無: 半透明3D
	Disable,     // 深度テストなし・書き込み無: 2D, UI, フルスクリーン
};


/// <summary>
/// 描画の際に調整したい設定の登録クラス
/// </summary>
class RenderStates {
public:

	/// <summary>
	/// 引数に入れた BlendMode の設定を返す
	/// </summary>
	static D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode);

	/// <summary>
	/// 引数に入れた RasterizerTypeの設定を返す
	/// </summary>
	static D3D12_RASTERIZER_DESC MakeRasterizerDesc(RasterizerType type);

	/// <summary>
	/// 引数にいれた
	/// </summary>
	static D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(DepthMode mode);
};
