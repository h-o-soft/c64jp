# フォント変換ツール

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

C64漢字ROMカートリッジ用のフォントデータ変換ツールです。

## 概要

美咲フォントのPNG画像ファイルから、Commodore 64で使用可能な8x8ピクセルのビットマップフォントデータを生成します。出力形式は1ピクセル1ビットのバイナリデータです。

## 生成されるフォントファイル

| ファイル名 | 説明 | サイズ | 文字数 |
|-----------|------|--------|--------|
| `font_misaki_gothic.bin` | 美咲ゴシック（JIS X 0208全角文字） | 70,688 bytes | 6,355文字 |
| `font_misaki_mincho.bin` | 美咲明朝（JIS X 0208全角文字） | 70,688 bytes | 6,355文字 |
| `font_jisx0201.bin` | JIS X 0201半角文字 | 2,048 bytes | 256文字 |

### フォントの特徴

本コンバータでは、**JIS X 0201（半角文字）とJIS X 0208（全角文字）の両方を同じ8x8ピクセルサイズで統一しています**。

- JIS X 0208：美咲フォントの全角文字をそのまま8x8ピクセルで使用
- JIS X 0201：美咲フォントの半角用画像（misaki_4x8.pngなど）は使用せず、**全角フォントから対応する文字をピックアップして生成**
  - 例：半角の「A」は全角の「Ａ」から、半角の「ｱ」は全角の「ア」から抽出
  - これにより、半角・全角どちらも同じ8x8ピクセルで表示可能

## 使用方法

### 基本的な使い方

```bash
# フォントファイルを生成
make

# すべてのフォントファイルを生成（デフォルトと同じ）
make all

# フォントファイルのみ変換（すでにPNGファイルがある場合）
make convert-font
```

### その他のターゲット

```bash
# 美咲フォントをダウンロードして展開
make download-misaki

# 美咲フォントの状態を確認
make check-misaki

# 生成ファイルを削除
make clean

# 美咲フォントを含むすべてのファイルを削除
make clean-all

# ヘルプを表示
make help
```

## フォントデータ形式

### バイナリフォーマット

各文字は8バイト（8x8ピクセル）で構成されます：
- 1バイト目: 1行目のピクセルデータ（MSBが左端）
- 2バイト目: 2行目のピクセルデータ
- ...
- 8バイト目: 8行目のピクセルデータ

ビット値：
- `1`: 前景色（文字部分）
- `0`: 背景色

### 文字コード対応

#### JIS X 0208（全角文字）
- 区点コード（Kuten code）順に格納
- 区点コードからオフセットへの変換式: `((区 - 1) * 94 + (点 - 1)) * 8`

#### JIS X 0201（半角文字）
- ASCIIコード順に格納（0x00〜0xFF）
- 文字コードからオフセットへの変換式: `文字コード * 8`

### Shift-JISからの変換

プログラムからフォントデータにアクセスする際は、Shift-JISコードを区点コードに変換してからオフセットを計算します。

```python
def sjis_to_kuten(sjis_high, sjis_low):
    """Shift-JISコードを区点コードに変換"""
    ku = sjis_high
    ten = sjis_low
    
    if ku <= 0x9f:
        if ten < 0x9f:
            ku = (ku << 1) - 0x102
        else:
            ku = (ku << 1) - 0x101
    else:
        if ten < 0x9f:
            ku = (ku << 1) - 0x182
        else:
            ku = (ku << 1) - 0x181
    
    if ten < 0x7f:
        ten -= 0x40
    elif ten < 0x9f:
        ten -= 0x41
    else:
        ten -= 0x9f
    
    return ku, ten

def kuten_to_offset(ku, ten):
    """区点コードからフォントデータオフセットを計算"""
    return ((ku - 1) * 94 + (ten - 1)) * 8
```

## 技術仕様

### mkfont.py

主要な処理：
1. **PNG画像読み込み**: 美咲フォントのPNG画像を読み込み
2. **グレースケール変換**: カラー画像をグレースケールに変換
3. **ビットマップ変換**: 8x8ピクセル単位でビットマップデータに変換
4. **バイナリ出力**: 各文字8バイトのバイナリファイルとして出力

変換ルール：
- ピクセル値 > 128: 白（0）
- ピクセル値 ≤ 128: 黒（1）

### JIS X 0201フォント生成

美咲フォントの半角用画像（misaki_4x8.pngなど）は使用せず、全角フォントデータから対応する文字を抽出して生成：

1. JIS X 0201の文字コード（0x00〜0xFF）に対応する全角文字を特定
   - 例：0x41（半角'A'） → 全角「Ａ」（JIS X 0208）
   - 例：0xB1（半角'ｱ'） → 全角「ア」（JIS X 0208）
2. 全角フォント（8x8ピクセル）から該当文字のデータを抽出
3. 256文字分のデータをまとめてバイナリファイルとして出力

これにより、JIS X 0201の半角文字もJIS X 0208の全角文字と同じ8x8ピクセルサイズで統一され、Commodore 64での表示処理が簡素化されます。

## ファイル構成

```
fontconv/
├── Makefile              # ビルドファイル
├── mkfont.py            # フォント変換スクリプト
├── README.md            # このファイル
├── misaki_gothic.png    # 美咲ゴシックPNG（ダウンロード後）
├── misaki_mincho.png    # 美咲明朝PNG（ダウンロード後）
├── font_misaki_gothic.bin  # 生成: ゴシック体フォント
├── font_misaki_mincho.bin  # 生成: 明朝体フォント
└── font_jisx0201.bin       # 生成: 半角フォント
```

## 依存関係

### 必須
- Python 3.x
- Pillow (Python Imaging Library)
- GNU Make
- curl（ダウンロード用）
- unzip（展開用）

### インストール方法
```bash
# Pythonパッケージのインストール
pip install Pillow

# macOSの場合
brew install curl unzip

# Linuxの場合
apt-get install curl unzip  # Debian/Ubuntu
yum install curl unzip      # CentOS/RHEL
```

## トラブルシューティング

### PIL/Pillowがインストールされていない
```bash
pip install Pillow
# または
pip3 install Pillow
```

### 美咲フォントのダウンロードに失敗する
手動でダウンロード：
1. https://littlelimit.net/arc/misaki/misaki_png_2021-05-05a.zip をダウンロード
2. fontconvディレクトリに展開
3. `make convert-font`を実行

### フォントサイズが期待値と異なる
- 美咲フォントのバージョンを確認
- PNG画像のサイズが正しいか確認（misaki_gothic.png: 752x752ピクセル）

## ライセンス

### mkfont.py
MITライセンス（プロジェクト全体のライセンスに準拠）

### 美咲フォント
- 作者: 門真なむ氏
- ライセンス: [MITライセンス](https://littlelimit.net/misaki.htm)
- 美咲フォントは自動的にダウンロードされます

## 関連ドキュメント

- [プロジェクトREADME](../README.md)
- [美咲フォント公式サイト](https://littlelimit.net/misaki.htm)
- [JIS X 0208文字コード表](https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0208.html)
- [JIS X 0201文字コード表](https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0201.html)