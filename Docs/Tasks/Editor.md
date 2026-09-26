# Editor：現在の状態と設計記録

## 反映済みの状態

| 項目 | 現行実装 |
|---|---|
| Entity | EntityId、名前・有効・親子、必須Transform、任意ModelRenderer |
| Inspector | ComponentEditorRegistryを走査。各ComponentEditorへ表示・操作を委譲 |
| Undo / Redo | 作成・削除・名前・有効・親子・Component追加削除・Inspectorの値変更 |
| コピー | 子孫ごとコピー、兄弟／子へ貼り付け、名前は(CopyN) |
| 日本語 | ImeInputが未確定文字を入力欄へ描く。候補一覧はWindows |
| Editorの配置 | History / Inspector / Viewport / Widgets / Windows |
| シーンの保存（S1。このブランチで写経待ち） | Control ウィンドウの Save / Ctrl+S（停止中だけ）。Play は退避してから始まり、Stop で Play 時点へ戻る。詳細は [Serialize.md](Serialize.md) |

## TypedComponentEditorでできること

エンジンのComponentはこのクラスを継承して `GetName` / `GetCategory` / `DrawComponent` だけ書き、`InspectorWindow::Initialize` で登録する（`TransformEditor` / `ModelRendererEditor` / `LightEditor`）。
取得・追加・削除は `TypedComponentEditor` が `EntityManager::Get<T>` などへ直結しているので、派生クラスは表示だけ書けばよい。
**ゲーム固有Componentは継承しない。** `COMPONENT(型名, "サブカテゴリ") { ui.Field(…); }` と書くと `DescribedComponentEditor<T>` が自動で作られ、`EntityManager::RegisterComponent<T>` と `ComponentEditorRegistry::Register` もまとめて走る（Entity.md 手順F）。
「Editorの登録」（`ComponentEditorRegistry::Register`）と「Runtimeで実体を保管する登録」（`EntityManager::RegisterComponent<T>`、Releaseでも必要）は別の責務。
Inspectorの区画はカテゴリ順（同じカテゴリの中は名前順）。ゲームのシーンがエンジンより先に登録しても Transform が先頭。
新しいComponentを追加するとき、InspectorWindowやEditorHistoryに型別分岐を足さない。

## Undoの仕組み（解説）

### 予約と実行

UIはRequest系APIで操作を予約し、次のフレーム先頭の `EditorHistory::Flush` が順番に実行する。
描画中にEntityの配列を増減すると参照が無効になるため、操作を受け付ける時点と構造を変える時点を分けている。
Inspectorの値変更自体はその場で行い、その記録を同じ予約列へ積む。

### 履歴1件の内容

履歴は「undo関数・redo関数」の組で、cursorが適用済み件数を示す。
Undo後に新しい編集をするとcursorより先のRedo履歴を捨てる。上限128件。
対象消失などで復元に失敗したら、食い違った履歴を続けずにクリアする。

| 操作 | 記録と復元 |
|---|---|
| 作成・削除・貼り付け | 親から子の順にEntityTreeを記録し、同じEntityIdで復元する |
| 名前・有効・親 | EntityIdと変更前後の値を持つ |
| Component追加・削除 | ComponentEditorを窓口に追加／削除する。削除前の値も保持する |
| Inspectorの変更 | 描画の前後を比較し、ドラッグ終了までを1件にまとめる |

### EntityIdとHandleの違い

削除のUndoでは、作り直されたEntityのHandleは別の世代になる。
履歴はEntityIdを保持し、操作時にFindByIdで現在のHandleを取得するので、それより前の編集も同じ相手に適用できる。
ゲーム側で保持していた古いHandle自体が有効に戻るわけではない。

### Componentの編集単位

