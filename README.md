# Ecosystem

昔HSPで作成した生態系シミュレータを、C++17 / DxLibへ移植したプロジェクトです。

## 現在の移植内容

旧HSP版で確認できた基本構造を優先して再現しています。

- シミュレーション領域: 640 x 480
- ウィンドウサイズ: 960 x 480
  - 左 640 x 480: 生態系シミュレーション
  - 右 320 x 480: 情報表示パネル
- 初期個体数
  - 肉食: 10
  - 草食: 50
  - 草: 100
- 個体上限
  - 草食: 300
  - 肉食: 100
  - 動物配列全体: 400
  - 草: 300
  - 草食と肉食は種別ごとに上限を持つため、片方が増え切っても他方の繁殖枠を奪わない
- 個体状態
  - Normal
  - Hungry
  - Breeding
- 草食
  - 近くに肉食がいると逃走する
  - 逃走時は16方向の候補から、最も近い肉食との距離を最大化する方向を選ぶ
  - 複数の肉食を同時に考慮するため、1匹から逃げて別の肉食へ近づく挙動を抑える
  - フィールド外へ出る候補は除外するため、壁際や角でも移動可能な方向へ逃げる
  - 空腹時は草を探す
  - 草を食べるとエネルギー回復
  - 2〜4回程度食べると繁殖状態へ
  - 繁殖成功時の子個体数は現在の草食個体数に応じて変化する
    - 100未満: 2〜3体
    - 100〜199: 1〜2体
    - 200以上: 1体
  - 繁殖相手の探索範囲は64
  - 繁殖までに十分な採食機会を得られるよう、エネルギー消費量は0.10 / simulation step
- 肉食
  - 空腹時は草食を探して追跡する
  - 獲物探索範囲は空腹度に応じて段階的に拡大
    - 通常の空腹時: 64
    - energy <= 30: 128
    - energy <= 15: 160
  - 草食を食べるとエネルギー回復
  - 5〜7回程度食べると繁殖状態へ
  - 繁殖成功時は1体の子個体を生成
  - 個体数が少ないときにも番を見つけられるよう、繁殖相手の探索範囲は800（640x480フィールド全域）
  - 頻繁な空腹による過剰捕食と、その後の餓死を抑えるため、エネルギー消費量は0.10 / simulation step
- 繁殖条件を満たしていても空腹になった場合は Hungry を優先し、採食・捕食で回復してから再び繁殖相手を探す
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
- 個体死亡時、その周囲へ草の種を最大5個生成
  - 半径32以内に草・種が6個以上ある地点には追加の種を置かず、死亡が集中した場所だけで草スロットを使い切るのを抑える
  - 種は一定時間後に成長済みの草へ変化
  - 草食が餌として認識するのは成長済みの草のみ
- 草の自然再生
  - 24 simulation stepごとに最大8地点をランダムに試す
  - 半径32以内の草・種が6個未満の地点だけを再生候補にする
  - 草の総数が初期値100以上でも、全体上限300未満なら疎な地域へ1本ずつ再生できる
  - 局所的な食べ尽くしが起きても、別地域に草が残っているだけで再生が完全停止しないようにする
- 死亡した動物は灰色の倒れた姿で短時間表示する
  - 初期値は30描画フレーム
  - シミュレーション速度をx16にしても表示時間が極端に短くならないよう、simulation stepではなく描画フレームで管理
  - Pause中は死亡表示も停止する
- 右側の情報表示パネルに現在値・統計・個体数推移・操作説明を表示する
  - 現在の Carnivore / Herbivore / Grass 数
  - Frame / Speed / RUNNING・PAUSED 状態
  - 出生数
  - Old Age / Starvation / Predation の死亡原因別統計
  - Population Graph
    - 100 simulation stepごとに Carnivore / Herbivore / Grass の個体数を記録
    - 最大280サンプルを保持し、古いデータから順にスクロール
    - Carnivore は赤、Herbivore は青、Grass は緑の折れ線で表示
    - 縦軸は0〜300で固定し、3系列を同じスケールで比較
  - 操作説明
  - `R` でResetすると統計とPopulation Graphの履歴もリセット

旧ソースから数値を確定できなかったエネルギー量・消費量・通常移動速度・草の自然再生間隔などは `SimulationConfig` にまとめ、あとから容易に調整できるようにしています。草食の密度依存繁殖の閾値・段階別出産数、肉食の出産数、種別ごとの繁殖相手探索範囲、肉食の空腹度別獲物探索範囲、草の局所密度半径・上限・自然再生試行回数、`framesPerAge`、種別ごとの寿命範囲、死亡表示時間も `SimulationConfig` で調整できます。

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
