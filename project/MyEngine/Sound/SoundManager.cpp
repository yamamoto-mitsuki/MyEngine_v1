#include "MyEngine/Sound/SoundManager.h"

#include <format>
#include <vector>
#include <fstream>
#include <algorithm>

#include <mfapi.h>
#include <mfidl.h>
#include <xaudio2fx.h>
#include <mfreadwrite.h>

#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/String/ConvertString.h"

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// staticメンバの定義
std::unique_ptr<SoundManager> SoundManager::instance_ = nullptr;

namespace {
constexpr uint32_t kBusChannels = 2; // 種類ごとの通り道は2ch（左右）。左右の振り分けは、いつもこの2chに対して書けばよい

// CoTaskMemFree で返す決まりのメモリを、unique_ptr に返してもらうための「消し方」
// （unique_ptr はふつう delete で消すので、消し方だけ差し替える）
struct CoTaskMemDeleter {
	void operator()(void* p) const { CoTaskMemFree(p); }
};

// 音の出口（左右の2ch）への割り振りを書く。gain は全体に掛ける大きさ、pan は -1=左 〜 1=右
// 中央（pan=0）は左右とも gain のまま＝何もしていない音と同じ大きさ。右へ振るほど左だけ小さくする
void ApplyOutputMatrix(IXAudio2SourceVoice* voice, float gain, float pan) {
	const float left = gain * std::clamp(1.0f - pan, 0.0f, 1.0f);
	const float right = gain * std::clamp(1.0f + pan, 0.0f, 1.0f);
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	const uint32_t inputChannels = details.InputChannels;
	// matrix[入力ch数 × 出口ch + 入力ch] が「入力chから出口chへ送る大きさ」
	std::vector<float> matrix(inputChannels * kBusChannels, 0.0f);
	if (inputChannels == 1) {
		// モノラル：1つの音を左右の両方へ
		matrix[0] = left;
		matrix[1] = right;
	} else {
		// ステレオ以上：偶数chを左、奇数chを右へ
		for (uint32_t channel = 0; channel < inputChannels; ++channel) {
			const uint32_t output = channel % 2;
			matrix[inputChannels * output + channel] = (output == 0) ? left : right;
		}
	}
	// 送り先は種類ごとの通り道1つだけなので、nullptr（＝その1つ）でよい
	voice->SetOutputMatrix(nullptr, inputChannels, kBusChannels, matrix.data());
}
} // namespace


//=============================================================================
// NVIパターン
//=============================================================================
void SoundManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()を2回呼んでいます");
	instance_ = std::make_unique<SoundManager>(ConstructorKey());
	instance_->InitInternal();
}

void SoundManager::Release() {
	MY_ASSERT_MSG(instance_ != nullptr, "Initialize()より先にRelease()が呼ばれています");
	instance_->ReleaseInternal();
	instance_.reset();
}

uint32_t SoundManager::Load(const std::filesystem::path& filePath) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->LoadInternal(filePath);
}

uint32_t SoundManager::Play(uint32_t soundHandle, bool loop, float volume, float pitch, AudioBus bus) {
	MY_ASSERT(instance_ && "[SoundManager::Play] Init()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, loop, volume, pitch, bus);
}

uint32_t SoundManager::Play(uint32_t soundHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, config.loop, config.volume, config.pitch, config.bus);
}

void SoundManager::SetVolume(uint32_t playHandle, float volume) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetVolume] playHandle={} volume={:.3f}", playHandle, volume));
	instance_->SetVolumeInternal(playHandle, volume);
}

void SoundManager::SetVolume(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetVolume] playHandle={} volume={:.3f}", playHandle, config.volume));
	instance_->SetVolumeInternal(playHandle, config.volume);
}

void SoundManager::SetPitch(uint32_t playHandle, float pitch) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPitch] playHandle={} pitch={:.3f}", playHandle, pitch));
	instance_->SetPitchInternal(playHandle, pitch);
}

void SoundManager::SetPitch(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPitch] playHandle={} pitch={:.3f}", playHandle, config.pitch));
	instance_->SetPitchInternal(playHandle, config.pitch);
}

void SoundManager::SetPan(uint32_t playHandle, float pan) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPan] playHandle={} pan={:.3f}", playHandle, pan));
	instance_->SetPanInternal(playHandle, pan);
}

