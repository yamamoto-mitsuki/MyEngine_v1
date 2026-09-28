#include "AudioSystem.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Sound/SoundManager.h"

namespace {
constexpr float kMinPitch = 0.5f; // ピッチの下限（Inspector のスライダーと同じ）
constexpr float kMaxPitch = 2.0f; // ピッチの上限（XAudio2 は、音を作るときの上限＝既定の2倍までしか上げられない）

/// <summary>
/// AudioSource 1つ分の、実行中だけの状態（保存しない。Play / Stop で作り直すと消える）
/// </summary>
struct SourceState {
	Handle<Entity> entity;    // 持ち主
	uint32_t playHandle = 0;  // 鳴っている音（SoundManager の再生ハンドル。0は鳴っていない）
	uint32_t soundHandle = 0; // 鳴らした音の素材（Inspector で音を変えたら止める）
	float volume = 1.0f;      // SoundManager に最後に伝えた音量（変わったときだけ伝える。SoundManager は呼ぶたびにログを出すので）
	float pitch = 1.0f;       // 最後に伝えたピッチ
};

/// <summary>
/// AudioSystem が持つ物（Initialize で作り、Release で消す）
/// </summary>
struct AudioState {
	// 見つけた AudioSource の一覧。鳴っていなくても持つ（「もう始まった」の印。無いと Play On Start が毎フレーム鳴り直す）
	std::vector<SourceState> sources;
};
std::unique_ptr<AudioState> state;

// 範囲に収めた音量・ピッチ（ファイルを手で書き換えて範囲の外になっても、XAudio2 に変な値を渡さない）
float ClampVolume(float volume) { return std::clamp(volume, 0.0f, 1.0f); }
float ClampPitch(float pitch) { return std::clamp(pitch, kMinPitch, kMaxPitch); }

// Entityの状態を探す（無ければ nullptr）。AudioSource の数しか無いので、順に見れば足りる
SourceState* FindSource(Handle<Entity> entity) {
	const auto it = std::ranges::find(state->sources, entity, &SourceState::entity);
	return (it != state->sources.end()) ? &*it : nullptr;
}

// Entityの状態を探し、無ければ足す（ゲームの Play が、そのEntityを Update が見つけるより先だったとき）
SourceState& FindOrAddSource(Handle<Entity> entity) {
	if (SourceState* source = FindSource(entity)) {
		return *source;
	}
	state->sources.push_back({entity});
	return state->sources.back();
}

// 鳴っていれば止める（鳴り終わった・StopAll で止まった音には何もしない＝SoundManager に警告を出させない）
void StopSource(SourceState& source) {
	if (SoundManager::IsPlaying(source.playHandle)) {
		SoundManager::Stop(source.playHandle);
	}
	source.playHandle = 0;
}

// 頭から鳴らす（鳴っていたら止めてから）。音が未選択なら鳴らさない
bool StartSource(SourceState& source, const AudioSourceComponent& component) {
	StopSource(source);
	if (component.soundHandle == 0) {
		return false;
	}
	source.soundHandle = component.soundHandle;
	source.volume = ClampVolume(component.volume);
	source.pitch = ClampPitch(component.pitch);
	source.playHandle = SoundManager::Play(source.soundHandle, component.loop, source.volume, source.pitch);
	return source.playHandle != 0;
}

// 鳴っている音に、Inspector などで変えた値を伝える
void ApplyChanges(SourceState& source, const AudioSourceComponent& component) {
	// 鳴り終わった（ループしない音が最後まで鳴った。SoundManager::CleanupSourceVoices が片付けた）
	if (!SoundManager::IsPlaying(source.playHandle)) {
		source.playHandle = 0;
		return;
	}
	// 音そのものを変えた → 止める（前の音のまま鳴り続けないように）
	if (component.soundHandle != source.soundHandle) {
		StopSource(source);
		return;
	}
	// 音量・ピッチ（変わったときだけ伝える）
	const float volume = ClampVolume(component.volume);
	if (volume != source.volume) {
		source.volume = volume;
		SoundManager::SetVolume(source.playHandle, volume);
	}
	const float pitch = ClampPitch(component.pitch);
	if (pitch != source.pitch) {
		source.pitch = pitch;
		SoundManager::SetPitch(source.playHandle, pitch);
	}
}

// ===== Inspector と 保存 =====

// AudioSource の見せ方。Inspector とシーンファイルの両方がこの並びを使う（Collider と同じ書き方）
// 保存の名前はメンバ名にし、Inspector には「表示###保存の名前」で読みやすい名前を出す
void DescribeAudioSource(ComponentUI& ui, AudioSourceComponent& value) {
	ui.AssetField("Sound###sound", value.soundHandle, AssetType::Sound, Tip("resources の下の .wav / .mp3"));
	ui.Field("Volume###volume", value.volume, Range(0.0f, 2.0f));
	ui.Field("Pitch###pitch", value.pitch, Range(kMinPitch, kMaxPitch).Tip("0.5で1オクターブ下、2で1オクターブ上"));
	ui.Field("Loop###loop", value.loop, Tip("最後まで鳴ったら頭から（BGM）。鳴っている途中で変えたときは、次に鳴らしたときから効く"));
	ui.Field("Play On Start###playOnStart", value.playOnStart, Tip("Play を押したとき（シーンが始まったとき）に鳴らす"));
}
} // namespace

