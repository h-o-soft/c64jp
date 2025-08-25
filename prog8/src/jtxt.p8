
; 日本語テキスト表示ライブラリ (jtxt.p8) - 6502最適化版
; Copyright (c) 2025 H.O SOFT Inc. - Licensed under MIT License
; MagicDesk漢字ROMを使ったPCG機能
jtxt {
    ; MagicDesk関連定数
    const uword ROM_BASE = $8000        ; ROM開始アドレス($8000-$9FFF) 8KB
    const uword BANK_REG = 56832        ; $DE00 MagicDeskバンク切り替え
    
    ; 文字関連定数
    const uword CHARSET_ROM = $D000     ; ROMキャラクタセット
    const uword CHARSET_RAM = $3000     ; RAMキャラクタセット（$3000-$3800）
    const uword SCREEN_RAM = $0400      ; テキストモード用
    const uword BITMAP_SCREEN_RAM = $5C00  ; ビットマップモード用（$6000直前、バンク1内）      
    const uword CHAR_WIDTH = 40
    
    ; ビットマップモード関連定数
    const uword BITMAP_BASE = $6000     ; ビットマップメモリ開始アドレス
    const ubyte TEXT_MODE = 0           ; テキストモード
    const ubyte BITMAP_MODE = 1         ; ビットマップモード
    
    ; フォントオフセット定数
    const uword JISX0201_SIZE = 2048    ; JIS X 0201フォントサイズ（256文字×8バイト）
    const uword JISX0208_OFFSET = JISX0201_SIZE  ; 全角フォントの開始オフセット
    
    ; 文字列リソース定数
    const ubyte STRING_RESOURCE_BANK = 36     ; 文字列リソース開始バンク（MagicDesk用）
    const uword STRING_RESOURCE_BASE = ROM_BASE   ; 文字列リソースベースアドレス（$8000）
    const uword STRING_BUFFER = $0340         ; 文字列バッファ（スタック上部、192バイト利用可能）
    const ubyte STRING_BUFFER_SIZE = 191      ; バッファサイズ（$0340-$03FF = 192バイト、NULL終端用に1バイト残す）
    
    ; ライブラリ状態変数（6502最適化のため絶対値で管理）
    ubyte chr_start = 128               ; 使用文字範囲開始
    ubyte chr_count = 64                ; 使用可能文字数
    ubyte current_index = 128           ; 現在の文字定義インデックス（絶対値）
    uword screen_pos = SCREEN_RAM       ; 現在のスクリーン描画位置（事前計算）
    uword color_pos = $D800             ; 現在のカラーRAM位置（screen_posと対応）
    ubyte current_color = 1             ; 現在の文字色（デフォルト：白）
    const uword SCREEN_END = SCREEN_RAM + 999  ; 画面最終位置（999 = 24*40+39）
    
    ; Shift-JISステート管理
    ubyte sjis_state = 0                ; 0=通常, 1=Shift-JIS 2バイト目待ち
    ubyte sjis_first_byte = 0           ; Shift-JIS 1バイト目の保存
    
    ; ビットマップモード関連変数
    ubyte display_mode = TEXT_MODE      ; 現在の表示モード
    ubyte bitmap_x = 0                  ; ビットマップ X座標（文字単位 0-39）
    ubyte bitmap_y = 0                  ; ビットマップ Y座標（文字単位 0-24）
    ubyte bitmap_fg_color = 1           ; ビットマップ前景色
    ubyte bitmap_bg_color = 0           ; ビットマップ背景色
    
    ; ビットマップ行範囲制御変数
    ubyte bitmap_top_row = 0            ; 描画開始行（デフォルト: 0）
    ubyte bitmap_bottom_row = 24        ; 描画終了行（デフォルト: 24）
    bool bitmap_window_enabled = false ; 行範囲制御有効フラグ
    
    ; ハードウェア初期化（文字範囲・モードも同時設定）
    sub init(ubyte start_char, ubyte char_count, ubyte mode) {
        ; 文字範囲設定
        set_range(start_char, char_count)
        
        ; 表示モード設定
        set_mode(mode)
        
        ; MagicDeskをバンク0に設定
        @(BANK_REG) = 0
        
        ; 割り込み禁止
        @($DC0E) = @($DC0E) & %11111110

        ; メモリマップ設定でキャラクタROMを見えるようにする
        @($01) = $33  ; ROM + I/O visible, CHAR ROM visible
        
        ; TEXT_MODEでのみキャラクタセットをROMからRAMにコピー
        ; BITMAP_MODEでは$3000のメモリ破壊を避けるためスキップ
        if display_mode == TEXT_MODE {
            copy_charset_to_ram()
        }

        ; メモリマップを戻す
        @($01) = $37  ; ROM + I/O visible

        ; 割り込み復帰
        @($DC0E) = @($DC0E) | %00000001

        ; VICをRAMキャラクタセットに切り替え（TEXT_MODEの場合のみ）
        if display_mode == TEXT_MODE {
            @($D018) = (@($D018) & %11110000) | %00001100
        }
    }
    
    ; 文字範囲設定のみ（最適化：絶対値で管理）
    sub set_range(ubyte start_char, ubyte char_count) {
        chr_start = start_char
        chr_count = char_count
        current_index = chr_start          ; 絶対値で初期化
        screen_pos = SCREEN_RAM            ; 画面左上から開始
        color_pos = $D800                  ; カラーRAM左上から開始
    }
    
    ; ライブラリ使用文字のクリア（最適化：範囲限定）
    sub cls() {
        ; 画面上のライブラリ文字をクリア（最適化：1回のループ）
        uword screen_ptr = SCREEN_RAM
        uword screen_end = SCREEN_RAM + 1000    ; 25行×40文字
        ubyte char_end = chr_start + chr_count
        
        while screen_ptr < screen_end {
            ubyte ch = @(screen_ptr)
            if ch >= chr_start and ch < char_end {
                @(screen_ptr) = 32  ; スペース文字（C64スクリーンコード）
            }
            screen_ptr++
        }
        
        ; インデックス初期化（最適化：代入のみ）
        current_index = chr_start
        locate(0, 0)  ; 画面左上に位置設定（アドレス事前計算）
        
        ; Shift-JISステートもクリア
        sjis_state = 0
        sjis_first_byte = 0
    }
    
    ; カーソル位置設定（最適化：アドレス事前計算）
    sub locate(ubyte x, ubyte y) {
        uword offset = (y as uword) * CHAR_WIDTH + x as uword
        screen_pos = SCREEN_RAM + offset
        color_pos = $D800 + offset
    }
    
    ; 1文字出力（txt.putc互換、Shift-JISステートフル処理）
    sub putc(ubyte char_code) {
        ; Shift-JIS 2バイト目待ち状態の処理
        if sjis_state == 1 {
            ; 2バイト目として有効な範囲チェック
            if (char_code >= $40 and char_code <= $7E) or (char_code >= $80 and char_code <= $FC) {
                ; Shift-JIS文字として処理
                uword sjis_code = (sjis_first_byte as uword << 8) | char_code as uword
                putc_internal(sjis_code)
                sjis_state = 0
                sjis_first_byte = 0
                return
            } else {
                ; 無効な2バイト目の場合、1バイト目を単独で出力してリセット
                putc_internal(sjis_first_byte as uword)
                sjis_state = 0
                sjis_first_byte = 0
                ; そして現在の文字を処理続行
            }
        }
        
        ; Shift-JIS 1バイト目判定
        if (char_code >= $81 and char_code <= $9F) or (char_code >= $E0 and char_code <= $FC) {
            ; Shift-JIS 1バイト目として保存
            sjis_state = 1
            sjis_first_byte = char_code
            return
        }
        
        ; 改行コード処理
        if char_code == $0A or char_code == $0D {
            newline()
            return
        }
        
        ; 通常の1バイト文字（半角カナ／ASCII）
        if (char_code >= $A1 and char_code <= $DF) or (char_code >= $20 and char_code <= $7E) {
            putc_internal(char_code as uword)
        }
        ; その他の制御文字は無視
    }
    
    ; 内部用文字出力（実際の描画処理）
    sub putc_internal(uword char_code) {
        ; 範囲チェック（最適化：1回の比較）
        if current_index >= chr_start + chr_count {
            return  ; オーバーフロー
        }
        
        ; 文字定義
        define_char(current_index, char_code)
        
        ; 画面表示（最適化：事前計算済みアドレス使用）
        @(screen_pos) = current_index
        @(color_pos) = current_color
        
        ; 位置更新（最適化：アドレスインクリメントのみ）
        current_index++
        if screen_pos < SCREEN_END {
            screen_pos++
            color_pos++
        }
    }
    
    ; 文字列出力（putcを使用してステートフル処理）
    sub puts(uword addr) {
        uword ptr = addr
        
        while @(ptr) != 0 {
            putc(@(ptr))
            ptr++
        }
    }
    
    ; 改行処理
    sub newline() {
        ; 現在の画面位置から画面先頭アドレスを引いて相対位置を取得
        uword relative_pos = screen_pos - SCREEN_RAM
        ; 40で割って現在行を求める
        uword current_row = relative_pos / 40
        if current_row < 24 {
            current_row++
        }
        current_row = current_row * 40
        ; 次の行の先頭アドレスを計算
        screen_pos = SCREEN_RAM + current_row
        color_pos = $D800 + current_row
    }

    ; 文字色設定（最適化：単純代入）
    sub set_color(ubyte color) {
        current_color = color & 15  ; 下位4ビットのみ有効
    }
    
    ; 背景色設定
    sub set_bgcolor(ubyte color) {
        @($D021) = color & 15  ; 背景色レジスタに設定
    }
    
    ; ボーダー色設定
    sub set_bordercolor(ubyte color) {
        @($D020) = color & 15  ; ボーダー色レジスタに設定
    }
    
    ; 表示モード切り替え
    sub set_mode(ubyte mode) {
        display_mode = mode
        
        if mode == BITMAP_MODE {
            ; ビットマップモードに切り替え
            bcls()  ; 画面クリア
            
            ; VICバンク1設定（$4000-$7FFF）
            @($DD00) = (@($DD00) & %11111100) | %00000010  ; VICバンク1選択
            
            ; VICレジスタ設定
            @($D011) = @($D011) | %00100000  ; BMM=1 (ビットマップモード有効)
            @($D018) = %01111001              ; ビットマップ=$6000（CB13=1）, スクリーン=$5C00（VM13-10=0111）
        } else {
            ; テキストモードに戻す
            @($D011) = @($D011) & %11011111  ; BMM=0 (ビットマップモード無効)
            
            ; VICバンク0に戻す（$0000-$3FFF）
            @($DD00) = (@($DD00) & %11111100) | %00000011  ; VICバンク0選択
            
            ubyte vic_reg = @($D018)
            vic_reg &= %11110000
            vic_reg |= %00001100              ; キャラクタRAM=$3000
            @($D018) = vic_reg
        }
    }
    
    ; ビットマップ画面クリア
    sub bcls() {
        if bitmap_window_enabled {
            ; 行範囲制御が有効な場合、指定範囲のみクリア
            ubyte row
            for row in bitmap_top_row to bitmap_bottom_row {
                ; 各行のビットマップデータをクリア（1行は320バイト）
                uword row_addr = BITMAP_BASE + (row as uword) * 320
                sys.memset(row_addr, 320, 0)
                
                ; 各行のカラー情報をクリア
                uword color_row_addr = BITMAP_SCREEN_RAM + (row as uword) * 40
                sys.memset(color_row_addr, 40, (bitmap_fg_color << 4) | bitmap_bg_color)
            }
            
            ; カーソル位置を範囲の開始位置に設定
            bitmap_x = 0
            bitmap_y = bitmap_top_row
        } else {
            ; 行範囲制御が無効な場合、全画面クリア
            sys.memset(BITMAP_BASE, 8000, 0)
            sys.memset(BITMAP_SCREEN_RAM, 1000, (bitmap_fg_color << 4) | bitmap_bg_color)
            
            ; ビットマップ座標初期化
            bitmap_x = 0
            bitmap_y = 0
        }
        
        ; Shift-JISステートもクリア
        sjis_state = 0
        sjis_first_byte = 0
    }
    
    ; ビットマップ座標設定
    sub blocate(ubyte x, ubyte y) {
        bitmap_x = x
        bitmap_y = y
    }
    
    ; ビットマップ色設定
    sub bcolor(ubyte fg, ubyte bg) {
        bitmap_fg_color = fg & 15
        bitmap_bg_color = bg & 15
    }
    
    ; ビットマップ行範囲設定
    sub bwindow(ubyte top_row, ubyte bottom_row) {
        if top_row <= bottom_row and bottom_row <= 24 {
            bitmap_top_row = top_row
            bitmap_bottom_row = bottom_row
            bitmap_window_enabled = true
        }
    }
    
    ; ビットマップ行範囲制御無効化
    sub bwindow_disable() {
        bitmap_window_enabled = false
    }
    
    ; ビットマップ行範囲制御有効化
    sub bwindow_enable() {
        bitmap_window_enabled = true
    }
    
    ; ビットマップモード改行処理
    sub bnewline() {
        bitmap_x = 0
        
        if bitmap_window_enabled {
            ; 行範囲制御有効時
            if bitmap_y >= bitmap_bottom_row {
                ; 最下行の場合、スクロール
                bscroll_up()
                bitmap_y = bitmap_bottom_row
            } else {
                bitmap_y++
            }
        } else {
            ; 行範囲制御無効時
            if bitmap_y >= 24 {
                ; 24行目で止める（スクロールしない）
                bitmap_y = 24
            } else {
                bitmap_y++
            }
        }
    }
    
    ; ビットマップモードバックスペース処理
    sub bbackspace() {
        ; Shift-JIS 2バイト目待ち状態の場合はキャンセルのみ
        if sjis_state == 1 {
            sjis_state = 0
            sjis_first_byte = 0
            return
        }
        
        ; カーソル位置を1つ戻す
        if bitmap_x > 0 {
            ; 同じ行内での後退
            bitmap_x--
        } else {
            ; 行頭の場合、前の行の行末に移動
            ubyte min_row = if bitmap_window_enabled bitmap_top_row else 0
            if bitmap_y > min_row {
                bitmap_y--
                bitmap_x = 39  ; 前の行の最後の位置
            } else {
                ; 範囲の上端または画面の左上角の場合は何もしない
                return
            }
        }
        
        ; 行範囲制御が有効で、範囲外の場合は描画しない
        if bitmap_window_enabled {
            if bitmap_y < bitmap_top_row or bitmap_y > bitmap_bottom_row {
                return
            }
        }
        
        ; 現在位置に空白文字を描画（文字を消去）
        draw_font_to_bitmap(32, bitmap_x, bitmap_y)  ; 32 = スペース文字
    }
    
    ; ビットマップ画面を上に1行スクロール（1行ずつ同期版）
    sub bscroll_up() {
        if bitmap_window_enabled {
            ; 行範囲制御が有効な場合、指定範囲内のみスクロール
            ubyte row_w
            for row_w in bitmap_top_row to bitmap_bottom_row - 1 {
                ; ビットマップデータをコピー（1文字行分）
                uword src_addr_w = BITMAP_BASE + ((row_w + 1) as uword) * 320
                uword dest_addr_w = BITMAP_BASE + (row_w as uword) * 320
                sys.memcopy(src_addr_w, dest_addr_w, 320)
                
                ; 同じ行のカラー情報もすぐにコピー
                uword src_color_w = BITMAP_SCREEN_RAM + ((row_w + 1) as uword) * 40
                uword dest_color_w = BITMAP_SCREEN_RAM + (row_w as uword) * 40
                sys.memcopy(src_color_w, dest_color_w, 40)
            }
            
            ; 範囲内の最下行をクリア
            uword last_row_w = BITMAP_BASE + (bitmap_bottom_row as uword) * 320
            sys.memset(last_row_w, 320, 0)
            
            ; 範囲内の最下行のカラー情報をクリア
            uword last_color_w = BITMAP_SCREEN_RAM + (bitmap_bottom_row as uword) * 40
            sys.memset(last_color_w, 40, (bitmap_fg_color << 4) | bitmap_bg_color)
        } else {
            ; 行範囲制御が無効な場合、全画面スクロール
            ubyte row
            for row in 0 to 23 {
                ; ビットマップデータをコピー（1文字行分）
                uword src_addr = BITMAP_BASE + ((row + 1) as uword) * 320
                uword dest_addr = BITMAP_BASE + (row as uword) * 320
                sys.memcopy(src_addr, dest_addr, 320)
                
                ; 同じ行のカラー情報もすぐにコピー
                uword src_color_addr = BITMAP_SCREEN_RAM + ((row + 1) as uword) * 40
                uword dest_color_addr = BITMAP_SCREEN_RAM + (row as uword) * 40
                sys.memcopy(src_color_addr, dest_color_addr, 40)
            }
            
            ; 最下行（24行目）をクリア
            uword last_row_addr = BITMAP_BASE + 7680  ; 24 * 320
            sys.memset(last_row_addr, 320, 0)
            
            ; 最下行のカラー情報をクリア
            uword last_color_addr = SCREEN_RAM + 960  ; 24 * 40
            sys.memset(last_color_addr, 40, (bitmap_fg_color << 4) | bitmap_bg_color)
        }
    }
    
    
    ; 終了処理
    sub cleanup() {
        ; テキストモードに戻す
        if display_mode == BITMAP_MODE {
            set_mode(TEXT_MODE)
        }
        
        ; VICバンクを0に戻す
        @($DD00) = (@($DD00) & %11111100) | %00000011
        
        ; VICを元に戻す
        @($D018) = (@($D018) & %00001111) | %00010000
        
        ; MagicDeskをバンク0に戻す
        @(BANK_REG) = 0
    }
    
    ; 以下は内部関数（最適化：呼び出し回数最小化）
    
    ; キャラクタセットをROMからRAMにコピー
    sub copy_charset_to_ram() {
        uword i
        for i in 0 to 2047 {
            ubyte char_data = @(CHARSET_ROM + i)
            @(CHARSET_RAM + i) = char_data
        }
    }
    
    ; JIS X 0201半角文字データを指定アドレスに書き込み（低レベル関数）
    sub define_jisx0201(uword dest_addr, ubyte jisx0201_code) {
        uword font_offset = (jisx0201_code as uword) * 8
        @(BANK_REG) = 1
        
        ubyte row
        for row in 0 to 7 {
            @(dest_addr + row as uword) = @(ROM_BASE + font_offset + row as uword)
        }
        
        @(BANK_REG) = 0
    }
    
    ; Shift-JISコードから漢字ROMオフセットを計算
    sub sjis_to_offset(uword sjis_code) -> uword {
        uword @zp ch = msb(sjis_code)
        uword @zp ch2 = lsb(sjis_code)
        
        ; ShiftJIS -> Ku/Ten変換
        if ch <= $9f {
            if ch2 < $9f {
                ch = (ch as uword << 1) - $102
            } else {
                ch = (ch as uword << 1) - $101
            }
        } else {
            if ch2 < $9f {
                ch = (ch as uword << 1) - $182
            } else {
                ch = (ch as uword << 1) - $181
            }
        }

        if ch2 < $7f {
            ch2 -= $40
        } else if ch2 < $9f {
            ch2 -= $41
        } else {
            ch2 -= $9f
        }
        
        uword code = ch * 94 + ch2
        uword offset = code * 8 + JISX0208_OFFSET
        return offset
    }
    
    ; 指定アドレスにフォントデータを書き込み（汎用関数）
    sub define_font(uword dest_addr, uword code) {
        if code <= 255 {
            ; 1バイト文字（ASCII、半角カナ）
            define_jisx0201(dest_addr, lsb(code))
        } else {
            ; 2バイト文字（漢字）
            define_kanji(dest_addr, code)
        }
    }
    
    ; 文字コードに文字を定義（既存の関数、define_fontを利用）
    sub define_char(ubyte char_code, uword code) {
        define_font(CHARSET_RAM + (char_code as uword) * 8, code)
    }
    
    ; Shift-JIS漢字データを指定アドレスに書き込み（低レベル関数、MagicDesk用8KBバンク）
    sub define_kanji(uword dest_addr, uword sjis_code) {
        uword kanji_offset = sjis_to_offset(sjis_code)
        ubyte bank = (kanji_offset / 8192) as ubyte + 1
        uword in_bank_offset = kanji_offset % 8192
        
        @(BANK_REG) = bank
        
        uword rom_addr = ROM_BASE + in_bank_offset
        
        ubyte row
        for row in 0 to 7 {
            @(dest_addr + row as uword) = @(rom_addr + row as uword)
        }
        
        @(BANK_REG) = 0
    }
    
    ; ビットマップモードで1文字描画（txt.putc互換、Shift-JISステートフル処理）
    sub bputc(ubyte char_code) {
        ; Shift-JIS 2バイト目待ち状態の処理
        if sjis_state == 1 {
            ; 2バイト目として有効な範囲チェック
            if (char_code >= $40 and char_code <= $7E) or (char_code >= $80 and char_code <= $FC) {
                ; Shift-JIS文字として処理
                uword sjis_code = (sjis_first_byte as uword << 8) | char_code as uword
                bputc_internal(sjis_code)
                sjis_state = 0
                sjis_first_byte = 0
                return
            } else {
                ; 無効な2バイト目の場合、1バイト目を単独で出力してリセット
                bputc_internal(sjis_first_byte as uword)
                sjis_state = 0
                sjis_first_byte = 0
                ; そして現在の文字を処理続行
            }
        }
        
        ; Shift-JIS 1バイト目判定
        if (char_code >= $81 and char_code <= $9F) or (char_code >= $E0 and char_code <= $FC) {
            ; Shift-JIS 1バイト目として保存
            sjis_state = 1
            sjis_first_byte = char_code
            return
        }
        
        ; バックスペース処理
        if char_code == $08 {
            bbackspace()
            return
        }
        
        ; 改行コード処理
        ; if char_code == $0A or char_code == $0D {
        if char_code == $0D {
            bnewline()
            return
        }
        
        ; 通常の1バイト文字（半角カナ／ASCII）
        if (char_code >= $A1 and char_code <= $DF) or (char_code >= $20 and char_code <= $7E) {
            bputc_internal(char_code as uword)
        }
        ; その他の制御文字は無視
    }
    
    ; 内部用ビットマップ文字出力（実際の描画処理）
    sub bputc_internal(uword char_code) {
        ; 行範囲制御が有効で、範囲外の場合は何もしない
        if bitmap_window_enabled {
            if bitmap_y < bitmap_top_row or bitmap_y > bitmap_bottom_row {
                return
            }
        }
        
        ; 範囲制御無効時も24行目を超えたら何もしない
        if not bitmap_window_enabled and bitmap_y > 24 {
            return
        }
        
        ; ビットマップアドレスを計算して直接書き込み
        draw_font_to_bitmap(char_code, bitmap_x, bitmap_y)
        
        ; 次の文字位置へ
        bitmap_x++
        if bitmap_x >= 40 {
            ; 自動改行処理
            bnewline()
        }
    }
    
    ; ビットマップモードで文字列描画（bputcを使用してステートフル処理）
    sub bputs(uword addr) {
        uword ptr = addr
        
        while @(ptr) != 0 {
            bputc(@(ptr))
            ptr++
        }
    }
    
    ; フォントデータを直接ビットマップメモリに描画
    sub draw_font_to_bitmap(uword char_code, ubyte x, ubyte y) {
        ; ビットマップ上の位置を計算
        ; C64のビットマップレイアウト：
        ; - 画面は8行ごとのブロックに分かれる（0-7行、8-15行、16-23行、24行）
        ; - 各ブロック内で、文字は左から右、上から下の順に配置
        ; - 各文字は8バイト連続で格納
        
        ubyte char_row = y / 8            ; 文字のブロック行（0-3）
        ubyte char_line = y & 7           ; ブロック内の行（0-7）
        
        ; ビットマップアドレス計算
        ; ブロック開始 + ブロック内の行オフセット + 文字位置
        uword bitmap_addr = BITMAP_BASE +
                           (char_row as uword) * 320 * 8 +    ; ブロック行オフセット（各2560バイト）
                           (char_line as uword) * 320 +       ; ブロック内行オフセット
                           (x as uword) * 8                   ; X位置（各文字8バイト）
        
        ; カラー情報設定（ビットマップモード用画面RAMの該当位置）
        uword color_addr = BITMAP_SCREEN_RAM + (y as uword) * 40 + x as uword
        @(color_addr) = (bitmap_fg_color << 4) | bitmap_bg_color
        
        ; フォントデータを直接ビットマップに書き込み
        define_font(bitmap_addr, char_code)
    }
    
    ; 文字列リソースを内部バッファに読み込み（共通処理）
    sub load_string_resource(ubyte resource_number) -> bool {
        ; 文字列リソースバンクに切り替え
        @(BANK_REG) = STRING_RESOURCE_BANK
        
        ; 文字列の個数を取得（リトルエンディアン4バイト）
        uword num_strings = @(STRING_RESOURCE_BASE) as uword | 
                           (@(STRING_RESOURCE_BASE + 1) as uword << 8)
        
        ; 範囲チェック
        if resource_number >= lsb(num_strings) {
            @(BANK_REG) = 0
            return false  ; 範囲外
        }
        
        ; オフセットテーブルから情報を読み取り（各エントリ4バイト、MagicDesk用）
        uword offset_table_addr = STRING_RESOURCE_BASE + 4 + (resource_number as uword) * 4
        ubyte target_bank = @(offset_table_addr)
        ubyte reserved = @(offset_table_addr + 1)       ; MagicDeskでは予約領域
        uword string_offset = @(offset_table_addr + 2) as uword | 
                             (@(offset_table_addr + 3) as uword << 8)
        
        ; 未定義の文字列リソースチェック
        if target_bank == 0 and string_offset == 0 {
            @(BANK_REG) = 0
            return false  ; 未定義
        }
        
        ; 文字列データのあるバンクに切り替え
        @(BANK_REG) = target_bank
        
        ; 文字列データのアドレス（MagicDeskは8KBバンクのみ）
        uword string_addr = ROM_BASE + string_offset
        
        ; 文字列をRAMバッファにコピー（8KBバンク境界またぎ対応）
        ubyte buffer_pos = 0
        ubyte current_bank = target_bank
        uword current_addr = string_addr
        
        while buffer_pos < STRING_BUFFER_SIZE {
            ; 現在のアドレスをチェックして8KB境界をまたぐかどうか判定
            if current_addr > $9FFF {
                ; 8KB境界を超えた場合、次のバンクに移動
                current_bank++
                current_addr = ROM_BASE     ; ROM開始アドレスに移動
                @(BANK_REG) = current_bank
            }
            
            ubyte char_data = @(current_addr)
            @(STRING_BUFFER + buffer_pos as uword) = char_data
            
            if char_data == 0 {
                break  ; NULL終端
            }
            buffer_pos++
            current_addr++
        }
        
        ; バッファが満杯の場合はNULL終端を追加
        if buffer_pos >= STRING_BUFFER_SIZE {
            @(STRING_BUFFER + STRING_BUFFER_SIZE as uword) = 0
        }
        
        ; バンク0に戻す（フォント用）
        @(BANK_REG) = 0
        return true
    }
    
    ; 文字列リソース表示（番号指定）
    sub putr(ubyte resource_number) {
        if load_string_resource(resource_number) {
            puts(STRING_BUFFER)
        }
    }
    
    ; ビットマップモードで文字列リソース表示
    sub bputr(ubyte resource_number) {
        if load_string_resource(resource_number) {
            bputs(STRING_BUFFER)
        }
    }
    
    ; Shift-JIS 1バイト目かどうかを判定
    sub is_firstsjis(ubyte char) -> bool {
        return (char >= $81 and char <= $9F) or (char >= $E0 and char <= $FC)
    }

    ; 16進数文字列（2桁）を出力
    sub bput_hex2(ubyte value) {
        ubyte hi = value >> 4
        ubyte lo = value & 15
        if hi < 10 {
            bputc(iso:'0' + hi)
        } else {
            bputc(iso:'A' + hi - 10)
        }
        if lo < 10 {
            bputc(iso:'0' + lo)
        } else {
            bputc(iso:'A' + lo - 10)
        }
    }

    ; 10進数（2桁）を出力
    sub bput_dec2(ubyte value) {

        bputc(iso:'0' + value / 10)
        bputc(iso:'0' + (value % 10))
    }

    ; 10進数（3桁）を出力
    sub bput_dec3(ubyte value) {
        bputc(iso:'0' + value / 100)
        bputc(iso:'0' + (value / 10) % 10)
        bputc(iso:'0' + (value % 10))
    }
}