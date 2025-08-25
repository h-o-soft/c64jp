from dataclasses import dataclass, field
from typing import List
import sys

# Convert SKK format dictionary to binary dictionary for Commander X16 tinyskk

# Dictionary format
#
# +0 : 'DIC' + 0
# +4 : Noun entry "あ" offset address L M H (3bytes)
# +7 : Noun entry "い" offset address L M H (3bytes)
# ....
# 82 * 3 bytes = 207bytes of noun entries
# 82 * 3 bytes = 207bytes of verb entries

offset_keys = [
    "あ",   #; 0
    "ぃ",   #; 1
    "い",   #; 2
    "ぅ",   #; 3
    "う",   #; 4
    "ぇ",   #; 5
    "え",   #; 6
    "ぉ",   #; 7
    "お",   #; 8
    "か",   #; 9
    "が",   #; 10
    "き",   #; 11
    "ぎ",   #; 12
    "く",   #; 13
    "ぐ",   #; 14
    "け",   #; 15
    "げ",   #; 16
    "こ",   #; 17
    "ご",   #; 18
    "さ",   #; 19
    "ざ",   #; 20
    "し",   #; 21
    "じ",   #; 22
    "す",   #; 23
    "ず",   #; 24
    "せ",   #; 25
    "ぜ",   #; 26
    "そ",   #; 27
    "ぞ",   #; 28
    "た",   #; 29
    "だ",   #; 30
    "ち",   #; 31
    "ぢ",   #; 32
    "っ",   #; 33
    "つ",   #; 34
    "づ",   #; 35
    "て",   #; 36
    "で",   #; 37
    "と",   #; 38
    "ど",   #; 39
    "な",   #; 40
    "に",   #; 41
    "ぬ",   #; 42
    "ね",   #; 43
    "の",   #; 44
    "は",   #; 45
    "ば",   #; 46
    "ぱ",   #; 47
    "ひ",   #; 48
    "び",   #; 49
    "ぴ",   #; 50
    "ふ",   #; 51
    "ぶ",   #; 52
    "ぷ",   #; 53
    "へ",   #; 54
    "べ",   #; 55
    "ぺ",   #; 56
    "ほ",   #; 57
    "ぼ",   #; 58
    "ぽ",   #; 59
    "ま",   #; 60
    "み",   #; 61
    "む",   #; 62
    "め",   #; 63
    "も",   #; 64
    "ゃ",   #; 65
    "や",   #; 66
    "ゅ",   #; 67
    "ゆ",   #; 68
    "ょ",   #; 69
    "よ",   #; 70
    "ら",   #; 71
    "り",   #; 72
    "る",   #; 73
    "れ",   #; 74
    "ろ",   #; 75
    "ゎ",   #; 76
    "わ",   #; 77
    "ゐ",   #; 78
    "ゑ",   #; 79
    "を",   #; 80
    "ん"    #; 81
]

def get_empty_list():
    return []

@dataclass
class CharOffsetEntry:
    key: str
    offset: int = 0
    header_size: int = 0
    entry_size: int = 0
    entries: list = field(default_factory=get_empty_list)

@dataclass
class DicEntry:
    all_size: int = 0
    key: str = ""
    kouho_count: int = 0
    kouho: list = field(default_factory=get_empty_list)

    def update_all_size(self):
        # Get size of key encoded in Shift-JIS
        sjis = self.key.encode("shift_jis")
        self.all_size = 2 + len(sjis) + 1 +  1   # sizeof(all_size) + sizeof(key) + 0 +  candidate_count(1byte)
        # Get size of each candidate item encoded in Shift-JIS and add to self.all_size
        for k in self.kouho:
            sjis = k.encode("shift_jis")
            self.all_size += len(sjis) + 1
        # all_size should now be calculated

meishi_offset_entries = [CharOffsetEntry(key=k) for k in offset_keys]
doushi_offset_entries = [CharOffsetEntry(key=k) for k in offset_keys]

# Calculate header size
all_header_size = 4
for e in meishi_offset_entries:
    all_header_size += 3
for e in doushi_offset_entries:
    all_header_size += 3


# 2 bytes : Total byte count of this entry. However, if this is the first entry of each entry group, the most significant bit is set (meaning you need to exit when the MSB is set during search)
# variable : Key string of this entry (null-terminated)
# 1 byte : Number of candidates (n) - Will be extended to 2 bytes if over 256 candidates are found during converter creation
# variable : Candidate string 1 (null-terminated)
# ....
# variable : Candidate string n (null-terminated)
# Next entry

# Command line argument processing
if len(sys.argv) != 3:
    print("Usage: python dicconv.py <input SKK dictionary file> <output binary file>")
    print("Example: python dicconv.py skkdic.txt skkdic.bin")
    sys.exit(1)

dic_path = sys.argv[1]
output_filename = sys.argv[2]

print(f"Input file: {dic_path}")
print(f"Output file: {output_filename}")

all_entries = []