void SoundManager::SetPan(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPan] playHandle={} pan={:.3f}", playHandle, config.pan));
	instance_->SetPanInternal(playHandle, config.pan);
}

void SoundManager::SetReverb(uint32_t playHandle, float mix) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetReverb] playHandle={} mix={:.3f}", playHandle, mix));
	instance_->SetReverbInternal(playHandle, mix);
}

void SoundManager::SetReverb(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetReverb] playHandle={} reverbMix={:.3f}", playHandle, config.reverbMix));
	instance_->SetReverbInternal(playHandle, config.reverbMix);
}

void SoundManager::SetLowPassFilter(uint32_t playHandle, float cutoff) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	instance_->SetLowPassFilterInternal(playHandle, cutoff);
}

void SoundManager::SetLowPassFilter(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter] playHandle={} lpfCutoff={:.3f}", playHandle, config.lpfCutoff));
	instance_->SetLowPassFilterInternal(playHandle, config.lpfCutoff);
}

void SoundManager::SetHighPassFilter(uint32_t playHandle, float cutoff) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	instance_->SetHighPassFilterInternal(playHandle, cutoff);
}

void SoundManager::SetHighPassFilter(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter] playHandle={} hpfCutoff={:.3f}", playHandle, config.hpfCutoff));
	instance_->SetHighPassFilterInternal(playHandle, config.hpfCutoff);
}

void SoundManager::FadeOut(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeOut] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	instance_->FadeOutInternal(playHandle, duration, targetVolume, easeType);
}

void SoundManager::FadeOut(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeOut] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	instance_->FadeOutInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

void SoundManager::FadeIn(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeIn] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	instance_->FadeInInternal(playHandle, duration, targetVolume, easeType);
}

void SoundManager::FadeIn(uint32_t playHandle, const SoundConfig& config) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeIn] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	instance_->FadeInInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

//=============================================================================
// 止めて破棄
//=============================================================================
void SoundManager::Stop(uint32_t playHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Warning(std::format("playHandle={} はvoices_に存在しません", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Warning(std::format("playHandle={} のvoiceがnullです", playHandle));
		return;
	}
	voice->Stop();
	voice->DestroyVoice();
	instance_->voices_.erase(it);
	LogManager::Log(std::format("playHandle={} を停止・破棄した", playHandle));
}

//=============================================================================
// 一時停止
//=============================================================================
void SoundManager::Pause(uint32_t playHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Warning(std::format("playHandle={} はvoices_に存在しません", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Warning(std::format("playHandle={} のvoiceがnullです", playHandle));
		return;
	}
	voice->Stop();
	LogManager::Log(std::format("playHandle={} を一時停止した", playHandle));
}

//=============================================================================
// 一時停止を再開
//=============================================================================
void SoundManager::Resume(uint32_t playHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Warning(std::format("playHandle={} はvoices_に存在しなません", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Warning(std::format("playHandle={} のvoiceがnullです", playHandle));
		return;
	}
	voice->Start();
	LogManager::Log(std::format("playHandle={} を再開した", playHandle));
}

//=============================================================================
// 一時停止
//=============================================================================
void SoundManager::RemoveReverb(uint32_t playHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Warning(std::format("playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Warning(std::format("playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetEffectChain(nullptr);
	LogManager::Log(std::format("playHandle={} リバーブを解除した", playHandle));
}

//=============================================================================
// 再生終了した音を破棄
//=============================================================================
void SoundManager::CleanupSourceVoices(float deltaTime) {
	// 毎フレーム処理のためログなし
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	// 一時停止中はフェードも止める（進めると、再開したときに音量が飛ぶ）
	if (!instance_->paused_) {
		instance_->UpdateFadeInternal(deltaTime);
	}

	for (auto it = instance_->voices_.begin(); it != instance_->voices_.end();) {
		uint32_t playHandle = it->first;
		IXAudio2SourceVoice* voice = it->second;
		// フェード中のボイスは掃除の対象から外す
		if (instance_->fadeInfos_.find(playHandle) != instance_->fadeInfos_.end()) {
			++it;
			continue;
		}
		XAUDIO2_VOICE_STATE state;
		voice->GetState(&state);
		// バッファが空（再生終了）になったら破棄
		if (state.BuffersQueued == 0) {
			voice->Stop();
			voice->DestroyVoice();
			it = instance_->voices_.erase(it);
		} else {
			++it;
		}
	}
}

//=============================================================================
// 全部止める
//=============================================================================
void SoundManager::StopAll() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("鳴っている音を全部止める 件数={}", instance_->voices_.size()));
	for (auto& [playHandle, voice] : instance_->voices_) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
		}
	}
	instance_->voices_.clear();
	instance_->fadeInfos_.clear();
}

//=============================================================================
// 全部を一時停止 / 再開
//=============================================================================
void SoundManager::SetPaused(bool paused) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	if (instance_->paused_ == paused) {
		return; // 同じ状態なら何もしない
	}
	instance_->paused_ = paused;
	// XAudio2 の Stop は「今の位置で止める」だけで、Start でそこから続きが鳴る（頭に戻るのは DestroyVoice して作り直したとき）
	for (auto& [playHandle, voice] : instance_->voices_) {
		if (!voice) {
			continue;
		}
		if (paused) {
			voice->Stop();
		} else {
			voice->Start();
		}
	}
	LogManager::Log(std::format("{} 件数={}", paused ? "全部の音を一時停止した" : "全部の音を再開した", instance_->voices_.size()));
}

