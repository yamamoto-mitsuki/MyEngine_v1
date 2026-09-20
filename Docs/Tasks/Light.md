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
| 5 | 描画データの収集をフレーム1回に（sRGB→リニア変換、方向の正規化もここ）。Rendererの設定からライトのポインタを無くす | 完了（`LightManager.h` 34行目はポインタで返す形に修正済み。これでもよい） |
| 5.1 | ライトの確認用ウィンドウ（ゲーム側のライトのコードを無くす） | 完了 |
| 5.1a | ギズモのアイコンの不具合修正（行列の上書き、インデックスの位置、深度） | 完了（実行して確認済み） |
| 5.2 | ライトの定数バッファを全描画で共有する（描画ごとのコピーをやめる） → 設置数の上限を64に | 完了（実行して確認済み） |
| （寄り道） | RenderContextの書き込み位置を1つにまとめる → `FrameLoop.md` Step 1 | 完了（実行して確認済み） |
| 5.5 | スポットライトの追加（Component、GPU、5つのシェーダー、LightManager、ギズモの円錐、確認用ウィンドウ） | 完了（実行して確認済み） |
| 5.6 | ライトのシェーダー周りの整理（使う種類だけ結ぶ、`PointLightFactor`、スポットのアイコン） | 完了（実行して確認済み） |
| （別ファイル） | マテリアルに `alphaCutoff`（切り抜き）を足す → `Material.md` Step 1 | 完了（実行して確認済み） |
| 5.7 | 非均一スケールでも法線が正しくなるようにする（法線用の行列） | 完了（学校の課題。コミット 2d0c3da に入っている） |
| （別ファイル） | 型別のComponent管理（`ComponentStorage`）、ゲーム固有Component → `Entity.md` | 完了（手順1〜6・A〜E とも写経済み、実行確認済み） |
| 6 | ライトをEntityのComponentにする（位置・向きはTransform、Inspector、Add Component、Create > Light、Lightsウィンドウを消す） | 完了（実行して確認済み。2026-09-20） |
| **6.1** | **Step 6の後の直し（ImGuiのID衝突、平行光源の太陽アイコン、`LightManager` → `LightSystem`）** | **写経待ち（2026-09-20）** |

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
| `PointLightComponent::position` は今は持つ | Entity / TransformComponentができたらTransformから取るようにして外す → **Step 6で外した**（この表の最後） |
| SlotMapは「生きている要素を隙間なく詰めて並べる」方式 | `for` でそのまま全部回せて、途中に死んだ要素が混ざらない。連続して並ぶのでARCHITECTURE.md 段階3の「型別の連続配列」にそのまま使える。削除も末尾との入れ替えで一瞬 |
| `Get` で受け取ったポインタは使い捨て | 追加・削除で要素の場所が動く（配列の伸長、末尾との入れ替え）。フレームをまたいで持つのはHandleだけ（原則1） |
| ライトの削除は予約して、更新の最後にまとめて反映 | ARCHITECTURE.md「生成、破棄の規約」。更新の途中で消すと、同じフレームで使っているポインタが別のライトを指してしまう |
| ライトの追加はその場で反映 | 追加直後に値を設定したいため。その代わり「追加したら、それより前に受け取ったポインタは使わない」 → **Step 6から、他のComponentと同じく予約（次のフレーム）**。初期値は `RequestAdd` に渡す |
| 平行光源はLightManagerが1つだけ持つ（Handle無し） | 普通はシーンに1つ。Entity化して複数置けるようになったら「どれをメインにするか」のルールを決める → **Step 6でEntityのComponentへ。ルールは「有効な物のうち最初に見つかった物」**（この表の最後） |
| LightManagerは全ウィンドウ共通（ParticleManagerと同じ） | 既存の管理クラスに合わせる。ウィンドウごとに別のライトを持つのは、シーン（Entity）がライトを持つようになってから |
| ギズモは `WindowManager::DrawAll` の `USE_IMGUI` の中で描く | 製品版（Release）ではギズモを出さない |
| シーンが追加したライトは、シーンの `Finalize` で削除する | Stop → Playでシーンが作り直されるたびに `Initialize` で追加されるので、消さないと増え続ける → **Step 6から、`sceneRoot_` の下に作れば `Finalize` の `Destroy(sceneRoot_)` で一緒に消える** |
| `LightManager::Release` はウィンドウ（シーン）の破棄より後 | シーン側がライトを消そうとしたときに、LightManagerが先に無くなっていると落ちる |
| 無効値に `std::numeric_limits<T>::max()` を使わない | `Windows.h` の `max` マクロとぶつかる。`0xFFFFFFFF` で書く（エンジンの他の無効値と同じ） |
| `DirectionalLight` / `PointLight` クラスは消して、Componentを直接持つ（Step 5） | 変換処理をLightManagerに移すと、クラスはComponentを包むだけの殻になる。ARCHITECTURE.md 段階3の「型別のデータ配列」に近づく |
| ライトはRendererの設定で指定しない。Unlit以外は全部LightManagerのライトで照らす（Step 5） | 描画ごとにポインタを渡す必要が無くなる（原則1）。Unityと同じく「シーンのライトは全部に効く」 |
| 依存の向きは「Light → Graphics」。RendererはLightManagerを知らない（Step 5） | LightManagerが `Renderer::SetFrameLights` で渡す。Graphicsが上位のモジュールに依存すると、循環しやすくなる |
| ライトの値の変更は `Update` の中で行う（Step 5） | 収集は更新の最後に1回。`Draw` の中で変えた値は次のフレームから反映される |
| 上限を超えたポイントライトは「SlotMapの並び順」で上限まで（Step 5） | まずは単純に。削除で並びが入れ替わるので、どれが選ばれるかは保証しない。カメラからの距離で選ぶなどは、上限引き上げ（5.2）の後に必要なら考える。超えたときは警告を1回だけ出す |
| ゲーム側のライトのコードは消す。確認はエンジンの確認用ウィンドウで行う（Step 5.1） | ライトの作り方が変わるたびにゲーム側を直す手間を無くす。ライトを大量に置く確認（5.2）も楽になる。ただしカメラとモデルを描くシーンは残す（エンジンは静的ライブラリなので単体では動かない） |
| 確認用ウィンドウは `ImGuiManager::Begin` で描く（Step 5.1） | Log / Parameters / Profilerと同じ場所。Inspector（Step 6）ができたら消す仮のもの → **Step 6で消した** |
| ヘッダのメンバ変数は `#ifdef USE_IMGUI` で囲まない（Step 5.1） | エンジン（.lib）とゲーム（.exe）で `USE_IMGUI` の設定が違うと、同じクラスの大きさが食い違ってメモリを壊す。関数の宣言は囲っても大きさが変わらないのでよい |
| 確認用ウィンドウで消せるのは、そのウィンドウで追加したライトだけ（Step 5.1） | SlotMapは要素からHandleを逆引きできない。ゲーム側が追加したライトを勝手に消さない（Step 6で確認用ウィンドウごと消した） |
| ライトの定数バッファは1フレームに1個を上書きして、全描画で共有する（Step 5.2） | どの描画でもライトは同じ。GPUの処理が終わるのを待ってから次のフレームに進む作りなので、上書きしてよい。待たない（非同期の）作りにするときはフレームごとにバッファを分ける |
| ポイントライトの上限は64（Step 5.2） | ピクセルシェーダーは1ピクセルごとに全ライトをループするので、ライトの数に比例して重くなる。手で置いて確認する規模には十分。これ以上はStructuredBuffer ＋ タイル / クラスター方式 |
| `kMaxPointLights` はC++とHLSLで必ず同じ値にする | 違うと構造体の大きさがずれ、`count` などを読む位置がずれて正しく光らない |
| リングバッファの書き込み位置は「1つのバッファに1つのカウンタ」で決める（Step 5.1a） | 行列のバッファをメッシュ（`drawCallIndex_`）と線（`drawCallLineIndex_`）が別々のカウンタで使っていて、同じ場所を上書きし合っていた。コマンドはフレームの最後にまとめて実行されるので、最後に書いた値で全部描かれる |
| ギズモのアイコンは半透明（`DepthMode::TestNoWrite`）で描く（Step 5.1a） | 不透明として描くと、アイコンの透明な部分まで深度を書く。後から描くSkyboxや他のアイコンがそこだけ描かれず、背景色が見える |
| アイコンの描き方（半透明 / 切り抜き）は、今は半透明のまま。切り抜きはマテリアルに「しきい値」を持たせる形で、マテリアル / Inspectorの作業のときにやる（2026-09-18） | 切り抜き（アルファがしきい値未満なら捨てる）は良い方法だが、今のUnlitは「アルファがちょうど0なら捨てる」しかない。シェーダーを分けるより、マテリアルの定数に `alphaCutoff` を持たせて0なら切り抜かない、とすればシェーダーもPSOも増えない |
| スポットライトの角度は、Componentでは「中心からの角度（度）」、GPUへは「cos」で送る（Step 5.5） | Inspectorで分かりやすいのは度。シェーダーは `dot` の結果（cos）とそのまま比べられるので、`acos` が要らない。変換はLightManagerの収集で1か所だけ |
| スポットライトは「内側の角度まで100%、外側の角度で0、間は直線」（Step 5.5） | 境目をくっきりにもぼかしにもできる（Unityの Inner / Outer Spot Angle と同じ考え方）。内側が外側より大きいと明るさが逆転するので、収集のときに外側までに収める |
| スポットライトの上限は32（Step 5.5） | ポイントライトより置く数が少ない想定。定数バッファは約2KB。これより多くはポイントライトと同じく、Compute Shaderの後にクラスター方式で考える |
| GPU用のライト構造体には `static_assert` で大きさを確かめる（Step 5.5） | HLSLと大きさが食い違うと、コンパイルは通るのにライトの値がずれる（`kMaxPointLights` の件と同じ種類）。C++側で大きさを固定しておくと、食い違いをコンパイルエラーで気づける |
| スポットライトの減衰の計算は `Light.hlsli` の関数（`SpotLightFactor`）にまとめる（Step 5.5） | 5つのシェーダーに同じ計算を書くと、直すときに5か所を直すことになる。ポイントライトの減衰も、後で同じように関数にまとめてよい |
| ライト付きの全シェーダーで `gSpotLights` を使う（Step 5.5） | 使っていない定数バッファはシェーダーのコンパイルで消え、RootSignatureにスロットができない。`DrawMesh` はスロットがあるつもりで結ぶので、1つでも使っていないシェーダーがあるとアサートで止まる |
| 「不透明 / 半透明」は、テクスチャに透明な部分があるかで決める（見た目の絵が透けているかではない） | `pointLight.png` は約82%のピクセルが完全に透明（電球の線だけ不透明）。`grid.png` は100%不透明。透明な部分は「後ろの色をそのまま見せる」ので、後ろを先に描いておく必要がある。数が多いもの（木の葉など）は、半透明ではなく切り抜き（アルファがしきい値未満なら捨てて不透明で描く）も選べる |
| Gameビューにギズモが出るのは、今は直さない | 最終的には出さない（`Editor.md` で決定済み）。RenderQueueにビューの区別を付ける作業なので、Editorの作業でまとめてやる |
| 64個より多く置く仕組みは後回し（Compute Shaderを実装してから） | ユーザーの判断。遠いライトを除く、StructuredBufferにする、タイル / クラスター方式にする、はまとめて考える。カメラから見えないライトをCPUで除くだけならCSは要らないが、SceneビューとGameビューでカメラが違うので、ライトのバッファをビューごとに分ける必要が出る（5.2の「フレームに1個」と合わない） |
| 法線は `transpose(inverse(worldMatrix))` で変換する。ワールド行列は使わない（Step 5.7） | 法線は「面に垂直」であることが大事な量なので、位置と同じ行列で変換すると非均一スケール（Yだけ2倍など）で垂直でなくなる。回転・平行移動・均一スケールだけなら結果が同じなので、今まで気づかなかった |
| `worldMatrix` と `normalMatrix` は `MakeObjectTransform` でセットで作る（Step 5.7） | 片方だけ入れると法線に前の描画の値が残る。入れる場所が2か所（`PushMesh` と `DrawModel`）に分かれているので、関数にしないと必ず直し忘れる（`FrameLoop.md` Step 1 で潰したリングバッファのバグと同じ形） |
| 将来「GPUを待たずに次のフレームへ進む」作りにするときは、ライトだけでなく毎フレーム書くバッファを全部まとめて変える | ライトは書く場所が `RenderContext::SetFrameLights` の1か所、結ぶ場所が `DrawMesh` の1か所なので、バッファを「フレーム数分の配列」にするだけで済む。ただしRenderContextの全リングバッファ、SceneRendererのカメラ、Skybox、Bloomの定数バッファも同じ前提（毎フレーム先頭から上書き）なので、ライトだけ変えても意味が無い |
| ライトはEntityに付けるComponent。位置・向きはEntityのTransformから取る（Step 6） | Unityと同じ。親子で一緒に動かせる。位置の持ち場所を1つにする（Componentの `position` とTransformの二重管理にしない）。コピー・Undoも今までの仕組みのまま効く |
| ライトが照らす向きはTransformの前（ローカルの+Z）＝ `worldMatrix` の3行目。位置は4行目（Step 6） | Unity・DirectXの慣習。回転0で+Z、Rotation Xを90度で真下。読み取りは `TransformComponent.h` の `GetWorldPosition` / `GetWorldForward` |
| 平行光源はGPUに1つ。有効な物のうち最初に見つかった物を使い、2つ以上あれば警告を1回（Step 6） | シェーダーが1つ分しか持たない。「最初」は配列の順で削除で入れ替わるので、2つ置かない。どれを使うかのルール（一番明るい物、指定など）は必要になってから |
| 平行光源が1つも無ければ `intensity = 0`（照らさない）。ゲームのシーンが Directional Light を作る（Step 6） | 前は LightManager がいつも1つ持っていた。シーンに置く物になったので、置かなければ無い（Unityと同じ） |
| `LightSystem`（旧 LightManager）は実体を持たず、EntityManager から `ForEach` で集める係（Step 6・6.1） | 実体の持ち主を1つにする（Undo・コピー・削除・シーンの作り直しがEntityManagerだけで済む） |
| 実体を持たず集めるだけのクラスは `〜System`（`LightSystem`・`ModelRenderSystem`）、実体を持つ物は `〜Manager`（`EntityManager`・`TextureManager`）（Step 6.1） | 名前で役目が分かる。`LightManager` は Step 6 で実体を EntityManager へ渡したので改名した |
| ライトの型の登録は `LightSystem::Initialize`、`EntityManager::Initialize` の後（Step 6） | 依存の向きは「Light → Entity」。EntityManagerはライトを知らない |
| 自分か親が無効なライトは照らさない・ギズモも出さない（Step 6） | Unityと同じ。ModelRenderSystemと同じ `EntityManager::IsActiveInHierarchy` を使う |
| 確認用の Lights ウィンドウは消した。全体のギズモのON/OFFはメニューバーの View → Gizmos（Step 6） | 役目はInspector・HierarchyのCreate > Light・Add Component → Lighting へ移った |
| Create > Light の Directional Light は位置(0,3,0)・回転(50,-30,0)度、Spot Light は回転X 90度（真下）（Step 6） | Directional はUnityの最初のシーンと同じ。平行光源の位置は照らし方に関係なく、ギズモを描く場所 |
| 平行光源のギズモは「太陽のアイコン＋向きに垂直な円盤＋向きに伸びる9本の線」（線は黄色）（Step 6.1） | Unityの平行光源のギズモと同じ形。アイコンは `directionalLight.png`。InspectorのGizmoの Icon / Range で別々に消せる |
| ライトのInspectorのギズモのチェックは `PushID("gizmo")` の中に置く（Step 6.1） | ImGuiは名前からIDを作るので、同じ区画に同じ名前（`Range`）が2つあると「conflicting ID」で止まる。縄張りを作れば、中の名前を気にしなくてよい |

