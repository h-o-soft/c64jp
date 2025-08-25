# ime.p8 - 日本語かな漢字変換ライブラリ

| [English](ime-en.md) | [日本語](ime.md) |
|---------------------------|----------------------|

C64用の日本語入力システム（IME）ライブラリです。ローマ字入力から日本語への変換を実現します。

## 概要

ime.p8は、Commodore 64で日本語入力を可能にするIMEライブラリです。ローマ字入力からひらがな・カタカナ・漢字への変換を行い、MagicDesk ROMカートリッジ内の辞書データを使用して単文節変換を実現しています。

## 主な機能

- **ローマ字→かな変換**
  - 標準的なローマ字入力規則に対応
  - 拗音（きゃ、しゃ等）、撥音（ん）、促音（っ）対応
  - 小文字（ぁぃぅぇぉ、ゃゅょ）対応

- **かな漢字変換**
  - 単文節変換方式
  - 複数候補からの選択
  - 動詞活用対応（送りあり変換）

- **入力モード**
  - ひらがなモード
  - カタカナモード  
  - 全角英数モード
  - 直接入力（IME OFF）

## 基本的な使い方

### 初期化
```prog8
ime.init()  ; IMEライブラリの初期化
```

### メインループでの処理（ノンブロッキング）
```prog8
ubyte event = ime.process()

when event {
    ime.IME_EVENT_NONE -> {
        ; 継続中（何もしない）
    }
    ime.IME_EVENT_CONFIRMED -> {
        ; 文字列が確定した
        uword text = ime.get_confirmed_text()
        jtxt.bputs(text)
    }
    ime.IME_EVENT_CANCELLED -> {
        ; 入力がキャンセルされた
    }
    ime.IME_EVENT_MODE_CHANGED -> {
        ; 入力モードが変更された
    }
    ime.IME_EVENT_KEY_PASSTHROUGH -> {
        ; IMEを通過したキーを処理
        ubyte key = ime.get_passthrough_key()
        ; アプリケーション側で処理
    }
    ime.IME_EVENT_DEACTIVATED -> {
        ; IMEが無効化された
    }
}
```

## キー操作

### IME制御
- **Commodore + スペース**: IME ON/OFF切り替え
- **F1**: ひらがなモード
- **F3**: カタカナモード
- **F5**: 全角英数モード
- **ESC**: 入力キャンセル
- **Return**: 確定

### 変換操作
- **スペース**: 変換開始／次候補
- **Shift + スペース**: 前候補
- **Return**: 候補確定
- **ESC**: 変換キャンセル

## 主要関数

### 初期化・状態管理

#### init()
IMEライブラリを初期化します。辞書データの存在確認と内部状態のリセットを行います。

#### process() -> ubyte
IMEのメイン処理関数（ノンブロッキング）。キー入力の取得、ローマ字変換、かな漢字変換、表示更新などを行います。
- 戻り値: IME_EVENT_* 定数のいずれか

#### is_ime_active() -> bool
IMEがアクティブかどうかを返します。

#### toggle_ime_mode()
IMEのON/OFFを切り替えます。

### 取得関数

#### get_confirmed_text() -> uword
確定した文字列のアドレスを取得します。IME_EVENT_CONFIRMEDイベント後に呼び出します。

#### get_passthrough_key() -> ubyte
IMEを通過したキーコードを取得します。IME_EVENT_KEY_PASSTHROUGHイベント後に呼び出します。

### モード設定

#### set_hiragana_mode()
ひらがなモードに切り替えます（F1キー相当）。

#### set_katakana_mode()
カタカナモードに切り替えます（F3キー相当）。

#### set_alphanumeric_mode()
全角英数モードに切り替えます（F5キー相当）。

### 表示関連

#### show_ime_status()
IMEの状態表示を更新します（画面右上に[あ]、[カ]、[英]などを表示）。

#### update_ime_display()
入力中の文字列表示を更新します。

#### draw_candidates_window()
変換候補ウィンドウを表示します。

## IMEイベント定数

```prog8
const ubyte IME_EVENT_NONE = 0          ; 継続中
const ubyte IME_EVENT_CONFIRMED = 1     ; 文字列確定
const ubyte IME_EVENT_CANCELLED = 2     ; キャンセル
const ubyte IME_EVENT_MODE_CHANGED = 3  ; モード変更
const ubyte IME_EVENT_DEACTIVATED = 4   ; IME無効化
const ubyte IME_EVENT_KEY_PASSTHROUGH = 5 ; キー透過
```

## ローマ字入力規則

### 基本規則
- あ行: a, i, u, e, o
- か行: ka, ki, ku, ke, ko
- が行: ga, gi, gu, ge, go
- さ行: sa, shi/si, su, se, so
- ざ行: za, ji/zi, zu, ze, zo
- た行: ta, chi/ti, tsu/tu, te, to
- だ行: da, di, du, de, do
- な行: na, ni, nu, ne, no
- は行: ha, hi, fu/hu, he, ho
- ば行: ba, bi, bu, be, bo
- ぱ行: pa, pi, pu, pe, po
- ま行: ma, mi, mu, me, mo
- や行: ya, yu, yo
- ら行: ra, ri, ru, re, ro
- わ行: wa, wo, n

### 特殊入力
- 拗音: kya, kyu, kyo, sha, shu, sho, cha, chu, cho など
- 促音: kk→っk、tt→っt など（子音重複）
- 撥音: n（単独）、nn（明示的）
- 小文字: xa→ぁ、xi→ぃ、xya→ゃ、xtsu→っ など

## 使用例（ime_test.p8より）

```prog8
%import ime
%import jtxt

main {
    sub start() {
        ; 初期化
        ime.init()
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        bool exit_requested = false
        
        ; メインループ
        while not exit_requested {
            ubyte event = ime.process()
            
            when event {
                ime.IME_EVENT_CONFIRMED -> {
                    ; 確定した文字列を表示
                    uword confirmed_text = ime.get_confirmed_text()
                    jtxt.bputs(confirmed_text)
                }
                ime.IME_EVENT_CANCELLED -> {
                    ; キャンセル処理
                }
                ime.IME_EVENT_KEY_PASSTHROUGH -> {
                    ; パススルーキー処理
                    ubyte key = ime.get_passthrough_key()
                    when key {
                        20 -> {  ; BackSpace
                            jtxt.bputc(8)
                        }
                        13 -> {  ; Return
                            jtxt.bnewline()
                        }
                    }
                }
                ime.IME_EVENT_DEACTIVATED -> {
                    ; IME無効化時の処理
                }
            }
            
            sys.wait(1)  ; フレーム制御
        }
    }
}
```

## メモリ使用

- ローマ字バッファ: 8バイト
- ひらがなバッファ: 64バイト
- 候補バッファ: 256バイト
- 確定文字列バッファ: 256バイト

## 注意事項

1. **辞書データ**: CRTファイルのBank 10-35に辞書データが必要です。辞書がない場合はひらがな入力のみ可能です。
2. **表示更新**: IME表示はjtxt.p8のビットマップモード機能を使用します。
3. **ノンブロッキング処理**: process()関数は毎フレーム呼び出す必要があります。
4. **文字コード**: 内部処理はShift-JISで統一されています。