# CRTカートリッジ作成ツール

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

C64漢字ROMシステム用のMagicDesk形式CRTカートリッジファイルを作成します。

## 概要

このツールは、日本語フォントデータ、辞書データ、文字列リソースをMagicDesk形式のカートリッジファイルにパッケージ化します。作成されたCRTファイルは、Commodore 64エミュレータや互換性のあるフラッシュカートリッジを搭載した実機で使用できます。

## 機能

- **MagicDesk形式**: 8KBバンク切り替え対応のType 19 (MagicDesk) CRTファイルを作成
- **複数リソース対応**: フォント、辞書、文字列リソースを単一カートリッジに統合
- **自動アセンブル**: 64tassを使用してブートコードを自動的にアセンブル
- **柔軟なレイアウト**: リソースを最適なバンク位置に自動配置

## ファイル構成

| ファイル | 説明 |
|---------|------|
| `create_crt.py` | メインCRT作成スクリプト |
| `kanji-magicdesk-basic.asm` | ブートコードソース（64tass形式） |
| `kanji-magicdesk-basic.bin` | アセンブル済みブートコード（生成される） |

## 使用方法

### 基本的な使い方

```bash
# フォントのみの基本カートリッジを作成
python create_crt.py

# カスタム出力名でカートリッジを作成
python create_crt.py -o my_cartridge.crt

# 辞書付きカートリッジを作成
python create_crt.py --dictionary-file ../dicconv/skkdicm.bin

# 文字列リソース付きカートリッジを作成
python create_crt.py --string-resource-file ../stringresources/test_strings.txt
```

### コマンドラインオプション

| オプション | 説明 | デフォルト値 |
|-----------|------|-------------|
| `-o, --output` | 出力CRTファイル名 | `c64jpkanji.crt` |
| `--font-file` | 全角フォントファイルパス | `../fontconv/font_misaki_gothic.bin` |
| `--jisx0201-file` | 半角フォントファイルパス | `../fontconv/font_jisx0201.bin` |
| `--dictionary-file` | 辞書ファイルパス | なし（オプション） |
| `--string-resource-file` | 文字列リソースファイル | なし（オプション） |

## カートリッジ構造

MagicDeskカートリッジは8KBバンク単位で構成されています：

### バンクレイアウト

| バンク | 内容 | サイズ |
|--------|------|--------|
| 0 | ブートコード（kanji-magicdesk-basic） | 8KB |
| 1-9 | フォントデータ（JIS X 0201 + JIS X 0208） | 約72KB |
| 10-35 | 辞書データ（含まれる場合） | 約208KB |
| 36+ | 文字列リソース（含まれる場合） | 可変 |

### メモリマッピング

- **$8000-$9FFF**: 8KBバンクウィンドウ（ROML）
- **$DE00**: MagicDeskバンクレジスタ

## ブートコード

カートリッジには最小限のブートプログラム（`kanji-magicdesk-basic.asm`）が含まれており、以下の処理を行います：

1. C64ハードウェアの初期化
2. "C64 KANJI ROM SYSTEM"メッセージの表示
3. 約1秒間の待機
4. BASICへの復帰

### カートリッジヘッダー

ブートコードには必須のCBM80署名が含まれています：
- **$8000-$8001**: コールドスタートベクター
- **$8002-$8003**: ウォームスタートベクター
- **$8004-$8008**: PETSCIIでの"CBM80"署名

## CRTファイル形式

生成されるCRTファイルは標準のVICE CRT形式に従います：

### CRTヘッダー（64バイト）
- 署名: "C64 CARTRIDGE   "
- ハードウェアタイプ: 19（MagicDesk）
- EXROM: 0、GAME: 1（16K構成）
- 名前: "KANJI ROM MD"

### CHIPパケット
各8KBバンクはCHIPパケットとして格納されます：
- CHIP署名
- パケット長
- バンク番号
- ロードアドレス（$8000）
- データ（8192バイト）

## ビルドプロセス

1. **ブートコードのアセンブル**: 64tassが`kanji-magicdesk-basic.asm`をバイナリにアセンブル
2. **リソースの読み込み**: フォントファイル、辞書、文字列リソースを読み込み
3. **バンクの配置**: リソースを連続した8KBバンクに配置
4. **CRTの作成**: CRTヘッダーとCHIPパケットを生成
5. **ファイルの書き込み**: 完成したCRTファイルを出力

## 必要環境

- Python 3.x
- 64tassアセンブラ（Prog8に含まれる）
- フォントファイル（fontconvで生成）
- オプション：辞書ファイル（dicconvで生成）
- オプション：文字列リソースファイル

## メインビルドとの統合

このツールは通常、メインのMakefileから呼び出されます：

```makefile
# 基本CRTを作成
make crt

# 辞書付きCRTを作成
make dict

# 文字列リソース付きCRTを作成
make TARGET=hello_resource run-strings
```

## テスト方法

生成されたカートリッジをVICEでテスト：

```bash
# VICEエミュレータで実行
x64sc -cartcrt c64jpkanji.crt

# 追加プログラムと共に実行
x64sc -cartcrt c64jpkanji.crt prog8/build/hello.prg
```

## ハードウェア互換性

MagicDesk形式は以下でサポートされています：
- EasyFlash 3
- Ultimate 64 / 1541 Ultimate-II+
- Kung Fu Flash
- VICEエミュレータ（x64sc）

## トラブルシューティング

### アセンブルエラー
64tassがブートコードのアセンブルに失敗する場合：
- 64tassがPATHに含まれているか確認
- kanji-magicdesk-basic.asmの構文を検証
- ファイルが存在し読み取り可能か確認

### ファイルが見つからない
フォントや他のリソースファイルが見つからない場合：
- `make fonts`を実行してフォントファイルを生成
- `make dict`を実行して辞書ファイルを生成
- コマンドライン引数のファイルパスを確認

### バイナリサイズ警告
バイナリサイズに関する警告が表示される場合：
- ツールは自動的に8KB境界にパディングまたは切り詰めを行います
- これは正常な動作で、機能には影響しません

### カートリッジが認識されない
カートリッジが動作しない場合：
- CRTファイルサイズが8KBの倍数 + 64バイト（ヘッダー）であることを確認
- ブートコードにCBM80署名が含まれているか確認
- 使用しているハードウェア/エミュレータがMagicDeskをサポートしているか確認

## 関連ドキュメント

- [メインREADME](../README.md) - プロジェクト概要
- [fontconv](../fontconv/README.md) - フォント変換ツール
- [dicconv](../dicconv/README.md) - 辞書変換
- [stringresources](../stringresources/README.md) - 文字列リソース管理