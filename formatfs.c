#include <stdint.h>
#include "softwaredisk.c"
// #include "filesystem.h"
#include "filesystem.c"

void formatfs()
{
    init_software_disk();

    memset(bitmap.bitmap, 0, software_disk_size() * sizeof(bitmap.bitmap));

    // setting bitmap block + 8 blocks for the inode block array as taken
    for (uint64_t i = 0; i < (BITMAP_BLOCK_SIZE + INODE_ARRAY_BLOCK_SIZE + FILES_ARRAY_BLOCK_SIZE); i++)
    {
        set_bit(i);
    }

    // initialize Inode Block Array
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            Inode inode;
            inode.size = -1;
            bzero(inode.blocks);
            inodeBlockArray[i].inodes[j] = inode;
        }
    }
};

int main()
{
    formatfs();

    // uint16_t numBlocks = software_disk_size();
};