#pragma once
#include <string>

// シーンファイルを読んだ結果
enum class SceneLoadResult {
	Loaded,   // 読めた
	NotFound, // ファイルが無い（まだ一度も保存していないシーン）
	Failed,   // ファイルはあるが読めない（壊れている・新しい版のエンジンで保存された）
};


/// <summary>
/// 全Entity（名前・有効・親子・Component）をJSONにする / JSONから作る
/// <para>Componentの中身は ComponentSerializer に登録した型だけが書かれる（COMPONENT(...) の型とエンジンの型は全部入っている）</para>
/// <para>読み込みはフレームの境目（SceneManager::Update の頭）でだけ行う。今あるEntityは消さないので、先に DestroyAllNow を呼ぶ</para>
/// </summary>
class SceneSerializer {
public:
	// ファイルの形の版。値の意味を変えたら（回転をクオータニオンにする等）上げて、古い版を読み替える処理を足す
	static constexpr int kVersion = 1;

	// ===== ファイル =====
	// フォルダが無ければ作る。前のファイルは「〇〇.bak」として1つだけ残す
	static bool SaveFile(const std::string& path);
	static SceneLoadResult LoadFile(const std::string& path);

	// ===== 文字列（Playを押した瞬間の退避に使う。ファイルと同じ中身）=====
	static std::string SaveToText();
	static bool LoadFromText(const std::string& text);

	// 全Entityを今すぐ消す（予約ではない。同じEntityIdで作り直すため）。フレームの境目でだけ呼ぶ
	static void DestroyAllNow();
};