#ifndef DEF_PROPS_BIN_H
#define DEF_PROPS_BIN_H

#include <stdint.h>
#include <stdbool.h>

// Field lengths matching Python script
#define MD5_LENGTH_BIN 16
#define MD5_LENGTH 32 // 16 bytes in hex string
#define MAPPER_LENGTH 8
#define DIFFICULTY_LENGTH 32
#define CONTROL_SWAP_LENGTH 8
#define CONTROL_LENGTHL 32
#define CONTROL_LENGTHR 32
#define PADDLE_SWAP_LENGTH 8
#define REGION_LENGTH 16
#define YOFFSET_LENGTH 8
#define HEIGHT_LENGTH 8

// Structure to hold ROM properties
typedef struct {
    char mapper[MAPPER_LENGTH];
    char difficulty[DIFFICULTY_LENGTH];
    char control_swap[CONTROL_SWAP_LENGTH];
    char control_left[CONTROL_LENGTHL];
    char control_right[CONTROL_LENGTHR];
    char paddle_swap[PADDLE_SWAP_LENGTH];
    char region[REGION_LENGTH];
    char yoffset[YOFFSET_LENGTH];
    char height[HEIGHT_LENGTH];
} rom_properties_t;

// Function to initialize the properties database
bool defprops_init(const char* filename);

// Function to get properties for a ROM by MD5
bool defprops_get_properties(const char* md5, rom_properties_t* props);

// Function to clean up resources
void defprops_cleanup(void);

#endif // DEF_PROPS_BIN_H 