# エンジンをサブモジュールにする（AL4_01 に MyEngine_v1 を入れる）

参考：
- Pro Git「Git のさまざまなツール - サブモジュール」：https://git-scm.com/book/ja/v2/Git-のさまざまなツール-サブモジュール
- `git submodule` のリファレンス：https://git-scm.com/docs/git-submodule
- MSBuild の予約済みプロパティ（`$(MSBuildThisFileDirectory)`・`$(SolutionDir)`）：https://learn.microsoft.com/ja-jp/visualstudio/msbuild/msbuild-reserved-and-well-known-properties
- GitHub の大きいファイルの上限（50MB で警告・100MB で拒否）：https://docs.github.com/ja/repositories/working-with-files/managing-large-files/about-large-files-on-github

## このファイルについて
- ユーザーの頼み：エンジン（MyEngine_v1）を AL4_01 のサブモジュールにして、AL4_01 をそのまま起動できるようにする。
- **エンジン側のファイルは1つも直さない**。直すのは AL4_01 の2ファイル（`AL4_01.vcxproj` の1行・`AL4_01.slnx` の3行）だけ。
- 確かめた環境：AL4_01 の develop（ff829f3）を一時フォルダに clone → 本体の feature/UIEditor（e300a7d）をサブモジュールで追加 → 3章のパスの直し → 2章の lib のコピー → `AL4_01.slnx` を Debug でビルド（7章）。
- 締め切り（10/28）の前なので、**ブランチを切ってやる**（失敗したら 9章で元に戻せる。外の `source/repos/MyEngine_v1` はそのまま残る）。

| 章 | どこで | 何をするか |
|---|---|---|
| 0 | 両方 | 前準備（.gitignore で抜けていた .obj を入れる・エンジンを push・AL4_01 の変更をコミットしてブランチを切る） |
| 1 | AL4_01 | `git submodule add` でエンジンを入れる |
| 2 | AL4_01 | assimp の lib をコピーする（git に入っていないので） |
| 3 | AL4_01 | `AL4_01.vcxproj`・`AL4_01.slnx` のパスを直す |
| 4 | AL4_01 | 古い中間ファイルを消して、ビルド・起動 |
| 5 | AL4_01 | コミット・push |
| 6 | - | これからの使い方（エンジンを直す流れ・別の PC で clone） |
| 7〜9 | - | 確かめたこと・困ったとき・元に戻す |

---

## 1. 何をするか（なぜ AL4_01 の2ファイルだけで済むか）

### ===== 今の形 =====
```
source/repos/
├─ MyEngine_v1/          ← エンジンのリポジトリ
│   └─ project/
│       ├─ MyEngine.props
│       └─ MyEngine_v1.vcxproj …
└─ AL4_01/               ← ゲームのリポジトリ
    └─ project/
        ├─ AL4_01.slnx       （"../../MyEngine_v1/project/…" でエンジンを指す）
        └─ AL4_01.vcxproj    （"..\..\MyEngine_v1\project\MyEngine.props" を読む）
```
AL4_01 は「となりのフォルダにエンジンがある」前提。別の PC で AL4_01 だけ clone すると、エンジンが無くて開けない。
しかも **どのエンジンのコミットで動くのか** が AL4_01 のどこにも残らない。

### ===== サブモジュールにした形 =====
```
AL4_01/
├─ .gitmodules           ← 「MyEngine_v1 フォルダは、この URL のリポジトリ」と書いたファイル（新しくできる）
├─ MyEngine_v1/          ← サブモジュール（中身はエンジンのリポジトリそのもの）
│   └─ project/
│       ├─ MyEngine.props
│       └─ …
└─ project/
    ├─ AL4_01.slnx       （"../MyEngine_v1/project/…" に直す）
    └─ AL4_01.vcxproj    （"..\MyEngine_v1\project\MyEngine.props" に直す）
```

