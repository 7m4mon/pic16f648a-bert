# PIC BERT（Bit Error Rate Tester）

PICマイコン（PIC16F648A）と CCS C Compiler 4.0 を用いて作成した  
**Bit Error Rate Tester（BERT）** です。

外部から入力されるクロックおよびデータ信号に対して、  
内部で生成した PN 系列（LFSR）と比較を行い、  
ビット誤り率（BER）を測定します。

![Image](https://github.com/user-attachments/assets/dc1e8af1-bd4c-4652-b41c-bedc5b5b7750)  
![Image](https://github.com/user-attachments/assets/2b11b135-4628-43e4-9b0d-6511e80ad99b)

---

## 特徴

- PIC16F648A 単体で動作するスタンドアロン BERT
- 外部クロック／外部データ入力方式
- PN 系列（LFSR）による期待値生成
- 自動同期（極性・位相合わせ）
- 同期完了後に誤り数をカウント
- 測定結果を 1602 キャラクタ LCD に表示
- 測定ビット数をボタンで切替可能
- 設定を内蔵 EEPROM に保存
- コンパイル済み HEX ファイル同梱

---

## 使用ハードウェア

| 項目 | 内容 |
|----|----|
| MCU | PIC16F648A |
| コンパイラ | CCS C Compiler v4.0 |
| クロック | 20MHz セラロック |
| 表示器 | 1602（16x2）キャラクタ LCD |
| 入力 | 押しボタン ×2、クロック入力、データ入力 |
| 出力 | 同期表示 LED |

---

## ピン配置

| 信号名 | PIC ピン | 説明 |
|------|--------|------|
| SW_SEL | RA0 | 測定開始／選択ボタン |
| SW_TRIG | RA1 | 設定変更ボタン |
| SYNC_LED | RA3 | 同期状態表示 LED |
| CLK_IN | RA4 | 外部クロック入力 |
| DATA_IN | RA5 | 外部データ入力 |

LCD は CCS 付属の `lcd_b.c` を用い、PORTB に接続します。

---

## 操作方法

### 電源投入時

- 通常起動  
  - EEPROM に保存された設定を読み込みます
- **SW_TRIG を押したまま電源 ON**  
  - データ極性（DataNeg）を反転
- **SW_SEL を押したまま電源 ON**  
  - クロック極性（ClockNeg）を反転


---

## EEPROM 設定内容

| アドレス | 内容 |
|--------|------|
| 0 | クロック極性フラグ |
| 1 | データ極性フラグ |
| 2 | 測定ビット数インデックス |
| 3 | 同期しきい値 |

---

## ファイル構成

| ファイル | 内容 |
|--------|------|
| `BERT.C` | ソースコード（コメント付き） |
| `BERT.hex` | コンパイル済み HEX ファイル |
| `README.md` | 本ドキュメント |

---

## ビルドについて

- CCS C Compiler v4.0 を使用
- 対象デバイス：PIC16F648A
- 20MHz セラロック使用
- EEPROM を設定保存に使用

※ HEX ファイルを使用する場合、再コンパイルは不要です。

---

## ライセンス

MIT License  

---

## 作者

- **7M4MON**
- 作成年：2008年9月

