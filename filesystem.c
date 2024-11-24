#include <stdint.h>
#include "softwaredisk.c"
#include "filesystem.h"

#define NUM_DIRECT_INODE_BLOCKS 13
#define MAX_NUMBER_OF_FILES 512

FSError Error;

InodeBlock *Inodes;

struct FileInternals { // 300 bytes
    char *name; // 264 bytes
    Inode *inode; // 32 bytes
    _FileMode fileMode; // 1 byte
};

// file type used by user code
typedef struct FileInternals *File;

typedef struct Inode // 32 bytes
{
    uint32_t size;
    uint16_t b[NUM_DIRECT_INODE_BLOCKS + 1];
} Inode;

typedef struct InodeBlock
{
    Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];
} InodeBlock;

typedef uint32_t _FileMode; // 4 byte

unsigned char bitmap[SOFTWARE_DISK_BLOCK_SIZE]; // 1024

// HELPER FUNCTIONS:

// fsmalloc that return a "pointer" which is the first blocknum it finds that is free


File create_file(char *name) {

}

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

void formatfs() {
    init_software_disk();
    memset(bitmap, 0, software_disk_size() * sizeof(bitmap));
    
    //Inodes = malloc(sizeof(InodeBlock));
};

int main()
{
    formatfs();

    uint16_t numBlocks = software_disk_size();
};