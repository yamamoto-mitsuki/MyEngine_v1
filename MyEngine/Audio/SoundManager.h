#pragma once
#include <wrl.h>
#include <xaudio2.h>
#include "MyEngine/Utils/Easing.h"

#include <string>
#include <unordered_map>

class SoundManager {
public:
	// チャンクヘッダ
	struct ChunkHeader {
		char id[4];   // チャンク毎のID
		int32_t size; // チャンクサイズ
	};

	// RIFFヘッダチャンク
	struct RiffHeader {
		ChunkHeader chunk; // "RIFF"
		char type[4];      // "WAVE"
	};

	// FMTチャンク
	struct FormatChunk {
		ChunkHeader chunk; // "fmt "
		WAVEFORMATEX fmt;  // 波形フォーマット
	};

	// 音声データ
	struct SoundData {
		WAVEFORMATEX wfex;       // 波形フォーマット
		BYTE* pBuffer;           // バッファの先頭アドレス
		unsigned int bufferSize; // バッファのサイズ
	};

	/// <summary>
	/// 再生・エフェクトのパラメータをまとめた設定構造体。
	/// </summary>
	struct SoundConfig {
		// --- Play ---
		float volume = 1.0f; // 音量。0.0f=無音、1.0f=原音量
		float pitch = 1.0f;  // ピッチ。0.5f=1オクターブ下、1.0f=原音程、2.0f=1オクターブ上
		bool loop = false;   // trueでループ再生

		// --- SetPan ---
		float pan = 0.0f; // -1.0f=完全に左、0.0f=中央、1.0f=完全に右

		// --- SetLowPassFilter ---
		float lpfCutoff = 1.0f; // 0.0f=最大カット、1.0f=フィルタなし

		// --- SetHighPassFilter ---
		float hpfCutoff = 0.0f; // 0.0f=フィルタなし、1.0f=最大カット

		// --- SetReverb ---
		float reverbMix = 0.0f; // 0.0f=なし、1.0f=最大

		// --- FadeOut / FadeIn ---
		float fadeDuration = 1.0f;                // フェードにかける時間（秒）
		float fadeTargetVolume = 0.0f;            // フェード完了時の音量
		Ease::Type fadeEaseType = Ease::Type::Linear;
	};

public:
	/// <summary>
	/// 初期化。プログラム起動時に1度だけ呼ぶ
	/// </summary>
	static void Init();

	/// <summary>
	/// 終了処理。プログラム終了時に1度だけ呼ぶ。
	/// 読み込んだ音声データをすべて解放する
	/// </summary>
	static void Finalize();

	/// <summary>
	/// 音声ファイルの読み込み。wav/mp3両対応。
	/// </summary>
	/// <param name="filename">ファイルパス（例: "resources/bgm.mp3"）</param>
	/// <returns>読み込んだ音声の素材ハンドル</returns>
	static uint32_t Load(const std::string& filename);

	/// <summary>
	/// 音声を再生する。戻り値のハンドルで後からStop等の操作が可能
	/// </summary>
	/// <param name="soundHandle">再生したい音声の素材ハンドル</param>
	/// <param name="loop">trueでループ再生、falseで1回再生</param>
	/// <param name="volume">音量。0.0f=無音、1.0f=原音量（デフォルト1.0f）</param>
	/// <param name="pitch">ピッチ。0.5f=1オクターブ下、1.0f=原音程、2.0f=1オクターブ上（デフォルト1.0f）</param>
	/// <returns>再生中の再生ハンドル。Stop/Pause等の操作に使う</returns>
	static uint32_t Play(uint32_t soundHandle, bool loop, float volume = 1.0f, float pitch = 1.0f);

	/// <summary>
	/// 音声を再生する（SoundConfig版）。config の volume / pitch / loop を使用する。
	/// 戻り値のハンドルで後から Stop 等の操作が可能。
	/// </summary>
	/// <param name="soundHandle">再生したい音声の素材ハンドル</param>
	/// <param name="config">再生パラメータ（volume / pitch / loop）</param>
	/// <returns>再生中の再生ハンドル。Stop/Pause等の操作に使う</returns>
	static uint32_t Play(uint32_t soundHandle, const SoundConfig& config);