bool SoundManager::IsPaused() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	return instance_->paused_;
}

//=============================================================================
// まだ鳴っているか
//=============================================================================
bool SoundManager::IsPlaying(uint32_t playHandle) {
	// 毎フレーム呼ばれるのでログなし
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	// 鳴り終わった音は CleanupSourceVoices が一覧から消すので、一覧にあればまだある
	return playHandle != 0 && instance_->voices_.contains(playHandle);
}

//=============================================================================
// 素材ハンドル → パス
//=============================================================================
const std::string& SoundManager::GetSoundPath(uint32_t soundHandle) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	static const std::string kEmpty; // 見つからないときに返す空文字（参照で返すので static にする）
	// 「パス → 番号」の表を逆から引く（読み込んだ音の数しか無いので、順に見れば足りる）
	const auto it = std::ranges::find_if(instance_->pathID_, [soundHandle](const auto& entry) { return entry.second == soundHandle; });
	return (it != instance_->pathID_.end()) ? it->first : kEmpty;
}

//=============================================================================
// 3Dの音（距離の大きさと左右）
//=============================================================================
void SoundManager::SetSpatial(uint32_t playHandle, float gain, float pan) {
	// 毎フレーム呼ばれるのでログなし。鳴り終わった音には何もしない
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	const auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end() || it->second == nullptr) {
		return;
	}
	ApplyOutputMatrix(it->second, gain, pan);
}

//=============================================================================
// 音の種類ごとの音量
//=============================================================================
void SoundManager::SetBusVolume(AudioBus bus, float volume) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("{} の音量を{:.3f}にした", magic_enum::enum_name(bus), volume));
	instance_->busVoices_[static_cast<size_t>(bus)]->SetVolume(volume);
}

float SoundManager::GetBusVolume(AudioBus bus) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	float volume = 0.0f;
	instance_->busVoices_[static_cast<size_t>(bus)]->GetVolume(&volume);
	return volume;
}

void SoundManager::SetMasterVolume(float volume) {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	LogManager::Log(std::format("全体の音量を{:.3f}にした", volume));
	instance_->masterVoice_->SetVolume(volume);
}

float SoundManager::GetMasterVolume() {
	MY_ASSERT_MSG(instance_, "Initialized()を先に呼んでください");
	float volume = 0.0f;
	instance_->masterVoice_->GetVolume(&volume);
	return volume;
}


//=============================================================================
// 初期化
//=============================================================================
void SoundManager::InitInternal() {
	HRESULT hr;
	// Media Foundation初期化（MP3等のデコードに使う）
	hr = MFStartup(MF_VERSION);
	MY_ASSERT_MSG(SUCCEEDED(hr), "MFStartup失敗");
	// XAudio2エンジン作成
	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	MY_ASSERT_MSG(SUCCEEDED(hr), "XAudio2Create失敗");
	// マスターボイス作成（最終的な音声出力先）
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	MY_ASSERT_MSG(SUCCEEDED(hr), "CreateMasteringVoice失敗");
	// 音の種類ごとの通り道（左右2ch・スピーカーと同じサンプリングレート）。鳴らす音はどれか1つを通ってマスターへ行く
	XAUDIO2_VOICE_DETAILS masterDetails;
	masterVoice_->GetVoiceDetails(&masterDetails);
	for (IXAudio2SubmixVoice*& busVoice : busVoices_) {
		hr = xAudio2_->CreateSubmixVoice(&busVoice, kBusChannels, masterDetails.InputSampleRate);
		MY_ASSERT_MSG(SUCCEEDED(hr), "CreateSubmixVoice失敗");
	}
	LogManager::Log("Initialized");
}

