# SD-Verb

[English](README.md) | [日本語](README.ja.md)

SD-Verb は、C++20 と JUCE 8 で実装された 64bit VST3 / スタンドアロン対応のリバーブプラグインです。短めの減衰を持つ Feedback Delay Network (FDN) および Schroeder 型リバーブに加え、テンポ同期対応ゲートとダッキング機能を内蔵しています。

## 主な機能

- **2種類の残響アルゴリズム**
  - **DENSE**: 高速ウォルシュ・アダマール変換（FWHT）を用いた 8 チャンネル FDN リバーブ。
  - **METAL**: チャンネルあたり 4 並列コムフィルタと 2 直列オールパスフィルタで構成された Schroeder 型リバーブ（44.1 kHz 基準で R チャンネルに +23 サンプルのオフセットを付与）。
- **入力フィルタと Pre-Delay**
  - 線形補間付きの Pre-Delay（`0.0 - 100.0 ms`）。
  - 2次バターワース HPF（`20 - 1000 Hz`、Q = 0.7071）および LPF（`1000 - 20000 Hz`、Q = 0.7071）。
- **共通サイドチェインによるゲート＆ダッキング**
  - ゲートとダッカーで共有するピークエンベロープフォロワー（Attack: `1.0 ms`、Release: `50.0 ms`）。
  - `-3.0 dB` のヒステリシスと `1.5 ms` の再オープン時スムージングを備えた 3 ステートゲート（`OPEN`, `HOLD`, `SHUT`）。
  - DAW の `AudioPlayHead` から取得した BPM へのテンポ同期（`1/32`, `1/16`, `1/8`, `1/4`, `1/16T`, `1/8D`、BPM 未取得時は `120.0 BPM` で動作）、またはミリ秒での直接指定。
  - 固定閾値（`-36.0 dB`、Attack: `2.0 ms`、Release: `80.0 ms`）のリバーブダッキング。
- **オーディオ処理の安定性**
  - `prepareToPlay()` 時に最大 `192 kHz` 相当のディレイバッファを事前確保し、オーディオスレッド上での動的メモリ確保を行いません。
  - 連続パラメータには `5.0 ms` のスムージングを適用し、ディレイ読み出しに線形補間を使用することで、操作時のジッパーノイズを抑えています。
- **ユーザーインターフェース**
  - アスペクト比固定リサイズ対応（基準 `720 x 410 px`、最小 `540 x 308 px`、最大 `1440 x 820 px`）。
  - 4種類のカラーテーマ（`LIGHT` / `DARK` × `ORG` / `CYN`）。選択状態はプラグインのステートと共に保存されます。
  - 160 ポイントのリアルタイム・エンベロープスコープ（`8 ms` 間隔、`1.28 s` 表示）、設定連動 Shape Guide、ステレオ幅インジケータを搭載。

---

## 用語説明

- **FDN (Feedback Delay Network)**: 複数の遅延線（ディレイライン）の出力を行列で混合し、入力側へ帰還させて残響を作る方式です。本プラグインの `DENSE` モードで使用しています。
- **Schroeder 型リバーブ**: 並列コムフィルタと直列オールパスフィルタを組み合わせた残響生成方式です。`METAL` モードでは短いディケイ時に金属的な共振が得られます。
- **リバーブダッキング (Reverb Ducking)**: 原音（Dry 信号）の音量に応じて残響音（Wet 信号）のゲインを一時的に下げる処理です。原音が連続するフレーズで残響による音の濁りを抑えたい場合に使用します。
- **ゲートリバーブ (Gated Reverb)**: 入力音が閾値を下回った後、設定した保持時間（Hold）だけ残響を維持し、Release 時間で遮断する処理です。

---

## パラメータ一覧

