from PIL import Image
import os

def convert_image_to_font_data(image_path, char_width=8, char_height=8):
    # Open image and convert to grayscale
    image = Image.open(image_path).convert('L')
    width, height = image.size
    
    # Output data
    font_data = []
    
    # Process image in 8x8 pixel chunks for font data
    for y in range(0, height, char_height):
        for x in range(0, width, char_width):
            for dy in range(char_height):
                row_data = 0
                for dx in range(char_width):
                    pixel = image.getpixel((x + dx, y + dy))
                    # White pixels = 0, black pixels = 1
                    bit = 0 if pixel > 128 else 1
                    row_data = (row_data << 1) | bit
                font_data.append(row_data)
    
    return font_data

# Convert Misaki Gothic font data
print("Converting Misaki Gothic...")
font_data = convert_image_to_font_data('misaki_gothic.png', 8, 8)

# Write font_data to binary file
with open('font_misaki_gothic.bin', 'wb') as f:
    for data in font_data:
        f.write(data.to_bytes(1, 'big'))

print(f"Created font_misaki_gothic.bin. Size: {len(font_data)} bytes")

# Convert Misaki Mincho font data
print("Converting Misaki Mincho...")
font_data = convert_image_to_font_data('misaki_mincho.png', 8, 8)

# Write font_data to binary file
with open('font_misaki_mincho.bin', 'wb') as f:
    for data in font_data:
        f.write(data.to_bytes(1, 'big'))

print(f"Created font_misaki_mincho.bin. Size: {len(font_data)} bytes")


# Offset calculation example
# Kuten code 1-1 (full-width space) is at offset 0
# Kuten code 1-2 (、) is at offset 8 (8 bytes)
# Formula to convert Kuten code to offset: ((ku-1) * 94 + (ten-1)) * 8

def kuten_to_offset(ku, ten):
    """Calculate font data offset from Kuten code"""
    return ((ku - 1) * 94 + (ten - 1)) * 8


# Shift-JIS to Kuten conversion example
def sjis_to_kuten(sjis_high, sjis_low):
    """Convert Shift-JIS code to Kuten code"""
    ku = sjis_high
    ten = sjis_low
    
    if ku <= 0x9f:
        if ten < 0x9f:
            ku = (ku << 1) - 0xe1
        else:
            ku = (ku << 1) - 0xe0
    else:
        if ten < 0x9f:
            ku = (ku << 1) - 0x161
        else:
            ku = (ku << 1) - 0x160
    
    if ten < 0x7f:
        ten -= 0x1f
    elif ten < 0x9f:
        ten -= 0x20
    else:
        ten -= 0x7e
    
    return ku - 0x20, ten - 0x20


# JIS X 0201 half-width font creation

# JIS X 0201 character array (256 characters) - represented as full-width characters
jisx0201 = '　' * 32  # Control character area (0x00-0x1F)
jisx0201 += '　！”＃＄％＆’（）＊＋，‐．／０１２３４５６７８９：；＜＝＞？＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ［＼］＾＿｀ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ｛｜｝〜　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　。「」、・ヲァィゥェォャュョッーアイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワン゛゜'
jisx0201 += '　' * 31  # Undefined area (0x80-0x9F)
jisx0201 += '〜'



# Error if not 256 characters
if len(jisx0201) != 256:
    print(f'Error: JIS X 0201 character count is not 256 ({len(jisx0201)} characters)')
    exit()

def create_jisx0201_font():
    """Create JIS X 0201 half-width font from full-width font"""
    print("Creating JIS X 0201 half-width font...")
    
    # Remove existing font_jisx0201.bin file if it exists
    if os.path.exists('font_jisx0201.bin'):
        os.remove('font_jisx0201.bin')
    
    # Process all 256 characters in order
    for char_index in range(256):
        char = jisx0201[char_index]
        
        try:
            # Encode character to Shift-JIS
            char_bytes = char.encode('shift_jis')
            
            if len(char_bytes) == 1:
                # 1-byte character (ASCII, half-width kana) - fill with blank
                with open('font_jisx0201.bin', 'ab') as f:
                    f.write(b'\x00' * 8)
            
            elif len(char_bytes) == 2:
                # 2-byte character (full-width character)
                ku = char_bytes[0]
                ten = char_bytes[1]
                
                # Convert Shift-JIS to Kuten
                if ku <= 0x9f:
                    if ten < 0x9f:
                        ku = (ku << 1) - 0x102
                    else:
                        ku = (ku << 1) - 0x101
                else:
                    if ten < 0x9f:
                        ku = (ku << 1) - 0x182
                    else:
                        ku = (ku << 1) - 0x181
                
                if ten < 0x7f:
                    ten -= 0x40
                elif ten < 0x9f:
                    ten -= 0x41
                else:
                    ten -= 0x9f
                
                # Convert Kuten to font offset
                code = ku * 94 + ten
                offset = code * 8  # 8 bytes for 8x8 font
                
                # Get corresponding character data from full-width font data
                try:
                    with open('font_misaki_gothic.bin', 'rb') as f:
                        f.seek(offset)
                        font_data = f.read(8)
                        if len(font_data) == 8:
                            with open('font_jisx0201.bin', 'ab') as f:
                                f.write(font_data)
                        else:
                            # Fill with blank if data is insufficient
                            with open('font_jisx0201.bin', 'ab') as f:
                                f.write(b'\x00' * 8)
                except Exception as e:
                    with open('font_jisx0201.bin', 'ab') as f:
                        f.write(b'\x00' * 8)
            
            else:
                # Unexpected encoding result
                with open('font_jisx0201.bin', 'ab') as f:
                    f.write(b'\x00' * 8)
                
        except Exception as e:
            # Fill with blank on encoding error
            with open('font_jisx0201.bin', 'ab') as f:
                f.write(b'\x00' * 8)
    
    # Check file size
    if os.path.exists('font_jisx0201.bin'):
        size = os.path.getsize('font_jisx0201.bin')
        print(f"Created font_jisx0201.bin. Size: {size} bytes (expected: 2048 bytes)")
        if size != 2048:
            print(f"Warning: Size differs from expected value")
    else:
        print("Error: Failed to create font_jisx0201.bin")

# Execute JIS X 0201 font creation
create_jisx0201_font()