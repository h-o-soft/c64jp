#!/usr/bin/env python3
"""
C64 Kanji ROM Cartridge Creation Script
Creates MagicDesk format CRT files
"""

import os
import sys
import argparse
import subprocess
import struct

def assemble_source(source_file):
    """Assemble source with 64tass and create binary file"""
    base_name = os.path.splitext(source_file)[0]
    bin_file = f"{base_name}.bin"
    fixed_bin_file = f"{base_name}-fixed.bin"
    
    print(f"Assembling with 64tass: {source_file} -> {bin_file}")
    result = subprocess.run(['64tass', '-b', '-o', bin_file, source_file], 
                          capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Assembly error:")
        print(result.stderr)
        return None
    
    # Adjust if 64tass output is not 8192 bytes
    file_size = os.path.getsize(bin_file)
    if file_size != 8192:
        print(f"Warning: Binary size is not 8192 bytes: {file_size} bytes")
        # Pad or truncate as necessary
        if file_size > 8192:
            print(f"Truncating binary size to 8192 bytes: {bin_file} -> {fixed_bin_file}")
            result = subprocess.run(['dd', f'if={bin_file}', f'of={fixed_bin_file}', 
                                   'bs=8192', 'count=1'], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                return fixed_bin_file
        else:
            # Padding
            print(f"Padding binary size to 8192 bytes: {bin_file} -> {fixed_bin_file}")
            with open(bin_file, 'rb') as f:
                data = f.read()
            data += bytes(8192 - len(data))
            with open(fixed_bin_file, 'wb') as f:
                f.write(data)
            return fixed_bin_file
    
    return bin_file

def align_to_boundary(size, boundary):
    """Align size to specified boundary"""
    if size % boundary == 0:
        return size
    return ((size // boundary) + 1) * boundary

def create_crt_header(name, hardware_type, exrom, game):
    """Create CRT file header"""
    header = bytearray(64)  # Fixed 64 bytes
    
    # CRT signature (16 bytes)
    signature = b'C64 CARTRIDGE   '
    header[0:16] = signature
    
    # Header length (big endian, always 0x40)
    header[16:20] = struct.pack('>I', 0x40)
    
    # Version (big endian, 1.0)
    header[20:22] = struct.pack('>H', 0x0100)
    
    # Hardware type (big endian)
    header[22:24] = struct.pack('>H', hardware_type)
    
    # EXROM line status
    header[24] = exrom
    
    # GAME line status  
    header[25] = game
    
    # Reserved (6 bytes) - already zero
    
    # Cartridge name (32 bytes)
    name_bytes = name.encode('ascii')[:31]
    header[32:32+len(name_bytes)] = name_bytes
    # Rest remains zero
    
    return header

def create_chip_header(bank_number, load_address, data_size):
    """Create CHIP packet header"""
    header = bytearray(16)
    
    # CHIP signature
    header[0:4] = b'CHIP'
    
    # Total packet length (header + data)
    packet_length = 16 + data_size
    header[4:8] = struct.pack('>I', packet_length)
    
    # Chip type (ROM = 0)
    header[8:10] = struct.pack('>H', 0)
    
    # Bank number
    header[10:12] = struct.pack('>H', bank_number)
    
    # Starting load address
    header[12:14] = struct.pack('>H', load_address)
    
    # ROM image size
    header[14:16] = struct.pack('>H', data_size)
    
    return header

def create_magicdesk_crt(base_file, font_file, output_crt, jisx0201_file=None, string_resource_file=None, dictionary_file=None):
    """Create MagicDesk CRT (manual CRT generation, 8KB bank units)"""
    
    if not os.path.exists(base_file):
        print(f"Error: {base_file} not found")
        return False
    
    if not os.path.exists(font_file):
        print(f"Error: {font_file} not found")
        return False
    
    # Load base code
    with open(base_file, 'rb') as f:
        base_data = f.read()
    
    # Load font data (JIS X 0208 full-width font)
    with open(font_file, 'rb') as f:
        font_data = f.read()
    
    # Load JIS X 0201 half-width font data (optional)
    jisx0201_data = b''
    if jisx0201_file and os.path.exists(jisx0201_file):
        with open(jisx0201_file, 'rb') as f:
            jisx0201_data = f.read()
    
    # Combine font data (place JIS X 0201 at the beginning)
    if jisx0201_data:
        combined_font_data = jisx0201_data + font_data
    else:
        combined_font_data = font_data
    
    font_data = combined_font_data
    
    # Build all bank data
    all_banks = []
    
    # Add base code (Bank 0)
    if len(base_data) > 8192:
        print(f"Warning: Base code exceeds 8192 bytes: {len(base_data)} bytes")
        base_data = base_data[:8192]  # Truncate
    elif len(base_data) < 8192:
        base_data += bytes(8192 - len(base_data))  # Padding
    
    all_banks.append(base_data)
    
    # Split font data into banks (8KB units)
    chunk_size = 8192
    bank_num = 1
    font_start_bank = bank_num
    
    for i in range(0, len(font_data), chunk_size):
        chunk = font_data[i:i+chunk_size]
        if len(chunk) < chunk_size:
            chunk += bytes(chunk_size - len(chunk))
        all_banks.append(chunk)
        bank_num += 1
    
    font_end_bank = bank_num - 1
    print(f"Font data: Banks {font_start_bank}-{font_end_bank} ({len(font_data)} bytes)")
    
    # Process dictionary data (place before string resources)
    if dictionary_file and os.path.exists(dictionary_file):
        # Check current bank position (dictionary is placed with 8KB alignment)
        dictionary_start_bank = len(all_banks)
        
        # Load dictionary data
        with open(dictionary_file, 'rb') as f:
            dictionary_data = f.read()
        
        # Place dictionary data in banks in 8KB units
        dict_banks = (len(dictionary_data) + 8191) // 8192
        
        for i in range(dict_banks):
            start = i * 8192
            end = min((i + 1) * 8192, len(dictionary_data))
            chunk = dictionary_data[start:end]
            
            # Pad to 8KB
            if len(chunk) < 8192:
                chunk += bytes(8192 - len(chunk))
            
            all_banks.append(chunk)
        
        dictionary_end_bank = dictionary_start_bank + dict_banks - 1
        print(f"Dictionary data: Banks {dictionary_start_bank}-{dictionary_end_bank} ({len(dictionary_data)} bytes)")
    
    # Add string resources (optional, place after dictionary)
    if string_resource_file and os.path.exists(string_resource_file):
        # Calculate string resource offset
        string_resource_offset = len(all_banks) * 8192
        string_resource_start_bank = len(all_banks)
        
        # Process string resource file
        if string_resource_file.endswith('.bin'):
            # If already a binary file, load directly
            with open(string_resource_file, 'rb') as f:
                string_resource_data = f.read()
        else:
            # If text file, perform conversion
            import tempfile
            with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as tmp_file:
                temp_bin_file = tmp_file.name
            
            # Call convert_string_resources.py
            # Calculate string resource bank number (8KB units)
            actual_string_bank = len(all_banks)
            result = subprocess.run(['python3', '../stringresources/convert_string_resources.py', 
                                   string_resource_file, temp_bin_file, 
                                   str(actual_string_bank * 8192), '--no-align'],
                                  capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"String resource conversion error: {result.stderr}")
                os.unlink(temp_bin_file)
                return False
            
            # Load converted binary
            with open(temp_bin_file, 'rb') as f:
                string_resource_data = f.read()
            os.unlink(temp_bin_file)
        
        # Place string resources in banks (8KB units)
        remaining_data = string_resource_data
        
        while remaining_data:
            # Data to place in this bank (max 8KB)
            chunk = remaining_data[:8192]
            remaining_data = remaining_data[8192:]
            
            # Pad if less than 8KB
            if len(chunk) < 8192:
                chunk += bytes(8192 - len(chunk))
            
            all_banks.append(chunk)
        
        string_resource_end_bank = len(all_banks) - 1
        if string_resource_start_bank == string_resource_end_bank:
            print(f"String resources: Bank {string_resource_start_bank} ({len(string_resource_data)} bytes)")
        else:
            print(f"String resources: Banks {string_resource_start_bank}-{string_resource_end_bank} ({len(string_resource_data)} bytes)")
    
    print(f"\nTotal banks: {len(all_banks)}")
    print(f"Total size: {len(all_banks) * 8192} bytes")
    print(f"Creating MagicDesk CRT: {output_crt}")
    
    with open(output_crt, 'wb') as f:
        # Write CRT header (MagicDesk = Type 19)
        # EXROM=0, GAME=1 (16K configuration)
        header = create_crt_header("KANJI ROM MD", 19, 0, 1)
        f.write(header)
        
        # Write each bank as CHIP packet
        for bank_num, bank_data in enumerate(all_banks):
            # Write CHIP header
            chip_header = create_chip_header(bank_num, 0x8000, 8192)
            f.write(chip_header)
            
            # Write bank data
            f.write(bank_data)
            
    
    print(f"MagicDesk CRT creation completed: {output_crt}")
    return True

def main():
    parser = argparse.ArgumentParser(description='C64 Kanji ROM Cartridge Creation')
    parser.add_argument('--output', '-o', default='kanji_magicdesk_basic.crt',
                      help='Output CRT filename (default: kanji_magicdesk_basic.crt)')
    parser.add_argument('--font-file', default='../fontconv/font_misaki_gothic.bin',
                      help='Full-width font file (default: font_misaki_gothic.bin)')
    parser.add_argument('--jisx0201-file', default='../fontconv/font_jisx0201.bin',
                      help='JIS X 0201 half-width font file (default: font_jisx0201.bin)')
    parser.add_argument('--string-resource-file', default=None,
                      help='String resource file (CSV format, optional)')
    parser.add_argument('--dictionary-file', default=None,
                      help='Dictionary file (skkdicm.bin, optional)')
    
    args = parser.parse_args()
    
    # Select source file
    source_file = 'kanji-magicdesk-basic.asm'
    
    print(f"C64 Kanji ROM Cartridge Creation - MagicDesk Format")
    print(f"Source: {source_file}")
    print(f"Fullwidth font: {args.font_file}")
    print(f"Halfwidth font: {args.jisx0201_file}")
    if args.string_resource_file:
        print(f"String resources: {args.string_resource_file}")
    if args.dictionary_file:
        print(f"Dictionary file: {args.dictionary_file}")
    print(f"Output: {args.output}")
    print()
    
    # Assemble
    bin_file = assemble_source(source_file)
    if bin_file is None:
        return 1
    
    # Create CRT
    success = create_magicdesk_crt(bin_file, args.font_file, args.output, 
                                   args.jisx0201_file, args.string_resource_file, args.dictionary_file)
    
    if success:
        print(f"\nCreation completed: {args.output}")
        print(f"Launch with VICE: x64sc -cartcrt {args.output}")
        return 0
    else:
        print(f"\nCreation failed")
        return 1

if __name__ == "__main__":
    sys.exit(main())