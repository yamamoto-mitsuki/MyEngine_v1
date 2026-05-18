#define NOMINMAX
#include "MyEngine/Audio/SoundManager.h"
#include "MyEngine/Utils/Easing.h"
#include "MyEngine/Log/LogManager.h"

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

//=============================
// static窓口側
//=============================
SoundManager* SoundManager::GetInstance() {
	static SoundManager instance;
	return &instance;
}

void SoundManager::Init() { GetInstance()->initInternal(); }
void SoundManager::Finalize() { GetInstance()->finalizeInternal(); }

uint32_t SoundManager::Load(const std::string& filename) { return GetInstance()->loadInternal(filename); }

// --- Play ---
uint32_t SoundManager::Play(uint32_t soundHandle, bool loop, float volume, float pitch) { return GetInstance()->playInternal(soundHandle, loop, volume, pitch); }
uint32_t SoundManager::Play(uint32_t soundHandle, const SoundConfig& config) { return GetInstance()->playInternal(soundHandle, config.loop, config.volume, config.pitch); }

// --- Stop / Pause / Resume ---
void SoundManager::Stop(uint32_t playHandle) {
	LogManager::Log(std::format("[SoundManager::Stop] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Stop] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = GetInstance()->voices_.find(playHandle);
	if (it == GetInstance()->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Stop] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Stop] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->Stop();
	voice->DestroyVoice();
	GetInstance()->voices_.erase(it);
	LogManager::Log(std::format("[SoundManager::Stop] playHandle={} を停止・破棄した", playHandle));
}

void SoundManager::Pause(uint32_t playHandle) {
	LogManager::Log(std::format("[SoundManager::Pause] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Pause] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = GetInstance()->voices_.find(playHandle);
	if (it == GetInstance()->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Pause] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Pause] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->Stop();
	LogManager::Log(std::format("[SoundManager::Pause] playHandle={} を一時停止した", playHandle));
}

void SoundManager::Resume(uint32_t playHandle) {
	LogManager::Log(std::format("[SoundManager::Resume] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::Resume] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = GetInstance()->voices_.find(playHandle);
	if (it == GetInstance()->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::Resume] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::Resume] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->Start();
	LogManager::Log(std::format("[SoundManager::Resume] playHandle={} を再開した", playHandle));
}

// --- SetVolume ---
void SoundManager::SetVolume(uint32_t playHandle, float volume) {
	LogManager::Log(std::format("[SoundManager::SetVolume] playHandle={} volume={:.3f}", playHandle, volume));
	GetInstance()->setVolumeInternal(playHandle, volume);
}
void SoundManager::SetVolume(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetVolume(config)] playHandle={} volume={:.3f}", playHandle, config.volume));
	GetInstance()->setVolumeInternal(playHandle, config.volume);
}

// --- SetPitch ---
void SoundManager::SetPitch(uint32_t playHandle, float pitch) {
	LogManager::Log(std::format("[SoundManager::SetPitch] playHandle={} pitch={:.3f}", playHandle, pitch));
	GetInstance()->setPitchInternal(playHandle, pitch);
}
void SoundManager::SetPitch(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetPitch(config)] playHandle={} pitch={:.3f}", playHandle, config.pitch));
	GetInstance()->setPitchInternal(playHandle, config.pitch);
}

// --- SetPan ---
void SoundManager::SetPan(uint32_t playHandle, float pan) {
	LogManager::Log(std::format("[SoundManager::SetPan] playHandle={} pan={:.3f}", playHandle, pan));
	GetInstance()->setPanInternal(playHandle, pan);
}
void SoundManager::SetPan(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetPan(config)] playHandle={} pan={:.3f}", playHandle, config.pan));
	GetInstance()->setPanInternal(playHandle, config.pan);
}

// --- SetReverb ---
void SoundManager::SetReverb(uint32_t playHandle, float mix) {
	LogManager::Log(std::format("[SoundManager::SetReverb] playHandle={} mix={:.3f}", playHandle, mix));
	GetInstance()->setReverbInternal(playHandle, mix);
}
void SoundManager::SetReverb(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetReverb(config)] playHandle={} reverbMix={:.3f}", playHandle, config.reverbMix));
	GetInstance()->setReverbInternal(playHandle, config.reverbMix);
}