# 1. Read dictionary file
#   Format is EUC-JP
#   Read line by line
#   Skip lines starting with ; (comments)
#   Skip lines starting with half-width characters
with open(dic_path, "r", encoding="euc-jp") as f:
    lines = f.readlines()
    # Remove newlines
    lines = [line.rstrip() for line in lines]
    for line in lines:
        if line[0] == ";":
            continue
        if ord(line[0]) < 0x80:
            continue
        # Start processing. Format is:
        # (full-width characters)(any number of spaces or tabs)/(candidate1)/candidate2)/.../(candidaten)/
        # First, get full-width characters separated by space or tab
        entry = DicEntry()
        dicinfo = line.split()
        dickey = dicinfo[0]
        # print(dickey)
        entry.key = dickey
        # Next, get candidates separated by /. However, exclude / at beginning and end of line if present
        kouho = [part for part in dicinfo[1].split("/") if part]
        entry.kouho_count = len(kouho)
        for k in kouho:
            entry.kouho.append(k)
            # print(k)
        all_entries.append(entry)
        
        # Encode dickey to Shift-JIS
        sjis = dickey.encode("shift_jis")

# Now we have what we need
for entry in all_entries:
    # print(entry.key)
    # print(entry.kouho_count)
    # for k in entry.kouho:
    #     print(k)
    # print()
    entry.update_all_size()
    # Check if the last character of entry.key is a half-width character (alphabet)
    target_offset_entries = None
    if ord(entry.key[-1]) < 0x80:
        # Half-width, so treat as verb (add to verb offset entries)
        target_offset_entries = doushi_offset_entries
    else:
        # Full-width, so treat as noun (add to noun offset entries)
        target_offset_entries = meishi_offset_entries
    # Find the first character of the key
    key_head = entry.key[0]
    # Find matching first character in target offset entries
    added = False
    for e in target_offset_entries:
        if e.key == key_head:
            # Found match, add entry
            e.entries.append(entry)
            added = True
            break
    if not added:
        # No match found, output error and exit
        print("Error: No matching entry found in target offset entries")
        exit(1)

# Sort entries within each group (longest first + alphabetical order)
def sort_entries(entries):
    """
    Sort entries with the following priority:
    1. String length (longest first)
    2. Alphabetical order (dictionary order)
    """
    return sorted(entries, key=lambda entry: (-len(entry.key), entry.key))

# Sort verb entries
for e in doushi_offset_entries:
    e.entries = sort_entries(e.entries)
    e.entry_size = 0    # Initialize just in case
    first = True
    for entry in e.entries:
        if first:
            # First entry, so set the most significant bit
            entry.all_size |= 0x8000
            first = False
        # Update entry_size
        e.entry_size += entry.all_size & 0x7fff

# Sort noun entries
for e in meishi_offset_entries:
    e.entries = sort_entries(e.entries)
    e.entry_size = 0    # Initialize just in case
    first = True
    for entry in e.entries:
        if first:
            # First entry, so set the most significant bit
            entry.all_size |= 0x8000
            first = False
        # Update entry_size
        e.entry_size += entry.all_size & 0x7fff

# Size calculation complete


# Noun entries start after header size
# Followed by verb entries

data_offset = all_header_size
# Set noun entry offsets
for e in meishi_offset_entries:
    if len(e.entries) == 0:
        e.offset = 0
    else:
        e.offset = data_offset
    data_offset += e.entry_size
# Set verb entry offsets
for e in doushi_offset_entries:
    if len(e.entries) == 0:
        e.offset = 0
    else:
        e.offset = data_offset
    data_offset += e.entry_size

print(f"Dictionary total size: {data_offset} bytes")

output_path = output_filename
with open(output_path, "wb") as f:
    # Write header
    f.write(b"DIC\x00")
    # Write noun entry offsets byte by byte
    # However, instead of byte count from beginning, put bank number in H when data block is divided into 8KB, and put offset within that bank in L and M
    for e in meishi_offset_entries:
        offset = e.offset % 8192
        f.write(offset.to_bytes(2, "little"))
        bank = e.offset // 8192
        f.write(bank.to_bytes(1, "little"))
    for e in doushi_offset_entries:
        offset = e.offset % 8192
        f.write(offset.to_bytes(2, "little"))
        bank = e.offset // 8192
        f.write(bank.to_bytes(1, "little"))
    # Write noun entries
    for e in meishi_offset_entries:
        for entry in e.entries:
            # 1. Total byte count of this entry minus entry key string size and its own size
            skip_size = entry.all_size - 2 - len(entry.key.encode("shift_jis")) - 1
            f.write(skip_size.to_bytes(2, "little"))
            # 2. Entry key string
            sjis = entry.key.encode("shift_jis")
            f.write(sjis)
            f.write(b"\x00")
            # 3. Number of candidates
            f.write(entry.kouho_count.to_bytes(1, "little"))
            # 4. Candidate strings
            for k in entry.kouho:
                sjis = k.encode("shift_jis")
                f.write(sjis)
                f.write(b"\x00")
    # Write verb entries
    for e in doushi_offset_entries:
        for entry in e.entries:
            # 1. Total byte count of this entry minus entry key string size and its own size
            skip_size = entry.all_size - 2 - len(entry.key.encode("shift_jis")) - 1
            f.write(skip_size.to_bytes(2, "little"))
            # 2. Entry key string
            sjis = entry.key.encode("shift_jis")
            f.write(sjis)
            f.write(b"\x00")
            # 3. Number of candidates
            f.write(entry.kouho_count.to_bytes(1, "little"))
            # 4. Candidate strings
            for k in entry.kouho:
                sjis = k.encode("shift_jis")
                f.write(sjis)
                f.write(b"\x00")