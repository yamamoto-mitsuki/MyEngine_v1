#pragma once
#include <cstdint>

// 音を鳴らすためのデータ。Entityに付けるComponent（カテゴリ：Audio）
// 鳴っている音の番号（SoundManager の再生ハンドル）は持たない。AudioSystem が持つ
// （ここに入れると、コピー・Undo で2つの AudioSource が同じ音を指してしまう）
// 位置はまだ使わない（離れると小さくなる・左右に振る、は後で。Audio.md「8. 未解決」）


/// <summary>
/// 音源
/// </summary>
struct AudioSourceComponent {
	uint32_t soundHandle = 0; // SoundManager::Load の番号（0は未選択）。保存はパスで
	float volume = 1.0f;      // 0=無音、1=元の大きさ
	float pitch = 1.0f;       // 0.5=1オクターブ下、1=元の高さ、2=1オクターブ上
	bool loop = false;        // 最後まで鳴ったら頭から鳴らし直す（BGM）
	bool playOnStart = true;  // シーンが始まったとき（Play を押したとき）に鳴らす
};