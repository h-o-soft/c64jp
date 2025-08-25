# Font Conversion Tool

| [English](README-en.md) | [日本語](README.md) |
|---------------------------|------------------------|

Font data conversion tool for C64 Kanji ROM cartridge.

## Overview

Generates 8x8 pixel bitmap font data usable on the Commodore 64 from Misaki font PNG image files. The output format is 1-pixel-per-bit binary data.

## Generated Font Files

| Filename | Description | Size | Character Count |
|----------|-------------|------|----------------|
| `font_misaki_gothic.bin` | Misaki Gothic (JIS X 0208 full-width characters) | 70,688 bytes | 6,355 characters |
| `font_misaki_mincho.bin` | Misaki Mincho (JIS X 0208 full-width characters) | 70,688 bytes | 6,355 characters |
| `font_jisx0201.bin` | JIS X 0201 half-width characters | 2,048 bytes | 256 characters |

### Font Characteristics

This converter **unifies both JIS X 0201 (half-width characters) and JIS X 0208 (full-width characters) to the same 8x8 pixel size**.

- JIS X 0208: Uses Misaki font full-width characters directly as 8x8 pixels
- JIS X 0201: **Does not use Misaki font half-width images** (misaki_4x8.png, etc.) but **picks corresponding characters from full-width fonts**
  - Example: Half-width "A" is extracted from full-width "Ａ", half-width "ｱ" from full-width "ア"
  - This allows both half-width and full-width characters to be displayed with the same 8x8 pixels

## Usage

### Basic Usage

```bash
# Generate font files
make

# Generate all font files (same as default)
make all

# Convert fonts only (if PNG files already exist)
make convert-font
```

### Other Targets

```bash
# Download and extract Misaki fonts
make download-misaki

# Check Misaki font status
make check-misaki

# Delete generated files
make clean

# Delete all files including Misaki fonts
make clean-all

# Display help
make help
```

## Font Data Format

### Binary Format

Each character consists of 8 bytes (8x8 pixels):
- Byte 1: Pixel data for row 1 (MSB is leftmost)
- Byte 2: Pixel data for row 2
- ...
- Byte 8: Pixel data for row 8

Bit values:
- `1`: Foreground color (character part)
- `0`: Background color

### Character Code Mapping

#### JIS X 0208 (Full-width Characters)
- Stored in Kuten code order
- Formula to convert Kuten code to offset: `((ku - 1) * 94 + (ten - 1)) * 8`

#### JIS X 0201 (Half-width Characters)  
- Stored in ASCII code order (0x00〜0xFF)
- Formula to convert character code to offset: `character_code * 8`

### Conversion from Shift-JIS

When accessing font data from programs, convert Shift-JIS codes to Kuten codes first, then calculate the offset.

```python
def sjis_to_kuten(sjis_high, sjis_low):
    """Convert Shift-JIS code to Kuten code"""
    ku = sjis_high
    ten = sjis_low
    
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
    
    return ku, ten

def kuten_to_offset(ku, ten):
    """Calculate font data offset from Kuten code"""
    return ((ku - 1) * 94 + (ten - 1)) * 8
```

## Technical Specifications

### mkfont.py

Main processing:
1. **PNG Image Loading**: Load Misaki font PNG images
2. **Grayscale Conversion**: Convert color images to grayscale
3. **Bitmap Conversion**: Convert to bitmap data in 8x8 pixel units
4. **Binary Output**: Output as binary files with 8 bytes per character

Conversion rules:
- Pixel value > 128: White (0)
- Pixel value ≤ 128: Black (1)

### JIS X 0201 Font Generation

Instead of using Misaki font half-width images (misaki_4x8.png, etc.), extracts corresponding characters from full-width font data:

1. Identify full-width characters corresponding to JIS X 0201 character codes (0x00〜0xFF)
   - Example: 0x41 (half-width 'A') → full-width "Ａ" (JIS X 0208)
   - Example: 0xB1 (half-width 'ｱ') → full-width "ア" (JIS X 0208)
2. Extract data for corresponding characters from full-width font (8x8 pixels)
3. Combine data for 256 characters and output as binary file

This unifies JIS X 0201 half-width characters with JIS X 0208 full-width characters at the same 8x8 pixel size, simplifying display processing on the Commodore 64.

## File Structure

```
fontconv/
├── Makefile              # Build file
├── mkfont.py            # Font conversion script
├── README.md            # This file
├── misaki_gothic.png    # Misaki Gothic PNG (after download)
├── misaki_mincho.png    # Misaki Mincho PNG (after download)
├── font_misaki_gothic.bin  # Generated: Gothic font
├── font_misaki_mincho.bin  # Generated: Mincho font
└── font_jisx0201.bin       # Generated: Half-width font
```

## Dependencies

### Required
- Python 3.x
- Pillow (Python Imaging Library)
- GNU Make
- curl (for downloading)
- unzip (for extraction)

### Installation
```bash
# Install Python packages
pip install Pillow

# On macOS
brew install curl unzip

# On Linux
apt-get install curl unzip  # Debian/Ubuntu
yum install curl unzip      # CentOS/RHEL
```

## Troubleshooting

### PIL/Pillow Not Installed
```bash
pip install Pillow
# or
pip3 install Pillow
```

### Misaki Font Download Fails
Manual download:
1. Download https://littlelimit.net/arc/misaki/misaki_png_2021-05-05a.zip
2. Extract to fontconv directory
3. Run `make convert-font`

### Font Size Differs from Expected
- Check Misaki font version
- Verify PNG image size is correct (misaki_gothic.png: 752x752 pixels)

## License

### mkfont.py
MIT License (follows project-wide license)

### Misaki Font
- Author: Namu Kadoma
- License: [MIT License](https://littlelimit.net/misaki.htm)
- Misaki font is downloaded automatically

## Related Documentation

- [Project README](../README.md)
- [Misaki Font Official Site](https://littlelimit.net/misaki.htm)
- [JIS X 0208 Character Code Table](https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0208.html)
- [JIS X 0201 Character Code Table](https://www.asahi-net.or.jp/~ax2s-kmtn/ref/jisx0201.html)