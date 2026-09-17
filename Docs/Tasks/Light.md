# Light系の作業

## このファイルについて
- Light周りの「作業手順」と「決めたこととその理由」を書く。話題ごとに1ファイルで、**終わっても消さずに更新し続ける**。
- 終わったStepで消してよいのは**写経用のコードだけ**。「なぜ」は「決めたこと」の表へ、「次にやること」は該当Stepへ移してから消す。
- **消すのはコミットしてから**（コミット前に消すとコードがどこにも残らない）。
- 会話で決まったことは「決めたこと」の表に追記する。ここが記憶の本体になる。
- 各Stepの確認は **エンジンのビルド → ゲームのビルド → 実行して「確認すること」** の3段階で行う（エンジンは静的ライブラリなので、エンジンだけのビルドではリンクエラーやゲーム側の食い違いが出ない）。

---

## 進捗

| Step | 内容 | 状態 |
|---|---|---|
| 0 | vcxprojのLightManager登録の修正、`instance_` の定義 | 完了 |
| 1 | Componentデータ（padding無し）とGPU用構造体を分ける | 完了 |
| 2 | PointLightから非データを外に出す ＋ ギズモ（ライトごと＋全体の表示設定） | 完了（ゲーム側での表示確認待ち） |
| 3 | GPUバッファの持ち主をRenderContextに一本化 | 完了 |
| 4 | LightManagerをHandle / SlotMapで管理。ギズモの呼び出しもLightManagerがまとめて行う | 完了 |
| **5** | **描画データの収集をフレーム1回に（sRGB→リニア変換、方向の正規化もここ）。Rendererの設定からライトのポインタを無くす** | **作業中** |
| 5.1 | ライトの定数バッファを全描画で共有する（描画ごとのコピーをやめる） → 設置数の上限を引き上げ | これから |
| 5.5 | スポットライトの追加（ギズモの `AddSpotLight` も） | これから |
| 6 | ライトのComponentをInspector / Add Componentに対応させる | `Editor.md` の作業の後 |

その後の予定：HDR化 → 描画サイズの可変 → 影（シャドウマップ） →（必要なら）エリアライト

---

## 決めたこと

| 決めたこと | 理由 |
|---|---|
| Componentの色は `Vector3`（アルファ無し） | シェーダーは `.rgb` しか使わない。GPU構造体側で `a = 1` を入れるのでHLSLは変えなくて済む |
| 色は0〜1、明るさは `intensity` に分ける | 色に明るさを混ぜない。HDR化しても、色は普通のカラーピッカーのままで済む |
| Componentが持つ色は「見た目の色（sRGB）」 | 保存ファイルもピッカーの見た目と一致する。リニアへの変換はStep 5の収集時に1か所で行う |
| 色を16進数で持たない | ImGuiは `float*` を直接編集する。8bitではHDRを表現できない。16進数はコードから色を書くときの入力用にとどめる |
| GPU用構造体（padding入り）はInspectorに出さない | paddingはGPUの都合。Componentはデータだけを持つ（ARCHITECTURE.md 原則2） |
| ライトのGPUバッファはRenderContextだけが持つ | 現状は3か所にあるが、実際にバインドされているのはRenderContextのリングバッファだけ |
| `GetData()` は「その場でComponentから作って値で返す」 | バッファからの読み返しをやめると、`Initialize` / `Update` の呼び忘れによる不具合が無くなる。32〜48バイトで、どうせ `MeshRequest` に値でコピーされるので参照で返す得も無い |
| ライトの種類を増やすのはStep 5の後 | 今の「描画1回ごとにライトを丸ごとコピー」の構造のまま足すと、Step 5で配線を書き直すことになる |
| 上限の引き上げもStep 5の後 | 今上げると、1描画あたりのコピー量とリングバッファ（現在約4MB）がそのまま倍々に増える |
| エリアライトは当面やらない | 正しくやるとLTCが必要。効果が出るのはHDRの後で、見た目の向上なら影のほうが先 |
| ライトの種類ごとに別のComponentにする（Add ComponentでPoint / Spotを選ぶ） | 種類ごとにデータの形が違う。型別の配列で持つ設計（ARCHITECTURE.md 段階2〜3）と相性が良い。種類を変えるときは外して付け直す |
| ギズモの表示設定はComponentの中に持つ（案A） | Handleが無くても今すぐ作れる。Inspectorで他の値と一緒に編集・保存できる（Unrealもエディタ専用データをコンポーネント内に持つ）。Entityができたら「エディタで隠す」はEntity単位へ移す |
| 表示は「全体の設定 AND ライトごとの設定」 | 全部まとめて消す操作と、1個だけ消す操作の両方ができる |
| 「ギズモを隠す」と「ライトを消す（照らさない）」は別のフラグ | 混ぜると「見えないけど照らしている」状態を作れない |
| 範囲の線は全ライト分を1回の `DrawLines` にまとめる | ライトごとに描くとドローコールが増える。エディタは同じキューをScene/Gameで2回描くので倍になる（原則3） |
| 全体の表示設定は `static` で持つ | 1つしか無い情報なので自然。前の設計の問題は `static` ではなく、フラグが全体の1つしか無かったこと |
| `PointLightComponent::position` は今は持つ | Entity / TransformComponentができたらTransformから取るようにして外す |
| SlotMapは「生きている要素を隙間なく詰めて並べる」方式 | `for` でそのまま全部回せて、途中に死んだ要素が混ざらない。連続して並ぶのでARCHITECTURE.md 段階3の「型別の連続配列」にそのまま使える。削除も末尾との入れ替えで一瞬 |
| `Get` で受け取ったポインタは使い捨て | 追加・削除で要素の場所が動く（配列の伸長、末尾との入れ替え）。フレームをまたいで持つのはHandleだけ（原則1） |
| ライトの削除は予約して、更新の最後にまとめて反映 | ARCHITECTURE.md「生成、破棄の規約」。更新の途中で消すと、同じフレームで使っているポインタが別のライトを指してしまう |
| ライトの追加はその場で反映 | 追加直後に値を設定したいため。その代わり「追加したら、それより前に受け取ったポインタは使わない」 |
| 平行光源はLightManagerが1つだけ持つ（Handle無し） | 普通はシーンに1つ。Entity化して複数置けるようになったら「どれをメインにするか」のルールを決める |
| LightManagerは全ウィンドウ共通（ParticleManagerと同じ） | 既存の管理クラスに合わせる。ウィンドウごとに別のライトを持つのは、シーン（Entity）がライトを持つようになってから |
| ギズモは `WindowManager::DrawAll` の `USE_IMGUI` の中で描く | 製品版（Release）ではギズモを出さない |
| シーンが追加したライトは、シーンの `Finalize` で削除する | Stop → Playでシーンが作り直されるたびに `Initialize` で追加されるので、消さないと増え続ける |
| `LightManager::Release` はウィンドウ（シーン）の破棄より後 | シーン側がライトを消そうとしたときに、LightManagerが先に無くなっていると落ちる |
| 無効値に `std::numeric_limits<T>::max()` を使わない | `Windows.h` の `max` マクロとぶつかる。`0xFFFFFFFF` で書く（エンジンの他の無効値と同じ） |
| `DirectionalLight` / `PointLight` クラスは消して、Componentを直接持つ（Step 5） | 変換処理をLightManagerに移すと、クラスはComponentを包むだけの殻になる。ARCHITECTURE.md 段階3の「型別のデータ配列」に近づく |
| ライトはRendererの設定で指定しない。Unlit以外は全部LightManagerのライトで照らす（Step 5） | 描画ごとにポインタを渡す必要が無くなる（原則1）。Unityと同じく「シーンのライトは全部に効く」 |
| 依存の向きは「Light → Graphics」。RendererはLightManagerを知らない（Step 5） | LightManagerが `Renderer::SetFrameLights` で渡す。Graphicsが上位のモジュールに依存すると、循環しやすくなる |
| ライトの値の変更は `Update` の中で行う（Step 5） | 収集は更新の最後に1回。`Draw` の中で変えた値は次のフレームから反映される |
| 上限を超えたポイントライトは「SlotMapの並び順」で16個まで（Step 5） | まずは単純に。削除で並びが入れ替わるので、どれが選ばれるかは保証しない。カメラからの距離で選ぶなどは、上限引き上げ（5.1）の後に必要なら考える。超えたときは警告を1回だけ出す |

