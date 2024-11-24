#include <stdint.h>
#include "softwaredisk.h"

uint16_t NUM_DIRECT_INODE_BLOCKS = 13;

uint32_t *bitmap;
InodeBlock *Inodes;

typedef struct FileInternals *File;

typedef enum
{
    READ_ONLY,
    READ_WRITE
} FileMode;

typedef struct Inode
{
    uint32_t size;
    char *name;
    uint16_t b[NUM_DIRECT_INODE_BLOCKS + 1];
} Inode;

typedef struct InodeBlock
{
    Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];
} InodeBlock;

File open_file(char *name, FileMode mode)
{
    switch (mode)
    {
    case READ_ONLY:
        break;
    case READ_WRITE:
        break;
    }

    // Inode inode =
};

Inode findInode(char *name)
{
    // Inode *current_dir = Inodes[0];
    for (int i = 0; i < Inodes->inodes.length; i++)
    {
        if (!strcmp(name, Inodes[i].name))
        {
            return Inodes[i];
        }
    }
}

void formatfs()
{
    init_software_disk();

    bitmap = malloc(software_disk_size() * sizeof(bitmap));
    memset(bitmap, 0, software_disk_size() * sizeof(bitmap));

    Inodes = malloc(sizeof(InodeBlock));
};

int main()
{
    formatfs();

    uint16_t numBlocks = software_disk_size();
};