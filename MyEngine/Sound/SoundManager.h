#pragma once
#include "MyEngine/Utils/Easing.h"
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <xaudio2.h>

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
	/// 再生・エフェクトのパラメータをまとめた設定構造体
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
		float fadeDuration = 1.0f;     // フェードにかける時間（秒）
		float fadeTargetVolume = 0.0f; // フェード完了時の音量
		Ease::Type fadeEaseType = Ease::Type::Linear; // フェードのイージング（Ease::Type:: で呼ぶ）
	};

public:
	// コピー・ムーブ禁止
	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	/// <summary>
	/// 初期化。Engine::Initialize()内で1度だけ呼ぶ
	/// </summary>
	static void Initialize();

	/// <summary>
	/// 終了処理。Engine::Finalize()内で呼ぶ。
	/// 再生中の全ボイスと読み込み済みの全音声データを解放する
	/// </summary>
	static void Release();

	/// <summary>
	/// 音声ファイルを読み込む。wav/mp3両対応。同じパスを渡すとキャッシュから返す
	/// </summary>
	/// <param name="filename">ファイルパス（例: "resources/bgm.mp3"）</param>
	/// <returns>読み込んだ音声の素材ハンドル</returns>
	static uint32_t Load(const std::string& filename);

	/// <summary>
	/// 音声を再生する。戻り値の再生ハンドルでStop等の操作が可能
	/// </summary>
	/// <param name="soundHandle">Load()が返した素材ハンドル</param>
	/// <param name="loop">trueでループ再生</param>
	/// <param name="volume">音量 0.0f=無音 1.0f=原音量</param>
	/// <param name="pitch">ピッチ 0.5f=1オクターブ下 1.0f=原音程 2.0f=1オクターブ上</param>
	/// <returns>再生ハンドル。Stop/Pause等の操作に使う</returns>
	static uint32_t Play(uint32_t soundHandle, bool loop, float volume = 1.0f, float pitch = 1.0f);

	/// <summary>
	/// 音声を再生する（SoundConfig版）
	/// </summary>
	static uint32_t Play(uint32_t soundHandle, const SoundConfig& config);

	/// <summary>
	/// 音声を停止してSourceVoiceを破棄する
	/// </summary>
	static void Stop(uint32_t playHandle);

	/// <summary>
	/// 音声を一時停止する。Resume()で再開できる
	/// </summary>
	static void Pause(uint32_t playHandle);

	/// <summary>
	/// 一時停止中の音声を再開する
	/// </summary>
	static void Resume(uint32_t playHandle);

	/// <summary>
	/// 再生中の音量を変更する
	/// </summary>
	static void SetVolume(uint32_t playHandle, float volume);

	/// <summary>
	/// 再生中の音量を変更する（SoundConfig版）。config.volumeを使用する
	/// </summary>
	static void SetVolume(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 再生中のピッチを変更する
	/// </summary>
	static void SetPitch(uint32_t playHandle, float pitch);

	/// <summary>
	/// 再生中のピッチを変更する（SoundConfig版）。config.pitchを使用する
	/// </summary>
	static void SetPitch(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 左右のパンニング（音の定位）を設定する。-1.0f=左 0.0f=中央 1.0f=右
	/// </summary>
	static void SetPan(uint32_t playHandle, float pan);

	/// <summary>
	/// 左右のパンニングを設定する（SoundConfig版）。config.panを使用する
	/// </summary>
	static void SetPan(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// リバーブ（残響）エフェクトをかける。0.0f=なし 1.0f=最大
	/// </summary>
	static void SetReverb(uint32_t playHandle, float mix);

	/// <summary>
	/// リバーブをかける（SoundConfig版）。config.reverbMixを使用する
	/// </summary>
	static void SetReverb(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// ローパスフィルタをかける。高音域をカットして篭った音になる。
	/// 壁の向こうの音や水中の表現に使う。0.0f=最大カット 1.0f=フィルタなし
	/// </summary>
	static void SetLowPassFilter(uint32_t playHandle, float cutoff);

	/// <summary>
	/// ローパスフィルタをかける（SoundConfig版）。config.lpfCutoffを使用する
	/// </summary>
	static void SetLowPassFilter(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// ハイパスフィルタをかける。低音域をカットして細い音になる。
	/// 電話越しの声やラジオ音声の表現に使う。0.0f=フィルタなし 1.0f=最大カット
	/// </summary>
	static void SetHighPassFilter(uint32_t playHandle, float cutoff);

	/// <summary>
	/// ハイパスフィルタをかける（SoundConfig版）。config.hpfCutoffを使用する
	/// </summary>
	static void SetHighPassFilter(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 音量を徐々に下げる。targetVolume=0.0fで完全に無音になる
	/// </summary>
	static void FadeOut(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);

	/// <summary>
	/// 音量を徐々に下げる（SoundConfig版）。
	/// config.fadeDuration / fadeTargetVolume / fadeEaseTypeを使用する
	/// </summary>
	static void FadeOut(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// 音量を徐々に上げる。音量0から始めてtargetVolumeまで上げる
	/// </summary>
	static void FadeIn(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);

	/// <summary>
	/// 音量を徐々に上げる（SoundConfig版）。
	/// config.fadeDuration / fadeTargetVolume / fadeEaseTypeを使用する
	/// </summary>
	static void FadeIn(uint32_t playHandle, const SoundConfig& config);

	/// <summary>
	/// リバーブエフェクトを解除する
	/// </summary>
	static void RemoveReverb(uint32_t playHandle);

	/// <summary>
	/// 再生終了した音声のSourceVoiceを破棄する。毎フレーム呼ぶこと
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間</param>
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

	// staticポインタ。Init()で生成、Release()で解放する。
	static SoundManager* instance_;

	// 内部処理用の関数
	void InitInternal();
	void ReleaseInternal();
	uint32_t LoadInternal(const std::string& filename);
	uint32_t PlayInternal(uint32_t soundHandle, bool loop, float volume, float pitch);
	void SetVolumeInternal(uint32_t playHandle, float volume);
	void SetPitchInternal(uint32_t playHandle, float pitch);
	void SetPanInternal(uint32_t playHandle, float pan);
	void SetReverbInternal(uint32_t playHandle, float mix);
	void SetLowPassFilterInternal(uint32_t playHandle, float cutoff);
	void SetHighPassFilterInternal(uint32_t playHandle, float cutoff);
	void FadeOutInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);
	void FadeInInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType);
	void UpdateFadeInternal(float deltaTime);

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// 音声データのパスから素材ハンドルを引く用
	std::unordered_map<std::string, uint32_t> pathID_;
	// 素材ハンドルから波形データを引く用
	std::unordered_map<uint32_t, SoundData> soundMap_;
	// 再生ハンドルから再生中のvoiceポインタを引く用
	std::unordered_map<uint32_t, IXAudio2SourceVoice*> voices_;
	// 再生ハンドルからフェード状態を引く用
	std::unordered_map<uint32_t, FadeInfo> fadeInfos_;

	// 再生ハンドルの発行カウンタ。0は無効なハンドルとするため1からスタートする
	uint32_t nextPlayHandle_ = 1;
};