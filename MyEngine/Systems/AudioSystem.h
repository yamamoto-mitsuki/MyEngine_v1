#pragma once
#include "MyEngine/Components/AudioSourceComponent.h"
#include "MyEngine/Core/Handle.h"
#include "MyEngine/ECS/Entity.h"


/// <summary>
/// AudioSourceComponent を持つEntityの音を、鳴らす・止める
/// <para>鳴っている音の番号（SoundManager の再生ハンドル）はここが持つ。Componentはデータだけ</para>
/// <para>エディタの停止中は鳴らさない。Entityを消す・無効にする・AudioSource を外すと、その音は止まる</para>
/// <para>一覧などの中身は AudioSystem.cpp の中だけに置く（このヘッダはゲームのComponentのファイルがみんな読むので、軽くしておく）</para>
/// </summary>
class AudioSystem {
public:
	// AudioSourceComponent を登録する（置き場所・保存・Inspector）。EntityManager::Initialize の後に呼ぶ
	static void Initialize();
	static void Release();

	/// <summary>
	/// 毎フレーム1回、EntityManager::FlushDestroy の後に呼ぶ
	/// <para>消えた・外した・無効になった AudioSource の音を止め、始まったばかりの AudioSource を鳴らし（Play On Start）、
	/// Inspector で変えた音量・ピッチを鳴っている音に伝える</para>
	/// </summary>
	/// <param name="isGameRunning">ゲームが動いているか（再生中・一時停止中は true、エディタの停止中は false）</param>
	static void Update(bool isGameRunning);

	// ===== ゲームから（SYSTEM の中で）使う =====

	// そのEntityの AudioSource を頭から鳴らす（鳴っていたら止めてから）。AudioSource が無い・音が未選択なら false
	static bool Play(Handle<Entity> entity);
	// そのEntityの音を止める（鳴っていなければ何もしない）
	static void Stop(Handle<Entity> entity);
	// そのEntityの音が鳴っているか（一時停止中も true。鳴り終わったら false）
	static bool IsPlaying(Handle<Entity> entity);
	// 重ねて鳴らす（効果音）。Entityを消しても最後まで鳴る。AudioSystem::Stop では止まらない（エディタの Stop では止まる）
	static bool PlayOneShot(Handle<Entity> entity);

	// ===== エディタの再生コントロールから使う（SceneManager が呼ぶ）=====

	// 全部の音を止める（AudioSource の音も、SoundManager で直接鳴らした音も）。シーンを作り直す前に呼ぶ
	static void StopAll();
	// 全部の音を一時停止する / 再開する
	static void SetPaused(bool paused);
};