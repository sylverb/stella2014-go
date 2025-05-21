#!/usr/bin/env python3
import re
import struct
import sys

# Used property types
property_type = [
    "mapper", # Cartridge_Type
    "difficulty", # Console_LeftDifficulty
    "control_swap", # Console_SwapPorts
    "control", # Controller_Left
    "paddle_swap", # Controller_SwapPaddles
    "region", # Display_Format
    "yoffset", # Display_YStart
    "height", # Display_Height
]

# Define fixed lengths for each field
FIELD_LENGTHS = {
    "mapper": 8,
    "difficulty": 32,
    "control_swap": 8,
    "control": 32,
    "paddle_swap": 8,
    "region": 16,
    "yoffset": 8,
    "height": 8
}

def parse_defprops(filename):
    data = []
    with open(filename, 'r') as file:
        content = file.read()
        # Find the array definition
        match = re.search(r'static const char\* DefProps\[DEF_PROPS_SIZE\]\[21\] = \{(.*?)\};', content, re.DOTALL)
        if not match:
            print("Error: Could not find DefProps array in file")
            return data
            
        array_content = match.group(1)
        # Parse each entry
        entries = re.finditer(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}', array_content)
        
        for entry in entries:
            # Only keep the MD5 and the properties we need
            data.append([
                entry.group(1),  # MD5
                entry.group(7),  # mapper
                entry.group(8),  # difficulty
                entry.group(11), # control_swap
                entry.group(12), # control
                entry.group(14), # paddle_swap
                entry.group(16), # region
                entry.group(17), # yoffset
                entry.group(18)  # height
            ])
            
    return data

def create_binary_format(data, output_file):
    if not data:
        print("Error: No data to write")
        return
        
    # Sort entries by MD5
    sorted_data = sorted(data, key=lambda x: x[0])
    
    # Calculate total entry size
    entry_size = sum(FIELD_LENGTHS.values()) + 16 # 16 bytes for MD5
    
    # Write binary file
    with open(output_file, 'wb') as f:
        # Write header: number of entries and entry size
        f.write(struct.pack('<II', len(sorted_data), entry_size))
        
        # Write each entry
        for entry in sorted_data:
            # Write MD5 as binary (16 bytes)
            md5_bytes = bytes.fromhex(entry[0])
            f.write(md5_bytes)
            
            # Write each property as fixed-length string
            for i, prop in enumerate(entry[1:]):  # Skip MD5
                field_name = property_type[i]
                field_len = FIELD_LENGTHS[field_name]
                # Pad or truncate string to fixed length
                prop_bytes = prop.encode('utf-8')[:field_len]
                prop_bytes = prop_bytes.ljust(field_len, b'\0')
                f.write(prop_bytes)

def main():
    if len(sys.argv) != 3:
        print("Usage: convert_defprops.py <DefProps.hxx> <output_file>")
        sys.exit(1)
        
    defprops_file = sys.argv[1]
    output_file = sys.argv[2]
    data = parse_defprops(defprops_file)
    if data:
        create_binary_format(data, output_file)
        print(f"Created {output_file} with {len(data)} entries")
    else:
        print("Error: No data parsed from file")
        sys.exit(1)

if __name__ == "__main__":
    main() 