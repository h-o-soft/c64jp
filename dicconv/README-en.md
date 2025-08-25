# Dictionary Conversion Tool

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Converts SKK format Japanese dictionaries to binary format for C64 Kanji ROM cartridge.

## Overview

Converts SKK format text dictionary files to binary format that can be accessed quickly on the C64. The converted dictionary is used by the IME (Japanese input) functionality to enable hiragana to kanji conversion.

## Usage

### Basic Usage

```bash
# Convert SKK dictionary to binary format
python3 dicconv.py skkdic.txt skkdic.bin

# Specify input and output files
python3 dicconv.py input_dictionary.txt output_dictionary.bin
```

### Using with Project Makefile

```bash
# Run from project root
make dict  # Convert dictionary and create CRT file
```

## Dictionary File Format

### Input Format (SKK Dictionary)

- **Character Encoding**: EUC-JP
- **Format**: SKK dictionary format
- **Structure**: `reading /candidate1/candidate2/.../candidateN/`

#### Okurinashi Entries (No Trailing Kana)
Entries where the reading is all hiragana with no trailing inflection:
```
あい /愛/哀/相/藍/
かんじ /漢字/感じ/幹事/
へんかん /変換/返還/
```

#### Okuriari Entries (With Trailing Kana)
Entries where the reading ends with romanized inflection letters:
```
かk /書/描/
おくr /送/贈/
たべr /食べ/
はしr /走/
```

Inflection notation:
- `k` - ka-gyou (か, き, く, け, こ)
- `r` - ra-gyou (ら, り, る, れ, ろ)
- `t` - ta-gyou (た, ち, つ, て, と)
- Other consonants used according to inflection patterns

### Output Format (Binary Dictionary)

#### Header Structure
```
+0x00: 'DIC' + 0x00 (4 bytes) - Magic number
+0x04: Okurinashi entry offset table (82 entries × 3 bytes = 246 bytes)
+0xFA: Okuriari entry offset table (82 entries × 3 bytes = 246 bytes)
```

#### Offset Table

Records the starting position of entries for each initial hiragana character (あ, い, う...):
- L (low byte): Bank offset low
- M (mid byte): Bank offset high  
- H (high byte): Bank number (per 8KB)

#### Entry Structure
```
+0: Entry size (2 bytes, little-endian)
    - MSB set for first entry (0x8000 OR)
+2: Reading string (Shift-JIS, null-terminated)
+n: Candidate count (1 byte)
+n+1: Candidate1 (Shift-JIS, null-terminated)
+n+m: Candidate2 (Shift-JIS, null-terminated)
...
```

## Index Keys

The dictionary is indexed by 82 types of hiragana:

```
あ ぃ い ぅ う ぇ え ぉ お
か が き ぎ く ぐ け げ こ ご
さ ざ し じ す ず せ ぜ そ ぞ
た だ ち ぢ っ つ づ て で と ど
な に ぬ ね の
は ば ぱ ひ び ぴ ふ ぶ ぷ へ べ ぺ ほ ぼ ぽ
ま み む め も
ゃ や ゅ ゆ ょ よ
ら り る れ ろ
ゎ わ ゐ ゑ を ん
```

## Classification Rules

- **Okurinashi entries**: Reading ends with full-width character (hiragana/katakana/kanji)
- **Okuriari entries**: Reading ends with half-width character (romanization indicating inflection)

Examples:
- `あい` → Okurinashi entry
- `かk` → Okuriari entry (inflection of「書く」, trailing kana「く」)
- `おくr` → Okuriari entry (inflection of「送る」, trailing kana「る」)

## Sort Order

Entries within each group are sorted by the following priority:
1. String length (longest first) - prioritize more specific conversions
2. Dictionary order (aiueo order)

## Technical Specifications

### dicconv.py

Main classes and functions:

#### CharOffsetEntry
```python
@dataclass
class CharOffsetEntry:
    key: str          # Index character (あ, い, う...)
    offset: int       # Data start offset
    header_size: int  # Header size
    entry_size: int   # Total entry size
    entries: list     # List of DicEntry
```

#### DicEntry
```python
@dataclass  
class DicEntry:
    all_size: int     # Total entry size
    key: str          # Reading (Shift-JIS)
    kouho_count: int  # Candidate count
    kouho: list       # Candidate list
```

### Memory Layout

Arranged in 8KB bank units to match C64 MagicDesk cartridge format:
- Bank 0: Program code
- Banks 1-9: Font data
- Banks 10+: Dictionary data

## File Structure

```
dicconv/
├── dicconv.py       # Dictionary conversion script
├── README.md        # This file
├── skkdic.txt       # Input: SKK format dictionary (EUC-JP)
└── skkdic.bin       # Output: Binary dictionary
```

## Dependencies

- Python 3.x
- dataclasses (standard library from Python 3.7+)

## Troubleshooting

### Character Encoding Error

If input file is not EUC-JP:
```bash
# Convert UTF-8 to EUC-JP
iconv -f UTF-8 -t EUC-JP input.txt > output_eucjp.txt
```

### Memory Shortage

For large dictionary files, converted size may become large:
- Remove unnecessary entries
- Limit candidate count
- Split into multiple dictionaries

### Dictionary Not Found

If dictionary is not found by IME:
- Verify dictionary is included in CRT file
- Check dictionary bank number is correct

## Dictionary Data License

When using SKK dictionaries, follow each dictionary's license:

- **SKK-JISYO.L**: GPLv2+
- **SKK-JISYO.M**: GPLv2+
- **SKK-JISYO.S**: GPLv2+

When creating custom dictionaries, follow SKK format guidelines.

## Related Documentation

- [Project README](../README.md)
- [SKK OpenLab](https://skk-dev.github.io/dict/)
- [SKK Dictionary Format](https://skk-dev.github.io/dict/format.html)