//=============================================================================
// 解放
//=============================================================================
void SoundManager::ReleaseInternal() {
	// 再生中の全ボイスを停止・破棄
	LogManager::Log(std::format("再生中のvoiceを破棄 件数={}", voices_.size()));
	for (auto& [playHandle, voice] : voices_) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
			LogManager::Log(std::format("playHandle={} を停止・破棄した", playHandle));
		}
	}
	voices_.clear();
	fadeInfos_.clear();
	// 読み込み済みの全波形データを解放（SoundData の vector が、clear で一緒に消える）
	LogManager::Log(std::format("波形データを解放 件数={}", soundMap_.size()));
	soundMap_.clear();
	pathID_.clear();
	// 種類ごとの通り道を破棄（流れ込む音を全部消した後、流れ出る先のマスターより先に）
	for (IXAudio2SubmixVoice*& busVoice : busVoices_) {
		if (busVoice) {
			busVoice->DestroyVoice();
			busVoice = nullptr;
		}
	}
	// マスターボイスを破棄
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		LogManager::Log("MasteringVoice を破棄した");
	}
	xAudio2_.Reset();
	MFShutdown();
	LogManager::Log("Released");
}

//=============================================================================
// 読み込み
//=============================================================================
uint32_t SoundManager::LoadInternal(const std::filesystem::path& filePath) {
	const std::string filename = ToPathKey(filePath);
	LogManager::Log(std::format("ファイル読み込み要求 filename=\"{}\"", filename));
	// キャッシュチェック
	if (pathID_.find(filename) != pathID_.end()) {
		uint32_t cachedID = pathID_[filename];
		// 同じ音を何個ものAudioSourceが使うと毎回ここに来る。ふつうのことなので警告にはしない
		LogManager::Log(std::format("キャッシュ済み filename=\"{}\" -> soundHandle={}", filename, cachedID));
		return cachedID;
	}
	HRESULT hr;
	// SourceReaderでファイルを開く
	// Media Foundation の物は COM なので ComPtr で持つ。関数を抜けると（途中の return でも）自動で Release される
	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &reader);
	if (FAILED(hr)) {
		// 止めずに 0（未選択）を返す。Inspector で読めないファイルを選んでも、エディタが落ちないように
		// コードで Load して 0 が返った音を Play すると、Play の assert で止まるので気づける
		LogManager::Error(std::format("ファイルの読み込みに失敗した filename=\"{}\" hr=0x{:08X}", filename, static_cast<uint32_t>(hr)));
		return 0;
	}
	LogManager::Log(std::format("SourceReader 作成完了 filename=\"{}\"", filename));
	// 出力フォーマットをPCMに設定
	Microsoft::WRL::ComPtr<IMFMediaType> type;
	hr = MFCreateMediaType(&type);
	MY_ASSERT(SUCCEEDED(hr));
	type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, type.Get());
	MY_ASSERT(SUCCEEDED(hr));
	LogManager::Log("メディアタイプをPCMに設定完了");
	// 波形フォーマット取得
	Microsoft::WRL::ComPtr<IMFMediaType> outputType;
	hr = reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &outputType);
	MY_ASSERT(SUCCEEDED(hr));
	WAVEFORMATEX* rawWaveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	hr = MFCreateWaveFormatExFromMFMediaType(outputType.Get(), &rawWaveFormat, &waveFormatSize);
	MY_ASSERT(SUCCEEDED(hr));
	// 受け取ったらすぐ unique_ptr に渡す（CoTaskMemFree を書き忘れても、関数を抜けると返される）
	std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter> waveFormat(rawWaveFormat);
	LogManager::Log(std::format("波形フォーマット取得完了 channels={} sampleRate={} bitsPerSample={}",
					waveFormat->nChannels, waveFormat->nSamplesPerSec, waveFormat->wBitsPerSample));
	// PCMデータを全て読み出す
	std::vector<BYTE> audioData;
	while (true) {
		// ループの中で作った ComPtr は、1周終わるたびに Release される
		Microsoft::WRL::ComPtr<IMFSample> sample;
		DWORD dwFlags = 0;
		hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &dwFlags, nullptr, &sample);
		MY_ASSERT(SUCCEEDED(hr));
		if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
			break;
		if (!sample)
			continue;
		Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
		hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
		MY_ASSERT(SUCCEEDED(hr));
		BYTE* pAudioBytes = nullptr;
		DWORD cbBuffer = 0;
		hr = mediaBuffer->Lock(&pAudioBytes, nullptr, &cbBuffer);
		MY_ASSERT(SUCCEEDED(hr));
		audioData.insert(audioData.end(), pAudioBytes, pAudioBytes + cbBuffer);
		mediaBuffer->Unlock(); // Lock と対になる物なので、これは残す（Release ではない）
	}
	LogManager::Log(std::format("PCMデータ読み出し完了 totalBytes={}", audioData.size()));
	// SoundDataとして登録（読んだ vector をそのまま渡す。コピーも new も要らない）
	SoundData soundData;
	soundData.wfex = *waveFormat;
	soundData.buffer = std::move(audioData);
	const size_t bufferSize = soundData.buffer.size();
	uint32_t newID = static_cast<uint32_t>(soundMap_.size() + 1);
	pathID_[filename] = newID;
	soundMap_[newID] = std::move(soundData);
	LogManager::Log(std::format("登録完了 filename=\"{}\" -> soundHandle={} bufferSize={}", filename, newID, bufferSize));
	return newID;
}

