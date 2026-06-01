#define NOMINMAX
#include "MyEngine/Audio/SoundManager.h"
#include "MyEngine/Log/LogManager.h"
#include "MyEngine/Utils/Easing.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <xaudio2fx.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include <algorithm>
#include <cassert>
#include <format>
#include <fstream>
#include <vector>

// staticメンバの定義
SoundManager* SoundManager::instance_ = nullptr;

//=============================================================================
// NVIパターン
//=============================================================================
void SoundManager::Initialize() {
	assert(instance_ == nullptr && "[SoundManager::Initialize] Initialize()を2回呼んでいます");
	instance_ = new SoundManager();
	instance_->InitInternal();
}

void SoundManager::Release() {
	assert(instance_ != nullptr && "[SoundManager::Release] Init()より先にRelease()が呼ばれています");
	instance_->ReleaseInternal();
	delete instance_;
	instance_ = nullptr;
}

uint32_t SoundManager::Load(const std::string& filename) {
	assert(instance_ && "[SoundManager::Load] Init()を先に呼んでください");
	return instance_->LoadInternal(filename);
}

uint32_t SoundManager::Play(uint32_t soundHandle, bool loop, float volume, float pitch) {
	assert(instance_ && "[SoundManager::Play] Init()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, loop, volume, pitch);
}

uint32_t SoundManager::Play(uint32_t soundHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::Play] Init()を先に呼んでください");
	return instance_->PlayInternal(soundHandle, config.loop, config.volume, config.pitch);
}

void SoundManager::SetVolume(uint32_t playHandle, float volume) {
	assert(instance_ && "[SoundManager::SetVolume] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetVolume] playHandle={} volume={:.3f}", playHandle, volume));
	instance_->SetVolumeInternal(playHandle, volume);
}

void SoundManager::SetVolume(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetVolume] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetVolume] playHandle={} volume={:.3f}", playHandle, config.volume));
	instance_->SetVolumeInternal(playHandle, config.volume);
}

void SoundManager::SetPitch(uint32_t playHandle, float pitch) {
	assert(instance_ && "[SoundManager::SetPitch] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPitch] playHandle={} pitch={:.3f}", playHandle, pitch));
	instance_->SetPitchInternal(playHandle, pitch);
}

void SoundManager::SetPitch(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetPitch] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPitch] playHandle={} pitch={:.3f}", playHandle, config.pitch));
	instance_->SetPitchInternal(playHandle, config.pitch);
}

void SoundManager::SetPan(uint32_t playHandle, float pan) {
	assert(instance_ && "[SoundManager::SetPan] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPan] playHandle={} pan={:.3f}", playHandle, pan));
	instance_->SetPanInternal(playHandle, pan);
}

void SoundManager::SetPan(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetPan] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetPan] playHandle={} pan={:.3f}", playHandle, config.pan));
	instance_->SetPanInternal(playHandle, config.pan);
}

void SoundManager::SetReverb(uint32_t playHandle, float mix) {
	assert(instance_ && "[SoundManager::SetReverb] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetReverb] playHandle={} mix={:.3f}", playHandle, mix));
	instance_->SetReverbInternal(playHandle, mix);
}

void SoundManager::SetReverb(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetReverb] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetReverb] playHandle={} reverbMix={:.3f}", playHandle, config.reverbMix));
	instance_->SetReverbInternal(playHandle, config.reverbMix);
}

void SoundManager::SetLowPassFilter(uint32_t playHandle, float cutoff) {
	assert(instance_ && "[SoundManager::SetLowPassFilter] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	instance_->SetLowPassFilterInternal(playHandle, cutoff);
}

void SoundManager::SetLowPassFilter(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetLowPassFilter] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter] playHandle={} lpfCutoff={:.3f}", playHandle, config.lpfCutoff));
	instance_->SetLowPassFilterInternal(playHandle, config.lpfCutoff);
}

void SoundManager::SetHighPassFilter(uint32_t playHandle, float cutoff) {
	assert(instance_ && "[SoundManager::SetHighPassFilter] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	instance_->SetHighPassFilterInternal(playHandle, cutoff);
}

void SoundManager::SetHighPassFilter(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::SetHighPassFilter] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter] playHandle={} hpfCutoff={:.3f}", playHandle, config.hpfCutoff));
	instance_->SetHighPassFilterInternal(playHandle, config.hpfCutoff);
}

void SoundManager::FadeOut(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	assert(instance_ && "[SoundManager::FadeOut] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeOut] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	instance_->FadeOutInternal(playHandle, duration, targetVolume, easeType);
}

void SoundManager::FadeOut(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::FadeOut] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeOut] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	instance_->FadeOutInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

