; MagicDesk漢字ROMカートリッジ - 基本版
; 8KBバンク単位、制御レジスタは$DE00
; 64tass形式アセンブラソース

.cpu '6502'
.enc 'none'

* = $8000

; カートリッジヘッダー（CBM80署名）
; $8000-$8001: コールドスタートベクター（リトルエンディアン）
	.word cold_start        ; 起動時のエントリポイント

; $8002-$8003: ウォームスタートベクター（通常は同じ）
	.word cold_start        ; RUN/STOPリストア時のエントリポイント

; $8004-$8008: カートリッジ署名 "CBM80"
	.byte $C3, $C2, $CD    ; 'CBM' in PETSCII
	.byte $38, $30         ; '80' in PETSCII

; MagicDesk制御レジスタ
MAGICDESK_CTRL = $DE00  ; MagicDesk制御レジスタ

cold_start
	; KERNAL初期化
	sei                 ; 割り込み無効化
	cld                 ; BCDモードクリア
	ldx #$FF
	txs                 ; スタックポインタ初期化
	
	; VICとCIA初期化
	jsr $FF84           ; IOINIT - I/Oチップ初期化
	jsr $FF87           ; RAMTAS - RAMテスト/初期化
	jsr $FF8A           ; RESTOR - I/Oベクトル復元
	jsr $FF81           ; CINT - 画面エディタ初期化
	
	cli                 ; 割り込み有効化

main
	; MagicDesk初期化（バンク0に設定）
	lda #$00
	sta MAGICDESK_CTRL
	
	; 画面クリア
	jsr clear_screen
	
	; ウェルカムメッセージ表示
	ldx #$00
print_loop
	lda welcome_msg,x
	beq show_instructions
	jsr $FFD2       ; CHROUT
	inx
	jmp print_loop

show_instructions
	; 使用方法を表示
	ldx #$00
inst_loop
	lda instructions,x
	beq wait_delay
	jsr $FFD2
	inx
	jmp inst_loop

wait_delay
	; 約1秒待機（PAL/NTSCの両方で約1秒）
	ldx #50         ; 50フレーム（PAL:1秒、NTSC:0.83秒）
delay_loop
	; VSYNCを待つ（ラスターライン$00を待つ）
wait_raster
	lda $D012       ; ラスターラインレジスタ
	cmp #$00        ; ラスターライン0
	bne wait_raster
	
	; ラスターライン0を通過するまで待つ
wait_raster_pass
	lda $D012
	cmp #$00
	beq wait_raster_pass
	
	dex
	bne delay_loop
	
	; BASICへ復帰
	jmp ($A000)     ; BASIC cold start vector

clear_screen
	lda #$93        ; CLR/HOME
	jsr $FFD2
	rts

; メッセージデータ
welcome_msg
	.text "C64 KANJI ROM SYSTEM"
	.byte $0D, $0D
	.text "READY."
	.byte $0D, $0D, $00

instructions
	.byte $00       ; すぐに待機処理へ

; パディング（8KBまで）
	.fill $2000 - (* - $8000), $FF