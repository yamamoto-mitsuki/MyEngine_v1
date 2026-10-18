#META
stage:   CS
path : Compute/ComputeTest.CS.hlsl
profile: cs_6_0
#META_END


#HLSL
// ===== CSの動作確認（ComputeSelfTest が起動時に1回だけ行う）=====
// gOutput[i] =  gInput[register] * 2 + register を、1スレッドが1要素づつ計算する

// 定数
struct TestInformation
{
    uint32_t count; // 要素の数
};

StructuredBuffer<uint32_t> gInput : register(t0); // 読むだけ（SRV）
RWStructuredBuffer<uint32_t> gOutput : register(u0); // 読み書き（UAV）
ConstantBuffer<TestInformation> gTestInformation : register(b0); // 定数（CBV）

// 1グループ ＝ x方向に64スレッド
[numthreads(64, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    // Dispatch全体で何番目のスレッドか（＝何番目の要素を受け持つか）
    uint32_t index = dispatchThreadId.x;
    // グループ単位で動かすので、最後のグループには要素の数を超えたスレッドがいる。何もせず帰る
    if (index >= gTestInformation.count)
    {
        return;
    }
    gOutput[index] = gInput[index] * 2 + index;
}

#HLSL_END