## 未解決
- **ギズモがGameビューにも出る。さらにギズモ自体にBloom / Lensがかかる**：RenderQueueに「Sceneビューだけ」という区別が無く、ギズモがポストエフェクトより前に描かれている。Renderer側の対応が必要 → `Editor.md`（GameビューにBloomがかかること自体は正しい）
- **半透明の並び順が、Sceneビューでは正しくないことがある**：奥から描くための距離（`cameraDistanceSq`）は、リクエストを作るときに `config.camera`（ギズモではゲームのカメラ）で1回だけ計算している。Sceneビューはデバッグカメラから見るので、アイコン同士が重なったときに前後がおかしく見えることがある。これもビューの区別の話なので `Editor.md` の作業でまとめて直す

### ライトは何個まで置けるか（2026-09-18）
ポイントライト64個＋スポットライト32個（今の上限）を置いても、GPUに書くデータは約1MB（64MB中）だった。「バッファは余っているから5000個いけるのでは？」の答え。

| 壁 | 今の値 | どうなるか |
|---|---|---|
| **1. ピクセルシェーダーのループ** | 1ピクセルごとに `count` 回 | **ここが最初に詰まる**。画面の全ピクセル × ライトの数だけ計算する。1920×1080は約200万ピクセルなので、100個置くと1フレームに2億回の計算になる |
| 2. 定数バッファの上限（64KB） | ポイントライト48バイト → 約1300個 | これを超えるにはStructuredBuffer（SRV）に変える |
| 3. ギズモの線のメモリ | 1個あたり約6KB（円3つ×2ビュー） | 5000個で約60MB。`FrameUploadBuffer`（64MB）があふれてアサートで止まる。ギズモをOFFにすれば回避できる |
| 4. CPUの収集（`CollectForGPU`） | 1個あたり数十バイトのコピー | 数千個でも数百KBなので、ここは問題にならない |

- つまり「置けるか」を決めるのは**バッファではなくピクセルシェーダーの計算量**。数千個を置きたいなら、Compute Shaderで「そのピクセルに届くライトだけ」を選ぶ仕組み（タイル / クラスター方式）が必要になる。順番はStructuredBuffer化 → クラスター方式。
- 「置いてあるけど画面外」「遠くて影響が無い」ライトをCPUで外すだけでも数を増やせるが、SceneビューとGameビューでカメラが違うので、ライトのバッファをビューごとに分ける必要が出る（`FrameLoop.md` の「決めたこと」）。


## Step 6：ライトをEntityのComponentにする（Inspector・Add Component・Createメニュー）

**先に [Entity.md](Entity.md) の手順A〜Eを写して、一度実行しておくこと**（このStepは `ComponentSnapshot`・`IsActiveInHierarchy`・`RequestCreate` の新しい引数を使う）。

### 目的

- ライトも ModelRenderer と同じく、**Entityに付けるComponent** にする。Hierarchyで選んでInspectorで編集、Undo・コピー・削除のUndoが何も書き足さずに効く
- 位置と向きをComponentから外し、**EntityのTransformから取る**（「`PointLightComponent::position` はTransformができたら外す」の約束）。親子にもできる
- 仮の確認用ウィンドウ（Lights）を消す（「Inspectorができたら消す」の約束）

### 使い方で変わる所

| 前 | 後 |
|---|---|
| ライトの追加は Lights ウィンドウの Add x10、またはコードの `LightManager::AddPointLight` | Hierarchyの **＋**（または何も無い所の右クリック）→ **Light** → Directional / Point / Spot。または Add Component → **Lighting** |
| 位置はComponentの Position、向きは Direction | EntityのTransformの **Position / Rotation**。親を動かすと付いてくる |
| 平行光源は LightManager がいつも1つ持っていた | 平行光源もEntity。**シーンに置かないと平行光源は無い**（IBLとポイント・スポットだけで照らす）。GameSceneで1つ作る（⑬） |
| 全部のギズモのON/OFFは Lights ウィンドウ | メニューバーの **View → Gizmos** |
| ライトを消すのは Remove Added | Delete（Undoできる）。Entityのチェックを外すと照らさない |

### Step 6の後のデータの流れ

```
 Hierarchy（Create > Light）/ Inspector（Add Component・値の編集）/ ゲームのコード（RequestAdd）
      │
      ▼
 EntityManager
   ComponentStorage<DirectionalLightComponent>  ┐
   ComponentStorage<PointLightComponent>        ├ LightManager::Initialize が登録する
   ComponentStorage<SpotLightComponent>         ┘
   ComponentStorage<TransformComponent>          ← UpdateTransforms が worldMatrix を計算
      │
      ▼  WindowManager::UpdateAll の中、UpdateTransforms の後
 LightManager::Update（ForEachで、ライトを持っているEntityだけ回す）
   ・自分と親が全部有効な物だけ（IsActiveInHierarchy）
   ・位置 ＝ worldMatrix の4行目、向き ＝ worldMatrix の3行目（ローカルの+Z）
   ・sRGB→リニア、角度→cos（前と同じ）
      │ Renderer::SetFrameLights
      ▼
 RenderContext（前と同じ）
```

### 変更するファイル（上から順に）

| # | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Entity/TransformComponent.h` | `GetWorldPosition` / `GetWorldForward` を足す |
| ② | `MyEngine/Light/LightComponent.h` | 全体を差し替え。`position` / `direction` を消す。平行光源にもギズモの設定 |
| ③ | `MyEngine/Light/LightManager.h` | 全体を差し替え。SlotMap・Add/Remove/Get・確認用ウィンドウを消す |
| ④ | `MyEngine/Light/LightManager.cpp` | 全体を差し替え。ライトの型を登録し、`ForEach` で集める |
| ⑤ | `MyEngine/Light/LightGizmo.h` | 全体を差し替え。位置・向きを引数で受け取る。平行光源のギズモを足す |
| ⑥ | `MyEngine/Light/LightGizmo.cpp` | 全体を差し替え |
| ⑦ | `MyEngine/Editor/Inspector/LightEditor.h`（**新規**） | 3つのライトのInspector |
| ⑧ | `MyEngine/Editor/Inspector/LightEditor.cpp`（**新規**） | 同上 |
| ⑨ | `MyEngine/Editor/Windows/InspectorWindow.cpp` | ⑦の3つを登録 |
| ⑩ | `MyEngine/Editor/Windows/HierarchyWindow.cpp` | Createメニューに Light |
| ⑪ | `MyEngine/Editor/ImGuiManager.cpp` | Lightsウィンドウを消し、View メニューを足す |
| ⑫ | `MyEngine/Engine.cpp` | 初期化の順番（EntityManager → LightManager） |
| ⑬ | ゲーム：`GameScene.cpp` | Directional Light を置く |

⑦⑧はエンジンのプロジェクトの `Editor/Inspector` フィルターに追加する（ソリューションエクスプローラーで右クリック → 追加 → 既存の項目）。

### ① `MyEngine/Entity/TransformComponent.h`

`struct TransformComponent` の後ろに足す。

```cpp


// ===== worldMatrix から読み取る（UpdateTransformsの後の値。作った直後のフレームは原点・回転なし）=====

// ワールド座標（行列の4行目＝平行移動）
inline Vector3 GetWorldPosition(const TransformComponent& transform) {
	const Matrix4x4& m = transform.worldMatrix;
	return {m.m[3][0], m.m[3][1], m.m[3][2]};
}

// 前（ローカルの+Z）がワールドでどちらを向いているか。長さ1（大きさが0なら(0,0,0)）
// 行列の3行目＝ローカルの(0,0,1)を変換した向き。ライトはこの向きに照らす
inline Vector3 GetWorldForward(const TransformComponent& transform) {
	const Matrix4x4& m = transform.worldMatrix;
	return Normalize(Vector3{m.m[2][0], m.m[2][1], m.m[2][2]});
}
```

### ② `MyEngine/Light/LightComponent.h`（ファイル全体を差し替え）

```cpp
#pragma once
#include "MyEngine/Math/Vector3.h"

// Inspectorで編集するライトのデータ。Entityに付けるComponent（カテゴリ：Lighting）
// GPUへ送る構造体（ShaderConstants.h）とは分けて、padding・ポインタ・GPUリソースを持たせない
// 位置と向きは持たない。付けたEntityのTransformから取る（位置＝ワールド座標、向き＝ローカルの+Z）


/// <summary>
/// ライトのエディタ表示のON / OFF。ゲームの絵には影響しない
/// </summary>
struct LightGizmoFlags {
	bool showIcon = true;  // アイコン
	bool showRange = true; // 光の届く範囲（平行光源は向き）のワイヤー
};


/// <summary>
/// 平行光源。GPUに送るのは1つだけ（有効な物のうち最初に見つかった物）
/// </summary>
struct DirectionalLightComponent {
	Vector3 color = {1.0f, 1.0f, 1.0f}; // 色
	float intensity = 1.0f;             // 強さ
	LightGizmoFlags gizmo;              // ギズモの表示設定（アイコンはまだ無いので、向きの線だけ）
};


/// <summary>
/// ポイントライト
/// </summary>
struct PointLightComponent {
	Vector3 color = {1.0f, 1.0f, 1.0f}; // 色
	float intensity = 1.0f;             // 強さ
	float radius = 10.0f;               // ライトの届く最大距離
	float decay = 1.0f;                 // 減衰率（-にはしない）
	LightGizmoFlags gizmo;              // ギズモの表示設定
};


/// <summary>
/// スポットライト。Transformの前（+Z）を照らす
/// </summary>
struct SpotLightComponent {
	static constexpr float kMaxAngle = 89.0f; // 角度の条件（度）。90度で円錐が平らになる

	Vector3 color = {1.0f, 1.0f, 1.0f}; // 色
	float intensity = 1.0f;             // 強さ
	float range = 10.0f;                // ライトの届く最大距離
	float decay = 1.0f;                 // 減衰率（-にはしない）
	float outerAngle = 30.0f;           // 外側の角度（度）。これより外は照らさない
	float innerAngle = 20.0f;           // 内側の角度（度）。これより内は100%で照らす。外側より大きくしない
	LightGizmoFlags gizmo;              // ギズモの表示設定
};
```

### ③ `MyEngine/Light/LightManager.h`（ファイル全体を差し替え）

```cpp
#pragma once

// 前方宣言
class Camera;


/// <summary>
/// ライトのComponent（Directional / Point / Spot）を持つEntityを集めて、GPU用にまとめる
/// <para>ライトの実体はEntityManagerが型別に持つ。ここはComponentを登録して、毎フレーム読むだけ</para>
/// <para>GPUバッファは持たない。GPUへ送るのはRenderContextの役目</para>
/// </summary>
class LightManager {
public:
	// EntityManagerにライトのComponentを登録する（EntityManager::Initializeの後に呼ぶ）
	static void Initialize();
	static void Release();

	/// <summary>
	/// 更新の最後（EntityManager::UpdateTransformsの後）に呼ぶ。
	/// このフレームのライトをGPU用の形にまとめてRendererへ渡す
	/// </summary>
	static void Update();

	/// <summary>
	/// 全ライトのギズモを描く（エディタ用）
	/// </summary>
	static void DrawGizmos(Camera* camera);

private:
	static LightManager* instance_;

	void CollectForGPU(); // Component → GPU用データにまとめてRendererへ渡す

	// 上限を超えたときの警告は1回だけ出す
	bool hasWarnedDirectionalLightLimit_ = false;
	bool hasWarnedPointLightLimit_ = false;
	bool hasWarnedSpotLightLimit_ = false;
};
```

### ④ `MyEngine/Light/LightManager.cpp`（ファイル全体を差し替え）

```cpp
#include "LightManager.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Light/LightComponent.h"
#include "MyEngine/Light/LightGizmo.h"

// 静的メンバ変数
LightManager* LightManager::instance_ = nullptr;

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン

/// <summary>
/// sRGB（見た目の色） → リニア（ライティング計算用）。1成分分
/// <para>GPUが _SRGB形式のテクスチャを読むときと同じ式</para>
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

/// <summary>
/// 向きを正規化する。(0,0,0)だとシェーダーで壊れるので、そのときは真下にする（Scaleが0のときなど）
/// </summary>
Vector3 NormalizeDirection(const Vector3& direction) {
	if (LengthSq(direction) > 1e-6f) {
		return Normalize(direction);
	}
	return {0.0f, -1.0f, 0.0f};
}

/// <summary>
/// ライトとして使えるEntityなら、そのTransformを返す（自分か親が無効ならnullptr＝照らさない・ギズモも出さない）
/// </summary>
const TransformComponent* FindActiveTransform(Handle<Entity> entity) {
	if (!EntityManager::IsActiveInHierarchy(entity)) {
		return nullptr;
	}
	return EntityManager::Get<TransformComponent>(entity);
}

// ライトが照らす向き（Transformの前＝ローカルの+Z）
Vector3 LightDirection(const TransformComponent& transform) { return NormalizeDirection(GetWorldForward(transform)); }
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void LightManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new LightManager();
	// ライトもEntityに付けるComponentとして、EntityManagerに置き場所を作ってもらう
	EntityManager::RegisterComponent<DirectionalLightComponent>();
	EntityManager::RegisterComponent<PointLightComponent>();
	EntityManager::RegisterComponent<SpotLightComponent>();
}

void LightManager::Release() {
	delete instance_;
	instance_ = nullptr;
}


//=============================================================================
// 更新
//=============================================================================
void LightManager::Update() { instance_->CollectForGPU(); }

// ====== Component → GPU用データにまとめてRendererへ渡す =====
void LightManager::CollectForGPU() {
	// --- 平行光源（GPUには1つだけ。有効な物のうち最初に見つかった物を使う）---
	DirectionalLightData directional;
	directional.intensity = 0.0f; // 1つも無ければ照らさない
	uint32_t directionalCount = 0;
	EntityManager::ForEach<DirectionalLightComponent>([&](Handle<Entity> entity, const DirectionalLightComponent& light) {
		const TransformComponent* transform = FindActiveTransform(entity);
		if (!transform) {
			return;
		}
		if (directionalCount == 0) {
			directional.color = SrgbToLinear(light.color);
			directional.intensity = light.intensity;
			directional.direction = LightDirection(*transform);
		}
		++directionalCount;
	});
	if (directionalCount > 1 && !hasWarnedDirectionalLightLimit_) {
		LogManager::Warning(std::format("平行光源が{}個あります。使われるのは1つだけです", directionalCount));
		hasWarnedDirectionalLightLimit_ = true;
	}

	// --- ポイントライト（見つかった順に上限まで） ---
	PointLightListData pointList;
	uint32_t pointCount = 0;
	EntityManager::ForEach<PointLightComponent>([&](Handle<Entity> entity, const PointLightComponent& light) {
		const TransformComponent* transform = FindActiveTransform(entity);
		if (!transform) {
			return;
		}
		if (pointCount >= kMaxPointLights) {
			if (!hasWarnedPointLightLimit_) {
				LogManager::Warning(std::format("ポイントライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxPointLights));
				hasWarnedPointLightLimit_ = true;
			}
			return;
		}
		PointLightData& data = pointList.lights[pointCount];
		data.color = SrgbToLinear(light.color);
		data.position = GetWorldPosition(*transform);
		data.intensity = light.intensity;
		data.radius = light.radius; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		++pointCount;
	});
	pointList.count = pointCount;

	// --- スポットライト（見つかった順に上限まで） ---
	SpotLightListData spotList;
	uint32_t spotCount = 0;
	EntityManager::ForEach<SpotLightComponent>([&](Handle<Entity> entity, const SpotLightComponent& light) {
		const TransformComponent* transform = FindActiveTransform(entity);
		if (!transform) {
			return;
		}
		if (spotCount >= kMaxSpotLights) {
			if (!hasWarnedSpotLightLimit_) {
				LogManager::Warning(std::format("スポットライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxSpotLights));
				hasWarnedSpotLightLimit_ = true;
			}
			return;
		}
		SpotLightData& data = spotList.lights[spotCount];
		data.color = SrgbToLinear(light.color);
		data.position = GetWorldPosition(*transform);
		data.intensity = light.intensity;
		data.direction = LightDirection(*transform);
		data.range = light.range; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		// 角度（度）→ cos。内側が外側より大きいと明るさの向きが逆になるので、外側までに収める
		float outerAngle = std::clamp(light.outerAngle, 0.0f, SpotLightComponent::kMaxAngle);
		float innerAngle = std::clamp(light.innerAngle, 0.0f, outerAngle);
		data.cosOuter = std::cos(outerAngle * kDegToRad);
		data.cosInner = std::cos(innerAngle * kDegToRad);
		++spotCount;
	});
	spotList.count = spotCount;

	Renderer::SetFrameLights(directional, pointList, spotList);
}


//=============================================================================
// ギズモ
//=============================================================================
void LightManager::DrawGizmos(Camera* camera) {
	LightGizmo::Begin(camera);
	EntityManager::ForEach<DirectionalLightComponent>([](Handle<Entity> entity, const DirectionalLightComponent& light) {
		if (const TransformComponent* transform = FindActiveTransform(entity)) {
			LightGizmo::AddDirectionalLight(GetWorldPosition(*transform), LightDirection(*transform), light);
		}
	});
	EntityManager::ForEach<PointLightComponent>([](Handle<Entity> entity, const PointLightComponent& light) {
		if (const TransformComponent* transform = FindActiveTransform(entity)) {
			LightGizmo::AddPointLight(GetWorldPosition(*transform), light);
		}
	});
	EntityManager::ForEach<SpotLightComponent>([](Handle<Entity> entity, const SpotLightComponent& light) {
		if (const TransformComponent* transform = FindActiveTransform(entity)) {
			LightGizmo::AddSpotLight(GetWorldPosition(*transform), LightDirection(*transform), light);
		}
	});
	LightGizmo::End();
}
```

