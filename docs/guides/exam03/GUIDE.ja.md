<a id="공개문제-3번-직접-제작-가이드"></a>

# 公開問題3：自分で作るガイド

**部品配置 → 電源配線 → 制御配線 → HMI・ラダー作成** の順に進めます。
各ページ下部の **次へ →** リンクで進んでください。

<a id="실습-시작"></a>

## 実習開始

| 順序 | 作業 | 到達点 |
| --- | --- | --- |
| 1 | [部品配置](01-placement.ja.md) | 部品39個、配線・HMI画面は空 |
| 2 | [電源配線](02-power.ja.md) | 電源・動力51本 |
| 3 | [制御配線](03-control.ja.md) | 制御・空圧・光65本 |
| 4 | [HMI画面作成](04-hmi.ja.md) | テスト動作／設備試運転の2画面 |
| 5 | [ラダー入力](05-ladder-basics.ja.md) | 接点・命令の入力方法を習得 |

[配置専用JSONをダウンロード](exam03-placement-only.json)

配線・HMI・プログラムはガイドを見て自分で作成します。

<a id="동작별-래더-작성"></a>

## 動作別のラダー作成

各ページで **動作の流れ → 論理図 → 実際のラダー → 入力方法 → 確認** を見ます。

1. [初期化](logic-01-init.ja.md)
2. [手動操作](logic-02-manual.ja.md)
3. [供給と加工](logic-03-process.ja.md)
4. [材質分岐](logic-04-material.ja.md)
5. [吸着と格納](logic-05-storage.ja.md)
6. [排出と終了](logic-06-finish.ja.md)
7. [一時停止と復帰](logic-07-pause.ja.md)

[ラダーの読み方](reference-ladder-reading.ja.md) · [コンパイルと試運転](06-checks.ja.md)

<a id="필요한-때만-보는-참고-자료"></a>

## 必要なときに読む参考資料

- [部品の座標・Z設定](reference-placement.ja.md)
- [アドレスの役割](reference-devices.ja.md)
- [段階・サーボ・復帰条件](reference-sequence.ja.md)
- [全回路の入力表と命令](reference-program.ja.md)