//=============================================================================
// 初期化 / 解放
//=============================================================================
void AudioSystem::Initialize() {
	MY_ASSERT_MSG(state == nullptr, "Initialize()が2回以上呼ばれています");
	state = std::make_unique<AudioState>();

	// AudioSource の置き場所・保存・Inspector を登録する。見せ方は DescribeAudioSource 1つで、保存と Inspector の両方に使う
	EntityManager::RegisterComponent<AudioSourceComponent>();
	ComponentSerializer::Register<AudioSourceComponent>("Audio Source", &DescribeAudioSource);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<AudioSourceComponent>>("Audio Source", "Audio", &DescribeAudioSource));
}

// SoundManager はウィンドウと一緒に先に片付いているので、ここでは SoundManager を呼ばない
void AudioSystem::Release() { state.reset(); }

//=============================================================================
// 毎フレーム
//=============================================================================
void AudioSystem::Update(bool isGameRunning) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	std::vector<SourceState>& sources = state->sources;

	// ===== 停止中は鳴らさない =====
	if (!isGameRunning) {
		for (SourceState& source : sources) {
			StopSource(source);
		}
		sources.clear(); // 次に Play したときは、始まりからやり直す
		return;
	}

	// ===== 1. 消えた・外した・無効になった AudioSource の音を止めて、一覧から外す =====
	const auto isGone = [](const SourceState& source) { return EntityManager::Get<AudioSourceComponent>(source.entity) == nullptr || !EntityManager::IsActiveInHierarchy(source.entity); };
	for (SourceState& source : sources) {
		if (isGone(source)) {
			StopSource(source);
		}
	}
	std::erase_if(sources, isGone);

	// ===== 2. 初めて見つけた AudioSource は一覧に足して、Play On Start なら鳴らす。見つけ済みの物は変更を伝える =====
	EntityManager::ForEach<AudioSourceComponent>([&sources](Handle<Entity> entity, const AudioSourceComponent& component) {
		if (!EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		if (SourceState* source = FindSource(entity)) {
			ApplyChanges(*source, component);
			return;
		}
		sources.push_back({entity});
		if (component.playOnStart) {
			StartSource(sources.back(), component);
		}
	});
}

//=============================================================================
// ゲームから
//=============================================================================
bool AudioSystem::Play(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const AudioSourceComponent* component = EntityManager::Get<AudioSourceComponent>(entity);
	// AudioSource が無い（RequestAdd した直後は、次のフレームまで付いていない）・無効なEntity
	if (component == nullptr || !EntityManager::IsActiveInHierarchy(entity)) {
		return false;
	}
	// 一覧に入れるので、この後の Update が Play On Start でもう一度鳴らすことはない
	return StartSource(FindOrAddSource(entity), *component);
}

void AudioSystem::Stop(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	if (SourceState* source = FindSource(entity)) {
		StopSource(*source);
	}
}

bool AudioSystem::IsPlaying(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const SourceState* source = FindSource(entity);
	return source != nullptr && SoundManager::IsPlaying(source->playHandle);
}

bool AudioSystem::PlayOneShot(Handle<Entity> entity) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	const AudioSourceComponent* component = EntityManager::Get<AudioSourceComponent>(entity);
	if (component == nullptr || component->soundHandle == 0) {
		return false;
	}
	// 一覧には入れない（持ち主が消えても止めない）。ループはしない（止める手段が StopAll しか無いので）
	return SoundManager::Play(component->soundHandle, false, ClampVolume(component->volume), ClampPitch(component->pitch)) != 0;
}

//=============================================================================
// エディタの再生コントロールから
//=============================================================================
void AudioSystem::StopAll() {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::StopAll();
	state->sources.clear(); // 作り直した後に見つけた AudioSource は「始まったばかり」になる（Play On Start がまた効く）
}

void AudioSystem::SetPaused(bool paused) {
	MY_ASSERT_MSG(state != nullptr, "AudioSystem::Initialize()を先に呼んでください");
	SoundManager::SetPaused(paused);
}