| パラメータ ID | 表示名 | 範囲 | 初期値 | 説明 |
| :--- | :--- | :--- | :--- | :--- |
| `algo` | Mode | `DENSE`, `METAL` | `DENSE` | 残響アルゴリズムの切替（切替時に内部バッファをゼロクリア） |
| `size` | Size | `10.0 - 100.0 %` | `35.0 %` | 内部ディレイ長に対する空間サイズ倍率 |
| `decay` | Decay | `0.05 - 5.00 s` | `0.80 s` | 残響の減衰時間（RT60） |
| `predelay` | Pre-Delay | `0.0 - 100.0 ms` | `10.0 ms` | 入力信号がリバーブへ到達するまでの遅延時間 |
| `damping` | Damping | `0.0 - 100.0 %` | `40.0 %` | 帰還ループ内の高域減衰量（`20 kHz` ～ `1 kHz`） |
| `in_hpf` | Low Cut | `20 - 1000 Hz` | `120 Hz` | 入力段 2次バターワース HPF のカットオフ周波数 |
| `in_lpf` | High Cut | `1000 - 20000 Hz` | `12000 Hz` | 入力段 2次バターワース LPF のカットオフ周波数 |
| `gate_enabled` | Gate Active | `OFF`, `ON` | `OFF` | ゲートの有効／バイパス切替 |
| `gate_thresh` | Gate Thresh | `-60.0 - 0.0 dB` | `-24.0 dB` | ゲートが開く閾値（閉じる閾値は `-3.0 dB` 下に設定） |
| `gate_sync` | Hold Sync | `OFF`, `ON` | `OFF` | ホールド時間の単位を ms と DAW テンポ同期（音価）で切替 |
| `gate_hold` | Hold Time | `0.0 - 500.0 ms` | `80.0 ms` | `Hold Sync` が `OFF` のときのゲート保持時間 |
| `gate_rate` | Hold Rate | `1/32` .. `1/8D` | `1/16` | `Hold Sync` が `ON` のときのゲート保持音価 |
| `gate_rel` | Gate Rel | `5.0 - 300.0 ms` | `30.0 ms` | ホールド終了後にゲートが閉じるまでの減衰時間 |
| `duck_amount` | Ducking | `0.0 - 100.0 %` | `0.0 %` | 原音入力時に残響を下げる深さ |
| `width` | Width | `0.0 - 200.0 %` | `100.0 %` | 残響音の Mid/Side ステレオ幅 |
| `mix` | Mix | `0.0 - 100.0 %` | `30.0 %` | 原音と残響音のバランス |

---

## 導入方法（Windows x64 配布バイナリ）