### ⑤ `MyEngine/Light/LightGizmo.h`（ファイル全体を差し替え）

```cpp
#pragma once
#include <cstdint>

#include "MyEngine/Light/LightComponent.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 前方宣言
class Camera;


/// <summary>
/// ライトをエディタ上で見せるための表示用のギズモ
/// <para>位置と向きはLightManagerがEntityのTransformから計算して渡す</para>
/// </summary>
class LightGizmo {
public:
	static void Initialize();

	/// <summary>
	/// 1回分の表示を始める。前回積んだ線を捨てる。
	/// </summary>
	static void Begin(Camera* camera);

	/// <summary>
	/// 平行光源を1つ積む。向きは円盤 ＋ 向きに伸びる線で表す（場所に意味は無いので、置いてある所に描くだけ）
	/// </summary>
	static void AddDirectionalLight(const Vector3& position, const Vector3& direction, const DirectionalLightComponent& light);

	/// <summary>
	/// ポイントライトを1つ積む。アイコンはその場で描き、線はEndでまとめて描く。
	/// </summary>
	static void AddPointLight(const Vector3& position, const PointLightComponent& light);

	/// <summary>
	/// スポットライトを1つ積む。範囲は円錐（底の円 ＋ 頂点から円への4本の線）で表す
	/// </summary>
	static void AddSpotLight(const Vector3& position, const Vector3& direction, const SpotLightComponent& light);

	/// <summary>
	/// 積んだ線を1回で描く
	/// </summary>
	static void End();

	// 全体の表示設定。ライトごとの設定とANDで判定（メニューバーの View から切り替える）
	static LightGizmoFlags& GetGlobalFlags() { return globalFlags_; }


private:
	// アイコンを1つ描く（ポイントライトとスポットライトで共通。絵だけ差し替える）
	static void DrawIcon(const Vector3& position, uint32_t textureHandle);

	static uint32_t pointIconTextureHandle_; // ポイントライトのアイコン（電球）
	static uint32_t spotIconTextureHandle_;  // スポットライトのアイコン（懐中電灯）
	static LightGizmoFlags globalFlags_;
	static Camera* camera_;                      // Begin～Endの間だけ使う。フレームをまたがない
	static bool isRecording_;                    // Begin～Endの間ならtrue
	static Renderer::LineListConfig rangeLines_; // 全ライト分の線の置き場。配列のメモリを使い回す
};
```

### ⑥ `MyEngine/Light/LightGizmo.cpp`（ファイル全体を差し替え）

```cpp
#include "LightGizmo.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

// 静的メンバ変数
uint32_t LightGizmo::pointIconTextureHandle_ = 0;
uint32_t LightGizmo::spotIconTextureHandle_ = 0;
LightGizmoFlags LightGizmo::globalFlags_;
Camera* LightGizmo::camera_ = nullptr;
bool LightGizmo::isRecording_ = false;
Renderer::LineListConfig LightGizmo::rangeLines_;

namespace {
constexpr float kPI = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPI / 180.0f;        // 度 → ラジアン
constexpr uint32_t kCircleDivision = 32;         // 円を何本の線で描くか
constexpr uint32_t kSpotEdgeCount = 4;           // スポットライトの頂点から円へ引く線の本数
constexpr uint32_t kRangeColor = 0xFFA500FF;     // 範囲の線の色（オレンジ）
constexpr uint32_t kDirectionColor = 0xFFE066FF; // 平行光源の線の色（黄色）
constexpr uint32_t kSunRayCount = 8;             // 平行光源の円盤から伸ばす線の本数
constexpr float kSunRadius = 0.5f;               // 平行光源の円盤の半径
constexpr float kSunRayLength = 2.0f;            // 平行光源の線の長さ
constexpr float kNoFadeStart = 10000.0f;         // 距離でフェードさせないための値
constexpr float kNoFadeEnd = 20000.0f;

/// <summary>
/// axisAとaxisBが作る平面上に、中心から半径radiusの円を線で並べる
/// <para>円周上の点 = 中心 + cos(角度) * axisA + sin(角度) * axisB</para>
/// </summary>
void PushCircle(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius, const Vector3& axisA, const Vector3& axisB, uint32_t color) {
	Vector3 prev = center + axisA * radius; // 角度0の点
	for (uint32_t i = 1; i <= kCircleDivision; ++i) {
		float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kCircleDivision);
		Vector3 current = center + axisA * (std::cos(angle) * radius) + axisB * (std::sin(angle) * radius);
		lines.push_back({prev, current, color});
		prev = current; // 次の線の始点にする
	}
}

/// <summary>
/// 向き（長さ1）と直交する2本の軸を作る（向きに垂直な円を描く平面）
/// <para>外積は平行なベクトル同士だと0になるので、向きとほぼ平行なら別の軸を基準にする</para>
/// </summary>
void MakePerpendicularAxes(const Vector3& direction, Vector3& axisA, Vector3& axisB) {
	Vector3 reference = (std::abs(direction.y) < 0.99f) ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
	axisA = Normalize(Cross(reference, direction));
	axisB = Cross(direction, axisA); // 直交する単位ベクトル同士の外積なので、長さは1
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void LightGizmo::Initialize() {
	pointIconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/pointLight.png");
	spotIconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/spotLight.png");
	// フェードしない値にする
	rangeLines_.fadeStartDistance = kNoFadeStart;
	rangeLines_.fadeEndDistance = kNoFadeEnd;
}


//=============================================================================
// 開始 / 終了
//=============================================================================
void LightGizmo::Begin(Camera* camera) {
	MY_ASSERT_MSG(!isRecording_, "LightGizmo::End を呼ぶ前に Begin が呼ばれました");
	isRecording_ = true;
	camera_ = camera;
	rangeLines_.camera = camera;
	rangeLines_.lines.clear(); // 中身だけ消す。確保済みのメモリは残るので、次から再確保が起きにくい
}

void LightGizmo::End() {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼ばずに End が呼ばれました");
	Renderer::DrawLines(rangeLines_); // 線が0本なら中で何もしない
	camera_ = nullptr;                // フレームをまたいで持たない
	rangeLines_.camera = nullptr;
	isRecording_ = false;
}


//=============================================================================
// アイコン（共通）
//=============================================================================
void LightGizmo::DrawIcon(const Vector3& position, uint32_t textureHandle) {
	Renderer::Rect3dConfig icon;
	icon.textureHandle = textureHandle;
	icon.shadingType = ShadingType::Unlit;
	icon.blendMode = BlendMode::Normal;
	icon.rasterizerType = RasterizerType::SolidNone;
	icon.depthMode = DepthMode::TestNoWrite; // 半透明として描く（深度を書かない。Skyboxの後に奥から順に描かれる）
	icon.isBillboard = true;
	icon.camera = camera_;
	icon.transform.translation = position;
	Renderer::DrawRect3d(icon);
}


//=============================================================================
// 平行光源
//=============================================================================
void LightGizmo::AddDirectionalLight(const Vector3& position, const Vector3& direction, const DirectionalLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddDirectionalLight を呼んでください");
	if (!globalFlags_.showRange || !light.gizmo.showRange) {
		return;
	}
	// 向きに垂直な円盤と、円盤の縁と中心から向きの方へ伸びる線（Unityの平行光源のギズモと同じ形）
	Vector3 axisA;
	Vector3 axisB;
	MakePerpendicularAxes(direction, axisA, axisB);
	PushCircle(rangeLines_.lines, position, kSunRadius, axisA, axisB, kDirectionColor);
	rangeLines_.lines.push_back({position, position + direction * kSunRayLength, kDirectionColor});
	for (uint32_t i = 0; i < kSunRayCount; ++i) {
		float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kSunRayCount);
		Vector3 start = position + axisA * (std::cos(angle) * kSunRadius) + axisB * (std::sin(angle) * kSunRadius);
		rangeLines_.lines.push_back({start, start + direction * kSunRayLength, kDirectionColor});
	}
}


//=============================================================================
// ポイントライト
//=============================================================================
void LightGizmo::AddPointLight(const Vector3& position, const PointLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddPointLight を呼んでください");

	// 全体とライトごと、両方ONのときだけ出す
	bool showIcon = globalFlags_.showIcon && light.gizmo.showIcon;
	bool showRange = globalFlags_.showRange && light.gizmo.showRange;

	// --- アイコン ---
	if (showIcon) {
		DrawIcon(position, pointIconTextureHandle_);
	}

	// --- 光の届く範囲。3方向の円を重ねて球に見せる。描くのはEndでまとめて ---
	if (showRange) {
		const Vector3 axisX = {1.0f, 0.0f, 0.0f};
		const Vector3 axisY = {0.0f, 1.0f, 0.0f};
		const Vector3 axisZ = {0.0f, 0.0f, 1.0f};
		PushCircle(rangeLines_.lines, position, light.radius, axisX, axisY, kRangeColor); // XY平面
		PushCircle(rangeLines_.lines, position, light.radius, axisY, axisZ, kRangeColor); // YZ平面
		PushCircle(rangeLines_.lines, position, light.radius, axisZ, axisX, kRangeColor); // ZX平面
	}
}


//=============================================================================
// スポットライト
//=============================================================================
void LightGizmo::AddSpotLight(const Vector3& position, const Vector3& direction, const SpotLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddSpotLight を呼んでください");

	bool showIcon = globalFlags_.showIcon && light.gizmo.showIcon;
	bool showRange = globalFlags_.showRange && light.gizmo.showRange;

	// --- アイコン ---
	if (showIcon) {
		DrawIcon(position, spotIconTextureHandle_);
	}

	// --- 光の届く範囲（円錐） ---
	if (showRange) {
		// 向きと直交する2本の軸を作る（円を描く平面）。directionはLightManagerで正規化済み
		Vector3 axisA;
		Vector3 axisB;
		MakePerpendicularAxes(direction, axisA, axisB);

		// 底の円：頂点から斜めにrange進んだ所（光が届く距離の端）
		float outerAngle = std::clamp(light.outerAngle, 0.0f, SpotLightComponent::kMaxAngle) * kDegToRad;
		float radius = light.range * std::sin(outerAngle);                            // 円の半径
		Vector3 center = position + direction * (light.range * std::cos(outerAngle)); // 円の中心
		PushCircle(rangeLines_.lines, center, radius, axisA, axisB, kRangeColor);

		// 頂点から円へ線を引いて、円錐に見せる
		for (uint32_t i = 0; i < kSpotEdgeCount; ++i) {
			float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kSpotEdgeCount);
			Vector3 edge = center + axisA * (std::cos(angle) * radius) + axisB * (std::sin(angle) * radius);
			rangeLines_.lines.push_back({position, edge, kRangeColor});
		}
	}
}
```

### ⑦ 新規：`MyEngine/Editor/Inspector/LightEditor.h`

```cpp
#pragma once
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Light/LightComponent.h"

// ライトのInspector（3種類。どれもカテゴリはLighting）
// 位置と向きはComponentに無いので、TransformのPosition / Rotationで動かす


/// <summary>
/// 平行光源（Transformの前＝+Zの向きに照らす）
/// </summary>
class DirectionalLightEditor : public TypedComponentEditor<DirectionalLightComponent> {
public:
	const char* GetName() const override { return "Directional Light"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Lighting; }

protected:
	void DrawComponent(DirectionalLightComponent& light) const override;
};


/// <summary>
/// ポイントライト（Transformの位置から全方向を照らす）
/// </summary>
class PointLightEditor : public TypedComponentEditor<PointLightComponent> {
public:
	const char* GetName() const override { return "Point Light"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Lighting; }

protected:
	void DrawComponent(PointLightComponent& light) const override;
};


/// <summary>
/// スポットライト（Transformの位置から、前＝+Zの向きを照らす）
/// </summary>
class SpotLightEditor : public TypedComponentEditor<SpotLightComponent> {
public:
	const char* GetName() const override { return "Spot Light"; }
	ComponentCategory GetCategory() const override { return ComponentCategory::Lighting; }

protected:
	void DrawComponent(SpotLightComponent& light) const override;
};
```

### ⑧ 新規：`MyEngine/Editor/Inspector/LightEditor.cpp`

```cpp
#include "LightEditor.h"

#include <externals/imgui/imgui.h>

namespace {
constexpr float kMaxIntensity = 100.0f; // 強さの上限（ドラッグで行き過ぎないように）

// このライトのギズモの表示（全体のON / OFFはメニューバーの View）
void DrawGizmoFlags(LightGizmoFlags& gizmo) {
	ImGui::Checkbox("Icon", &gizmo.showIcon);
	ImGui::SameLine();
	ImGui::Checkbox("Range", &gizmo.showRange);
}
} // namespace


//=============================================================================
// 平行光源
//=============================================================================
void DirectionalLightEditor::DrawComponent(DirectionalLightComponent& light) const {
	ImGui::ColorEdit3("Color", &light.color.x);
	ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, kMaxIntensity);
	ImGui::Checkbox("Show Direction", &light.gizmo.showRange); // アイコンの絵はまだ無いので、向きの線だけ
	ImGui::TextDisabled("向きは Transform の Rotation（+Zの向きに照らす）");
}


//=============================================================================
// ポイントライト
//=============================================================================
void PointLightEditor::DrawComponent(PointLightComponent& light) const {
	ImGui::ColorEdit3("Color", &light.color.x);
	ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, kMaxIntensity);
	ImGui::DragFloat("Radius", &light.radius, 0.05f, 0.0f, 1000.0f);
	ImGui::DragFloat("Decay", &light.decay, 0.01f, 0.0f, 10.0f);
	DrawGizmoFlags(light.gizmo);
}


//=============================================================================
// スポットライト
//=============================================================================
void SpotLightEditor::DrawComponent(SpotLightComponent& light) const {
	ImGui::ColorEdit3("Color", &light.color.x);
	ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, kMaxIntensity);
	ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 1000.0f);
	ImGui::DragFloat("Decay", &light.decay, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Outer Angle", &light.outerAngle, 0.1f, 0.0f, SpotLightComponent::kMaxAngle);
	ImGui::DragFloat("Inner Angle", &light.innerAngle, 0.1f, 0.0f, light.outerAngle); // 外側より大きくできないようにする
	DrawGizmoFlags(light.gizmo);
	ImGui::TextDisabled("向きは Transform の Rotation（+Zの向きに照らす）");
}
```

### ⑨ `MyEngine/Editor/Windows/InspectorWindow.cpp`

include に1行（`ComponentEditor.h` の下）。

```cpp
#include "MyEngine/Editor/Inspector/LightEditor.h"
```

`Initialize` を置き換える。

