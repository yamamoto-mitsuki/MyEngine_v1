#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include "Animation.h"


/// <summary>
/// アニメーションファイルを読み込み、番号（ハンドル）で管理する
/// </summary>
class AnimationManager {
public:
	static constexpr uint32_t kInvalidHandle = 0; // 読み込み失敗時の番号

	// ===== Passkey Idiom =====
	class ConstructorKey {
	private:
		ConstructorKey() = default;
		friend class AnimationManager;
	};
	explicit AnimationManager(ConstructorKey) {} // 鍵を受け取るコンストラクタ

	// コピー禁止
	AnimationManager(const AnimationManager&) = delete;
	AnimationManager& operator=(const AnimationManager&) = delete;

	// 初期化・解放
	static void Initialize();
	static void Release();

	/// <summary>
	/// アニメーションファイルを読み込む。同じパスならキャッシュから返す
	/// </summary>
	/// <param name="filePath">ファイルのパス（例: "resources/models/AnimatedCube/AnimatedCube.gltf"）</param>
	/// <returns>ハンドル。読めなかった・アニメーションが無いときは kInvalidHandle</returns>
	static uint32_t Load(const std::filesystem::path& filePath);

	/// <summary>
	/// アニメーションを取得する
	/// </summary>
	/// <param name="handle">Load が返したハンドル</param>
	/// <param name="index">ファイルの中の何番目か（ふつうは0）</param>
	/// <returns>見つからなければ nullptr</returns>
	static const Animation* GetAnimation(uint32_t handle, uint32_t index = 0);

	// ファイルの中のアニメーションの数（見つからなければ0）
	static uint32_t GetAnimationCount(uint32_t handle);
	// 読み込んだときのパス（見つからなければ空）。後でシーンに保存するときに、番号をパスに直すのに使う
	static const std::string& GetPath(uint32_t handle);

private:
	// ===== 1ファイル分 =====
	struct AnimationFile {
		std::string path;                  // 読み込んだときのパス（ToPathKey でそろえた UTF-8 の文字）
		std::vector<Animation> animations; // ファイルの中のアニメーション全部
	};

	/// <summary>
	/// assimpでファイルを読み、中のアニメーションを全部 Animationに直す
	/// </summary>
	/// <param name="path"></param>
	/// <returns></returns>
	static std::vector<Animation> LoadAnimationFile(const std::filesystem::path& path);


	// ハンドルからファイルを探す（無ければ nullptr）
	static const AnimationFile* Find(uint32_t handle);

	// シングルトン
	friend struct std::default_delete<AnimationManager>;
	~AnimationManager() = default;
	static std::unique_ptr<AnimationManager> instance_;

	std::unordered_map<uint32_t, AnimationFile> files_;      // ハンドル → 1ファイル分
	std::unordered_map<std::string, uint32_t> pathToHandle_; // パス → ハンドル（同じファイルを2回読まない）
	uint32_t nextHandle_ = 1;                                // 次に配る番号（0 は失敗の印なので1から）
};