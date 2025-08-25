%import textio
%import syslib
%import jtxt
%import swiftlink
%import ime

; SwiftLinkモデムテストプログラム（ビットマップモード日本語ターミナル）
main {
    ; 設定定数
    const ubyte INPUT_BUFFER_SIZE = 80
    const ubyte DISPLAY_WIDTH = 40
    const ubyte DISPLAY_HEIGHT = 25
    
    ; 状態変数
    ubyte[] input_buffer = [0] * INPUT_BUFFER_SIZE
    ubyte input_pos = 0
    ubyte display_line = 0
    ubyte cursor_x = 0        ; カーソルX座標（改行で0、それ以外は加算）
    ubyte cursor_y = 9        ; 現在のカーソルY座標
    bool modem_connected = false
    bool echo_mode = false
    
    sub start() {
        ; IME初期化
        ime.init()
        
        ; ライブラリ初期化（文字範囲64-254、190文字使用、ビットマップモード）
        jtxt.init(64, 190, jtxt.BITMAP_MODE)
        
        ; 画面設定
        jtxt.bcolor(5, 0)  ; 緑文字、黒背景（ターミナル風）
        jtxt.bcls()
        
        ; タイトル表示
        show_title()
        
        ; モデム初期化
        initialize_modem()

        ; メインループ
        main_loop()
        
        ; 終了処理
        cleanup()
    }
    
    sub show_title() {
        ; 行範囲設定（0-23行目をメインエリア、24行目をステータスライン）
        jtxt.bwindow(0, 23)
        
        ; タイトルバー
        jtxt.bcolor(0, 5)  ; 黒文字、緑背景（反転）
        jtxt.blocate(0, 0)
        
        ; タイトル文字列を1文字ずつ出力
        ubyte[] title1 = [83, 87, 73, 70, 84, 76, 73, 78, 75, 32, 77, 79, 68, 69, 77, 32, 84, 69, 82, 77, 73, 78, 65, 76, 32, 86, 49, 46, 48, 0]  ; "SWIFTLINK MODEM TERMINAL V1.0"
        ubyte idx = 0
        while title1[idx] != 0 and idx < 30 {
            jtxt.bputc(title1[idx])
            idx++
        }
        
        ; 残りのスペースを埋める
        while idx < 40 {
            jtxt.bputc(32)  ; スペース
            idx++
        }
        
        ; 通常色に戻す
        jtxt.bcolor(5, 0)
        
        ; 日本語タイトル
        jtxt.blocate(0, 2)
        ubyte[] title2 = [$83, $82, $83, $66, $83, $80, $83, $5E, $81, $5B, $83, $7E, $83, $69, $83, $8B, 0]  ; "モデムターミナル"
        ubyte i = 0
        while title2[i] != 0 {
            jtxt.bputc(title2[i])
            i++
        }
        
        ; 操作説明
        jtxt.blocate(0, 4)
        show_message(iso:"COMMANDS:", 3)  ; シアン
        
        jtxt.blocate(0, 5)
        jtxt.bcolor(7, 0)  ; 黄色
        jtxt.bputs(iso:"F1: INIT MODEM    F3: AT COMMAND")
        
        jtxt.blocate(0, 6)
        jtxt.bputs(iso:"F5: ECHO ON/OFF   F7: CLEAR SCREEN")
        
        jtxt.blocate(0, 7)
        jtxt.bputs(iso:"C64+SPACE: IME    ESC: EXIT PROGRAM")
        
        ; 区切り線
        jtxt.bcolor(5, 0)
        jtxt.blocate(0, 8)
        ubyte x
        for x in 0 to 39 {
            jtxt.bputc(45)  ; '-'
        }
        
        ; 初期ステータスライン（24行目）
        draw_status_line()
        
        ; 初期表示位置設定
        display_line = 9
        cursor_x = 0
        cursor_y = display_line
        jtxt.blocate(0, display_line)
    }
    
    sub initialize_modem() {
        show_status(iso:"INITIALIZING MODEM...", 14)  ; ライトブルー
        
        ; モデム初期化（19200bps, 8N1, NMI使用）
        swiftlink.init(swiftlink.BAUD_19200, swiftlink.CTRL_DATA_8, swiftlink.CTRL_STOP_1, swiftlink.CMD_PARITY_DISABLE, true)
        
        ; ATコマンドで確認
        if send_at_command(iso:"AT") {
            show_status(iso:"MODEM INITIALIZED (19200,8N1)", 5)  ; 緑
            modem_connected = true
        } else {
            show_status(iso:"MODEM NOT RESPONDING!", 2)  ; 赤
            modem_connected = false
        }
    }
    
    sub main_loop() {
        bool running = true
        
        while running {
            ; IME処理（内部でキー処理とトグル処理も行う）
            ubyte event = ime.process()
            
            when event {
                ime.IME_EVENT_CONFIRMED -> {
                    ; 確定文字列を出力（モデムにも送信）
                    uword text = ime.get_result_text()
                    if text != 0 {
                        ubyte text_len = ime.get_result_length()
                        ubyte i
                        for i in 0 to text_len-1 {
                            ubyte char = @(text+i)
                            ; 画面に表示（エコーモード時のみ）
                            if echo_mode {
                                jtxt.bputc(char)
                            }
                            ; モデムにも送信（接続時）
                            if modem_connected {
                                void swiftlink.send_byte(char)
                            }
                        }
                        if echo_mode {
                            cursor_x = jtxt.bitmap_x
                            cursor_y = jtxt.bitmap_y
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
                    ; ステータスラインをクリア
                    clear_status_line()
                }
                ime.IME_EVENT_MODE_CHANGED -> {
                    ; モード変更（F1/F3/F5で）
                }
                ime.IME_EVENT_KEY_PASSTHROUGH -> {
                    ; IMEからのパススルーキー処理
                    ubyte passthrough_key = ime.get_passthrough_key()
                    when passthrough_key {
                        27 -> {  ; ESC - 終了
                            running = false
                        }
                        133 -> {  ; F1 - モデム初期化
                            initialize_modem()
                        }
                        134 -> {  ; F3 - ATコマンド送信
                            send_at_dialog()
                        }
                        135 -> {  ; F5 - エコー切り替え
                            toggle_echo()
                        }
                        136 -> {  ; F7 - 画面クリア
                            clear_display()
                        }
                        13 -> {  ; RETURN
                            if echo_mode {
                                handle_newline()
                            }
                            ; モデムにCR送信（接続時）
                            if modem_connected {
                                void swiftlink.send_byte(13)
                            }
                        }
                        8, 20 -> {  ; BS/DEL
                            if echo_mode {
                                jtxt.bputc(8)
                            }
                            ; モデムにBS送信（接続時）
                            if modem_connected {
                                void swiftlink.send_byte(8)
                            }
                        }
                    }
                }
                ime.IME_EVENT_NONE -> {
                    ; IME無効時の通常キー処理
                    ubyte key = cbm.GETIN2()
                    
                    if key != 0 {
                        when key {
                            27 -> {  ; ESC - 終了
                                running = false
                            }
                            133 -> {  ; F1 - モデム初期化
                                initialize_modem()
                            }
                            134 -> {  ; F3 - ATコマンド送信
                                send_at_dialog()
                            }
                            135 -> {  ; F5 - エコー切り替え
                                toggle_echo()
                            }
                            136 -> {  ; F7 - 画面クリア
                                clear_display()
                            }
                            13 -> {  ; RETURN
                                if echo_mode {
                                    handle_newline()
                                }
                                ; モデムにCR送信（接続時）
                                if modem_connected {
                                    void swiftlink.send_byte(13)
                                }
                            }
                            8, 20 -> {  ; BS/DEL
                                if echo_mode {
                                    jtxt.bputc(8)
                                }
                                ; モデムにBS送信（接続時）
                                if modem_connected {
                                    void swiftlink.send_byte(8)
                                }
                            }
                            else -> {
                                ; 通常文字（ASCII印字可能文字）
                                if key >= 32 and key <= 126 {
                                    if echo_mode {
                                        jtxt.bputc(key)
                                        cursor_x++
                                        cursor_y = jtxt.bitmap_y
                                    }
                                    ; モデムに送信（接続時）
                                    if modem_connected {
                                        void swiftlink.send_byte(key)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            ; モデムからの受信チェック（ノンブロッキング、バッファ空まで処理）
            if modem_connected {
                ; 受信バッファにあるデータを全て処理（制限なし）
                while true {
                    uword received = swiftlink.receive_byte_nonblocking()
                    if received >= 256 {
                        break  ; データなし
                    }
                    
                    ; データ受信
                    ubyte data = lsb(received)
                    
                    ; 受信データを表示（Shift-JIS対応）
                    ; LF(10)は無視
                    if data != 10 {
                        if data == 13 {
                            ; CR(13)の場合、40の倍数位置でなければ表示
                            if (cursor_x % 40) != 0 {
                                jtxt.bcolor(3, 0)  ; シアン
                                jtxt.bputc(data)  ; CRを表示（これが改行処理）
                                jtxt.bcolor(5, 0)
                                cursor_x = 0      ; カーソル位置リセット
                                cursor_y = jtxt.bitmap_y  ; 同期
                            }
                            ; 40の倍数位置なら何もしない（自動改行済み）
                        } else if data == 8 {
                            ; BS(8)の場合
                            jtxt.bcolor(3, 0)  ; シアン
                            jtxt.bputc(data)  ; BSを処理（bbackspace()が呼ばれる）
                            jtxt.bcolor(5, 0)  ; 緑に戻す
                            ; カーソル位置を1つ戻す
                            if cursor_x > 0 {
                                cursor_x--
                            } else {
                                ; 行頭の場合、前の行の行末に移動
                                cursor_x = 39
                            }
                        } else {
                            ; 通常文字の場合
                            jtxt.bcolor(3, 0)  ; シアンで受信データ表示
                            jtxt.bputc(data)
                            jtxt.bcolor(5, 0)  ; 緑に戻す
                            cursor_x++  ; 単純に加算
                            cursor_y = jtxt.bitmap_y  ; 同期
                        }
                    }
                }
            }
            
            ; フレーム制御（1フレーム待ち）
            sys.wait(1)
        }
    }
    
    sub process_input() {
        ; この関数は1文字ずつ送信に変更したため、基本的に使用されない
        ; ATコマンド送信などでのみ使用される場合があるため残している
        
        ; 入力バッファクリア
        input_pos = 0
        input_buffer[0] = 0
        
        ; 改行
        handle_newline()
    }
    
    sub send_at_command(uword command) -> bool {
        show_message(iso:"SEND: ", 7)
        show_message(command, 7)
        handle_newline()
        
        ; ATコマンド送信
        bool result = swiftlink.send_at_command(command)
        
        ; レスポンス待ち（簡易版）
        if result {
            sys.wait(30)  ; 0.5秒待つ
            
            ; レスポンス受信
            ubyte response_count = 0
            uword received
            
            while response_count < 100 {
                received = swiftlink.receive_byte_nonblocking()
                if received < 256 {
                    ubyte data = lsb(received)
                    ; LF(10)は無視
                    if data != 10 {
                        ; CR(13)の場合の処理
                        if data == 13 {
                            jtxt.bcolor(3, 0)  ; シアン
                            jtxt.bputc(data)  ; これで改行される
                            jtxt.bcolor(5, 0)
                            cursor_x = 0      ; カーソル位置リセット
                            break
                        } else if data == 8 {
                            ; BS(8)の場合
                            if cursor_x > 0 {
                                cursor_x--
                            } else {
                                cursor_x = 39
                            }
                        } else {
                            ; 通常文字の場合は単純に加算
                            cursor_x++
                        }
                    }
                }
                response_count++
                sys.wait(1)
            }
        }
        
        return result
    }
    
    sub send_at_dialog() {
        show_status(iso:"ENTER AT COMMAND:", 14)
        
        ; 簡易ATコマンド入力（範囲内の最下行を使用）
        jtxt.blocate(0, 22)
        show_message(iso:"AT", 7)
        
        ; デバッグ: 受信バッファをクリア
        clear_receive_buffer()
        
        ; ここでは固定のATコマンドを送信（実際はユーザー入力を受け付ける）
        send_at_command_debug(iso:"AT")
    }
    
    sub toggle_echo() {
        echo_mode = not echo_mode
        if echo_mode {
            show_status(iso:"LOCAL ECHO: ON", 5)
        } else {
            show_status(iso:"LOCAL ECHO: OFF", 7)
        }
    }
    
    sub clear_display() {
        ; 表示エリアのみクリア（9行目から23行目まで、24行目のステータスラインは除く）
        ubyte y
        for y in 9 to 23 {
            jtxt.blocate(0, y)
            ubyte x
            for x in 0 to 39 {
                jtxt.bputc(32)  ; スペース
            }
        }
        
        display_line = 9
        cursor_x = 0
        cursor_y = display_line
        jtxt.blocate(0, display_line)
        show_status(iso:"SCREEN CLEARED", 5)
    }
    
    sub handle_newline() {
        ; jtxtの行範囲制御に任せて改行実行
        jtxt.bputc(13)
        cursor_x = 0  ; 改行したら0にリセット
        ; cursor_yをjtxt.bitmap_yと同期
        cursor_y = jtxt.bitmap_y
    }
    
    sub show_message(uword message, ubyte color) {
        jtxt.bcolor(color, 0)
        ; 文字列の長さを計算してカーソル位置を更新
        uword ptr = message
        while @(ptr) != 0 {
            cursor_x++  ; 単純に加算
            ptr++
        }
        jtxt.bputs(message)
        jtxt.bcolor(5, 0)  ; デフォルト緑に戻す
    }
    
    sub draw_status_line() {
        ; 行範囲制御を一時的に無効化してステータスラインを描画
        jtxt.bwindow_disable()
        
        ; 24行目に移動
        jtxt.blocate(0, 24)
        jtxt.bcolor(0, 7)  ; 黒文字、黄色背景
        
        ; ステータスライン全体をクリア
        ubyte x
        for x in 0 to 39 {
            jtxt.bputc(32)  ; スペース
        }
        
        ; 行範囲制御を再度有効化
        jtxt.bwindow_enable()
        jtxt.bcolor(5, 0)  ; デフォルト色に戻す
    }
    
    sub show_status(uword status, ubyte color) {
        ; 現在のカーソル位置を保存
        ubyte saved_x = cursor_x % 40
        ubyte saved_y = if cursor_y > 23 23 else cursor_y
        
        ; 行範囲制御を一時的に無効化
        jtxt.bwindow_disable()
        
        ; 24行目（ステータスライン）に移動
        jtxt.blocate(0, 24)
        jtxt.bcolor(0, color)  ; 黒文字、指定背景色
        
        ; ステータスライン全体をクリア
        ubyte x
        for x in 0 to 39 {
            jtxt.bputc(32)  ; スペース
        }
        
        ; ステータスメッセージを表示
        jtxt.blocate(0, 24)
        jtxt.bputs(status)
        
        ; 行範囲制御を再度有効化
        jtxt.bwindow_enable()
        jtxt.bcolor(5, 0)  ; デフォルト色に戻す
        
        ; カーソル位置を元に戻す
        jtxt.blocate(saved_x, saved_y)
    }
    
    ; ステータスラインクリア（IME無効化時など）
    sub clear_status_line() {
        ; 現在のカーソル位置を保存
        ubyte saved_x = cursor_x % 40
        ubyte saved_y = if cursor_y > 23 23 else cursor_y
        
        ; 行範囲制御を一時的に無効化
        jtxt.bwindow_disable()
        
        ; 24行目（ステータスライン）をクリア
        jtxt.blocate(0, 24)
        jtxt.bcolor(0, 0)  ; 黒文字、黒背景
        
        ubyte x
        for x in 0 to 39 {
            jtxt.bputc(32)  ; スペース
        }
        
        ; 行範囲制御を再度有効化
        jtxt.bwindow_enable()
        jtxt.bcolor(5, 0)  ; デフォルト色に戻す
        
        ; カーソル位置を元に戻す
        jtxt.blocate(saved_x, saved_y)
    }
    
    ; 受信バッファクリア関数
    sub clear_receive_buffer() {
        show_message(iso:"[CLEAR RX BUFFER] ", 6)
        swiftlink.clear_receive_buffer()
        show_message(iso:"DONE", 6)
        handle_newline()
    }
    
    ; デバッグ用ATコマンド送信
    sub send_at_command_debug(uword command) -> bool {
        show_message(iso:"[DEBUG] SEND: ", 6)
        show_message(command, 6)
        handle_newline()
        
        ; ATコマンド送信
        show_message(iso:"[DEBUG] Sending bytes...", 6)
        handle_newline()
        
        bool result = swiftlink.send_at_command(command)
        
        if result {
            show_message(iso:"[DEBUG] Send OK, waiting response...", 6)
            handle_newline()
            
            ; レスポンス待ち（詳細ログ付き）
            sys.wait(30)  ; 0.5秒待つ
            
            ; レスポンス受信
            ubyte response_count = 0
            uword received
            bool got_response = false
            
            while response_count < 100 {
                received = swiftlink.receive_byte_nonblocking()
                if received < 256 {
                    ubyte data = lsb(received)
                    got_response = true
                    show_message(iso:"[RX: ", 6)
                    jtxt.bputc(data + 48)  ; 数値表示用
                    show_message(iso:"] ", 6)
                    
                    ; LF(10)は無視、CR(13)で改行
                    if data == 13 {
                        jtxt.bcolor(3, 0)  ; シアン
                        jtxt.bputc(data)  ; これで改行される
                        jtxt.bcolor(5, 0)
                        cursor_x = 0      ; カーソル位置リセット
                        break
                    } else if data == 8 {
                        ; BS(8)の場合
                        jtxt.bcolor(3, 0)  ; シアン
                        jtxt.bputc(data)
                        jtxt.bcolor(5, 0)
                        if cursor_x > 0 {
                            cursor_x--
                        } else {
                            cursor_x = 39
                        }
                    } else if data != 10 {
                        jtxt.bcolor(3, 0)  ; シアン
                        jtxt.bputc(data)
                        jtxt.bcolor(5, 0)
                        cursor_x++  ; 単純に加算
                    }
                }
                response_count++
                sys.wait(1)
            }
            
            if not got_response {
                show_message(iso:"[DEBUG] No response received", 2)
                handle_newline()
            }
        } else {
            show_message(iso:"[DEBUG] Send FAILED", 2)
            handle_newline()
        }
        
        return result
    }
    
    sub cleanup() {
        ; IMEを無効化（念のため）
        if ime.is_ime_active() {
            ime.toggle_ime_mode()
        }
        
        ; モデム終了処理
        swiftlink.cleanup()
        
        ; 画面クリア
        jtxt.bcls()
        
        ; 終了メッセージ（日本語）
        jtxt.bcolor(1, 0)  ; 白
        jtxt.blocate(10, 10)
        
        ; "プログラム終了"
        ubyte[] msg = [$83, $76, $83, $8D, $83, $4F, $83, $89, $83, $80, $8F, $49, $97, $B9, 0]
        ubyte i = 0
        while msg[i] != 0 {
            jtxt.bputc(msg[i])
            i++
        }
        
        ; 3秒待つ
        sys.wait(180)
        
        ; テキストモードに戻す
        jtxt.set_mode(jtxt.TEXT_MODE)
        jtxt.cls()
        txt.clear_screen()
        
        txt.print(iso:"modem test completed.\n")
    }
}