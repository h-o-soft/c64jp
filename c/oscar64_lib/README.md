# Oscar64 日本語表示ライブラリ (jtxt)

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Oscar64コンパイラ用の共有ライブラリです。日本語表示（jtxt）、かな漢字変換（IME）、Ultimate II+ネットワーク通信（c64u）の機能を提供します。

## 概要

このライブラリは `c/oscar64/`、`c/oscar64_qe/`、`c/oscar64_crt/`、`c/oscar64_term/` から共有で使用されます。MagicDesk形式のカートリッジ（`c64jpkanji.crt`）と組み合わせて使用することで、日本語表示・入力機能を提供します。

## 特徴

### jtxt（日本語表示）
- **テキストモード**: PCG（Programmable Character Generator）による高速日本語表示
- **ビットマップモード**: 320x200ピクセルのビットマップ画面での日本語表示
- **Shift-JIS対応**: 標準的な文字コードでテキストを扱える
- **文字列リソース**: カートリッジに格納された定型文字列の読み込み

### IME（かな漢字変換）
- ローマ字→ひらがな→漢字の単文節変換
- ROMカートリッジ上の辞書を使用した高速検索
- 動詞活用対応（送りあり変換）
- 学習機能（候補選択頻度記録）

### c64u（Ultimate II+ネットワーク通信）
- Ultimate II+カートリッジのネットワーク機能を利用したTCP/IP通信
- ソケットの作成・接続・送受信・切断
- PETSCII/ASCII文字コード変換

## ファイル構成

```
oscar64_lib/
├── include/
│   ├── jtxt.h           # 日本語表示ヘッダ
│   ├── ime.h            # かな漢字変換ヘッダ
│   ├── c64u_network.h   # Ultimate II+ネットワーク通信ヘッダ
│   └── c64_oscar.h      # Oscar64固有の定義
└── src/
    ├── jtxt.c           # コアライブラリ
    ├── jtxt_bitmap.c    # ビットマップモード機能
    ├── jtxt_charset.c   # 文字定義機能
    ├── jtxt_resource.c  # 文字列リソース機能
    ├── jtxt_text.c      # テキストモード機能
    ├── ime.c            # かな漢字変換
    └── c64u_network.c   # Ultimate II+ネットワーク通信
```

## API一覧

### 初期化・終了

| 関数 | 説明 |
|------|------|
| `jtxt_init(mode)` | ライブラリ初期化（TEXT_MODE / BITMAP_MODE） |
| `jtxt_cleanup()` | 終了処理 |
| `jtxt_set_mode(mode)` | 表示モード切り替え |
| `jtxt_set_range(start, count)` | PCG使用範囲設定 |

### テキストモード

| 関数 | 説明 |
|------|------|
| `jtxt_cls()` | 画面クリア |
| `jtxt_locate(x, y)` | カーソル位置設定 |
| `jtxt_putc(c)` | 1文字出力（Shift-JIS） |
| `jtxt_puts(str)` | 文字列出力 |
| `jtxt_newline()` | 改行 |
| `jtxt_set_color(color)` | 文字色設定 |
| `jtxt_set_bgcolor(bg, border)` | 背景色・ボーダー色設定 |

### ビットマップモード

| 関数 | 説明 |
|------|------|
| `jtxt_bcls()` | 画面クリア |
| `jtxt_blocate(x, y)` | カーソル位置設定 |
| `jtxt_bputc(c)` | 1文字出力 |
| `jtxt_bputs(str)` | 文字列出力 |
| `jtxt_bnewline()` | 改行 |
| `jtxt_bbackspace()` | バックスペース |
| `jtxt_bcolor(fg, bg)` | 前景色・背景色設定 |
| `jtxt_bwindow(top, bottom)` | 表示ウィンドウ設定 |
| `jtxt_bscroll_up()` | 上スクロール |

### 文字列リソース

| 関数 | 説明 |
|------|------|
| `jtxt_putr(id)` | リソース文字列をテキストモードで出力 |
| `jtxt_bputr(id)` | リソース文字列をビットマップモードで出力 |

## 使用例

```c
#include "jtxt.h"

int main(void) {
    // テキストモードで初期化
    jtxt_init(JTXT_TEXT_MODE);

    // 画面クリア
    jtxt_cls();

    // 日本語文字列を表示
    jtxt_puts("こんにちは世界！");

    // 位置を指定して表示
    jtxt_locate(5, 10);
    jtxt_set_color(2);  // 赤色
    jtxt_puts("Commodore 64で日本語");

    // 終了処理
    jtxt_cleanup();
    return 0;
}
```

## 必要要件

- **Oscar64コンパイラ**: https://github.com/drmortalwombat/oscar64
- **MagicDeskカートリッジ**: `c64jpkanji.crt`（ルートディレクトリで `make crt` で生成）

## ビルド方法

このライブラリは直接ビルドするものではなく、他のプロジェクト（`oscar64/`、`oscar64_qe/`、`oscar64_term/`等）から参照して使用します。

```bash
# oscar64/から使用する場合
cd ../oscar64
make

# oscar64_qe/から使用する場合
cd ../oscar64_qe
make

# oscar64_term/から使用する場合
cd ../oscar64_term
make
```

## メモリマップ

| アドレス | 用途 |
|----------|------|
| $0340-$03FF | 文字列バッファ（192バイト） |
| $0400-$07FF | テキスト画面RAM |
| $3000-$37FF | PCG用キャラクタRAM |
| $5C00-$5FFF | ビットマップ用画面RAM |
| $6000-$7FFF | ビットマップデータ |
| $8000-$9FFF | MagicDeskバンク領域 |
| $DE00 | MagicDeskバンクレジスタ |

## 関連プロジェクト

- `../oscar64/` - 基本サンプル（Hello World）
- `../oscar64_qe/` - QEテキストエディタ（IME付き）
- `../oscar64_crt/` - EasyFlash版（カートリッジ単体動作）
- `../oscar64_term/` - ターミナル（Ultimate II+ネットワーク、Telnet、XMODEM）

## ライセンス

MITライセンス
