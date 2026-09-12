# Ecosystem

昔HSPで作成した生態系シミュレータを、C++17 / DxLibへ移植したプロジェクトです。

## 現在の移植内容

旧HSP版で確認できた基本構造を優先して再現しています。

- 画面サイズ: 640 x 480
- 初期個体数
  - 肉食: 10
  - 草食: 50
  - 草: 100
- 最大オブジェクト数: 300
- 個体状態
  - Normal
  - Hungry
  - Breeding
- 草食
  - 近くの肉食から逃げる
  - 空腹時は草を探す
  - 草を食べるとエネルギー回復
  - 2〜4回程度食べると繁殖状態へ
- 肉食
  - 空腹時は草食を探して追跡する
  - 草食を食べるとエネルギー回復
  - 4〜6回程度食べると繁殖状態へ
- 繁殖状態では同種個体へ近づき、接触すると子個体を生成
- 個体死亡時、その周囲へ草を5個生成
- 寿命による死亡は入れていません
  - 旧HSP版でも、生態系が早く滅びるため寿命処理は無効化していました

旧ソースから数値を確定できなかったエネルギー量・消費量・通常移動速度・草の自然再生間隔などは `SimulationConfig` にまとめ、あとから容易に調整できるようにしています。

## Graphics

旧HSP版の `img.bmp` に相当する24x24ドット絵を新しく作成しています。

- 0: 草
- 1: 草食（青白のウサギ風）
- 2: 肉食（赤茶のキツネ風）

スプライトの元データは `src/SpriteAsset.cpp` のテキストパターンとして保持し、起動時に72x24 / 24bit BMPの `assets/img.bmp` を生成します。

生成したBMPは `mygame::ImageManager::LoadDivided` で3分割して読み込みます。背景色 `(255, 0, 255)` は透過色として扱います。

画像読み込みに失敗した場合は、従来の円描画へ自動的にフォールバックします。

空腹状態はオレンジ枠、繁殖状態は紫枠で表示します。

## myGameUtil

`Aryudesu/myGameUtil` の `feature/core-game-utilities` ブランチの現在のコミットを固定して利用しています。

- `mygame::Random`
  - 初期配置
  - 移動方向
  - 繁殖までに必要な食事回数
  - 死亡地点周辺への草生成
- `mygame::InputManager`
  - ポーズ
  - 1フレーム実行
  - リセット
  - シミュレーション速度変更
- `mygame::Vec2` (`Collision2D`)
  - 生物・草の座標
- `mygame::ImageManager`
  - スプライトシートの分割読み込みと描画
- `mygame::FileUtil`
  - 起動時の `assets/img.bmp` 生成

CMakeでは `FetchContent` で取得します。

## 操作

| Key | Action |
| --- | --- |
| `SPACE` | Pause / Resume |
| `N` | Pause中に1 step進める |
| `R` | 初期状態へReset |
| `UP` | シミュレーション速度を上げる |
| `DOWN` | シミュレーション速度を下げる |
| `ESC` | 終了 |

## Build

### Requirements

- Windows
- Visual Studio / MSVC
- C++17以上
- DxLib
- CMake 3.20以上（CMakeを使用する場合）

### CMake

DxLibのinclude/libディレクトリを指定します。

```powershell
cmake -S . -B build `
  -DDXLIB_INCLUDE_DIR="<DxLib.h があるディレクトリ>" `
  -DDXLIB_LIBRARY_DIR="<DxLib の lib ディレクトリ>"
cmake --build build --config Release
```

`DxLib.h` 側の自動リンク設定を利用する構成です。

普段使用しているVisual StudioのDxLibプロジェクトへ `src` と `include` を追加してビルドしても構いません。その場合は `myGameUtil/include` も追加のインクルードディレクトリへ設定してください。

## Structure

```text
Ecosystem/
├─ include/Ecosystem/
│  ├─ Renderer.h
│  ├─ Simulation.h
│  └─ SpriteAsset.h
├─ src/
│  ├─ main.cpp
│  ├─ Renderer.cpp
│  ├─ Simulation.cpp
│  └─ SpriteAsset.cpp
├─ CMakeLists.txt
└─ README.md
```

`Simulation` は生態系ロジック、`Renderer` / `SpriteAsset` は描画とスプライト生成、`main.cpp` はDxLibのメインループ・入力を担当します。
