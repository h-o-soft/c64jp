
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

    uword @zp define_addr               ; フォント定義アドレス

    ; Shift-JISステート管理（sjis_first_byte!=0 で2バイト目待ちを表す）
    ubyte sjis_first_byte = 0           ; 0=通常, 非0=Shift-JIS 1バイト目保存
    
    ; ビットマップモード関連変数
    ubyte display_mode = TEXT_MODE      ; 現在の表示モード
    ubyte cursor_x = 0                  ; ビットマップ X座標（文字単位 0-39）
    ubyte cursor_y = 0                  ; ビットマップ Y座標（文字単位 0-24）
    ubyte bitmap_color = (1 << 4) | 0   ; ビットマップ色
    
    ; ビットマップ行範囲制御変数
    ubyte bitmap_top_row = 0            ; 描画開始行（デフォルト: 0）
    ubyte bitmap_bottom_row = 24        ; 描画終了行（デフォルト: 24）
    bool bitmap_window_enabled = false  ; ビットマップ行範囲制御有効フラグ
    
    ; ハードウェア初期化（モード設定のみ、文字範囲はデフォルトを使用）
    sub init(ubyte mode) {
        ; 表示モード設定（chr_start/chr_count はデフォルト値を使用）
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
    
    ; 画面クリア（全領域を無条件にスペースで埋める）
    sub cls() {
        sys.memset(SCREEN_RAM, 1000, 32)  ; 25行×40文字をスペースでクリア
        
        ; インデックス初期化
        current_index = chr_start
        locate(0, 0)
        
        ; Shift-JISステートもクリア
        sjis_first_byte = 0
    }
    
    ; カーソル位置設定
    sub locate(ubyte x, ubyte y) {
        screen_pos = SCREEN_RAM + (y as uword) * CHAR_WIDTH + x as uword
        color_pos = screen_pos + ($D800 - SCREEN_RAM)
    }
    
    ; 1文字出力（txt.putc互換、Shift-JISステートフル処理）
    sub putc(ubyte char_code) {
        ; Shift-JIS 2バイト目待ち状態の処理（sjis_first_byte!=0）
        if sjis_first_byte != 0 {
            ; 2バイト目として有効な範囲チェック
            if (char_code >= $40 and char_code <= $7E) or (char_code >= $80 and char_code <= $FC) {
                ; Shift-JIS文字として処理
                uword sjis_code = (sjis_first_byte as uword << 8) | char_code as uword
                putc_internal(sjis_code)
                sjis_first_byte = 0
                return
            } else {
                ; 無効な2バイト目の場合、1バイト目を単独で出力してリセット
                putc_internal(sjis_first_byte as uword)
                sjis_first_byte = 0
                ; そして現在の文字を処理続行
            }
        }
        
        ; Shift-JIS 1バイト目判定
        if (char_code >= $81 and char_code <= $9F) or (char_code >= $E0 and char_code <= $FC) {
            ; Shift-JIS 1バイト目として保存
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
    
    ; 文字列出力
    sub puts(uword addr) {
        %asm {{
-
            ldy  #0
            lda  (p8v_addr),y
            cmp  #0
            beq  _puts_done
            jsr  p8b_jtxt.p8s_putc
            inc  p8v_addr
            bne  +
            inc  p8v_addr+1
+
            jmp  -
_puts_done
        }}
    }
    
    ; 改行処理
    ; ※単純にx=0、y++してlocateした方がいいかも
    sub newline() {
        ; 現在の画面位置から画面先頭アドレスを引いて相対位置を取得
        screen_pos -= SCREEN_RAM
        ; 40で割って現在行を求める
        screen_pos /= 40
        if screen_pos < 24 {
            screen_pos++
        }
        screen_pos *= 40
        ; 次の行の先頭アドレスを計算
        screen_pos += SCREEN_RAM
        color_pos = screen_pos + ($D800 - SCREEN_RAM)
    }

    ; 文字色設定（最適化：単純代入）
    asmsub set_color(ubyte color @ A) {
        ; current_color = color & 15  ; 下位4ビットのみ有効
        %asm {{
            and  #15
            sta  p8b_jtxt.p8v_current_color
            rts
        }}
    }
    
    ; 背景色/ボーダー色設定
    asmsub set_bgcolor(ubyte bgcolor @ A, ubyte bordercolor @ Y) {
        %asm {{
            ; 背景色
            and  #15
            sta  $d021

            ; ボーダー色
            tya
            and  #15
            sta  $d020
            rts
        }}
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
            @($DD00) = (@($DD00) & %11111100) | %00000011   ; VICバンク1選択

            @($D018) = (@($D018) & %11110000) | %00001100   ; キャラクタRAM=$3000
        }
    }
    
    ; ビットマップ画面クリア
    sub bcls() {
        ; 行範囲制御が有効な場合、指定範囲のみクリア
        ubyte row
        cx16.r2 = BITMAP_BASE + (bitmap_top_row as uword) * 320
        cx16.r3 = BITMAP_SCREEN_RAM + (bitmap_top_row as uword) * 40
        for row in bitmap_top_row to bitmap_bottom_row {
            ; 各行のビットマップデータをクリア（1行は320バイト）
            sys.memset(cx16.r2, 320, 0)
            cx16.r2 += 320

            ; 各行のカラー情報をクリア
            sys.memset(cx16.r3, 40, bitmap_color)
            cx16.r3 += 40
        }
        
        ; カーソル位置を範囲の開始位置に設定
        cursor_x = 0
        cursor_y = bitmap_top_row
        
        ; Shift-JISステートもクリア
        sjis_first_byte = 0
    }
    
    ; ビットマップ座標設定
    asmsub blocate(ubyte x @ A, ubyte y @ Y) {
        %asm {{
            sta  p8b_jtxt.p8v_cursor_x
            sty  p8b_jtxt.p8v_cursor_y
            rts
        }}
    }
    
    ; ビットマップ色設定
    asmsub bcolor(ubyte fg @ A, ubyte bg @ Y) clobbers(A) {
        %asm {{
            sty  cx16.r2
            and  #15
            asl  a
            asl  a
            asl  a
            asl  a
            ora  cx16.r2
            sta  p8b_jtxt.p8v_bitmap_color
            rts
        }}
    }
    
    ; ビットマップ行範囲設定
    asmsub bwindow(ubyte top_row @ A, ubyte bottom_row @ Y) {
        ; ignore top_row <= bottom_row and bottom_row <= 24
        ; if top_row <= bottom_row and bottom_row <= 24 {
        %asm {{
            sta  p8b_jtxt.p8v_bitmap_top_row
            sty  p8b_jtxt.p8v_bitmap_bottom_row
            rts
        }}
        ; }
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
        cursor_x = 0
        
        ; 行範囲制御有効時
        if cursor_y >= bitmap_bottom_row {
            ; 最下行の場合、スクロール(bitmap_window_enabled=trueの場合)
            if bitmap_window_enabled {
                bscroll_up()
            }
            cursor_y = bitmap_bottom_row
        } else {
            cursor_y++
        }
    }
    
    ; ビットマップモードバックスペース処理
    sub bbackspace() {
        ; Shift-JIS 2バイト目待ち状態の場合はキャンセルのみ
        if sjis_first_byte != 0 {
            sjis_first_byte = 0
            return
        }
        
        ; カーソル位置を1つ戻す
        if cursor_x != 0 {
            ; 同じ行内での後退
            cursor_x--
        } else {
            ; 行頭の場合、前の行の行末に移動
            if cursor_y > bitmap_top_row {
                cursor_y--
                cursor_x = 39  ; 前の行の最後の位置
            } else {
                ; 範囲の上端または画面の左上角の場合は何もしない
                return
            }
        }
        
        ; 行範囲制御が有効で、範囲外の場合は描画しない
        if cursor_y < bitmap_top_row or cursor_y > bitmap_bottom_row {
            return
        }
        
        ; 現在位置に空白文字を描画（文字を消去）
        draw_font_to_bitmap(32)  ; 32 = スペース文字
    }
    
    ; ビットマップ画面を上に1行スクロール
    sub bscroll_up() {
        ; 行範囲制御が有効な場合、指定範囲内のみスクロール
        cx16.r3 = BITMAP_BASE + (bitmap_top_row as uword) * 320         ; dst
        cx16.r2 = cx16.r3 + 320                                         ; src
        cx16.r5 = BITMAP_SCREEN_RAM + (bitmap_top_row as uword) * 40    ; dst
        cx16.r4 = cx16.r5 + 40                                          ; src
        repeat bitmap_bottom_row - bitmap_top_row {
            ; ビットマップデータをコピー（1行分）
            sys.memcopy(cx16.r2, cx16.r3, 320)
            cx16.r2 += 320
            cx16.r3 += 320
            
            ; 同じ行のカラー情報もすぐにコピー
            sys.memcopy(cx16.r4, cx16.r5, 40)
            cx16.r4 += 40
            cx16.r5 += 40
        }
        
        ; 範囲内の最下行をクリア
        sys.memset(cx16.r3, 320, 0)         ; r3 = 最下行になっている(はず)
        
        ; 範囲内の最下行のカラー情報をクリア
        sys.memset(cx16.r5, 40, bitmap_color)     ; r5 = 最下行になっている(はず)
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
    asmsub copy_charset_to_ram() {
        %asm {{
            ; Set ROM Pointer
            lda #<p8c_CHARSET_ROM
            sta cx16.r2
            lda #>p8c_CHARSET_ROM
            ; lda #$d8    ; char set 2
            sta cx16.r2 + 1

            ; Set RAM Pointer
            lda #<p8c_CHARSET_RAM
            sta cx16.r3
            lda #>p8c_CHARSET_RAM
            sta cx16.r3 + 1

            ; Copy ROM to RAM
            ; $d000 -> $3000
            ldx #$08            ; 8 pages of 256 bytes = 2KB
            ldy #$00
-
            lda (cx16.r2),y     ; read byte from vector stored in cx16.r2/cx16.r2+1
            sta (cx16.r3),y     ; write to the RAM
            iny                 ; do this 255 times...
            bne -               ;  ..for low byte $00 to $FF

            inc cx16.r2 + 1     ; Increase high bytes
            inc cx16.r3 + 1
            dex                 ; decrease X by one
            bne -
            rts
        }}
    }
    
    ; JIS X 0201半角文字データを指定アドレスに書き込み（低レベル関数）
    asmsub define_jisx0201(ubyte jisx0201_code @ A) {
        %asm {{
            ldy  #0
            sty  P8ZP_SCRATCH_B1
            asl  a
            rol  P8ZP_SCRATCH_B1
            asl  a
            rol  P8ZP_SCRATCH_B1
            asl  a
            rol  P8ZP_SCRATCH_B1
            ldy  P8ZP_SCRATCH_B1
            clc
            adc  #<p8c_ROM_BASE
            tax
            tya
            adc  #>p8c_ROM_BASE
            tay
            txa
            sta  cx16.r2
            sty  cx16.r2 + 1

            lda  #1
            sta  p8c_BANK_REG

            lda  #8
            sta  define_jisx0201_tempv
-
            ldy  #0
            lda  (cx16.r2),y
            sta  (p8b_jtxt.p8v_define_addr),y
            inc  p8b_jtxt.p8v_define_addr
            bne  +
            inc  p8b_jtxt.p8v_define_addr+1
+
            inc  cx16.r2
            bne  +
            inc  cx16.r2 + 1
+
            dec  define_jisx0201_tempv
            bne  -

            lda  #0
            sta  p8c_BANK_REG
            rts

            .section BSS_NOCLEAR
define_jisx0201_tempv    .byte  ?
            .send BSS_NOCLEAR
; !notreached!
        }}
    }
    
    ; Shift-JISコードから漢字ROMオフセットを計算
    sub sjis_to_offset(uword sjis_code) -> uword {
        ubyte @zp ch = msb(sjis_code)
        ubyte @zp ch2 = lsb(sjis_code)
        
        ; ShiftJIS -> Ku/Ten変換
        uword row = (ch as uword) << 1
        if ch <= $9f {
            row -= $0102
        } else {
            row -= $0182
        }
        if ch2 >= $9f {
            row++
        }
        ch = lsb(row)

        if ch2 < $7f {
            ch2 -= $40
        } else if ch2 < $9f {
            ch2 -= $41
        } else {
            ch2 -= $9f
        }
        
        return ((ch as uword) * 94 + ch2 as uword)* 8 + JISX0208_OFFSET
    }
    
    ; 指定アドレスにフォントデータを書き込み（汎用関数）
    sub define_font(uword dest_addr, uword code) {
        define_addr = dest_addr
        if msb(code) == 0 {
            ; 1バイト文字（ASCII、半角カナ）
            define_jisx0201(lsb(code))
        } else {
            ; 2バイト文字（漢字）
            define_kanji(code)
        }
    }
    
    ; 文字コードに文字を定義（既存の関数、define_fontを利用）
    sub define_char(ubyte char_code, uword code) {
        define_font(CHARSET_RAM + (char_code as uword) * 8, code)
    }
    
    ; Shift-JIS漢字データを指定アドレスに書き込み（低レベル関数、MagicDesk用8KBバンク）
    sub define_kanji(uword sjis_code) {
        uword kanji_offset = sjis_to_offset(sjis_code)
        ubyte bank = (kanji_offset / 8192) as ubyte + 1
        uword in_bank_offset = kanji_offset % 8192
        
        @(BANK_REG) = bank
        
        uword rom_addr = ROM_BASE + in_bank_offset
        
        ubyte row
        for row in 0 to 7 {
            @(define_addr + row as uword) = @(rom_addr + row as uword)
        }
        
        @(BANK_REG) = 0
    }
    
    ; ビットマップモードで1文字描画（txt.putc互換、Shift-JISステートフル処理）
    sub bputc(ubyte char_code) {
        ; Shift-JIS 2バイト目待ち状態の処理（sjis_first_byte!=0）
        if sjis_first_byte != 0 {
            ; 2バイト目として有効な範囲チェック
            if (char_code >= $40 and char_code <= $7E) or (char_code >= $80 and char_code <= $FC) {
                ; Shift-JIS文字として処理
                uword sjis_code = (sjis_first_byte as uword << 8) | char_code as uword
                bputc_internal(sjis_code)
                sjis_first_byte = 0
                return
            } else {
                ; 無効な2バイト目の場合、1バイト目を単独で出力してリセット
                bputc_internal(sjis_first_byte as uword)
                sjis_first_byte = 0
                ; そして現在の文字を処理続行
            }
        }
        
        ; Shift-JIS 1バイト目判定
        if (char_code >= $81 and char_code <= $9F) or (char_code >= $E0 and char_code <= $FC) {
            ; Shift-JIS 1バイト目として保存
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
        if bitmap_window_enabled and (cursor_y < bitmap_top_row or cursor_y > bitmap_bottom_row) {
            return
        }
        
        ; ビットマップアドレスを計算して直接書き込み
        draw_font_to_bitmap(char_code)
        
        ; 次の文字位置へ
        cursor_x++
        if cursor_x >= 40 {
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
    sub draw_font_to_bitmap(uword char_code) {
        ; TODO 毎回cursor_x, cursor_yからアドレスを計算するのは無駄なのでそのうち最適化を検討したい

        ; カラー情報設定（ビットマップモード用画面RAMの該当位置）
        @(BITMAP_SCREEN_RAM + (cursor_y as uword) * 40 + cursor_x as uword) = bitmap_color
        
        ; フォントデータを直接ビットマップに書き込み
        define_font(BITMAP_BASE + ((cursor_y / 8) as uword) * 320 * 8 + ((cursor_y & 7) as uword) * 320 + (cursor_x as uword) * 8, char_code)
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
