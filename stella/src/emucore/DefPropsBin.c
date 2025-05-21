#include "DefPropsBin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static FILE* props_file = NULL;
static uint32_t num_entries = 0;
static uint32_t entry_size = 0;

// Convert hex string to binary
static bool hex_to_binary(const char* hex, unsigned char* binary, size_t binary_len) {
    if (strlen(hex) != binary_len * 2) {
        return false;
    }
    
    for (size_t i = 0; i < binary_len; i++) {
        char hex_byte[3] = {hex[i*2], hex[i*2+1], '\0'};
        char* end;
        binary[i] = strtol(hex_byte, &end, 16);
        if (*end != '\0') {
            return false;
        }
    }
    return true;
}

bool defprops_init(const char* filename) {
    props_file = fopen(filename, "rb");
    if (!props_file) {
        return false;
    }

    // Read header: number of entries and entry size
    if (fread(&num_entries, sizeof(uint32_t), 1, props_file) != 1 ||
        fread(&entry_size, sizeof(uint32_t), 1, props_file) != 1) {
        fclose(props_file);
        props_file = NULL;
        return false;
    }

    return true;
}

static bool read_fixed_string(char* dest, size_t max_len, size_t field_len) {
    if (fread(dest, 1, field_len, props_file) != field_len) {
        return false;
    }
    dest[field_len] = '\0';  // Ensure null termination
    return true;
}

bool defprops_get_properties(const char* md5, rom_properties_t* props) {
    if (!props_file || !md5 || !props) {
        return false;
    }

    // Convert input MD5 string to binary
    unsigned char md5_binary[MD5_LENGTH_BIN];
    if (!hex_to_binary(md5, md5_binary, MD5_LENGTH_BIN)) {
        return false;
    }

    // Binary search for the MD5
    long left = 0;
    long right = num_entries - 1;
    unsigned char current_md5[MD5_LENGTH_BIN];
    
    while (left <= right) {
        long mid = (left + right) / 2;
        
        // Calculate position in file
        long pos = sizeof(uint32_t) * 2 + mid * entry_size;
        fseek(props_file, pos, SEEK_SET);
        // Read MD5
        if (fread(current_md5, 1, MD5_LENGTH_BIN, props_file) != MD5_LENGTH_BIN) {
            return false;
        }
        
        // Compare binary MD5s
        int cmp = memcmp(md5_binary, current_md5, MD5_LENGTH_BIN);
        if (cmp == 0) {
            // Found the entry, read all properties
            fseek(props_file, pos, SEEK_SET);
            
            // Skip MD5
            fseek(props_file, MD5_LENGTH_BIN, SEEK_CUR);
            
            // Read all properties
            if (!read_fixed_string(props->mapper, sizeof(props->mapper), MAPPER_LENGTH) ||
                !read_fixed_string(props->difficulty, sizeof(props->difficulty), DIFFICULTY_LENGTH) ||
                !read_fixed_string(props->control_swap, sizeof(props->control_swap), CONTROL_SWAP_LENGTH) ||
                !read_fixed_string(props->control, sizeof(props->control), CONTROL_LENGTH) ||
                !read_fixed_string(props->paddle_swap, sizeof(props->paddle_swap), PADDLE_SWAP_LENGTH) ||
                !read_fixed_string(props->region, sizeof(props->region), REGION_LENGTH) ||
                !read_fixed_string(props->yoffset, sizeof(props->yoffset), YOFFSET_LENGTH) ||
                !read_fixed_string(props->height, sizeof(props->height), HEIGHT_LENGTH)) {
                return false;
            }
            return true;
        }
        
        if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return false;
}

void defprops_cleanup(void) {
    if (props_file) {
        fclose(props_file);
        props_file = NULL;
    }
} 