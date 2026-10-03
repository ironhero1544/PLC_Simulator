# PLC_Simulator
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)]()
![plcsimulator_lockup_whitebg.svg](docs_img/plcsimulator_lockup_whitebg.svg)

物理シミュレーション、OpenPLC互換ラダー実行パス、RTLハードウェア設計ワークフローを統合した、リアルタイムPLC配線/プログラミングシミュレーターです。

[English](README.md), [한국어](README_kr.md)

[プロジェクト履歴](docs/Project_History_ja.md) · [1.1.0 パッチノート](PATCH_NOTES_1.1.0.md)

## このプロジェクトの目的

PLC学習は、実機へのアクセス、時間、コストの制約を受けやすいです。  
本プロジェクトは次を目指しています。

- 配線 + プログラミング + シミュレーションを1つの環境で提供
- 実際のPLC学習フローに近い操作体験を提供
- デスクトップ上で高速な反復学習を可能にする
- PLCロジックとRTLハードウェア設計を1つのツールで統合

## 重視している点

- 実用ワークフロー: 配線構築、ラダー作成、動作検証、RTLモジュール設計を1か所で実行
- 拡張性: components/physics/programming/RTL のモジュール分離アーキテクチャ
- 性能: LOD、ビューポートカリング、空間分割、リソースキャッシュ最適化
- 現実性: OpenPLCベースの変換/実行パス + Box2D統合

## 主な機能

### 配線モード

- ドラッグ＆ドロップによるコンポーネント配置が可能なインタラクティブキャンバス
- 自動配線ルーティングと接続管理
- 重複回避とマルチアロー統合対応のスマートタグラベル
- PLC I/O、空圧（FRL、シリンダー、ソレノイドバルブ、メーターバルブ）、スイッチ、センサー、リレーなどのコンポーネントライブラリ
- シンク/ソース配線モード変換
- ジェスチャー＆タッチ対応: ピンチズーム、2本指パン、FRL/メーターバルブ回転操作

### ラダープログラミングモード

- GX Developerスタイルのラダーエディタ
- 命令サポート: `XIC`, `XIO`, `OTE`, `SET`, `RST`, `TON`, `CTU`, `RST_TMR_CTR`, `BKRST`
- ブロック選択、コピー/ペースト、ドラッグベースのラング並べ替え
- `F9` / `Shift+F9` で縦分岐（OR）編集
- OpenPLC互換LD変換・コンパイルパイプライン
- GX2 CSVラダープログラム保存/読み込み

### モニターモード

- リアルタイムI/O、タイマー、カウンター状態確認
- アクティブなコンパイル済みラダーに同期した現在値とプリセット表示

### RTLモード *(1.1.0で新規追加)*

- シンタックスハイライト付き統合Verilog RTLエディタ
- RTLモジュール管理: RTLライブラリでのモジュール作成、リネーム、削除、整理
- 分析、ビルド、テストベンチワークフローと専用ログパネル
- RTLツールチェーン管理: verilatorのワンクリックセットアップ、検証、削除
- `.plccomp` コンポーネントパッケージ: Verilogソース付きまたはランタイム専用バンドルとして再利用可能なRTLモジュールのエクスポート/インポート
- RTLランタイムアーティファクトが `.plcproj` プロジェクトパッケージにバンドルされ、クロスマシン移植に対応

### シミュレーションエンジン

- Mitsubishi FX3U-32MベースのPLCデザイン
- 自作(in-house)マルチドメイン物理エンジン（電気 / 空圧 / 機械）
- 衝突とワークピース相互作用のためのBox2D統合
- リミットスイッチ、ボタンユニット、緊急停止の状態統合用コンポーネント入力リゾルバ

### プロジェクトワークフロー

- 配線、ラダー、RTLデータを含む統合プロジェクトパッケージ形式（`.plcproj`）
- 配線モードとプログラミングモードで `Ctrl+S` クイック保存
- NSISインストーラーにアンインストール時のRTLツールチェーンクリーンアップオプション付き
- 1.0.xプロジェクトファイルとの下位互換性

### ローカライゼーションとヘルプ

- 多言語UI: 韓国語、英語、日本語（`resources/lang`）
- 配線、プログラミング、RTL、ショートカット、カメラ操作をカバーする内蔵ヘルプ

## 前提条件

- CPU: 4スレッド以上
- RAM: 最低 2GB
- CMake >= 3.20
- C++20 コンパイラ
- OpenGL ランタイム
- Git（FetchContent依存関係の取得用）
- *(オプション)* 古いWindowsバージョンでのRTLツールチェーンスクリプト実行にPowerShell 7+

## サードパーティライブラリ

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG

## 技術スタック

| 領域 | スタック |
|---|---|
| 言語 | C++20 |
| ビルド | CMake, CPack (NSIS + ZIP) |
| レンダリング/UI | OpenGL, GLFW, GLAD, Dear ImGui |
| 物理 | 自作(in-house)マルチドメイン物理エンジン（電気/空圧/機械）+ Box2D |
| PLC/ラダー | OpenPLC互換 LD 変換/実行パイプライン |
| RTL | verilator（外部ツールチェーン、アプリから管理） |
| データ/入出力 | nlohmann/json, miniz, XML serializer |

このプロジェクトは汎用の外部ゲームエンジンではなく、自作シミュレーションエンジンをコアとして使用しています。

## ビルド

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 実行

```bash
# single-config generator
./build/bin/PLCSimulator

# multi-config (Visual Studio)
./build/bin/Release/PLCSimulator.exe
```

## テスト

```bash
cmake -S . -B build-test -DBUILD_TESTING=ON
cmake --build build-test --target plc_tests
ctest --test-dir build-test --output-on-failure
```

## プロジェクト構成

```text
include/plc_emulator/   # パブリックヘッダー
src/                    # アクティブな実装コード
  application/          # アプリライフサイクル、入力、レンダリング、RTL UI
  programming/          # ラダーエディタ、コンパイラ、PLCエグゼキュータ
  wiring/               # 配線キャンバス、コンポーネント、物理同期
  rtl/                  # RTLプロジェクト/ライブラリ/ランタイム管理
  project/              # プロジェクトファイル保存/読み込み、GX2 CSV
resources/              # フォント、i18n言語ファイル、アセット
tools/                  # RTLツールチェーン セットアップ/検証/削除スクリプト
tests/                  # ユニット/統合テスト
legacy/                 # 旧MVPコード（参照用のみ、ビルド対象外）
```

## キーボードショートカット

| ショートカット | 動作 |
|---|---|
| `F2` | モニターモード切替 |
| `F5` / `F6` / `F7` | XIC / XIO / Coil |
| `F9` | 縦ライン追加 |
| `Shift+F9` | 縦ライン削除 |
| `Ctrl+Z` / `Ctrl+Y` | 元に戻す / やり直し |
| `Ctrl+S` | プロジェクト保存 |
| `Ctrl+C` / `Ctrl+V` | 選択コピー / ペースト |
| `Delete` | 選択ラングブロック削除 |

キャンバスナビゲーション: マウスホイールズーム、ミドルドラッグパン、`Alt`+右ドラッグパン、トラックパッドピンチ/スクロール、タッチピンチズーム＆ドラッグパン

（ショートカット文字列は `resources/lang/*.lang` に定義されています。）

## ステータス

追加コンポーネントやバグ修正のPull Requestを受け付けています。開発は段階的に進行中です。

## ライセンス

GPL-3.0