Inspectorの前後を比べて変わっていたら、**Component全体**の「編集開始時」と「最後に変えた時点」を記録する。
その前の「4バイト単位で変わった所だけ」は、任意のdouble・uint64_t・隣の小さな値まで一般化できないので、やめた。
そのため、同じComponent内をゲームが書き換えていると、それも全体と一緒に戻る（Play中だけ起きる。Stop中はゲームのUpdateが止まっている）。
編集設定とゲーム実行中の状態を別々に扱いたい場合は、Component／Systemの責務を分ける。
Transformで「ドラッグして元の値にぴったり戻した」ときは、`worldMatrix` が前のフレームの値のまま比べられるので、空のUndoが1件できる（Ctrl+Zが1回空振りするだけ）。
Inspector外の変化だけで履歴を自動追加するものではない。

### コピー・保存との境界

- 削除のUndo・コピーの写しは `EntityManager::CaptureComponents` で**登録された全部の型**から集める（Entity.md 手順B・C）。Editorを登録していないRuntimeだけの型も戻る・付いてくる。
- モデル・テクスチャはHandleを共有し、GPU資産自体を複製しない。
- Component内のEntity参照を、複製された別Entityへ付け替える機能はまだない。
- クリップボードはClearで消さないのでStop後にも貼れる。資産の寿命は別途守る。
- バイト列は同一実行・同一型定義での履歴用。シーン保存や型定義変更をまたぐデータ形式には使わない。

## 残す設計記録

