#pragma once
#include <cstdint>
#include <string>
#include <filesystem>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Vector3.h"

// Prefab の置き場所と、名前の後ろに付く部分（"resources/prefabs/Box.prefab" の Prefab の名前は "Box"）
inline constexpr const char* kPrefabFolder = "resources/prefabs/";
inline constexpr const char* kPrefabFileExtension = ".prefab";
// 名前から Prefab のファイルの場所を作る（"Box" → "resources/prefabs/Box.prefab"）
inline std::string MakePrefabFilePath(const std::string& name) { return std::string(kPrefabFolder) + name + kPrefabFileExtension; }


/// <summary>
/// Prefab（Entity・その子・Component・Script をまとめてファイルに取っておき、同じ物を何個でも作れる物。Unity の Prefab）
/// <para>ファイルの中身はシーンファイルと同じ形（{"version", "entities"}）。並びは一番上の Entity が先頭で、親が子より前</para>
/// <para>作る：Instantiate。Entity と Transform はその場でできるので、すぐ位置を変えてよい。
/// ほかの Component・Script は次のフレームの頭（Flush）に付く（GameObject::AddComponent と同じ「予約」）</para>
/// <para>作った Entity は Prefab とつながっていない（Prefab を保存し直しても、もう作った物は変わらない）</para>
/// </summary>
class PrefabManager {
public:
	static void Initialize();
	static void Release();

	// ===== 読み込み（アセット。InspectorのAssetField（AssetType::Prefab）からも呼ばれる）=====
	// 同じファイルは1回だけ読む（2回目からは同じ番号を返す）。読めなければ0
	static uint32_t Load(const std::filesystem::path& filePath);
	// 番号からパス（UTF-8。保存・Inspector の表示に使う）。知らない番号なら空
	static const std::string& GetPath(uint32_t handle);

	// ===== 保存（エディタの Hierarchy の右クリック → Save As Prefab から）=====
	/// <summary>
	/// rootと、その子孫をPrefabのファイルに書く。もう読み込んでいる Prefab なら、中身も差し替える（このあと作る物から新しい中身になる）
	/// </summary>
	/// <param name="path">書く場所（UTF-8。ふつうは MakePrefabFilePath(名前)）</param>
	static bool Save(Handle<Entity> root, const std::string& path);

	// ===== 作る =====
	/// <summary>
	/// Prefab から Entity を作り、一番上の Entity を返す（読めていない番号なら無効な Handle）
	/// <para>一番上の Entity の位置（translation）は position になる。回転・大きさは Prefab のまま</para>
	/// </summary>
	/// <param name="parent">親（無効な Handle なら一番上に置く）</param>
	static Handle<Entity> Instantiate(uint32_t prefab, const Vector3& position, Handle<Entity> parent = {});
	// エディタから置く（ImGui を描いている途中で Entity を増やさないように、次のフレームの頭に作る。原点に置く）
	static void RequestPlace(uint32_t prefab);
	// 予約した物を作る・Component を付ける。フレームの頭（WindowManager::UpdateAll の EntityManager::FlushComponentChanges の後）に呼ぶ
	static void Flush();
};