# jtxt.p8 - 日本語テキスト表示ライブラリ

| [English](jtxt-en.md) | [日本語](jtxt.md) |
|---------------------------|----------------------|

MagicDesk漢字ROMカートリッジを使用したC64用日本語テキスト表示ライブラリです。

## 概要

jtxt.p8は、Commodore 64で日本語文字（漢字・ひらがな・カタカナ）を表示するためのライブラリです。MagicDesk形式のROMカートリッジに格納された美咲フォント（8x8ピクセル）を使用し、PCG（Programmable Character Generator）による高速描画を実現しています。

## 主な機能

- **2つの表示モード対応**
  - テキストモード: PCGを使用した高速文字表示
  - ビットマップモード: より柔軟な画面制御が可能
  
- **Shift-JIS文字コード対応**
  - 全角文字（JIS X 0208）
  - 半角カナ（JIS X 0201）
  - ASCII文字
  
- **フォントデータアクセス**
  - MagicDeskバンク切り替えによる大容量フォントアクセス
  - 美咲ゴシック/美咲明朝フォント対応

- **文字列リソース機能**
  - CRTファイル内の文字列リソースを読み込み可能
  - 固定文字列の効率的な管理

## 定数定義

### メモリアドレス
```prog8
const uword ROM_BASE = $8000        ; ROM開始アドレス
const uword BANK_REG = $DE00        ; MagicDeskバンク切り替えレジスタ
const uword CHARSET_RAM = $3000     ; RAMキャラクタセット（テキストモード）
const uword BITMAP_BASE = $6000     ; ビットマップメモリ開始
const uword SCREEN_RAM = $0400      ; テキストモード画面RAM
const uword BITMAP_SCREEN_RAM = $5C00  ; ビットマップモード画面RAM
```

### 表示モード
```prog8
const ubyte TEXT_MODE = 0           ; テキストモード
const ubyte BITMAP_MODE = 1         ; ビットマップモード
```

### 文字列リソース
```prog8
const ubyte STRING_RESOURCE_BANK = 36  ; 文字列リソース開始バンク
const uword STRING_BUFFER = $0340      ; 文字列バッファ（192バイト）
```

## 主要関数

### 初期化・設定

#### init(start_char, char_count, mode)
ライブラリを初期化し、使用する文字範囲と表示モードを設定します。
- `start_char`: 使用開始文字コード（128-255の範囲）
- `char_count`: 使用可能文字数（最大127）
- `mode`: 表示モード（TEXT_MODE または BITMAP_MODE）

```prog8
jtxt.init(128, 64, jtxt.TEXT_MODE)  ; テキストモードで初期化
```

#### set_mode(mode)
表示モードを切り替えます。
- `mode`: TEXT_MODE または BITMAP_MODE

#### set_range(start_char, char_count)
使用する文字範囲を変更します。

### テキストモード関数

#### cls()
画面をクリアします。

#### locate(x, y)
カーソル位置を設定します。
- `x`: X座標（0-39）
- `y`: Y座標（0-24）

#### putc(char_code)
1文字を出力します。Shift-JISの2バイト文字にも対応。
- `char_code`: 文字コード

#### puts(addr)
NULL終端文字列を出力します。
- `addr`: 文字列のアドレス

```prog8
ubyte[] message = [$82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD, $00]  ; "こんにちは"
jtxt.puts(&message)
```

#### newline()
改行します。

#### set_color(color)
文字色を設定します（0-15）。

#### set_bgcolor(color)
背景色を設定します（0-15）。

#### set_bordercolor(color)
ボーダー色を設定します（0-15）。

### ビットマップモード関数

#### bcls()
ビットマップ画面をクリアします。

#### blocate(x, y)
ビットマップモードでのカーソル位置を設定します。
- `x`: X座標（文字単位、0-39）
- `y`: Y座標（文字単位、0-24）

#### bcolor(fg, bg)
ビットマップモードでの前景色・背景色を設定します。
- `fg`: 前景色（0-15）
- `bg`: 背景色（0-15）

#### bputc(char_code)
ビットマップモードで1文字を出力します。

#### bputs(addr)
ビットマップモードでNULL終端文字列を出力します。

#### bwindow(top, bottom)
ビットマップモードでの描画範囲を制限します。
- `top`: 開始行（0-24）
- `bottom`: 終了行（0-24）

#### bwindow_enable() / bwindow_disable()
行範囲制御の有効/無効を切り替えます。

#### bnewline()
ビットマップモードで改行します。

#### bscroll_up()
ビットマップ画面を1行上にスクロールします。

### 文字列リソース関数

#### load_string_resource(index) -> uword
指定インデックスの文字列リソースを読み込みます。
- `index`: リソースインデックス
- 戻り値: 文字列バッファのアドレス（$0340）

```prog8
uword message = jtxt.load_string_resource(0)
jtxt.puts(message)  ; リソース0の文字列を表示
```

#### putr(index)
文字列リソースを直接出力します（テキストモード）。

#### bputr(index)  
文字列リソースを直接出力します（ビットマップモード）。

### ユーティリティ関数

#### is_firstsjis(code) -> bool
指定コードがShift-JISの第1バイトかを判定します。

#### bput_hex2(value)
2桁の16進数を表示します（ビットマップモード）。

#### bput_dec2(value) / bput_dec3(value)
2桁/3桁の10進数を表示します（ビットマップモード）。

## 使用例

### 基本的な日本語表示
```prog8
%import jtxt

main {
    sub start() {
        ; 初期化
        jtxt.init(128, 64, jtxt.TEXT_MODE)
        
        ; 画面設定
        jtxt.cls()
        jtxt.set_bgcolor(0)  ; 黒背景
        jtxt.set_color(1)    ; 白文字
        
        ; 日本語メッセージ表示
        ubyte[] message = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF, $82, $CD,  ; "こんにちは"
            $00
        ]
        jtxt.locate(10, 10)
        jtxt.puts(&message)
    }
}
```

### ビットマップモードでの表示
```prog8
%import jtxt

main {
    sub start() {
        ; ビットマップモードで初期化
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        ; 画面クリア
        jtxt.bcls()
        
        ; 色設定
        jtxt.bcolor(1, 0)  ; 白文字、黒背景
        
        ; 日本語表示
        jtxt.blocate(5, 5)
        jtxt.bputs(iso:"HELLO ")
        jtxt.bputs(&japanese_text)
    }
}
```

## メモリ使用

- **$0340-$03FF**: 文字列バッファ（192バイト）
- **$3000-$3800**: PCG用キャラクタRAM（テキストモード時、2KB）
- **$5C00-$5FFF**: ビットマップ用画面RAM（1KB）
- **$6000-$7FFF**: ビットマップデータ（8KB）
- **$8000-$9FFF**: MagicDeskバンク切り替え領域（8KB）

## 注意事項

1. **文字範囲の制限**
   - 同時に表示できる文字数は最大127文字（PCGの制限）
   - 多くの異なる文字を表示する場合は適切な範囲設定が必要

2. **バンク切り替えの影響**
   - MagicDeskのバンク切り替えは$DE00を使用
   - 他のI/Oデバイスとの競合に注意

3. **Shift-JISエンコーディング**
   - 日本語文字列は必ずShift-JISでエンコード
   - ASCII文字列は`iso:`プレフィックスを使用

4. **メモリ配置**
   - ビットマップモード使用時は$6000-$7FFFが占有される
   - テキストモード使用時は$3000-$37FFが占有される