| 方針 | 理由・現在の仕様 |
|---|---|
| EntityはHandle、Component実体は型別の `ComponentStorage<T>`（EntityManagerが持つ。ゲーム固有の型も `RegisterComponent<T>` で同じ） | 段階3の「型別の連続配列」。取得ポインタは同フレーム内で使い捨て。`ForEach<T>` の中で増減させてはいけないのは、回している型の配列だけ（Entity.md 手順B） |
| 全EntityがTransformを持つ | 親子行列の処理を共通化。UIは将来UITransformを追加 |
| データはModelRendererComponent、処理はModelRenderSystem | Renderer::ModelConfigのポインタや文字列をComponentへ保存しない |
| 追加はフレーム境界、1EntityにModelRendererは1個 | UI走査中の再確保を避ける。モデル内のノード分割はModel.mdで扱う |
| IBLはシーン所有、Drawへ一時的に渡す | GPUリソースはGPU使用完了まで生存。PBRはIBL無しなら描画しない |
| シーンファイルを使うシーンは、全Entityを自分の物として扱う（入るときも出るときも全部消す）。`sceneRoot_` と `Finalize` での破棄はやめた（Serialize.md S1） | 前は sceneRoot の下だけ作り直され、Hierarchy の一番上に作った物は Play 中の変化を引きずって残っていた。全部を同じ扱いにして、保存・復元の対象にする |
| 現行Drawのroot={}は全Entity対象 | 複数シーンへ同じEntityを重複描画しない所属管理は未解決 |
| BillboardはNone/Full/AxisY、VSで頂点zも含めて回す | Scene/Gameそれぞれのカメラへ向け、3Dモデルを板に潰さない。Primitiveのbool指定はFullへ変換 |
| ImGuiは項目を「名前から作ったID」で区別する。1つのComponentの区画の中で同じ名前を2回使わない（使うなら `##` か `PushID` で分ける） | 同じIDが2つあると赤枠の警告が出て操作が壊れる。Inspectorは区画ごとに `PushID(editor.GetName())` しているので、別のComponent同士はぶつからない |
| Add Componentはカテゴリの**文字列のパス**＋検索、モデル・テクスチャの一覧は ComponentUI の `AssetField`（Model Renderer の Inspector もこれを使う。S1 から） | 最小のアセット選択を優先。メニューは `ComponentEditorRegistry` から作る。カテゴリは `"Gameplay/Movement"` のように `/` で区切り、メニューを入れ子にする。並びは「上の段の決まった順（`ComponentEditor.cpp` の `kTopLevelOrder`）→ パスの文字順 → 名前順」で、登録した順に左右されない＝ファイルが増えても並びが変わらない |
| ゲーム固有Componentは `COMPONENT` / `SYSTEM` の2つのマクロだけで書く。ImGuiもカテゴリも登録も書かない | 1ファイル＝1Component。`ui.Field` が型ごとにImGuiを呼ぶので、ゲーム側にImGuiと `#ifdef USE_IMGUI` が出てこない。`ComponentUI` は差し替えられる作りなので、同じ `COMPONENT` の中身をシリアライズにも使える |
| 自動登録（static変数の初期化）が効くのはexeに直接入るファイルだけ。エンジン（.lib）のComponentは明示的に登録する | 誰も参照していない .lib のファイルはリンカに捨てられ、登録も消える。実測で確認（obj直リンク＝登録される／lib経由＝消える） |
| `SYSTEM` の関数は、先頭で `Handle<Entity>` を受け取れる（省略可） | 「倒したら自分を消す」が書けないと実際のゲームが作れない。回している最中の `Destroy` は予約なので安全 |
| **ヘッダの中で `std::numeric_limits<T>::max()` を使わない**（`<cfloat>` の `FLT_MAX` などにする） | `windows.h` が `max` という名前のマクロを作るので、読み込み順によっては壊れる。`#define NOMINMAX` は windows.h を読むより前でないと効かない＝そのヘッダ自身に書いても手遅れ |
| ゲーム全体の状態（スコア、倒した数）は、必ずしもComponentにしない | `inline` 変数1個で済むならそれでよい。Inspectorで見たい物だけComponentにする。Systemどうしは直接呼び合わず「片方が書いて片方が読む」にすると実行順に左右されない（2026-09-21、Entity.md F-18） |
| エディタからビルドするときは、MSBuildを子プロセスで動かし、`○○.cpp` の行を数えて `3 / 12` を出す | バーは正確でなくてよい（UnityもUnrealも正確ではない）。終了コードが0なら自動で起動、0以外ならログの `error` 行だけを出す。**未実装**。先にPCHで1ファイル2.5秒→0.2秒にする（実測、Entity.md「ビルド時間の実測」） |
| 日本語はUTF-8のstd::string＋Meiryo動的フォント | 固定長バッファで切らない。未収録の字形まで保証しない |
| 変換中の文字はImGuiの入力欄の中に自分で描く（`ImeInput`）。候補の一覧はWindows | 入力欄と同じ見た目で、IMEごとの動作は実機確認が必要。Windowsの変換窓は `ISC_SHOWUICOMPOSITIONWINDOW` を外して出さない |
| メッセージは `PeekMessage(&msg, nullptr, …)` でスレッドの分を全部取り出す | ウィンドウを指定すると、IMEなどWindowsが作ったウィンドウ宛てのメッセージが残り続ける。変換窓が出なかった原因の有力候補 |
| Componentごとのエディタの扱いは `ComponentEditor`（UnityのCustomEditor）。`TypedComponentEditor<T>` を継承して表示だけ書き、登録する（実体の出し入れは `EntityManager` 直結） | Inspector・Add Component・Undo・コピーがComponentの型を知らずに済む。Componentを増やしても `EditorHistory`・`InspectorWindow` は変えない |
| Componentの編集の記録は「Inspectorが描く前と後のバイト比較」、変わっていたらComponent全体を記録 | 項目の手書き（旧 `TrackInspectorValues`）をやめる。Inspectorの外（ゲームの `Update`）の変化だけでは記録しない。4バイト単位は任意型に一般化できないので全体へ |
| Componentは `std::is_trivially_copyable_v` を満たす | バイト列で写してUndo・コピーするため。`static_assert` でバイトコピー可能性を検査する。生ポインタを含まないことは別途設計で守る |
| Entityは `EntityId`（消して作り直しても変わらない番号）を持つ。履歴は番号で相手を覚える | 削除のUndoでHandleが変わっても、前の履歴が同じ相手を見つけられる。将来のシーン保存の親子の書き出しにも使う |
| Componentの追加も取り外しも「予約 → `FlushComponentChanges`」 | UIの一覧を回している途中で配列を動かさない。Undoからは予約の直後にFlushを呼ぶ（フレームの境目なので安全） |
| 履歴の変更はすべて `EditorHistory::Flush`（フレームの最初）で、頼まれた順に行う | 描画中に構造を変えない。「編集の記録 → Undo」の順番が入れ替わらない |
| コピーした物は `EditorHistory::Clear`（シーンの作り直し）で消さない | Play中にコピーして、Stopの後に貼れる（Unityと同じ） |
| Undo/RedoはEditメニューとCtrl+Z / Ctrl+Y（どのウィンドウでも）。コピー・貼り付け・削除のキーはHierarchyを触っているときだけ | 文字の入力中とドラッグ中は、入力欄のUndoを優先する |
| Editorのフォルダは History / Inspector / Viewport / Widgets / Windows | Componentが増えると増えるファイル（Inspector）と、あまり増えないファイルを分ける（2026-09-18） |
| Gameは製品の絵、Sceneは編集用 | Gameはポストエフェクトあり・ギズモなしが目標。ギズモ混入とビュー別の半透明ソートは未解決 |
| ライトは種類別Component、ギズモは全体AND個別 | 詳細と進捗はLight.mdへ集約 |
| Play は「退避 → 退避から作り直す」、Stop は「退避から作り直す」、Restart は再生中だけ。退避はシーンファイルと同じ JSON（Serialize.md S1） | Stop で戻る物＝保存される物。バイト列の写しだと Component の中の `Handle<Entity>` が作り直しで無効になる。作り直しの前後で選択は EntityId で選び直す |
| Undoはゲームの時間を戻す機能ではない | 編集した値だけを記録。削除前のゲーム側Handleは無効のまま（ゲームが覚え続けたいなら `EntityId` で覚える） |
| ファイルを増やすのは大きな話題の区切りだけ | 完了した写経コードは実装へ、理由はこの表へ。古い「追加する／消す」の抜け殻は残さない |