## 未解決
- **ギズモがGameビューにも出る。さらにギズモ自体にBloom / Lensがかかる**：RenderQueueに「Sceneビューだけ」という区別が無く、ギズモがポストエフェクトより前に描かれている。Renderer側の対応が必要 → `Editor.md`（GameビューにBloomがかかること自体は正しい）

---

## Step 2：PointLightから非データを外に出す ＋ ギズモ

### 目的
1. `PointLight` から、ARCHITECTURE.md 原則2の「データではないもの」を全部外に出す。

| 外に出すもの | 理由 |
|---|---|
| `Camera* camera_` | ポインタ。フレームをまたいで保持しない（原則1） |
| `Renderer::Rect3dConfig rectConfig_` | `std::wstring` とポインタを含む描画設定。データではない |
| `uint32_t textureHandle_` | 表示のための資源。ライトの状態ではない |
| `Initialize()` / `Update()` / `Draw()` | 振る舞い |

2. 行き先は `LightGizmo`（エディタ表示専用）。アイコンと、光の届く範囲のワイヤーを出す。
3. 表示のON/OFFを**全体**と**ライトごと**の両方で持つ。アイコンと線は別々に切り替えられる。

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Light/LightComponent.h` | ギズモの表示フラグを追加 |
| `MyEngine/Light/LightGizmo.h` | 新規（前回の版から全面的に書き直し） |
| `MyEngine/Light/LightGizmo.cpp` | 新規（前回の版から全面的に書き直し） |
| `MyEngine/Light/PointLight.h` | 非データを削除 |
| `MyEngine/Light/PointLight.cpp` | 非データを削除 |
| `MyEngine/Light/LightIncludes.h` | 追加したヘッダを登録 |
| `MyEngine/Engine.cpp` | `LightGizmo::Initialize()` を1行追加 |

> VSで新しいファイルを作るときは、保存場所を `MyEngine\Light\` に変えること。既定だとプロジェクト直下に作られる。

---

### 呼び出し方（Step 4でLightManagerの中に移すまでの仮）
**書く場所：ゲーム側のプロジェクトの、ライトを持っているシーンの `Draw()` の中。**
- MyEngineは静的ライブラリ（`.lib`）で、このリポジトリにはシーンの実装が無い。ギズモを出すには、ゲーム側でライトを持ち、描画のタイミングで `LightGizmo` に渡す必要がある。
- Step 4でLightManagerが全ライトを持つようになったら、LightManagerの中で自動的に呼ぶので、ゲーム側のこのコードは消す。

ゲーム側のシーン（例：`GameScene`）のヘッダ
```cpp
#include <vector>

#include "MyEngine/Light/LightIncludes.h"

class GameScene : public IScene {
	// （今あるメンバはそのまま）
private:
	std::vector<PointLight> pointLights_; // 表示確認用のポイントライト
};
```

`Initialize()` の中（置く場所と設定）
```cpp
	pointLights_.resize(2);
	pointLights_[0].GetComponent().position = {0.0f, 2.0f, 0.0f};
	pointLights_[1].GetComponent().position = {6.0f, 2.0f, 0.0f};
	pointLights_[1].GetComponent().radius = 4.0f;
	pointLights_[1].GetComponent().gizmo.showIcon = false; // 2個目はアイコンだけ消して確認
```

`Draw()` の中（他の描画と同じ場所でよい）
```cpp
	LightGizmo::Begin(GetCamera()); // シーンで使っているカメラ
	for (PointLight& light : pointLights_) {
		LightGizmo::AddPointLight(light.GetComponent());
	}
	LightGizmo::End();
