# フレームの進め方（GPU用データの書き込み / GPUを待たない作り / マルチスレッド）

## このファイルについて
- 1フレームの中で「CPUがGPU用のデータをどこに書き、いつGPUに渡し、いつ待つか」に関する作業と、将来のGPUを待たない作り・マルチスレッドの検討を書く。
- 話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。終わったStepで消してよいのは写経用のコードだけ（コミットしてから）。「なぜ」は「決めたこと」の表へ移す。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階で行う。
- 行番号やファイルの状態は2026-09-17時点（`Light.md` Step 5.2まで入れた状態）のもの。

---

## 予定

| Step | 作業 | 状態 |
|---|---|---|
| 1 | RenderContextの書き込み位置を1つにまとめる（`FrameUploadBuffer`） | 完了（2026-09-18 実行して確認済み。60ライトで約1MB） |
| 1a | `FrameUploadBuffer` をコピー禁止にする（2行＋コメントの誤字） | これから（小さいので `Light.md` Step 5.5と一緒に入れてよい） |
| 2 | SceneRendererのカメラ、Skybox、Bloom、Lensの定数も `FrameUploadBuffer` から取る | これから（4の前に必ずやる） |
| 3 | 描画リクエストを作るときに1回だけ書く（Scene / Gameの2ビューで同じデータを2回書かない） | 要検討（`Editor.md` のビューの区別と一緒に） |
| 4 | GPUを待たない作り（frames in flight） | 将来（HDR・エディタが一段落してから） |
| 5 | マルチスレッド | 将来（4の後） |

---

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| RenderContextの書き込み位置は `FrameUploadBuffer` の `offset_` 1つで管理する（Step 1） | `Light.md` Step 5.1aの不具合は「バッファとカウンタの対応ずれ」だった。場所を決める変数が1つしか無ければ、同じ種類の不具合は起きない。`UploadContext` の `staging_` もすでに同じ形 |
| 頂点・インデックス・定数・パーティクルを、型ごとのバッファに分けない（Step 1） | GPUから見るとバッファはただのバイト列で、「頂点として読む / 定数として読む」はVBV・IBV・CBV・SRVを結ぶときに決まる。分けると型ごとに上限を決めることになり、片方が余っていても片方があふれる |
| 定数は開始位置も大きさも256バイトの倍数にする。配列は `alignof(T)` にそろえる（Step 1） | 定数は前のスロットと同じ決まり。HLSL側の定数バッファがC++の構造体より少し大きくても、次のデータを読まない |
| ライトのバッファは `FrameUploadBuffer` に入れない（Step 1） | 書くのが `Update`（描画より前、リセットの後）で、フレームに1個・場所が固定。描画ごとに増えるデータと寿命が違う |
| 容量は64MBで、Profilerに使用量を出す（Step 1） | 変更前は種類ごとの上限の合計で約73MB確保していた。実際の使用量をProfilerで見て、必要なら変える。あふれたらアサートで止める |
| 実測：ポイントライト60個で約1MB、0個でほぼ0（2026-09-18）。容量はStep 4で見直す | 大半はライトの範囲の線（60個×96本×2頂点×32バイト×2ビュー ≒ 0.7MB）。64MBは今は大きすぎるが困らない。Step 4でフレーム数分（2〜3個）持つときに、実測をもとに小さくする |
| `FrameUploadBuffer` はコピー禁止にする（Step 1a） | 間違えてコピーすると、コピー側の `offset_` だけが進み、本物は先頭のまま。次の描画が同じ場所に書く（5.1aと同じ不具合）。コピー禁止ならコンパイルエラーで気づける |
| `ResetDrawCallIndex` は `ResetFrame` に名前を変える（Step 1） | 描画回数を数えなくなるため。「フレームの終わりに書き込み位置を先頭に戻す」という中身に合わせる |
| 将来は **GPUを待たない作り → マルチスレッド** の順にする | DX12の基本（フレームごとのリソース、Fence、解放のタイミング）が学べて範囲が小さい。効果を今のProfilerで数字にしやすい。マルチスレッドで描画コマンドを積むときも同じ「リソースを分ける」準備が要る |
| スワップチェーンの枚数（ダブル / トリプルバッファ）と、GPUを待たない作りは別の話として扱う | 前者は画面に出す画像の枚数、後者はCPUがGPUより何フレーム先まで進んでよいか |
| 64個より多いライトの仕組みは `Light.md` で扱う | Compute Shaderを実装してから（`Light.md` の「決めたこと」） |

---

## 今のフレームの流れ（2026-09-17時点）

1. `Engine::BeginFrame`：入力、deltaTime
2. ゲーム側から `WindowManager::UpdateAll`
   - シーン、エディタのカメラ、パーティクルの更新
   - 最後に `LightManager::Update` → `RenderContext::SetFrameLights` でライトをGPU用のバッファに書く
3. ゲーム側から `WindowManager::DrawAll`
   - シーン、エディタ、パーティクル、ライトのギズモが `RenderQueue` にリクエストを積む（まだGPU用のバッファには書かない）
4. `Engine::EndFrame`
   1. `PreRenderAll`
      - ImGuiのフレーム開始（Lights、Profilerなどのウィンドウ）
      - ビュー（Scene / Game）ごとに `RenderQueue` をコマンドリストに積む。**このときRenderContextがGPU用のバッファに書く**
   2. `PostRenderAll`
      - ImGuiを描く → コマンドリストを閉じてGPUに渡す → Present
      - キューのクリア、書き込み位置のリセット（`ResetFrame` / `ResetViewIndex`）
      - **`WaitForGPU`：GPUの処理が終わるまでCPUが止まる**
      - コマンドアロケータとコマンドリストをReset → ウィンドウのリサイズ
   3. Profilerに時間などを登録

- コマンドリスト1本、コマンドアロケータ1個、Fence1つ（`DirectXCommon`）。

### 「毎フレーム先頭から上書き」が前提になっているもの

| もの | 場所 | 戻すところ |
|---|---|---|
| 描画ごとの定数・頂点・インデックス・パーティクル | `RenderContext`（Step 1の後は `frameUploadBuffer_` 1つ） | `RenderContext::ResetFrame` |
| ライト | `RenderContext::SetFrameLights` | 毎フレーム同じ場所に上書き |
| カメラ（ビューごと、4つまで） | `SceneRenderer::cameraCB_` | `SceneRenderer::ResetViewIndex` |
| Skybox（ビューごと） | `Skybox::cb_` | `ResetViewIndex` の中の `Skybox::ResetSlot` |
| Bloom（32パス）・Lens（8パス） | `BloomPass::cb_` / `LensPass::cb_` | `ResetViewIndex` の中の `PostProcess::ResetSlot` |
| コマンドアロケータ・コマンドリスト | `DirectXCommon` | `WaitForGPU` の後に `Reset` |

