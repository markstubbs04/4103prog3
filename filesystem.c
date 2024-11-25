#include <stdint.h>
#include "softwaredisk.c"
#include "filesystem.h"
#include <string.h>

#define NUM_DIRECT_INODE_BLOCKS 13
#define MAX_NUMBER_OF_FILES 256
#define BITMAP_BLOCK_SIZE 1       // needs enough size for each file
#define INODE_ARRAY_BLOCK_SIZE 8  // (256 inodes * 32 bytes each) / 1024 bytes
#define FILES_ARRAY_BLOCK_SIZE 75 // (256 files * 300 bytes each) / 1024 bytes
#define FILE_DATA_BLOCK_SIZE 6656 // 256 files * 26 blocks each
#define SOFTWARE_DISK_BLOCK_SIZE 6740

#define MAX_FILE_NAME_SIZE 472

FSError Error;

InodeBlock *Inodes;

struct FileInternals
{ // 512 bytes
    // char name[260];        // 260 bytes
    char name[MAX_FILE_NAME_SIZE]; // 472 bytes
    Inode *inode;                  // 32 bytes
    uint32_t filePosition;         // 4 bytes
    _FileMode fileMode;            // 4 byte
};

// file type used by user code
typedef struct FileInternals *File;

typedef struct Inode // 32 bytes
{
    int32_t size;                                 // size of the file | 4 bytes
    uint16_t blocks[NUM_DIRECT_INODE_BLOCKS + 1]; // 28 bytes | all of the pointers to direct
                                                  // blocks and a pointer to another array of indirect blocks
} Inode;

typedef struct InodeBlock
{
    // 1024       /    32      => 32 inodes in 1 block
    Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];
} InodeBlock;

typedef uint32_t _FileMode; // 4 byte

typedef struct BitMap
{
    // bitmap is the size of all structures' blocks
    unsigned char bitmap[(BITMAP_BLOCK_SIZE + INODE_ARRAY_BLOCK_SIZE + FILES_ARRAY_BLOCK_SIZE + FILE_DATA_BLOCK_SIZE) / 8]; // array that you can access each bit to check for file system availability
} BitMap;

BitMap bitmap;

InodeBlock inodeBlockArray[INODE_ARRAY_BLOCK_SIZE];

File files[MAX_NUMBER_OF_FILES]; // array of all the fileinternal pointers

// unsigned char bitmap[SOFTWARE_DISK_BLOCK_SIZE]; // 1024
// memset(bitmap, 0, software_disk_size() * sizeof(bitmap));

// HELPER FUNCTIONS:

// fsmalloc that return a "pointer" which is the first blocknum it finds that is free

File create_file(char *name) // might need to add a null terminating character during file creation
{
    if (name[0] == '\0' || strlen(name) > MAX_FILE_NAME_SIZE)
    {
        Error = FS_ILLEGAL_FILENAME;
        return NULL;
    }

    int32_t index = -1; // tracks the index
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (files[i] != NULL && !strcmp(files[i]->name, name))
        {
            Error = FS_FILE_ALREADY_EXISTS;
            return NULL;
        }
        else if (files[i] == 0)
        {
            index = i;
        }
    }

    if (index == -1)
    {
        Error = FS_OUT_OF_SPACE;
        return NULL;
    }

    File newfile = malloc(sizeof(struct FileInternals));
    files[index] = newfile;
    strcpy(newfile->name, name);
    newfile->filePosition = 0;

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            if (inodeBlockArray[i].inodes[j].size == -1)
            {
                // space is free, grab that inode and set its size to 0 to show it is initialized
                newfile->inode = &(inodeBlockArray[i].inodes[j]);
                newfile->inode->size = 0;
                break;
            }
        }
    }

    if (newfile->inode == NULL)
    {
        Error = FS_OUT_OF_SPACE;
        free(newfile);
        return NULL;
    }

    return open_file(name, READ_WRITE);
}

bool file_exists(char *name)
{
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (files[i] != NULL)
        {
            if (!strcmp(files[i]->name, name))
            {
                return true;
            }
        }
    }
    return false;
}

File open_file(char *name, FileMode mode)
{
    File file;
    int index = -1;
    for (int i = 0; i < MAX_NUMBER_OF_FILES; i++)
    {
        if (files[i] != NULL)
        {
            if (!strcmp(files[i]->name, name))
            {
                index = i;
            }
        }
    }

    if (index != -1)
    {
        file = files[index];
    }
    else
    {
        Error = FS_FILE_NOT_FOUND;
        return NULL;
    }
    file->fileMode = mode;

    // check if file already has space
    // find a free space
    // for (int j = 0; j < NUM_DIRECT_INODE_BLOCKS + 1; j++)
    // {
    //     if (j == NUM_DIRECT_INODE_BLOCKS)
    //     {
    //         // indirect
    //     }

    if (file->inode->blocks[0] == 0) // only on new files that have no allocated blocks
    {
        int16_t blocknum = findFreeSpace();
        if (blocknum == -1)
        {
            Error = FS_OUT_OF_SPACE;
            return NULL;
        }
        file->inode->blocks[0] = blocknum; // gives the first free space in the bitmap
    }
    // }

    return file;
}

uint64_t write_file(File file, void *buf, uint64_t numbytes)
{ // I think we need to check bitmap and request space from the software disk and get that blocknum (pointer)
}

// bitmap helper functions

// finds free space in the bitmap
int16_t findFreeSpace()
{
    for (uint64_t j = 0; j < FILE_DATA_BLOCK_SIZE; j++)
    {
        if (is_bit_set(bitmap.bitmap, j))
        {
            return j;
        }
    }
    return -1;
}

// set jth bit in a bitmap composed of 8-bit integers
void set_bit(uint64_t j)
{
    bitmap.bitmap[j / 8] != (1 << (j % 8));
}

// clear jth bit in a bitmap composed of 8-but integers
void clear_bit(uint64_t j)
{
    bitmap.bitmap[j / 8] &= ~(1 << (j % 8));
}

// returns true if jth bit is set in a bitap of 8-bit integers,
// otherwise false
bool is_bit_set(unsigned char *bitmap, uint64_t j)
{
    return bitmap[j / 8] & (1 << (j % 8));
}

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

int main()
{
    formatfs();

    uint16_t numBlocks = software_disk_size();
};