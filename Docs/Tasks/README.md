参考：
- `git show`（昔のコミットのファイルを読む）：https://git-scm.com/docs/git-show
- `git tag`：https://git-scm.com/book/ja/v2/Git-の基本-タグ

# Docs/Tasks の md について

## 残している md（話題ごとの記録）

終わっても消さずに、「決めたこと」「未解決」「この先の予定」を足していく md。

| md | 話題 |
|---|---|
| [AL4_01Game.md](AL4_01Game.md) | 課題ゲーム（アイス・ジャンプ）の仕様・作った順番。この先の G-3（スター）・G12・G13 もここ |
| [Animation.md](Animation.md) | アニメーション・Skeleton・Skinning の設計と学校との分担 |
| [Collision.md](Collision.md) | Collider・CollisionSystem の設計と、Play / Stop で落ちた件 |
| [Compute.md](Compute.md) | Compute Shader でやること（GPU パーティクル・ライトのカリング など）の順番 |
| [Editor.md](Editor.md) | エディタの作り（Inspector・Undo・コピー）の記録 |
| [FrameLoop.md](FrameLoop.md) | 1フレームの進め方・GPU を待たない作り・マルチスレッドの検討 |
| [Light.md](Light.md) | ライトの作業と決めたこと |
| [Material.md](Material.md) | マテリアルの予定（屈折 など） |
| [Model.md](Model.md) | モデル読み込み（assimp）の予定 |
| [PostEffect.md](PostEffect.md) | ポストエフェクト（Bloom・HDR・ToneMap）と、次の G-3 スターの目標 |

## 消した md（写経用の手順書）

写経が終わってコードに入った手順書は消した。読みたいときは、消す前のコミットから出せる。

```bash
git show AL4_01_submit:Docs/Tasks/G11DepthOfField.md
```

| 消した日 | 読める場所 | md |
|---|---|---|
| 2026-10-28 | タグ `AL4_01_submit` | G5Player・G6Polygon・G7Gimmick・G7bLaunchArrow・G7cCurvedArrow・G8Hazards・G9aTriggerActivator・G9bUIEditor・G9cSceneTransition・G9dUIScreen・G9dSelectedStyle・G10Forest・G11DepthOfField・EditorFolder・AnimationManager・PathLoad・TextEffects・EngineSubmodule・KawaseBloom・HDRRendering・ParticleSystem・ParticleRotation・BloomEdgeClamp・Entity |
| それより前 | コミット `6423542` の1つ前（`6423542^`） | Audio・Serialize・GameBasics・InkSlash・SceneFlow |

- 手順書の画像（`images/`）は `.gitignore` で git に入れていない（30MB あるため）。上のやり方で出した md の画像は表示されない。
- EdgeStiffness.md・G1Trampoline.md は昔の worktree だけにあった物で、git には一度も入っていない。
