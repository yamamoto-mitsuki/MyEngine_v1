#pragma once

/// <summary>
/// CSの土台（RootSignature・PSO・UAV・Dispatch・バリア）が動くかを、起動時に1回だけ確かめる
/// <para>CPUで同じ計算をして、GPU（CS）の答えと1つずつ比べる。合わなければ assert で止める</para>
/// </summary>
namespace ComputeSelfTest {
/// <summary>
/// 確かめる。Engine::Initialize の中で、ShaderPackageLoader::LoadAll（.hlsl を作る）より後に呼ぶ
/// </summary>
void Run();
} // namespace ComputeSelfTest