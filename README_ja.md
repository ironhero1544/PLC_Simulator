# PLC_Simulator
![plcsimulator_lockup_whitebg.svg](docs_img/plcsimulator_lockup_whitebg.svg)

物理シミュレーションを統合し、OpenPLC互換のラダー実行パスを備えた、リアルタイムPLC配線/プログラミングシミュレーターです。

[English](README.md), [한국어](README_kr.md)

[プロジェクト履歴](docs/Project_History_ja.md)

## このプロジェクトの目的

PLC学習は、実機へのアクセス、時間、コストの制約を受けやすいです。  
本プロジェクトは次を目指しています。

- 配線 + プログラミング + シミュレーションを1つの環境で提供
- 実際のPLC学習フローに近い操作体験を提供
- デスクトップ上で高速な反復学習を可能にする

## 重視している点

- 実用ワークフロー: 配線構築、ラダー作成、動作検証を1か所で実行
- 拡張性: components/physics/programming のモジュール分離アーキテクチャ
- 性能: LOD、ビューポートカリング、空間分割による最適化
- 現実性: OpenPLCベースの変換/実行パス + Box2D統合

## 主な機能

- Wiring mode
- Ladder programming mode
- Monitor mode（I/O、タイマー、カウンターの確認）
- PLCデザインは Mitsubishi FX3U-32M をベースにしています
- 命令サポート: `XIC`, `XIO`, `OTE`, `SET`, `RST`, `TON`, `CTU`, `RST_TMR_CTR`, `BKRST`
- OpenPLC互換のLD変換/コンパイルパイプライン
- 物理シミュレーション（電気 / 空圧 / 機械 + ワークピース相互作用）
- 衝突/相互作用のためのBox2D統合
- 多言語リソース（`resources/lang`: ko/en/ja）
- プロジェクト保存/読み込み

## 前提条件

- CPU: 4スレッド以上
- RAM: 最低 2GB
- CMake >= 3.20
- C++20 コンパイラ
- OpenGL ランタイム
- Git（FetchContent依存関係の取得用）

## サードパーティライブラリ

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG

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
include/plc_emulator/   # public headers
src/                    # active implementation
resources/              # fonts, i18n, assets
tests/                  # unit/integration tests
legacy/                 # old MVP code (reference only, not built)
```

## 使用メモ

- `F2`: monitor mode
- `F5/F6/F7`: XIC/XIO/Coil
- `F9`, `Shift+F9`: 縦ラインの追加/削除
- `Ctrl+Z`, `Ctrl+Y`: 元に戻す/やり直し

（ショートカット文字列は `resources/lang/*.lang` に定義されています。）

## ステータス

追加コンポーネントやバグ修正のPull Requestを受け付けています。開発は段階的に進行中です。

## ライセンス

GPL-3.0