```cpp
void InspectorWindow::Initialize() {
	// エンジンのComponent。Inspectorの区画はカテゴリ順（同じカテゴリの中は登録した順）に並ぶ
	ComponentEditorRegistry::Register(std::make_unique<TransformEditor>());
	ComponentEditorRegistry::Register(std::make_unique<ModelRendererEditor>());
	ComponentEditorRegistry::Register(std::make_unique<DirectionalLightEditor>());
	ComponentEditorRegistry::Register(std::make_unique<PointLightEditor>());
	ComponentEditorRegistry::Register(std::make_unique<SpotLightEditor>());
}
```

### ⑩ `MyEngine/Editor/Windows/HierarchyWindow.cpp`

include に2行（`<numbers>` と `LightComponent.h`）。

```cpp
#include "HierarchyWindow.h"

#include <algorithm>
#include <cfloat>
#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Light/LightComponent.h"
```

無名namespaceの先頭に1行。

```cpp
namespace {
constexpr const char* kDragDropType = "ENTITY_HANDLE";          // ドラッグで運ぶものの種類の名前
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
```

`DrawCreateMenu` を置き換える。

```cpp
void HierarchyWindow::DrawCreateMenu() {
	if (ImGui::MenuItem("Create Entity")) {
		EditorHistory::RequestCreate("Entity", {});
	}
	if (ImGui::MenuItem("Create Child", nullptr, false, selected_.IsValid())) {
		EditorHistory::RequestCreate("Child", selected_);
	}
	// --- ライト：Componentを付けた状態で作る（1回のUndoでEntityごと消える）---
	if (ImGui::BeginMenu("Light")) {
		if (ImGui::MenuItem("Directional Light")) {
			TransformComponent transform;
			transform.translation = {0.0f, 3.0f, 0.0f};                          // 位置は照らし方に関係ない（ギズモを描く場所）
			transform.rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f}; // 斜め上から照らす（Unityの最初のシーンと同じ位置・向き）
			EditorHistory::RequestCreate("Directional Light", {}, {ComponentSnapshot::Make(transform), ComponentSnapshot::Make(DirectionalLightComponent{})});
		}
		if (ImGui::MenuItem("Point Light")) {
			EditorHistory::RequestCreate("Point Light", {}, {ComponentSnapshot::Make(PointLightComponent{})});
		}
		if (ImGui::MenuItem("Spot Light")) {
			TransformComponent transform;
			transform.rotation.x = 90.0f * kDegToRad; // 真下を照らす（前＝+Zを、X軸で90度倒す）
			EditorHistory::RequestCreate("Spot Light", {}, {ComponentSnapshot::Make(transform), ComponentSnapshot::Make(SpotLightComponent{})});
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	// 何も無い所から貼るので、一番上（root）に貼る
	if (ImGui::MenuItem("Paste", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste({});
	}
}
```

### ⑪ `MyEngine/Editor/ImGuiManager.cpp`

include を1行置き換える（確認用ウィンドウが無くなるので、LightManagerはもう使わない）。

```cpp
#include "MyEngine/Light/LightGizmo.h"
```

メニューバーの Edit の後ろに View を足す。

```cpp
		// --- View ---
		if (ImGui::BeginMenu("View")) {
			// ライトのギズモ（全部まとめてON / OFF。1つずつはInspectorのライトの区画で）
			LightGizmoFlags& lightGizmo = LightGizmo::GetGlobalFlags();
			ImGui::SeparatorText("Gizmos");
			ImGui::MenuItem("Light Icons", nullptr, &lightGizmo.showIcon);
			ImGui::MenuItem("Light Ranges", nullptr, &lightGizmo.showRange);
			ImGui::EndMenu();
		}
```

`Begin` の中の次の3行を消す。

```cpp
	// ===== ライトの確認用ウィンドウ =====
	LightManager::DrawDebugWindow();

```

### ⑫ `MyEngine/Engine.cpp`

`Initialize` の中の2行の順番を入れ替える（`LightManager::Initialize` が `EntityManager::RegisterComponent` を呼ぶので）。

```cpp
	LightGizmo::Initialize();
	EntityManager::Initialize();
	LightManager::Initialize(); // ライトのComponentをEntityManagerに登録するので、EntityManagerの後
```

`Finalize` の `LightManager::Release(); EntityManager::Release();` の順番はそのままでよい（LightManagerはもう実体を持たない）。

### ⑬ ゲーム：`CG3_Project/GameScene.cpp`

include を2つ足す（`<numbers>` は `GameComponents.h` の下、`LightComponent.h` は `InputManager.h` の下）。

```cpp
#include "GameScene.h"
#include "GameComponents.h"
#include <numbers>
```

```cpp
#include <MyEngine/Light/LightComponent.h>
```

`#include <externals/imgui/imgui.h>` の下（`#ifdef USE_IMGUI` の外）に足す。

```cpp
namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace
```

`Initialize` の `sceneRoot_ = EntityManager::Create("GameScene");` の直後に足す。

```cpp
	// 平行光源（ライトもEntityに付けるComponent。向きはTransformの回転で、+Zの向きに照らす）
	// 置き場所・向きはUnityの最初のシーンと同じ（位置は照らし方に関係なく、ギズモを描く場所なだけ）
	const Handle<Entity> sun = EntityManager::Create("Directional Light", sceneRoot_);
	TransformComponent* sunTransform = EntityManager::Get<TransformComponent>(sun); // Transformは作った直後から取れる
	sunTransform->translation = {0.0f, 3.0f, 0.0f};
	sunTransform->rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f};
	EntityManager::RequestAdd<DirectionalLightComponent>(sun);
```

### 解説

**位置と向きをTransformから取る理由**

- Unityと同じ形。ライトを他のEntityの子にすれば、親と一緒に動く（例：回るMonsterBallの子にしたライトは一緒に回る）
- 位置の持ち場所を1つにする。Componentにも `position` があると、「TransformのPositionとライトのPositionのどちらが本当か」が分からなくなる
- 位置と向きはComponentに入っていないので、コピー・Undoも、Transformの分と合わせて今までどおり効く

**「前」＝ローカルの+Z ＝ worldMatrix の3行目**

このエンジンの行列は「行ベクトル × 行列」の形（`MakeAffineMatrix` が S×R×T）。ローカルの(0,0,1)を変換すると、行列の3行目がそのまま出てくる。平行移動は4行目。

| Rotation（度） | 照らす向き（テストで計算した値） |
|---|---|
| (0, 0, 0) | (0, 0, 1)：奥（カメラの向こう）へ |
| (90, 0, 0) | (0, -1, 0)：真下へ（Createメニューのスポットライト） |
| (50, -30, 0) | (-0.32, -0.77, 0.56)：カメラ側の右上から、左奥の下へ（Createメニュー・GameSceneの平行光源。Unityの最初のシーンと同じ） |

Scaleが0だと向きの長さが0になるので、そのときは前と同じく真下にしている（`NormalizeDirection`）。

**平行光源は1つだけ**

シェーダーは平行光源を1つ分しか持たない。有効な物のうち**最初に見つかった物**を使い、2つ以上あればLogに警告を1回出す。「最初」はComponentの配列の順番で、削除で入れ替わるので、2つ置かないのがよい。どれを使うかのルール（一番明るい物、指定した物など）は、必要になってから決める。
1つも無ければ `intensity = 0`（照らさない）。前は LightManager がいつも1つ持っていたので、**Step 6の後はシーンが Directional Light を作る**（⑬）。平行光源の位置は照らし方に関係なく、ギズモを描く場所なだけ。

**LightManager の役目が変わる**

前は「ライトの実体の持ち主」だったが、実体はEntityManagerへ移った。LightManagerは「ライトの型を登録する」「毎フレーム `ForEach` で集めてGPU用にまとめる」「ギズモを描く」だけになる（ModelRenderSystemと同じ立場）。名前は今回は変えない（`LightSystem` にしてもよい）。
実体の持ち主が1つになったので、Undo・コピー・削除・シーンの作り直し（`sceneRoot_` の破棄）がEntityManagerだけで済む。前は「シーンが追加したライトはFinalizeで消す」を自分で書く必要があった。

**初期化の順番**

依存の向きは「Light → Entity」（EntityManagerはライトを知らない）。LightManagerが自分の型を `RegisterComponent` するので、EntityManagerを先に初期化する（⑫）。

**1フレームの遅れ**

`RequestAdd` は次のフレームの最初に付く。GameSceneのDirectional Lightは、最初の1フレームだけ平行光源無しで描かれる。消すときも、破棄はフレームの最後（ライトを集めた後）なので、照らさなくなるのは次のフレームから。どちらも目で見て分かる差ではない。

**ギズモ**

- 自分か親が無効なライトはギズモも出さない（照らさない物を見せない）
- 平行光源は「向きに垂直な円盤＋円盤の縁と中心から向きの方へ伸びる9本の線」（黄色）。アイコンの絵がまだ無いので、Inspectorでは `Show Direction`（中身は `gizmo.showRange`）だけ出す。全体のOFF（View → Light Ranges）でも消える
- ポイントライト・スポットライトは前と同じ形。位置・向きをTransformから渡すようになっただけ

### 確認すること

1. 起動 → Hierarchyの `GameScene` の下に `Directional Light` がある。MonsterBallが斜め上から照らされる（前は真上から）
2. `Directional Light` を選ぶ → Inspectorに Transform と Directional Light。Rotationを変えると照らす向きと黄色い線の向きが変わる。Intensityを0にすると平行光源が消える
3. ＋ → Light → **Point Light** → 原点に電球のアイコンとオレンジの球。Positionで動かせる。Radius・Colorが効く
4. ＋ → Light → **Spot Light** → 真下を照らす円錐。Rotation Xを変えると傾く。Outer / Inner Angleが効く
5. Point Light を MonsterBall にドラッグして子にする → MonsterBallを動かすとライトも動く。**Play** するとSpinで回るMonsterBallと一緒に回る
6. ライトのEntityのチェックを外す → そのライトは照らさない・ギズモも消える
7. View → Gizmos → Light Icons / Light Ranges のチェックを外すと、全部のアイコン・線が消える。ライトごとの Icon / Range も効く
8. ライトの値の変更・Delete・Ctrl+D・Ctrl+Z / Ctrl+Y が効く。＋ → Light で作った物は、Ctrl+Z 1回でEntityごと消える
9. Point Light を選んで Ctrl+D を続けて65個以上にする → Logに上限の警告が1回だけ出る
10. Directional Light をもう1つ作る → Logに警告が1回。照らすのは先にあった方
11. Releaseでビルドして起動 → ライトは照らす（ギズモ・Inspectorは無い）

### 確認したこと（2026-09-19、Claude）

- エンジン78ファイル（77＋LightEditor.cpp）・ゲーム5ファイルを `/W4` の Debug / Release でコンパイル。エラー0、新しい警告0
- テスト（[Entity.md](Entity.md) の「確認したこと」と同じもの）でライトについて確かめたこと：
  - 平行光源が無いと intensity 0。(50,-30,0)度 → (-0.321, -0.766, 0.557)、長さ1。回転0 → (0,0,1)
  - 平行光源が2つ → 先の方だけ使い、警告は1回
  - スポットライト：X90度で(0,-1,0)。位置はTransformの位置
  - ポイントライト：親(10,0,0)の子(0,2,0) → (10,2,0)。親を無効にすると集めない・ギズモも出ない
  - 70個置くと64個まで・警告1回。Scaleが0でも向きの長さは1
  - Createメニューと同じ呼び方（Transform＋PointLightの写しを渡す）で作る → 同じフレームにライトとして集まる → Undoで消える → Redoで同じ値で戻る
- 実際の絵（照らされ方・ギズモの形）は見ていない

### 次のStepでやること（ここではやらない）

- ギズモをSceneビューだけに出す（「未解決」の1つ目。RenderQueueにビューの区別を付ける。Editor.md）
- 選んでいるライトのギズモを目立たせる、Sceneビューでクリックして選ぶ
- 平行光源のアイコン（画像が要る）
- その後の予定：HDR化 → 描画サイズの可変 → 影（シャドウマップ）

## Step 6.1：Step 6 を実行して見つかった直し（2026-09-20）

| # | 直すこと | 理由 |
|---|---|---|
| ① | スポットライトのInspectorで `Range` を触ると「conflicting ID」のエラーが出る | 同じ区画に `Range` という名前が2つ（距離のドラッグと、ギズモのチェック）あるため |
| ② | 平行光源のアイコン（太陽）を出す | 画像 `directionalLight.png` を用意したので |
| ③ | `LightManager` → `LightSystem` に改名 | もう実体を持たず、集めて渡すだけになったので（`ModelRenderSystem` と同じ立場） |
| ④ | `ImGuiManager.cpp` の使っていない `#include` を消す | Step 6で `LightManager` を使わなくなった |

### ① ImGuiの「同じIDの項目が2つある」

ImGuiは項目を**名前から作ったID**で区別する（見た目ではない）。同じウィンドウ・同じIDの縄張りの中に同じ名前の項目が2つあると、どちらを操作しているか決められないので、あの赤い枠と警告が出る。
`SpotLightEditor` には距離の `DragFloat("Range")` があり、その下の `DrawGizmoFlags` にも `Checkbox("Range")` があった（ポイントライトは距離が `Radius` なのでぶつからなかった）。

直し方は3つあって、今回は3番目にした。

1. 名前を変える（`Range Wire` など）… 表示が長くなる
2. `##` で「見えない部分」を足す（`Range##gizmo`）… 1か所だけならこれでよい
3. **`PushID` / `PopID` で名前の縄張りを作る**… 中の名前を何も気にしなくてよくなる

`MyEngine/Editor/Inspector/LightEditor.cpp` の `DrawGizmoFlags` を置き換える。

```cpp
// このライトのギズモの表示（全体のON / OFFはメニューバーの View）
// PushIDで名前の縄張りを作る。そうしないと、同じ区画の中に "Range" が2つ（スポットライトの距離とこのチェック）できて、
// ImGuiが「同じIDの項目が2つある」と怒る（ImGuiは項目を見た目ではなく「名前から作ったID」で区別している）
void DrawGizmoFlags(LightGizmoFlags& gizmo) {
	ImGui::PushID("gizmo");
	ImGui::TextDisabled("Gizmo");
	ImGui::SameLine();
	ImGui::Checkbox("Icon", &gizmo.showIcon);
	ImGui::SameLine();
	ImGui::Checkbox("Range", &gizmo.showRange);
	ImGui::PopID();
}
```

**覚えておくと良いこと**：Inspectorの区画ごとには `InspectorWindow::DrawComponent` が `PushID(editor.GetName())` をしているので、**別のComponent同士**で名前がぶつかることはない。ぶつかるのは1つのComponentの中だけ。

### ② 平行光源のアイコン（太陽）

まず画像のファイル名を直す：`Resources/Textures/directionlLight.png` → **`directionalLight.png`**（`a` が抜けている）。

`MyEngine/Light/LightGizmo.h` のメンバを3つに。

```cpp
	static uint32_t directionalIconTextureHandle_; // 平行光源のアイコン（太陽）
	static uint32_t pointIconTextureHandle_;       // ポイントライトのアイコン（電球）
	static uint32_t spotIconTextureHandle_;        // スポットライトのアイコン（懐中電灯）
```

クラスの説明の `AddDirectionalLight` のコメントも直す（任意）。

```cpp
	/// <summary>
	/// 平行光源を1つ積む。太陽のアイコン ＋ 円盤と向きに伸びる線（場所に意味は無いので、置いてある所に描くだけ）
	/// </summary>
```

`MyEngine/Light/LightGizmo.cpp` の静的メンバの定義に1行足す。

```cpp
// 静的メンバ変数
uint32_t LightGizmo::directionalIconTextureHandle_ = 0;
uint32_t LightGizmo::pointIconTextureHandle_ = 0;
uint32_t LightGizmo::spotIconTextureHandle_ = 0;
```

`Initialize` に1行足す。

```cpp
	directionalIconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/directionalLight.png");
```

`AddDirectionalLight` を置き換える（他の2つと同じ「アイコン ＋ 範囲」の形にそろえる）。

