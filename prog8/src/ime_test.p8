; ==============================================================================
; ime_test.p8 - ローマ字→かな変換テストプログラム（ビットマップモード版）
; ==============================================================================

%import syslib
%import textio
%import conv
%import strings
%import jtxt
%import ime

main {
    sub start() {
        ; IME初期化
        ime.init()
        
        ; jtxtライブラリ初期化（文字範囲64-254、190文字使用、ビットマップモード）
        jtxt.init(64, 190, jtxt.BITMAP_MODE)

        ; タイトル表示
        show_title()
        
        ; ウィンドウ設定（1行目から23行目まで）
        jtxt.bwindow(1, 23)
        
        ; シンプル入力テスト実行
        simple_input_test()
        
        ; 終了処理
        cleanup()
    }
    
    sub show_title() {
        ; タイトルバー
        jtxt.bcolor(1, 0)  ; 白文字、黒背景
        jtxt.blocate(0, 0)
        
        ; タイトル表示
        jtxt.bputs(iso:"IME TEST v1.0")
        
        ; 残りのスペースを埋める
        ubyte x
        for x in 32 to 39 {
            jtxt.bputc(32)  ; スペース
        }
    }
    
    ; シンプル入力テスト（ノンブロッキング型）
    sub simple_input_test() {
        bool exit_requested = false
        ubyte frame_counter = 0
        
        ; BASIC風の色設定（青い背景、白い文字）
        jtxt.bwindow(1, 23)     ; 入力エリアの制限を設定（タイトル保護）
        jtxt.bcolor(1, 6)       ; 白文字、青背景
        jtxt.bcls()             ; 色設定を反映するため画面クリア（制限範囲内のみ）
        
        ; メインループ活動インジケーター初期化
        ; ビットマップメモリ右上角にインジケーターを描画
        ; ビットマップベース=$6000、右上角の位置(39*8, 0)付近を使用
        uword indicator_bitmap = $6000 + (39 * 8); 右上角のバイト位置
        uword indicator_color = $5C00 + 39     ; ビットマップスクリーンRAM右上角
        @(indicator_color) = $02  ; 赤色に設定（上位4bit=背景、下位4bit=前景）
        
        ; メインループ
        repeat {
            ; IME処理（内部でトグル処理も行う）
            ubyte event = ime.process()
            
            when event {
                ime.IME_EVENT_CONFIRMED -> {
                    ; 確定文字列を出力
                    uword text = ime.get_result_text()
                    if text != 0 {
                        ubyte text_len = ime.get_result_length()
                        ubyte i
                        for i in 0 to text_len-1 {
                            jtxt.bputc(@(text+i))
                        }
                    }
                    ; 結果バッファをクリア
                    ime.clear_ime_output()
                }
                ime.IME_EVENT_CANCELLED -> {
                    ; キャンセル処理（特に何もしない）
                }
                ime.IME_EVENT_DEACTIVATED -> {
                    ; IMEが無効化された（Commodore+Spaceで）
                    ; カーソル位置を保存
                    ubyte saved_x = jtxt.bitmap_x
                    ubyte saved_y = jtxt.bitmap_y
                    
                    ; 最下行（24行目）をクリア
                    jtxt.bwindow_disable()  ; 一時的に制限解除
                    jtxt.blocate(0, 24)
                    ubyte col
                    for col in 0 to 39 {
                        jtxt.bputc(32)  ; スペースで埋める
                    }
                    jtxt.bwindow(1, 23)  ; ウィンドウ制限を復帰
                    
                    ; カーソル位置を復帰
                    jtxt.blocate(saved_x, saved_y)
                }
                ime.IME_EVENT_MODE_CHANGED -> {
                    ; モード変更（F1/F3/F5で）
                }
                ime.IME_EVENT_KEY_PASSTHROUGH -> {
                    ; IMEからのパススルーキー処理
                    ubyte passthrough_key = ime.get_passthrough_key()
                    when passthrough_key {
                        20 -> {  ; BackSpace
                            ; 1文字戻る（簡易実装）
                            if jtxt.bitmap_x > 0 {
                                jtxt.bitmap_x--
                                jtxt.bputc(32)  ; スペースで消去
                                jtxt.bitmap_x--
                            }
                        }
                        13 -> {  ; Return
                            jtxt.bnewline()
                        }
                    }
                }
                ime.IME_EVENT_NONE -> {
                    ; IME無効時の通常キー処理
                    ubyte key = cbm.GETIN2()
                    
                    if key != 0 {
                        when key {
                            27 -> {  ; ESC - 終了
                                exit_requested = true
                            }
                            8, 20 -> {  ; BS/DEL
                                jtxt.bputc(8)
                            }
                            13 -> {  ; RETURN
                                jtxt.bputc(13)
                            }
                            else -> {
                                ; 直接文字出力
                                if key >= 32 and key <= 126 {
                                    jtxt.bputc(key)
                                }
                            }
                        }
                    }
                }
            }
            
            if exit_requested break
            
            ; メインループ活動インジケーター更新(IMEが重い変換などをしている間は動かない)
            frame_counter++
            ; 8フレームごとにパターン変更（約8/50秒 = 0.16秒間隔）
            ubyte indicator_pattern = frame_counter >> 3  ; 8で割った値
            ubyte pattern
            when indicator_pattern & 3 {  ; 4パターンでサイクル
                0 -> pattern = %11111111  ; 全ビット点灯
                1 -> pattern = %10101010  ; 縦縞
                2 -> pattern = %00000000  ; 全消灯
                3 -> pattern = %01010101  ; 逆縦縞
            }
            for i in 0 to 7 {
                @(indicator_bitmap + i) = pattern
            }
            
            ; フレーム制御（ノンブロッキング処理のため）
            sys.wait(1)
        }
    }
    
    ; 終了処理
    sub cleanup() {
        ; IMEを無効化（念のため）
        if ime.is_ime_active() {
            ime.toggle_ime_mode()
        }
        
        ; テキストモードに戻す
        jtxt.set_mode(jtxt.TEXT_MODE)
    }
}