// --- SetLowPassFilter ---
void SoundManager::SetLowPassFilter(uint32_t playHandle, float cutoff) {
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	GetInstance()->setLowPassFilterInternal(playHandle, cutoff);
}
void SoundManager::SetLowPassFilter(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetLowPassFilter(config)] playHandle={} lpfCutoff={:.3f}", playHandle, config.lpfCutoff));
	GetInstance()->setLowPassFilterInternal(playHandle, config.lpfCutoff);
}

// --- SetHighPassFilter ---
void SoundManager::SetHighPassFilter(uint32_t playHandle, float cutoff) {
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter] playHandle={} cutoff={:.3f}", playHandle, cutoff));
	GetInstance()->setHighPassFilterInternal(playHandle, cutoff);
}
void SoundManager::SetHighPassFilter(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::SetHighPassFilter(config)] playHandle={} hpfCutoff={:.3f}", playHandle, config.hpfCutoff));
	GetInstance()->setHighPassFilterInternal(playHandle, config.hpfCutoff);
}

// --- FadeOut ---
void SoundManager::FadeOut(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	LogManager::Log(std::format("[SoundManager::FadeOut] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	GetInstance()->fadeOutInternal(playHandle, duration, targetVolume, easeType);
}
void SoundManager::FadeOut(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::FadeOut(config)] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	GetInstance()->fadeOutInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

// --- FadeIn ---
void SoundManager::FadeIn(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	LogManager::Log(std::format("[SoundManager::FadeIn] playHandle={} duration={:.3f} targetVolume={:.3f}", playHandle, duration, targetVolume));
	GetInstance()->fadeInInternal(playHandle, duration, targetVolume, easeType);
}
void SoundManager::FadeIn(uint32_t playHandle, const SoundConfig& config) {
	LogManager::Log(std::format("[SoundManager::FadeIn(config)] playHandle={} fadeDuration={:.3f} fadeTargetVolume={:.3f}", playHandle, config.fadeDuration, config.fadeTargetVolume));
	GetInstance()->fadeInInternal(playHandle, config.fadeDuration, config.fadeTargetVolume, config.fadeEaseType);
}

// --- RemoveReverb ---
void SoundManager::RemoveReverb(uint32_t playHandle) {
	LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={}", playHandle));
	if (!playHandle) {
		LogManager::Log("[SoundManager::RemoveReverb] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = GetInstance()->voices_.find(playHandle);
	if (it == GetInstance()->voices_.end()) {
		LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->SetEffectChain(nullptr);
	LogManager::Log(std::format("[SoundManager::RemoveReverb] playHandle={} リバーブを解除した", playHandle));
}

void SoundManager::CleanupSourceVoices(float deltaTime) {
	// 毎フレーム処理のためログなし
	auto* inst = GetInstance();
	inst->updateFadeInternal(deltaTime);
	for (auto it = inst->voices_.begin(); it != inst->voices_.end();) {
		uint32_t playHandle = it->first;
		IXAudio2SourceVoice* voice = it->second;
		// フェード中のボイスは掃除の対象から外す
		if (inst->fadeInfos_.find(playHandle) != inst->fadeInfos_.end()) {
			++it;
			continue;
		}
		XAUDIO2_VOICE_STATE state;
		voice->GetState(&state);
		// バッファが空（再生終了）になったら破棄
		if (state.BuffersQueued == 0) {
			voice->Stop();
			voice->DestroyVoice();
			it = inst->voices_.erase(it);
		} else {
			++it;
		}
	}
}

//=============================
// 実処理側
//=============================

void SoundManager::initInternal() {
	LogManager::Log("[SoundManager::initInternal] 初期化開始");
	HRESULT hr;
	hr = MFStartup(MF_VERSION);
	assert(SUCCEEDED(hr));
	LogManager::Log("[SoundManager::initInternal] Media Foundation 初期化完了");
	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));
	LogManager::Log("[SoundManager::initInternal] XAudio2 作成完了");
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));
	LogManager::Log("[SoundManager::initInternal] MasteringVoice 作成完了。初期化終了");
}

void SoundManager::finalizeInternal() {
	LogManager::Log("[SoundManager::finalizeInternal] 終了処理開始");
	LogManager::Log(std::format("[SoundManager::finalizeInternal] 再生中のvoiceを破棄 件数={}", voices_.size()));
	for (auto& [playHandle, voice] : voices_) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
			LogManager::Log(std::format("[SoundManager::finalizeInternal] playHandle={} を停止・破棄した", playHandle));
		}
	}
	voices_.clear();
	fadeInfos_.clear();
	LogManager::Log(std::format("[SoundManager::finalizeInternal] 波形データを解放 件数={}", soundMap_.size()));
	for (auto& [soundHandle, data] : soundMap_) {
		if (data.pBuffer) {
			delete[] data.pBuffer;
			LogManager::Log(std::format("[SoundManager::finalizeInternal] soundHandle={} の波形データを解放した", soundHandle));
		}
	}
	soundMap_.clear();
	pathID_.clear();
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		LogManager::Log("[SoundManager::finalizeInternal] MasteringVoice を破棄した");
	}
	xAudio2_.Reset();
	MFShutdown();
	LogManager::Log("[SoundManager::finalizeInternal] XAudio2・Media Foundation を終了した。終了処理完了");
}