```
- `Renderer::Draw〇〇` は「描画のお願いをキューに積む」だけなので、`Draw()` のどこで呼んでもよい。実際に描かれるのはフレームの最後。
- カメラは `Begin` から `End` の間だけ使われ、`End` で手放される（フレームをまたいで持たない）。
全体の表示を切り替えるときは次のように書く。
```cpp
LightGizmo::GetGlobalFlags().showRange = false; // 全ライトの線を消す
```
1個だけ切り替えるときは次のように書く。
```cpp
light.GetComponent().gizmo.showIcon = false; // このライトのアイコンだけ消す
```

---

### 解説

**表示の判定が「全体 AND ライトごと」の理由**
- 全体をOFFにすると、ライトごとの設定に関係なく全部消える。全体をONに戻すと、ライトごとの設定が生きたまま復帰する。
- ライトごとの設定を書き換えずに一括で消せるので、全体をON/OFFしても個別の設定が失われない。

**Begin / Add / End に分けた理由**
- 線は「全ライト分を1つの配列に積んで、最後に1回だけ `DrawLines`」にしたい。そのため「積み始め」と「描く」のタイミングを分ける必要がある。
- 描画リクエストは線1回＋アイコンがライトの数だけ。前回の「ライトごとに線を1回ずつ」より、ライトが増えるほど差が大きくなる。
- アイコン（`DrawRect3d`）はRenderer側にまとめて描く仕組みが無いので、今はライトごとのまま。

**`camera_` を `static` で持っても原則1に反しない理由**
- 原則1で禁止されているのは「フレームをまたいで保持すること」。`End()` で `nullptr` に戻すので、同じフレームの中だけで完結する一時的な使用に当たる。
- `isRecording_` とアサートは、`Begin` / `End` の呼び忘れをすぐ見つけるためのもの。

**`lines.clear()` でメモリを使い回す理由**
- `std::vector::clear()` は中身を消すが、確保済みのメモリ（capacity）は残す。前回の設計ではライトごとに毎フレーム配列を作り直していたので、そのたびにメモリ確保が起きていた。
- 2フレーム目以降は、ライトの数が増えない限り確保が起きない。

**円を3枚描くと球に見える理由**
- XY・YZ・ZXの3平面に円を描くと、どの方向から見ても輪郭に近い円が必ず1つは見える。球のワイヤーを全部描くより線がずっと少ない（1ライトあたり32分割 × 3枚 = 96本）。

**線の色について**
- `Renderer::DrawLines` は「線ごとの色 × 全体の色」で最終的な色を出す。`LineListConfig::color` は既定が白（`0xFFFFFFFF`）なので、線ごとの色がそのまま出る。
- 色は `0xRRGGBBAA` の並び。

---

### 確認すること
- ビルドが通る
- ライトの位置にアイコンが出て、`radius` の大きさの球状のワイヤーが見える
- `radius` を変えるとワイヤーの大きさが変わる
- `GetGlobalFlags().showRange = false` で全ライトの線が消え、`true` に戻すと元に戻る
- 1個のライトの `gizmo.showIcon = false` で、そのライトのアイコンだけ消える
- （既知の問題）Gameビューにもギズモが出る → 未解決の欄を参照

### 次のStepでやること（ここではやらない）
- Step 4：`Begin` / `AddPointLight` / `End` の呼び出しをLightManagerの中に移し、全ライトをまとめて回す
- Step 5.5：`AddSpotLight`（円錐のワイヤー）を追加。`LightGizmoFlags` はそのまま使い回す
- Step 6：Inspectorに、値・アイコン・線の設定欄を出す。全体の設定もUIにする

---

## Step 3：GPUバッファの持ち主をRenderContextに一本化

### 目的
ライト用のGPUバッファが3か所にあるのを、実際に使われている `RenderContext` だけにする。

| 場所 | 持っているもの | 使われているか |
|---|---|---|
| `DirectionalLight` | `lightBuffer_`、`mappedPtr_` | シェーダーには結ばれていない。値を置いて読み返すだけ |
| `LightManager` | `directionlLightBuffer_`、`pointLightBuffer_` など | 使われていない（`Initialize` もどこからも呼ばれていない） |
| `RenderContext` | `directionalLightDataRingBuffer_`、`pointLightDataRingBuffer_` | **これだけが実際にシェーダーに結ばれている** |

### ライトのデータがGPUに届くまで（今の流れ）
1. ゲームがComponentの値を書き換える
2. `Renderer::DrawModel` などが `DirectionalLight::GetData()` を呼び、`MeshRequest::directionalLightData` に**値でコピー**する
3. `MeshRequest` が `RenderQueue` に積まれる
4. フレームの最後に `RenderContext::DrawMesh` が、リングバッファの「この描画1回分の場所」に `memcpy` する
5. `SetGraphicsRootConstantBufferView` で、その場所をシェーダーの `b2`（`gDirectionalLight`）に結ぶ

`DirectionalLight` が持っていたバッファは、この流れの**どこでもシェーダーに結ばれていない**。2で値を取り出すための置き場として使われていただけ。

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Light/DirectionalLight.h` | GPUバッファと `Initialize` / `Update` を削除。`GetData()` をPointLightと同じ形にする |
| `MyEngine/Light/DirectionalLight.cpp` | 同上 |
| `MyEngine/Light/LightManager.h` | GPUバッファとテクスチャハンドルを削除 |
| `MyEngine/Light/LightManager.cpp` | 同上。`Initialize` / `Release` を正しい形にする |
| ゲーム側のシーン | `DirectionalLight` の `Initialize()` / `Update()` を呼んでいたら消す |

`RenderContext` は変更しない（すでに正しく持っている）。

---

### ⑤ ゲーム側のシーン
- `DirectionalLight` の `Initialize()` と `Update()` を呼んでいる行を消す（関数が無くなるのでビルドエラーになる）。
- 値の変更は `GetComponent()` 経由で行う（Step 1から同じ）。

---

### 解説

