# 文字列リソース管理ツール

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

C64漢字ROMカートリッジ用の文字列リソース変換・管理ツールです。

## 概要

固定文字列（メニュー、メッセージ、説明文など）をROMカートリッジに格納し、プログラムから高速にアクセスできるようにするツールです。テキストファイルまたはCSVファイルから、インデックス付きバイナリ形式に変換します。

## 使用方法

### 基本的な使い方

```bash
# テキストファイルからバイナリ変換
python3 convert_string_resources.py input.txt output.bin

# CSVファイルから変換（ID付き）
python3 convert_string_resources.py strings.csv strings.bin

# 開始オフセットを指定（CRTファイル内の位置）
python3 convert_string_resources.py input.txt output.bin 0x20000

# 8KBアラインメントなし（デフォルトは8KBアラインあり）
python3 convert_string_resources.py input.txt output.bin 0x20000 --no-align
```

### プロジェクトのMakefileから使用

```bash
# プロジェクトルートで実行
make TARGET=stateful_test run-strings  # 文字列リソース付きで実行
```

## 入力ファイル形式

### テキストファイル形式

1行に1つの文字列を記述：
```
こんにちは、世界！
Commodore 64で日本語を表示
漢字ROMカートリッジ
メニュー項目1
メニュー項目2
```

### CSVファイル形式

ID番号と文字列をカンマで区切って記述：
```csv
0,メインメニュー
1,ゲーム開始
2,オプション設定
3,ハイスコア表示
4,終了
10,設定画面
11,音量調整
12,画面設定
```

## 出力形式（バイナリ）

### ヘッダー構造
```
+0x00: 'STR' + 0x00 (4 bytes) - マジックナンバー
+0x04: エントリ数 (2 bytes, リトルエンディアン)
+0x06: 予約領域 (2 bytes)
+0x08: インデックステーブル開始
```

### インデックステーブル

各エントリごとに3バイト：
```
+0: オフセット低位 (1 byte)
+1: オフセット高位 (1 byte)  
+2: バンク番号 (1 byte)
```

実際のアドレス計算：
```
実アドレス = バンク番号 * 8192 + オフセット
```

### 文字列データ

インデックステーブルの後に、各文字列がShift-JISエンコーディング、null終端で格納されます。

## C64プログラムからの使用

### Prog8での使用例

```prog8
; 文字列リソースの初期化
jtxt.load_string_resource(resource_bank)

; ID指定で文字列を取得して表示
ubyte[] buffer = [0] * 256
if jtxt.get_string_by_id(5, buffer, 256) {
    jtxt.bputs(buffer)
}

; インデックス指定で文字列を取得
if jtxt.get_string_by_index(0, buffer, 256) {
    jtxt.bputs(buffer)
}
```

### アセンブリでの使用例

```assembly
; バンク切り替え
lda #string_bank
sta $de00

; インデックステーブルから文字列アドレスを取得
ldy string_id
lda index_table,y
sta ptr_lo
iny
lda index_table,y
sta ptr_hi

; 文字列を読み込み
ldy #0
loop:
    lda (ptr),y
    beq done
    jsr putchar
    iny
    bne loop
done:
```

## 技術仕様

### convert_string_resources.py

主要な関数：

#### load_strings_from_file
```python
def load_strings_from_file(filename):
    """テキストまたはCSVファイルから文字列を読み込み"""
    # CSVの場合: ID,文字列の形式
    # テキストの場合: 1行1文字列
    return strings_dict
```

#### create_binary_resource
```python
def create_binary_resource(strings, base_offset=0):
    """文字列辞書からバイナリリソースを作成"""
    # 1. ヘッダー作成
    # 2. インデックステーブル作成
    # 3. 文字列データ追加
    return binary_data
```

### メモリ効率

- 重複文字列の自動検出と共有
- 8KBバンク境界でのアラインメント
- 圧縮オプション（将来実装予定）

## ファイル構成

```
stringresources/
├── convert_string_resources.py  # 変換スクリプト
├── README.md                    # このファイル
├── test_strings.txt             # サンプル: テキスト形式
├── verified_strings.csv         # サンプル: CSV形式
├── string_resources.bin         # 出力: バイナリリソース
└── string_resources_test.bin    # テスト用バイナリ
```

## 制限事項

- 最大エントリ数: 65,535（16ビットインデックス）
- 最大文字列長: 実質無制限（メモリサイズに依存）
- 文字コード: Shift-JISのみ対応
- 総サイズ: MagicDeskカートリッジの容量（1MB）まで

## トラブルシューティング

### 文字化け

- 入力ファイルの文字コードを確認（UTF-8推奨）
- Shift-JISに変換できない文字が含まれていないか確認

### メモリ不足

- 不要な文字列を削除
- 長い文字列を分割
- 複数のリソースファイルに分割

### アクセスエラー

- バンク番号が正しいか確認
- オフセット計算が正しいか確認
- CRTファイルに文字列リソースが含まれているか確認

## ベストプラクティス

1. **ID管理**: CSVファイルでIDを明示的に管理
2. **グループ化**: 関連する文字列は近いIDに配置
3. **バージョン管理**: 文字列リソースファイルをGit管理
4. **多言語対応**: 言語ごとに別ファイルを作成

## 関連ドキュメント

- [プロジェクトREADME](../README.md)
- [CRTツールマニュアル](../CRT_TOOLS_MANUAL.md)
- [文字列リソース設計書](../STRING_RESOURCE.md)