どれも「次に書くときには、前のフレームのGPUの処理が終わっている」（`WaitForGPU` で待っている）から安全。

---

## GPUを待たない作り（frames in flight）の検討

### 何が変わるか
- **今**：CPUがフレームNを積む → GPUに渡す → **GPUが終わるまで待つ** → フレームN+1。GPUが描いている間、CPUは何もしていない。
- **待たない作り**：CPUはフレームNを渡したら、すぐにN+1の準備に入る。待つのは「GPUが2〜3フレーム遅れたとき」だけ。CPUとGPUが同時に動く。

### 必要になること
1. 上の表のものを、フレーム数分（2〜3個）持って順番に使う。GPUがまだ読んでいるかもしれない場所に書かないため。
2. コマンドアロケータもフレーム数分持つ。アロケータは、そのフレームのGPUの処理が終わるまでResetできない。
3. フレームごとにFenceの値を覚えておき、「そのフレームのGPUの処理が終わったか」で次の判断をする。
4. GPUのリソースの削除（テクスチャの解放、RenderTextureの作り直しなど）を、使っていたフレームが終わるまで遅らせる。
5. リサイズやシーン切り替えなど「全部止めたいとき」だけは、今まで通り全部待つ。
6. ImGuiのDX12バックエンドにもフレーム数の設定がある。GPUProfilerの読み戻しが何フレーム遅れるかも考える。

Step 1・2で「毎フレーム書く場所」を `FrameUploadBuffer` に集めておくと、1は「`FrameUploadBuffer` をフレーム数分持って、今のフレームの分を使う」だけになる。

### ダブル / トリプルバッファとの違い
- `DirectXCommon::kSwapChainBufferCount = 2`（ダブルバッファ）は、画面に出す画像（バックバッファ）の枚数。
- GPUを待たない作りは「CPUがGPUより何フレーム先まで進んでよいか」の話で、別のもの。
- ただし、何フレーム先まで積めるかはスワップチェーンの設定（バッファの枚数、`SetMaximumFrameLatency` など）とも関係するので、やるときに一緒に確認する。

### やる前に測ること
- 今のProfilerの `CPU / Times / Render` は、`PreRenderAll` と `PostRenderAll` をまとめて測っているので、**`WaitForGPU` で待っている時間も入っている**。
- 先に `WaitForGPU` の前後だけを別に測り、「CPUがGPUを待っている時間」を数字で出しておく。待ち時間が長いほど、待たない作りの効果が大きい。
- やった後に同じ項目で比べる（前後の数字がそのまま成果になる）。

---

## マルチスレッドの検討

### 候補（始めやすい順）

| 候補 | 内容 | 効果が見える場面 | 注意 |
|---|---|---|---|
| 読み込みを別スレッドにする | テクスチャ、モデル、Skyboxの読み込み | 起動やシーン切り替えの時間 | GPUへの転送（`UploadContext`）と、読み込み結果をメインスレッドに渡すタイミング |
| 更新処理を並列にする（ジョブシステム） | パーティクル、当たり判定、アニメーション | オブジェクトが多いとき | データが型ごとの連続した配列になっていると分けやすい（ARCHITECTURE.md 段階3） |
| 描画コマンドを並列に積む | ビュー（Scene / Game）ごとにコマンドリストを分けて、別スレッドで積む | 描画の数が多いとき | 今の規模ではコマンドを積む時間が短く、効果が出にくい |

### 今の作りのままではスレッドを分けられない所
- `RenderContext`、`RenderQueue`、`Renderer`、`LightManager` などは、`static` の1つの実体を全員で書き換えている。
- 例えば2つのスレッドが同時に `DrawMesh` を呼ぶと、`FrameUploadBuffer` の `offset_` を同時に進めて、同じ場所に書いてしまう（`Light.md` Step 5.1aと同じ種類の不具合が、タイミング次第で起きるので、もっと見つけにくい）。
- 分けるなら「スレッドごとに `FrameUploadBuffer` とコマンドリストを持つ」形にする。Step 1で書き込み場所を1つのクラスにまとめておくと、「スレッドごとに1個持つ」に変えやすい。
- ログなど、全体で共有しているものも同時に呼ばれて大丈夫か確認が必要。

### 学生として、どちらを先にやるか（2026-09-17の会話）
- 私見：**GPUを待たない作り → マルチスレッド** の順がよい。
  1. DX12の基本（フレームごとのリソース、Fence、解放のタイミング）がそのまま学べて、範囲が小さい。
  2. 効果（CPUの待ち時間が減る）を、今のProfilerで数字にしやすい。
  3. マルチスレッドで描画コマンドを積むときも「フレームごと・スレッドごとにリソースを分ける」準備が要るので、先にやっておくと楽になる。
- マルチスレッドのほうが目を引くが、今のシーンの規模だとCPUがボトルネックでない可能性が高い。効果を数字で示せないと、評価につながりにくい。
- どちらをやっても見られるのは「なぜやったか」と「測った結果（前後の比較）」。
- 時期は、HDR・エディタなど今の予定が一段落してから。Step 1・2は、その準備として先にやっておく。
- エディタはScene / Gameの2つのビューを描いているので、「ビューごとにスレッドを分ける」は後で自然に試せる題材になる。

---

## Step 1：RenderContextの書き込み位置を1つにまとめる

### 目的
1. 頂点・インデックス・定数・パーティクルを、**1つのUploadバッファ（`FrameUploadBuffer`）に先頭から詰めて書く**。
2. 書き込み位置を決める変数を `offset_` の1つだけにする（`Light.md` Step 5.1aの種類の不具合を、仕組みで起きなくする）。
3. 使われていないバッファ（`cameraDataRingBuffer_`）と、解放漏れ（`instance_`、パーティクル、ライトのバッファ）を片付ける。
4. 1フレームで使った量をProfilerに出す。

