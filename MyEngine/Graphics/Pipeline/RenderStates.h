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

// シェーディングタイプ
enum class ShadingType {
	Unlit,       // Lightingなし
	Lambert,     // Lambert
	HalfLambert, // Half Lambert
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

class RenderStates {
public:
	D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode);
};