//=============================================================================
// 再生
//=============================================================================
uint32_t SoundManager::PlayInternal(uint32_t soundHandle, bool loop, float volume, float pitch, AudioBus bus) {
	LogManager::Log(std::format("soundHandle={} loop={} volume={:.3f} pitch={:.3f} bus={}", soundHandle, loop, volume, pitch, magic_enum::enum_name(bus)));
	if (soundHandle == 0 || soundMap_.find(soundHandle) == soundMap_.end()) {
		LogManager::Error(std::format("未登録のsoundHandle={} が指定された", soundHandle));
		MY_ASSERT_MSG(false, "登録されていないハンドルが指定されました");
		return 0;
	}
	const SoundData& data = soundMap_[soundHandle];
	HRESULT hr;
	// SourceVoice作成。送り先は音の種類の通り道（マスターへ直接は送らない）
	XAUDIO2_SEND_DESCRIPTOR send = {0, busVoices_[static_cast<size_t>(bus)]};
	const XAUDIO2_VOICE_SENDS sends = {1, &send};
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &data.wfex, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends);
	if (FAILED(hr)) {
		LogManager::Error(std::format("SourceVoiceの作成に失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		MY_ASSERT_MSG(false, "SourceVoiceの作成に失敗しました");
		return 0;
	}
	pSourceVoice->SetVolume(volume);
	pSourceVoice->SetFrequencyRatio(pitch);
	// バッファをキューに積む
	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = data.buffer.data();
	buffer.AudioBytes = static_cast<UINT32>(data.buffer.size());
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	if (loop) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	hr = pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr)) {
		LogManager::Error(std::format("SubmitSourceBuffer失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		pSourceVoice->DestroyVoice();
		return 0;
	}
	// 一時停止中（SetPaused）は Start しない。バッファは積んであるので、再開したときに頭から鳴る
	if (!paused_) {
		hr = pSourceVoice->Start();
		if (FAILED(hr)) {
			LogManager::Error(std::format("Start失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
			pSourceVoice->DestroyVoice();
			return 0;
		}
	}
	// 再生ハンドルを発行して登録
	uint32_t playHandle = nextPlayHandle_++;
	if (nextPlayHandle_ == 0)
		nextPlayHandle_ = 1; // オーバーフロー対策
	voices_[playHandle] = pSourceVoice;
	LogManager::Log(std::format("再生開始 soundHandle={} -> playHandle={}", soundHandle, playHandle));
	return playHandle;
}

//=============================================================================
// 音量調整
//=============================================================================
void SoundManager::SetVolumeInternal(uint32_t playHandle, float volume) {
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のためスキップ");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Warning(std::format("playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Warning(std::format("playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetVolume(volume);
	LogManager::Log(std::format("playHandle={} の音量を{:.3f}に変更した", playHandle, volume));
}

//=============================================================================
// ピッチ調整
//=============================================================================
void SoundManager::SetPitchInternal(uint32_t playHandle, float pitch) {
	if (!playHandle) {
		LogManager::Warning("無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetFrequencyRatio(pitch);
	LogManager::Log(std::format("playHandle={} のピッチを{:.3f}に変更した", playHandle, pitch));
}

//=============================================================================
// パン調整
//=============================================================================
void SoundManager::SetPanInternal(uint32_t playHandle, float pan) {
	if (!playHandle) {
		LogManager::Log("無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("playHandle={} のvoiceがnull", playHandle));
		return;
	}
	// 左右への割り振り（3Dの音の SetSpatial と同じ関数。大きさは変えない）
	ApplyOutputMatrix(voice, 1.0f, pan);
	LogManager::Log(std::format("playHandle={} パンを{:.3f}に設定した", playHandle, pan));
}

//=============================================================================
// リバーブ調整
//=============================================================================
void SoundManager::SetReverbInternal(uint32_t playHandle, float mix) {
	LogManager::Log(std::format("[SoundManager::SetReverbInternal] playHandle={} mix={:.3f}", playHandle, mix));
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetReverbInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetReverbInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	LogManager::Log(std::format("[SoundManager::SetReverbInternal] InputChannels={}", details.InputChannels));
	// リバーブエフェクトを作成してエフェクトチェーンに設定する
	Microsoft::WRL::ComPtr<IUnknown> reverbEffect; // SetEffectChain の中で XAudio2 も持つので、こちらの分は関数を抜けると自動で Release
	HRESULT hr = XAudio2CreateReverb(&reverbEffect);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::SetReverbInternal] XAudio2CreateReverb失敗 hr=0x{:08X}", static_cast<uint32_t>(hr)));
		LogManager::Flush();
		return;
	}
	XAUDIO2_EFFECT_DESCRIPTOR descriptor{};
	descriptor.InitialState = TRUE;
	descriptor.OutputChannels = details.InputChannels;
	descriptor.pEffect = reverbEffect.Get();
	XAUDIO2_EFFECT_CHAIN chain{};
	chain.EffectCount = 1;
	chain.pEffectDescriptors = &descriptor;
	hr = voice->SetEffectChain(&chain);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::SetReverbInternal] SetEffectChain失敗 hr=0x{:08X}", static_cast<uint32_t>(hr)));
		return;
	}
	// リバーブパラメータを設定する
	XAUDIO2FX_REVERB_PARAMETERS params{};
	params.ReflectionsDelay = 50;
	params.ReverbDelay = 60;
	params.RearDelay = 10;
	params.PositionLeft = 6;
	params.PositionRight = 6;
	params.PositionMatrixLeft = 0;
	params.PositionMatrixRight = 0;
	params.EarlyDiffusion = 15;
	params.LateDiffusion = 15;
	params.LowEQGain = 8;
	params.LowEQCutoff = 4;
	params.HighEQGain = 12;
	params.HighEQCutoff = 16;
	params.RoomFilterFreq = 5000.0f;
	params.RoomFilterMain = -1000.0f;
	params.RoomFilterHF = -100.0f;
	params.ReflectionsGain = 0.0f;
	params.ReverbGain = 0.0f;
	params.DecayTime = 5.0f;
	params.Density = 60.0f;
	params.RoomSize = 80.0f;
	params.WetDryMix = mix * 100.0f; // 0.0f〜1.0fを0%〜100%に変換
	voice->SetEffectParameters(0, &params, sizeof(params));
	LogManager::Log(std::format("[SoundManager::SetReverbInternal] playHandle={} リバーブを適用した WetDryMix={:.1f}%", playHandle, params.WetDryMix));
}

//=============================================================================
// ローパスフィルタをかける
//=============================================================================
void SoundManager::SetLowPassFilterInternal(uint32_t playHandle, float cutoff) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetLowPassFilterInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetLowPassFilterInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::SetLowPassFilterInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	// cutoff値をXAudio2のフィルター周波数に変換する（0.0f〜1.0f → 0〜XAUDIO2_MAX_FILTER_FREQUENCY）
	XAUDIO2_FILTER_PARAMETERS filter{};
	filter.Type = LowPassFilter;
	filter.Frequency = cutoff * XAUDIO2_MAX_FILTER_FREQUENCY;
	filter.OneOverQ = 1.0f;
	voice->SetFilterParameters(&filter);
	LogManager::Log(std::format("[SoundManager::SetLowPassFilterInternal] playHandle={} ローパスフィルタをcutoff={:.3f}で設定した", playHandle, cutoff));
}

//=============================================================================
// ハイパスフィルタをかける
//=============================================================================
void SoundManager::SetHighPassFilterInternal(uint32_t playHandle, float cutoff) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetHighPassFilterInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetHighPassFilterInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::SetHighPassFilterInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	// cutoff値をXAudio2のフィルター周波数に変換する（ハイパスは反転）
	XAUDIO2_FILTER_PARAMETERS filter{};
	filter.Type = HighPassFilter;
	filter.Frequency = (1.0f - cutoff) * XAUDIO2_MAX_FILTER_FREQUENCY;
	filter.OneOverQ = 1.0f;
	voice->SetFilterParameters(&filter);
	LogManager::Log(std::format("[SoundManager::SetHighPassFilterInternal] playHandle={} ハイパスフィルタをcutoff={:.3f}で設定した", playHandle, cutoff));
}

//=============================================================================
// フェードアウト
//=============================================================================
void SoundManager::FadeOutInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::FadeOutInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::FadeOutInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::FadeOutInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	// 現在の音量からフェード開始する
	float currentVolume = 0.0f;
	voice->GetVolume(&currentVolume);
	FadeInfo fadeInfo{};
	fadeInfo.startVolume = currentVolume;
	fadeInfo.targetVolume = targetVolume;
	fadeInfo.timer = 0.0f;
	fadeInfo.duration = duration;
	fadeInfo.easeType = easeType;
	fadeInfos_[playHandle] = fadeInfo;
	LogManager::Log(
	    std::format("[SoundManager::FadeOutInternal] playHandle={} フェードアウト開始 startVolume={:.3f} -> targetVolume={:.3f} duration={:.3f}s", playHandle, currentVolume, targetVolume, duration));
}

//=============================================================================
// フェードイン
//=============================================================================
void SoundManager::FadeInInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::FadeInInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::FadeInInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	// 音量0から開始してtargetVolumeまでフェードインする
	voice->SetVolume(0.0f);
	FadeInfo fadeInfo{};
	fadeInfo.startVolume = 0.0f;
	fadeInfo.targetVolume = targetVolume;
	fadeInfo.timer = 0.0f;
	fadeInfo.duration = duration;
	fadeInfo.easeType = easeType;
	fadeInfos_[playHandle] = fadeInfo;
	LogManager::Log(std::format("[SoundManager::FadeInInternal] playHandle={} フェードイン開始 0.0 -> targetVolume={:.3f} duration={:.3f}s", playHandle, targetVolume, duration));
}