**`DirectionalLight` のバッファを消してよい理由**
- 上の「GPUに届くまで」の通り、シェーダーに結ばれているのはRenderContextのリングバッファだけ。
- 描画1回ごとに別の場所へコピーする方式なので、ライト側が自分用のバッファを1つ持っていても使い道が無い。

**`GetData()` を「読み返し」から「その場で作る」に変えた理由**
- 前は、GPU用のバッファ（Uploadヒープ）に書いた値をCPUが読み返していた。Uploadヒープは「CPUが書いてGPUが読む」ためのメモリで、CPUから読むと遅くなることがある。
- `Initialize()` を呼び忘れると、`mappedPtr_` がnullptrのまま `GetData()` でクラッシュしていた。
- `Update()` を呼び忘れると、Componentを変えても古い値のまま描かれていた。
- その場で作る形にすると、この3つが全部無くなる。PointLightと同じ形になるので、Step 4で2種類をまとめて扱いやすくなる。

**値で返す（コピーする）コスト**
- `DirectionalLightData` は32バイト。どのみち `MeshRequest` に値でコピーされるので、ここで参照を返しても得は無い。

**LightManagerの `Initialize` を直した理由**
- 前の `Initialize` は `instance_` を作らずに `instance_->` を使っていたので、呼ぶとクラッシュする状態だった（どこからも呼ばれていなかったので表に出ていなかった）。
- `SceneRenderer` と同じ「`Initialize` で `new`、`Release` で `delete`」の形にそろえる。`Engine.cpp` から呼ぶのはStep 4。
- `Update` / `Draw` / `AddPointLight` は宣言だけ残している。どこからも呼ばれていないのでリンクエラーにはならない。中身はStep 4で作る。

---

### 確認すること
- エンジンとゲームの両方でビルドが通る
- 平行光源とポイントライトの当たり方が前と変わらない
- 実行中にComponentの色・強さ・向きを変えると、`Update()` を呼ばなくてもすぐ反映される
- （任意）PIXなどでGPUリソースの一覧を見ると、`DirectionalLight_CB` が無くなっている

### 次のStepでやること（ここではやらない）
- Step 4：LightManagerにライトを持たせる（`Handle` / `SlotMap`）。`SlotMap` の中身の実装もここでやる。ギズモの呼び出しもゲーム側からLightManagerへ移す。

---

## Step 4：LightManagerをHandle / SlotMapで管理する

### 目的
1. ライトの実体を**LightManagerだけ**が持つ。ゲーム側は `Handle`（引換券）だけを持つ（ARCHITECTURE.md 原則1）。
2. `SlotMap` の中身を実装する（今は宣言だけ）。
3. ギズモの描画を、ゲーム側からLightManagerへ移す（原則3「全部に対して書く」）。
4. ライトの削除を「予約 → 更新の最後にまとめて反映」にする（ARCHITECTURE.md「生成、破棄の規約」）。

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Core/SlotMap.h` | 中身を実装（ファイル全体を差し替え） |
| `MyEngine/Light/LightManager.h` | ファイル全体を差し替え |
| `MyEngine/Light/LightManager.cpp` | ファイル全体を差し替え |
| `MyEngine/Engine.cpp` | `LightManager` の `Initialize` / `Release` を追加 |
| `MyEngine/Window/WindowManager.cpp` | `LightManager::Update` / `DrawGizmos` を追加 |
| ゲーム側のシーン | 自分で持っていたライトをやめて、LightManagerを使う |

`Core/Handle.h` は今のままで変更なし。

> `SlotMap.h` と `LightManager.h` / `.cpp` は、エンジンの実際のヘッダでコンパイルが通ることを確認済み（`/W4` で警告なし）。`SlotMap` は作成・削除・再利用・古いHandleの無効化・`for` で回す動作もテスト済み。

---

### ⑥ ゲーム側のシーン

**消すもの（Step 2の確認用に書いたもの）**
- `std::vector<PointLight> pointLights_;`
- `Draw()` の中の `LightGizmo::Begin` 〜 `End`（LightManagerが自動で描くようになる）
- シーンが自分で持っている `DirectionalLight` があれば、それも消す

ヘッダ
```cpp
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Light/LightManager.h"

class GameScene : public IScene {
	// （今あるメンバはそのまま）
private:
	std::vector<Handle<PointLight>> pointLightHandles_; // このシーンが追加したライト
};
```

`Initialize()`
```cpp
	// 平行光源（LightManagerが1つだけ持っている）
	DirectionalLightComponent& sun = LightManager::GetDirectionalLight()->GetComponent();
	sun.color = {1.0f, 1.0f, 1.0f};
	sun.intensity = 1.0f;

	// ポイントライト：追加して、Handleで取り出して値を設定する
	Handle<PointLight> handle = LightManager::AddPointLight();
	if (PointLight* light = LightManager::GetPointLight(handle)) {
		light->GetComponent().position = {0.0f, 2.0f, 0.0f};
		light->GetComponent().radius = 6.0f;
	}
	pointLightHandles_.push_back(handle);
```

`Draw()`（モデルなどを描く直前に、毎フレーム設定する）
```cpp
	config.directionalLight = LightManager::GetDirectionalLight();
	config.pointLights = LightManager::GetPointLightPointers(); // Step 5で消える
	Renderer::DrawModel(config);
```

`Finalize()`
```cpp
	for (Handle<PointLight> handle : pointLightHandles_) {
		LightManager::RemovePointLight(handle);
	}
	pointLightHandles_.clear();
```

途中でライトを動かすとき（`Update()` など）
```cpp
	if (PointLight* light = LightManager::GetPointLight(pointLightHandles_[0])) {
		light->GetComponent().position.x += 0.1f;
	}