uint32_t SoundManager::loadInternal(const std::string& filename) {
	LogManager::Log(std::format("[SoundManager::loadInternal] ファイル読み込み要求 filename=\"{}\"", filename));
	if (pathID_.find(filename) != pathID_.end()) {
		uint32_t cachedID = pathID_[filename];
		LogManager::Log(std::format("[SoundManager::loadInternal] キャッシュ済み filename=\"{}\" -> soundHandle={}", filename, cachedID));
		return cachedID;
	}
	HRESULT hr;
	std::wstring wFilename(filename.begin(), filename.end());
	IMFSourceReader* pReader = nullptr;
	hr = MFCreateSourceReaderFromURL(wFilename.c_str(), nullptr, &pReader);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::loadInternal] ファイルの読み込みに失敗した filename=\"{}\" hr=0x{:08X}", filename, static_cast<uint32_t>(hr)));
		assert(false && "音声ファイルの読み込みに失敗しました。パスが正しいか確認してください。");
		return 0;
	}
	LogManager::Log(std::format("[SoundManager::loadInternal] SourceReader 作成完了 filename=\"{}\"", filename));
	IMFMediaType* pType = nullptr;
	hr = MFCreateMediaType(&pType);
	assert(SUCCEEDED(hr));
	pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pType);
	assert(SUCCEEDED(hr));
	pType->Release();
	LogManager::Log("[SoundManager::loadInternal] メディアタイプを PCM に設定完了");
	IMFMediaType* pOutputType = nullptr;
	hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pOutputType);
	assert(SUCCEEDED(hr));
	WAVEFORMATEX* pWaveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	hr = MFCreateWaveFormatExFromMFMediaType(pOutputType, &pWaveFormat, &waveFormatSize);
	assert(SUCCEEDED(hr));
	pOutputType->Release();
	LogManager::Log(
	    std::format(
	        "[SoundManager::loadInternal] 波形フォーマット取得完了 channels={} sampleRate={} bitsPerSample={}", pWaveFormat->nChannels, pWaveFormat->nSamplesPerSec, pWaveFormat->wBitsPerSample));
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
	LogManager::Log(std::format("[SoundManager::loadInternal] PCMデータ読み出し完了 totalBytes={}", audioData.size()));
	SoundData soundData = {};
	soundData.wfex = *pWaveFormat;
	soundData.bufferSize = static_cast<unsigned int>(audioData.size());
	soundData.pBuffer = new BYTE[soundData.bufferSize];
	memcpy(soundData.pBuffer, audioData.data(), soundData.bufferSize);
	CoTaskMemFree(pWaveFormat);
	uint32_t newID = static_cast<uint32_t>(soundMap_.size() + 1);
	pathID_[filename] = newID;
	soundMap_[newID] = soundData;
	LogManager::Log(std::format("[SoundManager::loadInternal] 登録完了 filename=\"{}\" -> soundHandle={} bufferSize={}", filename, newID, soundData.bufferSize));
	return newID;
}

