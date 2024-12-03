#include <stdint.h>
#include <string.h>
#include "softwaredisk.h"
#include "filesystem.h"

// Also have to redeclare it here to use
typedef struct FreeBitmap
{
    unsigned char map[SOFTWARE_DISK_BLOCK_SIZE];
} FreeBitmap;

extern FreeBitmap bitmap; // need this so we can use it here

// set jth bit in a bitmap composed of 8-bit integers
void set_jth_bit(uint64_t j)
{
    bitmap.map[j / 8] |= (1 << (j % 8));
}

void formatfs()
{
    init_software_disk();

    memset(bitmap.map, 0, SOFTWARE_DISK_BLOCK_SIZE);
    set_jth_bit(0);
    write_sd_block(bitmap.map, 0);
};

int main()
{
    formatfs();
    return 0;
};