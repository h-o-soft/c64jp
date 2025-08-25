; SwiftLink RS-232通信ライブラリ (swiftlink.p8)
; Copyright (c) 2025 H.O SOFT Inc. - Licensed under MIT License
; 6551 ACIA (Asynchronous Communications Interface Adapter) 用
; NMI割り込み駆動、256バイトリングバッファ版

swiftlink {
    ; SwiftLink ACIAレジスタ定義（$DF00ベース）
    const uword SWIFTLINK_BASE = $DF00     ; SwiftLink基底アドレス（標準）
    const uword ACIA_DATA = $DF00          ; データレジスタ (R/W)
    const uword ACIA_STATUS = $DF01        ; ステータスレジスタ (R) / リセットレジスタ (W)
    const uword ACIA_COMMAND = $DF02       ; コマンドレジスタ (W)
    const uword ACIA_CONTROL = $DF03       ; コントロールレジスタ (W)
    
    ; ステータスレジスタビット定義
    const ubyte STATUS_PARITY_ERROR = %00000001    ; パリティエラー
    const ubyte STATUS_FRAMING_ERROR = %00000010   ; フレーミングエラー
    const ubyte STATUS_OVERRUN_ERROR = %00000100   ; オーバーランエラー
    const ubyte STATUS_RX_FULL = %00001000         ; 受信データレジスタフル
    const ubyte STATUS_TX_EMPTY = %00010000        ; 送信データレジスタエンプティ
    const ubyte STATUS_DCD = %00100000             ; データキャリア検出（注：DSRと逆）
    const ubyte STATUS_DSR = %01000000             ; データセットレディ（注：DCDと逆）
    const ubyte STATUS_IRQ = %10000000             ; 割り込み発生
    
    ; コマンドレジスタビット定義
    const ubyte CMD_DTR = %00000001                ; DTR出力制御
    const ubyte CMD_RX_IRQ_ENABLE = %00000001      ; 受信割り込み有効（DTRと同じビット）
    const ubyte CMD_RX_IRQ_DISABLE = %00000010     ; 受信割り込み無効
    const ubyte CMD_TX_IRQ_DISABLE = %00001100     ; 送信割り込み無効（ビット2,3）
    const ubyte CMD_ECHO_MODE = %00010000          ; エコーモード
    const ubyte CMD_PARITY_DISABLE = %00000000     ; パリティ無効
    const ubyte CMD_PARITY_ODD = %00100000         ; 奇数パリティ
    const ubyte CMD_PARITY_EVEN = %01100000        ; 偶数パリティ
    const ubyte CMD_PARITY_MARK = %10100000        ; マークパリティ
    const ubyte CMD_PARITY_SPACE = %11100000       ; スペースパリティ
    
    ; コントロールレジスタビット定義（ボーレート設定）
    const ubyte CTRL_STOP_1 = %00000000            ; ストップビット1
    const ubyte CTRL_STOP_2 = %10000000            ; ストップビット2
    const ubyte CTRL_DATA_8 = %00000000            ; データビット8
    const ubyte CTRL_DATA_7 = %00100000            ; データビット7
    const ubyte CTRL_DATA_6 = %01000000            ; データビット6
    const ubyte CTRL_DATA_5 = %01100000            ; データビット5
    const ubyte CTRL_INTERNAL_CLOCK = %00010000    ; 内蔵クロック使用（必須）
    
    ; ボーレート設定値（SwiftLink互換品では3.6864MHz水晶使用により倍速動作）
    ; 実際の通信速度は定数名の2倍になることが多い（例：BAUD_19200 = 38400bps）
    const ubyte BAUD_50 = %00000001
    const ubyte BAUD_75 = %00000010
    const ubyte BAUD_110 = %00000011
    const ubyte BAUD_135 = %00000100
    const ubyte BAUD_150 = %00000101
    const ubyte BAUD_300 = %00000110
    const ubyte BAUD_600 = %00000111
    const ubyte BAUD_1200 = %00001000
    const ubyte BAUD_1800 = %00001001
    const ubyte BAUD_2400 = %00001010
    const ubyte BAUD_3600 = %00001011
    const ubyte BAUD_4800 = %00001100
    const ubyte BAUD_7200 = %00001101
    const ubyte BAUD_9600 = %00001110
    const ubyte BAUD_19200 = %00001111
    
    ; エラーコード
    const ubyte ERROR_NONE = 0
    const ubyte ERROR_TIMEOUT = 1
    const ubyte ERROR_PARITY = 2
    const ubyte ERROR_FRAMING = 3
    const ubyte ERROR_OVERRUN = 4
    const ubyte ERROR_BUFFER_FULL = 5
    
    ; 受信バッファ（256バイトリングバッファ）
    ubyte[256] receive_buffer
    ubyte receive_head = 0         ; 読み出し位置
    ubyte receive_tail = 0         ; 書き込み位置
    ubyte receive_count = 0        ; バッファ内のバイト数
    
    ; 状態変数
    ubyte last_error = ERROR_NONE              ; 最後のエラー状態
    uword old_nmi_vector = 0                   ; 元のNMIベクタ保存用
    bool nmi_installed = false                 ; NMIハンドラインストール済みフラグ
    bool flow_control_stopped = false          ; RTS制御でフロー停止中フラグ
    ubyte rts_off_value = 0                 ; RTS無効時のコマンドレジスタ値
    
    ; NMIベクタアドレス
    const uword NMI_VECTOR = $0318
    
    ; RTS/CTSフロー制御閾値（cc65準拠）
    const ubyte FLOW_STOP_THRESHOLD = 33    ; 残り33バイトでRTS無効
    const ubyte FLOW_RESUME_THRESHOLD = 63  ; 63バイト以上でRTS有効
    
    ; SwiftLink初期化（NMI割り込み版）
    ; baud_rate: ボーレート設定値（BAUD_xxxを使用）
    ; data_bits: データビット設定（CTRL_DATA_xxx）
    ; stop_bits: ストップビット設定（CTRL_STOP_xxx）
    ; parity: パリティ設定（CMD_PARITY_xxx）
    ; use_nmi: NMI割り込みを使用するか（true推奨）
    sub init(ubyte baud_rate, ubyte data_bits, ubyte stop_bits, ubyte parity, bool use_nmi) {
        ; バッファ初期化
        receive_head = 0
        receive_tail = 0
        receive_count = 0
        last_error = ERROR_NONE
        flow_control_stopped = false
        rts_off_value = 0
        
        %asm {{
            sei                           ; 割り込み無効化
            
            ; cc65準拠の初期化順序（動作確認済み）
            
            ; 1. DTR無効化と割り込み無効化
            lda  #%00001010              ; DTR=0, 割り込み無効
            sta  p8c_ACIA_COMMAND
            
            ; 2. コントロールレジスタ設定（ボーレート、データビット、ストップビット）
            lda  p8v_baud_rate
            ora  p8v_data_bits
            ora  p8v_stop_bits
            ora  #$10                     ; ビット4=1: 内蔵クロック使用
            sta  p8c_ACIA_CONTROL
            
            ; 3. コマンドレジスタ設定（RTS無効値も保存）
            lda  p8v_parity
            ora  #%00000001               ; DTR=1
            sta  p8v_rts_off_value        ; RTS無効時の値として保存
            sta  p8c_ACIA_COMMAND         ; まだ割り込みは無効
            
            cli                           ; 割り込み有効化
        }}
        
        ; NMI割り込みを使用する場合
        if use_nmi {
            install_nmi_handler()
            
            ; 受信割り込みとRTSを有効化
            %asm {{
                sei
                lda  p8v_rts_off_value
                ora  #%00001000           ; 受信割り込み有効（ビット3）
                sta  p8c_ACIA_COMMAND
                cli
            }}
        }
    }
    
    ; デフォルト設定での簡易初期化（2400bps, 8N1, NMI使用）
    sub init_default() {
        init(BAUD_2400, CTRL_DATA_8, CTRL_STOP_1, CMD_PARITY_DISABLE, true)
    }
    
    ; NMIハンドラのインストール
    sub install_nmi_handler() {
        if not nmi_installed {
            %asm {{
                sei
                
                ; 元のNMIベクタを保存
                lda  p8c_NMI_VECTOR
                sta  p8v_old_nmi_vector
                lda  p8c_NMI_VECTOR+1
                sta  p8v_old_nmi_vector+1
                
                ; 新しいNMIハンドラを設定
                lda  #<p8s_nmi_handler
                sta  p8c_NMI_VECTOR
                lda  #>p8s_nmi_handler
                sta  p8c_NMI_VECTOR+1
                
                cli
            }}
            nmi_installed = true
        }
    }
    
    ; NMIハンドラのアンインストール
    sub uninstall_nmi_handler() {
        if nmi_installed {
            %asm {{
                sei
                
                ; 元のNMIベクタを復元
                lda  p8v_old_nmi_vector
                sta  p8c_NMI_VECTOR
                lda  p8v_old_nmi_vector+1
                sta  p8c_NMI_VECTOR+1
                
                cli
            }}
            nmi_installed = false
        }
    }
    
    ; 1バイト送信（ポーリング版）
    ; data: 送信するバイト
    ; 戻り値: true=成功, false=タイムアウト
    sub send_byte(ubyte data) -> bool {
        bool result
        %asm {{
            ; タイムアウトカウンタ
            ldy  #0
            ldx  #0
            
        send_wait:
            ; ステータスレジスタチェック（送信レジスタ空き待ち）
            lda  p8c_ACIA_STATUS
            and  #$10                     ; STATUS_TX_EMPTY
            bne  send_ready               ; ビットが1なら送信可能
            
            ; タイムアウトチェック
            inx
            bne  send_wait
            iny
            cpy  #10                      ; 約10*256サイクル
            bne  send_wait
            
            ; タイムアウト
            lda  #1                       ; ERROR_TIMEOUT
            sta  p8v_last_error
            lda  #0
            sta  p8v_result
            jmp  send_exit
            
        send_ready:
            ; データ送信
            lda  p8v_data
            sta  p8c_ACIA_DATA
            
            ; 成功
            lda  #0                       ; ERROR_NONE
            sta  p8v_last_error
            lda  #1
            sta  p8v_result
            
        send_exit:
        }}
        return result
    }
    
    ; 1バイト受信（バッファから）
    ; 戻り値: 受信したバイト（$100以上はエラーまたはデータなし）
    sub receive_byte() -> uword {
        if receive_count == 0 {
            return $100  ; データなし
        }
        
        uword result = 0
        %asm {{
            sei                           ; 割り込み無効化
            
            ; バッファから1バイト読み出し
            ldx  p8v_receive_head
            lda  p8v_receive_buffer,x
            sta  p8v_result
            
            ; ヘッド更新
            inc  p8v_receive_head
            
            ; カウント更新
            dec  p8v_receive_count
            
            cli                           ; 割り込み有効化
            
            ; 成功を示す上位バイト
            lda  #0
            sta  p8v_result+1
        }}
        
        ; フロー制御チェック（バッファから読み出したので余裕ができた可能性）
        check_flow_control()
        
        return result
    }
    
    ; ノンブロッキング受信（receive_byteと同じ）
    sub receive_byte_nonblocking() -> uword {
        return receive_byte()
    }
    
    ; 受信バッファをクリア
    sub clear_receive_buffer() {
        %asm {{
            sei
            lda  #0
            sta  p8v_receive_head
            sta  p8v_receive_tail
            sta  p8v_receive_count
            cli
        }}
        
        ; フロー制御状態もリセット（バッファが空になったのでRTS有効）
        if flow_control_stopped {
            restore_rts()
            flow_control_stopped = false
        }
    }
    
    ; 受信バッファのバイト数を取得
    sub get_receive_count() -> ubyte {
        return receive_count
    }
    
    ; 文字列送信
    sub send_string(uword text_addr) -> bool {
        uword ptr = text_addr
        while @(ptr) != 0 {
            if not send_byte(@(ptr)) {
                return false
            }
            ptr++
        }
        return true
    }
    
    ; ATコマンド送信（CR付き）
    sub send_at_command(uword cmd_addr) -> bool {
        if not send_string(cmd_addr) {
            return false
        }
        return send_byte(13)  ; CR
    }
    
    ; RTS/CTSフロー制御チェックと処理
    sub check_flow_control() {
        ; バッファ残りが少なくなったらRTS無効（受信停止要求）
        if receive_count >= 256 - FLOW_STOP_THRESHOLD {
            if not flow_control_stopped {
                assert_flow_control()  ; RTS無効化
                flow_control_stopped = true
            }
        }
        ; バッファに余裕ができたらRTS有効（受信再開要求）
        else if receive_count <= 256 - FLOW_RESUME_THRESHOLD {
            if flow_control_stopped {
                restore_rts()          ; RTS有効化
                flow_control_stopped = false
            }
        }
    }
    
    ; RTS制御によるフロー制御アサート（受信停止要求）
    sub assert_flow_control() {
        %asm {{
            ; RTS無効化（RTS=1で受信停止要求）
            lda  p8v_rts_off_value        ; DTR=1, RTS=0の基本値
            sta  p8c_ACIA_COMMAND
        }}
    }
    
    ; RTS制御によるフロー制御解除（受信再開許可）
    sub restore_rts() {
        %asm {{
            ; RTS有効化（RTS=0で受信再開許可）
            lda  p8v_rts_off_value        ; DTR=1, RTS=0の基本値
            ora  #%00001000               ; 受信割り込み有効
            sta  p8c_ACIA_COMMAND
        }}
    }
    
    ; キャリア検出チェック
    sub check_carrier() -> bool {
        return (@(ACIA_STATUS) & STATUS_DCD) == 0  ; アクティブロー
    }
    
    ; データセットレディチェック
    sub check_dsr() -> bool {
        return (@(ACIA_STATUS) & STATUS_DSR) == 0  ; アクティブロー
    }
    
    ; 最後のエラー取得
    sub get_last_error() -> ubyte {
        return last_error
    }
    
    ; エラークリア
    sub clear_error() {
        last_error = ERROR_NONE
        ; ACIAのエラーフラグもクリア（ダミーリード）
        ubyte dummy = @(ACIA_DATA)
    }
    
    ; モデム終了処理
    sub cleanup() {
        ; NMIハンドラをアンインストール
        uninstall_nmi_handler()
        
        %asm {{
            ; 割り込み無効化とDTR無効化
            lda  #%00001010               ; DTR=0, 割り込み無効
            sta  p8c_ACIA_COMMAND
        }}
        
        ; バッファクリア
        clear_receive_buffer()
    }
    
    ; NMI割り込みハンドラ（アセンブラ）
    asmsub nmi_handler() {
        %asm {{
            ; レジスタ保存
            pha
            txa
            pha
            tya
            pha
            
            ; ACIA割り込みかチェック
            lda  p8c_ACIA_STATUS
            and  #%00001000               ; RX_FULL bit
            beq  nmi_exit                 ; 受信データなし
            
            ; 受信データあり - バッファに格納
            lda  p8c_ACIA_DATA            ; データ読み取り
            
            ; バッファフルチェック
            ldy  p8v_receive_count
            cpy  #255
            beq  nmi_exit                 ; バッファフル（データ破棄）
            
            ; バッファに格納
            ldy  p8v_receive_tail
            sta  p8v_receive_buffer,y
            
            ; テール更新
            inc  p8v_receive_tail
            
            ; カウント更新
            inc  p8v_receive_count
            
            ; フロー制御が必要かチェック（prog8のサブルーチンを呼ぶ）
            jsr  p8s_check_flow_control
            
        nmi_exit:
            ; レジスタ復元
            pla
            tay
            pla
            tax
            pla
            
            ; 割り込みから復帰
            rti
        }}
    }
}