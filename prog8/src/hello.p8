%import textio
%import syslib
%import jtxt
%zeropage kernalsafe

; C64漢字ROMカートリッジ ハローワールドプログラム
main {
    
    sub start() {
        ; 日本語テキストライブラリ初期化（文字128-191を使用、モード0）
        jtxt.init(128, 64, 0)
        
        ; 画面クリアと初期設定
        jtxt.cls()
        jtxt.set_bgcolor(0)        ; 黒い背景
        jtxt.set_bordercolor(6)    ; 青いボーダー
        jtxt.set_color(1)          ; 白い文字
        
        ; タイトル表示
        txt.print(iso:"C64 KANJI ROM CARTRIDGE\n")
        txt.print(iso:"=======================")

        ; 「こんにちはコモドール！」のShift-JIS文字列
        ubyte[] hello_message = [
            $82, $B1, $82, $F1, $82, $C9, $82, $BF,     ; "こんにちは"
            $82, $CD, $83, $52, $83, $82, $83, $68,     ; "はコモド"
            $81, $5B, $83, $8B, $81, $49,               ; "ール！"
            $00                                          ; null終端
        ]
        
        ; 説明文
        txt.column(0)
        txt.row(3)
        txt.print(iso:"JAPANESE MESSAGE:")
        
        ; 日本語メッセージを画面中央に表示
        jtxt.set_bgcolor(6)        ; 青い背景
        jtxt.set_bordercolor(14)   ; ライトブルーのボーダー
        jtxt.set_color(7)          ; 黄色い文字
        jtxt.locate(5, 10)
        jtxt.puts(&hello_message)

        jtxt.locate(0, 20)
        txt.column(0)
        txt.row(20)
        txt.print(iso:"PRESS ANY KEY TO CONTINUE...")
        
        ; キー入力待ち
        repeat {
            sys.waitvsync()
            if cbm.GETIN2() != 0 {
                break
            }
        }
        
        ; 画面をクリアして別の色で表示(全てjtxtで表示)
        jtxt.cls()
        
        jtxt.set_bgcolor(0)        ; 黒い背景
        jtxt.set_bordercolor(0)    ; 黒いボーダー
        jtxt.set_color(10)         ; ライトレッドの文字
        jtxt.locate(3, 8)
        jtxt.puts(&hello_message)
        
        jtxt.set_color(1)          ; 白い文字
        jtxt.locate(0, 20)
        jtxt.puts(iso:"PRESS ANY KEY TO EXIT...     ")
        
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