void SoundManager::FadeIn(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	assert(instance_ && "[SoundManager::FadeIn] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeIn] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	instance_->FadeInInternal(playHandle, duration, targetVolume, easeType);
}

void SoundManager::FadeIn(uint32_t playHandle, const SoundConfig& config) {
	assert(instance_ && "[SoundManager::FadeIn] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::FadeIn] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	instance_->FadeInInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

//=============================================================================
// 止めて破棄
//=============================================================================
void SoundManager::Stop(uint32_t playHandle) {
	assert(instance_ && "[SoundManager::Stop] Initialize()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::Stop] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Stop] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Stop] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Stop] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->Stop();
	voice->DestroyVoice();
	instance_->voices_.erase(it);
	LogManager::Log(std::format("[SoundManager::Stop] playHandle={} を停止・破棄した", playHandle));
}

//=============================================================================
// 一時停止
//=============================================================================
void SoundManager::Pause(uint32_t playHandle) {
	assert(instance_ && "[SoundManager::Pause] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::Pause] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Pause] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Pause] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Pause] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->Stop();
	LogManager::Log(std::format("[SoundManager::Pause] playHandle={} を一時停止した", playHandle));
}

//=============================================================================
// 一時停止を再開
//=============================================================================
void SoundManager::Resume(uint32_t playHandle) {
	assert(instance_ && "[SoundManager::Resume] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::Resume] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Resume] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Resume] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Resume] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->Start();
	LogManager::Log(std::format("[SoundManager::Resume] playHandle={} を再開した", playHandle));
}

//=============================================================================
// 一時停止
//=============================================================================
void SoundManager::RemoveReverb(uint32_t playHandle) {
	assert(instance_ && "[SoundManager::RemoveReverb] Init()を先に呼んでください");
	LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::RemoveReverb] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = instance_->voices_.find(playHandle);
	if (it == instance_->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetEffectChain(nullptr);
	LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} リバーブを解除した", playHandle));
}

//=============================================================================
// 再生終了した音を破棄
//=============================================================================
void SoundManager::CleanupSourceVoices(float deltaTime) {
	// 毎フレーム処理のためログなし
	assert(instance_ && "[SoundManager::CleanupSourceVoices] Init()を先に呼んでください");
	instance_->UpdateFadeInternal(deltaTime);
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
// 初期化
//=============================================================================
void SoundManager::InitInternal() {
	LogManager::Log("[SoundManager::InitInternal] 初期化開始");
	HRESULT hr;
	// Media Foundation初期化（MP3等のデコードに使う）
	hr = MFStartup(MF_VERSION);
	assert(SUCCEEDED(hr) && "[SoundManager::InitInternal] MFStartup失敗");
	LogManager::Log("[SoundManager::InitInternal] Media Foundation 初期化完了");
	// XAudio2エンジン作成
	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr) && "[SoundManager::InitInternal] XAudio2Create失敗");
	LogManager::Log("[SoundManager::InitInternal] XAudio2 作成完了");
	// マスターボイス作成（最終的な音声出力先）
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr) && "[SoundManager::InitInternal] CreateMasteringVoice失敗");
	LogManager::Log("[SoundManager::InitInternal] MasteringVoice 作成完了。初期化完了");
}

//=============================================================================
// 解放
//=============================================================================
void SoundManager::ReleaseInternal() {
	LogManager::Log("[SoundManager::releaseInternal] 終了処理開始");
	// 再生中の全ボイスを停止・破棄
	LogManager::Log(std::format("[SoundManager::releaseInternal] 再生中のvoiceを破棄 件数={}", voices_.size()));
	for (auto& [playHandle, voice] : voices_) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
			LogManager::Log(std::format("[SoundManager::releaseInternal] playHandle={} を停止・破棄した", playHandle));
		}
	}
	voices_.clear();
	fadeInfos_.clear();
	// 読み込み済みの全波形データを解放
	LogManager::Log(std::format("[SoundManager::releaseInternal] 波形データを解放 件数={}", soundMap_.size()));
	for (auto& [soundHandle, data] : soundMap_) {
		if (data.pBuffer) {
			delete[] data.pBuffer;
			LogManager::Log(std::format("[SoundManager::releaseInternal] soundHandle={} の波形データを解放した", soundHandle));
		}
	}
	soundMap_.clear();
	pathID_.clear();
	// マスターボイスを破棄
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		LogManager::Log("[SoundManager::releaseInternal] MasteringVoice を破棄した");
	}
	xAudio2_.Reset();
	MFShutdown();
	LogManager::Log("[SoundManager::releaseInternal] XAudio2・Media Foundation を終了した。終了処理完了");
}