### 変更前と変更後
| | 変更前 | 変更後 |
|---|---|---|
| 描画ごとのデータのバッファ | 12個（行列、カメラ（未使用）、頂点×3、インデックス×2、マテリアル×4、パーティクル） | 1個 |
| 書き込み位置を決める変数 | 9個（`drawCallIndex_`、`drawCallLineIndex_`、`drawCallParticleIndex_`、`vertex3dIndex_`、`vertex2dIndex_`、`vertexLineIndex_`、`index3dIndex_`、`index2dIndex_`、`particleIndex_`） | 1個（`offset_`） |
| 上限 | 種類ごと（頂点100万、線の頂点6.5万、描画4096回、パーティクル4096個…） | 合計64MB |
| 確保しているメモリ | 約73MB | 64MB |

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Graphics/GPU/FrameUploadBuffer.h` | **新規**（ファイル全体） |
| `MyEngine/Graphics/GPU/FrameUploadBuffer.cpp` | **新規**（ファイル全体） |
| `MyEngine/Graphics/Renderer/RenderContext.h` | ファイル全体を差し替え |
| `MyEngine/Graphics/Renderer/RenderContext.cpp` | ファイル全体を差し替え |
| `MyEngine/Window/WindowManager.cpp` | `ResetDrawCallIndex` → `ResetFrame`（1行） |
| `MyEngine/Engine.cpp` | Profilerに使用量を出す（任意） |

- 新規ファイルは、Visual Studioのソリューションエクスプローラーで `Graphics/GPU` のフィルタに追加する（`UploadContext` と同じ場所）。
- RenderQueue、Renderer、SceneRendererなど、RenderContextを呼ぶ側は変更なし（`DrawMesh` などの関数の形は同じ）。
- `RenderContext::kMaxVertices` などの定数は消える。エンジンの中では使われていないことを確認済み。ゲーム側で使っていたらエラーになるので消す。
- `Light.md` Step 5.1aで直したカウンタの部分は、コードごと無くなる。

> スクラッチで、`FrameUploadBuffer`、`RenderContext`、`WindowManager`、`Engine`、`Renderer`、`RenderQueue`、`SceneRenderer`、`RenderWindow`、`DirectXCommon`、`LightManager`、`LightGizmo`、`ImGuiManager` をDebug / Releaseでコンパイルし、通ることを確認済み（`/W4`。新しい警告なし。元からある `Renderer.cpp` の `r, g, b, a` などの警告だけ）。

---

### ① `MyEngine/Graphics/GPU/FrameUploadBuffer.h`（新規）
```cpp
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

#include <d3d12.h>
#include <wrl.h>


/// <summary>
/// 1フレームの間だけ使うGPU用のデータ（定数・頂点・インデックスなど）を、1つのUploadバッファに先頭から詰めて書く
/// <para>書き込み位置は offset_ の1つだけ。型ごとにカウンタを持たないので、場所がぶつからない</para>
/// <para>フレームの終わりに Reset で先頭に戻す（GPUの処理が終わるのを待ってから次のフレームを書く前提）</para>
/// </summary>
class FrameUploadBuffer {
public:
	// 確保した場所
	struct Allocation {
		uint8_t* cpuAddress = nullptr;            // CPUから書き込む先
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0; // GPUに渡す場所（CBV / VBV / IBV / SRVに使う）
		size_t size = 0;                          // 確保したバイト数
	};

	/// <summary>
	/// バッファを作ってMapしたままにする
	/// </summary>
	void Initialize(size_t capacity, const std::string& name);

	/// <summary>
	/// バッファを解放する
	/// </summary>
	void Release();

	/// <summary>
	/// sizeバイトを確保する。開始位置をalignmentの倍数にそろえる（alignmentは2のべき乗）
	/// </summary>
	Allocation Allocate(size_t size, size_t alignment);

	/// <summary>
	/// 定数バッファ1個分を書き込み、GPUに渡す場所を返す
	/// <para>開始位置も大きさも256バイトの倍数にする（HLSL側が少し大きくても次のデータを読まない）</para>
	/// </summary>
	template<class T> D3D12_GPU_VIRTUAL_ADDRESS PushConstant(const T& data) {
		Allocation allocation = Allocate(AlignUp(sizeof(T), kConstantBufferAlignment), kConstantBufferAlignment);
		std::memcpy(allocation.cpuAddress, &data, sizeof(T));
		return allocation.gpuAddress;
	}

	/// <summary>
	/// 配列（頂点・インデックス・パーティクルなど）を書き込み、確保した場所を返す
	/// </summary>
	template<class T> Allocation PushArray(const T* data, size_t count) {
		Allocation allocation = Allocate(sizeof(T) * count, alignof(T));
		if (count > 0) {
			std::memcpy(allocation.cpuAddress, data, sizeof(T) * count);
		}
		return allocation;
	}

	/// <summary>
	/// 書き込み位置を先頭に戻す。フレームの終わりに1回呼ぶ
	/// </summary>
	void Reset();

	// 今のフレームで使っているバイト数
	size_t GetUsedBytes() const { return offset_; }
	// 前のフレームで使ったバイト数（Resetの直前の値。Profiler用）
	size_t GetLastFrameUsedBytes() const { return lastFrameUsedBytes_; }
	// 容量
	size_t GetCapacity() const { return capacity_; }
	// GPUページフォルトの調査用
	ID3D12Resource* GetResource() const { return buffer_.Get(); }

private:
	static constexpr size_t kConstantBufferAlignment = 256;