### --- サブモジュールとは ---
- 親（AL4_01）のリポジトリは、サブモジュールの **中身のファイルを持たない**。持つのは「`MyEngine_v1` フォルダ ＝ エンジンのリポジトリの **このコミット**（例 e300a7d）」という1行だけ。
- なので AL4_01 のコミットごとに「このゲームはこのエンジンのコミットで動く」が残る。エンジンを先に進めても、古い AL4_01 のコミットに戻せば、そのときのエンジンに戻る。
- サブモジュールの中は普通の git リポジトリ。中で `git switch`・`git commit`・`git push` すると、エンジンのリポジトリ（GitHub の MyEngine_v1）に入る。

### --- エンジン側を直さなくていい理由 ---
エンジンの設定は全部「自分のファイルの場所」か「ソリューションの場所」から決めているので、エンジンのフォルダがどこに移っても動く。

| 何 | どこから決めているか | サブモジュールにすると |
|---|---|---|
| インクルードの場所・assimp の lib・参照するプロジェクト（`MyEngine.props`） | `$(MyEngineRoot)` ＝ `$(MSBuildThisFileDirectory)`（props 自身のフォルダ） | `AL4_01/MyEngine_v1/project/` に自動で変わる |
| 出力先・中間ファイル（`OutputDirs.props`） | `$(SolutionDir)..\generated\` | 今と同じ `AL4_01/generated/`。**サブモジュールの中は汚れない** |
| シェーダーとエンジンの画像のコピー（エンジンのビルド後イベント） | `$(SolutionDir)MyEngine\` | 今と同じ `AL4_01/project/MyEngine/`（AL4_01 の .gitignore で無視済み） |
| Release 以外で .shader を直接読む場所（`MYENGINE_ROOT_DIR`） | `$(MyEngineRoot)` | サブモジュールの `project/` を読む |

`$(MSBuildThisFileDirectory)` は「今読んでいる props/vcxproj ファイルがあるフォルダ（末尾に `\` 付き）」。`$(SolutionDir)` は「開いたソリューション（AL4_01.slnx）のフォルダ」。
変わるのは **AL4_01 から見たエンジンの場所**（`..\..\MyEngine_v1` → `..\MyEngine_v1`）だけなので、AL4_01 の2ファイルを直せば済む。

---

## 0. 前準備

### ===== エンジン：grid.obj を git に入れる（.gitignore の直し） =====
試したら、**GitHub から clone したエンジンだと、エディタの起動で assert で止まった**：
```
AssertError: scene != nullptr - 読み込み失敗: Unable to open file "MyEngine/Resources/Model/Grid/grid.obj".
```
エンジンの .gitignore（Visual Studio のひな形）に `*.obj` がある。これは本来 **コンパイルの中間ファイル（main.obj など）** を無視するための行だが、モデルの `.obj` も同じ拡張子なので、`project/Resources/Model/Grid/grid.obj` も無視されて git に入っていなかった（`grid.mtl` だけ入っている）。
今までは外のフォルダに手元のファイルがあったので気づかなかった。サブモジュールは GitHub から取ってくるので、無いと困る。

エンジンの `.gitignore` の一番下に足す：
```gitignore

# モデルの .obj は入れる（上の *.obj はコンパイルの中間ファイル用。中間ファイルは /generated/ に出るので、Resources の下に .obj ができることはない）
!/project/Resources/**/*.obj
```
- 先頭の `!` は「上の行で無視したものでも、これには当てはまるなら無視しない」。
- `**` は「下のフォルダ何段でも」。`/project/Resources/` の下の `.obj` だけを入れ直す（ほかの場所の中間ファイルは今までどおり無視）。

```bash
cd C:/Users/K025G/source/repos/MyEngine_v1
git add .gitignore project/Resources/Model/Grid/grid.obj
git commit -m "モデルの .obj を git に入れる"
```
`git status --ignored project/Resources` で、`!!`（無視されている）の行が出なくなれば OK。

### ===== AL4_01：cube.obj も同じ =====
AL4_01 の .gitignore にも同じ `*.obj` があって、`project/Resources/Models/cube/cube.obj` が git に入っていない。今の PC では困らないが、別の PC で clone すると無い。AL4_01 の `.gitignore` の一番下にも足す：
```gitignore