//=============================================================================
// 読み込み
//=============================================================================
uint32_t SoundManager::LoadInternal(const std::string& filename) {
	LogManager::Log(std::format("[SoundManager::LoadInternal] ファイル読み込み要求 filename=\"{}\"", filename));
	// キャッシュチェック
	if (pathID_.find(filename) != pathID_.end()) {
		uint32_t cachedID = pathID_[filename];
		LogManager::Log(std::format("[SoundManager::LoadInternal] キャッシュ済み filename=\"{}\" -> soundHandle={}", filename, cachedID));
		return cachedID;
	}
	HRESULT hr;
	std::wstring wFilename(filename.begin(), filename.end());
	// SourceReaderでファイルを開く
	IMFSourceReader* pReader = nullptr;
	hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pReader);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::LoadInternal] ファイルの読み込みに失敗した filename=\"{}\" hr=0x{:08X}", filename, static_cast<uint32_t>(hr)));
		assert(false && "[SoundManager::LoadInternal] 音声ファイルの読み込みに失敗しました");
		return 0;
	}
	LogManager::Log(std::format("[SoundManager::LoadInternal] SourceReader 作成完了 filename=\"{}\"", filename));
	// 出力フォーマットをPCMに設定
	IMFMediaType* pType = nullptr;
	hr = MFCreateMediaType(&pType);
	assert(SUCCEEDED(hr));
	pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pType);
	assert(SUCCEEDED(hr));
	pType->Release();
	LogManager::Log("[SoundManager::LoadInternal] メディアタイプをPCMに設定完了");
	// 波形フォーマット取得
	IMFMediaType* pOutputType = nullptr;
	hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pOutputType);
	assert(SUCCEEDED(hr));
	WAVEFORMATEX* pWaveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	hr = MFCreateWaveFormatExFromMFMediaType(pOutputType, &pWaveFormat, &waveFormatSize);
	assert(SUCCEEDED(hr));
	pOutputType->Release();
	LogManager::Log(std::format("[SoundManager::LoadInternal] 波形フォーマット取得完了 channels={} sampleRate={} bitsPerSample={}", 
					pWaveFormat->nChannels, pWaveFormat->nSamplesPerSec, pWaveFormat->wBitsPerSample));
	// PCMデータを全て読み出す
	std::vector<BYTE> audioData;
	while (true) {
		IMFSample* pSample = nullptr;
		DWORD dwFlags = 0;
		hr = pReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &dwFlags, nullptr, &pSample);
		assert(SUCCEEDED(hr));
		if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
			break;
		if (!pSample)
			continue;
		IMFMediaBuffer* pBuffer = nullptr;
		hr = pSample->ConvertToContiguousBuffer(&pBuffer);
		assert(SUCCEEDED(hr));
		BYTE* pAudioBytes = nullptr;
		DWORD cbBuffer = 0;
		hr = pBuffer->Lock(&pAudioBytes, nullptr, &cbBuffer);
		assert(SUCCEEDED(hr));
		audioData.insert(audioData.end(), pAudioBytes, pAudioBytes + cbBuffer);
		pBuffer->Unlock();
		pBuffer->Release();
		pSample->Release();
	}
	pReader->Release();
	LogManager::Log(std::format("[SoundManager::LoadInternal] PCMデータ読み出し完了 totalBytes={}", audioData.size()));
	// SoundDataとして登録
	SoundData soundData = {};
	soundData.wfex = *pWaveFormat;
	soundData.bufferSize = static_cast<unsigned int>(audioData.size());
	soundData.pBuffer = new BYTE[soundData.bufferSize];
	memcpy(soundData.pBuffer, audioData.data(), soundData.bufferSize);
	CoTaskMemFree(pWaveFormat);
	uint32_t newID = static_cast<uint32_t>(soundMap_.size() + 1);
	pathID_[filename] = newID;
	soundMap_[newID] = soundData;
	LogManager::Log(std::format("[SoundManager::LoadInternal] 登録完了 filename=\"{}\" -> soundHandle={} bufferSize={}", filename, newID, soundData.bufferSize));
	return newID;
}

