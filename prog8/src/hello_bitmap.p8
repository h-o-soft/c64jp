%import textio
%import syslib
%import jtxt
%zeropage kernalsafe

; C64漢字ROMカートリッジ ビットマップモード ハローワールドプログラム
main {
    
    sub start() {
        ; 日本語テキストライブラリ初期化（テキストモード）
        jtxt.init(jtxt.TEXT_MODE)
        
        ; 画面クリアと初期設定
        jtxt.cls()
        jtxt.set_bgcolor(0, 6)     ; 背景=黒, ボーダー=青
        jtxt.set_color(1)          ; 白い文字
        
        ; タイトル表示
        txt.print(iso:"C64 KANJI ROM CARTRIDGE - BITMAP MODE\n")
        txt.print(iso:"=====================================")
        
        ; 「こんにちはコモドール！」のShift-JIS文字列
        ubyte[] hello_message = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF,     ; "こんにちは"
            $82, $CD, $83, $52, $83, $82, $83, $68,     ; "はコモド"
            $81, $5B, $83, $8B, $81, $49,               ; "ール！"
            $00                                          ; null終端
        ]
        
        ; まずテキストモードで表示
        txt.column(0)
        txt.row(3)
        txt.print(iso:"TEXT MODE PREVIEW:")
        jtxt.set_color(14)         ; ライトブルーの文字
        jtxt.locate(2, 5)
        jtxt.puts(&hello_message)
        
        txt.column(0)
        txt.row(20)
        txt.print(iso:"PRESS ANY KEY FOR BITMAP MODE...")
        
        ; キー入力待ち
        repeat {
            sys.waitvsync()
            if cbm.GETIN2() != 0 {
                break
            }
        }
        
        ; ビットマップモードに切り替え
        jtxt.set_mode(1)  ; ビットマップモード
        
        ; ビットマップモードでの描画
        jtxt.bcolor(7, 6)     ; 黄色文字、青背景
        jtxt.blocate(5, 8)    ; 画面中央やや上
        jtxt.bputs(&hello_message)
        
        ; 別の位置にも描画
        jtxt.bcolor(1, 0)     ; 白文字、黒背景
        jtxt.blocate(3, 12)   ; 少し下に
        jtxt.bputs(&hello_message)
        
        ; さらに別の色で描画
        jtxt.bcolor(10, 2)    ; ライトレッド文字、赤背景
        jtxt.blocate(7, 16)   ; さらに下に
        jtxt.bputs(&hello_message)
        
        ; タイトル用の英語メッセージ
        ubyte[] title_message = [
            66, 73, 84, 77, 65, 80, 32, 77, 79, 68, 69, 32,  ; "BITMAP MODE "
            74, 65, 80, 65, 78, 69, 83, 69,                  ; "JAPANESE"
            00                                                ; null終端
        ]
        
        ; 英語タイトルを上部に表示
        jtxt.bcolor(14, 0)    ; ライトブルー文字、黒背景
        jtxt.blocate(8, 2)
        jtxt.bputs(&title_message)
        
        ; 完了メッセージ
        jtxt.bcolor(1, 0)     ; 白文字、黒背景
        jtxt.blocate(2, 20)
        jtxt.bputs(iso:"BITMAP MODE RENDERING COMPLETE!")
        jtxt.blocate(2, 22)
        jtxt.bputs(iso:"PRESS ANY KEY TO EXIT...")
        
        ; キー入力待ち
        repeat {
            sys.waitvsync()
            if cbm.GETIN2() != 0 {
                break
            }
        }
        
        ; 日本語テキストライブラリ終了処理
        jtxt.cleanup()
        %asm {{
            jmp ($fffc)
        }}
        
        return
    }
}
