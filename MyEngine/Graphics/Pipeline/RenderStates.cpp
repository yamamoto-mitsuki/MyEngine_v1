#include "MyEngine/Graphics/Pipeline/RenderStates.h"


//=============================================================================
// BlendMode → D3D12_BLEND_DESC
//=============================================================================
D3D12_BLEND_DESC RenderStates::MakeBlendDesc(BlendMode mode) {
	// ブレンド設定
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	// BlendModeによって分岐
	switch (mode) {
	// --- ブレンド無し ---
	case BlendMode::None:
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	// --- 通常aブレンド（デフォルト） ---
	case BlendMode::Normal:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	// --- 加算 ---
	case BlendMode::Add:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	// --- 減算 ---
	case BlendMode::Subtract:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_SUBTRACT;
		break;
	// --- 乗算 ---
	case BlendMode::Multiply:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	// --- スクリーン合成 ---
	case BlendMode::Screen:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		break;
	}

	return blendDesc;
}


//=============================================================================
// RasterizerType → D3D12_RASTERIZER_DESC
//=============================================================================
D3D12_RASTERIZER_DESC RenderStates::MakeRasterizerDesc(RasterizerType type) {
	D3D12_RASTERIZER_DESC desc{};
	// --- 全タイプ共通の既定値 ---
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = D3D12_CULL_MODE_BACK;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthClipEnable = TRUE;

	// --- タイプで変わる所だけ上書き ---
	switch (type) {
	case RasterizerType::SolidBack:
		// 既定のまま（背面カリング＋塗りつぶし）
		break;

	case RasterizerType::SolidNone:
		desc.CullMode = D3D12_CULL_MODE_NONE; // 両面描画
		break;

	case RasterizerType::Wireframe:
		desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		break;

	case RasterizerType::SolidFront:
		// シャドウマップ生成パス用：前面カリング＋深度バイアスで影のアクネ/ピーターパンを抑制
		desc.CullMode = D3D12_CULL_MODE_FRONT;
		desc.DepthBias = 10000;           // 定数バイアス（深度フォーマット依存。要調整）
		desc.SlopeScaledDepthBias = 1.5f; // 傾斜バイアス（要調整）
		desc.DepthBiasClamp = 0.0f;
		break;
	}
	return desc;
}


//=============================================================================
// DepthMode → D3D12_DEPTH_STENCIL_DESC
//=============================================================================
D3D12_DEPTH_STENCIL_DESC RenderStates::MakeDepthStencilDesc(DepthMode mode) {
	D3D12_DEPTH_STENCIL_DESC desc{};
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 既定（手前を採用）
	
	switch (mode) {
	case DepthMode::TestWrite: // 不透明3D
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		break;

	case DepthMode::TestNoWrite: // 半透明3D（不透明の深度に対してテストはするが書かない）
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		break;

	case DepthMode::Disable: // 2D / UI / フルスクリーン
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		break;
	}
	return desc;
}