# モデルの .obj は入れる（上の *.obj はコンパイルの中間ファイル用）
!/project/Resources/**/*.obj
```
（コミットは下の「AL4_01：今の変更をコミット」と一緒でいい。`git add -A` で cube.obj も入る）

### ===== エンジン：今のブランチを GitHub に push する =====
サブモジュールは **GitHub にあるコミットしか指せない**（clone した人が取りに行くのは GitHub なので）。
今の feature/UIEditor（e300a7d）はまだ GitHub に無い（上流ブランチが無い）ので、先に push する。

```bash
cd C:/Users/K025G/source/repos/MyEngine_v1
git push -u origin feature/UIEditor
```
- Docs の未コミットの変更（Reference.md・G9 の md など）は、このあと使わないので、そのままでいい（push されるのはコミット済みの e300a7d まで）。
- もう develop に入れているなら、develop を push するのでもいい（1章の `switch` のブランチ名を合わせる）。

### ===== AL4_01：今の変更をコミットして、ブランチを切る =====
今 AL4_01 の develop には未コミットの変更（Resources の画像・prefab・シーン）がある。混ざらないように先にコミットしておく。

```bash
cd C:/Users/K025G/source/repos/AL4_01
git add -A
git commit -m "リソースの更新"
git switch -c feature/EngineSubmodule
```

---

## 1. サブモジュールを足す

AL4_01 の **ルート**（`project/` ではない）で：

```bash
cd C:/Users/K025G/source/repos/AL4_01
git submodule add https://github.com/yamamoto-mitsuki/MyEngine_v1.git MyEngine_v1
```
- GitHub からエンジンを `AL4_01/MyEngine_v1/` に clone して、`.gitmodules` を作り、2つを `git add` した状態にしてくれる。
- このときサブモジュールの中は **GitHub の既定のブランチ（master）** になっている。今のゲームは feature/UIEditor のエンジンで動いているので、切り替える：

```bash
git -C MyEngine_v1 switch feature/UIEditor
git add MyEngine_v1
```
- `git -C <フォルダ>` は「そのフォルダに cd したつもりで git を動かす」。`cd MyEngine_v1` → `git switch …` → `cd ..` と同じ。
- `switch feature/UIEditor` は、ローカルに無いブランチでも `origin/feature/UIEditor` から同じ名前のブランチを作って切り替えてくれる。
- 2行目の `git add MyEngine_v1` で、親（AL4_01）に「指すコミットは e300a7d」を覚えさせる（サブモジュールの中のコミットが変わったら、毎回これをする。6章）。

確かめる：
```bash
git status
```
```
new file:   .gitmodules
new file:   MyEngine_v1
```
`.gitmodules` の中身はこうなっている：
```
[submodule "MyEngine_v1"]
	path = MyEngine_v1
	url = https://github.com/yamamoto-mitsuki/MyEngine_v1.git
```

---

## 2. assimp の lib をコピーする

`project/externals/assimp/lib/Debug/`・`lib/Release/` の lib は、エンジンの .gitignore の `[Dd]ebug/`・`[Rr]elease/` に引っかかって **git に入っていない**（`assimp-vc143-mtd.lib` が 85MB あるので、入れないと決めた分）。
なので GitHub から clone したサブモジュールには lib が無く、このままだとリンクで `LNK1104: ファイル 'assimp-vc143-mtd.lib' を開くことができません` になる。
今のエンジンのフォルダからコピーする（PowerShell）：

```powershell
Copy-Item -Recurse C:\Users\K025G\source\repos\MyEngine_v1\project\externals\assimp\lib C:\Users\K025G\source\repos\AL4_01\MyEngine_v1\project\externals\assimp\
```
- コピー先の `assimp\lib\Debug\`・`assimp\lib\Release\` に lib が並べば OK。
- コピーしたファイルもサブモジュールの .gitignore で無視されるので、`git status` には出てこない（エンジンのリポジトリを汚さない）。
- **別の PC で clone したときも、毎回これが要る**（6章）。git に入れる方法は提出の後に考える（今のメモの通り vcpkg 案など）。

---

## 3. パスを直す

### ===== `project/AL4_01.vcxproj`（79行目あたり） =====
```xml
  <ImportGroup Label="PropertySheets">
    <Import Project="..\..\MyEngine_v1\project\MyEngine.props" />
  </ImportGroup>