未完了：シリアライズ（Serialize.md S1、写経待ち）、Collider（S2）、AudioSource（S3）、PCH（S4）、保存していない印（*）、Play をまたぐ Undo 履歴、エンジンの型の Inspector を Describe 関数に寄せる、クオータニオン、エディタからのビルド（進捗バー）、Projectパネルとドラッグ＆ドロップ、Sceneギズモ分離、半透明のビュー別ソート、ImGuizmo、シーン所属（複数ウィンドウ）。
C#スクリプティングは**やらない**（数ヶ月かかり、得られるのは「ビルドと再起動が要らない」だけ。シリアライズ＋速いビルドで代わりになる。理由は Entity.md「ドラッグ＆ドロップ・C#・マルチスレッドの優先順位」）。マルチスレッドは測ってから（今はボトルネックが分かっていない）。
ゲーム側MonsterBall移行を確認する場合は、シーン所有IBL・ModelRenderSystem::DrawへのIBL引数を確認する（sceneRoot は S1 でやめた）。旧GameScene全文は再掲しない。
補足：imgui_style.iniはSaveStyle/LoadStyleにある項目だけを保存し、色はリニア。ヘッダー配布のxcopyは削除済みヘッダーを消さないため、改名時の古い配布ファイルに注意。以前の配布手順は `e04d804` のこのファイル内、A-4に残っている。

予定（2026-09-21 に見直し。理由は Serialize.md 1章）：シリアライズ → Collider → AudioSource → PCH → 小さなゲームを1本通す → プレハブ・シーンを開く → クオータニオン。その他の候補：Sceneギズモ分離（Gameビューにギズモ・Bloomが乗る件）、Model.md Step 2、Compute.md CS-1、パーティクルのField（学校の課題）。