```cpp
void LightGizmo::AddDirectionalLight(const Vector3& position, const Vector3& direction, const DirectionalLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddDirectionalLight を呼んでください");

	// 全体とライトごと、両方ONのときだけ出す
	bool showIcon = globalFlags_.showIcon && light.gizmo.showIcon;
	bool showRange = globalFlags_.showRange && light.gizmo.showRange;

	// --- アイコン（太陽）---
	if (showIcon) {
		DrawIcon(position, directionalIconTextureHandle_);
	}

	// --- 向き。円盤と、円盤の縁と中心から向きの方へ伸びる線（Unityの平行光源のギズモと同じ形）---
	if (showRange) {
		Vector3 axisA;
		Vector3 axisB;
		MakePerpendicularAxes(direction, axisA, axisB);
		PushCircle(rangeLines_.lines, position, kSunRadius, axisA, axisB, kDirectionColor);
		rangeLines_.lines.push_back({position, position + direction * kSunRayLength, kDirectionColor});
		for (uint32_t i = 0; i < kSunRayCount; ++i) {
			float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kSunRayCount);
			Vector3 start = position + axisA * (std::cos(angle) * kSunRadius) + axisB * (std::sin(angle) * kSunRadius);
			rangeLines_.lines.push_back({start, start + direction * kSunRayLength, kDirectionColor});
		}
	}
}
```

`MyEngine/Editor/Inspector/LightEditor.cpp` の平行光源の欄も、`Show Direction` のチェック1つから、他と同じ Icon / Range にそろえる。

```cpp
void DirectionalLightEditor::DrawComponent(DirectionalLightComponent& light) const {
	ImGui::ColorEdit3("Color", &light.color.x);
	ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, kMaxIntensity);
	DrawGizmoFlags(light.gizmo); // Iconは太陽の絵、Rangeは向きの線
}
```

### ③ `LightManager` → `LightSystem`

やることは名前の置き換えだけ（中身は変えない）。

1. `MyEngine/Light/LightManager.h` / `.cpp` の**ファイル名**を `LightSystem.h` / `LightSystem.cpp` に変える（エクスプローラーで名前変更 → VSのプロジェクトからは古い名前を削除して、新しいファイルを追加）
2. その2ファイルの中の `LightManager` を全部 `LightSystem` に置き換える（`LightSystem.cpp` の先頭の `#include "LightManager.h"` も）
3. 呼んでいる所を置き換える
   - `MyEngine/Engine.cpp`：`LightSystem::Initialize();` と `LightSystem::Release();`
   - `MyEngine/Window/WindowManager.cpp`：include 1つと `LightSystem::Update();` `LightSystem::DrawGizmos(gizmoCamera);`
   - `MyEngine/Light/LightIncludes.h`：include 1つ
4. コメントの中の「LightManager」も直す（`Entity/EntityManager.cpp` の18行目、`Graphics/Renderer/Renderer.h` の306行目、`Light/LightGizmo.h` の13行目、`Light/LightGizmo.cpp` の183行目）
5. ゲーム側に配られた古いヘッダーを消す：`CG3_Project/MyEngine/include/MyEngine/Light/LightManager.h`（xcopyは消えたファイルを消さないので、残っていると古いものをincludeできてしまう）

### ④ `MyEngine/Editor/ImGuiManager.cpp`

22行目の `#include "MyEngine/Light/LightManager.h"` を消す（13行目の `LightGizmo.h` だけでよい）。

### 確認すること

1. スポットライトの Range を触っても、もうエラーの枠が出ない
2. 平行光源のEntityの所に太陽のアイコンが出る。Inspectorの Gizmo の Icon / Range で1つずつ消せる。View → Gizmos でも消える
3. 今までどおりライトが照らす（改名だけなので見た目は変わらない）

### 確認したこと（2026-09-20、Claude）

- ①〜④を当てたエンジン78ファイルと、ゲーム5ファイルを `/W4` の Debug / Release でコンパイル。エラー0、新しい警告0
- 太陽の画像は他のアイコンと同じ「黒い線＋透明」なので、`DrawIcon`（Unlit・半透明）でそのまま出る。色は付けていない

---




> 整理済み：コミット済みの旧写経コードを省略。実装と旧手順はコミット `936cdcf` を参照。以下は設計理由・確認項目の記録で、再適用する手順ではない。

<details>
<summary>過去の変更と解説を開く</summary>

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

`Initialize()` の中（置く場所と設定）

`Draw()` の中（他の描画と同じ場所でよい）
- `Renderer::Draw〇〇` は「描画のお願いをキューに積む」だけなので、`Draw()` のどこで呼んでもよい。実際に描かれるのはフレームの最後。
- カメラは `Begin` から `End` の間だけ使われ、`End` で手放される（フレームをまたいで持たない）。
全体の表示を切り替えるときは次のように書く。
1個だけ切り替えるときは次のように書く。

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

`Initialize()`

`Draw()`（モデルなどを描く直前に、毎フレーム設定する）

`Finalize()`

途中でライトを動かすとき（`Update()` など）

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

> **【要修正 2026-09-17】コミット `bb177b3` の `LightManager.h` 34行目が、Step 4の形のまま残っている。** `DirectionalLight` クラスは削除済みなので、`LightManager.cpp` / `WindowManager.cpp` / `Engine.cpp` のコンパイルが通らない（実際にコンパイルして確認）。①の通り、次の1行に直す。直せば、このコミットで変わったファイルはすべてコンパイルが通る（確認済み）。
> ```cpp
> 	// 変更前（Step 4のまま）
> 	static DirectionalLight* GetDirectionalLight() { return &instance_->directionalLight_; }
> 	// 変更後
> 	static DirectionalLightComponent& GetDirectionalLight() { return instance_->directionalLight_; }
> ```

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

### ② `MyEngine/Light/LightManager.cpp`（ファイル全体を差し替え）

### ③ `MyEngine/Light/LightIncludes.h`（ファイル全体を差し替え）
- 今は `LightManager.h` が `LightIncludes.h` を読み、`LightIncludes.h` も `LightManager.h` を読む「お互いにinclude」の状態になっている（`#pragma once` のおかげで動いているだけ）。①で `LightManager.h` は `LightComponent.h` だけを読むようにしたので、この循環も無くなる。

### ④ `MyEngine/Graphics/Renderer/Renderer.h`

**削除する**：前方宣言の2行

**削除する**：次の2行を、`ModelConfig` / `TriangleConfig` / `SphereConfig` / `Rect3dConfig` / `Quad3dConfig` / `AABBConfig` / `OBBConfig` の**7か所すべて**から消す（Ctrl+Fで `directionalLight` を検索すると見つけやすい）

**追加する**：`static void DrawLines(const LineListConfig& config);` の下（`private:` の上）

**追加する**：`private:` の中の `static Renderer* instance_;` の下

### ⑤ `MyEngine/Graphics/Renderer/Renderer.cpp`

**削除する**：includeの4行（ライトの2行と、警告関数でしか使っていなかった2行）

**削除する**：警告関数を丸ごと

**追加する**：`Renderer::Initialize()` の下

**`PushMesh` の中**：先頭の警告の3行を削除する
ライトを詰める部分を差し替える。

変更前
変更後

**`DrawModel` の中**：先頭のアサートを削除する（`// 参照するモデル` 以降の早期リターンは残す）
ライトを詰める部分を差し替える。

変更前
変更後

### ⑥ `MyEngine/Graphics/Model/ModelManager.h`（任意）
使われていない前方宣言を削除する。

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

`Initialize()`

`Draw()`

`Finalize()`

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

**まだ残っている無駄（Step 5.2で直す）**
- ライトのデータはフレームに1回まとめるようになったが、`MeshRequest` への値のコピーと、`RenderContext` のリングバッファへの書き込みは、まだ描画1回ごとに行っている（ポイントライト16個分で約1KB × 描画回数）。
- どの描画でもライトは同じなので、Step 5.2で「1フレームに1回だけGPUに書いて、全描画で同じ場所を結ぶ」形にする。そうすると上限を上げてもコストがほとんど増えない。

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
- Step 5.1：ライトの確認用ウィンドウを作り、ゲーム側のライトのコードを無くす。
- Step 5.2：ライトの定数バッファを1フレームに1回だけ書いて、全描画で共有する。`MeshRequest` からライトのデータを外し、RenderContextのライト用リングバッファを消す。その後で `kMaxPointLights` を64に上げる。

---

## Step 5.1：ライトの確認用ウィンドウ（ゲーム側のライトのコードを無くす）

> **始める前に**：Step 5の冒頭の【要修正】（`LightManager.h` 34行目）を直しておくこと。

### 目的
- ゲーム側にライトのコードを書かなくても、実行中にライトの追加・削除・値の変更ができるようにする。
- Step 5.2（上限を64に上げる）の確認ではライトを大量に置く必要がある。ゲーム側で書くのは手間なので、先にこれを作る。
- Inspector（Step 6）ができるまでの仮。Inspectorができたら消す。

### ゲーム側のコードの扱い
- **ライトに関するコードは全部消してよい**（`pointLightHandles_`、`Initialize()` のライトの設定、`Finalize()` の削除）。
- ただし、**カメラと、Unlit以外のモデルを描くシーンは残す**。エンジンは静的ライブラリなので単体では実行できず、ライトの当たり方はモデルが無いと見えない。
- ゲーム側のプロジェクトも、消す前にコミットしておく。

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Light/LightManager.h` | `DrawDebugWindow` の宣言と、`debugPointLights_` を追加 |
| `MyEngine/Light/LightManager.cpp` | includeを追加、`DrawDebugWindow` を追加 |
| `MyEngine/Editor/ImGuiManager.cpp` | includeと、`DrawDebugWindow` の呼び出しを追加 |
| ゲーム側のシーン | ライトに関するコードを削除 |

> DebugビルドとReleaseビルド（`USE_IMGUI` なし）の両方で、コンパイルが通ることを確認済み（`/W4`）。

---

### ① `MyEngine/Light/LightManager.h`

**追加する**：`GetPointLight` の宣言の下（`private:` の上）

**追加する**：`private:` の中の `hasWarnedPointLightLimit_` の下

### ② `MyEngine/Light/LightManager.cpp`

**includeの部分を次のようにする**（`<numbers>` とImGuiを追加）

**追加する**：ファイルの最後

### ③ `MyEngine/Editor/ImGuiManager.cpp`

**追加する**：includeの `Profiler.h` の下

**追加する**：`ImGuiManager::Begin()` の最後（`Profiler::Draw();` の下）

### ④ ゲーム側のシーン
- ライトに関するメンバ、`Initialize()` のライトの設定、`Finalize()` の削除を全部消す。
- `#include "MyEngine/Light/LightManager.h"` も、ほかで使っていなければ消す。
- カメラとモデルの描画は残す。

---

### 解説

**なぜ `ImGuiManager::Begin` に置くのか**
- Log・Parameters・Profilerのウィンドウも同じ場所で描いている。ImGuiのフレームの中で毎フレーム1回呼ばれる。

**値の変更が反映されるタイミング**
- ImGuiの処理は、ゲームの `Update` / `Draw` とLightManagerの収集が終わった後（描画コマンドを積む直前）に行われる。
- なので、ウィンドウで変えた値や削除は**次のフレーム**の収集で反映される。1フレーム遅れるが、見た目では分からない。

**`debugPointLights_` を `#ifdef USE_IMGUI` で囲まない理由**
- エンジン（.lib）とゲーム（.exe）はそれぞれ別にコンパイルされる。片方だけ `USE_IMGUI` が定義されていると、同じ `LightManager` クラスなのに大きさが食い違い、メンバを読む位置がずれてメモリを壊す（ODR違反と呼ばれる、見つけにくい不具合）。
- メンバ変数を1つ余分に持つだけなら害は無いので、囲まないほうが安全。関数の宣言は囲ってもクラスの大きさが変わらないのでよい。

**「Remove Added」が、このウィンドウで追加した分だけ消す理由**
- `SlotMap` は「要素 → Handle」の逆引きができない。Handleを覚えているのは、このウィンドウで追加した分だけ。
- ゲーム側が追加したライトを、確認用のウィンドウが勝手に消さないようにするためでもある。

**`PushID` と `TreeNode("PointLight", "Point %d", index)` を使う理由**
- ImGuiは「ラベルの文字列」で部品を見分ける。`"Position"` などの同じラベルがライトの数だけ並ぶので、IDを分けないと、1つを動かすと全部が一緒に動いてしまう。
- `PushID(index)` でライトごとにIDの区切りを作っている。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとReleaseの両方）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 「Lights」ウィンドウが出る
   - 「Add x10」で円状に10個並び、ギズモも出る。もう一度押すと、1周り外側に10個増える
   - ライトを開いて位置・色・半径を変えると、当たり方とギズモが変わる
   - 「Remove Added」で追加した分が全部消える
   - 「Icon (All)」「Range (All)」で全ライトのギズモを切り替えられる
   - 20個以上にすると、上限16個を超えた警告が1回だけ出る（Step 5.2で64に上げる前の確認。先に `kMaxPointLights` を64にした場合は、70個で64個の警告）

### 次のStepでやること（ここではやらない）
- Step 5.2：ライトの定数バッファを全描画で共有し、上限を64に上げる。確認はこのウィンドウで行う。

---

## Step 5.1a：ギズモのアイコンの不具合修正

### 起きていたこと（Step 5.1をDebugで実行して見つかった）
1. Point 0, 1 の `Position` を動かすと、範囲の線は動くのにアイコンが動かない（または最初から見えない）
2. アイコンのチェックボックスを押すと、ほかのアイコンが消えることがある
3. アイコンの周りが背景色（クリアカラー）で四角く抜けることがある。位置によって変わる
4. Gameビューにも範囲とアイコンが出る → 既知の問題（「未解決」の1つ目）。このStepでは直さない

### 原因

**1と2：行列のバッファの同じ場所に、メッシュと線が書き込んでいた**
- `RenderContext` の行列のバッファ（`matricesDataRingBuffer_`）は、メッシュ（`DrawMesh`）と線（`DrawLines`）の両方が使っている。
- ところが書き込む場所を、メッシュは `drawCallIndex_`、線は `drawCallLineIndex_` という**別々のカウンタ**で決めていた。どちらも0から数えるので、線の1回目はメッシュの1個目と同じ場所、線の2回目はメッシュの2個目と同じ場所に書く。
- コマンドリストが覚えているのは「値」ではなく「バッファの場所」だけ。GPUが実際に読むのはフレームの最後なので、読まれるのは**最後に書いた値**になる。
- 線の行列は「単位行列・ビルボードなし」なので、上書きされたメッシュは原点に、カメラを向かない1×1の板として描かれる。
- 線はフレームに4回描かれる（カメラの視錐台とライトの範囲 × Scene / Gameビュー）ので、**フレームの最初の数個のメッシュ**が壊れる。Point 0, 1 のアイコンがその範囲に入っていた。
- チェックボックスで1つ消すと並びが1つ詰まり、別のアイコンが壊れる位置に入る → 「ほかのアイコンが消える」ように見える。
- Step 2でアイコンを出す前は、壊れる位置に入っていたのがグリッドなど元から単位行列のものだったので、上書きされても見た目が変わらず気づかなかった。

**3：アイコンを不透明として描いていた**
- `Rect3dConfig` の `depthMode` の初期値は `TestWrite`（不透明）。なので不透明のキューに入り、**Skyboxより前**に、深度を書きながら描かれていた。
- Unlitのシェーダーが捨てるのはアルファが**ちょうど0**のピクセルだけ。画像の縁のぼかしや、遠くで縮小して読んだときのにじみでアルファが少しでも残ると、見た目はほぼ透明なのに深度を書く。
- その後にSkyboxを描くと、そのピクセルだけ深度で負けて描かれない → 背景色が見える。
- 空と重なる位置では背景色が見え、先に描いたモデルと重なる位置では目立たない → 位置によって変わって見える。アイコン同士でも、先に描いたアイコンの透明な部分に後のアイコンが隠される。

