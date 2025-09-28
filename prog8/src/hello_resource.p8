%import textio
%import syslib
%import jtxt
%zeropage kernalsafe

; C64漢字ROMカートリッジ 文字列リソース ハローワールドプログラム
main {
    
    sub start() {
        ; 日本語テキストライブラリ初期化（テキストモード）
        jtxt.init(0)
        
        ; 画面クリアと色設定
        jtxt.cls()
        jtxt.set_bgcolor(0, 6)     ; 背景=黒, ボーダー=青
        jtxt.set_color(1)          ; 白
        
        ; タイトル表示
        jtxt.locate(0, 0)
        jtxt.puts(iso:"C64 KANJI ROM - STRING RESOURCES")
        
        ; 利用可能な文字列リソース一覧
        jtxt.locate(0, 4)
        jtxt.puts(iso:"0: HELLO")
        jtxt.locate(0, 5)
        jtxt.puts(iso:"1: MULTILINE MESSAGE")
        jtxt.locate(0, 6)
        jtxt.puts(iso:"2: TEST")
        jtxt.locate(0, 7)
        jtxt.puts(iso:"3: HELLO WORLD IN JAPANESE")
        jtxt.locate(0, 8)
        jtxt.puts(iso:"4: C64 JAPANESE DISPLAY")
        jtxt.locate(0, 9)
        jtxt.puts(iso:"6: MAGICDESK KANJI ROM")
        jtxt.locate(0, 10)
        jtxt.puts(iso:"7: STRING RESOURCE SYSTEM")
        jtxt.locate(0, 11)
        jtxt.puts(iso:"9: ALPHABET TEST")
        
        jtxt.locate(0, 13)
        jtxt.puts(iso:"PRESS ANY KEY...")
        
        ; キー入力待ち
        repeat {
            sys.waitvsync()
            if cbm.GETIN2() != 0 {
                break
            }
        }
        
        ; 文字列リソース表示デモ
        jtxt.cls()
        
        ; リソース0: "Hello"を表示
        jtxt.set_color(14)         ; ライトブルー
        jtxt.locate(2, 2)
        jtxt.puts(iso:"RESOURCE 0: ")
        jtxt.putr(0)
        
        ; リソース3: "こんにちは、世界！"を表示
        jtxt.set_color(7)          ; 黄色
        jtxt.locate(2, 4)
        jtxt.puts(iso:"RESOURCE 3: ")
        jtxt.putr(3)
        
        ; リソース4: "Commodore 64で日本語表示"を表示
        jtxt.set_color(10)         ; ライトレッド
        jtxt.locate(2, 6)
        jtxt.puts(iso:"RESOURCE 4: ")
        jtxt.putr(4)
        
        ; リソース7: "文字列リソースシステム"を表示
        jtxt.set_color(13)         ; ライトグリーン
        jtxt.locate(2, 8)
        jtxt.puts(iso:"RESOURCE 7: ")
        jtxt.putr(7)
        
        ; リソース1: 複数行メッセージを表示
        jtxt.set_color(11)         ; ダークグレー
        jtxt.locate(2, 10)
        jtxt.puts(iso:"RESOURCE 1: ")
        jtxt.putr(1)
        
        jtxt.set_color(1)          ; 白
        jtxt.locate(0, 15)
        jtxt.puts(iso:"PRESS ANY KEY FOR BITMAP MODE DEMO...")
        
        ; キー入力待ち
        repeat {
            sys.waitvsync()
            if cbm.GETIN2() != 0 {
                break
            }
        }
        
        ; ビットマップモードに切り替え
        jtxt.set_mode(1)  ; ビットマップモード
        
        ; ビットマップモードでの文字列リソース表示
        ; リソース3を複数の色と位置で表示
        jtxt.bcolor(7, 6)     ; 黄色文字、青背景
        jtxt.blocate(5, 8)
        jtxt.bputr(3)        ; "こんにちは、世界！"
        
        jtxt.bcolor(1, 0)     ; 白文字、黒背景  
        jtxt.blocate(3, 12)
        jtxt.bputr(4)        ; "Commodore 64で日本語表示"
        
        jtxt.bcolor(10, 2)    ; ライトレッド文字、赤背景
        jtxt.blocate(7, 16)
        jtxt.bputr(7)        ; "文字列リソースシステム"
        
        ; タイトル表示
        jtxt.bcolor(14, 0)    ; ライトブルー文字、黒背景
        jtxt.blocate(6, 2)
        jtxt.bputr(0)        ; "Hello"
        
        ; 完了メッセージ
        jtxt.bcolor(1, 0)     ; 白文字、黒背景
        jtxt.blocate(2, 20)
        jtxt.bputs(iso:"STRING RESOURCE DEMO COMPLETE!")
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
