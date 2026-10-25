#pragma once
#include <string>
#include <filesystem>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"

// 画面のファイルの置き場所と、名前の後ろに付く部分（"resources/ui/Pause.uiscreen" の画面の名前は "Pause"）
inline constexpr const char* kUIScreenFolder = "resources/ui/";
inline constexpr const char* kUIScreenFileExtension = ".uiscreen";
// 名前から画面のファイルの場所を作る（"Pause" → "resources/ui/Pause.uiscreen"）
inline std::string MakeUIScreenFilePath(const std::string& name) { return std::string(kUIScreenFolder) + name + kUIScreenFileExtension; }


/// <summary>
/// 画面（ポーズ画面・クリア画面など）を出す・消す
/// <para>画面は resources/ui/〇〇.uiscreen のファイル（中身は Prefab と同じ形。全部のシーンで使える）。エディタの Edit UI Screen で作る</para>
/// <para>出すと、ファイルから今のシーンに Entity を作る（Prefab の Instantiate）。消すと、その Entity を消す。シーンが変わると一緒に消える</para>
/// <para>動きは一番上の Entity の UI Screen Component（出し方・消し方・ゲームを止めるか）。時間は止めない時間で数える</para>
/// <para>ゲームを止めている間（ポーズ中）も、開いている画面の中の Script は呼ばれる（項目を選ぶ Script を画面の中に置ける）</para>
/// </summary>
class UIScreen {
public:
	// UI Screen・UI Screen Element を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	// ===== ゲームから使う =====
	// name の画面を出す（Component が付くのは次のフレームの頭なので、見え始めるのは次のフレーム）
	// 停止中・もう出ている・消している途中・ファイルが無いときは false
	static bool Open(const std::string& name);
	// name の画面を消す（消し方の動きが終わったら、Entity を消す）。出ていなければ false
	static bool Close(const std::string& name);
	// name の画面が出ているか（出している途中も true。消している途中は false）
	static bool IsOpen(const std::string& name);

	// ===== エンジンから使う =====
	// entity が、開いている画面の中の物か（画面の一番上の Entity も含む）。ポーズ中に呼ぶ Script を選ぶのに使う
	static bool IsInsideOpenScreen(Handle<Entity> entity);
	// フレームの頭（PrefabManager::Flush の直後）。Component が付いたばかりの画面に、出し始めの形を当てる（描く前に当てるので、一瞬元の形で見えることはない）
	static void Flush();
	// 毎フレーム（WindowManager::UpdateAll）。出す・消す動きを進めて、ゲームを止める・動かす。停止中は全部忘れる
	static void Update(bool isGameRunning);

	// ===== エディタから使う =====
	// 中身が一番上の Entity（UI Screen Component 付き）だけの画面のファイルを作る（Project ウィンドウの Create → UI Screen）
	static bool CreateNewFile(const std::filesystem::path& path);
};