**ついでに見つけたもの：3Dのインデックスの書き込み場所が進んでいなかった**
- `DrawMesh` の動的メッシュ（Rect3d、Sphere、AABBなど）で、`vertex3dIndex_` は進めているが `index3dIndex_` を進めていない（上限のアサートもコメントアウトされている）。全員がインデックスのバッファの**先頭**に書くので、1と同じ理由で、最後に書いたインデックスが全員に使われる。
- 今はアイコン（全部同じ `{0, 1, 2, 1, 3, 2}`）しか無いので表に出ていない。同じフレームに `DrawSphere` などを混ぜると、アイコンが球のインデックスで描かれて壊れる（頂点バッファの範囲外を読む）。
- 1と同じ「バッファとカウンタの対応ずれ」なので一緒に直す。

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Graphics/Renderer/RenderContext.cpp` | `DrawLines`：行列の場所を `drawCallIndex_` で決めて進める。`DrawMesh`：`index3dIndex_` を進める |
| `MyEngine/Graphics/Renderer/RenderContext.h`（任意） | カウンタのコメントだけ |
| `MyEngine/Light/LightGizmo.cpp` | アイコンを `DepthMode::TestNoWrite` で描く |

> スクラッチで、今の状態（5.1）に入れてDebug / Release、5.2を入れた後に入れてDebugでコンパイルが通ることを確認済み（`/W4` で警告なし）。5.2の変更とは場所が重ならないので、5.1aと5.2のどちらを先に入れてもよい。

---

### ① `MyEngine/Graphics/Renderer/RenderContext.cpp`

**`DrawMesh` の中**（`// --- 動的（Primitive）` の部分）：アサートのコメントを外す

変更前
変更後

**`DrawMesh` の中**：追加する（`instance_->vertex3dIndex_ += req.vertices.size();` の下）

**`DrawLines` の中**：追加する（`MY_ASSERT_MSG(instance_->drawCallLineIndex_ < kMaxDrawCalls, ...);` の下）

**`DrawLines` の中**：行列の場所を差し替える

変更前
変更後

**`DrawLines` の中**：追加する（関数の最後の `instance_->drawCallLineIndex_++;` の下）

### ② `MyEngine/Graphics/Renderer/RenderContext.h`（任意）

`// --- カウント ---` の最初の2行にコメントを付ける

### ③ `MyEngine/Light/LightGizmo.cpp`

**`AddPointLight` の中**：追加する（アイコンの `icon.rasterizerType = RasterizerType::SolidNone;` の下）

---

### 解説

**なぜ線用に行列のバッファを分けず、カウンタをそろえるのか**
- 線専用の行列のバッファを作る方法もある。ただしメンバ変数、マップポインタ、作成、`LogFaultResource` と直す場所が増える。
- 「バッファ1つにカウンタ1つ」というルールにそろえるほうが変更が小さく、ほかのバッファを見直すときの基準にもなる。
- `drawCallIndex_` の上限（4096）を線の分も使うことになるが、線はフレームに数回なので影響は無い。

**なぜ `TestNoWrite` にすると3が直るのか**
- `RenderQueue::Request` は、`depthMode` が `TestNoWrite` のものを半透明のキューに入れる。
- 描く順番は「不透明 → **Skybox** → 半透明」なので、アイコンはSkyboxの後に描かれる。さらに半透明のキューはカメラから遠い順に並べ替えられる。
- 深度はテストだけして書かないので、モデルの後ろのアイコンは隠れるが、アイコンの透明な部分が後ろを消すことは無い。パーティクルと同じ扱い。

**アイコンは透けて見えないのに、なぜ「半透明」なのか（グリッドとの違い）**（2026-09-17に追記）
- **絵は不透明でも、板には透明な部分が多い**
  - `pointLight.png`（800×800）を調べると、アルファが0（完全に透明）のピクセルが約82%、255（完全に不透明）が約17%、その間（線の縁のなめらかな部分）が約1%。
  - 板は四角いが、見せたい形（電球）は四角くない。四角い板から電球の形だけを見せるために、テクスチャのアルファで「ここは板の後ろを見せる」と決めている。
  - `grid.png`（1024×1024）は、アルファが255のピクセルが100%。グレーの市松模様が全面に描いてあり、透明な部分が1つも無い。`grid.obj` も100×100の板（三角形2枚）で、板の全部が不透明な床になる。だからグリッドは不透明で描いてよい。
- **エンジンの「不透明 / 半透明」は「そのピクセルの色を決めるのに、後ろの色が必要か」で分けている**
  - アイコンの透明な部分は「後ろの色をそのまま見せる」。縁のアルファ0.3のピクセルは「アイコンの色×0.3 ＋ 後ろの色×0.7」。どちらも後ろの色が要る。
  - ブレンドは、**描いた瞬間にレンダーターゲットに入っている色**と混ぜる。不透明のキューはSkyboxより前に描くので、その時点の「後ろの色」はクリアカラー。だから縁が背景色になっていた。
  - 深度バッファは1ピクセルに「一番手前の距離」を1つ持つだけで、そのピクセルが透明だったかは知らない。不透明として描くと、透明な部分でも（アルファがちょうど0でなければ）距離を書き、後から描くSkyboxが「手前に何かある」と判断して描かれない。
  - なので、後ろが透けて見える部分があるものは「後ろにある物（不透明な物体、Skybox）を全部描いた後に、奥から順に、深度を書かずに」描く。これが半透明のキュー。
- **遠いほど症状が大きくなった理由**
  - テクスチャはミップマップ（縮小版）を持っていて、遠いと縮小版を読む。縮小するときに透明なピクセルと不透明なピクセルが平均されるので、線の周りにアルファ0.xのにじみが広がる。
  - Unlitのシェーダーはアルファが**ちょうど0**のピクセルしか捨てないので、このにじみが全部深度を書いていた。
- **もう1つのやり方：切り抜き（カットアウト）**
  - シェーダーで「アルファが0.5未満なら捨てる」として、不透明のまま描く方法もある（UnityのCutout）。深度を書けるので並べ替えが要らず、重なりも正しい。代わりに縁がギザギザになる。
  - アイコンは数が少なく縁をなめらかに見せたいので、半透明にした。木の葉や金網のように数が多く、並べ替えが大変なものはカットアウトが向いている。

**アイコンの質問と答え（2026-09-18）**
- **Q. アイコンのpngを正方形（透明な部分なし）にすれば、不透明で描いてよいか**
  - よい。全ピクセルのアルファが255なら「後ろの色が必要な場所」が無いので、不透明（`TestWrite`）で正しく描ける。グリッドと同じ扱いになる。縮小版（ミップマップ）でもアルファは255のままなので、にじみも起きない。
  - ただし見た目は「電球が描かれた四角い板」になる（背景が四角く見える）。
  - 板の縁のギザギザは、アルファではなく三角形の縁の話なので、アンチエイリアス（MSAAなど）の話になる。
- **Q. 切り抜き（アルファ0.5未満を捨てる）にするなら、シェーダーを分けるのか**

  | やり方 | 内容 | 良い点 | 悪い点 |
  |---|---|---|---|
  | シェーダーを分ける | `UnlitCutout.PS` のように、`discard` する版を別に作る | 切り抜かない物は `discard` の分だけ軽い | シェーダーとPSOの組み合わせが増える |
  | マテリアルに値を持たせる | 定数に `alphaCutoff` を入れて `if (a < alphaCutoff) discard;`。0なら何も捨てない | シェーダーもPSOも増えない | `discard` があるシェーダーは、描く前の深度テスト（Early-Z）が効きにくい |

  - このエンジンの規模なら「マテリアルに値を持たせる」で十分。今のUnlitもすでに `discard` を持っているので、Early-Zの条件は今と変わらない。
- **Q. Unityはどうしているのか**
  - マテリアルで描き方を選ぶ。Built-inのStandardシェーダーは Rendering Mode（Opaque / Cutout / Fade / Transparent）、URP / HDRPは Surface Type（Opaque / Transparent）＋ Alpha Clipping（ON / OFFとしきい値）。
  - 中身は、同じシェーダーファイルを「キーワード」で `discard` あり / なしの2つにコンパイルしている（シェーダーバリアント）。書く人は1つだけ書き、エンジンが必要な組み合わせを作る。上の表の「シェーダーを分ける」を自動でやっている形。
  - 描く順番は Render Queue の番号で決まる（Geometry = 2000 → AlphaTest = 2450 → Transparent = 3000）。切り抜きは「不透明の後、半透明の前」。
  - テクスチャの読み込み設定にも関係するものがある。Alpha Is Transparency は、透明な部分の色を周りの色で埋めて縁が黒くにじむのを防ぐ。Mip Maps Preserve Coverage は、縮小版でアルファが薄まって切り抜きが細く消えていくのを防ぐ。
- **Q. アンチエイリアシングのことか。`discard` と同じか**
  - `discard`（HLSLの `clip(x)` も同じで、xが負なら捨てる）は「このピクセルを描かない」命令。切り抜きはこれで作る。
  - アンチエイリアシングは「縁のギザギザをなめらかに見せる」技術で、別の話。
  - 切り抜きは「描く / 描かない」の2択なので縁がギザギザになる。これをなめらかにする方法の1つが Alpha to Coverage（MSAAと組み合わせて、アルファを「ピクセルの何割を覆うか」に変える。Unityでは Alpha To Mask）。このエンジンは今MSAAを使っていない（サンプル数はすべて1）ので、まだ使えない。
  - 半透明は縁のアルファがそのまま混ざるので、もともとなめらか。その代わり並べ替えが必要。

**なぜ見つけにくかったのか（1と「ついで」の不具合）**
- 書いている場所はバッファの範囲内なので、コンパイルもアサートもD3D12のデバッグレイヤーも何も言わない。
- 「場所を渡すAPI」では、**GPUが実行する前に同じ場所へ2回書くと、前の値は消える**。Step 5.2の「フレームに1回、同じ場所を上書き」が安全なのは、書くのがフレームに1回だけで、前のフレームのGPUの処理が終わってから書くから。今回はその逆で、同じフレームの中で2回書いていた。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - 「Add x10」で並べたどのライトでも、`Position` を動かすとアイコンが一緒に動く
   - アイコンのチェックボックスを切り替えても、ほかのアイコンが消えない
   - Sceneビューでカメラを回しても、アイコンの周りが背景色で抜けない（空と重なる位置でも）
   - アイコンがモデルの後ろに回ると隠れる
   - カメラの視錐台の線、グリッド、モデルの位置が前と変わらない

### 次のStepでやること（ここではやらない）
- Step 5.2：ライトの定数バッファを全描画で共有する（手順は書き済み。①②の64はもう入っている）。
- Gameビューにギズモが出ること、Sceneビューで半透明の前後がずれることがあるのは `Editor.md` の作業で直す。

---

## Step 5.2：ライトの定数バッファを全描画で共有する ＋ 上限を64に

### 目的
1. ライトのデータを、1フレームに1回だけGPUに書き、全部の描画で同じ場所を結ぶ。
2. `MeshRequest` からライトのデータを外す（1件あたり816バイト小さくなる）。
3. `RenderContext` のライト用リングバッファ（描画回数分）を、1フレーム分の小さいバッファ2つに置き換える。
4. `kMaxPointLights` を16から64に上げる（C++とHLSLの両方）。

### 変更前と変更後
| | 変更前（上限16） | 変更後（上限64） |
|---|---|---|
| CPU側のコピー | 描画ごとに `MeshRequest` へ816バイト | 無し |
| GPUへの書き込み | 描画ごとにリングバッファへ | 1フレームに1回 |
| ライト用のGPUメモリ | (256 + 1024) × 4096描画 ≒ 5.2MB | 256 + 3328 ≒ 3.6KB |

### 変更するファイル
| ファイル | 内容 |
|---|---|
| `MyEngine/Graphics/Pipeline/ShaderConstants.h` | `kMaxPointLights` を64に |
| `MyEngine/Shader/Data/Buffers/Light.hlsli.shader` | `kMaxPointLights` を64に |
| `MyEngine/Graphics/Renderer/DrawRequest.h` | `MeshRequest` のライト2行を削除 |
| `MyEngine/Graphics/Renderer/RenderContext.h` | `SetFrameLights` を追加。ライト用リングバッファを消して、1フレーム分のバッファに置き換え |
| `MyEngine/Graphics/Renderer/RenderContext.cpp` | `SetFrameLights` を追加。`DrawMesh`、`InitInternal`、`LogFaultResource` を変更 |
| `MyEngine/Graphics/Renderer/Renderer.h` | このフレームのライトのメンバ2行を削除 |
| `MyEngine/Graphics/Renderer/Renderer.cpp` | `SetFrameLights` の中身を変更。`PushMesh` / `DrawModel` のライト2行を削除 |

LightManager、ゲーム側は変更なし。シェーダーは中身が変わると起動時に自動で作り直される（`ShaderPackageLoader` が前回の `.hlsl` と比べている）ので、キャッシュを消す必要は無い。

> Step 5.1の変更を入れた状態で、関係するファイル（RenderContext、Renderer、RenderQueue、SceneRenderer、LightManager、ImGuiManager、LightGizmo、WindowManager、Engine、Skybox、IBLBaker）のコンパイルが通ることを確認済み（`/W4`）。

---

### ① `MyEngine/Graphics/Pipeline/ShaderConstants.h`

### ② `MyEngine/Shader/Data/Buffers/Light.hlsli.shader`

### ③ `MyEngine/Graphics/Renderer/DrawRequest.h`

**削除する**：`MeshRequest` の中の2行

### ④ `MyEngine/Graphics/Renderer/RenderContext.h`

**追加する**：`static void ResetDrawCallIndex();` の下

**削除する**：リングバッファの2行（`// 共通` の中）

**削除する**：マップポインタの2行（`// --- 永続マップポインタ ---` の中）

**削除する**：スロットサイズの2行（`// --- CBufferスロットサイズ ---` の中）

**追加する**：`particleDataRingBuffer_` の下

### ⑤ `MyEngine/Graphics/Renderer/RenderContext.cpp`

**追加する**：`// 描画カウント・頂点カウントをリセット` の区切りの上

**`DrawMesh` の中**：リングバッファへの書き込みのうち、ライトの部分を削除する

**`DrawMesh` の中**：バインドを差し替える（`// --- Lit系のみ存在するスロット ---` の中）

変更前
変更後

**`InitInternal` の中**：ライトのバッファ作成を差し替える

変更前
変更後

**`LogFaultResource` の中**：一覧の2行を差し替える

変更前
変更後

### ⑥ `MyEngine/Graphics/Renderer/Renderer.h`

**削除する**：`private:` の中の2行（と、その下の空行）

### ⑦ `MyEngine/Graphics/Renderer/Renderer.cpp`

**`SetFrameLights` の中身を差し替える**

**`PushMesh` の中**：削除する

**`DrawModel` の中**：削除する

---

### 解説

**なぜ1つのバッファを毎フレーム上書きしてよいのか**
- `WindowManager::PostRenderAll` で、コマンドを実行した後に `DirectXCommon::WaitForGPU()` でGPUの処理が終わるのを待っている。
- 次のフレームで `LightManager::Update` がバッファに書き込むときには、前のフレームのGPUの処理はもう終わっている。なので同じ場所を上書きしても、GPUが読んでいる最中の値を壊すことが無い。
- 将来「GPUを待たずに次のフレームへ進む」作りにする場合は、フレームごとに別のバッファ（2〜3個を順番に使う）が必要になる。

**同じフレームに2回描いても共有してよい理由**
- エディタはSceneビューとGameビューで同じキューを2回描くが、LightManagerの収集はフレームに1回なので、どちらのビューも同じライトを使う。描画ごとにコピーする必要はもともと無かった。

**なぜサイズを256の倍数にするのか（`AlignTo256`）**
- D3D12の定数バッファは、大きさを256バイトの倍数にする決まりがある。今までのリングバッファも同じ理由で、1スロットを256の倍数にしていた。

**初期値を書き込む理由**
- 普通は描画より先に `LightManager::Update` が値を書くが、GPU用のバッファの中身は作った直後は何が入っているか保証されない。念のため「白・真下・強さ1、ポイントライト0個」にしておく。