	// valueを、alignmentの倍数に切り上げる
	static size_t AlignUp(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

	Microsoft::WRL::ComPtr<ID3D12Resource> buffer_ = nullptr;
	uint8_t* mappedPtr_ = nullptr;           // 永続Mapしている先頭
	D3D12_GPU_VIRTUAL_ADDRESS gpuBase_ = 0;  // GPUから見た先頭
	size_t capacity_ = 0;                    // 容量（バイト）
	size_t offset_ = 0;                      // 次に書き込む位置（バイト）。書き込み位置はこれ1つだけ
	size_t lastFrameUsedBytes_ = 0;          // 前のフレームで使ったバイト数
};
```

### ② `MyEngine/Graphics/GPU/FrameUploadBuffer.cpp`（新規）
```cpp
#include "MyEngine/Graphics/GPU/FrameUploadBuffer.h"

#include <format>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/String/ConvertString.h"


//=============================================================================
// 初期化 / 解放
//=============================================================================
void FrameUploadBuffer::Initialize(size_t capacity, const std::string& name) {
	MY_ASSERT_MSG(!buffer_, "FrameUploadBuffer::Initialize が2回呼ばれました");
	buffer_ = DirectXCommon::CreateMappedUploadBuffer(capacity, reinterpret_cast<void**>(&mappedPtr_));
	buffer_->SetName(ConvertString(name).c_str());
	gpuBase_ = buffer_->GetGPUVirtualAddress();
	capacity_ = capacity;
	offset_ = 0;
	LogManager::Log(std::format("{} : {} MB", name, capacity / (1024 * 1024)));
}

void FrameUploadBuffer::Release() {
	buffer_ = nullptr; // Mapしたまま解放してよい（リソースが消えるときに外れる）
	mappedPtr_ = nullptr;
	gpuBase_ = 0;
	capacity_ = 0;
	offset_ = 0;
}


//=============================================================================
// 確保
//=============================================================================
FrameUploadBuffer::Allocation FrameUploadBuffer::Allocate(size_t size, size_t alignment) {
	MY_ASSERT_MSG(mappedPtr_, "FrameUploadBuffer::Initialize を先に呼んでください");
	MY_ASSERT_MSG(alignment != 0 && (alignment & (alignment - 1)) == 0, "alignmentは2のべき乗にしてください");

	size_t start = AlignUp(offset_, alignment); // 前のデータの続きを、alignmentの倍数にそろえた位置から使う
	MY_ASSERT_MSG(start + size <= capacity_,
		std::format("FrameUploadBufferの容量が足りません（{} / {} バイト）。RenderContext::kFrameUploadBytes を増やしてください", start + size, capacity_));
	offset_ = start + size; // 次はこの後ろから

	Allocation allocation;
	allocation.cpuAddress = mappedPtr_ + start;
	allocation.gpuAddress = gpuBase_ + start;
	allocation.size = size;
	return allocation;
}


//=============================================================================
// フレームの終わり
//=============================================================================
void FrameUploadBuffer::Reset() {
	lastFrameUsedBytes_ = offset_; // 使った量を残してから戻す
	offset_ = 0;
}
```

### ③ `MyEngine/Graphics/Renderer/RenderContext.h`（ファイル全体を差し替え）
```cpp
#pragma once
#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Graphics/GPU/FrameUploadBuffer.h"
#include "MyEngine/Graphics/Renderer/DrawRequest.h"

// 前方宣言
class RenderWindow;

/// <summary>
/// 描画1回分のデータをGPU用のバッファに書き、DrawCallを発行するだけの実行層
/// <para>状態切替（RootSignature/PSO）はRenderQueueの仕事</para>
/// </summary>
class RenderContext {
public:
	// 1フレームに書けるGPU用データの合計（頂点・インデックス・定数・パーティクルの合計）
	static constexpr size_t kFrameUploadBytes = 64 * 1024 * 1024;

	RenderContext(const RenderContext&) = delete;
	RenderContext& operator=(const RenderContext&) = delete;
	// 初期化・解放
	static void Initialize();
	static void Release();

	/// <summary>
	/// 3Dメッシュを1つ描画する（動的=Primitive / 静的=Model の両対応）
	/// </summary>
	static void DrawMesh(const MeshRequest& req);

	/// <summary>
	/// 2Dスプライトを1つ描画する
	/// </summary>
	static void DrawSprite(const SpriteRequest& req);

	/// <summary>
	/// パーティクルを描画する
	/// </summary>
	static void DrawParticles(const ParticleRequest& req);

	/// <summary>
	/// 3Dライン群を描画する（LINELIST。奇数個の頂点は最後を切り捨て）
	/// </summary>
	static void DrawLines(const LineRequest& req);

	/// <summary>
	/// GPUページフォルトのアドレスがどのバッファ内かログに出力する
	/// </summary>
	static void LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA);

	/// <summary>
	/// このフレームのGPU用データの書き込み位置を先頭に戻す。フレームの終わりに1回呼ぶ
	/// </summary>
	static void ResetFrame();

	/// <summary>
	/// このフレームのライトを書き込む。全描画で同じ場所を結ぶ
	/// </summary>
	static void SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights);

	// 前のフレームでGPU用に書いたデータの量（バイト）。Profiler用
	static size_t GetLastFrameUploadBytes() { return instance_->frameUploadBuffer_.GetLastFrameUsedBytes(); }
	// 1フレームに書ける量（バイト）
	static size_t GetFrameUploadCapacity() { return instance_->frameUploadBuffer_.GetCapacity(); }


private:
	RenderContext() = default;
	~RenderContext() = default;

	static RenderContext* instance_;

	void InitInternal();

	// --- パーティクル用の共通Quad（最初に1回だけ書く） ---
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadVB_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleQuadIB_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW particleQuadVBV_{};
	D3D12_INDEX_BUFFER_VIEW particleQuadIBV_{};

	// --- 描画ごとに増えるデータ（定数・頂点・インデックス・パーティクル）。全部ここに先頭から詰めて書く ---
	FrameUploadBuffer frameUploadBuffer_;

	// --- ライト（1フレームに1個。全描画で共有する） ---
	Microsoft::WRL::ComPtr<ID3D12Resource> frameDirectionalLightBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> framePointLightsBuffer_ = nullptr;
	DirectionalLightData* frameDirectionalLightMappedPtr_ = nullptr;
	PointLightListData* framePointLightsMappedPtr_ = nullptr;
};
```

### ④ `MyEngine/Graphics/Renderer/RenderContext.cpp`（ファイル全体を差し替え）
```cpp
#include "MyEngine/Graphics/Renderer/RenderContext.h"

#include <format>
#include <iterator>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/Profiling/GPUScope.h"
#include "MyEngine/Graphics/Pipeline/PSOManager.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/RenderTarget/RenderWindow.h"

// 静的メンバ変数
RenderContext* RenderContext::instance_ = nullptr;

static UINT SlotOf(const RootSignatureInfo& rs, RootBind bind) {
	auto it = rs.slotOf.find(bind);
	MY_ASSERT_MSG(it != rs.slotOf.end(), std::format("slotOf に RootBind::{} が無い。NameToRoleの名前とHLSLの変数名が不一致の可能性", magic_enum::enum_name(bind)));
	return it->second;
}


//=============================================================================
// 初期化・解放
//=============================================================================
// ===== 初期化 =====
void RenderContext::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new RenderContext();
	instance_->InitInternal();
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void RenderContext::Release() {
	delete instance_; // ComPtrとFrameUploadBufferは、メンバの破棄で一緒に解放される
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// このフレームのライト
//=============================================================================
void RenderContext::SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights) {
	// GPUの処理が終わるのを待ってから次のフレームに進む作りなので、毎フレーム同じ場所を上書きしてよい
	std::memcpy(instance_->frameDirectionalLightMappedPtr_, &directionalLight, sizeof(DirectionalLightData));
	std::memcpy(instance_->framePointLightsMappedPtr_, &pointLights, sizeof(PointLightListData));
}