//=============================================================================
// 再生
//=============================================================================
uint32_t SoundManager::PlayInternal(uint32_t soundHandle, bool loop, float volume, float pitch) {
	LogManager::Log(std::format("[SoundManager::PlayInternal] soundHandle={} loop={} volume={:.3f} pitch={:.3f}", soundHandle, loop, volume, pitch));
	if (soundHandle == 0 || soundMap_.find(soundHandle) == soundMap_.end()) {
		LogManager::Log(std::format("[SoundManager::PlayInternal] 未登録のsoundHandle={} が指定された", soundHandle));
		assert(false && "[SoundManager::PlayInternal] 登録されていないハンドルが指定されました");
		return 0;
	}
	const SoundData& data = soundMap_[soundHandle];
	HRESULT hr;
	// SourceVoice作成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &data.wfex);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::PlayInternal] SourceVoiceの作成に失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		assert(false && "[SoundManager::PlayInternal] SourceVoiceの作成に失敗しました");
		return 0;
	}
	pSourceVoice->SetVolume(volume);
	pSourceVoice->SetFrequencyRatio(pitch);
	// バッファをキューに積む
	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = data.pBuffer;
	buffer.AudioBytes = data.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	if (loop) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	hr = pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::PlayInternal] SubmitSourceBuffer失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		pSourceVoice->DestroyVoice();
		return 0;
	}
	hr = pSourceVoice->Start();
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::PlayInternal] Start失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		pSourceVoice->DestroyVoice();
		return 0;
	}
	// 再生ハンドルを発行して登録
	uint32_t playHandle = nextPlayHandle_++;
	if (nextPlayHandle_ == 0)
		nextPlayHandle_ = 1; // オーバーフロー対策
	voices_[playHandle] = pSourceVoice;
	LogManager::Log(std::format("[SoundManager::PlayInternal] 再生開始 soundHandle={} -> playHandle={}", soundHandle, playHandle));
	return playHandle;
}

//=============================================================================
// 音量調整
//=============================================================================
void SoundManager::SetVolumeInternal(uint32_t playHandle, float volume) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetVolumeInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetVolumeInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::SetVolumeInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetVolume(volume);
	LogManager::Log(std::format("[SoundManager::SetVolumeInternal] playHandle={} の音量を{:.3f}に変更した", playHandle, volume));
}

//=============================================================================
// ピッチ調整
//=============================================================================
void SoundManager::SetPitchInternal(uint32_t playHandle, float pitch) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetPitchInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetPitchInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::SetPitchInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	voice->SetFrequencyRatio(pitch);
	LogManager::Log(std::format("[SoundManager::SetPitchInternal] playHandle={} のピッチを{:.3f}に変更した", playHandle, pitch));
}

//=============================================================================
// パン調整
//=============================================================================
void SoundManager::SetPanInternal(uint32_t playHandle, float pan) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::SetPanInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::SetPanInternal] playHandle={} はvoices_に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::SetPanInternal] playHandle={} のvoiceがnull", playHandle));
		return;
	}
	// pan値からL/Rの出力比率を計算する
	float left = (1.0f - pan) / 2.0f;
	float right = (1.0f + pan) / 2.0f;
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	uint32_t inputChannels = details.InputChannels;
	uint32_t outputChannels = 2; // 出力は常にステレオとする
	std::vector<float> matrix(inputChannels * outputChannels);
	if (inputChannels == 1) {
		// モノラル: 1chをL/Rに振り分ける
		matrix[0] = left;
		matrix[1] = right;
	} else {
		// ステレオ以上: 偶数chをL、奇数chをRに振り分ける
		for (uint32_t i = 0; i < inputChannels; ++i) {
			if (i % 2 == 0) {
				matrix[i * 2 + 0] = left;
				matrix[i * 2 + 1] = 0.0f;
			} else {
				matrix[i * 2 + 0] = 0.0f;
				matrix[i * 2 + 1] = right;
			}
		}
	}
	voice->SetOutputMatrix(nullptr, inputChannels, outputChannels, matrix.data());
	LogManager::Log(std::format("[SoundManager::SetPanInternal] playHandle={} パンを{:.3f}に設定した (left={:.3f} right={:.3f})", playHandle, pan, left, right));
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
	IUnknown* pReverbEffect = nullptr;
	HRESULT hr = XAudio2CreateReverb(&pReverbEffect);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::SetReverbInternal] XAudio2CreateReverb失敗 hr=0x{:08X}", static_cast<uint32_t>(hr)));
		return;
	}
	XAUDIO2_EFFECT_DESCRIPTOR descriptor{};
	descriptor.InitialState = TRUE;
	descriptor.OutputChannels = details.InputChannels;
	descriptor.pEffect = pReverbEffect;
	XAUDIO2_EFFECT_CHAIN chain{};
	chain.EffectCount = 1;
	chain.pEffectDescriptors = &descriptor;
	hr = voice->SetEffectChain(&chain);
	pReverbEffect->Release();
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