**なぜ上限を64にしたのか**
- ピクセルシェーダーは、1ピクセルごとに `count` 回ループしてライトを足している。ライトの数に比例して重くなる。
- 定数バッファの上限は64KB（ポイントライト48バイトなら約1300個）なので、64個は余裕がある。手で置いて確認する規模としても十分。
- これより多く置きたくなったら、StructuredBufferに変え、「そのピクセルに届くライトだけ」を選ぶ仕組み（タイル / クラスター方式）に進む。

**C++とHLSLの値を必ずそろえる理由**
- `PointLightListData` は `lights[64]` の後ろに `count` がある。片方だけ64にすると、`count` を読む位置がずれて、ライトが光らなかったりおかしな値になったりする。コンパイルエラーにはならないので気づきにくい。

---

### 確認すること
1. **エンジンのビルド**が通る
2. **ゲームのビルド**が通る
3. **実行して確認**（Step 5.1の「Lights」ウィンドウを使う）
   - 当たり方がStep 5.1のときと同じ
   - 「Add x10」を6回押して60個にしても、警告が出ない
   - 7回押して70個にすると、上限64個を超えた警告が1回だけ出る
   - SceneビューとGameビューで、同じ当たり方になっている
   - （任意）ProfilerのDraw Callsが前と変わらない

### 次のStepでやること（ここではやらない）
- 先に `FrameLoop.md` Step 1：RenderContextの書き込み位置を1つにまとめる（2026-09-17に決定）。
- Step 5.5：スポットライトを追加する（Component、GPU用構造体、HLSL、各シェーダーの計算、LightManagerの収集、ギズモの円錐、確認用ウィンドウ）。

---

## Step 5.5：スポットライトの追加

### 目的
1. `SpotLightComponent` を追加する（Inspectorで編集する形。角度は度で持つ）。
2. GPU用の `SpotLightData` / `SpotLightListData`（上限32）と、シェーダーの `gSpotLights`（`b4`）を追加する。
3. ライト付きの5つのシェーダー（Lambert / HalfLambert / Phong / BlinnPhong / PBR）でスポットライトを足す。Unlitは変えない。
4. LightManagerに追加・削除・取得、収集、削除予約、ギズモ、確認用ウィンドウを足す。
5. ギズモに円錐のワイヤー（底の円 ＋ 頂点から円への4本）を足す。

### スポットライトの計算
- **ポイントライト × 円錐による減衰**。距離の減衰はポイントライトと同じ式。
- 円錐による減衰
  1. `cosAngle = dot(ライトの向き, -L)`：「ライトの向き」と「ライト→表面の向き」が作る角度のcos（`L` は表面→ライトなので、逆向きの `-L` を使う）。円錐の中心ほど1に近い。
  2. 外側の角度のcos（`cosOuter`）で0、内側の角度のcos（`cosInner`）で1になるように、間を直線でつなぐ：`saturate((cosAngle - cosOuter) / (cosInner - cosOuter))`
- 角度が大きいほどcosは小さくなるので、「内側の角度 < 外側の角度」は「`cosInner` > `cosOuter`」になる。大小が逆になる所に注意。
- 例：外側30度、内側20度

| 表面の位置（中心からの角度） | cos | 円錐による減衰 |
|---|---|---|
| 0度（中心） | 1.000 | 1（`saturate` で1に収まる） |
| 20度（内側の端） | 0.940 | 1 |
| 25度 | 0.906 | (0.906 − 0.866) ÷ (0.940 − 0.866) ≒ 0.55 |
| 30度（外側の端） | 0.866 | 0 |
| 40度 | 0.766 | 0（`saturate` で0に収まる） |

### 変更するファイル（上から順にやる）
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Light/LightComponent.h` | `SpotLightComponent` を追加 |
| ② | `MyEngine/Graphics/Pipeline/ShaderConstants.h` | `SpotLightData` / `kMaxSpotLights` / `SpotLightListData` を追加 |
| ③ | `MyEngine/Shader/Data/Buffers/Light.hlsli.shader` | `SpotLight` / `gSpotLights` / `SpotLightFactor` を追加 |
| ④〜⑧ | `MyEngine/Shader/Data/Model/` の `Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / `PBR` の `.PS.shader` | スポットライトのループを追加 |
| ⑨ | `MyEngine/Graphics/Pipeline/RootSignatureManager.h` | `RootBind::SpotLights` を追加 |
| ⑩ | `MyEngine/Graphics/Pipeline/RootSignatureManager.cpp` | `"gSpotLights"` を `NameToRole` に追加 |
| ⑪ | `MyEngine/Graphics/Renderer/RenderContext.h` | `SetFrameLights` の引数、スポットライトのバッファ |
| ⑫ | `MyEngine/Graphics/Renderer/RenderContext.cpp` | `SetFrameLights`、`DrawMesh` のバインド、`InitInternal`、`LogFaultResource` |
| ⑬ | `MyEngine/Graphics/Renderer/Renderer.h` | `SetFrameLights` の引数 |
| ⑭ | `MyEngine/Graphics/Renderer/Renderer.cpp` | `SetFrameLights` の引数 |
| ⑮ | `MyEngine/Light/LightGizmo.h` | `AddSpotLight`、`DrawIcon` を追加（コメントの誤字も） |
| ⑯ | `MyEngine/Light/LightGizmo.cpp` | ファイル全体を差し替え（アイコンを `DrawIcon` にまとめ、円錐を追加） |
| ⑰ | `MyEngine/Light/LightManager.h` | ファイル全体を差し替え |
| ⑱ | `MyEngine/Light/LightManager.cpp` | ファイル全体を差し替え |

- **シェーダーとC++を全部入れてから実行する**。C++だけだと、`gSpotLights` のスロットが無くてアサートで止まる。シェーダーだけだと、`gSpotLights` が結ばれずにD3D12のエラーになる。
- シェーダーは中身が変わると起動時に自動で作り直される。
- ゲーム側は変更なし。

> 確認済み（2026-09-18）
> - C++：この変更を入れた状態で、エンジンの全67個の `.cpp` をDebug / Releaseでコンパイルして通る（`/W4`。新しい警告なし）。`FrameLoop.md` Step 1aも入れた状態。
> - シェーダー：5つのPSを、Windows SDKのDXCでエンジンと同じ設定（`ps_6_0`、`-Zpr`）でコンパイルして通る（HLSL 2018 / 2021の両方）。`gSpotLights` が `b4` に来ていること、`SpotLight` が1個64バイトで `count` が2048バイト目にあること（C++と同じ）も確認。

---

### ① `MyEngine/Light/LightComponent.h`

**追加する**：ファイルの最後（`PointLightComponent` の下）

### ② `MyEngine/Graphics/Pipeline/ShaderConstants.h`

**追加する**：`PointLightListData` の下

### ③ `MyEngine/Shader/Data/Buffers/Light.hlsli.shader`

**追加する**：`ConstantBuffer<PointLightLists> gPointLights : register(b3);` の下（`#HLSL_END` の上）

### ④ `MyEngine/Shader/Data/Model/Lambert.PS.shader`

**追加する**：ポイントライトの `{ for ... }` のブロックの下（`float32_t4 transformedUV` の上の空行の前）

### ⑤ `MyEngine/Shader/Data/Model/HalfLambert.PS.shader`

**追加する**：`// ===== PointLights =====` のブロックの下（`// ===== Texture =====` の上）

### ⑥ `MyEngine/Shader/Data/Model/Phong.PS.shader`

**追加する**：`// ===== PointLight =====` のブロックの下（`// ===== Texture =====` の上）

### ⑦ `MyEngine/Shader/Data/Model/BlinnPhong.PS.shader`

**追加する**：`// ===== PointLight =====` のブロックの下（`// ===== Texture =====` の上）

### ⑧ `MyEngine/Shader/Data/Model/PBR.PS.shader`

**追加する**：`// --- ポイントライト ---` の `for` の下（`// 環境光は今は定数で代用` の上）

### ⑨ `MyEngine/Graphics/Pipeline/RootSignatureManager.h`

**追加する**：`enum class RootBind` の `PointLights,` の下

### ⑩ `MyEngine/Graphics/Pipeline/RootSignatureManager.cpp`

**追加する**：`NameToRole` の表の `{"gPointLights", ...},` の下

### ⑪ `MyEngine/Graphics/Renderer/RenderContext.h`

**変更する**：`SetFrameLights` の宣言（引数を1つ増やす）

**追加する**：`framePointLightsBuffer_` の下

**追加する**：`framePointLightsMappedPtr_` の下

### ⑫ `MyEngine/Graphics/Renderer/RenderContext.cpp`

**変更する**：`SetFrameLights` の1行目

**追加する**：`SetFrameLights` の中、`framePointLightsMappedPtr_` への `memcpy` の下

**追加する**：`DrawMesh` の中、`// PointLight` のバインドの下（`// IBL` の上）

**追加する**：`InitInternal` の中、`framePointLightsBuffer_->SetName(...)` の下

**追加する**：`InitInternal` の中、`*framePointLightsMappedPtr_ = PointLightListData{};` の下

**追加する**：`LogFaultResource` の一覧、`framePointLightsBuffer_` の行の下

### ⑬ `MyEngine/Graphics/Renderer/Renderer.h`

**変更する**：`SetFrameLights` の宣言

### ⑭ `MyEngine/Graphics/Renderer/Renderer.cpp`

**変更する**：`SetFrameLights` の全体

### ⑮ `MyEngine/Light/LightGizmo.h`

**追加する**：`AddPointLight` の宣言の下

**追加する**：`private:` のすぐ下

**直す**：`GetGlobalFlags` の上のコメントの誤字（「ラ内ごと」→「ライトごと」）

### ⑯ `MyEngine/Light/LightGizmo.cpp`（ファイル全体を差し替え）

### ⑰ `MyEngine/Light/LightManager.h`（ファイル全体を差し替え）

### ⑱ `MyEngine/Light/LightManager.cpp`（ファイル全体を差し替え）

---

### 解説

**なぜComponentは「度」、GPUは「cos」なのか**
- Inspectorで「30度」と入れるほうが、「0.866」と入れるより分かりやすい。
- シェーダーは `dot` で角度のcosを直接求められるので、cosのまま比べれば `acos` が要らない。
- 変換はLightManagerの収集（`CollectForGPU`）の1か所だけ。sRGB→リニアの色の変換と同じ考え方。

**内側の角度を外側までに収める理由**
- 内側が外側より大きいと `cosInner - cosOuter` が負になり、「中心が暗く外側が明るい」逆の円錐になる。
- 確認用ウィンドウでも `Inner Angle` の最大値を `outerAngle` にしているが、ゲーム側のコードから変な値を入れられても大丈夫なように、収集でも `std::clamp` する。
- シェーダーでも割る数を `max(..., 0.0001f)` にして、内側と外側が同じ角度のとき（くっきりした境目）に0で割らないようにしている。

**`static_assert(sizeof(SpotLightData) == 64)`**
- コンパイル時に条件を確かめ、違えばコンパイルエラーにする。
- HLSLの定数バッファは4成分（16バイト）の行単位で詰められる。`SpotLight` は16バイト×4行＝64バイトになるように並べてある（`float3` の後ろに `float` を1つ置くと、ちょうど1行になる）。
- C++の構造体の大きさがずれると、後ろのライトの値が全部ずれて、コンパイルは通るのに光り方がおかしくなる。その種類の不具合に、C++側だけでも早く気づけるようにしている。

**減衰の計算を `Light.hlsli` の関数にした理由**
- 5つのシェーダーに同じ計算を書くと、直すときに5か所を直すことになる。関数にすれば1か所で済む。
- `Light.hlsli` をincludeしている全部のシェーダーにこの関数が入るが、使わない関数はコンパイルで消えるので重くならない。
- ポイントライトの減衰（今は各シェーダーに直接書いてある）も、同じように関数にまとめてよい（今回はやらない）。

**全部のライト付きシェーダーで `gSpotLights` を使う必要がある理由**
- RootSignatureは、シェーダーのコンパイル結果（リフレクション）から作っている。**使っていない定数バッファはコンパイルで消える**ので、スロットが作られない。
  - 実際に `Lambert` は `Buffers/Camera.hlsli` をincludeしているが `gCamera` を使っていないので、コンパイル結果に `gCamera` が無い（DXCで確認）。
- `DrawMesh` はUnlit以外なら必ず `gSpotLights` を結ぶので、1つでもスポットライトを使っていないシェーダーがあると、スロットが無くて止まる。
- `rs.slotOf.at(...)` だと `std::out_of_range` の例外で落ちて原因が分かりにくいので、`SlotOf`（名前付きのアサート）を使った。

**PBRだけ `{ }` で囲んでいる理由**
- PBRのポイントライトの `for (int i ...)` は、ブロックに囲まれていない。
- 古いHLSL（2018）では、`for` の中で宣言した `i` がループの外まで生き残る。同じ場所にもう一度 `int i` を書くと二重定義になるおそれがある。新しいHLSL（2021）ではC++と同じくループの中だけ。
- どちらでも通るように、スポットライトのループを `{ }` で囲んだ（他の4つは元から囲まれている）。

**ギズモの円錐の計算**
- 光が届くのは「頂点からの距離が `range` 以内」で「円錐の中」。その境目は、頂点から斜めに `range` 進んだ所の円になる。
  - 円の中心：頂点 ＋ 向き × `range × cos(外側の角度)`
  - 円の半径：`range × sin(外側の角度)`
- 円を描くには、向きと直交する2本の軸（`axisA`、`axisB`）が要る。
  - 基準のベクトル（普通は上向き `(0, 1, 0)`）と向きの外積で、向きと直交する `axisA` を作る。
  - 向きと `axisA` の外積で、両方と直交する `axisB` を作る。
  - 向きが真上・真下のときは基準の上向きと平行になり、外積が0になる。そのときだけ基準を `(1, 0, 0)` にする。
- あとはポイントライトと同じ `PushCircle` で円を描き、頂点から円周上の4点へ線を引く。

**確認用ウィンドウの `PushID("PointLights")` / `PushID("SpotLights")`**
- `CollapsingHeader` は、`TreeNode` と違ってIDの区切りを作らない。
- ポイントライトとスポットライトの欄に同じ「Remove Added」ボタンがあると、ImGuiは同じボタンだと思い、片方しか反応しない。欄ごとに `PushID` で区切った。
- 同じ理由で、スポットライトの中の `Range`（距離）とチェックボックスの `Range` がぶつかるので、チェックボックスは「Range Wire」にした。

**上限を32にした理由**
- ポイントライトより置く数が少ない想定（街灯、ステージのライトなど）。定数バッファは 64 × 32 ＋ 16 ＝ 約2KB。
- シェーダーは `count` 回しかループしないので、置いていない分は重くならない。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**（Lightsウィンドウの「Spot Lights」）
   - 起動時にシェーダーのコンパイルエラーやアサートが出ない
   - 「Add x4」で、高さ4から真下を照らすスポットライトが4個並び、円錐のワイヤーが出る
   - Lit（Lambertなど）のモデルの真上にスポットライトを動かすと、丸く照らされる
   - `Outer Angle` を大きくすると照らす円とワイヤーが広がり、`Inner Angle` を外側に近づけると縁がくっきりする
   - `Direction` を変えると、照らす向きとワイヤーの向きが一緒に変わる。真上・真下・横向きでもワイヤーが崩れない
   - `Range` を小さくすると、光が届かなくなる所とワイヤーの円が合っている
   - ポイントライトとスポットライトの「Remove Added」が、それぞれ別々に効く
   - 「Add x4」を9回押して36個にすると、上限32個の警告が1回だけ出る
   - （モデルのシェーディングを変えられるなら）Lambert / HalfLambert / Phong / BlinnPhong / PBR のどれでも光る

### 次のStepでやること（ここではやらない）
- Step 6：ライトのComponentをInspector / Add Componentに対応させる（`Editor.md` の作業の後）。
- （任意）ポイントライトの減衰も `Light.hlsli` の関数にまとめる。
- （任意）マテリアルに `alphaCutoff` を持たせて、アイコンを切り抜きで描けるようにする（マテリアル / Inspectorの作業のとき）。

---

## Step 5.6：ライトのシェーダー周りの整理