//=============================================================================
// フレームの終わり
//=============================================================================
void RenderContext::ResetFrame() { instance_->frameUploadBuffer_.Reset(); }


//=============================================================================
// メッシュ描画
//=============================================================================
void RenderContext::DrawMesh(const MeshRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;

	// ===== 定数（動的・静的共通） =====
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	D3D12_GPU_VIRTUAL_ADDRESS transformAddress = upload.PushConstant(req.objectTransformData);

	// ===== ジオメトリ =====
	uint32_t indexCount = 0;
	if (req.isStatic) {
		// --- 静的（Model）: GPU常駐バッファをバインドする ---
		cmdList->IASetVertexBuffers(0, 1, &req.vbv); // 頂点バッファ
		cmdList->IASetIndexBuffer(&req.ibv);         // インデックスバッファ
		indexCount = req.indexCount;                 // インデックス数
	} else {
		// --- 動的（Primitive）: 頂点とインデックスをこのフレーム用のバッファに書いて VBV / IBV を組む ---
		FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), req.vertices.size());
		FrameUploadBuffer::Allocation indices = upload.PushArray(req.indices.data(), req.indices.size());
		// VertexBufferView（型の区別は StrideInBytes で伝える）
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = vertices.gpuAddress;
		vbv.SizeInBytes = static_cast<UINT>(vertices.size);
		vbv.StrideInBytes = sizeof(Vertex3dData);
		// IndexBufferView（型の区別は Format で伝える）
		D3D12_INDEX_BUFFER_VIEW ibv{};
		ibv.BufferLocation = indices.gpuAddress;
		ibv.SizeInBytes = static_cast<UINT>(indices.size);
		ibv.Format = DXGI_FORMAT_R32_UINT;

		cmdList->IASetVertexBuffers(0, 1, &vbv);
		cmdList->IASetIndexBuffer(&ibv);
		indexCount = static_cast<uint32_t>(req.indices.size());
	}
	// トポロジ
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Model, req.shadingType);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(SlotOf(rs, RootBind::Material), materialAddress);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), transformAddress);
	// --- Lit系のみ存在するスロット ---
	if (req.shadingType != ShadingType::Unlit) {
		// DirectionalLight
		cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::DirectionalLight), instance_->frameDirectionalLightBuffer_->GetGPUVirtualAddress());
		// PointLight
		cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::PointLights), instance_->framePointLightsBuffer_->GetGPUVirtualAddress());

		// IBL（PBRのRootSignatureにだけ存在する）
		if (req.shadingType == ShadingType::PBR) {
			auto it = rs.slotOf.find(RootBind::IBL);
			MY_ASSERT_MSG(it != rs.slotOf.end(), "PBRのRootSignatureにIBLスロットがありません");
			MY_ASSERT_MSG(req.iblParamsAddress != 0, "PBRにはIBLEnvironmentの設定が必要です");
			cmdList->SetGraphicsRootConstantBufferView(it->second, req.iblParamsAddress);
		}
	}

	// ===== DrawCall =====
#ifdef _DEBUG
	// 例: "suzanne / Material.001"
	std::string marker = req.debugName ? *req.debugName : std::string("Primitive");
	if (req.debugSubName && !req.debugSubName->empty()) {
		marker += " / " + *req.debugSubName;
	}
	GPU_MARKER(cmdList, marker.c_str());
#endif
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}


//=============================================================================
// パーティクル描画
//=============================================================================
void RenderContext::DrawParticles(const ParticleRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;
	// RootSignature
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Particle);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// ===== このフレーム用のバッファに書く =====
	// インスタンス配列（VSがStructuredBufferとして読む）
	FrameUploadBuffer::Allocation instances = upload.PushArray(req.instances.data(), req.instances.size());
	// グループのマテリアル
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);

	// ===== バインド =====
	// VSのParticle
	cmdList->SetGraphicsRootShaderResourceView(rs.slotOf.at(RootBind::Particles), instances.gpuAddress);
	// マテリアル
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);

	// quadをインスタンス数分
	UINT count = static_cast<UINT>(req.instances.size());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &instance_->particleQuadVBV_);
	cmdList->IASetIndexBuffer(&instance_->particleQuadIBV_);
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(6, count, 0, 0, 0);
}


//=============================================================================
// 2Dスプライト描画
//=============================================================================
void RenderContext::DrawSprite(const SpriteRequest& req) {
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;

	// ===== このフレーム用のバッファに書く =====
	static constexpr uint32_t kIndices[] = {0, 1, 2, 1, 3, 2};
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), req.vertices.size());
	FrameUploadBuffer::Allocation indices = upload.PushArray(kIndices, std::size(kIndices));

	// ===== ジオメトリ =====
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = vertices.gpuAddress;
	vbv.SizeInBytes = static_cast<UINT>(vertices.size);
	vbv.StrideInBytes = sizeof(Vertex2dData);
	// IndexBufferView
	D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = indices.gpuAddress;
	ibv.SizeInBytes = static_cast<UINT>(indices.size);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetIndexBuffer(&ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ===== ShaderConstantsバインド =====
	// RootSignatureID
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Sprite);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawIndexedInstanced(static_cast<UINT>(std::size(kIndices)), 1, 0, 0, 0);
}


//=============================================================================
// Line3D描画
//=============================================================================
void RenderContext::DrawLines(const LineRequest& req) {
	// 奇数は切り捨て。描く線が無ければ何もしない
	size_t vertexCount = req.vertices.size() & ~size_t(1);
	if (vertexCount == 0) {
		return;
	}
	auto* cmdList = DirectXCommon::GetCommandList();
	FrameUploadBuffer& upload = instance_->frameUploadBuffer_;
	uint32_t prog = PSOManager::GetShaderProgramID(DrawCategory::Line);
	const RootSignatureInfo& rs = PSOManager::GetRootSignatureInfo(prog);

	// ===== このフレーム用のバッファに書く =====
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress = upload.PushConstant(req.materialData);
	D3D12_GPU_VIRTUAL_ADDRESS transformAddress = upload.PushConstant(req.objectTransformData);
	FrameUploadBuffer::Allocation vertices = upload.PushArray(req.vertices.data(), vertexCount);

	// ===== ジオメトリ =====
	// VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	vbv.BufferLocation = vertices.gpuAddress;
	vbv.SizeInBytes = static_cast<UINT>(vertices.size);
	vbv.StrideInBytes = sizeof(VertexLineData);
	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// ===== ShaderConstantsバインド =====
	// Material
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::Material), materialAddress);
	// TransformationMatrix
	cmdList->SetGraphicsRootConstantBufferView(rs.slotOf.at(RootBind::ObjectTransform), transformAddress);

	// ===== Draw Call =====
	DirectXCommon::IncrementDrawCallCount();
	cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
}


