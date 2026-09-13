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
  - 繁殖成功時は1〜2体の子個体を生成
- 肉食
  - 空腹時は草食を探して追跡する
  - 草食を食べるとエネルギー回復
  - 5〜7回程度食べると繁殖状態へ
  - 繁殖成功時は1体の子個体を生成
  - 寿命内に複数回の捕食機会を得やすくするため、エネルギー消費量は0.20 / simulation step
- 繁殖状態では同種個体へ近づき、接触すると子個体を生成
- 動物は移動方向に応じて左右を向く
- 動物には歩行・走行の2種類の移動モーションがある
  - 通常徘徊・繁殖相手への移動: 歩行
  - 草食の逃走・餌への移動: 走行
  - 肉食の獲物追跡: 走行
- 動物には年齢と寿命がある
  - 600 simulation stepごとに年齢が1増える
  - 草食の寿命は個体ごとに8〜12
  - 肉食の寿命は個体ごとに12〜16
  - 寿命に達すると自然死する
  - 自然死した場所にも草の種を生成する
  - 旧HSP版にも `nen` / `jumyo` は存在したが、加齢処理が無効化されていたため、C++版で再度有効化
- 個体死亡時、その周囲へ草の種を5個生成
  - 種は一定時間後に成長済みの草へ変化
  - 草食が餌として認識するのは成長済みの草のみ
- 死亡した動物は灰色の倒れた姿で短時間表示する
  - 初期値は30描画フレーム
  - シミュレーション速度をx16にしても表示時間が極端に短くならないよう、simulation stepではなく描画フレームで管理
  - Pause中は死亡表示も停止する
- シミュレーション中の出生・死亡統計を表示する
  - 出生数は初期配置を含まず、繁殖で生まれた個体のみを集計
  - 死亡原因を Old Age / Starvation / Predation に分けて種別ごとに集計
  - `R` でResetすると統計も0に戻る

旧ソースから数値を確定できなかったエネルギー量・消費量・通常移動速度・草の自然再生間隔などは `SimulationConfig` にまとめ、あとから容易に調整できるようにしています。草食・肉食の1回あたりの出産数、`framesPerAge`、種別ごとの寿命範囲、死亡表示時間も `SimulationConfig` で調整できます。

## Graphics

旧HSP版の `img.bmp` に相当する24x24ドット絵を新しく作成しています。

`img.bmp` は 4列 x 3行のスプライトシートとして整理しています。

| Row | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- |
| 0 | Grass | Seed | Herbivore Dead | Carnivore Dead |
| 1 | Herbivore Walk 1 | Herbivore Walk 2 | Herbivore Run 1 | Herbivore Run 2 |
| 2 | Carnivore Walk 1 | Carnivore Walk 2 | Carnivore Run 1 | Carnivore Run 2 |

スプライトの元データは `src/SpriteAsset.cpp` のテキストパターンとして保持し、起動時に96x72 / 24bit BMPの `assets/img.bmp` を生成します。このためバイナリ画像を別途配置しなくても、そのまま起動できます。

生成したBMPは `mygame::ImageManager::LoadDivided` で12分割して読み込みます。背景色 `(255, 0, 255)` は透過色として扱います。

歩行・走行とも2コマアニメーションです。コマ切り替えは単純なフレーム数ではなく、個体が実際に移動した距離を基準にしているため、シミュレーション速度を変更しても移動量に応じてアニメーションします。停止中は1コマ目で静止します。

死亡スプライトは通常スプライトを灰色化して90度倒した姿として生成します。

画像生成・読み込みに失敗した場合は、従来の円描画へ自動的にフォールバックします。フォールバック時も死亡個体は灰色の横長表示で短時間残ります。

空腹状態はオレンジ枠、繁殖状態は紫枠で表示します。動物スプライトは移動方向に応じて左右反転します。

## myGameUtil

`Aryudesu/myGameUtil` の `feature/core-game-utilities` ブランチの現在のコミットを固定して利用しています。

- `mygame::Random`
- `mygame::InputManager`
- `mygame::Vec2` (`Collision2D`)
- `mygame::ImageManager`
- `mygame::FileUtil`

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
