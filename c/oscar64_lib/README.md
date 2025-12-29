# Oscar64 日本語表示ライブラリ (jtxt)

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Oscar64コンパイラ用の日本語表示ライブラリです。MagicDeskカートリッジに格納されたフォントデータを使用して、Commodore 64で日本語テキストを表示します。

## 概要

このライブラリは `c/oscar64/` および `c/oscar64_qe/` から共有で使用されます。MagicDesk形式のカートリッジ（`c64jpkanji.crt`）と組み合わせて使用することで、日本語表示機能を提供します。

## 特徴

- **テキストモード**: PCG（Programmable Character Generator）による高速日本語表示
- **ビットマップモード**: 320x200ピクセルのビットマップ画面での日本語表示
- **Shift-JIS対応**: 標準的な文字コードでテキストを扱える
- **文字列リソース**: カートリッジに格納された定型文字列の読み込み

## ファイル構成

```
oscar64_lib/
├── include/
│   ├── jtxt.h         # メインヘッダ
│   └── c64_oscar.h    # Oscar64固有の定義
└── src/
    ├── jtxt.c         # コアライブラリ
    ├── jtxt_bitmap.c  # ビットマップモード機能
    ├── jtxt_charset.c # 文字定義機能
    ├── jtxt_resource.c # 文字列リソース機能
    └── jtxt_text.c    # テキストモード機能
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

このライブラリは直接ビルドするものではなく、他のプロジェクト（`oscar64/`、`oscar64_qe/`）から参照して使用します。

```bash
# oscar64/から使用する場合
cd ../oscar64
make

# oscar64_qe/から使用する場合
cd ../oscar64_qe
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

## ライセンス

MITライセンス