```

---

### 解説

**Handleとは（引換券）**
- ゲーム側はライトの実体（ポインタ）ではなく、`Handle`（`index` と `generation` の2つの数字）を持つ。
- 使うときに毎回 `GetPointLight(handle)` で「引き換える」。消えていれば `nullptr` が返るので、**消えたライトを触って落ちることが無い**。

**SlotMapの中身（「複雑な実装」なので図で説明）**

3つの配列を持つ。
| 配列 | 役割 |
|---|---|
| `dense_` | ライトの実体。生きているものだけが**隙間なく**並ぶ |
| `slots_` | 台帳。`Handle.index` 番目の行に「`dense_` の何番目にいるか」と「世代」を書く |
| `denseToSlot_` | 逆引き。`dense_` の i 番目が台帳の何行目のものか |

例：A・B・Cを作って、Aを消す。

作った直後
| dense_ | [0] A | [1] B | [2] C |
|---|---|---|---|
| **slots_** | 行0: dense=0, 世代1 | 行1: dense=1, 世代1 | 行2: dense=2, 世代1 |

Handleは A={0,1}、B={1,1}、C={2,1}。

Aを消す → **末尾のCを、Aがいた場所に移す**
| dense_ | [0] C | [1] B | |
|---|---|---|---|
| **slots_** | 行0: 空き, **世代2** | 行1: dense=1, 世代1 | 行2: **dense=0**, 世代1 |

- Cは `dense_` 上で2番から0番に動いたが、台帳の行2を書き換えたので、Handle C={2,1} は**そのまま使える**。
- 行0は世代が2に進んだので、古いHandle A={0,1} は「世代が合わない」→ `nullptr`。
- 次に作るライトは空いた行0を再利用し、Handle={0,2} になる。古いAのHandleと区別できる。

**なぜ「末尾と入れ替え」で消すのか**
- 途中を消して後ろを全部詰めると、要素の数だけ移動が起きる。末尾と入れ替えれば移動は1回で済む。
- 隙間が無いので、`for (PointLight& light : pointLights_)` で生きているライトだけを回せる（ギズモや、Step 5の収集で使う）。
- 代わりに**並び順は保たれない**。ライトの順番に意味を持たせないこと。

**なぜ `Get` のポインタは使い捨てなのか**
- `Create` で `dense_` が伸びるとき、配列ごと別の場所に引っ越すことがある。
- `Destroy` で末尾の要素が別の場所に移る。
- どちらも、前に受け取ったポインタが「別のライト」や「解放済みのメモリ」を指すことになる。なので、**フレームをまたいで持つのはHandleだけ**。

**なぜ削除は予約なのか**
- シーンの `Update` の途中で消すと、その場で末尾のライトが移動する。同じフレームの中で、そのライトのポインタを使っている処理があると壊れる。
- 予約しておき、全員の更新が終わった後（`WindowManager::UpdateAll` の最後）にまとめて消す。ARCHITECTURE.mdの「フレームの更新順序」の6に当たる。
- 同じHandleを2回予約しても、`Destroy` が「もう消えていたら何もしない」ので安全。

**`GetPointLightPointers` が仮の関数である理由**
- 今のRendererの設定は `std::vector<PointLight*>*` を受け取る形なので、それに合わせた橋渡し。
- 呼ばれるたびに作り直すので、**描画の直前に毎フレーム呼ぶ**こと（設定をメンバ変数に持っていて、`Initialize` で1回だけ代入していると、ライトの追加・削除の後に古いポインタが残る）。
- Step 5でRendererがLightManagerから直接データを受け取るようになったら消す。

**ギズモを `WindowManager` で描く理由**
- パーティクル（`ParticleManager::Draw`）と同じ場所・同じカメラの選び方にそろえた。
- `#ifdef USE_IMGUI` の中なので、製品版ではギズモの処理自体が入らない。

**シーンの `Finalize` でライトを消す理由**
- Stop → Playでシーンが作り直されると、`Finalize` → 新しいシーンの `Initialize` が呼ばれる。消さないと、そのたびにライトが増えていく。

---

### 確認すること
1. **エンジンのビルド**が通る
2. **ゲームのビルド**が通る（リンクエラーが無い）
3. **実行して確認**
   - ライトの当たり方とギズモの表示が前と同じ
   - Stop → Playを何回か繰り返しても、ライト（ギズモ）が増えない
   - `Update()` でライトを動かすと、ギズモと当たり方が一緒に動く
   - （任意）`RemovePointLight` した後のHandleで `GetPointLight` すると `nullptr` になる

### 次のStepでやること（ここではやらない）
- Step 5：LightManagerが1フレームに1回、ライトをGPU用の形にまとめる（sRGB→リニア変換、方向の正規化もここ）。Rendererの設定から `directionalLight` / `pointLights` のポインタを無くし、`GetPointLightPointers` を消す。

---

## Step 5：描画データの収集をフレーム1回にする

> **始める前に**：Step 4の `SlotMap.h` の修正（`<limits>` を消して `kInvalidIndex = 0xFFFFFFFF`）が入っていること（2026-09-17時点で反映済みを確認）。

### 目的
1. LightManagerが1フレームに1回、全ライトをGPU用の形（`DirectionalLightData` / `PointLightListData`）にまとめる。
   - 色をsRGB（見た目の色）→ リニアに変換する
   - 平行光源の向きを正規化する（`(0,0,0)` なら真下にする）
   - ポイントライトを上限（16個）まで詰める
2. まとめたデータを `Renderer::SetFrameLights` で渡す。Rendererの7つの設定（Config）から `directionalLight` / `pointLights` を削除する。
3. `DirectionalLight` / `PointLight` クラスを削除し、LightManagerがComponentを直接持つ。
4. Step 4の仮の関数 `GetPointLightPointers` を削除する。

### Step 5の後のデータの流れ
1. ゲームが `Update()` でComponentの値を書き換える
2. `WindowManager::UpdateAll` の最後で `LightManager::Update()`
   1. `CollectForGPU`：GPU用の形にまとめて `Renderer::SetFrameLights` へ渡す（ARCHITECTURE.md 更新順序の5）
   2. `FlushRemovals`：削除予約を反映する（更新順序の6）