	/// <summary>
	/// 音声を停止してSourceVoiceを破棄する。
	/// </summary>
	/// <param name="playHandle">停止したい再生ハンドル</param>
	static void Stop(uint32_t playHandle);

	/// <summary>
	/// 音声を一時停止する。Resume()で再開できる
	/// </summary>
	/// <param name="playHandle">一時停止したい再生ハンドル</param>
	static void Pause(uint32_t playHandle);

	/// <summary>
	/// 一時停止中の音声を再開する
	/// </summary>
	/// <param name="playHandle">再開したい再生ハンドル</param>
	static void Resume(uint32_t playHandle);

	/// <summary>
	/// 再生中の音量を変更する
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="volume">音量。0.0f=無音、1.0f=原音量</param>
	static void SetVolume(uint32_t playHandle, float volume);

	/// <summary>
	/// 再生中の音量を変更する（SoundConfig版）。config.volume を使用する。
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="config">config.volume のみ参照する</param>
	static void SetVolume(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 再生中のピッチを変更する
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="pitch">ピッチ。0.5f=1オクターブ下、1.0f=原音程、2.0f=1オクターブ上</param>
	static void SetPitch(uint32_t playHandle, float pitch);

	/// <summary>
	/// 再生中のピッチを変更する（SoundConfig版）。config.pitch を使用する。
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="config">config.pitch のみ参照する</param>
	static void SetPitch(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 左右のパンニング（音の定位）を設定する
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="pan">-1.0f=完全に左、0.0f=中央、1.0f=完全に右</param>
	static void SetPan(uint32_t playHandle, float pan);

	/// <summary>
	/// 左右のパンニング（音の定位）を設定する（SoundConfig版）。config.pan を使用する。
	/// </summary>
	/// <param name="playHandle">変更したい再生ハンドル</param>
	/// <param name="config">config.pan のみ参照する</param>
	static void SetPan(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// リバーブ（残響）エフェクトをかける。
	/// </summary>
	/// <param name="playHandle">エフェクトをかけたい再生ハンドル</param>
	/// <param name="mix">エフェクトのかかり具合。0.0f=なし、1.0f=最大（デフォルト0.5f）</param>
	static void SetReverb(uint32_t playHandle, float mix);

	/// <summary>
	/// リバーブ（残響）エフェクトをかける（SoundConfig版）。config.reverbMix を使用する。
	/// </summary>
	/// <param name="playHandle">エフェクトをかけたい再生ハンドル</param>
	/// <param name="config">config.reverbMix のみ参照する</param>
	static void SetReverb(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// ローパスフィルタをかける。高音域をカットして篭った音になる。
	/// 壁の向こうの音や水中の表現に使う
	/// </summary>
	/// <param name="playHandle">フィルタをかけたい再生ハンドル</param>
	/// <param name="cutoff">カットオフ周波数の割合。0.0f=最大カット、1.0f=フィルタなし（デフォルト0.5f）</param>
	static void SetLowPassFilter(uint32_t playHandle, float cutoff);

	/// <summary>
	/// ローパスフィルタをかける（SoundConfig版）。config.lpfCutoff を使用する。
	/// </summary>
	/// <param name="playHandle">フィルタをかけたい再生ハンドル</param>
	/// <param name="config">config.lpfCutoff のみ参照する</param>
	static void SetLowPassFilter(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// ハイパスフィルタをかける。低音域をカットして細い音になる。
	/// 電話越しの声やラジオ音声の表現に使う
	/// </summary>
	/// <param name="playHandle">フィルタをかけたい再生ハンドル</param>
	/// <param name="cutoff">カットオフ周波数の割合。0.0f=フィルタなし、1.0f=最大カット（デフォルト0.5f）</param>
	static void SetHighPassFilter(uint32_t playHandle, float cutoff);

	/// <summary>
	/// ハイパスフィルタをかける（SoundConfig版）。config.hpfCutoff を使用する。
	/// </summary>
	/// <param name="playHandle">フィルタをかけたい再生ハンドル</param>
	/// <param name="config">config.hpfCutoff のみ参照する</param>
	static void SetHighPassFilter(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 音量を徐々に下げて停止する。
	/// </summary>
	/// <param name="playHandle">フェードアウトさせたい再生ハンドル</param>
	/// <param name="duration">フェードアウトにかける時間（秒）</param>
	/// <param name="targetVolume">最終的な音量</param>
	/// <param name="easeType">演出するイージング</param>
	static void FadeOut(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);

	/// <summary>
	/// 音量を徐々に下げて停止する（SoundConfig版）。
	/// config.fadeDuration / fadeTargetVolume / fadeEaseType を使用する。
	/// </summary>
	/// <param name="playHandle">フェードアウトさせたい再生ハンドル</param>
	/// <param name="config">config.fadeDuration / fadeTargetVolume / fadeEaseType のみ参照する</param>
	static void FadeOut(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 音量を徐々に上げる。
	/// </summary>
	/// <param name="playHandle">フェードインさせたい再生ハンドル</param>
	/// <param name="duration">フェードインにかける時間（秒）</param>
	/// <param name="targetVolume">最終的な音量</param>
	static void FadeIn(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);

	/// <summary>
	/// 音量を徐々に上げる（SoundConfig版）。
	/// config.fadeDuration / fadeTargetVolume / fadeEaseType を使用する。
	/// </summary>
	/// <param name="playHandle">フェードインさせたい再生ハンドル</param>
	/// <param name="config">config.fadeDuration / fadeTargetVolume / fadeEaseType のみ参照する</param>
	static void FadeIn(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// リバーブ（残響）エフェクトを解除する
	/// </summary>
	/// <param name="playHandle">エフェクトを解除したい再生ハンドル</param>
	static void RemoveReverb(uint32_t playHandle);

	/// <summary>
	/// 再生終了した音声を破棄する。毎フレーム呼ぶこと。
	/// </summary>
	/// <param name="deltaTime">基本は1.0f / 60.0f</param>
	static void CleanupSourceVoices(float deltaTime);

private:
	// フェードの状態を管理する構造体
	struct FadeInfo {
		float startVolume;  // フェード開始時の音量
		float targetVolume; // フェード終了時の音量
		float timer;        // フェードの経過時間
		float duration;     // フェードにかける時間
		Ease::Type easeType;
	};

	static SoundManager* GetInstance();

	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	// 内部処理用の関数
	void initInternal();
	void finalizeInternal();
	uint32_t loadInternal(const std::string& filename);
	uint32_t playInternal(uint32_t soundHandle, bool loop, float volume, float pitch);
	void setVolumeInternal(uint32_t playHandle, float volume);
	void setPitchInternal(uint32_t playHandle, float pitch);
	void setPanInternal(uint32_t playHandle, float pan);
	void setReverbInternal(uint32_t playHandle, float mix);
	void setLowPassFilterInternal(uint32_t playHandle, float cutoff);
	void setHighPassFilterInternal(uint32_t playHandle, float cutoff);
	void fadeOutInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);
	void fadeInInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);
	void updateFadeInternal(float deltaTime);

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// 音声データのパスから素材番号を引く用
	std::unordered_map<std::string, uint32_t> pathID_;
	// 素材番号から波形データを引く用
	std::unordered_map<uint32_t, SoundData> soundMap_;
	// 再生番号から再生中のvoiceポインタを引く用
	std::unordered_map<uint32_t, IXAudio2SourceVoice*> voices_;
	// 再生番号からフェード状態を引く用
	std::unordered_map<uint32_t, FadeInfo> fadeInfos_;

	// 再生ハンドルの発行に使うカウンタ。0は無効なハンドルとするため、1からスタートする
	uint32_t nextPlayHandle_ = 1;
};