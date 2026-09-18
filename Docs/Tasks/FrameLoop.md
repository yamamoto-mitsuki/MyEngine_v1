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


> 整理済み：コミット済みの旧写経コードを省略。実装と旧手順はコミット `936cdcf` を参照。以下は設計理由・確認項目の記録で、再適用する手順ではない。

<details>
<summary>過去の変更と解説を開く</summary>

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

### ② `MyEngine/Graphics/GPU/FrameUploadBuffer.cpp`（新規）

### ③ `MyEngine/Graphics/Renderer/RenderContext.h`（ファイル全体を差し替え）

### ④ `MyEngine/Graphics/Renderer/RenderContext.cpp`（ファイル全体を差し替え）

### ⑤ `MyEngine/Window/WindowManager.cpp`

**`PostRenderAll` の中**：1行差し替える

変更前
変更後

### ⑥ `MyEngine/Engine.cpp`（任意）

**`EndFrame` の中**：追加する（`DirectXCommon::ResetDrawCallCount();` の下、`#endif` の上）

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

</details>

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