3. シーンの `Draw()` → `Renderer::DrawModel` などが、Rendererが持っている「このフレームのライト」を `MeshRequest` にコピーする
4. `RenderContext` がリングバッファに書いてシェーダーに結ぶ（変更なし）

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Light/LightManager.h` | ファイル全体を差し替え |
| `MyEngine/Light/LightManager.cpp` | ファイル全体を差し替え |
| `MyEngine/Light/LightIncludes.h` | ファイル全体を差し替え |
| `MyEngine/Light/DirectionalLight.h` / `.cpp` | **削除** |
| `MyEngine/Light/PointLight.h` / `.cpp` | **削除** |
| `MyEngine/Graphics/Renderer/Renderer.h` | 前方宣言を削除、7つのConfigから2行ずつ削除、`SetFrameLights` とメンバを追加 |
| `MyEngine/Graphics/Renderer/Renderer.cpp` | includeと警告関数を削除、ライトを詰める部分を差し替え、`SetFrameLights` を追加 |
| `MyEngine/Graphics/Model/ModelManager.h` | 使われていない前方宣言 `class DirectionalLight;` を削除（任意） |
| ゲーム側のシーン | 型名の置き換え、Configのライト指定を削除 |

`LightComponent.h`、`LightGizmo`、`SlotMap.h`、`Engine.cpp`、`WindowManager.cpp` はStep 4のままで変更なし。

> ファイルの削除は、VSのソリューションエクスプローラーで「削除」→「削除（ファイルも消す）」を選ぶ。「プロジェクトから除外」だけだとファイルが残る。

> この手順のコード（LightManager、Renderer、LightGizmo、ゲーム側の使い方）は、2026-09-17時点のエンジンの実際のヘッダ（Step 4完了後）でコンパイルが通ることを確認済み（`/W4`、`Windows.h` を先に読み込む順番でも確認）。Rendererの `DrawModel` で出る `r,g,b,a` 未使用の警告（C4189）は、この変更の前からあるもの。

---

### ① `MyEngine/Light/LightManager.h`（ファイル全体を差し替え）
```cpp
#pragma once
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Light/LightComponent.h"

// 前方宣言
class Camera;


/// <summary>
/// ライトの管理。ライトのComponentはここだけが持ち、外にはHandleを渡す
/// <para>GPUバッファは持たない。GPU用の形にまとめてRendererへ渡すまでが役目</para>
/// </summary>
class LightManager {
public:
	static void Initialize();
	static void Release();

	/// <summary>
	/// 更新の最後に呼ぶ
	/// <para>1. このフレームのライトをGPU用の形にまとめてRendererへ渡す</para>
	/// <para>2. 削除予約されたライトをまとめて消す</para>
	/// </summary>
	static void Update();

	/// <summary>
	/// 全ライトのギズモを描く（エディタ用）
	/// </summary>
	static void DrawGizmos(Camera* camera);

	// ===== 平行光源（1つだけ） =====
	static DirectionalLightComponent& GetDirectionalLight() { return instance_->directionalLight_; }

	// ===== ポイントライト =====
	static Handle<PointLightComponent> AddPointLight();

	/// <summary>
	/// 削除を予約する。実際に消えるのは Update のとき
	/// </summary>
	static void RemovePointLight(Handle<PointLightComponent> handle);

	/// <summary>
	/// Handleからライトを取り出す。消えていれば nullptr
	/// <para>受け取ったポインタは使い捨てにする（メンバ変数に保存しない）</para>
	/// </summary>
	static PointLightComponent* GetPointLight(Handle<PointLightComponent> handle);

private:
	static LightManager* instance_;

	void CollectForGPU(); // Component → GPU用データにまとめてRendererへ渡す
	void FlushRemovals(); // 削除予約を反映する

	DirectionalLightComponent directionalLight_;
	SlotMap<PointLightComponent> pointLights_;
	std::vector<Handle<PointLightComponent>> pendingRemovePointLights_; // 削除予約
	bool hasWarnedPointLightLimit_ = false;                             // 上限超えの警告を1回だけ出す
};
```

### ② `MyEngine/Light/LightManager.cpp`（ファイル全体を差し替え）
```cpp
#include "LightManager.h"

#include <cmath>
#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Light/LightGizmo.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 静的メンバ変数
LightManager* LightManager::instance_ = nullptr;

namespace {
/// <summary>
/// sRGB（見た目の色）→ リニア（ライティング計算用）。1成分分
/// <para>GPUが _SRGB 形式のテクスチャを読むときと同じ式</para>
/// </summary>
float SrgbToLinear(float c) {
	if (c <= 0.04045f) {
		return c / 12.92f;
	}
	return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/// <summary>
/// sRGBの色(RGB) → リニアの色(RGBA)。アルファはシェーダーで使わないので1固定
/// </summary>
Vector4 SrgbToLinear(const Vector3& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), 1.0f}; }
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void LightManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new LightManager();
}

void LightManager::Release() {
	delete instance_;
	instance_ = nullptr;
}


//=============================================================================
// 更新
//=============================================================================
void LightManager::Update() {
	instance_->CollectForGPU(); // ARCHITECTURE.md フレームの更新順序 5. 描画データの収集
	instance_->FlushRemovals(); // ARCHITECTURE.md フレームの更新順序 6. 破棄の反映
}