uint32_t SoundManager::playInternal(uint32_t soundHandle, bool loop, float volume, float pitch) {
	LogManager::Log(std::format("[SoundManager::playInternal] soundHandle={} loop={} volume={:.3f} pitch={:.3f}", soundHandle, loop, volume, pitch));
	if (soundHandle == 0 || soundMap_.find(soundHandle) == soundMap_.end()) {
		LogManager::Log(std::format("[SoundManager::playInternal] 未登録の soundHandle={} が指定された", soundHandle));
		assert(false && "登録されていないハンドルが指定されました。");
		return 0;
	}
	const SoundData& data = soundMap_[soundHandle];
	HRESULT hr;
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &data.wfex);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::playInternal] SourceVoice の作成に失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		assert(false && "ソースボイスの作成に失敗しました。");
		return 0;
	}
	pSourceVoice->SetVolume(volume);
	pSourceVoice->SetFrequencyRatio(pitch);
	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = data.pBuffer;
	buffer.AudioBytes = data.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	if (loop) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	hr = pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::playInternal] SubmitSourceBuffer 失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		pSourceVoice->DestroyVoice();
		return 0;
	}
	hr = pSourceVoice->Start();
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::playInternal] Start 失敗 soundHandle={} hr=0x{:08X}", soundHandle, static_cast<uint32_t>(hr)));
		pSourceVoice->DestroyVoice();
		return 0;
	}
	uint32_t playHandle = nextPlayHandle_++;
	if (nextPlayHandle_ == 0)
		nextPlayHandle_ = 1; // オーバーフロー対策
	voices_[playHandle] = pSourceVoice;
	LogManager::Log(std::format("[SoundManager::playInternal] 再生開始 soundHandle={} -> playHandle={}", soundHandle, playHandle));
	return playHandle;
}

void SoundManager::setVolumeInternal(uint32_t playHandle, float volume) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::setVolumeInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setVolumeInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::setVolumeInternal] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->SetVolume(volume);
	LogManager::Log(std::format("[SoundManager::setVolumeInternal] playHandle={} の音量を {:.3f} に変更した", playHandle, volume));
}

void SoundManager::setPitchInternal(uint32_t playHandle, float pitch) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::setPitchInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setPitchInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::setPitchInternal] playHandle={} の voice が null", playHandle));
		return;
	}
	voice->SetFrequencyRatio(pitch);
	LogManager::Log(std::format("[SoundManager::setPitchInternal] playHandle={} のピッチを {:.3f} に変更した", playHandle, pitch));
}

void SoundManager::setPanInternal(uint32_t playHandle, float pan) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::setPanInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setPanInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::setPanInternal] playHandle={} の voice が null", playHandle));
		return;
	}
	float left = (1.0f - pan) / 2.0f;
	float right = (1.0f + pan) / 2.0f;
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	uint32_t inputChannels = details.InputChannels;
	uint32_t outputChannels = 2; // 出力は常にステレオとする
	std::vector<float> matrix(inputChannels * outputChannels);
	if (inputChannels == 1) { // モノラル
		matrix[0] = left;
		matrix[1] = right;
	} else { // チャンネル数2以上(ステレオ以上)
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
	LogManager::Log(std::format("[SoundManager::setPanInternal] playHandle={} パンを {:.3f} に設定した (left={:.3f} right={:.3f})", playHandle, pan, left, right));
}

void SoundManager::setReverbInternal(uint32_t playHandle, float mix) {
	LogManager::Log(std::format("[SoundManager::setReverbInternal] playHandle={} mix={:.3f}", playHandle, mix));
	if (!playHandle) {
		LogManager::Log("[SoundManager::setReverbInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setReverbInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	XAUDIO2_VOICE_DETAILS details;
	voice->GetVoiceDetails(&details);
	LogManager::Log(std::format("[SoundManager::setReverbInternal] InputChannels={}", details.InputChannels));
	IUnknown* pReverbEffect = nullptr;
	HRESULT hr = XAudio2CreateReverb(&pReverbEffect);
	if (FAILED(hr)) {
		LogManager::Log(std::format("[SoundManager::setReverbInternal] XAudio2CreateReverb 失敗 hr=0x{:08X}", static_cast<uint32_t>(hr)));
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
		LogManager::Log(std::format("[SoundManager::setReverbInternal] SetEffectChain 失敗 hr=0x{:08X}", static_cast<uint32_t>(hr)));
		return;
	}
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
	params.WetDryMix = mix * 100.0f;
	voice->SetEffectParameters(0, &params, sizeof(params));
	LogManager::Log(std::format("[SoundManager::setReverbInternal] playHandle={} リバーブを適用した WetDryMix={:.1f}%", playHandle, params.WetDryMix));
}

void SoundManager::setLowPassFilterInternal(uint32_t playHandle, float cutoff) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::setLowPassFilterInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setLowPassFilterInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::setLowPassFilterInternal] playHandle={} の voice が null", playHandle));
		return;
	}
	XAUDIO2_FILTER_PARAMETERS filter{};
	filter.Type = LowPassFilter;
	filter.Frequency = cutoff * XAUDIO2_MAX_FILTER_FREQUENCY;
	filter.OneOverQ = 1.0f;
	voice->SetFilterParameters(&filter);
	LogManager::Log(std::format("[SoundManager::setLowPassFilterInternal] playHandle={} ローパスフィルタを cutoff={:.3f} で設定した", playHandle, cutoff));
}