//=============================================================================
// 初期化（内部）
//=============================================================================
void RenderContext::InitInternal() {
	// ===== 描画ごとに増えるデータの置き場（1つだけ） =====
	frameUploadBuffer_.Initialize(kFrameUploadBytes, "frameUploadBuffer_");

	// ===== ライト（1フレームに1個。全描画で同じ場所を結ぶので、描画回数分の大きさは要らない） =====
	// 大きさはCreateUploadBufferの中で256の倍数にそろえられる
	frameDirectionalLightBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(DirectionalLightData), reinterpret_cast<void**>(&frameDirectionalLightMappedPtr_));
	frameDirectionalLightBuffer_->SetName(L"frameDirectionalLightBuffer_");
	framePointLightsBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(PointLightListData), reinterpret_cast<void**>(&framePointLightsMappedPtr_));
	framePointLightsBuffer_->SetName(L"framePointLightsBuffer_");
	*frameDirectionalLightMappedPtr_ = DirectionalLightData{}; // 最初のSetFrameLightsまでの既定値（白・真下・強さ1）
	*framePointLightsMappedPtr_ = PointLightListData{};        // ポイントライト0個

	// ===== パーティクル用の共通Quad =====
	// 頂点フォーマットは Particle の InputLayout（POSITION + TEXCOORD） = VertexParticleData
	const Vertex2dData quadVertices[4] = {
	    {{-0.5f, +0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 左上
	    {{+0.5f, +0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 右上
	    {{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
	    {{+0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
	};
	const uint32_t quadIndices[6] = {0, 1, 2, 1, 3, 2};
	// 頂点バッファ：Uploadヒープに1回だけ書いてUnmap（永続Mapしない。二度と書き換えないので）
	particleQuadVB_ = DirectXCommon::CreateUploadBuffer(sizeof(quadVertices));
	particleQuadVB_->SetName(L"ParticleQuadVB");
	void* mapped = nullptr;
	particleQuadVB_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, quadVertices, sizeof(quadVertices));
	particleQuadVB_->Unmap(0, nullptr);
	particleQuadVBV_.BufferLocation = particleQuadVB_->GetGPUVirtualAddress();
	particleQuadVBV_.SizeInBytes = sizeof(quadVertices);
	particleQuadVBV_.StrideInBytes = sizeof(Vertex2dData);
	// インデックスバッファ
	particleQuadIB_ = DirectXCommon::CreateUploadBuffer(sizeof(quadIndices));
	particleQuadIB_->SetName(L"ParticleQuadIB");
	particleQuadIB_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, quadIndices, sizeof(quadIndices));
	particleQuadIB_->Unmap(0, nullptr);
	particleQuadIBV_.BufferLocation = particleQuadIB_->GetGPUVirtualAddress();
	particleQuadIBV_.SizeInBytes = sizeof(quadIndices);
	particleQuadIBV_.Format = DXGI_FORMAT_R32_UINT;
}


//=============================================================================
// GPUページフォルトのアドレスをどのリソースが原因かログに出力する
//=============================================================================
void RenderContext::LogFaultResource(D3D12_GPU_VIRTUAL_ADDRESS faultVA) {
	// 解放後に呼ばれたときは調べない
	if (!instance_) {
		return;
	}

	struct Entry {
		const char* name;
		ID3D12Resource* resource;
	};
	Entry buffers[] = {
	    {"frameUploadBuffer_",           instance_->frameUploadBuffer_.GetResource()   },
	    {"frameDirectionalLightBuffer_", instance_->frameDirectionalLightBuffer_.Get()},
	    {"framePointLightsBuffer_",      instance_->framePointLightsBuffer_.Get()     },
	    {"particleQuadVB_",              instance_->particleQuadVB_.Get()             },
	    {"particleQuadIB_",              instance_->particleQuadIB_.Get()             },
	};

	for (const Entry& e : buffers) {
		if (!e.resource) {
			continue;
		}
		D3D12_GPU_VIRTUAL_ADDRESS base = e.resource->GetGPUVirtualAddress(); // GPUアドレス
		UINT64 size = e.resource->GetDesc().Width;                           // バッファのバイトサイズ

		if (faultVA >= base && faultVA < base + size) {
			UINT64 offset = faultVA - base;
			LogManager::Error(std::format("[DRED] fault VA is inside '{}'  offset = {} / {} bytes", e.name, offset, size));
			if (e.resource == instance_->frameUploadBuffer_.GetResource()) {
				// 使っていた範囲より後ろなら、確保していない場所を読んでいる
				LogManager::Error(std::format("[DRED] frameUploadBuffer_ used = {} bytes", instance_->frameUploadBuffer_.GetUsedBytes()));
			}
			return;
		}
	}
	LogManager::Error("[DRED] fault VA does not match any RenderContext buffer");
}
```

### ⑤ `MyEngine/Window/WindowManager.cpp`

**`PostRenderAll` の中**：1行差し替える

変更前
```cpp
	RenderContext::ResetDrawCallIndex(); // 描画コールインデックスをリセット
```
変更後
```cpp
	RenderContext::ResetFrame(); // GPU用データの書き込み位置を先頭に戻す
```

### ⑥ `MyEngine/Engine.cpp`（任意）

**`EndFrame` の中**：追加する（`DirectXCommon::ResetDrawCallCount();` の下、`#endif` の上）
```cpp
	// 1フレームにGPU用に書いたデータの量（RenderContextのFrameUploadBuffer）
	constexpr float kBytesToMB = 1.0f / (1024.0f * 1024.0f);
	float uploadMB = static_cast<float>(RenderContext::GetLastFrameUploadBytes()) * kBytesToMB;
	float uploadCapacityMB = static_cast<float>(RenderContext::GetFrameUploadCapacity()) * kBytesToMB;
	Profiler::Category(ProfCategory::GPU).Group("Stats").Bar("Frame Upload", uploadMB, uploadCapacityMB, "MB");
```

---

### 解説

**直すべきだったのは「バッファが分かれていること」ではなく「場所の決め方が分かれていること」**
- 5.1aの不具合は、1つのバッファ（行列）を、2つのカウンタ（メッシュ用と線用）で使っていたことが原因だった。
- バッファが12個、カウンタが9個あり、「どのバッファをどのカウンタで数えるか」がコードのあちこちに散らばっていた。新しい描画の種類を足すたびに、この対応を正しく選ぶ必要があった。
- `FrameUploadBuffer` は、場所を決める変数が `offset_` の1つしか無い。`Allocate` を呼ぶたびに必ず後ろへ進むので、2つの描画が同じ場所をもらうことが起きない。

**型が違うデータを、同じバッファに置いてよい理由**
- GPUから見ると、バッファはただのバイト列。「どう読むか」は結ぶときに決まる。
  - 頂点：`D3D12_VERTEX_BUFFER_VIEW` の `StrideInBytes`（1頂点の大きさ）
  - インデックス：`D3D12_INDEX_BUFFER_VIEW` の `Format`（`R32_UINT`）
  - 定数：`SetGraphicsRootConstantBufferView` に渡した場所から、シェーダーの構造体の形で読む
  - パーティクル：`SetGraphicsRootShaderResourceView` に渡した場所から、StructuredBufferとして読む
- Uploadヒープのバッファは `GENERIC_READ` の状態で作られていて、これは頂点・インデックス・定数・シェーダーからの読み取りを全部含む。なので1つのバッファを全部の用途に使える。

**なぜ「後ろに足していくだけ」でよいのか（線形アロケータ）**
- 普通のメモリ管理（`new` / `delete`）は、途中で消した場所を再利用するために空きを管理する必要がある。
- このバッファのデータは「このフレームの間だけ使い、フレームの終わりに全部まとめて捨てる」。途中で1個だけ消すことが無いので、空きの管理が要らない。確保は `offset_` を足すだけで、捨てるのは `offset_ = 0` だけ。
- この形を **線形アロケータ**（バンプアロケータ）と呼ぶ。ゲームエンジンで「1フレームだけ使うメモリ」によく使われる。
- エンジンの `UploadContext` の `staging_` / `stagingOffset_` も、すでに同じ形になっている。

**アラインメント（`AlignUp`）**
- 定数：開始位置を256の倍数にそろえ、大きさも256の倍数にする。前のリングバッファの「1スロット256バイト」と同じ決まり。HLSLの定数バッファの大きさがC++の構造体より少し大きくても、隣のデータを読まない。
- 配列：開始位置を `alignof(T)`（`Vertex3dData` などは4）の倍数にそろえる。
- `(value + alignment - 1) & ~(alignment - 1)` は「alignmentの倍数に切り上げる」書き方。alignmentが2のべき乗のときだけ正しいので、`Allocate` でアサートしている。

**ライトを `FrameUploadBuffer` に入れない理由**
- ライトを書くのは `Update` の中（描画より前）で、フレームに1回・場所は固定。描画のたびに増えるデータとは寿命が違う。
- `FrameUploadBuffer` に入れると、「`ResetFrame` の後、描画の前に書く」という順番に依存する。順番を間違えると、他のデータに上書きされて、ライトがおかしくなる不具合になる。

**容量（64MB）と上限の考え方**
- 前は種類ごとに上限があった（頂点100万個で36MB、2Dの頂点100万個で24MB…）。使っていない種類の分もずっと確保していた。
- 今は合計で64MB。どの種類が使ってもよい。足りなくなったら `Allocate` のアサートで止まり、使った量が表示される。
- ProfilerのGPU / Statsの「Frame Upload」で、実際にどれだけ使っているかを見て、必要なら `kFrameUploadBytes` を変える。

**解放の変更**
- 前の `Release` は一部のバッファを `nullptr` にするだけで、`instance_` を `delete` していなかった（パーティクルとライトのバッファ、`instance_` 自体が解放されていなかった）。
- 今は `delete instance_` で、メンバの `ComPtr` と `FrameUploadBuffer` が全部一緒に解放される。
- `LogFaultResource` は、解放の後に呼ばれても落ちないように、`instance_` が無ければ何もしない。

**5.1aの修正はどうなるのか**
- カウンタごと無くなるので、5.1aで足した行も一緒に消える。5.1aで「なぜ壊れたか」を理解したうえで、「そもそも壊れない形」にするのがこのStep。

### ついでに見つけたもの（このStepではやらない）
- `MeshRequest::cameraData` は `DrawModel` で値を入れているが、GPUには送っていない（カメラは `SceneRenderer` のカメラの定数を結んでいる）。`MeshRequest::isBillboard` も使われていない（使っているのは `objectTransformData.isBillboard`）。どちらも消してよい。
- `DrawMesh` の `SlotOf(rs, ...)` と `rs.slotOf.at(...)` の書き方が混ざっている。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る（`RenderContext::kMaxVertices` などを使っていたらエラーになるので消す）
3. **実行して確認**
   - 見た目が前と同じ：モデル、グリッド、Skybox、ライトのアイコンと範囲、カメラの視錐台、パーティクル、スプライト（UI）、Bloom
   - Lightsウィンドウでライトを70個にしても、アイコンと当たり方が正しい
   - ProfilerのGPU / Statsに「Frame Upload」のバーが出る。数字をメモしておく（「決めたこと」の容量の判断に使う）
   - Draw Callsの数が前と同じ
   - 終了したときに、出力ウィンドウのリソースリークの報告（`D3D12ResourceLeakChecker`）が前より増えていない

### 次のStepでやること（ここではやらない）
- Step 2：SceneRendererのカメラ、Skybox、Bloom、Lensの定数も、それぞれのバッファとスロットをやめて `FrameUploadBuffer` から取る（`RenderContext` から外に使える形にする）。
- `Light.md` Step 5.5（スポットライト）。

---

### Step 1の質問と答え（2026-09-18）

**`FrameUploadBuffer& upload = instance_->frameUploadBuffer_;` の `&` は何か**
- `&` は参照。「`instance_->frameUploadBuffer_` に `upload` という別名を付ける」だけで、中身は同じ1つのもの。長い名前を毎回書かなくて済む。
- `&` を付けないと**コピー**になる。コピーの `offset_` を進めても本物の `offset_` は進まないので、次の描画も本物の先頭から書き始め、全部の描画が同じ場所に書く（5.1aと同じ不具合）。
- 見た目は1文字の違いで気づきにくいので、Step 1aで「コピーするとコンパイルエラー」にする。

**アラインメントは256でなくても、2のべき乗ならよいのか。なぜ256なのか**
- 定数バッファの256は、D3D12の決まり（定数バッファのビューの大きさは256バイトの倍数）。GPUによって「定数を置く位置の区切り」の都合が違うので、どのGPUでも困らない値としてAPI側が決めている、と考えるとよい。
- このエンジンは `SetGraphicsRootConstantBufferView` で場所だけを渡しているので、64にしても手元のGPUでは動くかもしれない。ただし決まりの外なので、別のGPUやデバッグ用の検証で問題になりうる。**決まりに合わせて256にしておく**のが安全。
- 1024にしても正しく動くが、無駄が増える。`Material3dData`（約160バイト）なら256で足りるのに、1024だと1回の描画で約860バイト捨てることになる。
- 頂点やインデックスの配列は、この決まりが無いので `alignof(T)`（`Vertex3dData` なら4）で詰めている。

**なぜ2のべき乗なのか。倍数ではダメなのか**
- 「alignmentの倍数に切り上げる」だけなら、どんな数でもできる：`(value + alignment - 1) / alignment * alignment`（割り算と掛け算）。
- 2のべき乗なら、`(value + alignment - 1) & ~(alignment - 1)` とビット演算1回で書ける。256なら `~(256 - 1)` は「下8ビットを0にする」マスクで、割り算が要らない。
- そもそもメモリの揃えの要求（C++の `alignof`、GPUの決まり）は、必ず2のべき乗になっている。なので2のべき乗だけ受け付けて、それ以外はアサートで止めている。

**`UploadContext` はどこで使われているか、何をするクラスか**
- 使っているのは `ModelManager` だけ（モデルを読み込んだとき、頂点とインデックスを `QueueUpload` し、最後に `Flush`）。
- 役割は「**ずっと使うデータ**を、GPU専用の速いメモリ（DEFAULTヒープ）へ1回だけ送る」こと。
  1. CPUから書けるステージング用バッファ（UPLOADヒープ）に書く
  2. 専用のコマンドリストで「ステージング → DEFAULTヒープ」のコピーを積む
  3. `Flush` でGPUに実行させ、終わるまで待つ（フレームの描画とは別のFence）
- `FrameUploadBuffer` との違い

| | `UploadContext` | `FrameUploadBuffer` |
|---|---|---|
| 送るデータ | モデルの頂点・インデックス（読み込み時に1回） | 描画ごとの定数・頂点・インデックス（毎フレーム） |
| GPUが読む場所 | DEFAULTヒープ（GPU専用で速い）にコピーした先 | UPLOADヒープをそのまま読む |
| 寿命 | モデルを解放するまで | そのフレームだけ |
| 共通点 | どちらも「1つのバッファ＋書き込み位置1つ」で、途中の空きを管理しない（線形アロケータ） | |

- 毎フレーム変わるデータをDEFAULTヒープにコピーすると、コピーの手間のほうが大きい。ずっと使うデータは、1回コピーしておけば毎フレーム速く読める。だから2つを使い分ける。

**60個で約1MB、0個で0は正常か**
- 正常。計算するとほぼ合う。
  - 範囲の線：60個 × 3つの円 × 32本 × 2頂点 × 32バイト ≒ 0.35MB（1MB = 1024×1024バイト）。SceneビューとGameビューで2回描くので約0.7MB
  - アイコン：定数2つ（256＋256）＋頂点とインデックス（168バイト、次の定数の位置を256にそろえるので実質256）＝ 768バイト × 60個 × 2ビュー ≒ 0.09MB
  - モデル、グリッド、視錐台などは数KB
- 0個のときも数KBは使っているが、表示が小数2桁（MB）なので0.00になる。
- 大半が線なので、ライトの数に比例して増える。64MBはかなり余裕がある（「決めたこと」参照）。

**モデルも描いているのに、ライトが0個だとほぼ0なのはなぜか**
- モデルの描画（`DrawModel`）は**静的**。頂点とインデックスは読み込み時にGPU専用のメモリ（DEFAULTヒープ）へ置いてあるので、毎フレーム書くのは**定数2つだけ**（マテリアル256バイト＋行列256バイト＝512バイト）。
- サブメッシュ1つ × 2ビューで1KB。5個あっても5KB。小数2桁のMB表示では0.00になる。
- 毎フレーム全部書いているのは、Primitive（板・球・AABB）・線・スプライト・パーティクル。だからアイコンと範囲の線だけが目立つ。

| 描くもの | 1つあたり、毎フレーム書く量 |
|---|---|
| モデルのサブメッシュ1つ（グリッドも含む） | 512バイト（定数2つだけ。頂点は常駐） |
| アイコン（板1枚） | 768バイト（定数2つ＋頂点4つ＋インデックス6つ） |
| 範囲の線（ポイントライト1個分＝96本） | 約6KB |
| スプライト1枚 | 約1KB |
| パーティクル1グループ（100個） | 約8KB（1個80バイト） |

- これが `MeshRequest::isStatic` の分岐の効果。モデルをたくさん置いてもこのバッファはほとんど増えず、増えるのはPrimitiveと線。
- ポイントライト64個＋スポットライト32個（上限）でも約0.98MBだった。ライトを数千個置けるかは `Light.md` の「ライトは何個まで置けるか」に書いた（バッファではなくピクセルシェーダーが先に限界になる）。

---

## Step 1a：`FrameUploadBuffer` をコピー禁止にする

### ① `MyEngine/Graphics/GPU/FrameUploadBuffer.h`

**追加する**：`public:` のすぐ下
```cpp
	FrameUploadBuffer() = default;
	// コピー禁止。コピーすると offset_ が別々に進み、同じ場所に書いてしまう
	FrameUploadBuffer(const FrameUploadBuffer&) = delete;
	FrameUploadBuffer& operator=(const FrameUploadBuffer&) = delete;
```

**直す**：クラスの説明コメントの誤字（「型だけに」→「型ごとに」）
```cpp
/// <para>書き込み位置は offset_ の1つだけ。型ごとにカウンタを持たないので、場所がぶつからない</para>
```

### 解説
- コピーコンストラクタを `delete` すると、コンストラクタを1つも書いていない扱いではなくなり、引数なしのコンストラクタが自動で作られなくなる。なので `FrameUploadBuffer() = default;` も書く（`RenderContext` のメンバとして作るときに必要）。
- 試しに `FrameUploadBuffer upload = instance_->frameUploadBuffer_;`（`&` なし）と書くと、コンパイルエラーになることを確認するとよい（確認したら戻す）。

> エンジンの全67個の `.cpp` をDebug / Releaseでコンパイルして通ることを確認済み（`Light.md` Step 5.5の変更と一緒に確認）。

### 確認すること
1. エンジンのビルド → ゲームのビルドが通る
2. 実行して、見た目が変わらない
