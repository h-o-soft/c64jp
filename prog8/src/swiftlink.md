# swiftlink.p8 - SwiftLink RS-232通信ライブラリ

| [English](swiftlink-en.md) | [日本語](swiftlink.md) |
|--------------------------------|---------------------------|

Commodore 64用のSwiftLink互換RS-232シリアル通信ライブラリです。6551 ACIAチップを制御します。

## 概要

swiftlink.p8は、SwiftLinkカートリッジやその互換品を使用してRS-232シリアル通信を行うためのライブラリです。NMI割り込み駆動による256バイトリングバッファを実装し、安定した通信を実現しています。

## 主な機能

- **6551 ACIA制御**
  - 各種ボーレート対応（実際は定数名の2倍速、最大38400bps）
  - 8N1、7E1などの通信パラメータ設定
  - ハードウェアフロー制御（RTS/CTS）

- **NMI割り込み駆動**
  - 受信データの自動バッファリング
  - データロスの防止

- **256バイトリングバッファ**
  - 効率的なデータ管理
  - バッファオーバーフロー検出

## I/Oアドレス設定

デフォルトでは`$DF00`を使用しますが、REUとの競合を避けるため変更可能です。

```prog8
const uword SWIFTLINK_BASE = $DF00  ; デフォルト
; REU併用時は $DF80 に変更推奨
```

### レジスタアドレス
- `$DF00`: データレジスタ (R/W)
- `$DF01`: ステータス/リセットレジスタ (R/W)
- `$DF02`: コマンドレジスタ (W)
- `$DF03`: コントロールレジスタ (W)

## 基本的な使い方

### 初期化と通信開始
```prog8
%import swiftlink

main {
    sub start() {
        ; 2400bps, 8N1で初期化
        swiftlink.init_simple(swiftlink.BAUD_2400)
        
        ; またはカスタム設定
        swiftlink.init(
            swiftlink.BAUD_9600, 
            swiftlink.DATA_8 | swiftlink.STOP_1 | swiftlink.PARITY_NONE
        )
    }
}
```

### データ送信
```prog8
; 1バイト送信
swiftlink.send_byte('A')

; 文字列送信
swiftlink.send_string(iso:"Hello, World!\r\n")

; Shift-JIS文字列送信（日本語）
ubyte[] message = [$82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD, $00]
swiftlink.send_string(&message)
```

### データ受信
```prog8
; 1バイト受信（ブロッキング）
ubyte data = swiftlink.receive_byte()

; 1バイト受信（ノンブロッキング）
if swiftlink.data_available() {
    ubyte data = swiftlink.get_byte()
}

; 文字列受信
ubyte[256] buffer
ubyte len = swiftlink.receive_string(&buffer, 255, 10)  ; 最大255バイト、タイムアウト10
```

## 主要関数

### 初期化・設定

#### init(baud_rate, config)
指定のボーレートと設定で初期化します。
- `baud_rate`: BAUD_* 定数のいずれか
- `config`: データビット、ストップビット、パリティの組み合わせ

#### init_simple(baud_rate)
8N1設定で簡易初期化します。
- `baud_rate`: BAUD_* 定数のいずれか

#### close()
通信を終了し、NMIハンドラを復元します。

### 送信関数

#### send_byte(data) -> bool
1バイト送信します。
- `data`: 送信データ
- 戻り値: true=成功、false=タイムアウト

#### send_string(str_ptr) -> bool
NULL終端文字列を送信します。
- `str_ptr`: 文字列のアドレス
- 戻り値: true=成功、false=エラー

### 受信関数

#### data_available() -> bool
受信データがあるかチェックします。

#### get_byte() -> ubyte
バッファから1バイト取得します（ノンブロッキング）。

#### receive_byte() -> ubyte
1バイト受信します（ブロッキング）。

#### receive_string(buffer, max_len, timeout) -> ubyte
文字列を受信します。
- `buffer`: 受信バッファ
- `max_len`: 最大受信長
- `timeout`: タイムアウト値（0=無限待機）
- 戻り値: 実際に受信したバイト数

### フロー制御

#### set_rts(enabled)
RTS信号を制御します。
- `enabled`: true=有効、false=無効

#### get_dcd() -> bool
キャリア検出状態を取得します。

#### get_dsr() -> bool
データセットレディ状態を取得します。

### エラー処理

#### get_last_error() -> ubyte
最後のエラーコードを取得します。

#### clear_error()
エラー状態をクリアします。

## ボーレート定数

**注意**: SwiftLink互換品では3.6864MHz水晶使用により、実際の通信速度は定数名の約2倍になります。

```prog8
const ubyte BAUD_300 = %00000110    ; 実際は約600bps
const ubyte BAUD_600 = %00000111    ; 実際は約1200bps  
const ubyte BAUD_1200 = %00001000   ; 実際は約2400bps
const ubyte BAUD_2400 = %00001010   ; 実際は約4800bps
const ubyte BAUD_4800 = %00001100   ; 実際は約9600bps
const ubyte BAUD_9600 = %00001110   ; 実際は約19200bps
const ubyte BAUD_19200 = %00001111  ; 実際は約38400bps
```

## エラーコード

```prog8
const ubyte ERROR_NONE = 0
const ubyte ERROR_TIMEOUT = 1
const ubyte ERROR_PARITY = 2
const ubyte ERROR_FRAMING = 3
const ubyte ERROR_OVERRUN = 4
const ubyte ERROR_BUFFER_FULL = 5
```

## 使用例（modem_test.p8より）

```prog8
%import swiftlink
%import jtxt

main {
    sub start() {
        ; 初期化
        swiftlink.init_simple(swiftlink.BAUD_2400)
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        ; 日本語メッセージ送信
        ubyte[] hello = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD,  ; "こんにちは"
            $0D, $0A, $00  ; CRLF
        ]
        swiftlink.send_string(&hello)
        
        ; エコーサーバー
        repeat {
            if swiftlink.data_available() {
                ubyte ch = swiftlink.get_byte()
                jtxt.bputc(ch)  ; 画面表示
                swiftlink.send_byte(ch)  ; エコーバック
            }
        }
    }
}
```

## VICEエミュレータでの使用

VICEでRS-232エミュレーションを使用する場合：

```bash
make TARGET=modem_test run-modem
```

または手動で：
```bash
x64sc -rsdev2 "127.0.0.1:25232" -rsuserbaud "2400" \
      -rsdev2ip232 -rsuserdev "1" -userportdevice "2"
```

## 注意事項

1. **I/Oアドレス競合**
   - REU使用時: SwiftLinkを`$DF80`に移動
   - MagicDesk: `$DE00`を使用（競合なし）

2. **NMI割り込み**
   - 他のNMI使用コードとの競合に注意
   - close()で必ず元に戻すこと

3. **バッファサイズ**
   - 受信バッファは256バイト固定
   - 高速通信時はこまめにバッファをチェック

4. **実機での動作**
   - 実際のSwiftLinkカートリッジが必要
   - ヌルモデムケーブルでPCと接続可能