void SoundManager::setHighPassFilterInternal(uint32_t playHandle, float cutoff) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::setHighPassFilterInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::setHighPassFilterInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::setHighPassFilterInternal] playHandle={} の voice が null", playHandle));
		return;
	}
	XAUDIO2_FILTER_PARAMETERS filter{};
	filter.Type = HighPassFilter;
	filter.Frequency = (1.0f - cutoff) * XAUDIO2_MAX_FILTER_FREQUENCY;
	filter.OneOverQ = 1.0f;
	voice->SetFilterParameters(&filter);
	LogManager::Log(std::format("[SoundManager::setHighPassFilterInternal] playHandle={} ハイパスフィルタを cutoff={:.3f} で設定した", playHandle, cutoff));
}

void SoundManager::fadeOutInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::fadeOutInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::fadeOutInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	if (!voice) {
		LogManager::Log(std::format("[SoundManager::fadeOutInternal] playHandle={} の voice が null", playHandle));
		return;
	}
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
	    std::format("[SoundManager::fadeOutInternal] playHandle={} フェードアウト開始 startVolume={:.3f} -> targetVolume={:.3f} duration={:.3f}s", playHandle, currentVolume, targetVolume, duration));
}

void SoundManager::fadeInInternal(uint32_t playHandle, float duration, float targetVolume, Ease::Type easeType) {
	if (!playHandle) {
		LogManager::Log("[SoundManager::fadeInInternal] 無効なハンドル(0)のため終了");
		return;
	}
	auto it = voices_.find(playHandle);
	if (it == voices_.end()) {
		LogManager::Log(std::format("[SoundManager::fadeInInternal] playHandle={} は voices_ に存在しない", playHandle));
		return;
	}
	IXAudio2SourceVoice* voice = it->second;
	voice->SetVolume(0.0f);
	FadeInfo fadeInfo{};
	fadeInfo.startVolume = 0.0f;
	fadeInfo.targetVolume = targetVolume;
	fadeInfo.timer = 0.0f;
	fadeInfo.duration = duration;
	fadeInfo.easeType = easeType;
	fadeInfos_[playHandle] = fadeInfo;
	LogManager::Log(std::format("[SoundManager::fadeInInternal] playHandle={} フェードイン開始 0.0 -> targetVolume={:.3f} duration={:.3f}s", playHandle, targetVolume, duration));
}

void SoundManager::updateFadeInternal(float deltaTime) {
	// 毎フレーム処理のためログなし
	for (auto it = fadeInfos_.begin(); it != fadeInfos_.end();) {
		uint32_t playHandle = it->first;
		FadeInfo& fadeInfo = it->second;
		if (voices_.find(playHandle) == voices_.end()) {
			it = fadeInfos_.erase(it);
			continue;
		}
		IXAudio2SourceVoice* voice = voices_[playHandle];
		fadeInfo.timer += deltaTime;
		float progress = std::min(fadeInfo.timer / fadeInfo.duration, 1.0f);
		float easedProgress = Ease::Apply(progress, fadeInfo.easeType);
		float currentVolume = fadeInfo.startVolume + (fadeInfo.targetVolume - fadeInfo.startVolume) * easedProgress;
		voice->SetVolume(currentVolume);
		if (progress >= 1.0f) {
			if (fadeInfo.targetVolume <= 0.0f) {
				voice->Stop();
			}
			it = fadeInfos_.erase(it);
		} else {
			++it;
		}
	}
}