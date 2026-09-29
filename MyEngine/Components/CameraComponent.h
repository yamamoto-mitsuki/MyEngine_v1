#pragma once

// ゲームの絵を撮るカメラ。Entityに付けるComponent（カテゴリ：Rendering3D）
// 位置・向きは Transform（親の下に置けば一緒に動く）。ここに持つのは写り方だけ
// 実際のカメラ（Camera クラス）は CameraSystem が1つ持ち、毎フレーム一番 priority の高い Camera の値を写す

/// <summary>
/// カメラ
/// </summary>
struct CameraComponent {
	float fovY = 45.0f;   // 縦の視野角（度）。小さいほど望遠
	float nearZ = 0.1f;   // これより近い物は映らない
	float farZ = 1000.0f; // これより遠い物は映らない
	int priority = 0;     // 有効な Camera が複数あるとき、大きい方を使う（同じなら先に見つけた方）
};