void LightManager::CollectForGPU() {
	// --- 平行光源 ---
	DirectionalLightData directional;
	directional.color = SrgbToLinear(directionalLight_.color);
	directional.intensity = directionalLight_.intensity;
	// 向きが(0,0,0)だとシェーダーのnormalizeで壊れるので、真下にしておく
	if (LengthSq(directionalLight_.direction) > 1e-6f) {
		directional.direction = Normalize(directionalLight_.direction);
	} else {
		directional.direction = {0.0f, -1.0f, 0.0f};
	}

	// --- ポイントライト（先頭から上限まで） ---
	PointLightListData pointList;
	uint32_t count = 0;
	for (const PointLightComponent& light : pointLights_) {
		if (count >= kMaxPointLights) {
			if (!hasWarnedPointLightLimit_) {
				LogManager::Warning(std::format("ポイントライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxPointLights));
				hasWarnedPointLightLimit_ = true;
			}
			break;
		}
		PointLightData& data = pointList.lights[count];
		data.color = SrgbToLinear(light.color);
		data.position = light.position;
		data.intensity = light.intensity;
		data.radius = light.radius; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		++count;
	}
	pointList.count = count;

	Renderer::SetFrameLights(directional, pointList);
}

void LightManager::FlushRemovals() {
	for (Handle<PointLightComponent> handle : pendingRemovePointLights_) {
		pointLights_.Destroy(handle);
	}
	pendingRemovePointLights_.clear();
}


//=============================================================================
// ギズモ
//=============================================================================
void LightManager::DrawGizmos(Camera* camera) {
	LightGizmo::Begin(camera);
	for (const PointLightComponent& light : instance_->pointLights_) {
		LightGizmo::AddPointLight(light);
	}
	LightGizmo::End();
}


//=============================================================================
// ポイントライト
//=============================================================================
Handle<PointLightComponent> LightManager::AddPointLight() { return instance_->pointLights_.Create(); }

void LightManager::RemovePointLight(Handle<PointLightComponent> handle) { instance_->pendingRemovePointLights_.push_back(handle); }

PointLightComponent* LightManager::GetPointLight(Handle<PointLightComponent> handle) { return instance_->pointLights_.Get(handle); }
```

### ③ `MyEngine/Light/LightIncludes.h`（ファイル全体を差し替え）
```cpp
#pragma once
#include "MyEngine/Light/LightComponent.h"
#include "MyEngine/Light/LightGizmo.h"
#include "MyEngine/Light/LightManager.h"
```
- 今は `LightManager.h` が `LightIncludes.h` を読み、`LightIncludes.h` も `LightManager.h` を読む「お互いにinclude」の状態になっている（`#pragma once` のおかげで動いているだけ）。①で `LightManager.h` は `LightComponent.h` だけを読むようにしたので、この循環も無くなる。

### ④ `MyEngine/Graphics/Renderer/Renderer.h`

**削除する**：前方宣言の2行
```cpp
class DirectionalLight;
class PointLight;
```

**削除する**：次の2行を、`ModelConfig` / `TriangleConfig` / `SphereConfig` / `Rect3dConfig` / `Quad3dConfig` / `AABBConfig` / `OBBConfig` の**7か所すべて**から消す（Ctrl+Fで `directionalLight` を検索すると見つけやすい）
```cpp
		DirectionalLight* directionalLight = nullptr;              // 平行光源設定
		std::vector<PointLight*>* pointLights = nullptr;           // ポイントライト設定
```

**追加する**：`static void DrawLines(const LineListConfig& config);` の下（`private:` の上）
```cpp
	//=============================================================================
	// ライト
	//=============================================================================
	/// <summary>
	/// このフレームで使うライトを設定する（LightManagerが1フレームに1回呼ぶ）
	/// <para>Unlit以外の描画は、すべてこのライトで照らされる</para>
	/// </summary>
	static void SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights);
```

**追加する**：`private:` の中の `static Renderer* instance_;` の下
```cpp
	// このフレームのライト（SetFrameLightsで設定される）
	DirectionalLightData frameDirectionalLight_{};
	PointLightListData framePointLights_{};
```

### ⑤ `MyEngine/Graphics/Renderer/Renderer.cpp`

**削除する**：includeの4行（ライトの2行と、警告関数でしか使っていなかった2行）
```cpp
#include <unordered_set>
#include <externals/magic_enum/magic_enum.hpp>
#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Light/PointLight.h"
```

**削除する**：警告関数を丸ごと
```cpp
// ===== 光源未設定の警告 =====
static void WarnMissingDirectionalLight(ShadingType shadingType) {
	// （中身ごと全部）
}
```

**追加する**：`Renderer::Initialize()` の下
```cpp
//=============================================================================
// ライト
//=============================================================================
void Renderer::SetFrameLights(const DirectionalLightData& directionalLight, const PointLightListData& pointLights) {
	instance_->frameDirectionalLight_ = directionalLight;
	instance_->framePointLights_ = pointLights;
}
```

**`PushMesh` の中**：先頭の警告の3行を削除する
```cpp
	if (config.shadingType != ShadingType::Unlit && config.directionalLight == nullptr) {
		WarnMissingDirectionalLight(config.shadingType);
	}
```
ライトを詰める部分を差し替える。

変更前
```cpp
	req.directionalLightData = config.directionalLight ? config.directionalLight->GetData() : DirectionalLightData{};
	// ポイントライト
	if (config.pointLights) {
		uint32_t n = std::min((uint32_t)config.pointLights->size(), kMaxPointLights);
		for (uint32_t i = 0; i < n; ++i) {
			req.pointLightListData.lights[i] = (*config.pointLights)[i]->GetData();
		}
		req.pointLightListData.count = n;
	}
```
変更後
```cpp
	// ライト（LightManagerがこのフレーム用にまとめたもの）
	req.directionalLightData = instance_->frameDirectionalLight_;
	req.pointLightListData = instance_->framePointLights_;
```

**`DrawModel` の中**：先頭のアサートを削除する（`// 参照するモデル` 以降の早期リターンは残す）
```cpp
	// 早期リターン
	if (config.shadingType != ShadingType::Unlit) {
		MY_ASSERT_MSG(config.directionalLight != nullptr, "ShadingType::Unlit以外には光源を設置してください");
	}
```
ライトを詰める部分を差し替える。

変更前
```cpp
		req.directionalLightData = config.directionalLight ? config.directionalLight->GetData() : DirectionalLightData{};
		// 参照分ポイントライトを設定
		if (config.pointLights) {
			uint32_t n = std::min((uint32_t)config.pointLights->size(), kMaxPointLights);
			for (uint32_t i = 0; i < n; ++i) {
				req.pointLightListData.lights[i] = (*config.pointLights)[i]->GetData();
			}
			req.pointLightListData.count = n;
		}
```
変更後
```cpp
		// ライト（LightManagerがこのフレーム用にまとめたもの）
		req.directionalLightData = instance_->frameDirectionalLight_;
		req.pointLightListData = instance_->framePointLights_;
```

### ⑥ `MyEngine/Graphics/Model/ModelManager.h`（任意）
使われていない前方宣言を削除する。
```cpp
class DirectionalLight;
```

### ⑦ ゲーム側のシーン

| 変更前（Step 4） | 変更後（Step 5） |
|---|---|
| `#include "MyEngine/Light/PointLight.h"` など | `#include "MyEngine/Light/LightManager.h"` |
| `Handle<PointLight>` | `Handle<PointLightComponent>` |
| `PointLight* light = LightManager::GetPointLight(h);` | `PointLightComponent* light = LightManager::GetPointLight(h);` |
| `light->GetComponent().position` | `light->position` |
| `LightManager::GetDirectionalLight()->GetComponent()` | `LightManager::GetDirectionalLight()` |
| `config.directionalLight = ...;` | **行ごと削除** |
| `config.pointLights = ...;` | **行ごと削除** |

変更後の例

ヘッダ
```cpp
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Light/LightManager.h"

class GameScene : public IScene {
	// （今あるメンバはそのまま）
private:
	std::vector<Handle<PointLightComponent>> pointLightHandles_; // このシーンが追加したライト
};
```

`Initialize()`
```cpp
	// 平行光源
	DirectionalLightComponent& sun = LightManager::GetDirectionalLight();
	sun.color = {1.0f, 1.0f, 1.0f};
	sun.direction = {0.0f, -1.0f, 0.3f}; // 正規化しなくてよい（収集時に正規化される）
	sun.intensity = 1.0f;

	// ポイントライト
	Handle<PointLightComponent> handle = LightManager::AddPointLight();
	if (PointLightComponent* light = LightManager::GetPointLight(handle)) {
		light->position = {0.0f, 2.0f, 0.0f};
		light->radius = 6.0f;
	}
	pointLightHandles_.push_back(handle);
```

`Draw()`
```cpp
	// ライトの指定はもう要らない。Unlit以外は自動でLightManagerのライトで照らされる
	Renderer::DrawModel(config);
```

`Finalize()`
```cpp
	for (Handle<PointLightComponent> handle : pointLightHandles_) {
		LightManager::RemovePointLight(handle);
	}
	pointLightHandles_.clear();
```

---

### 解説

**sRGB → リニア変換の式**
- sRGBは「人の目に合わせて暗い側を細かく記録する」形式で、そのままでは足し算・掛け算の計算に使えない。ライティングの計算はリニア（明るさが数値に比例する）で行う必要がある。
- 暗い部分（0.04045以下）は直線、それより明るい部分は2.4乗の曲線、という区分式。GPUが `_SRGB` 形式のテクスチャを読むときと同じ式なので、テクスチャの色とライトの色の扱いがそろう。
- **見た目の変化**：白（1,1,1）と黒（0,0,0）は変換しても変わらない。中間の色は数値が小さくなる。例：オレンジ (1, 0.5, 0) → (1, 0.21, 0)。今まで「コードに書いた数値そのまま」だったのが、「その数値をカラーピッカーで見たときの色」で照らされるようになる。色付きのライトが濃く見えるようになるのは正しい変化。

**なぜ `Renderer::SetFrameLights` で渡すのか（依存の向き）**
- Renderer（Graphics）がLightManager（Light）を直接読みに行く形にすると、「LightがGraphicsを使い、GraphicsもLightを使う」循環になる（LightGizmoはすでにRendererを使っている）。
- 「LightManagerがRendererに渡す」一方向にしておくと、Rendererはライトがどこから来たかを知らなくて済む。

**`DirectionalLight` / `PointLight` クラスを消す理由**
- 変換（`GetData`）をLightManagerに移すと、クラスに残るのは「Componentを包んでいるだけ」になる。
- Componentを直接 `SlotMap` に入れた方が、`light->position` のように1段短く書ける。型別の連続したデータ配列（ARCHITECTURE.md 段階3）の形にも近い。

**警告関数とアサートを消してよい理由**
- 今までは「Configに平行光源を渡し忘れる」ことがあり得たので警告していた。
- Step 5からはLightManagerが常に平行光源を1つ持っていて、自動で使われるので、渡し忘れが起きなくなった。

**まだ残っている無駄（Step 5.1で直す）**
- ライトのデータはフレームに1回まとめるようになったが、`MeshRequest` への値のコピーと、`RenderContext` のリングバッファへの書き込みは、まだ描画1回ごとに行っている（ポイントライト16個分で約1KB × 描画回数）。
- どの描画でもライトは同じなので、Step 5.1で「1フレームに1回だけGPUに書いて、全描画で同じ場所を結ぶ」形にする。そうすると上限を上げてもコストがほとんど増えない。

**挙動の変化（知っておくこと）**
- 今まではConfigに `pointLights` を渡さなければ、そのモデルはポイントライトの影響を受けなかった。Step 5からは**Unlit以外の全描画が、全ライトの影響を受ける**。
- 特定の物だけライトを受けないようにしたい場合は、今は `ShadingType::Unlit` を使う。ライトごとに「どの物を照らすか」を選ぶ仕組み（UnityのCulling Mask）は、必要になったら別で考える。

---

### 確認すること
1. **エンジンのビルド**が通る（`DirectionalLight` / `PointLight` をincludeしている場所が残っていればエラーで分かる）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 白いライトの当たり方は前と同じ
   - 色付きのライトは、前より濃い色で照らされる（上の「見た目の変化」の通り）
   - Configでライトを指定しなくても、モデルが照らされる
   - 平行光源の `direction` を `(0,0,0)` にしても、落ちたり真っ暗になったりしない（真下から当たる）
   - ポイントライトを17個以上置くと、警告が1回だけ出る
   - Stop → Playを繰り返しても、ライトが増えない（Step 4と同じ）

### 次のStepでやること（ここではやらない）
- Step 5.1：ライトの定数バッファを1フレームに1回だけ書いて、全描画で共有する。`MeshRequest` からライトのデータを外し、RenderContextのライト用リングバッファを消す。その後で `kMaxPointLights` を引き上げる。
