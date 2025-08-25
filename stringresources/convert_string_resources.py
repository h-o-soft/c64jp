#!/usr/bin/env python3
"""
String Resource Converter Tool
Generates binary string resources from text files
"""

import sys
import struct
import csv

def parse_string_resource_file(filename):
    """Parse string resource file"""
    resources = {}
    
    with open(filename, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) >= 2:
                try:
                    index = int(row[0])
                    # Join if there are commas
                    text = ','.join(row[1:])
                    # Remove leading and trailing quotes
                    if text.startswith('"') and text.endswith('"'):
                        text = text[1:-1]
                    resources[index] = text
                except ValueError:
                    print(f"Warning: Skipping invalid row: {row}")
    
    return resources

def text_to_shiftjis(text):
    """Convert text to Shift-JIS byte sequence"""
    try:
        # Process newlines
        text = text.replace('\\n', '\n')
        # Convert to Shift-JIS
        return text.encode('shift-jis')
    except UnicodeEncodeError as e:
        print(f"Error: Character cannot be converted to Shift-JIS: {e}")
        return b''

def calculate_string_layout(resources, start_offset=0):
    """Calculate string layout
    
    Args:
        resources: Dictionary of {index: text}
        start_offset: Offset after kanji font data (in bytes)
    
    Returns:
        Tuple of (string_data, offset_table)
        string_data: Byte sequence of all string data
        offset_table: List of [(bank, lohi, offset), ...]
    """
    
    # Sort by index
    sorted_indices = sorted(resources.keys())
    
    string_data = bytearray()
    offset_table = []
    
    # Calculate header size (4 bytes for string count + offset table)
    max_index = max(resources.keys()) if resources else -1
    num_strings = max_index + 1
    header_size = 4 + num_strings * 4
    
    # Starting offset for string data (adding header size)
    current_offset = start_offset + header_size
    
    for idx in sorted_indices:
        text = resources[idx]
        sjis_bytes = text_to_shiftjis(text)
        
        # Add NULL terminator
        sjis_bytes += b'\x00'
        
        # Calculate bank information from current offset
        bank = current_offset // 8192  # 8KB bank size (for MagicDesk)
        bank_offset = current_offset % 8192
        
        if bank_offset >= 8192:
            # ROMH ($A000-$BFFF)
            lohi = 1
            offset_in_window = bank_offset - 8192
        else:
            # ROML ($8000-$9FFF)
            lohi = 0
            offset_in_window = bank_offset
        
        # Add to offset table
        offset_table.append((bank, lohi, offset_in_window))
        
        # Add to string data
        string_data.extend(sjis_bytes)
        
        # Update offset
        current_offset += len(sjis_bytes)
    
    return bytes(string_data), offset_table

def create_resource_binary(resources, start_offset=0):
    """Create binary string resources
    
    Format:
    - 4 bytes: Number of strings (little-endian)
    - 4 bytes per string:
      - 1 byte: Bank number
      - 1 byte: L(0) or H(1)
      - 2 bytes: Offset within 8KB range (little-endian)
    - Followed by actual string data
    """
    
    string_data, offset_table = calculate_string_layout(resources, start_offset)
    
    # Calculate number of strings as max index + 1
    max_index = max(resources.keys()) if resources else -1
    num_strings = max_index + 1
    
    # Create binary data
    output = bytearray()
    
    # Number of strings (4 bytes, little-endian)
    output.extend(struct.pack('<I', num_strings))
    
    # Offset table
    # Store in index order (fill non-existent indices with 0)
    for i in range(num_strings):
        if i in resources:
            # Find corresponding offset information
            sorted_indices = sorted(resources.keys())
            table_index = sorted_indices.index(i)
            bank, lohi, offset = offset_table[table_index]
            
            # 4-byte offset information
            output.append(bank)  # Bank number
            output.append(lohi)  # L(0) or H(1)
            output.extend(struct.pack('<H', offset))  # Offset (2 bytes)
        else:
            # Fill non-existent indices with 0
            output.extend(b'\x00\x00\x00\x00')
    
    # Add string data
    output.extend(string_data)
    
    return bytes(output)

def align_to_boundary(offset, boundary):
    """Align offset to specified boundary
    
    Args:
        offset: Original offset
        boundary: Boundary size (8192 or 16384)
    
    Returns:
        Aligned offset
    """
    if offset % boundary == 0:
        return offset
    return ((offset // boundary) + 1) * boundary

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_string_resources.py <input_text_file> [output_binary_file] [start_offset] [options]")
        print("\nOptions:")
        print("  --align16k  Align to 16KB boundary (default)")
        print("  --align8k   Align to 8KB boundary")
        print("  --no-align  No alignment")
        print("\nExamples:")
        print("  python convert_string_resources.py strings.txt")
        print("  python convert_string_resources.py strings.txt strings.bin")
        print("  python convert_string_resources.py strings.txt strings.bin 75776 --align8k")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'string_resources.bin'
    
    # Start offset (default is 75776 bytes after kanji font)
    # JIS X 0201: 2048 bytes
    # JIS X 0208: 70656 bytes (8836 characters × 8 bytes)
    # Padding: 3072 bytes
    # Total: 75776 bytes
    default_offset = 75776
    
    # Parse offset
    if len(sys.argv) > 3 and not sys.argv[3].startswith('--'):
        start_offset = int(sys.argv[3])
    else:
        start_offset = default_offset
    
    # Check alignment option (default is 16KB)
    align_option = 16384  # Default 16KB alignment
    for arg in sys.argv[3:]:
        if arg == '--align8k':
            align_option = 8192
            break
        elif arg == '--align16k':
            align_option = 16384
            break
        elif arg == '--no-align':
            align_option = None
            break
    
    # Alignment processing
    original_offset = start_offset
    if align_option:
        start_offset = align_to_boundary(start_offset, align_option)
        if start_offset != original_offset:
            print(f"Aligned offset to {align_option} byte boundary: {original_offset} -> {start_offset}")
    
    print(f"Input file: {input_file}")
    print(f"Output file: {output_file}")
    print(f"Start offset: {start_offset} bytes")
    
    # Parse string resources
    resources = parse_string_resource_file(input_file)
    print(f"Number of strings loaded: {len(resources)}")
    
    # Debug output
    for idx in sorted(resources.keys()):
        print(f"  [{idx}] {repr(resources[idx][:50] + '...' if len(resources[idx]) > 50 else resources[idx])}")
    
    # Create binary
    binary_data = create_resource_binary(resources, start_offset)
    
    # Write to file
    with open(output_file, 'wb') as f:
        f.write(binary_data)
    
    print(f"\nConversion complete:")
    print(f"  Offset table size: {4 + (max(resources.keys())+1)*4 if resources else 4} bytes")
    print(f"  String data size: {len(binary_data) - (4 + (max(resources.keys())+1)*4 if resources else 4)} bytes")
    print(f"  Total size: {len(binary_data)} bytes")
    
    # Display layout information
    string_data, offset_table = calculate_string_layout(resources, start_offset)
    print(f"\nString layout information:")
    for i, idx in enumerate(sorted(resources.keys())):
        bank, lohi, offset = offset_table[i]
        window = "ROMH" if lohi == 1 else "ROML"
        print(f"  [{idx}] Bank {bank}, {window}, Offset 0x{offset:04X}")

if __name__ == "__main__":
    main()