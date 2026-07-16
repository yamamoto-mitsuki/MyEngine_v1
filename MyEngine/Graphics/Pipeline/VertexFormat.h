#pragma once
#include <format>

#include <d3d12.h>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Math/MathIncludes.h"

//=============================================================================
// InputLayout（頂点シェーダ―）に入れるもの
//=============================================================================

// 2D頂点データ構造
struct Vertex2dData {
	Vector4 position;
	Vector2 texcoord;
};

// 3D頂点データ構造
struct Vertex3dData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// Line3D頂点データ構造
struct VertexLineData {
	Vector4 position;
	Vector4 color;
};


//=============================================================================
// InputLayout
//=============================================================================

// InputLayout の全種類
enum class InputLayoutID {
	Sprite,
	Model,
	Particle,
	Line,
};

// ===== InputLayout の全情報 =====
namespace {
// Sprite
const D3D12_INPUT_ELEMENT_DESC kInputElementSpriteDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};
const D3D12_INPUT_LAYOUT_DESC kInputLayoutSpriteDesc{kInputElementSpriteDescs, _countof(kInputElementSpriteDescs)};
// Model
const D3D12_INPUT_ELEMENT_DESC kInputElementModelDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};
const D3D12_INPUT_LAYOUT_DESC kInputLayoutModelDesc{kInputElementModelDescs, _countof(kInputElementModelDescs)};
// Particle
const D3D12_INPUT_ELEMENT_DESC kInputElementParticleDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};
const D3D12_INPUT_LAYOUT_DESC kInputLayoutParticleDesc{kInputElementParticleDescs, _countof(kInputElementParticleDescs)};
// Line
const D3D12_INPUT_ELEMENT_DESC kInputElementLineDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};
const D3D12_INPUT_LAYOUT_DESC kInputLayoutLineDesc{kInputElementLineDescs, _countof(kInputElementLineDescs)};
} // namespace


/// <summary>
/// InputLayoutID から D3D12_INPUT_LAYOUT_DESC を入手
/// </summary>
/// <param name="id">InputLayout の全種類を格納したenum</param>
static D3D12_INPUT_LAYOUT_DESC GetInputLayout(InputLayoutID id) {

	// --- IDで分岐 ---
	switch (id) {
	case InputLayoutID::Sprite:
		return kInputLayoutSpriteDesc;
	case InputLayoutID::Model:
		return kInputLayoutModelDesc;
	case InputLayoutID::Particle:
		return kInputLayoutParticleDesc;
	case InputLayoutID::Line:
		return kInputLayoutLineDesc;
	default:
		MY_ASSERT_MSG(false, std::format("{} 存在していないInputLayoutIDです", magic_enum::enum_name(id)));
		break;
	}
}