```
↓
```xml
  <ImportGroup Label="PropertySheets">
    <!-- エンジンはサブモジュール（AL4_01/MyEngine_v1/）。props がエンジンの場所・参照・ライブラリを全部決める -->
    <Import Project="..\MyEngine_v1\project\MyEngine.props" />
  </ImportGroup>
```
`..\..\` → `..\` に変わるだけ（`project/` から1つ上がれば AL4_01 のルート）。

### ===== `project/AL4_01.slnx`（3か所） =====
```xml
  <Folder Name="/externals/">
    <Project Path="../../MyEngine_v1/project/externals/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj" Id="371b9fa9-4c90-4ac6-a123-aced756d6c77">
      <BuildType Solution="Release|*" Project="Development" />
    </Project>
    <Project Path="../../MyEngine_v1/project/externals/imgui/imgui.vcxproj" Id="86a1cafe-73b6-4e0a-8c54-ef98278a3806">
      <BuildType Solution="Release|*" Project="Development" />
    </Project>
  </Folder>
  <Project Path="../../MyEngine_v1/project/MyEngine_v1.vcxproj" Id="1d0f63d3-16bb-43b8-985e-6bb516a1d662">
```
↓
```xml
  <Folder Name="/externals/">
    <Project Path="../MyEngine_v1/project/externals/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj" Id="371b9fa9-4c90-4ac6-a123-aced756d6c77">
      <BuildType Solution="Release|*" Project="Development" />
    </Project>
    <Project Path="../MyEngine_v1/project/externals/imgui/imgui.vcxproj" Id="86a1cafe-73b6-4e0a-8c54-ef98278a3806">
      <BuildType Solution="Release|*" Project="Development" />
    </Project>
  </Folder>
  <Project Path="../MyEngine_v1/project/MyEngine_v1.vcxproj" Id="1d0f63d3-16bb-43b8-985e-6bb516a1d662">
