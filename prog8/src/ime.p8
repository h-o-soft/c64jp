; ==============================================================================
; ime.p8 - 単文節変換日本語IME for C64
; Copyright (c) 2025 H.O SOFT Inc. - Licensed under MIT License
; ==============================================================================
;
; 単文節変換方式の日本語入力システム
; 辞書データはSKK辞書を元にdicconv/dicconv.pyで変換したものを使用
; CRTファイル内のBank 10から配置済み
;
; ==============================================================================
; 【辞書ファイル構造】
; ==============================================================================
;
; ■ 全体構成
; +0      : 'DIC' + 0x00 (4バイト) - マジックナンバー
; +4      : 名詞エントリオフセットテーブル (82エントリ × 3バイト = 246バイト)
; +250    : 動詞エントリオフセットテーブル (82エントリ × 3バイト = 246バイト)
; +496    : データ部（名詞エントリ群 → 動詞エントリ群の順）
;
; ==============================================================================
; 【オフセットテーブル】
; ==============================================================================
;
; ■ オフセット形式（3バイト/エントリ）
; - バイト0-1: バンク内オフセット (リトルエンディアン、0-8191)
; - バイト2  : バンク番号 (0から開始)
;
; ■ オフセットテーブルのインデックス（82エントリ）
; 各インデックスはひらがな1文字に対応:
;
;  0: あ   1: ぃ   2: い   3: ぅ   4: う   5: ぇ   6: え   7: ぉ   8: お
;  9: か  10: が  11: き  12: ぎ  13: く  14: ぐ  15: け  16: げ  17: こ  18: ご
; 19: さ  20: ざ  21: し  22: じ  23: す  24: ず  25: せ  26: ぜ  27: そ  28: ぞ
; 29: た  30: だ  31: ち  32: ぢ  33: っ  34: つ  35: づ  36: て  37: で  38: と  39: ど
; 40: な  41: に  42: ぬ  43: ね  44: の
; 45: は  46: ば  47: ぱ  48: ひ  49: び  50: ぴ  51: ふ  52: ぶ  53: ぷ  54: へ  55: べ  56: ぺ  57: ほ  58: ぼ  59: ぽ
; 60: ま  61: み  62: む  63: め  64: も
; 65: ゃ  66: や  67: ゅ  68: ゆ  69: ょ  70: よ
; 71: ら  72: り  73: る  74: れ  75: ろ
; 76: ゎ  77: わ  78: ゐ  79: ゑ  80: を  81: ん
;
; ※ エントリが存在しない場合、オフセットは 0x000000
;
; ==============================================================================
; 【データ部エントリ形式】
; ==============================================================================
;
; ■ 各エントリの構造（可変長）
;
; +0  : スキップサイズ (2バイト、リトルエンディアン)
;       - bit15: グループ先頭フラグ（1=グループの最初のエントリ）
;       - bit14-0: 次のエントリまでのスキップサイズ
;       - スキップサイズ = エントリ全体サイズ - 2（スキップサイズ自身）- キー文字列長 - 1（終端NULL）
; +2  : キー文字列 (Shift-JIS、NULL終端)
; +n  : 候補数 (1バイト)
; +n+1: 候補1 (Shift-JIS、NULL終端)
; +... : 候補2 (Shift-JIS、NULL終端)
; +... : 候補n (Shift-JIS、NULL終端)
;
; ■ エントリの例
; 「あい /愛/相/合い/」の場合:
; - スキップサイズ: 0x800C (先頭フラグ付き、12バイトスキップ)
; - キー: 0x82 0xA0 0x82 0xA2 0x00 ("あい" in Shift-JIS)
; - 候補数: 0x03
; - 候補1: 0x88 0xA4 0x00 ("愛")
; - 候補2: 0x91 0x8A 0x00 ("相")
; - 候補3: 0x8D 0x87 0x82 0xA2 0x00 ("合い")
;
; ==============================================================================
; 【8KBバンク境界処理】
; ==============================================================================
;
; ■ 重要な注意点
; - エントリは8KB境界をまたぐことがある
; - 1バイトずつ読み込む際は常にバンク境界チェックが必要
; - current_offset が 8192 に達したら次のバンクに切り替え
;
; ■ 読み込み擬似コード
; ```
; sub read_dic_byte() -> ubyte {
;     @(BANK_REG) = current_bank
;     ubyte data = @(ROM_BASE + current_offset)
;     current_offset++
;     if current_offset >= 8192 {
;         current_offset = 0
;         current_bank++
;     }
;     return data
; }
; ```
;
; ==============================================================================
; 【名詞・動詞の分類】
; ==============================================================================
;
; ■ 分類ルール
; - キーの末尾が半角文字（ASCII）→ 動詞エントリ
; - キーの末尾が全角文字 → 名詞エントリ
;
; ■ 動詞活用マッチング（重要）
; 辞書内の動詞エントリ（例：「とr」）を入力した読み（例：「とる」）にマッチさせる
; 
; 1. 「とる」で変換時の検索順序
;    a) 名詞エントリ「とる」を検索
;    b) 動詞エントリ「とr」を検索し、「とr」+「u」→「とる」としてマッチ
;
; 2. 動詞語幹の推定（ローマ字ベース）
;    入力「toru」→ 語幹「tor」を推定
;    辞書内「とr」エントリの候補「取/撮/捕」を表示
;
; 3. マッチパターン例
;    - 「とr」+「a」→「とら」（取ら）
;    - 「とr」+「i」→「とり」（取り）  
;    - 「とr」+「u」→「とる」（取る）
;    - 「とr」+「e」→「とれ」（取れ）
;    - 「とr」+「o」→「とろ」（取ろ）
;
; ■ 検索方法
; 1. 読みの1文字目からインデックスを決定
; 2. 名詞エントリを優先検索
; 3. 動詞エントリを語幹推定で検索
; 4. オフセットからバンクとアドレスを計算
; 5. グループ内を線形検索（先頭フラグが立つまで）
;
; ==============================================================================
; 【カートリッジ内での配置】
; ==============================================================================
;
; ■ MagicDesk/GMOD2 CRT内蔵配置
; - Bank 0: ベースコード
; - Bank 1-9: フォントデータ（美咲フォント）
; - Bank 10-27: 辞書データ（約144KB、CRT作成時に統合済み）
; - Bank 28-: 文字列リソース等（可変）
;
; ■ 辞書アクセス定数
; const ubyte DICTIONARY_START_BANK = 10  ; 辞書データ開始バンク（固定）
; const ubyte DICTIONARY_END_BANK = 27    ; 辞書データ終了バンク（固定）
;
; ==============================================================================
; 【単文節変換の動作】
; ==============================================================================
;
; ■ 変換フロー
; 1. ローマ字入力→ひらがな変換（リアルタイム）
; 2. 変換キー（スペース等）で変換開始
; 3. 辞書から候補を検索・表示（名詞・動詞両方）
; 4. 候補選択（次候補/前候補）
; 5. 確定またはキャンセル
;
; ■ ローマ字変換仕様
; - 基本：子音+母音 (ka→か, ki→き, ...)
; - 促音：子音重複 (kk→っk, tt→っt, ...)
; - 撥音：n単独またはnn (n→ん, nn→ん)
; - 拗音：子音+y+母音 (kya→きゃ, sho→しょ, ...)
; - 長音：ローマ字のまま変換（実装により異なる）
;
; ■ SKKとの違い
; - 単文節変換：文節単位で変換（SKKは単語単位）
; - 送り仮名：自動判定（SKKは明示的指定）
; - 変換モード：常に日本語入力可能（SKKはモード切替必要）
;
; ■ 使用例（modem_test等から）
; ```
; ; メインループ内
; key = cbm.GETIN2()
; 
; ; コモドール+Space検出（別途実装必要）
; if is_commodore_space(key) {
;     ime.toggle_mode()
;     continue
; }
; 
; ; IMEがアクティブな場合、キー入力を横取り
; if ime.is_active() {
;     if ime.process_key(key) {
;         ; IMEが処理した
;         if ime.input_state == 0 {
;             ; 確定された
;             uword text = ime.get_confirmed_text()
;             ; textをmodemに送信、画面に表示等
;             send_to_modem(text)
;         }
;     }
;     continue  ; メイン側の処理はスキップ
; }
; 
; ; 通常のキー処理...
; ```
;
; ==============================================================================
; 【実装時の注意】
; ==============================================================================
;
; 1. 辞書はCRT内蔵のため、Bank 10からアクセス開始
; 2. 辞書読み込み前にマジックナンバー 'DIC\0' を確認
; 3. バンク切り替えは $DE00 (GMOD2/MagicDesk) を使用
; 4. 検索は読みのひらがな1文字目でグループを特定
; 5. Shift-JIS文字列の処理に注意（2バイト文字の境界）
; 6. 8KB境界をまたぐ可能性を常に考慮
; 7. 候補バッファは十分なサイズを確保
;
; ==============================================================================

%import syslib
%import jtxt

