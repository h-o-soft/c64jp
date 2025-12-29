# Oscar64 日本語表示サンプル

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Oscar64コンパイラで日本語表示を行う基本サンプルプログラムです。

## 概要

このサンプルは、MagicDeskカートリッジ（`c64jpkanji.crt`）と組み合わせて使用することで、Commodore 64のテキストモードで日本語を表示します。共有ライブラリ `oscar64_lib/` の基本的な使用例を示しています。

## 機能

- テキストモードでの日本語表示
- Shift-JIS文字列の出力
- 色設定（文字色・背景色・ボーダー色）

## 注意事項

### テキストモードの文字数制限

テキストモードは初期状態で64文字の定義（表示）を行うようになっています。そのため、本プログラムの「Looping forever...」という表示は「Looping foreve」と途中で文字数が足りなくなり、表示されなくなります。

全ての文字を表示するには、`jtxt_set_range()`関数で定義範囲を変更する必要があります：

```c
// 128文字分の定義領域を確保（デフォルトは64文字）
jtxt_set_range(128, 128);
```

## ファイル構成

```
oscar64/
├── Makefile           # ビルド設定
└── src/
    └── main.c         # メインプログラム
```

ライブラリファイルは `../oscar64_lib/` から参照しています。

## ビルド方法

```bash
# このディレクトリでビルド
make

# または、ルートディレクトリから
cd ../..
make oscar-build
```

## 実行方法

MagicDeskカートリッジが必要です。ルートディレクトリから実行してください：

```bash
cd ../..
make oscar-hello
```

単体で実行する場合は、カートリッジを別途指定：

```bash
x64sc -cartcrt ../../crt/c64jpkanji.crt -autostart hello.prg
```

## 必要要件

- Oscar64コンパイラ
- MagicDeskカートリッジ（`c64jpkanji.crt`）
- VICEエミュレータまたは実機

## サンプルコード

```c
#include "jtxt.h"

int main(void) {
    // テキストモードで初期化
    jtxt_init(JTXT_TEXT_MODE);

    // 画面クリア
    jtxt_cls();

    // 日本語テキストを表示
    jtxt_locate(5, 8);
    jtxt_puts("こんにちは！");

    while (1) {
        // メインループ
    }
    return 0;
}
```

## 関連プロジェクト

- `../oscar64_lib/` - 共有jtxtライブラリ
- `../oscar64_qe/` - QEテキストエディタ（IME付き）
- `../oscar64_crt/` - EasyFlash版（カートリッジ単体）

## ライセンス

MITライセンス
