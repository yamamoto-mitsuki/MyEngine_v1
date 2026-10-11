#pragma once
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <source_location>

#include <externals/spdlog/common.h>
#include <externals/spdlog/spdlog.h>
#include <externals/spdlog/async.h>

/// <summary>
/// Log管理クラス
/// <para>呼び出すと自動で ファイル名, 行数, 呼び出した箇所の関数名 も出力される</para>
/// </summary>
class LogManager {
public:
	/// <summary>
	/// 初期化。Engine::Initialize()内で1度だけ呼ぶ。
	/// </summary>
	static void Initialize();

	/// <summary>
	/// 終了処理。全シンクをフラッシュしてスレッドを止める。
	/// </summary>
	static void Shutdown();

	/// <summary>
	/// バッファを即時フラッシュする。
	/// <para>assertの直前など、強制終了前に確実にログを書き出したいときに呼ぶ。</para>
	/// </summary>
	static void Flush() {
		if (instance_->logger_) {
			instance_->logger_->flush();
		}
	}

	/// <summary>
	/// 現在のログファイルパスを返す
	/// </summary>
	static const std::string& GetLogFilePath() { return instance_->logFilePath_; }

	/// <summary>
	/// INFOレベルでログを出力する。
	/// </summary>
	static void Log(const std::string& message, const std::source_location& loc = std::source_location::current());

	 /// <summary>
	/// WARNレベルでログを出力する。ImGuiで黄色表示される。
	/// </summary>
	static void Warning(const std::string& message, const std::source_location& loc = std::source_location::current());

	/// <summary>ERRORレベルでログを出力する。
	/// ImGuiで赤色表示される。
	/// </summary>
	static void Error(const std::string& message, const std::source_location& loc = std::source_location::current());

#ifdef USE_IMGUI
	/// <summary>
	/// ImGuiのログウィンドウを描画する。
	/// </summary>
	static void DrawImGuiWindow();
#endif

private:
	static LogManager* instance_;

	// spdlogの非同期ロガー本体
	std::shared_ptr<spdlog::async_logger> logger_;

	std::ofstream logStream_;
	std::string logFilePath_;

#ifdef USE_IMGUI
	// ImGui表示用バッファ
	struct LogEntry {
		std::string message;
		spdlog::level::level_enum level;
	};
	static constexpr int kMaxLines = 500;

	std::vector<LogEntry> logBuffer_;
	std::mutex bufferMutex_; // 非同期スレッドからも書き込まれるので排他制御
	bool scrollToBottom_;
	bool autoScroll_;
#endif
};
