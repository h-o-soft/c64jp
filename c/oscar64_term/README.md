# C64 日本語ターミナル

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Oscar64コンパイラでビルドされたCommodore 64用日本語対応ネットワークターミナルです。

## 概要

Ultimate II+カートリッジのネットワーク機能を利用して、Telnetサーバーに接続し日本語表示でターミナル操作を行うアプリケーションです。MagicDesk形式のCRTカートリッジとしても動作します。

### 主な機能

- **Telnet接続**: Ultimate II+経由のTCP/IPネットワーク接続
- **日本語表示**: jtxtライブラリによるビットマップモードでの日本語表示
- **かな漢字変換**: IMEによるローマ字入力からの日本語変換
- **XMODEMファイル転送**: ダウンロード（受信）およびアップロード（送信）対応
- **VT100エスケープシーケンス**: 基本的なカーソル移動・画面制御に対応
- **MagicDesk CRT版**: カートリッジ単体で動作するCRT版（オーバーレイバンク使用）

## ファイル構成

```
oscar64_term/
├── Makefile           # ビルド設定
├── include/
│   ├── telnet.h       # Telnetプロトコルヘッダ
│   └── xmodem.h       # XMODEMプロトコルヘッダ
└── src/
    ├── term_main.c    # メイン（接続UI、ターミナルセッション）
    ├── telnet.c       # TelnetプロトコルIAC処理
    └── xmodem.c       # XMODEMファイル転送・KERNAL I/O
```

共有ライブラリ（jtxt, IME, c64uネットワーク）は `../oscar64_lib/` から参照しています。

## ビルド方法

```bash
# PRG版のビルド
make

# MagicDesk CRT版のビルド
make crt

# Ultimate 64へのデプロイ（PRG版）
make deploy

# VICE エミュレータで実行
make run
```

## 使用方法

### 接続
1. 起動するとホスト名入力画面が表示されます
2. 接続先ホスト名とポート番号を入力してReturn
3. 接続成功後、ターミナルセッションに入ります

### ターミナル操作

| キー | 動作 |
|------|------|
| `Commodore + Space` | IME（かな漢字変換）の有効化/無効化 |
| `F1` | XMODEMダウンロード（ファイル受信） |
| `F3` | XMODEMアップロード（ファイル送信） |
| `F7` | 切断 |
| `RUN/STOP` | XMODEM転送の中断 |

### XMODEMファイル転送

ダウンロード・アップロードともに以下の手順：
1. デバイス番号を選択（+/-で変更、Returnで確定）
2. ファイル名を入力
3. ファイルタイプを選択（P:プログラム / S:シーケンシャル / U:ユーザー）
4. 確認画面でY/Nを選択
5. 転送実行（進捗はドットで表示）

XMODEM-CRC（CRC-16）モードとチェックサムモードの両方に対応しています。

## MagicDesk CRT版について

CRT版ではオーバーレイバンクを使用して、限られたメモリ空間でIMEとXMODEMを共存させています：

- **Bank 1**: IMEオーバーレイ（通常時）
- **Bank 37**: XMODEMオーバーレイ（ファイル転送時）

XMODEM機能の使用後はIMEオーバーレイが自動的に再ロードされます。

## 必要要件

- **Oscar64コンパイラ**: https://github.com/drmortalwombat/oscar64
- **Ultimate II+カートリッジ**: ネットワーク接続用（実機またはUltimate 64）
- **MagicDeskカートリッジ**: `c64jpkanji.crt`（ルートディレクトリで `make crt` で生成）
- **VICEエミュレータ**（オプション、ネットワーク機能は実機のみ）

## 関連プロジェクト

- `../oscar64_lib/` - 共有ライブラリ（jtxt, IME, c64uネットワーク）
- `../oscar64/` - 基本サンプル
- `../oscar64_qe/` - QEテキストエディタ
- `../oscar64_crt/` - EasyFlash版

## ライセンス

MITライセンス