```
`../../MyEngine_v1/` → `../MyEngine_v1/` を3か所。それ以外（Id・BuildType・AL4_01.vcxproj の行）はそのまま。

- **Visual Studio を閉じてから直す**（開いたままだと、VS が古い slnx で上書きすることがある）。
- `MyEngine.props` の中の `ProjectReference`（エンジン・DirectXTex・imgui を参照する所）は `$(MyEngineRoot)` から書いてあるので、直さなくていい。

---

## 4. ビルド・起動

### ===== 古い中間ファイルを消す =====
`AL4_01/generated/obj/MyEngine_v1/` などには、**外のフォルダ**（`source/repos/MyEngine_v1`）のソースから作った .obj と、その記録（.tlog）が残っている。
ソースの場所が変わるので、1回だけ `AL4_01/generated/` を丸ごと消してからビルドする（ビルドの出力だけのフォルダなので、消しても次のビルドでまた作られる）。

```powershell
Remove-Item -Recurse -Force C:\Users\K025G\source\repos\AL4_01\generated
```

### ===== ビルドして起動 =====
1. `AL4_01/project/AL4_01.slnx` を Visual Studio で開く。
2. ソリューションエクスプローラーで、MyEngine_v1・DirectXTex・imgui のプロジェクトが「読み込めません」になっていないか見る（なっていたら 3章のパスの写し違い）。
3. Debug でビルド → F5。今までどおりエディタが起動すれば OK。
4. Development・Release も1回ずつビルドして起動する。

### ===== エンジンのシェーダーがサブモジュールから読まれているか =====
Debug で起動中に、`AL4_01/MyEngine_v1/project/MyEngine/Shader/Data/` の .shader をどれか少しだけ変えて（色を変えるなど）起動し直すと、反映される（`MYENGINE_ROOT_DIR` がサブモジュールを指している）。確かめたら元に戻す。

---

## 5. コミット・push

```bash
cd C:/Users/K025G/source/repos/AL4_01
git add .gitmodules MyEngine_v1 project/AL4_01.vcxproj project/AL4_01.slnx
git commit -m "エンジンをサブモジュールにした"
git push -u origin feature/EngineSubmodule
```
- GitHub の AL4_01 を開くと、`MyEngine_v1 @ e300a7d` のようなリンクになっている（押すとエンジンのリポジトリのそのコミットに飛ぶ）。
- 動くのを確かめたら develop にマージする。

---

## 6. これからの使い方

### ===== エンジンを直す流れ =====
エンジンのフォルダが2つになる（外の `source/repos/MyEngine_v1` と、`AL4_01/MyEngine_v1`）。**どちらも同じ GitHub のリポジトリの clone** で、間は GitHub の push/pull でつなぐ。
ゲームで動かすのはサブモジュールの方なので、**エンジンのコード（写経）はサブモジュールの方で直す** のがおすすめ（直してすぐ AL4_01 で動かせる）。

1. `AL4_01/MyEngine_v1/` の中のエンジンのコードを直す（VS では今までどおり、ソリューションの MyEngine_v1 プロジェクトのファイルを直せばいい）。
2. サブモジュールの中でコミット・push（エンジンのリポジトリに入る）：
   ```bash
   cd C:/Users/K025G/source/repos/AL4_01/MyEngine_v1
   git add -A
   git commit -m "〇〇を追加"
   git push
   ```
3. 親（AL4_01）で、新しいエンジンのコミットを指すようにしてコミット：
   ```bash
   cd C:/Users/K025G/source/repos/AL4_01
   git add MyEngine_v1
   git commit -m "エンジンを更新"
   ```
   - 2 の後に親で `git status` を見ると `modified: MyEngine_v1 (new commits)` と出る。「サブモジュールの中のコミットが、親が覚えているのと違う」という意味。`git add MyEngine_v1` で今のコミットを覚えさせる。
   - **2 の push を忘れて 3 だけ push すると、別の PC で clone したときに「そのコミットが無い」で失敗する**（8章）。必ずエンジンを先に push。
4. 外の `source/repos/MyEngine_v1`（Docs や md を置いている方）は、使う前に `git pull` すれば同じになる。

Visual Studio の「Git 変更」ウィンドウには、AL4_01 と MyEngine_v1 の2つのリポジトリが出る（上のリポジトリの名前のところで切り替える）。

### ===== 「HEAD が切り離されています（detached HEAD）」に注意 =====
`git submodule update` をすると、サブモジュールの中は **ブランチではなく、コミットそのもの** に切り替わる（親が覚えているコミットを出すだけなので）。
このままコミットすると、どのブランチにも載らないコミットになって、あとで迷子になる。
**サブモジュールの中でコミットする前に、`git status` でブランチの上にいるか見る**。`HEAD detached at …` と出たら、先にブランチに切り替える：
```bash
git -C MyEngine_v1 switch feature/UIEditor
```

### ===== 外のフォルダで直したエンジンを、AL4_01 に持ってくる =====
外の `source/repos/MyEngine_v1` でコミットした場合は、そっちで push してから：
```bash
cd C:/Users/K025G/source/repos/AL4_01
git -C MyEngine_v1 pull
git add MyEngine_v1
git commit -m "エンジンを更新"
```

### ===== 別の PC（学校の PC など）で clone する =====
```bash
git clone --recurse-submodules https://github.com/yamamoto-mitsuki/AL4_01.git
```
- `--recurse-submodules` を付けると、サブモジュール（エンジン）も一緒に取ってくる。
- 付け忘れたときは、clone した後に AL4_01 のルートで：
  ```bash
  git submodule update --init
  ```
- そのあと **2章の assimp の lib のコピー** が要る（USB などで持っていく）。

### ===== 別の PC で、新しいエンジンを取ってくる =====
```bash
git pull
git submodule update
```
`git pull` だけだと、親が覚えているコミットは新しくなるのに、サブモジュールの中身は古いまま（`git status` に `modified: MyEngine_v1` と出る）。`git submodule update` で中身を合わせる。

---

## 7. 確かめたこと
- 一時フォルダに AL4_01 の develop（ff829f3）を clone → 本体の feature/UIEditor（e300a7d）を `git submodule add` → 3章の直し → 2章の lib のコピー → `MSBuild AL4_01.slnx /p:Configuration=Debug /p:Platform=x64`。
- **Debug のビルドは通った**（エラー 0。警告は C4865・C4005 で、今までと同じもの）。exe は `AL4_01/generated/outputs/Debug/AL4_01.exe`、シェーダーとエンジンの画像は `AL4_01/project/MyEngine/` にコピーされた。
- ビルド後、サブモジュールの中の `git status` は空（出力は `AL4_01/generated/` に出て、サブモジュールの中は汚れない）。
- `project/` を作業フォルダにして起動 → **grid.obj が無くて assert**（0章の1つ目の原因）。grid.obj を置いて（＝0章の直しをした状態）、AL4_01 の未コミットの Resources も写して起動し直したら、Level01 を開いたエディタが起動し、20秒動かしてログにエラー・警告は無かった。
- Development・Release はビルドしていない（4章で確かめる）。
- clone したばかりだと `imgui.ini`（ウィンドウの並び）が無いので、エディタのウィンドウが重なって出る（git に入れない設定のファイルなので正しい動き）。今の PC の AL4_01 には手元にあるので、そのまま使われる。

---

## 8. 困ったとき

| 症状 | 原因と直し方 |
|---|---|
| ソリューションを開くと MyEngine_v1 が「読み込めません」 | 3章の slnx のパスの写し違い。または 1章をルートでなく `project/` でやった（`AL4_01/project/MyEngine_v1/` にできている） |
| `MyEngine.props が見つかりません` | 3章の vcxproj の行の写し違い |
| `LNK1104: assimp-vc143-mtd.lib を開けません` | 2章の lib のコピーを忘れている |
| 起動で `Unable to open file "MyEngine/Resources/Model/Grid/grid.obj"` | 0章の .gitignore の直しと grid.obj のコミットが、サブモジュールが指すコミットに入っていない（push して、1章の `switch` からやり直すか、`git -C MyEngine_v1 pull`） |
| clone で `fatal: … not our ref` / `reference is not a tree` | 親が指しているエンジンのコミットが GitHub に無い。エンジンの方を push する（6章の 2） |
| 親の `git status` に `modified: MyEngine_v1 (modified content)` | サブモジュールの中に未コミットの変更がある。中で `git status` を見る |
| 親の `git status` に `modified: MyEngine_v1 (new commits)` | 中のコミットが進んだ。良ければ `git add MyEngine_v1` してコミット（6章） |
| サブモジュールの中で直したのに、別のフォルダで反映されない | push/pull を忘れている。フォルダが2つあるので、間は GitHub でつなぐ |

---

## 9. 元に戻す
`feature/EngineSubmodule` ブランチでやっているので、develop に戻ればサブモジュールの無い形に戻る。
```bash
cd C:/Users/K025G/source/repos/AL4_01
git switch develop
```
- develop に戻っても、`AL4_01/MyEngine_v1/` のフォルダ自体は残る（git が消さない）。邪魔なら手で消していい（外の `source/repos/MyEngine_v1` は消さないこと）。
- develop の slnx・vcxproj は `../../MyEngine_v1` のままなので、外のエンジンをそのまま使う。
- 戻した後は、4章と同じ理由で `AL4_01/generated/` を1回消してからビルドする。
