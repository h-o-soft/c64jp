# C64 Kanji ROM Cartridge - Oscar64 EasyFlash Build

Oscar64 コンパイラで漢字 ROM とプログラムを統合した EasyFlash カートリッジを生成するサンプルプロジェクトです。

## 特徴

- **単一ソースビルド**: `hello_easyflash.c` から直接 CRT ファイルを生成
- **フォント埋め込み**: `#embed` ディレクティブでバイナリデータを埋め込み
- **自動 RAM 展開**: EasyFlash の自動起動機能でメインコードを RAM にコピー
- **クロスバンク実行**: ROM バンクのコードを RAM にコピーして実行可能
- **Python 不要**: Oscar64 コンパイラのみで完結

## ビルド方法

```bash
make
```

これだけで `hello_easyflash.crt` が生成されます。

## 実行方法

```bash
make run
```

VICE エミュレータでカートリッジを起動します。

ルートディレクトリからも実行可能：

```bash
make oscar-crt-run
```

## メモリマップ

### RAM 領域

| アドレス | 用途 |
|----------|------|
| $0900-$7FFF | メインプログラム (自動コピー) |
| $3000-$37FF | キャラクタ RAM (テキストモード) |
| $5C00-$5FFF | 画面 RAM (ビットマップモード) |
| $6000-$7FFF | ビットマップデータ |
| $C000-$CFFF | 追加コード領域 (ROM からコピー) |

### EasyFlash ROM バンク (16KB each, $8000-$BFFF)

| Bank | 内容 | サイズ |
|------|------|--------|
| 0 | メインプログラム (起動時 RAM にコピー) | 16KB |
| 1 | JIS X 0201 + ゴシック Part 1 | 2KB + 14KB |
| 2 | ゴシック Part 2 | 16KB |
| 3 | ゴシック Part 3 | 16KB |
| 4 | ゴシック Part 4 | 16KB |
| 5 | ゴシック Part 5 (残り) | ~6KB |
| 6 | 追加コード ($C000 にコピーして実行) | 4KB |

## プロジェクト構造

```
hello_easyflash.c    メインソースファイル
├── Main Region      $0900-$8000 (RAM に自動コピー)
│   ├── ccopy()      クロスバンクコピー関数
│   ├── bankcall_*() バンク呼び出しラッパー
│   ├── jtxt ライブラリ
│   └── main()
├── Bank 1-5         フォントデータ
└── Bank 6           追加コード ($C000 にリロケート)
```

## クロスバンクコード実行

Bank 6 のコードは `#pragma region` の 7 番目のパラメータでリロケーションアドレスを指定：

```c
#pragma region(bank6, 0x8000, 0xc000, , 6, { code6 }, 0xc000)
```

これにより、Bank 6 のコードは $8000 に配置されますが、$C000 で実行されるようにリンクされます。

### 使用方法

```c
// 1. ROM から RAM にコピー
ccopy(6, (char*)0xC000, (char*)0x8000, 0x1000);

// 2. 関数を直接呼び出し
test_from_bank6_first();
test_from_bank6_second();
```

## 必要要件

- **Oscar64 コンパイラ**: https://github.com/drmortalwombat/oscar64
- **フォントファイル**: `../../fontconv/` に配置
  - `font_jisx0201.bin` (2KB)
  - `font_misaki_gothic.bin` (~70KB)
- **VICE エミュレータ** (実行時のみ)

## ビルドコマンド詳細

```bash
oscar64 -n -tf=crt -i=include -o=hello_easyflash.crt hello_easyflash.c src/*.c
```

- `-n`: スタートアップコード不要
- `-tf=crt`: EasyFlash 形式 CRT 出力 (16KB バンク)
- `-i=include`: インクルードディレクトリ

## トラブルシューティング

### フォントファイルが見つからない

```bash
cd ../../fontconv
make
```

### Oscar64 がインストールされていない

https://github.com/drmortalwombat/oscar64 からダウンロード・インストールしてください。

## 関連プロジェクト

- `../oscar64/` - PRG 版 (外部 MagicDesk カートリッジ使用)
- `../oscar64_qe/` - QE テキストエディタ (外部カートリッジ使用)

## ライセンス

本プロジェクトのサンプルコードは自由に使用・改変できます。
