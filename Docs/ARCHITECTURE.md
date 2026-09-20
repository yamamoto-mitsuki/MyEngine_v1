# 目的
UnityのようなEditorからEntityを配置し、InspectorからComponentの値を編集できるゲーム開発環境を目指す。
Runtimeの内部実装は、Entity,Component,Systemを分離させ、将来的にComponentを型別に管理できる設計へ発展予定。


# 基本設計原則

## 1.ゲーム側に生ポインタを保持させない
ゲーム側のコードは、Engine内部のオブジェクトへの生ポインタ(T*)を所有、保持しない。
EntityやComponentを参照する場合は、Handle,Entity IDなどの間接参照を使用する。
### 例外
以下は生ポインタの保持ではなく、一時的な使用なので許可する。
・同一フレーム内で完結する一時的な変数(例: GameObject* obj = world.Get(h); で取得し、その場で使い捨てる)
・関数の引数として受け取る参照(呼び出し元が寿命を保証する間だけ有効)
禁止されるのは、フレームをまたいでメンバ変数として保持すること。
### 目的
・オブジェクトの生成、破棄をEngine側で一元管理する
・メモリ上の配置を変更しても、ゲーム側への影響を小さくする
・Entityの寿命を安全に管理する

## 2. Componentは「データ」と「振る舞い」を分離する
Componentは原則として、ゲーム状態を表すデータを保持する。
例:
struct TransformComponent {
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
};

Component自身に毎フレーム実行される複雑なゲームロジックを持たせない。
処理はSystemやManagerなど、Engine側の処理単位に分離させる。
例:
TransformComponent → 位置、回転、拡縮を保持
MovementSystem     → Transformを更新
### データかどうかの判断基準
Componentのデータ構造体に、以下が含まれていないことを基準とする。
・ポインタ(T*)
・std::string,std::vectorなど可変長、ヒープを使う型
・仮想関数(virtualポインタが入るため)
・GPUリソース(ID3D12Resourceなど)

これは現段階で単純なコピー・寿命管理を保つための制約。std::stringなどを含む型も技術的には連続配列で管理できるが、コピー・破棄・Undoの方法を別途定義する必要があるため、今回のデータComponentの対象外とする。
std::is_trivially_copyable_vだけでは、生ポインタを含まないことまでは検査できない。ポインタを持たせない規約は別途守る。
### 目的
・Componentを型別にまとめて管理しやすくなる
・大量のComponentを一括処理しやすくなる
・InspectorからComponentもデータを編集しやすくなる
### 例外: Script系Component
ゲーム固有の振る舞い(PlayerController,EnemyAIなど)は、Script系Componentとして「振る舞いを持ってよいComponent」に分類する。
・Script系は仮想関数Update()を持って良い。数が少なく、一括処理の対象外だから

## 3.ロジックは「1個」ではなく、「全部」に対して書く
可能な限り、特定のオブジェクト1個を直接更新するのではなく、同じ種類のデータを持つEntity群をまとめて処理する設計を優先する。
例:
for (auto& transform : transforms) {
    // Transformを更新
}
のように、同じComponentを持つ複数Entityを一括して処理できる形を目指す。
ただし、PlayerやBossなど個体固有の特殊な処理まで無理に一括することはしない。
### 目的
・データの連続配置によるキャッシュ効率を高めやすくする
・大量Entityの処理へ発展しやすくする
・System単位で処理を分離しやすくする
・将来的にマルチスレッド化を考慮


# 設計の基本方針
優先順位は以下の通り

1.Editorからの扱いやすさ
2.Entity,Componentの責務が明確か
3.Componentのデータと処理を分離できること
4.大量データを一括処理できる構造へ発展できること
5.必要になった箇所は最適化

全てのEntityを無理にECS化するのではなく、処理内容とデータ量に応じて、適切なStorage,Systemを選択する。


# フレームの更新順序
Update:
1. 生成待ちEntityの反映
2. Transformのワールド行列更新(親→子の順)
3. Component / System の更新
4. 衝突判定
5. 描画データの収集(ライトの選抜など)
6. 破棄フラグの付いたEntityの掃除

Draw:
1. 描画リクエストの積み込み
2. RenderQueueのフラッシュ


# 生成、破棄の規約
Entity、Componentの生成、破棄は必ずフレーム境界では反映する。
Update中は直接deleteしてはならない(保持中のポインタ、インデックスが無効化されるため)。
破棄はフラグを立てるのみとし、掃除はUpdateの6でまとめて行う。


# Editorとの関係
EditorとRuntimeは同じEntity,Component構造を利用する。
Editorでは、Entityを選択してInspectorからComponentのデータを編集できる。

Scene
 └ Entity
    ├ TransformComponent
    ├ ModelRendererComponent
    └ ColliderComponent

Inspector
 ├ Transform
 ├ Render
 └ Collider

Inspectorによる編集は、Componentのデータを変更する操作として扱う。
Editorの使いやすさとRuntime内部のデータ管理方式を分離し、Editor側からComponentの内部Storage方式を意識せず操作できるようにする。
また複数のゲームで本エンジンを使用するためEditorの再利用性にも気を配る。


# 発展段階と現在の状況
段階1: GameObjectがComponentを、vector<unique_ptr<Component>>で所有。仮想関数で更新
段階2: Componentを型別の配列に分けて、Managerがまとめて回す
段階3: Entityは単なるID。データは型別の連続配列。Systemが配列を走査
段階4: マルチスレッド化

現在（2026-09-19）: Transform・ModelRenderer・ライト（Light.md Step 6の後）は、EntityManagerの型別の連続配列（ComponentStorage<T>）で管理している。段階2の終わり〜段階3の入り口。Editorの登録表（ComponentEditorRegistry）はRuntimeの実体管理とは別。
ゲーム固有のComponentも EntityManager::RegisterComponent<T>() で同じように扱える。手順は [Tasks/Entity.md](Tasks/Entity.md)。
Entityの名前・親子をIDから分離することや、複数World・マルチスレッド化は今回の対象に含めない。