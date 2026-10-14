# PLC Simulator

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Windows](https://img.shields.io/badge/Platform-Windows-lightgrey)

![PLC Simulator](docs_img/plcsimulator_lockup_whitebg.svg)

物理シミュレーション、OpenPLC互換ラダー実行パス、RTLハードウェア設計ワークフローを統合した、リアルタイムPLC配線/プログラミングシミュレーターです。

[English](README.md) · [한국어](README_kr.md)

[プロジェクト履歴](docs/Project_History_ja.md)

## このプロジェクトの目的

PLC学習は、実機へのアクセス、時間、コストの制約を受けやすいです。
本プロジェクトは次を目指しています。

- 配線 + プログラミング + シミュレーションを1つの環境で提供
- 実際のPLC学習フローに近い操作体験を提供
- デスクトップ上で高速な反復学習を可能にする
- PLCロジックとRTLハードウェア設計を1つのツールで統合

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
- 命令サポート: `LD`, `LDI`, `OUT`, `SET`, `RST`, `MOV`, `BKRST`
- ブロック選択、コピー/ペースト、ドラッグベースのラング並べ替え
- `F9` の水平線 / `Shift+F9` の垂直接続で直列・並列回路を編集
- OpenPLC互換LD変換・コンパイルパイプライン
- GX2 CSVラダープログラム保存/読み込み

### モニターモード

- リアルタイムI/O、タイマー、カウンター状態確認
- アクティブなコンパイル済みラダーに同期した現在値とプリセット表示

### RTLモード

- シンタックスハイライト付き統合Verilog RTLエディタ
- RTLモジュール管理: RTLライブラリでのモジュール作成、リネーム、削除、整理
- 分析、ビルド、テストベンチワークフローと専用ログパネル
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
- NSISインストーラー: アンインストール時も共有WSL環境を保持

### ローカライゼーションとヘルプ

- 多言語UI: 韓国語、英語、日本語（`resources/lang`）
- 配線、プログラミング、RTL、ショートカット、カメラ操作をカバーする内蔵ヘルプ

## 2.0 の変更点

### 自動化設備部品の追加

- センサー内蔵の複動・吸着シリンダー、ストッパー、サーボリフトと一体型部品
- DC モータードライバー、サーボモーター・アンプ、光ファイバーセンサーと送受信ヘッド
- 6 スロット・駆動倉庫、ワーク検出・重力供給・ボックス内整列
- 平面・正面表示と自動化設備の資格別部品フィルター

![追加された自動化設備部品](docs/design_assets/native_components_f_preview.png)

### プログラミングモードの改善

- 命令範囲に応じたセル選択、行・セルのコピー／貼り付けと分岐編集の改善
- パルス・比較接点とモニターの接点・コイル状態表示の改善
- タイマー・カウンターの現在値と D アドレス指定の設定値表示の改善
- 作成／モニター切り替えと再コンパイル時に残っていた出力状態を初期化

![改善されたラダーモニターと選択表示](docs/images/program-guide/07-monitor-selection.png)

![タイマーの現在値と設定値](docs/images/program-guide/monitor-timer-values.png)

### HMI の追加

- 通信配線なしで PLC に接続する HMI
- ボタン・表示灯・数値表示・設定値入力・メッセージの画面編集
- 複数画面、画面切り替えボタンとサイズ変更可能な 4:3 操作ウィンドウ
- 画面編集中は一時停止し、閉じると編集前の運転状態を復元

![HMI 画面エディター](docs/images/exam03-guide/09-hmi-editor.png)

### Z・L レイヤーの追加

- **Z**：物理的な高さと移動範囲、異なる高さでの相互作用
- **L**：表示順・グループ・透明度・非表示、複数選択とドラッグ
- 物理的な高さと描画順序を独立して保存

![物理 Z レイヤー設定](docs/images/program-guide/03-z-layer.png)

![L レイヤーの表示順とグループ](docs/images/program-guide/04-render-layer.png)

### 配線ツールの改善

- ピンの種類から電気・空圧・光接続を決定する統合配線ツール
- 部品名の編集とキャンバス上の名前表示
- センサー内蔵部品・モータードライバー・倉庫の物理動作と配線連動

![統合配線ツールと経由点](docs/images/program-guide/wiring-route.gif)

### 操作ガイドの追加

- 部品配置・配線・ラダー・モニター・HMI・レイヤー・RTL の練習ガイド
- Ctrl+? または Ctrl+, のヘルプから開く内蔵 Markdown ビューアー
- オフラインで画像・GIF・表・コード・数式・図を表示
- 目次・検索・履歴・画像拡大・ウィンドウサイズ変更

![内蔵操作ガイド](docs/images/program-guide/user-guide-viewer.png)

### RTL ツールチェーンの改善

- 解析・ビルド・テストベンチを WSL2 ベースへ移行
- MinGW-w64 でビルドした部品は Windows ネイティブ EXE として実行
- Tool Doctor の環境選択・診断・導入・起動／停止・WSL2 変換・更新
- ビルド済み RTL 部品は WSL のインストールなしで使用可能

![WSL2 RTL Tool Doctor](docs/images/program-guide/06-rtl-doctor.png)


## 重視している点

- **一連の実習**：部品配置・配線からラダー作成・運転・状態確認まで、1つのアプリケーションで進めます。
- **自動化設備の練習**：センサー・シリンダー・モーター・ワークの相互作用を見ながら制御順序を学びます。
- **修正と確認**：配線やプログラムを変更し、動作を繰り返し確認して問題の原因を探します。
- **部品の拡張**：標準部品に加えて、自作の RTL 回路を部品として登録して使用できます。

## はじめに

1. [リリース](https://github.com/ironhero1544/PLC_Simulator/releases)で配布形式を選びます。
2. `PLCSimulator.exe` を起動し、**配線**モードで部品を配置します。
3. **配線ツール**で始点ピンと終点ピンを順に接続します。
4. **プログラミング → 作成 (F3)** でラダーを入力し、**コンパイル (F4)** します。
5. シミュレーションを開始し、**モニター (F2)** で入出力と回路状態を確認します。

初めて使う場合は [ボタンとランプの練習](docs/guides/program/05-first-practice.md)から進めてください。

| 配布形式 | 使用方法 |
| --- | --- |
| インストーラー EXE | セットアップウィザードでインストール |
| MSI | Windows Installer でインストール |
| Portable ZIP | 展開してフォルダー内の EXE を起動 |
| 単一 Portable EXE | ユーザーのローカルキャッシュに必要なファイルを展開して起動 |

## ヘルプと操作ガイド

**Ctrl+?** (`Ctrl+Shift+/`)、**Ctrl+,** または **Ctrl+/** でヘルプを開き、**操作ガイドを開く**を選びます。
内蔵ビューアーはオフラインで画像・GIF・表・コード・数式・図を表示します。
目次・検索・履歴・画像拡大・ウィンドウサイズ変更に対応し、PLC Simulator のウィンドウより前面に表示されます。
ガイド本文は韓国語です。

- [操作ガイド](docs/guides/program/README.md)
- [自動化設備の資格試験：公開課題 3](docs/guides/exam03/GUIDE.md)
- [開始・停止・タイマー・カウンターの基本回路](docs/guides/program/reference-basic-circuits.md)
- [Full Adder：RTL 部品の作成・配線・操作](docs/guides/program/rtl-full-adder.md)

## 実行環境

- 64 ビット Windows、OpenGL 3.3 対応のグラフィックスドライバー
- 内蔵ガイド：Microsoft Edge WebView2 Runtime
- RTL のビルド・テスト：WSL2、Linux 環境、Verilator 5.x 以降、g++、make、MinGW-w64 POSIX
- Tool Doctor の自動ツール導入は Ubuntu/Debian に対応

## サードパーティライブラリ

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG
- webview / Microsoft Edge WebView2
- marked / DOMPurify / highlight.js / KaTeX / Mermaid

## 技術スタック

| 領域 | スタック |
|---|---|
| 言語 | C++20 |
| ビルド | CMake, CPack (NSIS + ZIP + WiX MSI) |
| レンダリング/UI | OpenGL, GLFW, GLAD, Dear ImGui |
| 物理 | 自作(in-house)マルチドメイン物理エンジン（電気/空圧/機械）+ Box2D |
| PLC/ラダー | OpenPLC互換 LD 変換/実行パイプライン |
| RTL | verilator（外部ツールチェーン、アプリから管理） |
| データ/入出力 | nlohmann/json, miniz, XML serializer |

このプロジェクトは汎用の外部ゲームエンジンではなく、自作シミュレーションエンジンをコアとして使用しています。

## ソースからビルド

CMake 3.20 以降、C++20 コンパイラー (MSVC または MinGW)、Git、Python 3 が必要です。
MSVC は Developer PowerShell、MinGW はコンパイラーを PATH に登録したターミナルから実行します。

```powershell
git clone https://github.com/ironhero1544/PLC_Simulator.git
cd PLC_Simulator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

単一構成では `build/bin/PLCSimulator.exe`、Visual Studio の複数構成では `build/bin/Release/PLCSimulator.exe` が生成されます。
ガイド文書と画像は、ビルド時に実行ファイルの `resources/help` に含まれます。

### テスト

```powershell
cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-test --config Release
ctest --test-dir build-test -C Release --output-on-failure
```

### 配布ファイルの作成

CPack で NSIS インストーラー EXE と ZIP を生成します。MSI の作成には WiX 3.x が追加で必要です。
`scripts/BuildStandalone.ps1` は CPack ZIP から単一 Portable EXE を作成します。

## プロジェクト構成

```text
include/plc_emulator/   # パブリックヘッダー
src/                    # アクティブな実装コード
  application/          # アプリライフサイクル、入力、レンダリング、RTL UI
  programming/          # ラダーエディタ、コンパイラ、PLCエグゼキュータ
  wiring/               # 配線キャンバス、コンポーネント、物理同期
  rtl/                  # RTLプロジェクト/ライブラリ/ランタイム管理
  project/              # プロジェクトファイル保存/読み込み、GX2 CSV
docs/guides/            # Markdown guides
scripts/                # Guide packaging and release scripts
resources/              # フォント、i18n言語ファイル、アセット
tools/                  # 従来のネイティブRTLスクリプト（WSL2 Tool Doctorでは未使用）
tests/                  # ユニット/統合テスト
legacy/                 # 旧MVPコード（参照用のみ、ビルド対象外）
```

## キーボードショートカット

| ショートカット | 動作 |
| --- | --- |
| `Ctrl+? / Ctrl+, / Ctrl+/` | ヘルプ・操作ガイド |
| `Tab` | 配線 / プログラミング切り替え |
| `Ctrl+S` | プロジェクト保存 |
| `F2 / F3` | モニター / 作成 |
| `F4` | コンパイル (作成モード) |
| `F5 / F6 / F7` | A接点 / B接点 / コイル |
| `F8` | 応用命令 |
| `F9` | 水平線追加 |
| `Shift+F9` | 垂直接続追加 |
| `Ctrl+Z / Ctrl+Y` | 元に戻す / やり直し |
| `Ctrl+C / Ctrl+V` | コピー / 貼り付け |
| `Delete` | 選択削除 |

## ライセンスと貢献

[GPL-3.0](LICENSE)。部品追加や不具合修正の Pull Request を歓迎します。
C++20 アプリケーションは GLFW、Dear ImGui、OpenGL、Box2D などを使用しています。
内蔵ガイドの依存関係とライセンスは [ビューアー依存関係](third_party/webview/README.md)を参照してください。

## 制作者からの案内

本プロジェクトは REA（Reverse Engineer Anything）とは一切関係ありません。実習の練習を支援するため、GXworks2 と互換性のある CSV 構造および命令の UI・UX のみを参考にしており、実際の製品とはまったく異なる方式で動作します。制作者は本プログラムの営利目的での使用を控えることを希望し、教育および非営利目的での使用を推奨します。
