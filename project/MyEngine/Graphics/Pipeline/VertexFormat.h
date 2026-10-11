#pragma once
#include <format>
#include <vector>
#include <d3d12.h>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Pipeline/ShaderCompiler.h"

//=============================================================================
// 頂点データ構造体
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
// パーティクルデータ構造体
struct VertexParticleData {
	Vector4 position;
	Vector2 texcoord;
};
// Line3D頂点データ構造
struct VertexLineData {
	Vector4 position;
	Vector4 color;
};


/// <summary>
/// 
/// </summary>
class VertexFormat {
public:
	/// <summary>
	/// シェーダーリフレクション情報からInputLayoutを生成
	/// </summary>
	static std::vector<D3D12_INPUT_ELEMENT_DESC> MakeInputLayout(const std::vector<ShaderInputParameter>& inputs);

private:
	/// <summary>
	/// シェーダー入力パラメータから対応するDXGI_FORMATを取得
	/// </summary>
	static DXGI_FORMAT FormatOf(D3D_REGISTER_COMPONENT_TYPE type, BYTE mask);
};