ime {
    ; 辞書アクセス定数
    const ubyte DICTIONARY_START_BANK = 10  ; 辞書開始バンク（CRT内固定配置）
    const ubyte DICTIONARY_END_BANK = 27    ; 辞書終了バンク
    const uword ROM_BASE = $8000           ; カートリッジROMベースアドレス
    const uword BANK_REG = $DE00           ; バンク切り替えレジスタ
    
    ; IMEモード定数
    const ubyte KEY_COMMODORE = $E0        ; コモドールキー（仮）
    const ubyte KEY_SPACE = 32             ; スペースキー
    const ubyte KEY_RETURN = 13            ; リターンキー
    const ubyte KEY_ESC = 27               ; ESCキー（キャンセル用）
    
    const ubyte IME_STATUS_WIDTH = 4       ; IME状態表示の幅 "[あ] " = 4文字
    
    ; 色定数
    const ubyte COLOR_DEFAULT_FG = 1       ; デフォルト前景色（白）
    const ubyte COLOR_DEFAULT_BG = 0       ; デフォルト背景色（黒）
    const ubyte COLOR_STATUS_FG = 0        ; ステータス前景色（黒）
    const ubyte COLOR_STATUS_BG = 1        ; ステータス背景色（白）
    
    ; IMEイベント定数
    const ubyte IME_EVENT_NONE = 0          ; 何もなし（継続）
    const ubyte IME_EVENT_CONFIRMED = 1     ; 文字列確定
    const ubyte IME_EVENT_CANCELLED = 2     ; 入力キャンセル
    const ubyte IME_EVENT_MODE_CHANGED = 3  ; 入力モード変更
    const ubyte IME_EVENT_DEACTIVATED = 4   ; IME無効化
    const ubyte IME_EVENT_KEY_PASSTHROUGH = 5 ; キー透過（メイン側処理）
    
    ; 実装済み機能一覧はIME_TODO.mdを参照

    ; ==============================================================================
    ; ローマ字→かな変換実装
    ; ==============================================================================
    
    ; ローマ字変換状態定数
    const ubyte ROMAJI_EMPTY = 0         ; 空状態
    const ubyte ROMAJI_CONSONANT = 1     ; 子音待ち
    const ubyte ROMAJI_N = 2             ; n特殊処理
    const ubyte ROMAJI_SMALL_TSU = 3     ; っ処理
    const ubyte ROMAJI_WAITING_2ND = 4   ; 2文字目待ち（sh, ch, ts用）
    const ubyte ROMAJI_X_PREFIX = 5      ; x小文字用
    const ubyte ROMAJI_Y_WAITING = 6     ; 拗音待ち（ky, sh, ch等+y）
    const ubyte ROMAJI_SKIP_NEXT = 7     ; 次の1文字をスキップ（TSU等）
    
    ; ローマ字変換用変数
    ubyte romaji_state
    ubyte last_consonant               ; 前の子音を記憶
    ubyte second_consonant             ; 2文字目子音（sh, ch, ts用）
    ubyte[8] romaji_buffer             ; ローマ字バッファ
    ubyte romaji_pos                   ; バッファ位置
    ubyte[64] hiragana_buffer          ; ひらがなバッファ
    ubyte hiragana_pos                 ; ひらがなバッファ位置
    
    ; ブロッキング型IME用変数
    ubyte saved_cursor_x               ; process開始時のカーソルX位置
    ubyte saved_cursor_y               ; process開始時のカーソルY位置
    ubyte saved_fg_color               ; process開始時のfg色
    ubyte saved_bg_color               ; process開始時のbg色

    ubyte passthrough_key              ; パススルーするキーコード
    
    ; 基本ひらがなマッピング（あ行から順に）
    ; 各行5文字（あいうえお、かきくけこ、...）
    uword[] basic_hiragana = [
        $82A0, $82A2, $82A4, $82A6, $82A8,  ; あいうえお (0-4)
        $82A9, $82AB, $82AD, $82AF, $82B1,  ; かきくけこ (5-9)
        $82B3, $82B5, $82B7, $82B9, $82BB,  ; さしすせそ (10-14)
        $82BD, $82BF, $82C2, $82C4, $82C6,  ; たちつてと (15-19)
        $82C8, $82C9, $82CA, $82CB, $82CC,  ; なにぬねの (20-24)
        $82CD, $82D0, $82D3, $82D6, $82D9,  ; はひふへほ (25-29)
        $82DC, $82DD, $82DE, $82DF, $82E0,  ; まみむめも (30-34)
        $82E2, $82E4, $82E6,                  ; やゆよ (35-37)
        $82E7, $82E8, $82E9, $82EA, $82EB,  ; らりるれろ (38-42)
        $82ED, $82F0, $82F1                   ; わをん (43-45)
    ]
    
    ; 濁音・半濁音マッピング
    uword[] dakuten_hiragana = [
        $82AA, $82AC, $82AE, $82B0, $82B2,  ; がぎぐげご (0-4)  
        $82B4, $82B6, $82B8, $82BA, $82BC,  ; ざじずぜぞ (5-9)
        $82BE, $82C0, $82C3, $82C5, $82C7,  ; だぢづでど (10-14)
        $82CE, $82D1, $82D4, $82D7, $82DA   ; ばびぶべぼ (15-19)
    ]
    
    uword[] handakuten_hiragana = [
        $82CF, $82D2, $82D5, $82D8, $82DB   ; ぱぴぷぺぽ (0-4)
    ]
    
    ; 小文字ひらがな
    uword[] small_hiragana = [
        $829F, $82A1, $82A3, $82A5, $82A7,  ; ぁぃぅぇぉ (0-4) - ぁ=$829Fに修正
        $82E1, $82E3, $82E5,                  ; ゃゅょ (5-7)
        $82C1                                   ; っ (8)
    ]

    ; ==============================================================================
    ; ローマ字→かな変換関数
    ; ==============================================================================
    
    ; 母音インデックスを取得 (a=0, i=1, u=2, e=3, o=4, それ以外=255)
    sub vowel_index(ubyte ch) -> ubyte {
        when ch {
            iso:'a' -> return 0
            iso:'i' -> return 1  
            iso:'u' -> return 2
            iso:'e' -> return 3
            iso:'o' -> return 4
            else -> return 255
        }
    }
    
    ; 子音を行インデックスに変換 (k=1, s=2, t=3, ..., 不明=255)
    sub consonant_to_row(ubyte ch) -> ubyte {
        ubyte[] row_keys = [iso:'k', iso:'s', iso:'t', iso:'c', iso:'n', iso:'h', iso:'f', iso:'m', iso:'y', iso:'r', iso:'w']
        ubyte[] row_vals = [1,       2,       3,       3,       4,       5,       5,       6,       7,       8,       9]
        ubyte i
        for i in 0 to 10 {
            if ch == row_keys[i] return row_vals[i]
        }
        return 255

        ; どちらがいいか悩み中
        ; when ch {
        ;     iso:'k' -> return 1   ; か行
        ;     iso:'s' -> return 2   ; さ行
        ;     iso:'t' -> return 3   ; た行
        ;     iso:'c' -> return 3   ; た行 (chi = ti)
        ;     iso:'n' -> return 4   ; な行
        ;     iso:'h' -> return 5   ; は行
        ;     iso:'f' -> return 5   ; は行 (fu = hu)
        ;     iso:'m' -> return 6   ; ま行
        ;     iso:'y' -> return 7   ; や行
        ;     iso:'r' -> return 8   ; ら行
        ;     iso:'w' -> return 9   ; わ行
        ;     else -> return 255
        ; }
    }
    
    ; 特殊2文字パターン変換（shi, chi, tsu等）
    sub convert_special_2char(ubyte ch1, ubyte ch2) -> uword {
        if ch1 == iso:'s' and ch2 == iso:'h' {
            return $82B5  ; し (sh)
        }
        if ch1 == iso:'c' and ch2 == iso:'h' {
            return $82BF  ; ち (ch)  
        }
        if ch1 == iso:'t' and ch2 == iso:'s' {
            return $82C2  ; つ (ts)
        }
        return 0  ; 特殊パターンなし
    }
    
    ; 特殊1文字変換（si→し、ti→ち、hu→ふ等）
    sub convert_special_1char(ubyte consonant, ubyte vowel) -> uword {
        when consonant {
            iso:'s' -> when vowel {
                iso:'i' -> return $82B5  ; し (si→shi扱い)
            }
            iso:'t' -> when vowel {
                iso:'i' -> return $82BF  ; ち (ti→chi扱い)  
                iso:'u' -> return $82C2  ; つ (tu→tsu扱い)
            }
            iso:'h' -> when vowel {
                iso:'u' -> return $82D3  ; ふ (hu→fu扱い)
            }
            iso:'f' -> when vowel {
                iso:'a' -> {  ; fa → ふぁ
                    add_to_hiragana_buffer($82D3)  ; ふ
                    add_to_hiragana_buffer($829F)  ; ぁ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
                iso:'i' -> {  ; fi → ふぃ
                    add_to_hiragana_buffer($82D3)  ; ふ
                    add_to_hiragana_buffer($82A1)  ; ぃ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
                iso:'u' -> return $82D3  ; ふ (fu我い)
                iso:'e' -> {  ; fe → ふぇ
                    add_to_hiragana_buffer($82D3)  ; ふ
                    add_to_hiragana_buffer($82A5)  ; ぇ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
                iso:'o' -> {  ; fo → ふぉ
                    add_to_hiragana_buffer($82D3)  ; ふ
                    add_to_hiragana_buffer($82A7)  ; ぉ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
            }
        }
        return 0  ; 特殊変換なし
    }
    
    ; 基本子音+母音変換
    sub convert_basic(ubyte consonant, ubyte vowel) -> uword {
        ubyte row = consonant_to_row(consonant)
        ubyte vol = vowel_index(vowel)
        
        if row == 255 or vol == 255 {
            return 0  ; 不正な組み合わせ
        }
        
        ; や行は「や・ゆ・よ」の3文字のみ（i, e は存在しない）
        if row == 7 {  ; や行
            when vol {
                0 -> return basic_hiragana[35]  ; や (ya)
                2 -> return basic_hiragana[36]  ; ゆ (yu)  
                4 -> return basic_hiragana[37]  ; よ (yo)
                else -> return 0  ; yi, ye は存在しない
            }
        }
        
        ; わ行は「わ・を」の2文字のみ（wi, weは特殊処理）
        if row == 9 {  ; わ行
            when vol {
                0 -> return basic_hiragana[43]  ; わ (wa)
                1 -> {  ; wi → うぃ
                    add_to_hiragana_buffer($82A4)  ; う
                    add_to_hiragana_buffer($82A1)  ; ぃ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
                3 -> {  ; we → うぇ
                    add_to_hiragana_buffer($82A4)  ; う
                    add_to_hiragana_buffer($82A5)  ; ぇ (小文字)
                    return $FFFF  ; 成功フラグ（無効な文字コード）
                }
                4 -> return basic_hiragana[44]  ; を (wo)
                else -> return 0  ; wu は存在しない
            }
        }
        
        ; 通常の行（か、さ、た、な、は、ま、ら行）
        if row >= 1 and row <= 8 and row != 7 {  ; や行以外
            ubyte index
            if row < 7 {
                ; か〜ま行 (row 1-6)
                index = (row - 1) * 5 + 5 + vol  ; あ行(5文字)をスキップ
            } else {
                ; ら行 (row 8)
                index = 38 + vol  ; ら行の開始位置 (38)
            }
            return basic_hiragana[index]
        }
        
        return 0
    }
    
    ; 濁音変換 (ga, gi, gu, ge, go, za, zi, zu, ze, zo, da, di, du, de, do, ba, bi, bu, be, bo等)
    sub convert_dakuten(ubyte consonant, ubyte vowel) -> uword {
        ubyte vol = vowel_index(vowel)
        if vol > 4 return 0

        ubyte base = 255
        when consonant {
            iso:'g' -> base = 0
            iso:'z' -> base = 5
            iso:'d' -> base = 10
            iso:'b' -> base = 15
            else -> return 0
        }
        return dakuten_hiragana[base + vol]
    }
    
    ; 半濁音変換 (pa, pi, pu, pe, po)
    sub convert_handakuten(ubyte consonant, ubyte vowel) -> uword {
        if consonant == iso:'p' {
            ubyte vol = vowel_index(vowel)
            if vol <= 4 return handakuten_hiragana[vol]
        }
        return 0
    }
    
    ; 小文字変換 (xa→ぁ, xi→ぃ等)
    sub convert_small(ubyte vowel_or_y) -> uword {
        when vowel_or_y {
            iso:'a' -> return small_hiragana[0]  ; ぁ
            iso:'i' -> return small_hiragana[1]  ; ぃ  
            iso:'u' -> return small_hiragana[2]  ; ぅ
            iso:'e' -> return small_hiragana[3]  ; ぇ
            iso:'o' -> return small_hiragana[4]  ; ぉ
            iso:'y' -> return small_hiragana[5]  ; ゃ (xya)
        }
        return 0
    }
    
    ; 拗音変換 (kya→きゃ, shu→しゅ, cho→ちょ等)
    sub convert_youon(ubyte consonant, ubyte y_vowel) -> uword {
        ; 基本子音のi段を取得
        uword base_i = 0
        ubyte base_index = 0

        when consonant {
            iso:'k' -> { base_index = 6  }   ; き
            iso:'s' -> { base_index = 11 }  ; し  
            iso:'t' -> { base_index = 16 }  ; ち
            iso:'n' -> { base_index = 21 }  ; に
            iso:'h' -> { base_index = 26 }  ; ひ
            iso:'f' -> { base_index = 27 }  ; ふ (fu系) - 27はふのインデックス
            iso:'m' -> { base_index = 31 }  ; み
            iso:'r' -> { base_index = 39 }  ; り
            iso:'g' -> { base_index = 1 | $40 } ; ぎ
            iso:'z' -> { base_index = 6 | $40 } ; じ
            iso:'j' -> { base_index = 6 | $40 } ; じ (jも同じくじ)
            iso:'d' -> { base_index = 11 | $40 } ; ぢ  
            iso:'b' -> { base_index = 16 | $40 } ; び
            iso:'p' -> { base_index = 1 | $80 } ; ぴ
            else -> return 0  ; 拗音対応なし
        }
        when base_index & $c0 {
            $00 -> base_i = basic_hiragana[base_index & $3f]
            $40 -> base_i = dakuten_hiragana[base_index & $3f]
            $80 -> base_i = handakuten_hiragana[base_index & $3f]
        }
        if base_i == 0 return 0
        
        ; 小さい「や・ゆ・よ」を取得
        uword small_ya = 0
        when y_vowel {
            iso:'a' -> small_ya = small_hiragana[5]  ; ゃ
            iso:'u' -> small_ya = small_hiragana[6]  ; ゅ  
            iso:'o' -> small_ya = small_hiragana[7]  ; ょ
            else -> return 0
        }
        
        ; 2文字分をひらがなバッファに追加
        add_to_hiragana_buffer(base_i)
        add_to_hiragana_buffer(small_ya)
        return $FFFF  ; 成功フラグ（無効な文字コード）
    }
    
    ; 特殊拗音変換 (sha→しゃ, chu→ちゅ, cho→ちょ等)
    sub convert_special_youon(ubyte ch1, ubyte ch2, ubyte y_vowel) -> uword {
        ; 特殊2文字パターンのベースを取得
        uword base = 0
        
        ; ch + 母音 -> ち + 小文字
        if ch1 == iso:'c' and ch2 == iso:'h' {
            base = $82BF  ; ち
        }
        ; sh + 母音 -> し + 小文字  
        else if ch1 == iso:'s' and ch2 == iso:'h' {
            base = $82B5  ; し
        }
        ; ts は拗音なし
        else {
            return 0
        }
        
        ; 小さい「や・ゆ・よ」を取得
        uword small_ya = 0
        when y_vowel {
            iso:'a' -> small_ya = small_hiragana[5]  ; ゃ
            iso:'u' -> small_ya = small_hiragana[6]  ; ゅ  
            iso:'o' -> small_ya = small_hiragana[7]  ; ょ
            else -> return 0
        }
        
        ; 2文字分をひらがなバッファに追加
        add_to_hiragana_buffer(base)
        add_to_hiragana_buffer(small_ya)
        return $FFFF  ; 成功フラグ（無効な文字コード）
    }
    
    ; ひらがなバッファに文字を追加
    sub add_to_hiragana_buffer(uword sjis_char) {
        if hiragana_pos >= 62 return  ; バッファオーバーフロー防止
        
        ; カタカナモード時はひらがなをカタカナに変換
        if ime_input_mode == IME_MODE_KATAKANA {
            ubyte first_byte = lsb(sjis_char >> 8)
            ubyte second_byte = lsb(sjis_char)
            if first_byte >= $82 and second_byte >= $9f and second_byte <= $f1 {
                first_byte = $83
                if second_byte <= $dd {
                    second_byte = second_byte - $5f
                } else {
                    second_byte = second_byte - $5e
                }
                sjis_char = mkword(first_byte, second_byte)
            }
        }
        
        ; Shift-JISの2バイト文字をバッファに格納
        hiragana_buffer[hiragana_pos] = lsb(sjis_char >> 8)      ; 上位バイト
        hiragana_buffer[hiragana_pos + 1] = lsb(sjis_char)       ; 下位バイト
        hiragana_pos += 2
    }
    
    ; 促音処理 (kka→っか, tta→った等)
    sub add_small_tsu() {
        add_to_hiragana_buffer(small_hiragana[8])  ; っ
    }
    
    ; 撥音処理 (n→ん)
    sub add_n() {
        add_to_hiragana_buffer(basic_hiragana[45])  ; ん
    }
    
    ; IME初期化
    sub init() {
        ; IMEは初期状態でOFF
        ime_active = false
        
        ; 初期入力モードはひらがな
        ime_input_mode = IME_MODE_HIRAGANA
        
        ; バッファクリア
        clear_romaji_buffer()
        clear_hiragana_buffer()
        clear_conversion_key_buffer()
        
        ; 状態リセット
        prev_commodore_state = false
        prev_space_state = false
        prev_display_length = 0
        prev_display_chars = 0
        ime_has_output = false
        ime_output_length = 0
        
        ; 変換状態リセット
        ime_conversion_state = IME_STATE_INPUT
        candidate_count = 0
        current_candidate = 0
        conversion_key_length = 0
    }
    
    ; ローマ字バッファをクリア
    sub clear_romaji_buffer() {
        romaji_pos = 0
        romaji_state = ROMAJI_EMPTY
        last_consonant = 0
        second_consonant = 0
        ; バッファの内容もクリア
        sys.memset(&romaji_buffer, 8, 0)
    }
    
    ; メインローマ字入力処理
    sub input_romaji(ubyte key) -> bool {
        ; 制御文字は処理しない
        if key < 32 or key > 126 return false
        
        ; バッファがフルの場合は強制確定
        if romaji_pos >= 7 {
            force_confirm_romaji()
        }
        
        ; ローマ字バッファに追加
        romaji_buffer[romaji_pos] = key
        romaji_pos++
        
        ; デバッグ出力
        debug_print_state(key)
        
        ; 状態機械による変換判定
        when romaji_state {
            ROMAJI_EMPTY -> {
                return handle_empty_state(key)
            }
            ROMAJI_CONSONANT -> {
                return handle_consonant_state(key)
            }
            ROMAJI_N -> {
                return handle_n_state(key)
            }
            ROMAJI_SMALL_TSU -> {
                return handle_small_tsu_state(key)
            }
            ROMAJI_WAITING_2ND -> {
                return handle_waiting_2nd_state(key)
            }
            ROMAJI_X_PREFIX -> {
                return handle_x_prefix_state(key)
            }
            ROMAJI_Y_WAITING -> {
                return handle_y_waiting_state(key)
            }
            ROMAJI_SKIP_NEXT -> {
                return handle_skip_next_state(key)
            }
            else -> {
                clear_romaji_buffer()
                return false
            }
        }
    }
    
    ; デバッグ用状態表示
    sub debug_print_state(ubyte key) {
        ; jtxtはすでにインポート済み
        ; jtxt.bputs(iso:"KEY:")
        ; jtxt.bputc(key)
        ; jtxt.bputs(iso:" ST:")
        ; jtxt.bputc('0' + romaji_state)
        ; jtxt.bputs(iso:" LST:")
        ; if last_consonant != 0 jtxt.bputc(last_consonant)
        ; jtxt.bputs(iso:" 2ND:")
        ; if second_consonant != 0 jtxt.bputc(second_consonant)
        ; jtxt.bputc(13)  ; 改行
    }
    
    ; 空状態の処理
    sub handle_empty_state(ubyte key) -> bool {
        ; 句読点の直接変換
        when key {
            iso:'-' -> {
                add_to_hiragana_buffer($815B)  ; ー (長音符)
                clear_romaji_buffer()
                return true
            }
            iso:',' -> {
                add_to_hiragana_buffer($8141)  ; 、(読点)
                clear_romaji_buffer()
                return true
            }
            iso:'.' -> {
                add_to_hiragana_buffer($8142)  ; 。(句点)
                clear_romaji_buffer()
                return true
            }
        }
        
        ; 母音の場合、そのまま変換
        ubyte vol = vowel_index(key)
        if vol != 255 {
            uword result = basic_hiragana[vol]  ; あいうえお
            add_to_hiragana_buffer(result)
            clear_romaji_buffer()
            return true
        }
        
        ; 'n' の場合、撥音の可能性
        if key == iso:'n' {
            romaji_state = ROMAJI_N
            last_consonant = iso:'n'
            return true  ; まだ確定しない
        }
        
        ; 'x' の場合、小文字プレフィックス
        if key == iso:'x' {
            romaji_state = ROMAJI_X_PREFIX
            return true
        }
        
        ; 子音の場合
        ubyte row = consonant_to_row(key)
        
        ; 濁音・半濁音・j子音も処理する
        if row != 255 or key == iso:'g' or key == iso:'z' or key == iso:'d' or key == iso:'b' or key == iso:'p' or key == iso:'j' {
            ; 2文字特殊パターンの先頭チェック (s, c, t, f, d)
            if key == iso:'s' or key == iso:'c' or key == iso:'t' or key == iso:'f' or key == iso:'d' {
                romaji_state = ROMAJI_WAITING_2ND
                last_consonant = key
                return true
            }
            
            ; 通常の子音（濁音・半濁音・j含む）
            romaji_state = ROMAJI_CONSONANT
            last_consonant = key
            return true
        }
        
        clear_romaji_buffer()
        return false
    }
    
    ; 子音状態の処理
    sub handle_consonant_state(ubyte key) -> bool {
        ; 促音チェック（同じ子音の連続）
        if key == last_consonant and key != iso:'n' {
            add_small_tsu()
            romaji_state = ROMAJI_CONSONANT  ; 続けて処理
            return true
        }
        
        ; jの特殊処理：母音は「い」のみ、それ以外は拗音
        if last_consonant == iso:'j' {
            if key == iso:'i' {
                ; ji -> じ
                add_to_hiragana_buffer($82B6)  ; じ
                clear_romaji_buffer()
                return true
            }
            if key == iso:'a' or key == iso:'u' or key == iso:'o' {
                ; ja, ju, jo -> じゃ, じゅ, じょ
                add_to_hiragana_buffer($82B6)  ; じ
                when key {
                    iso:'a' -> add_to_hiragana_buffer($82E1)  ; ゃ
                    iso:'u' -> add_to_hiragana_buffer($82E3)  ; ゅ
                    iso:'o' -> add_to_hiragana_buffer($82E5)  ; ょ
                }
                clear_romaji_buffer()
                return true
            }
            if key == iso:'e' {
                ; je -> じぇ
                add_to_hiragana_buffer($82B6)  ; じ
                add_to_hiragana_buffer($82A5)  ; ぇ
                clear_romaji_buffer()
                return true
            }
            if key == iso:'y' {
                ; jy -> 拗音待ち状態へ
                romaji_state = ROMAJI_Y_WAITING
                return true
            }
            ; jの後に無効な文字が来た場合
            clear_romaji_buffer()
            return false
        }
        
        ; 母音の場合、変換実行
        ubyte vol = vowel_index(key)
        if vol != 255 {
            uword result = 0
            
            ; 特殊変換を先にチェック
            result = convert_special_1char(last_consonant, key)
            if result == 0 {
                ; 濁音・半濁音チェック
                result = convert_dakuten(last_consonant, key)
                if result == 0 {
                    result = convert_handakuten(last_consonant, key)
                    if result == 0 {
                        ; 基本変換
                        result = convert_basic(last_consonant, key)
                    }
                }
            }
            
            if result != 0 {
                ; $FFFFは成功フラグだが、文字としては追加しない
                if result != $FFFF {
                    add_to_hiragana_buffer(result)
                }
                clear_romaji_buffer()
                return true
            }
        }
        
        ; 'y' の場合、拗音の可能性
        if key == iso:'y' {
            romaji_state = ROMAJI_Y_WAITING
            return true
        }
        
        clear_romaji_buffer()
        return false
    }
    
    ; n状態の処理
    sub handle_n_state(ubyte key) -> bool {
        ; n + n の場合、撥音として確定
        if key == iso:'n' {
            add_n()
            clear_romaji_buffer()
            return true
        }
        
        ; 子音が来た場合、撥音として確定（j, g, z, d, b, p も含む）
        ubyte row = consonant_to_row(key)
        if row != 255 and key != iso:'y' {
            add_n()
            clear_romaji_buffer()
            ; 次の子音を処理するため再帰呼び出し
            return input_romaji(key)
        }
        
        ; j, g, z, d, b, p も子音として扱う
        if key == iso:'j' or key == iso:'g' or key == iso:'z' or key == iso:'d' or key == iso:'b' or key == iso:'p' {
            add_n()
            clear_romaji_buffer()
            ; 次の子音を処理するため再帰呼び出し
            return input_romaji(key)
        }
        
        ; 母音またはyが来た場合、na, ni, nu, ne, no または nya, nyu, nyo
        ubyte vol = vowel_index(key)
        if vol != 255 {
            uword result = convert_basic(iso:'n', key)
            if result != 0 {
                ; $FFFFは成功フラグだが、文字としては追加しない
                if result != $FFFF {
                    add_to_hiragana_buffer(result)
                }
                clear_romaji_buffer()
                return true
            }
        }
        
        if key == iso:'y' {
            romaji_state = ROMAJI_Y_WAITING
            return true
        }
        
        clear_romaji_buffer()
        return false
    }
    
    ; 小さいっ状態の処理（促音後）
    sub handle_small_tsu_state(ubyte key) -> bool {
        ; 前回と同じ処理を継続
        clear_romaji_buffer()
        return input_romaji(key)
    }
    
    ; 2文字目待ち状態の処理 (s, c, t, f の後)
    sub handle_waiting_2nd_state(ubyte key) -> bool {
        ; 促音チェック（同じ子音の連続）- sshu, cchi, tti等
        if key == last_consonant {
            add_small_tsu()
            ; 同じ状態を維持して次の文字を待つ
            return true
        }

        ; 'h' だけは拗音/shi, chi, dhi へ繋げるため特別扱い
        if key == iso:'h' {
            second_consonant = key
            romaji_state = ROMAJI_Y_WAITING
            return true
        }

        ; ts -> つ
        if last_consonant == iso:'t' and key == iso:'s' {
            add_to_hiragana_buffer($82C2)  ; つ
            clear_romaji_buffer()
            romaji_state = ROMAJI_SKIP_NEXT
            return true
        }

        ; その他は通常の子音処理へ（tu, fu などは convert_special_1char で処理）
        romaji_state = ROMAJI_CONSONANT
        return handle_consonant_state(key)
    }
    
    ; x プレフィックス状態の処理
    sub handle_x_prefix_state(ubyte key) -> bool {
        uword result = convert_small(key)
        if result != 0 {
            add_to_hiragana_buffer(result)
            clear_romaji_buffer()
            return true
        }
        
        clear_romaji_buffer()
        return false
    }
    
    ; y待ち状態の処理（拗音または直接母音）
    sub handle_y_waiting_state(ubyte key) -> bool {
        uword result = 0
        
        ; second_consonant == 'h' の場合の処理（sh, ch系）
        if second_consonant == iso:'h' {
            ; 母音チェック
            ubyte vol = vowel_index(key)
            if vol != 255 {
                when key {
                    iso:'i' -> {
                        ; shi, chi, dhi
                        if last_consonant == iso:'d' {
                            ; dhi -> でぃ
                            add_to_hiragana_buffer($82C5)  ; で
                            add_to_hiragana_buffer($82A1)  ; ぃ (小文字)
                            clear_romaji_buffer()
                            return true
                        } else {
                            ; shi, chi
                            result = convert_special_2char(last_consonant, second_consonant)
                            if result != 0 {
                                add_to_hiragana_buffer(result)
                                clear_romaji_buffer()
                                return true
                            }
                        }
                    }
                    iso:'a', iso:'u', iso:'o' -> {
                        if last_consonant == iso:'d' {
                            ; dha, dhu, dho -> でゃ, でゅ, でょ
                            add_to_hiragana_buffer($82C5)  ; で
                            when key {
                                iso:'a' -> add_to_hiragana_buffer($82E1)  ; ゃ
                                iso:'u' -> add_to_hiragana_buffer($82E3)  ; ゅ  
                                iso:'o' -> add_to_hiragana_buffer($82E5)  ; ょ
                            }
                            clear_romaji_buffer()
                            return true
                        } else {
                            ; sha, shu, cho
                            result = convert_special_youon(last_consonant, second_consonant, key)
                            if result != 0 {
                                clear_romaji_buffer()  ; convert_special_youon内でバッファ追加済み
                                return true
                            }
                        }
                    }
                    iso:'e' -> {
                        if last_consonant == iso:'d' {
                            ; dhe -> でぇ  
                            add_to_hiragana_buffer($82C5)  ; で
                            add_to_hiragana_buffer($82A5)  ; ぇ (小文字)
                            clear_romaji_buffer()
                            return true
                        }
                    }
                }
            }
        } else {
            ; 通常の拗音 (kya, nyu等)
            if key == iso:'a' or key == iso:'u' or key == iso:'o' {
                result = convert_youon(last_consonant, key)
                if result != 0 {
                    clear_romaji_buffer()  ; convert_youon内でバッファ追加済み
                    return true
                }
            }
        }
        
        clear_romaji_buffer()
        return false
    }
    
    ; 次文字スキップ状態の処理（TSU等で使用）
    sub handle_skip_next_state(ubyte key) -> bool {
        ; 1文字スキップして空状態に戻る
        clear_romaji_buffer()
        return true  ; 処理済み（文字は無視）
    }
    
    ; 強制確定（バッファフル時）
    sub force_confirm_romaji() {
        if romaji_state == ROMAJI_N {
            add_n()
        }
        clear_romaji_buffer()
    }
    
    ; バックスペース処理
    sub backspace_romaji() -> bool {
        ; ローマ字バッファが空の場合、ひらがなバッファから削除
        if romaji_pos == 0 {
            if hiragana_pos >= 2 {
                hiragana_pos -= 2  ; Shift-JISは2バイト文字
                return true
            }
            return false  ; 削除できない
        }
        
        ; ローマ字バッファから1文字削除
        romaji_pos--
        
        ; 状態を再計算（バッファの内容に基づいて状態を復元）
        recalculate_state()
        
        return true
    }
    
    ; ローマ字バッファの内容に基づいて状態を再計算
    sub recalculate_state() {
        ; バッファが空になった場合
        if romaji_pos == 0 {
            clear_romaji_buffer()
            return
        }
        
        ; 最後の文字を確認して状態を設定
        ubyte last_char = romaji_buffer[romaji_pos - 1]
        
        ; 1文字の場合の状態判定
        if romaji_pos == 1 {
            if last_char == iso:'n' {
                romaji_state = ROMAJI_N
                last_consonant = iso:'n'
            } else if last_char == iso:'x' {
                romaji_state = ROMAJI_X_PREFIX
                last_consonant = 0
                second_consonant = 0
            } else if last_char == iso:'s' or last_char == iso:'c' or last_char == iso:'t' or last_char == iso:'f' or last_char == iso:'d' {
                romaji_state = ROMAJI_WAITING_2ND
                last_consonant = last_char
                second_consonant = 0
            } else {
                ; 通常の子音
                ubyte row = consonant_to_row(last_char)
                if row != 255 or last_char == iso:'g' or last_char == iso:'z' or last_char == iso:'d' or last_char == iso:'b' or last_char == iso:'p' or last_char == iso:'j' {
                    romaji_state = ROMAJI_CONSONANT
                    last_consonant = last_char
                    second_consonant = 0
                } else {
                    ; 母音など、EMPTY状態に戻る
                    clear_romaji_buffer()
                }
            }
            return
        }
        
        ; 2文字の場合の状態判定
        if romaji_pos == 2 {
            ubyte first_char = romaji_buffer[0]
            
            ; 2文字特殊パターン
            if (first_char == iso:'s' and last_char == iso:'h') or
               (first_char == iso:'c' and last_char == iso:'h') or
               (first_char == iso:'t' and last_char == iso:'s') {
                romaji_state = ROMAJI_Y_WAITING
                last_consonant = first_char
                second_consonant = last_char
                return
            }
            
            ; 拗音パターン（子音+y）
            if last_char == iso:'y' {
                romaji_state = ROMAJI_Y_WAITING
                last_consonant = first_char
                second_consonant = 0
                return
            }
            
            ; その他は通常の子音状態として扱う
            romaji_state = ROMAJI_CONSONANT
            last_consonant = first_char
            second_consonant = 0
            return
        }
        
        ; 3文字以上の場合（まれ）
        ; 一旦クリアして最後の1文字から再構築
        ubyte saved_char = last_char
        clear_romaji_buffer()
        romaji_buffer[0] = saved_char
        romaji_pos = 1
        recalculate_state()
    }
    
    ; ひらがなバッファを取得
    sub get_hiragana_buffer() -> uword {
        return &hiragana_buffer
    }
    
    ; ひらがなバッファ長を取得
    sub get_hiragana_length() -> ubyte {
        return hiragana_pos
    }
    
    ; ひらがなバッファをクリア
    sub clear_hiragana_buffer() {
        hiragana_pos = 0
    }
    
    ; 変換キーバッファをクリア
    sub clear_conversion_key_buffer() {
        sys.memset(&conversion_key_buffer, 64, 0)  ; 64バイトを0でクリア
    }
    
    ; ひらがな文字列をカタカナに変換
    sub convert_to_katakana(uword target_ptr) {
        ubyte i = 0
        ubyte ch
        repeat {
            if @(target_ptr + i) == 0 break
            if @(target_ptr + i) < $80 {
                i++
                continue
            }
            ch = @(target_ptr + i + 1)
            if @(target_ptr + i) >= $82 and ch >= $9f and ch <= $f1 {
                @(target_ptr + i) = $83
                if ch <= $dd {
                    @(target_ptr + i + 1) = ch - $5f
                } else {
                    @(target_ptr + i + 1) = ch - $5e
                }
            }
            i += 2
        }
    }
    
    ; カタカナ文字列をひらがなに変換
    sub convert_to_hiragana(uword target_ptr) {
        ubyte i = 0
        ubyte ch
        repeat {
            if @(target_ptr + i) == 0 break
            if @(target_ptr + i) < $80 {
                i++
                continue
            }
            ch = @(target_ptr + i + 1)
            if @(target_ptr + i) == $83 and ch >= $40 and ch <= $93 {
                @(target_ptr + i) = $82
                if ch <= $7e {
                    @(target_ptr + i + 1) = ch + $5f
                } else {
                    @(target_ptr + i + 1) = ch + $5e
                }
            }
            i += 2
        }
    }
    
    ; ==============================================================================
    ; Commodoreキー検出とIME管理機能
    ; ==============================================================================
    
    ; CIA1 キーマトリックス定数
    const uword CIA1_DATA_A = $DC00    ; キーマトリックス行選択
    const uword CIA1_DATA_B = $DC01    ; キーマトリックス列読み取り
    
    ; Commodoreキーの位置（行7, ビット5）
    const ubyte COMMODORE_ROW = %01111111   ; 行7を選択（ビット7を0にする）
    const ubyte COMMODORE_BIT = %00100000   ; ビット5（Commodoreキー）
    const ubyte SPACE_ROW = %01111111       ; 行7を選択（ビット7を0にする）
    const ubyte SPACE_BIT = %00010000       ; ビット4（スペースキー）
    
    ; IME状態管理
    bool ime_active = false
    bool prev_commodore_state = false
    bool prev_space_state = false
    
    ; 変換状態定数
    const ubyte IME_STATE_INPUT = 0      ; ローマ字入力中
    const ubyte IME_STATE_CONVERTING = 1 ; 変換候補選択中
    
    ; 変換状態変数
    ubyte ime_conversion_state = IME_STATE_INPUT
    
    ; 辞書検索用変数
    ubyte current_bank = DICTIONARY_START_BANK
    uword current_offset = 0
    
    ; 候補管理
    ubyte[256] candidates_buffer         ; 候補文字列バッファ
    ubyte[16] candidate_offsets          ; 各候補の開始位置
    ubyte candidate_buffer_pos = 0
    ubyte candidate_count = 0            ; 候補数
    ubyte current_candidate = 0          ; 現在選択中の候補番号
    
    ; 変換対象文字列
    ubyte[64] conversion_key_buffer      ; 変換対象のひらがな文字列
    ubyte conversion_key_length = 0
    
    ; IME入力モード定数
    const ubyte IME_MODE_HIRAGANA = 0    ; ひらがなモード
    const ubyte IME_MODE_KATAKANA = 1    ; カタカナモード  
    const ubyte IME_MODE_FULLWIDTH = 2   ; 全角英数モード
    
    ; 現在のIME入力モード（初期はひらがな）
    ubyte ime_input_mode = IME_MODE_HIRAGANA
    
    ; Commodoreキーの状態を取得
    sub is_commodore_pressed() -> bool {
        @(CIA1_DATA_A) = COMMODORE_ROW
        ubyte result = @(CIA1_DATA_B)
        return (result & COMMODORE_BIT) == 0  ; 0で押下
    }
    
    ; スペースキーの状態を取得
    sub is_space_pressed() -> bool {
        @(CIA1_DATA_A) = SPACE_ROW
        ubyte result = @(CIA1_DATA_B)
        return (result & SPACE_BIT) == 0  ; 0で押下
    }
    
    ; F1キーの状態を取得
    sub is_f1_pressed() -> bool {
        @(CIA1_DATA_A) = $FE  ; ROW $FE
        ubyte result = @(CIA1_DATA_B)
        return (result & $10) == 0  ; 0で押下
    }
    
    ; F3キーの状態を取得
    sub is_f3_pressed() -> bool {
        @(CIA1_DATA_A) = $FE  ; ROW $FE
        ubyte result = @(CIA1_DATA_B)
        return (result & $20) == 0  ; 0で押下
    }
    
    ; F5キーの状態を取得
    sub is_f5_pressed() -> bool {
        @(CIA1_DATA_A) = $FE  ; ROW $FE
        ubyte result = @(CIA1_DATA_B)
        return (result & $40) == 0  ; 0で押下
    }
    
    ; Commodore+スペースの組み合わせを検出
    sub check_commodore_space() -> bool {
        bool current_commodore = is_commodore_pressed()
        bool current_space = is_space_pressed()
        
        ; トリガー検出：Commodore押下中にスペースが押された瞬間のみ
        bool trigger = false
        if current_commodore and current_space and not prev_space_state {
            trigger = true
        }
        
        ; 状態を更新
        prev_commodore_state = current_commodore
        prev_space_state = current_space
        
        return trigger
    }
    
    ; IMEモードのトグル
    sub toggle_ime_mode() {
        ime_active = not ime_active
        if ime_active {
            activate_ime_input()
        } else {
            deactivate_ime_input()
        }
    }
    
    ; IMEモード状態を取得
    sub is_ime_active() -> bool {
        return ime_active
    }
    
    ; 入力モードをひらがなに切り替え（F1キー）
    sub set_hiragana_mode() {
        ime_input_mode = IME_MODE_HIRAGANA
        ; 既存のカタカナバッファをひらがなに変換
        if hiragana_pos > 0 {
            convert_to_hiragana(&hiragana_buffer)
            ; 全描画しなおし
            prev_display_chars = prev_display_length = 0
            update_ime_display()
        }
        if ime_active show_ime_status()
    }
    
    ; 入力モードをカタカナに切り替え（F3キー）
    sub set_katakana_mode() {
        ime_input_mode = IME_MODE_KATAKANA
        ; 既存のひらがなバッファをカタカナに変換
        if hiragana_pos > 0 {
            convert_to_katakana(&hiragana_buffer)
            ; 全描画しなおし
            prev_display_chars = prev_display_length = 0
            update_ime_display()
        }
        if ime_active show_ime_status()
    }
    
    ; 入力モードを英数に切り替え（F5キー）
    sub set_alphanumeric_mode() {
        ime_input_mode = IME_MODE_FULLWIDTH
        if ime_active show_ime_status()
    }
    
    ; 現在の入力モードを取得
    sub get_input_mode() -> ubyte {
        return ime_input_mode
    }
    
    ; IME入力領域の活性化（画面最下行をIME用に確保）
    sub activate_ime_input() {
        ; jtxtのウィンドウを23行まで（行0-23）に制限（24行目をIME用に確保）
        jtxt.bwindow(0, 23)  ; height=23行（0から22行目まで）
        
        ; IME入力行をクリア
        clear_ime_input_line()
        
        ; IME状態表示
        show_ime_status()
    }
    
    ; IME入力領域の非活性化
    sub deactivate_ime_input() {
        ; jtxtのウィンドウを全画面に復帰
        jtxt.bwindow_disable()
        
        ; IME入力行をクリア
        clear_ime_input_line()
        
        ; IME変換バッファもクリア
        clear_romaji_buffer()
        clear_hiragana_buffer()
        
        ; 変換状態リセット
        ime_conversion_state = IME_STATE_INPUT
        candidate_count = 0
        current_candidate = 0
        conversion_key_length = 0
    }
    
    ; IME入力行をクリア（24行目）
    sub clear_ime_input_line() {
        jtxt.bwindow_disable()  ; 一時的にウィンドウ制限を解除
        jtxt.bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG)  ; デフォルト色設定
        jtxt.blocate(0, 24)
        ubyte i
        for i in 0 to 39 {
            jtxt.bputc(32)  ; スペースで埋める
        }
        
        ; 表示状態をリセット
        prev_display_length = 0
        prev_display_chars = 0
        prev_romaji_pos = 0
        
        if ime_active {
            jtxt.bwindow_enable()  ; ウィンドウ制限を復帰
        }
    }
    
    ; IME状態を表示
    sub show_ime_status() {
        if ime_active {
            jtxt.bwindow_disable()  ; 一時的にウィンドウ制限を解除
            jtxt.blocate(37, 24)  ; 右端に表示（40-3=37）
            jtxt.bcolor(COLOR_STATUS_FG, COLOR_STATUS_BG)  ; 白背景に黒文字
            ; モードに応じて表示を変更
            when ime_input_mode {
                ; IME_MODE_HIRAGANA -> jtxt.bputs(iso:"[あ] ")
                IME_MODE_HIRAGANA -> jtxt.bputs([$5b, $82, $a0, $5d, 0])   ; '[' 'あ' ']'
                ; IME_MODE_KATAKANA -> jtxt.bputs(iso:"[ア] ")
                IME_MODE_KATAKANA -> jtxt.bputs([$5b, $83, $41, $5d, 0])   ; '[' 'ア' ']'
                ; IME_MODE_FULLWIDTH -> jtxt.bputs(iso:"[Ａ] ")
                IME_MODE_FULLWIDTH -> jtxt.bputs([$5b, $82, $60, $5d, 0])   ; '[' 'Ａ' ']'
            }
            jtxt.bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG)  ; デフォルト色に戻す
            jtxt.bwindow_enable()  ; ウィンドウ制限を復帰
        }
        ; IME OFFの場合は何も表示しない（最下部をアプリケーションに解放）
    }
    
    ; ==============================================================================
    ; IME入力処理の中核機能
    ; ==============================================================================
    
    ; IME出力バッファ（元のサイズに戻して問題を再現）
    ubyte[128] ime_output_buffer    ; 確定後の出力文字列（Shift-JIS）
    ubyte ime_output_length = 0
    bool ime_has_output = false
    
    ; 表示最適化用変数
    ubyte prev_display_length = 0  ; 前回表示した長さ（バイト単位）
    ubyte prev_display_chars = 0   ; 前回表示した文字数（画面位置用）
    ubyte prev_romaji_pos = 0       ; 前回表示したローマ字位置
    
    ; Shiftキーの状態を取得
    sub is_shift_pressed() -> bool {
        ; キーボードマトリックスのRow 1を選択（$FD）
        @(CIA1_DATA_A) = $FD
        ; Column値を読み取り、bit 7がLeft Shift
        ubyte cols = @(CIA1_DATA_B)
        return (cols & $80) == 0  ; 0の時に押されている
    }
    
    ; キー入力をIMEで処理（戻り値：IMEが処理した場合true）
    sub process_ime_key(ubyte key) -> bool {
        if not ime_active {
            return false  ; IME非活性時は処理しない
        }
        
        ; F1、F3、F5キーでモード切り替え
        if is_f1_pressed() {
            set_hiragana_mode()
            return true
        }
        if is_f3_pressed() {
            set_katakana_mode()
            return true
        }
        if is_f5_pressed() {
            set_alphanumeric_mode()
            return true
        }
        
        when key {
            13 -> {  ; RETURN - 確定 or 変換確定
                when ime_conversion_state {
                    IME_STATE_INPUT -> {
                        confirm_ime_input()
                    }
                    IME_STATE_CONVERTING -> {
                        confirm_conversion()
                        update_ime_display()
                    }
                }
                return true
            }
            27 -> {  ; ESC - キャンセル or 変換キャンセル
                when ime_conversion_state {
                    IME_STATE_INPUT -> {
                        cancel_ime_input()
                    }
                    IME_STATE_CONVERTING -> {
                        cancel_conversion()
                    }
                }
                return true
            }
            8, 20 -> {  ; BACKSPACE/DELETE
                when ime_conversion_state {
                    IME_STATE_INPUT -> {
                        backspace_ime_input()
                    }
                    IME_STATE_CONVERTING -> {
                        ; 変換状態ではバックスペースで変換をキャンセルし、文字削除
                        cancel_conversion()
                        backspace_ime_input()
                    }
                }
                return true
            }
            32 -> {  ; SPACE - 変換開始 or 次候補
                ; カタカナモード時はスペース変換無効
                if ime_input_mode == IME_MODE_KATAKANA {
                    input_ime_char(32)
                } else {
                    when ime_conversion_state {
                        IME_STATE_INPUT -> {
                            ; 変換開始
                            if start_conversion() {
                                update_ime_display()
                            } else {
                                ; 変換候補がない場合はスペースを入力
                                input_ime_char(32)
                            }
                        }
                        IME_STATE_CONVERTING -> {
                            ; 次候補選択
                            next_candidate()
                            update_ime_display()
                        }
                    }
                }
                return true
            }
            160 -> {  ; Shift+SPACE ($A0) - 前候補選択
                ; カタカナモード時はShift+Spaceも無効
                if ime_input_mode != IME_MODE_KATAKANA and ime_conversion_state == IME_STATE_CONVERTING {
                    ; 前候補選択
                    prev_candidate()
                    update_ime_display()
                    return true
                }
                ; 入力状態またはカタカナモードの場合は無視
                return false
            }
            else -> {
                ; xキーも含めて通常の文字として処理
                ; 印字可能文字の処理
                if key >= 32 and key <= 126 {
                    when ime_conversion_state {
                        IME_STATE_INPUT -> {
                            input_ime_char(key)
                        }
                        IME_STATE_CONVERTING -> {
                            ; 変換状態では新しい文字入力で変換を確定してから新しい入力開始
                            confirm_conversion()
                            update_ime_display()
                            input_ime_char(key)
                        }
                    }
                    return true
                }
            }
        }
        
        return false
    }
    
    ; IME文字入力
    sub input_ime_char(ubyte key) {
        ; 大文字を小文字に変換
        ubyte processed_key = key
        if key >= iso:'A' and key <= iso:'Z' {
            processed_key = key + 32
        }
        
        ; ローマ字→ひらがな変換を実行
        bool converted = input_romaji(processed_key)
        
        ; IME入力行を更新
        update_ime_display()
    }
    
    ; IME入力のバックスペース処理
    sub backspace_ime_input() {
        bool deleted = backspace_romaji()
        if deleted {
            update_ime_display()
        }
    }
    
    ; IME入力の確定
    sub confirm_ime_input() {
        ; 現在のひらがなバッファを出力バッファにコピー
        ime_output_length = hiragana_pos
        if ime_output_length > 0 {
            ubyte i
            for i in 0 to ime_output_length-1 {
                ime_output_buffer[i] = hiragana_buffer[i]
            }
            ime_has_output = true
        }
        
        ; バッファクリア
        clear_romaji_buffer()
        clear_hiragana_buffer()
        
        ; 表示状態をリセット
        prev_display_length = 0
        prev_display_chars = 0
        prev_romaji_pos = 0
        
        ; IME入力行の入力部分をクリア（確定後は表示しない）
        clear_ime_input_area()
        
        ; 表示更新
        update_ime_display()
    }
    
    ; IME入力のキャンセル
    sub cancel_ime_input() {
        clear_romaji_buffer()
        clear_hiragana_buffer()
        
        ; 表示状態をリセット
        prev_display_length = 0
        prev_display_chars = 0
        prev_romaji_pos = 0
        
        update_ime_display()
    }
    
    ; IME表示を更新（入力・変換状態対応版）
    sub update_ime_display() {
        jtxt.bwindow_disable()  ; 一時的にウィンドウ制限を解除
        jtxt.bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG)  ; デフォルト色設定
        
        when ime_conversion_state {
            IME_STATE_INPUT -> {
                display_input_text()
            }
            IME_STATE_CONVERTING -> {
                display_conversion_candidates()
            }
        }
        
        jtxt.bwindow_enable()  ; ウィンドウ制限を復帰
    }
    
    ; 入力中のテキストを表示（確定ひらがな + 未確定ローマ字）
    sub display_input_text() {
        ubyte current_length = hiragana_pos  ; 確定ひらがなのバイト単位
        ubyte current_chars = bytes_to_chars(current_length)  ; 確定ひらがなの文字数
        ubyte current_romaji = romaji_pos    ; 未確定ローマ字の文字数
        ubyte total_chars = current_chars + current_romaji  ; 総文字数
        ubyte prev_total_chars = prev_display_chars + prev_romaji_pos
        ubyte i
        
        ; 表示内容が変わった場合のみ更新
        bool needs_update = (current_length != prev_display_length) or 
                           (current_romaji != prev_romaji_pos)
        
        if needs_update {
            ; 表示可能な範囲をチェック
            if total_chars < 37 {  ; 右端のIME状態表示を避ける
                jtxt.blocate(0, 24)
                
                ; 確定ひらがな部分を表示
                if current_length > 0 {
                    for i in 0 to current_length-1 {
                        jtxt.bputc(hiragana_buffer[i])
                    }
                }
                
                ; 未確定ローマ字部分を表示（半角文字として）
                if current_romaji > 0 {
                    for i in 0 to current_romaji-1 {
                        jtxt.bputc(romaji_buffer[i])
                    }
                }
                
                ; 必要な分だけ末尾をクリア（前回より短くなった場合）
                if total_chars < prev_total_chars {
                    for i in total_chars to prev_total_chars-1 {
                        jtxt.bputc(32)
                    }
                }
            }
            
            ; 表示状態を更新
            prev_display_length = current_length
            prev_display_chars = current_chars
            prev_romaji_pos = current_romaji
        }
    }
    
    ; 変換候補を表示
    sub display_conversion_candidates() {
        ; 入力エリアをクリア
        clear_ime_input_area()
        
        if candidate_count == 0 return
        
        ; 現在の候補を表示
        ; 最下行なのでウィンドウ制限を解除
        jtxt.bwindow_disable()
        jtxt.blocate(0, 24)

        uword candidate_str = get_current_candidate()
        if candidate_str != 0 {
            ; 現在の候補を表示
            ubyte i = 0
            ubyte char
            ubyte col = 0
            repeat {
                char = @(candidate_str + i)
                if char == 0 break
                if col >= 20 break  ; 表示幅制限
                jtxt.bputc(char)
                if jtxt.is_firstsjis(char) {
                    ; 2バイト文字の場合、2文字目も出力
                    i++
                    if i < 128 {
                        jtxt.bputc(@(candidate_str + i))
                    }
                    col += 2
                } else {
                    col++
                }
                i++
            }
        }
        
        ; 候補番号を表示
        jtxt.bputc(32)  ; スペース

        ; ubyte[] num_str = [iso:'(', iso:'1', iso:'/', iso:'1', iso:')', 0]
        ; num_str[1] = '1' + current_candidate
        ; num_str[3] = '1' + candidate_count - 1
        ; jtxt.bputs(&num_str)
        jtxt.bputs(conv.str_ub(current_candidate))
        jtxt.bputc(iso:'/')
        jtxt.bputs(conv.str_ub(candidate_count - 1))

        
        ; 表示状態をリセット（変換表示は固定）
        prev_display_length = 0
        prev_display_chars = 0
    }
    
    ; 確定済み文字列を取得
    sub get_ime_output() -> uword {
        if ime_has_output {
            return &ime_output_buffer
        }
        return 0
    }
    
    ; 確定済み文字列の長さを取得
    sub get_ime_output_length() -> ubyte {
        if ime_has_output {
            return ime_output_length
        }
        return 0
    }
    
    ; 出力バッファをクリア
    sub clear_ime_output() {
        ime_has_output = false
        ime_output_length = 0
    }
    
    ; バイト位置から画面上の文字数を計算
    sub bytes_to_chars(ubyte byte_length) -> ubyte {
        if byte_length == 0 return 0
        
        ubyte char_count = 0
        ubyte i = 0
        
        while i < byte_length {
            if jtxt.is_firstsjis(hiragana_buffer[i]) {
                ; Shift-JIS 2バイト文字
                char_count++
                i += 2
            } else {
                ; ASCII等の1バイト文字
                char_count++
                i++
            }
        }
        
        return char_count
    }
    
    ; IME入力エリア（左端から右端のIME状態表示まで）をクリア
    sub clear_ime_input_area() {
        jtxt.bwindow_disable()  ; 一時的にウィンドウ制限を解除
        jtxt.bcolor(COLOR_DEFAULT_FG, COLOR_DEFAULT_BG)  ; デフォルト色設定
        jtxt.blocate(0, 24)  ; 左端から開始
        ubyte i
        for i in 0 to 36 {  ; 入力エリアのみクリア（右端のIME状態表示を避ける）
            jtxt.bputc(32)
        }
        if ime_active {
            jtxt.bwindow_enable()  ; ウィンドウ制限を復帰
        }
    }
    
    ; ==============================================================================
    ; 辞書検索機能
    ; ==============================================================================

    ubyte current_entry_length
    
    ; 8KB境界対応バイト読み込み
    sub read_dic_byte() -> ubyte {
        @(BANK_REG) = current_bank
        ubyte data = @(ROM_BASE + current_offset)
        current_offset++
        if current_offset >= 8192 {
            current_offset = 0
            current_bank++
        }
        return data
    }
    
    ; 辞書から文字列読み込み（NULL終端まで）
    sub read_dic_string(uword buffer) -> ubyte {
        ubyte length = 0
        ubyte char
        repeat {
            char = read_dic_byte()
            @(buffer + length) = char
            if char == 0 break
            length++
            if length >= 63 break  ; バッファオーバーフロー防止
        }
        return length
    }
    
    ; ひらがな1文字目をインデックスに変換
    sub hiragana_to_index(ubyte first_byte, ubyte second_byte) -> ubyte {
        uword ch = mkword(first_byte, second_byte)
        ch -= $82A0
        if ch >= 0 and ch <= 82 return lsb(ch)
        return 255
    }
    
    ; 送り仮名のマッチングチェック
    sub check_okurigana_match(uword okurigana_buffer, ubyte verb_suffix) -> bool {
        ; verb_suffixは半角文字（例：'k', 'r', 's'など）
        ; okurigana_bufferはひらがな（例：「く」「る」「す」など）
        
        if @(okurigana_buffer) != $82 return false  ; ひらがな以外
        ubyte second_byte = @(okurigana_buffer + 1)
        
        ; ローマ字から対応するひらがなを判定
        when verb_suffix {
            iso:'k' -> {
                ; か行：か($A9)、き($AB)、く($AD)、け($AF)、こ($B1)
                return second_byte >= $A9 and second_byte <= $B1 and (second_byte & 1) == 1
            }
            iso:'g' -> {
                ; が行：が($AA)、ぎ($AC)、ぐ($AE)、げ($B0)、ご($B2)
                return second_byte >= $AA and second_byte <= $B2 and (second_byte & 1) == 0
            }
            iso:'s' -> {
                ; さ行：さ($B3)、し($B5)、す($B7)、せ($B9)、そ($BB)
                return second_byte >= $B3 and second_byte <= $BB and (second_byte & 1) == 1
            }
            iso:'z', iso:'j' -> {
                ; ざ行：ざ($B4)、じ($B6)、ず($B8)、ぜ($BA)、ぞ($BC)
                return second_byte >= $B4 and second_byte <= $BC and (second_byte & 1) == 0
            }
            iso:'t' -> {
                ; た行：た($BD)、ち($BF)、つ($C2)、て($C4)、と($C6)、っ($C1)
                return second_byte == $BD or second_byte == $BF or 
                       second_byte == $C1 or second_byte == $C2 or 
                       second_byte == $C4 or second_byte == $C6
            }
            iso:'d' -> {
                ; だ行：だ($BE)、ぢ($C0)、づ($C3)、で($C5)、ど($C7)
                return second_byte == $BE or second_byte == $C0 or 
                       second_byte == $C3 or second_byte == $C5 or second_byte == $C7
            }
            iso:'n' -> {
                ; な行：な($C8)、に($C9)、ぬ($CA)、ね($CB)、の($CC)、ん($F1)
                return (second_byte >= $C8 and second_byte <= $CC) or second_byte == $F1
            }
            iso:'h' -> {
                ; は行：は($CD)、ひ($CE)、ふ($CF)、へ($D0)、ほ($D1)
                return second_byte >= $CD and second_byte <= $D1
            }
            iso:'b' -> {
                ; ば行：ば($D2)、び($D3)、ぶ($D4)、べ($D5)、ぼ($D6)
                return second_byte >= $D2 and second_byte <= $D6
            }
            iso:'p' -> {
                ; ぱ行：ぱ($D7)、ぴ($D8)、ぷ($D9)、ぺ($DA)、ぽ($DB)
                return second_byte >= $D7 and second_byte <= $DB
            }
            iso:'m' -> {
                ; ま行：ま($DC)、み($DD)、む($DE)、め($DF)、も($E0)
                return second_byte >= $DC and second_byte <= $E0
            }
            iso:'r' -> {
                ; ら行：ら($E7)、り($E8)、る($E9)、れ($EA)、ろ($EB)
                return second_byte >= $E7 and second_byte <= $EB
            }
            iso:'w' -> {
                ; わ行：わ($ED)、を($F0)、う($A4)
                return second_byte == $ED or second_byte == $F0 or second_byte == $A4
            }
            iso:'i' -> {
                ; い($A2)
                return second_byte == $A2
            }
            iso:'u' -> {
                ; う($A4)
                return second_byte == $A4
            }
            iso:'e' -> {
                ; え($A6)
                return second_byte == $A6
            }
            iso:'o' -> {
                ; お($A8)
                return second_byte == $A8
            }
        }
        
        return false
    }
    
    ; 名詞エントリを検索
    sub search_noun_entries(uword key_buffer, ubyte key_length) -> bool {
        if key_length < 2 return false  ; 最低2バイト（1文字）必要
        
        ; インデックスを取得（配列アクセスを修正）
        ubyte first_byte = @(key_buffer)
        ubyte second_byte = @(key_buffer + 1)
        
        ubyte index = hiragana_to_index(first_byte, second_byte)
        
        if index == 255 return false
        
        ; 名詞オフセットテーブルから開始位置を取得
        current_bank = DICTIONARY_START_BANK
        current_offset = 4 + (index as uword * 3)  ; +4はマジックナンバー分
        
        ; オフセット読み込み（3バイト：リトルエンディアン+バンク）
        ubyte offset_low = read_dic_byte()
        ubyte offset_high = read_dic_byte()
        ubyte offset_bank = read_dic_byte()
        
        
        ; エントリが存在しない場合
        if offset_low == 0 and offset_high == 0 and offset_bank == 0 {
            return false
        }
        
        ; バンクとオフセットを設定
        current_bank = DICTIONARY_START_BANK + offset_bank
        current_offset = mkword(offset_high, offset_low)
        
        return search_entries_in_group(key_buffer, key_length, false)  ; 名詞検索
    }
    
    ; 動詞エントリを検索
    sub search_verb_entries(uword key_buffer, ubyte key_length) -> bool {
        if key_length < 2 return false  ; 最低2バイト（1文字）必要
        
        ; インデックスを取得
        ubyte first_byte = @(key_buffer)
        ubyte second_byte = @(key_buffer + 1)
        
        ubyte index = hiragana_to_index(first_byte, second_byte)
        
        if index == 255 return false
        
        ; 動詞オフセットテーブルから開始位置を取得
        ; 動詞テーブルは名詞テーブルの後（+250バイト）
        current_bank = DICTIONARY_START_BANK
        current_offset = 250 + (index as uword * 3)  ; +250は名詞テーブル分
        
        ; オフセット読み込み（3バイト：リトルエンディアン+バンク）
        ubyte offset_low = read_dic_byte()
        ubyte offset_high = read_dic_byte()
        ubyte offset_bank = read_dic_byte()
        
        ; エントリが存在しない場合
        if offset_low == 0 and offset_high == 0 and offset_bank == 0 {
            return false
        }
        
        ; バンクとオフセットを設定
        current_bank = DICTIONARY_START_BANK + offset_bank
        current_offset = mkword(offset_high, offset_low)
        
        return search_entries_in_group(key_buffer, key_length, true)  ; 動詞検索
    }

    ; 動詞のマッチ情報
    ubyte verb_match_length
    ubyte verb_match_bank
    uword verb_match_offset
    uword verb_match_okurigana
    ubyte verb_candidate_count

    ubyte match_length
    ubyte match_bank
    uword match_offset
    uword match_okurigana
    ubyte match_candidate_count

    bool is_verb_first

    ; グループ内のエントリを検索（名詞・動詞共通）
    sub search_entries_in_group(uword key_buffer, ubyte key_length, bool is_verb_search) -> bool {
        candidate_count = 0
        ubyte entry_count = 0
        
        ; スキップサイズ読み込み（2バイト）
        ubyte skip_low = read_dic_byte()
        ubyte skip_high = read_dic_byte()
        uword skip_size = mkword(skip_high, skip_low) & $7FFF
        
        ; グループ先頭フラグチェック
        bool is_group_start = false
        repeat {
            ; キー文字列読み込み
            ubyte[64] entry_key_buffer
            ubyte entry_key_length = read_dic_string(&entry_key_buffer)
            
            entry_count++
            
            ; キー比較
            bool should_check = false
            
            if is_verb_search {
                ; 動詞検索の場合：エントリ末尾が半角文字か確認
                if entry_key_length > 0 {
                    if entry_key_buffer[entry_key_length - 1] < 128 {  ; ASCII文字（半角）
                        ; 送り仮名を除いた部分で比較
                        if key_length >= entry_key_length - 1 {
                            should_check = true
                        }
                    }
                }
            } else {
                ; 名詞検索の場合：通常の最長一致
                if key_length >= entry_key_length and entry_key_length > 0 {
                    should_check = true
                }
            }
            
            if should_check {
                bool match = true
                ubyte i
                ubyte compare_length = entry_key_length
                ubyte last_char = 0
                ubyte search_byte
                ubyte entry_byte

                ; 動詞の場合は送り文字の手前までの一致を得る
                if is_verb_search {
                    entry_key_length--
                }

                for i in 0 to entry_key_length-1 {
                    search_byte = @(key_buffer + i)
                    entry_byte = entry_key_buffer[i]
                    if search_byte != entry_byte {
                        match = false
                        break
                    }
                }

                if is_verb_search and match {
                    ; 送りの前までは一致したので、送り仮名をチェックする

                    ; 送りの文字(半角)
                    last_char = entry_key_buffer[entry_key_length]
                    if last_char < 128 {
                        ; 送り仮名のマッチング
                        match = check_okurigana_match(key_buffer + entry_key_length, 
                                                     last_char)
                        entry_key_length++
                    } else {
                        ; 送りがないぞ
                        match = false
                    }
                }
                
                if match {
                    ; マッチした文字列の長さを記録(動詞と名詞の一致サイズを比較して、長い方を先に候補に出すため)
                    match_length = entry_key_length
                    ; ROM位置を保存
                    match_bank = current_bank
                    match_offset = current_offset
                    match_okurigana = 0

                    current_entry_length = entry_key_length
                    if is_verb_search {
                        current_entry_length++
                    }

                    ; 動詞の場合、送り仮名を準備
                    if is_verb_search and key_length > entry_key_length - 1 {
                        match_okurigana = mkword(@(key_buffer + entry_key_length - 1), @(key_buffer + entry_key_length))
                    }

                    ; ここまでで一度戻る
                    return true
                    ; add_candidates(match_okurigana)
                    
                    ; if candidate_count > 0 {
                    ;     current_candidate = 0
                    ;     return true
                    ; }
                }
            }
            
            ; 次のエントリへスキップ（候補部分をスキップ）
            uword iw
            for iw in 0 to skip_size-1 {
                void read_dic_byte()
            }

            skip_low = read_dic_byte()
            skip_high = read_dic_byte()
            skip_size = mkword(skip_high, skip_low) & $7FFF
        
            ; グループ先頭フラグチェック
            is_group_start = (skip_high & $80) != 0

            ; グループ先頭に戻った場合は終了
            if is_group_start and candidate_count == 0 {
                break
            }
        }
        
        return candidate_count > 0
    }

    sub add_candidates(uword okurigana)
    {
        ; 候補数読み込み
        ubyte num_candidates = read_dic_byte()
        
        
        ; 各候補を読み込み
        ubyte i
        for i in 0 to num_candidates-1 {
            if candidate_count >= 15 break  ; 最大15候補
            
            candidate_offsets[candidate_count] = candidate_buffer_pos
            ubyte candidate_length = read_dic_string(&candidates_buffer + candidate_buffer_pos)
            candidate_buffer_pos += candidate_length + 1  ; NULL終端含む
            
            ; 動詞の場合、送り仮名を追加
            if okurigana != 0 {
                ; NULL終端を上書きして送り仮名を追加
                candidate_buffer_pos--  ; NULL終端の位置に戻る
                candidates_buffer[candidate_buffer_pos] = msb(okurigana)
                candidate_buffer_pos++
                candidates_buffer[candidate_buffer_pos] = lsb(okurigana)
                candidate_buffer_pos++
                candidates_buffer[candidate_buffer_pos] = 0  ; 新しいNULL終端
                candidate_buffer_pos++
            }
            
            candidate_count++
        }
    }
    
    ; ==============================================================================
    ; 候補管理機能
    ; ==============================================================================
    
    ; 変換を開始（ひらがなバッファの内容で辞書検索）
    sub start_conversion() -> bool {
        if hiragana_pos == 0 return false
        
        ; 変換対象キーをコピー
        conversion_key_length = hiragana_pos
        ubyte i
        for i in 0 to conversion_key_length-1 {
            conversion_key_buffer[i] = hiragana_buffer[i]
        }
        
        ; マジックナンバー確認
        current_bank = DICTIONARY_START_BANK
        current_offset = 0
        @(BANK_REG) = current_bank
        if @(ROM_BASE) != iso:'D' or @(ROM_BASE+1) != iso:'I' or @(ROM_BASE+2) != iso:'C' {
            return false  ; 辞書なし
        }
        
        ; 辞書検索実行（テスト：動詞のみ検索）

        ; 候補バッファをクリア
        candidate_buffer_pos = 0
        candidates_buffer[0] = 0


        ; TODO: 将来的には名詞と動詞両方を検索して候補を統合
        bool found = search_verb_entries(&conversion_key_buffer, conversion_key_length)

        ; このタイミングでfoundの場合、
        ; match_length = マッチした文字列の長さ
        ; match_bank = マッチした文字列のバンク
        ; match_offset = マッチした文字列のオフセット
        ; match_okurigana = マッチした送り仮名
        ; が、入っている(ので、一度記録する)
        bool verb_found = found
        if found {
            verb_match_length = match_length
            verb_match_bank = match_bank
            verb_match_offset = match_offset
            verb_match_okurigana = match_okurigana
        }

        ; 次に名詞を探す
        found = search_noun_entries(&conversion_key_buffer, conversion_key_length)

        ; 名詞と動詞が両方見つかった場合、長さを比較して、長い方を先に候補として足す
        is_verb_first = false
        if found and verb_found {
            is_verb_first = verb_match_length > match_length
            if is_verb_first {
                ; 動詞を先に登録
                current_bank = verb_match_bank
                current_offset = verb_match_offset
                add_candidates(verb_match_okurigana)
                verb_candidate_count = candidate_count
                ; 名詞を後に登録
                current_bank = match_bank
                current_offset = match_offset
                add_candidates(0)
                match_candidate_count = candidate_count
            } else {
                ; 名詞の場合はROM位置はそのままで良い
                add_candidates(0)
                match_candidate_count = candidate_count
                ; 動詞を後に登録
                current_bank = verb_match_bank
                current_offset = verb_match_offset
                add_candidates(verb_match_okurigana)
                verb_candidate_count = candidate_count
            }
        } else if found {
            ; 名詞のみ見つかった場合
            current_bank = match_bank
            current_offset = match_offset
            add_candidates(0)
            match_candidate_count = candidate_count
        } else if verb_found {
            is_verb_first = true
            ; 動詞のみ見つかった場合
            current_bank = verb_match_bank
            current_offset = verb_match_offset
            add_candidates(verb_match_okurigana)
            verb_candidate_count = candidate_count
        }
        
        if found or verb_found {
            ime_conversion_state = IME_STATE_CONVERTING
            current_candidate = 0
            return true
        }
        
        return false
    }
    
    ; 次候補選択
    sub next_candidate() {
        if ime_conversion_state == IME_STATE_CONVERTING and candidate_count > 0 {
            current_candidate++
            if current_candidate >= candidate_count {
                current_candidate = 0  ; 最後まで行ったら最初に戻る
            }
        }
    }
    
    ; 前候補選択
    sub prev_candidate() {
        if ime_conversion_state == IME_STATE_CONVERTING and candidate_count > 0 {
            if current_candidate == 0 {
                current_candidate = candidate_count - 1  ; 最初から最後に戻る
            } else {
                current_candidate--
            }
        }
    }
    
    ; 現在の候補文字列を取得
    sub get_current_candidate() -> uword {
        if ime_conversion_state == IME_STATE_CONVERTING and 
           candidate_count > 0 and current_candidate < candidate_count {
            return &candidates_buffer + candidate_offsets[current_candidate]
        }
        return 0
    }
    
    ; 変換確定
    sub confirm_conversion() {
        if ime_conversion_state == IME_STATE_CONVERTING {
            uword candidate_str = get_current_candidate()
            if candidate_str != 0 {
                ; 候補文字列を出力バッファにコピー
                ime_output_length = 0
                ubyte char
                ubyte i = 0
                repeat {
                    char = @(candidate_str + i)
                    if char == 0 break
                    if ime_output_length >= 127 break
                    ime_output_buffer[ime_output_length] = char
                    ime_output_length++
                    i++
                }
                ; 入力バッファについて候補文字列ぶんを詰める

                ; 動詞エントリなのか名詞エントリなのかを確認
                ubyte entry_length
                if is_verb_first {
                    if current_candidate < verb_candidate_count {
                        entry_length = verb_match_length + 1
                    } else {
                        entry_length = match_length
                    }
                } else {
                    if current_candidate < match_candidate_count {
                        entry_length = match_length
                    } else {
                        entry_length = verb_match_length + 1
                    }
                }
                uword head_str = &hiragana_buffer[0]
                uword tail_str = &hiragana_buffer[entry_length]
                repeat {
                    char = @(tail_str)
                    @(head_str) = char
                    if char == 0 break
                    head_str++
                    tail_str++
                }
                hiragana_pos -= entry_length
                clear_ime_input_area()
                ime_has_output = true
            }
        }
        
        ; 状態をリセット
        cancel_conversion()
    }
    
    ; 変換キャンセル
    sub cancel_conversion() {
        ime_conversion_state = IME_STATE_INPUT
        candidate_count = 0
        current_candidate = 0
        conversion_key_length = 0
        
        ; 変換キーバッファをクリア（重要：残存データを消去）
        clear_conversion_key_buffer()
        
        ; 表示状態をリセット
        prev_display_length = 0
        prev_display_chars = 0
        prev_romaji_pos = 0
        
        ; 入力エリアをクリア（変換候補表示を消す）
        clear_ime_input_area()
        
        update_ime_display()
    }
    
    ; ==============================================================================
    ; ブロッキング型IME新API
    ; ==============================================================================
    
    ; IME有効化
    sub activate() {
        ime_active = true
        activate_ime_input()
    }
    
    ; IME無効化
    sub deactivate() {
        ime_active = false
        deactivate_ime_input()
        ; 結果バッファをクリア
        clear_ime_output()
    }
    
    ; 結果文字列のアドレス取得
    sub get_result_text() -> uword {
        return get_ime_output()
    }
    
    ; 結果文字列の長さ取得
    sub get_result_length() -> ubyte {
        return get_ime_output_length()
    }
    
    ; パススルーキーコード取得
    sub get_passthrough_key() -> ubyte {
        return passthrough_key
    }
    
    ; 現在の入力モード取得
    sub get_current_mode() -> ubyte {
        return ime_input_mode
    }

    sub backup_cursor() {
        saved_cursor_x = jtxt.bitmap_x
        saved_cursor_y = jtxt.bitmap_y
        saved_fg_color = jtxt.bitmap_fg_color
        saved_bg_color = jtxt.bitmap_bg_color
    }

    sub restore_cursor() {
        jtxt.blocate(saved_cursor_x, saved_cursor_y)
        jtxt.bcolor(saved_fg_color, saved_bg_color)
    }
    
    ; ノンブロッキングIME処理（メイン関数）
    sub process() -> ubyte {
        ; まず、IME非アクティブ時でもCommodore+Spaceをチェック
        if check_commodore_space() {
            if ime_active {
                ; IMEアクティブ時は事前にカーソル位置を保存してからdeactivate
                backup_cursor()
                deactivate()
                restore_cursor()
                return IME_EVENT_DEACTIVATED
            } else {
                backup_cursor()
                activate()
                restore_cursor()
                return IME_EVENT_NONE  ; アクティブ化されたので継続
            }
        }
        
        ; IME非アクティブ時はイベントなしで返す
        if not ime_active return IME_EVENT_NONE
        
        ; キー取得（ノンブロッキング）
        ubyte key = cbm.GETIN2()
        if key == 0 return IME_EVENT_NONE  ; キーがない場合は即座に返す
        
        ; カーソル位置を保存（キーが押された時のみ）
        backup_cursor()

        ; F1/F3/F5 モード切り替えチェック
        ubyte mode_event = check_mode_keys()
        if mode_event != IME_EVENT_NONE {
            restore_cursor()
            return mode_event
        }
        
        
        ; 結果バッファをクリア
        clear_ime_output()
        
        ; ESCキーでキャンセル
        if key == KEY_ESC {
            cancel_ime_input()
            ; カーソル位置を復帰
            restore_cursor()
            return IME_EVENT_CANCELLED
        }
        
        ; BSキーまたはRETURNキーでバッファが空の場合はパススルー
        if (key == 20 or key == KEY_RETURN) {  ; 20 = BackSpace
            if romaji_pos == 0 and hiragana_pos == 0 {
                passthrough_key = key
                restore_cursor()
                return IME_EVENT_KEY_PASSTHROUGH
            }
        }
        
        ; IMEキー処理
        bool handled = process_ime_key(key)
        if handled {
            ; 確定文字列をチェック
            uword ime_output = get_ime_output()
            if ime_output != 0 {
                ubyte ime_length = get_ime_output_length()
                if ime_length > 0 {
                    ; 結果は既にime_output_bufferに格納済み
                    ; clear_ime_output()は呼び出し元で実行する
                    
                    ; カーソル位置を復帰
                    restore_cursor()
                    return IME_EVENT_CONFIRMED
                }
            }
        }
        
        ; カーソル位置を復帰（処理が完了したが確定されなかった場合）
        restore_cursor()
        return IME_EVENT_NONE
    }
    
    ; モードキーチェック（F1/F3/F5）
    sub check_mode_keys() -> ubyte {
        if is_f1_pressed() {
            set_hiragana_mode()
            return IME_EVENT_MODE_CHANGED
        }
        if is_f3_pressed() {
            set_katakana_mode()
            return IME_EVENT_MODE_CHANGED
        }
        if is_f5_pressed() {
            set_alphanumeric_mode()
            return IME_EVENT_MODE_CHANGED
        }
        return IME_EVENT_NONE
    }
}