1. GitHub Releases ページから `SD-Verb-Windows-x64.zip` をダウンロードして解凍します。
2. `SD-Verb.vst3` フォルダを `C:\Program Files\Common Files\VST3\` へコピーします。
3. REAPER を起動し、表示されない場合は **オプション -> 設定 -> プラグイン -> VST -> 再スキャン** を実行します。

---

## 初期プリセット一覧

プラグイン画面右上のドロップダウン、または REAPER 上部の VST3 プリセット一覧から選択できます。

| # | プリセット名 | モード | 用途・設定内容 |
| :---: | :--- | :---: | :--- |
| 0 | `Default (Init)` | `DENSE` | 初期設定（`Decay: 0.80 s`, `Mix: 30 %`） |
| 1 | `Main Lead - Tight Ducking` | `DENSE` | 主旋律向け。`Ducking 55 %` と `Low Cut 220 Hz` で発音中の濁りを抑える |
| 2 | `Main Lead - 1/16 Gate` | `DENSE` | 刻み・スタッカート主旋律向け。16分音符同期ゲートで拍ごとに残響を遮断 |
| 3 | `Harmony / Backing - Wide` | `DENSE` | ハモリ・バッキング向け。`Width 165 %`、`300 Hz - 8.5 kHz` 帯域制限 |
| 4 | `Vocal - Short Room + Duck` | `DENSE` | ボーカル／音声素材向け。`Pre-Delay 20 ms` で発音頭を残し `Ducking 45 %` を適用 |
| 5 | `Drum Snare - Gated Metal` | `METAL` | スネア・クラップ向け。`Hold 120 ms` / `Release 35 ms` の金属的ゲートリバーブ |
| 6 | `Drum Kit - Tight Room` | `DENSE` | ドラムバス／パーカッション向け。`Size 22 %` / `Decay 0.38 s` の短い空間 |
| 7 | `SE / Synth Hit - Metallic Tail` | `METAL` | 単発 SE・ヒット音向け。`Size 78 %` / `Decay 2.40 s` の金属的残響 |

---

## 設定例

- **発音の輪郭を保つショートリバーブ＋ダッキング（ボーカル／音MADの刻み向け）**
  - `Mode`: `DENSE`、`Size`: `35 %`、`Decay`: `0.60 s`、`Ducking`: `60 %`、`Mix`: `30 %`
  - 原音が鳴っている間は残響が下がり、音と音の隙間だけ残響が前に出ます。
- **テンポ同期ゲートリバーブ（打楽器／単発ヒット向け）**
  - `Mode`: `METAL`、`Size`: `50 %`、`Decay`: `1.50 s`、`Gate Active`: `ON`、`Gate Thresh`: `-24 dB`、`Hold Sync`: `ON`、`Hold Rate`: `1/16`、`Gate Rel`: `30 ms`
  - 16分音符の長さだけ残響を残し、次の拍に合わせて残響を遮断します。

## UI 操作

- **ノブをダブルクリック**: デフォルト値にリセット
- **`Shift + ダブルクリック` / `Alt + クリック` / 数値欄クリック**: キーボードによる数値直接入力（`12k`、`1/16T` などの単位・音価表記にも対応）
- **ノブを右クリック**: DAW ホストのパラメータメニューを表示
- **右上のプリセット選択欄**: 8種類の初期プリセットを切り替え、DAW 側のプログラム表示と同期

---

## ビルド方法（Windows x64）

### 必要環境

- Windows 10 / 11 (x64)
- CMake 3.22 以上
- Visual Studio 2022（または Build Tools for Visual Studio 2022 / C++20 対応ツールセット）
- リポジトリ直下の `JUCE/` ディレクトリに JUCE 8 を配置

### ビルド手順

```powershell
# JUCE 8 が未配置の場合はクローンする
git clone --depth 1 --branch 8.0.9 https://github.com/juce-framework/JUCE.git JUCE

# Release 構成でコンフィグ・ビルド（VST3 と Standalone）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /nr:false /m:1
```

> 注: MSBuild に `/nr:false /m:1` を渡すことでノード再利用を無効化し、一部の Visual Studio 2022 Build Tools 環境で発生するビルド待機（ハング）を回避できます。

### 出力ファイル

- **VST3**: `build/SDVerb_artefacts/Release/VST3/SD-Verb.vst3`
- **Standalone**: `build/SDVerb_artefacts/Release/Standalone/SD-Verb.exe`

`SD-Verb.vst3` を `C:\Program Files\Common Files\VST3\` へコピーすると、REAPER などの VST3 対応 DAW から読み込めます。

---

## ディレクトリ構成

```text
.
├── .github/
│   └── workflows/
│       └── build.yml
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
├── README.ja.md
├── Resources/
│   └── Fonts/
│       ├── ChakraPetch-OFL.txt
│       ├── ChakraPetch-SemiBold.ttf
│       ├── EncodeSansExpanded-Black.ttf
│       └── OFL.txt
└── Source/
    ├── PluginEditor.cpp
    ├── PluginEditor.h
    ├── PluginProcessor.cpp
    ├── PluginProcessor.h
    ├── dsp/
    │   ├── FdnReverb.h
    │   └── GateDucker.h
    └── ui/
        ├── RetroLookAndFeel.h
        └── VerbVisualizer.h
```

## ライセンス

- プラグイン本体ソースコード: [MIT License](LICENSE)
- 組み込みフォント（`Encode Sans Expanded`, `Chakra Petch`）: [SIL Open Font License 1.1](Resources/Fonts/OFL.txt) / [ChakraPetch-OFL.txt](Resources/Fonts/ChakraPetch-OFL.txt)
- フレームワーク: [JUCE](https://juce.com/)（バイナリを再配布する場合は `JUCE/LICENSE.md` のライセンス条項を確認してください）