//=============================================================================
// フェードの更新
//=============================================================================
void SoundManager::UpdateFadeInternal(float deltaTime) {
	// 毎フレーム処理のためログなし
	for (auto it = fadeInfos_.begin(); it != fadeInfos_.end();) {
		uint32_t playHandle = it->first;
		FadeInfo& fadeInfo = it->second;
		// voiceが既に破棄されていたらフェード情報も削除する
		if (voices_.find(playHandle) == voices_.end()) {
			it = fadeInfos_.erase(it);
			continue;
		}
		IXAudio2SourceVoice* voice = voices_[playHandle];
		// 経過時間を進めてイージングを適用した音量を計算する
		fadeInfo.timer += deltaTime;
		float progress = std::min(fadeInfo.timer / fadeInfo.duration, 1.0f);
		float easedProgress = Ease::Apply(progress, fadeInfo.easeType);
		float currentVolume = fadeInfo.startVolume + (fadeInfo.targetVolume - fadeInfo.startVolume) * easedProgress;
		voice->SetVolume(currentVolume);
		if (progress >= 1.0f) {
			// フェード完了。targetVolumeが0以下なら停止する
			if (fadeInfo.targetVolume <= 0.0f) {
				voice->Stop();
			}
			it = fadeInfos_.erase(it);
		} else {
			++it;
		}
	}
}