### 目的
1. **シェーダーが使っているライトだけを結ぶ**ようにする。これで「このシェーダーはポイントライトを使わない」を書けるようになる。
2. ポイントライトの距離の減衰も `Light.hlsli` の関数（`PointLightFactor`）にまとめる。5つのシェーダーに同じ計算を書くのをやめる。
3. スポットライトのアイコンを、用意した `spotLight.png` に変える（ポイントライトと見分けられるようにする）。

### 「使っていないライトは書かなくてよい」仕組み
- RootSignatureは、シェーダーのコンパイル結果（リフレクション）から作っている。**使っていない定数バッファはコンパイルで消えるので、スロットができない**。
- つまり「そのシェーダーが使っているライトの種類」は、RootSignatureを見れば分かる。`rs.slotOf` に入っていれば使っている。
- なので `DrawMesh` 側を「スロットがあれば結ぶ、無ければ何もしない」に変えれば、シェーダーは好きな種類だけ書けばよくなる。
- この書き方はエンジンの中にすでにある。`RenderQueue::FlushMeshList` のカメラは `rs.slotOf.find(RootBind::Camera)` で見つかったときだけ結んでいる（だから `Lambert` はカメラを使っていなくても動く）。
- 例えば「平行光源だけの軽いシェーダー」を作りたければ、`Light.hlsli` の `gDirectionalLight` だけを使って `gPointLights` / `gSpotLights` を1度も書かなければよい。C++は何も変えなくてよい。

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Graphics/Renderer/RenderContext.cpp` | ライトのバインドを「スロットがあれば結ぶ」に変える |
| ② | `MyEngine/Shader/Data/Buffers/Light.hlsli.shader` | `PointLightFactor` を追加 |
| ③〜⑦ | `Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / `PBR` の `.PS.shader` | ポイントライトのループを `PointLightFactor` を使う形に差し替え |
| ⑧ | `MyEngine/Light/LightGizmo.h` | アイコンのハンドルを2つにする |
| ⑨ | `MyEngine/Light/LightGizmo.cpp` | スポットライトのアイコンを読み込んで使う。1:1の絵になったので縦に伸ばすのをやめる |

> 確認済み（2026-09-18）：エンジンの全67個の `.cpp` をDebug / Releaseでコンパイルして通る（変更したファイルの警告は0個）。6つのPSをDXCでコンパイルして通る（HLSL 2018 / 2021）。

---

### ① `MyEngine/Graphics/Renderer/RenderContext.cpp`

**追加する**：`SlotOf` 関数の下

**`DrawMesh` の中**：ライトのバインドを差し替える

変更前
変更後（`if (Unlit以外)` で囲む必要が無くなる。Unlitはライトのスロットを持たないので、何もしないだけ）

### ② `MyEngine/Shader/Data/Buffers/Light.hlsli.shader`

**追加する**：`ConstantBuffer<PointLightLists> gPointLights : register(b3);` の下

### ③ `Lambert.PS.shader`

**差し替える**：ポイントライトのループ（コメントはそのままでよい）

### ④ `HalfLambert.PS.shader`

### ⑤ `Phong.PS.shader`

### ⑥ `BlinnPhong.PS.shader`

### ⑦ `PBR.PS.shader`（ここは `{ }` で囲まれていないループ）

### ⑧ `MyEngine/Light/LightGizmo.h`

**変更する**：`DrawIcon` の宣言（どの絵を描くかを引数でもらう）

**変更する**：アイコンのハンドル（1つ → 2つ。`private:` の中の `static uint32_t iconTextureHandle_;` を差し替える）

> **注意（2026-09-18）**：このとき `GetGlobalFlags()` を消さないこと。`LightManager::DrawDebugWindow` の「Icon (All)」「Range (All)」が使っているので、消すとコンパイルエラーになる（`'GetGlobalFlags': 'LightGizmo' のメンバーではありません`）。`End()` の下に次の2行が必要。
> ```cpp
> 	// 全体の表示設定。ライトごとの設定とANDで判定
> 	static LightGizmoFlags& GetGlobalFlags() { return globalFlags_; }
> ```
> また `DrawIcon` は外から呼ばないので `private:` の中に置く（`public:` にあっても動くが、中でだけ使う関数なので隠す）。

### ⑨ `MyEngine/Light/LightGizmo.cpp`

**変更する**：静的メンバ変数の定義

**変更する**：`Initialize` の読み込み

**変更する**：`DrawIcon`（引数を増やし、縦に伸ばすのをやめる）

変更前
変更後

**削除する**：`DrawIcon` の中の1行（絵が1:1になったので、縦に伸ばさない）

**変更する**：`AddPointLight` の中

**変更する**：`AddSpotLight` の中

---

### 解説

**`BindConstantIfUsed` にして、`if (Unlitじゃないとき)` を消せる理由**
- Unlitのシェーダーはライトの定数バッファを1つも使っていないので、`rs.slotOf` にライトのスロットが無い。よって「あれば結ぶ」なら自動で何もしない。
- 「シェーディングの種類で分岐する」より「シェーダーが何を要求しているかで決める」ほうが、シェーダーを足したときに壊れにくい。
- 結ばなくても困らないのは、そのシェーダーがそのバッファを読まないから。逆に**読むのに結んでいないとD3D12のエラー**になるので、「使っているのに結ばない」は起きないようにする（`BindConstantIfUsed` は使っていれば必ず結ぶ）。

**ポイントライトの計算を関数にまとめたついでの修正**
- 前は `length(light.position - input.worldPosition)` と `normalize(light.position - input.worldPosition)` で、同じ引き算と長さの計算を2回していた。
- 1回だけ `toLight` と `distance` を求め、`toLight / distance` で向きを作る形にした（`normalize` の中でやっていることを自分で書いた形）。スポットライトのループと同じ並びになる。

**アイコンの `1.5` を消した理由**
- 前の絵は縦長に見せたかったので `scale.y = 1.5f` にしていた。用意した絵が縦横1:1（800×800、512×512）なので、そのまま等倍で出す。
- 大きさを変えたいときは `icon.transform.scale` を両方そろえて変える（例：`{1.5f, 1.5f, 1.5f}`）。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
2. **ゲームのビルド**が通る
3. **実行して確認**
   - ポイントライト・スポットライト・平行光源の当たり方が、Step 5.5のときと変わらない
   - スポットライトのアイコンが懐中電灯の絵になり、ポイントライトは電球のまま
   - アイコンが縦長でなく、正方形で出る
   - （任意）`Lambert.PS.shader` から `gPointLights` のループを一時的に消すと、C++を変えなくてもエラーにならず、Lambertのモデルだけポイントライトで光らなくなる（確認したら戻す）

### 次のStepでやること（ここではやらない）
- `Material.md` Step 1：マテリアルに `alphaCutoff`（切り抜き）を足し、`ModelConfig::material` を実際に使うようにする。
- Step 6：ライトのComponentをInspector / Add Componentに対応させる（`Editor.md` の作業の後）。

---

</details>

## Step 5.7：非均一スケールでも法線が正しくなるようにする（学校の課題）

### 症状
Yだけ2倍などの**非均一スケール**（uniformでないスケール）をかけると、**陰影がおかしくなる**。潰した球のふちが暗すぎる／明るすぎる、ハイライトの位置がずれる、など。

### 原因
今の頂点シェーダーは、**法線をワールド行列でそのまま変換している**。

```hlsl
worldNormal = normalize(mul(input.normal, (float32_t3x3) gObjectTransform.world)); // ← ここ
```

法線は「向き」ではなく「面に垂直であること」が大事な量なので、**位置と同じ行列で変換してはいけない**。

45度の斜面で考える（法線 `n` と、その面に沿った接線 `t`。`n・t = 0`）。
Yだけ2倍のスケール `S = diag(1, 2, 1)` をかけると、

| | 変換後 | `n・t` |
|---|---|---|
| 接線（面はこう動く） | `t * S = (1, -2, 0)` | — |
| 法線をワールド行列で変換 | `n * S = (1, 2, 0)` | **-3 → 垂直でない（間違い）** |
| 法線を「逆行列の転置」で変換 | `n * (S⁻¹)ᵀ = (1, 0.5, 0)` | **0 → 垂直のまま（正しい）** |

- Yを引き伸ばすと、面の傾きは**緩く**なる。なのに法線をワールド行列で変換すると、法線のYが**さらに伸びて**傾きが急になり、逆向きに動いてしまう。
- 正しい変換は **`transpose(inverse(worldMatrix))`**。これを「法線行列（normal matrix）」と呼ぶ。

**なぜ逆行列の転置なのか**（行ベクトル規約での導出）
- 位置は `p' = p * M`。接線も位置の差なので `t' = t * M`。
- 法線を `n' = n * A` としたとき、`n' · t' = 0` であってほしい。
- `n' · t'ᵀ = (n A)(t M)ᵀ = n A Mᵀ tᵀ`。これが `n tᵀ`（= 0）と等しくなるには `A Mᵀ = I`、つまり **`A = (Mᵀ)⁻¹ = (M⁻¹)ᵀ`**。

**今まで気づかなかった理由**
- **回転と平行移動と「均一」スケールだけなら、逆転置は元の行列と同じ向きになる**（長さだけ変わるが `normalize` するので消える）。つまり今までのモデルでは差が出なかった。
- 差が出るのは**非均一スケールのときだけ**。Inspectorで `Scale` を触れるようになったので、これから毎回踏む。

**実測**（エンジンの `Inverse` / `Transpose` を使って確認した）
```
変換前         n.t = +0.000000  （0のはず）
world で変換   n.t = -0.600000  ← 垂直でなくなる（陰影が崩れる）
逆転置で変換   n.t = +0.000000  ← 垂直のまま（正しい）

均一スケール： world版と逆転置版の差 = 0.000000 （0なら同じ向き）
```

### 変更するファイル
| | ファイル | 内容 |
|---|---|---|
| ① | `MyEngine/Graphics/Pipeline/ShaderConstants.h` | `ObjectTransformData` に `normalMatrix` ＋ 作る関数 |
| ② | `MyEngine/Shader/Data/Buffers/ObjectTransform.hlsli.shader` | HLSL側にも同じ順番で追加 |
| ③ | `MyEngine/Shader/Data/Model/Object3d.VS.shader` | 法線の変換を `normalMatrix` に差し替え |
| ④ | `MyEngine/Graphics/Renderer/Renderer.cpp` | 行列を入れている2か所を作る関数に差し替え |

---

### ① `MyEngine/Graphics/Pipeline/ShaderConstants.h`

**差し替える**：`ObjectTransformData`（ファイルの先頭あたり）
```cpp
// 座標変換
struct ObjectTransformData {
	Matrix4x4 worldMatrix = MakeIdentity4x4();
	Matrix4x4 normalMatrix = MakeIdentity4x4(); // 法線用。worldMatrixの「逆行列の転置」
	uint32_t isBillboard = 0;
	float padA[3] = {};
};
static_assert(sizeof(ObjectTransformData) == 144, "HLSLのObjectTransformと大きさが違います");

/// <summary>
/// ワールド行列と、そこから作る法線用の行列をまとめて入れる
/// <para>非均一スケール（Yだけ2倍など）のとき、法線をワールド行列で変換すると面に垂直でなくなり陰影が崩れる</para>
/// </summary>
inline ObjectTransformData MakeObjectTransform(const Matrix4x4& worldMatrix, bool isBillboard = false) {
	ObjectTransformData data;
	data.worldMatrix = worldMatrix;
	data.normalMatrix = Transpose(Inverse(worldMatrix));
	data.isBillboard = isBillboard ? 1u : 0u;
	return data;
}
```

---

### ② `MyEngine/Shader/Data/Buffers/ObjectTransform.hlsli.shader`

**追加する**：`float4x4 world;` の下に1行（**C++と同じ順番**にすること）
```hlsl
    float4x4 normalMatrix; // world の逆行列の転置。非均一スケールでも法線が面に垂直になる
```

DXCの出力で並びを確認した結果（`-Zpr` なので行優先）。C++の `sizeof` 144 と一致する。
```
;   row_major float4x4 world;         ; Offset:    0
;   row_major float4x4 normalMatrix;  ; Offset:   64
;   uint isBillboard;                 ; Offset:  128
;   float3 padA;                      ; Offset:  132
; } gObjectTransform;                 ; Offset: 0  Size: 144
```

---

### ③ `MyEngine/Shader/Data/Model/Object3d.VS.shader`

**差し替える**：`else` の中（ビルボードでないときの法線）
```hlsl
        worldNormal = normalize(mul(input.normal, (float32_t3x3) gObjectTransform.normalMatrix));
```

- ビルボード側は法線をカメラ方向から作っているので、直す必要はない。
- `(float3x3)` で切り出しているので、法線行列の平行移動の行は使われない（法線に位置は関係ないので、これで正しい）。

---

### ④ `MyEngine/Graphics/Renderer/Renderer.cpp`

**差し替える**：`PushMesh` の中（2行 → 1行）
```cpp
	req.objectTransformData = MakeObjectTransform(worldMatrix, config.isBillboard);
```

**差し替える**：`DrawModel` の中（2行 → 1行）
```cpp
			req.objectTransformData = MakeObjectTransform(node.worldMatrix * worldMatrix, config.isBillboard); // ノードの行列を挟む
```

- どちらも、下にあった `req.objectTransformData.isBillboard = ...;` の行を**消す**（`MakeObjectTransform` が入れてくれる）。
- `DrawLines` の `req.objectTransformData.worldMatrix = identity;` は**そのままでよい**。単位行列の逆転置は単位行列で、`normalMatrix` の既定値がすでに単位行列だから。

---

### 解説

**なぜ関数（`MakeObjectTransform`）にするのか**
- `worldMatrix` と `normalMatrix` は**必ずセットで入れないといけない**。片方だけ入れると、法線だけ前の描画の値が残る。
- 入れる場所が2か所に分かれていると、片方を直し忘れる。これは `FrameLoop.md` Step 1 で潰したリングバッファのバグと同じ形（同じものを2か所で管理すると必ずずれる）。

**毎描画で逆行列を計算して重くないのか**
- 4x4の逆行列はCPUで数十回の掛け算程度。1フレーム数百〜数千回でも誤差。
- 気になるようになったら、**スケールが均一かどうかを調べて、均一なら `worldMatrix` をそのまま使う**分岐を入れられる（今はやらない。分岐のほうが読みにくくなる）。

**スケールを0にしたとき**
- スケール0の行列は逆行列が作れない（行列式が0）。エンジンの `Inverse` はピボット選択付きのガウス消去なので、0除算でNaNが混ざる可能性がある。
- スケール0はそもそも何も見えない状態なので実害は無いが、**Inspectorで `Scale` を0にしたまま放置すると法線がNaNになる**ことは覚えておく（真っ黒 or 真っ白になる）。気になるなら `Scale` の `DragFloat3` に下限を付けるのが簡単。

**接線（Tangent）はどうするのか**
- 接線は「面に沿う量」なので、**ワールド行列で変換するのが正しい**（法線と逆）。
- 今は法線マップを使っていないので接線を送っていない。`Material.md` Step 4 で法線マップを入れるときに、`aiProcess_CalcTangentSpace` で作った接線を頂点に足す。そのときは**接線はworld、法線はnormalMatrix**と使い分ける。

---

### 確認すること
1. **エンジンのビルド**が通る（DebugとRelease）
   - `static_assert` が通れば、C++とHLSLの大きさが合っている
2. **ゲームのビルド**が通る
3. **実行して確認**
   - いま置いてあるモデル（均一スケール）の陰影が**前と変わらない**
   - InspectorやGlobalVariablesで `Scale` を `(1, 3, 1)` のように非均一にすると、**引き伸ばした面の陰影が自然になる**（前は暗くなりすぎたり、ハイライトがずれていた）
   - 潰した球（`Scale` = `(1, 0.3, 1)`）で、上から当てたライトの当たり方が正しい
   - ライト3種類（平行光源・ポイント・スポット）とも、`Lambert` / `HalfLambert` / `Phong` / `BlinnPhong` / `PBR` で崩れない
   - 線（`DrawLines`）やギズモが変わらない

### 次のStepでやること（ここではやらない）
- `Editor.md` Step 3：`RenderComponent`。
- Step 6：ライトのComponentをInspectorに対応させる。
