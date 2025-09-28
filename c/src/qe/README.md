# QE - Text Editor for Commodore 64

QEは、Commodore 64用の日本語対応テキストエディタです。

## 概要

このエディタは、David GivenによるCP/M-65プロジェクトの[qe](https://github.com/davidgiven/cpm65/blob/master/apps/qe.c)を大幅に改造したものです。元のエディタはCP/M-65用に設計されていましたが、本バージョンではCommodore 64のネイティブ環境向けに以下の機能を追加しています：

### 主な機能

- **日本語表示対応**: Shift-JIS文字コードによる全角文字の表示
- **ビットマップモード**: jtxtライブラリによる高品質な日本語表示
- **かな漢字変換**: IMEによるローマ字入力からの日本語変換（オプション）
- **Viライクなキーバインディング**: 効率的なテキスト編集操作
- **ファイルI/O**: C64 KERNALルーチンによるファイルの読み書き

## ビルド方法

```bash
cd /path/to/qe
make                    # デフォルトビルド（IME有効）
make QE_ENABLE_IME=0    # IME無効でビルド
```

### ビルドオプション

- `QE_ENABLE_IME=1`: かな漢字変換機能を有効化（デフォルト）
- `ENABLE_FILE_IO`: ファイル入出力機能を有効化（デフォルトで有効）

## 実行方法

```bash
make run                # エディタを起動
```

MagicDesk形式のROMカートリッジ（日本語フォント・辞書データ）が必要です。
カートリッジファイルは `../../../crt/c64jpkanji.crt` に配置してください。

## エディタの基本概念

QEは**モーダルエディタ**です。一般的なエディタ（メモ帳など）と異なり、キーボード入力の動作が「モード」によって変わります。

### モードとは

- **ノーマルモード**（起動時のデフォルト）: カーソル移動、削除、コマンド実行などを行うモード。文字キーを押しても文字は入力されません。
- **インサートモード**: 文字入力を行うモード。キーを押すとその文字がテキストに挿入されます。
- **リプレースモード**: 既存の文字を上書きするモード。

この仕組みにより、キーボードのホームポジションを崩さずに素早く編集できます。例えば、ノーマルモードで`h` `j` `k` `l`キーがカーソル移動になるため、矢印キーに手を伸ばす必要がありません。

### 基本的な流れ

1. エディタ起動 → **ノーマルモード**
2. `i`キーを押す → **インサートモード**に切り替わり、文字入力開始
3. `ESC`キーを押す → **ノーマルモード**に戻る
4. `:`キーを押してコマンド入力（例：`:w`で保存、`:q`で終了）

## 基本操作

### ノーマルモード

| キー | 動作 |
|------|------|
| `i` | インサートモード開始 |
| `A` | 行末からインサートモード開始 |
| `o` | 下に新しい行を挿入してインサートモード |
| `O` | 上に新しい行を挿入してインサートモード |
| `h` `j` `k` `l` | カーソル移動（左・下・上・右） |
| `^` `$` | 行頭・行末に移動 |
| `G` | 指定行に移動 |
| `x` | 1文字削除 |
| `dd` | 行削除 |
| `d$` | カーソル位置から行末まで削除 |
| `J` | 次の行を連結 |
| `R` | リプレースモード |
| `:` | コロンコマンドモード |
| `Ctrl+R` | 画面再描画 |

### インサートモード / リプレースモード

| キー | 動作 |
|------|------|
| `ESC` | ノーマルモードに戻る |
| `Commodore + Space` | IME有効化（IME有効ビルドのみ） |
| `Backspace` | 1文字削除（全角対応） |
| 通常文字 | 文字入力 |

### コロンコマンド

| コマンド | 動作 |
|----------|------|
| `:w` | ファイル保存 |
| `:w filename` | 指定ファイル名で保存 |
| `:q` | 終了 |
| `:q!` | 強制終了（未保存でも終了） |
| `:e filename` | ファイルを開く |
| `:e!` | 再読み込み（変更を破棄） |
| `:n` | 新規ファイル |
| `:n!` | 強制的に新規ファイル（変更を破棄） |

## IME（かな漢字変換）

IME有効ビルドでは、インサートモード中に`Commodore + Space`でIMEを有効化できます。

### IME操作

- **ローマ字入力**: `a`, `ka`, `kya`などで入力
- **変換**: `Space`キーで変換候補を表示
- **候補選択**: `Space`で次候補、`Shift+Space`で前候補
- **確定**: `Enter`で確定
- **キャンセル**: `ESC`でIME無効化
- **モード切り替え**: `Ctrl+K`でひらがな/カタカナ切り替え

## 技術仕様

### メモリ使用

- エディタバッファ: 11KB ($A000-$CBFF)
- ビットマップ画面: $5C00-$7FFF
- テキスト領域: 40列 × 25行

### ファイルフォーマット

- エンコーディング: Shift-JIS
- 改行コード: CR ($0D)
- 最大ファイル名長: 64文字

### Shift-JIS対応

全角文字（2バイト文字）は以下のように処理されます：

- 第1バイト: 0x81-0x9F または 0xE0-0xFC
- 第2バイト: 任意のバイト
- カーソル移動、削除、置換などの操作で文字境界を自動判定

## ビルド要件

- **コンパイラ**: llvm-mos (mos-c64-clang)
- **依存ライブラリ**:
  - jtxt (日本語テキスト表示)
  - ime (かな漢字変換、オプション)
- **カートリッジ**: MagicDesk形式ROMカートリッジ（日本語フォント・辞書データ）

## 制限事項

- 最大編集可能サイズ: 約11KB
- 長い行（40文字以上）は複数の画面行に折り返されます
- Undo/Redo機能なし
- 検索・置換機能なし

## ライセンス

このソフトウェアは、元のqeエディタ（© 2019 David Given）を大幅に改造したものです。

元のqeエディタは2条項BSDライセンスの下で配布されています：
https://github.com/davidgiven/cpm65

改変部分も同じ2条項BSDライセンスの下で配布されます。

### 2-Clause BSD License

Copyright (c) 2019 David Given (original qe editor)
Copyright (c) 2025 Hiroshi OGINO / H.O SOFT Inc. (Japanese language support and C64 port)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## クレジット

- **元のqeエディタ**: David Given (https://github.com/davidgiven/cpm65)
- **C64移植・日本語対応**: Hiroshi OGINO / H.O SOFT Inc. (https://github.com/h-o-soft/c64jp)
- **美咲フォント**: 門真なむ氏 (https://littlelimit.net/misaki.htm)

## 参考リンク

- 元のqeエディタ: https://github.com/davidgiven/cpm65/blob/master/apps/qe.c
- c64jpプロジェクト: https://github.com/h-o-soft/c64jp