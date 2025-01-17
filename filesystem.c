#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include "softwaredisk.h"
#include "filesystem.h"

// Alex Brodsky && Mark Stubbs

/*
Bitmap:
1 block = 8,192 blocks

Inodes:
1 block each * 256 files
= 256 blocks

Dir Entries:
1 block each * 256 files
= 256 blocks

Free data:
13 direct + 512 indirect blocks = 525 blocks each
Limited to 7679 Free Data block due to bitmap size

We use 1 block for the bitmap, 256 for the Inodes, 256 for the Dir Entries that makes 513 blocks. Which leave 8192 - 513 = 7679 blocks for free data.

So, once the user gets to 7679 blocks, they will not be allowed to make anymore files.
*/

#define MAX_NUMBER_OF_FILES 256
#define NUM_DIRECT_INODE_BLOCKS 13
#define NUM_INDIRECT_INODE_BLOCKS 512
#define BITMAP_BLOCK_SIZE 1 // The bitmap for the entire file system
#define BITMAP_BLOCKNUM 0

#define INODE_BLOCK_SIZE 1 // 1 for each inode * 256 so 256 total
#define INODE_FIRST_BLOCKNUM 1

#define DIR_ENTRY_BLOCK_SIZE 1 // 1 for each dir entry * 256 so 256 total
#define DIR_ENTRY_FIRST_BLOCKNUM 257

#define DIR_ENTRY_AND_INODE_SIZE 256 // 256 files

#define FILE_DATA_BLOCK_SIZE 7679 // Max size left for file data
#define FILE_DATA_FIRST_BLOCKNUM 513
#define MAX_DIRECT_BLOCK 13
#define MAX_INDIRECT_BLOCK 512

#define FILE_SYSTEM_SIZE 8192

#define MAX_FILE_NAME_SIZE 1021

FSError fserror = FS_NONE;

typedef enum DataType
{
    INODE,
    DIRECTORY_ENTRY,
    BLOCKS,
} DataType;

typedef struct FreeBitmap
{
    // bitmap is the size of all structures' blocks
    unsigned char map[SOFTWARE_DISK_BLOCK_SIZE]; // array that you can access each bit to check for file system availability
} FreeBitmap;

FreeBitmap bitmap;

typedef struct Inode // 32 bytes
{
    int32_t size;                                 // size of the file | 4 bytes
    uint16_t blocks[NUM_DIRECT_INODE_BLOCKS + 1]; // 28 bytes | all of the pointers to direct
                                                  // blocks and a pointer to another array of indirect blocks
} Inode;

typedef struct InodeBlock
{
    Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)]; // 32 inodes per block
} InodeBlock;

typedef struct DirEntry
{                           // 1024 total
    char name[MAX_FILE_NAME_SIZE];             // min 257 actually 1021
    bool isFileOpen;        // 1 byte
    uint16_t inodeBlockNum; // 2 bytes
} DirEntry;

typedef struct IndirectBlock
{
    uint16_t blocks[NUM_INDIRECT_INODE_BLOCKS];
} IndirectBlock;

struct FileInternals
{
    uint32_t filePosition;    // 4 bytes
    FileMode fileMode;        // 4 byte
    DirEntry *directoryEntry; // 300 bytes
    Inode *inode;
};
// file type used by user code
typedef struct FileInternals *File;

struct FileInternals files[MAX_NUMBER_OF_FILES]; // array of all the fileinternal pointers

// HELPER FUNCTIONS:

size_t get_data_size(DataType type)
{
    switch (type)
    {
    case INODE:
        return sizeof(Inode);
    case DIRECTORY_ENTRY:
        return sizeof(DirEntry);
    case BLOCKS:
        return SOFTWARE_DISK_BLOCK_SIZE;
    default:
        return 0; // Unsupported type
    }
}

// READS DATA FROM BLOCKNUM AT POSITION TILL THE END OF THE FILE
// Correctly null terminates the end of the data read.
unsigned char *read_data_from_disk(uint16_t blocknum, uint32_t position)
{
    if (position >= SOFTWARE_DISK_BLOCK_SIZE || position < 0)
    {
        return NULL;
    }
    unsigned char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    if (!read_sd_block(buf, blocknum))
    {
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }
    unsigned char *data = malloc(sizeof(char) * ((SOFTWARE_DISK_BLOCK_SIZE - position) + 1));
    if (!data)
    {
        return NULL;
    }
    memcpy(data, buf + position, sizeof(char) * (SOFTWARE_DISK_BLOCK_SIZE - position));
    data[((SOFTWARE_DISK_BLOCK_SIZE - position) + 1)] = '\0';
    fserror = FS_NONE;
    return data;
}

// reads type DataType at blocknum with optionl position in file
void *read_from_disk(DataType type, uint16_t blocknum)
{   
   // printf("READ FROM DISK\n");
    char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    if (!read_sd_block(buf, blocknum))
    {
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }

    size_t size = get_data_size(type);
    if (size == 0)
    { // Unsupported type
        return NULL;
    }

    void *object = malloc(size);
    if (!object)
    {
        return NULL;
    }
    
    memcpy(object, buf, size);
   // printf("END READ FROM DISK\n");
    fserror = FS_NONE;
    return object;
}

bool write_to_disk(void *data, DataType type, uint16_t blocknum)
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    size_t size = get_data_size(type);
    if (size == 0)
    { // Unsupported type
        return false;
    }

    memcpy(buf, data, size);

    bool result = write_sd_block(buf, blocknum);

    // testing
    if (type == DIRECTORY_ENTRY) {
        // read_sd_block(buf, blocknum);

        // buf[SOFTWARE_DISK_BLOCK_SIZE-1] = '\0';

        DirEntry *dirEntry = (DirEntry *) read_from_disk(DIRECTORY_ENTRY, blocknum);

        // printf("testing writing %s\n", dirEntry->name);
    }

    return result;
}

bool clear_block(uint16_t blocknum)
{
    char clear[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    fserror = FS_NONE;
    return write_to_disk(&clear, BLOCKS, blocknum);
}

char *getFullFileName(char *name, size_t length)
{
    if (length > MAX_FILE_NAME_SIZE) {
        return NULL;
    }

    // Allocate exactly 1021 bytes
    char *padded_name = (char *)malloc(MAX_FILE_NAME_SIZE);
    if (!padded_name)
    {
        return NULL;
    }

    strncpy(padded_name, name, length);

    // Fill the remaining bytes with '\0', but apparently strncpy does this already
    // memset(padded_name + length, '\0', MAX_FILE_NAME_SIZE - length);

    padded_name[MAX_FILE_NAME_SIZE - 1] = '\0';

    return padded_name;
}

// BITMAP HELPERS:
// set jth bit in a bitmap composed of 8-bit integers
void set_bit(uint16_t j)
{
    bitmap.map[j / 8] |= (1 << (j % 8));
}

// clear jth bit in a bitmap composed of 8-bit integers
void clear_bit(uint16_t j)
{
    bitmap.map[j / 8] &= ~(1 << (j % 8));
}

// returns true if jth bit is set in a bitap of 8-bit integers,
// otherwise false
bool is_bit_set(uint16_t j)
{
    return bitmap.map[j / 8] & (1 << (j % 8));
}

// try to find an existing directory entry, if u cant 
int16_t findDirEntry(char *name)
{   
//    printf("FIND DIR ENTRY\n");
    // char buf[SOFTWARE_DISK_BLOCK_SIZE];
    read_sd_block(bitmap.map, 0);
    for (uint16_t i = DIR_ENTRY_FIRST_BLOCKNUM; i < DIR_ENTRY_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; i++)
    {   
        // printf("Checking for dir entry at block %d\n", i);
        // if (i == 257) {
        //     DirEntry *dirEntry = (DirEntry *)read_from_disk(DIRECTORY_ENTRY, i);
        //     if (dirEntry == NULL) {
        //         fserror = FS_IO_ERROR;
        //         return -1;
        //     }

        //    printf("ABout to string compare\n");
        //    printf("strlen of name passed in: %lu\n", strlen(name));
        //    printf("strlen of DIR ENTRY name in: %lu\n", strlen(dirEntry->name));
        //     if (strcmp(dirEntry->name, name) == 0) // its a match!
        //     {
        //        printf("FOUND A MATCH\n");
        //         return i + DIR_ENTRY_FIRST_BLOCKNUM;
        //     }
        //    printf("Not a match");
        // }
        if (is_bit_set(i))
        {
            DirEntry *dirEntry = (DirEntry *)read_from_disk(DIRECTORY_ENTRY, i);
            if (dirEntry == NULL) {
                fserror = FS_IO_ERROR;
                return -1;
            }

        //    printf("ABout to string compare\n");
        //    printf("strlen of name passed in: %lu\n", strlen(name));
        //    printf("strlen of DIR ENTRY name in: %lu\n", strlen(dirEntry->name));
            if (strcmp(dirEntry->name, name) == 0) // its a match!
            {
            //    printf("FOUND A MATCH\n");
                return i + DIR_ENTRY_FIRST_BLOCKNUM;
            }
        //    printf("Not a match");
        }
    }
//    printf("END FIND DIR ENTRY\n");
    return -1;
}

// finds free space in the bitmap
int16_t findFreeDataSpace(void)
{
    for (uint16_t j = FILE_DATA_FIRST_BLOCKNUM; j < FILE_SYSTEM_SIZE; j++)
    {
        if (!is_bit_set(j))
        {
            return j;
        }
    }
    return -1;
}

// finds free space in the bitmap
int16_t findFreeInodeSpace(void)
{
    for (uint16_t j = INODE_FIRST_BLOCKNUM; j < INODE_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; j++)
    {
        if (!is_bit_set(j))
        {
            return j;
        }
    }
    return -1;
}

int16_t findFreeDirEntrySpace(void)
{
    for (uint16_t j = DIR_ENTRY_FIRST_BLOCKNUM; j < DIR_ENTRY_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; j++)
    {
        if (!is_bit_set(j))
        {
            return j;
        }
    }
    return -1;
}

// MAIN FUNCTIONS:

File create_file(char *name)
{   
    // printf("CREATE FILE\n");
    if (name[0] == '\0' || strlen(name) > 257)
    {
        fserror = FS_ILLEGAL_FILENAME;
       // printf("Illegal filename");
        return NULL;
    }

    int32_t index = -1; // tracks the index

    // char buf[SOFTWARE_DISK_BLOCK_SIZE];
    bool success = read_sd_block(bitmap.map, 0);
    if (!success) {
        // printf("CREATE FILE READ BITMAP BLOCK FAILED\n");
        fserror = FS_IO_ERROR;
        return NULL;
    }
    else {
    }
    bool found = true;
    // reading dir entries to check for duplicate names
    for (uint16_t i = DIR_ENTRY_FIRST_BLOCKNUM; i < DIR_ENTRY_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; i++)
    {
        if (is_bit_set(i))
        {
            // read_sd_block(buf, i);

            // We want the file to not exist yet with that name
            if (findDirEntry(name) != -1)
            {
                fserror = FS_FILE_ALREADY_EXISTS;
                return NULL;
            }
        }
        else if (found)
        {
            index = i;     // gets earliest open spot
            found = false; // prev
        }
    }

    if (index == -1)
    {
        fserror = FS_OUT_OF_SPACE;
        // printf("CreateFile directory entry couldnt find a spot\n");
        return NULL;
    }

    File file = malloc(sizeof(struct FileInternals));

    Inode *newInode = malloc(sizeof(Inode));
    newInode->size = 0;
    memset(newInode->blocks, '\0', NUM_DIRECT_INODE_BLOCKS + 1);
    file->inode = newInode;

    // find the first data block that is available
    int16_t blocknum = findFreeDataSpace();
    if (blocknum == -1)
    {
        fserror = FS_OUT_OF_SPACE;
        //  printf("CreateFile INODE couldnt find a spot\n");
        return NULL;
    }
    // printf("Checked for free space for INODE\n");
    file->inode->blocks[0] = (uint16_t)blocknum;
    clear_block((uint16_t)blocknum);
    set_bit((uint16_t)blocknum); // setting bitmap for when we allocate a block to newly opened file data

    // find a free space for the inode and dir entry
    int16_t inodeBlockNum = findFreeInodeSpace();
    if (inodeBlockNum == -1)
    {
        fserror = FS_OUT_OF_SPACE;
        // printf("CreateFile inodeblocknum couldnt find a spot\n");
        return NULL;
    }
    set_bit((uint16_t)inodeBlockNum); // set the new Inode in bitmapp

    // printf("INODE BLOCK: %d\n", inodeBlockNum);



    // write the inode and dir entry
    if (!write_to_disk(newInode, INODE, (uint16_t)inodeBlockNum))
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }

    DirEntry *newDirEntry = malloc(sizeof(DirEntry));
    newDirEntry->inodeBlockNum = inodeBlockNum;
    char *fileName = getFullFileName(name, strlen(name));
    strcpy(newDirEntry->name, fileName);
    newDirEntry->isFileOpen = false;
    file->directoryEntry = newDirEntry;
    // printf("Creating a file with the name: %s\n", file->directoryEntry->name);

    uint16_t dirEntryBlockNum = (uint16_t)inodeBlockNum + MAX_NUMBER_OF_FILES;
    if (!write_to_disk(newDirEntry, DIRECTORY_ENTRY, dirEntryBlockNum))
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }
    set_bit(dirEntryBlockNum); // marks the directory entry as taken
    // printf("DIRENTRY BLOCK: %d\n", dirEntryBlockNum);

    file->filePosition = (uint32_t)0;

    // updates bitmap
    if (!write_sd_block(bitmap.map, 0))
    {
        fserror = FS_IO_ERROR;
        return false;
    }

    // printf("END CREATE\n");
    fserror = FS_NONE;
    return open_file(name, READ_WRITE);
}

File open_file(char *name, FileMode mode)
{
    // printf("OPENING\n");
    File file = malloc(sizeof(struct FileInternals));
    int16_t index = findDirEntry(name);
    if (index == -1)
    {
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }
    // printf("File FOund\n");

    file->filePosition = 0;
    file->fileMode = mode;

    DirEntry *dirEntry = malloc(sizeof(DirEntry));
    void *data = read_from_disk(DIRECTORY_ENTRY, (uint16_t)index);
    if (data == NULL)
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }
    else
    {
        dirEntry = (DirEntry *)data;
    }
    if (dirEntry->isFileOpen)
    {
        fserror = FS_FILE_OPEN;
        return NULL;
    }
    char *fileName = getFullFileName(name, strlen(name));
    strcpy(dirEntry->name, fileName);
    dirEntry->isFileOpen = true;
    file->directoryEntry = dirEntry;
    // printf("OPEN FILE name %s\n", file->directoryEntry->name);

    Inode *inode = malloc(sizeof(Inode));
    data = read_from_disk(INODE, (uint16_t)index);
    if (data == NULL)
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }
    else
    {
        inode = (Inode *)data;
    }

    file->inode = inode;

    // printf("END OPEN\n");
    fserror = FS_NONE;
    return file;
}

void close_file(File file)
{  
    if (file == NULL || !file->directoryEntry->isFileOpen) {
        fserror = FS_FILE_NOT_OPEN;
        return;
    }
    // printf("CLOSING: File name: %s\n", file->directoryEntry->name);
    if (findDirEntry("superfile") != -1) {
        // printf("Found the superfile\n");
    }
    else {
        // printf("didnt find the superfile\n");
    }
    file->directoryEntry->isFileOpen = false;

    fserror = FS_NONE;
    if (!write_to_disk(file->directoryEntry, DIRECTORY_ENTRY, file->directoryEntry->inodeBlockNum + MAX_NUMBER_OF_FILES)) {
        fserror = FS_IO_ERROR;
    }
    free(file);
}

uint64_t read_file(File file, void *buf, uint64_t numbytes)
{
    if (!file->directoryEntry->isFileOpen)
    {
        fserror = FS_FILE_NOT_OPEN;
        return 0;
    }

    fserror = FS_NONE;

    // char *tempBuf = malloc(numbytes * sizeof(char));
    unsigned char *tempBuf = calloc(numbytes, sizeof(char)); // initialize it with zeros too
    uint64_t bytesRead = 0;

    int32_t size = file->inode->size;

    int16_t blockIndex = file->filePosition / SOFTWARE_DISK_BLOCK_SIZE;
    int16_t positionInBlock = file->filePosition % SOFTWARE_DISK_BLOCK_SIZE;
    int16_t indirectBlockIndex = 0;
    if (blockIndex > NUM_DIRECT_INODE_BLOCKS)
    {
        indirectBlockIndex = blockIndex - NUM_DIRECT_INODE_BLOCKS;
        blockIndex = NUM_DIRECT_INODE_BLOCKS;
    }
    int16_t currBlock = blockIndex;

    // 13 direct + 1 indirect block, so dont want to exceed. 0 is a not set block
    while (currBlock <= NUM_DIRECT_INODE_BLOCKS+1 && indirectBlockIndex < MAX_INDIRECT_BLOCK && file->inode->blocks[currBlock] != 0 && bytesRead + (uint64_t)file->filePosition < (uint64_t)size)
    {
        if (currBlock > NUM_DIRECT_INODE_BLOCKS) // at indirect block
        {
            int16_t currIndirectBlock = indirectBlockIndex;
            uint16_t *indirectBlocks = (uint16_t *)read_from_disk(BLOCKS, file->inode->blocks[NUM_DIRECT_INODE_BLOCKS]);

            while (currIndirectBlock < MAX_INDIRECT_BLOCK && indirectBlocks[currIndirectBlock] != 0 && (int32_t)bytesRead + (int32_t)file->filePosition < size)
            {
                // READ WHAT IS IN FRONT STARTING AT POSITION IN BLOCK
                // unsigned char *currBuf = malloc((SOFTWARE_DISK_BLOCK_SIZE - positionInBlock + 1) * sizeof(char));
                // Software disk block size is 1024, so subtract the current position to get how many bytes you need to read total
                //  Ex: if positionInBlock is 0 then we want to read SOFTWARE_DISK_BLOCK_SIZE size data, not positionInBlock size data
                unsigned char *currBuf = read_data_from_disk((uint16_t)indirectBlocks[currIndirectBlock], positionInBlock);
                // for every iteration other than the first iteration the position is 0
                if (currBuf == NULL)
                {
                    fserror = FS_IO_ERROR;
                    break;
                }

                uint64_t currLength = strlen((const char *) currBuf);
                uint64_t bytesLeftToRead = numbytes - bytesRead;
                // going to read more than the length of the file
                if ((int32_t)bytesLeftToRead + (int32_t)file->filePosition > file->inode->size)
                {
                    bytesLeftToRead = file->inode->size - file->filePosition;
                }

                if (currLength + bytesRead < numbytes)
                { // not done reading numbytes

                    // reads the smaller amount of data
                    if (SOFTWARE_DISK_BLOCK_SIZE - (uint64_t)positionInBlock < bytesLeftToRead)
                    {
                        bytesLeftToRead = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;
                        strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                        bytesRead += bytesLeftToRead;
                    }
                    else
                    {
                        // WE ARE DONE READING RAN OUT OF SPACE
                        strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                        bytesRead += bytesLeftToRead;
                        break;
                    }
                }
                else
                { // last read of numbytes

                    // numbytes - bytesRead will never be greater than SOFTWARE_DISK_BLOCK_SIZE because of the previous if statement
                    if (bytesLeftToRead > numbytes - bytesRead)
                    { // if the end of the file comes after how far I want to read
                        bytesLeftToRead = numbytes - bytesRead;
                    }
                    strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                    bytesRead += bytesLeftToRead;
                }

                free(currBuf);

                positionInBlock = 0;

                currIndirectBlock++;
            }
        }
        else
        { // direct blocks
            // READ WHAT IS IN FRONT STARTING AT POSITION IN BLOCK
            // unsigned char *currBuf = malloc((SOFTWARE_DISK_BLOCK_SIZE - positionInBlock + 1) * sizeof(char));
            // Software disk block size is 1024, so subtract the current position to get how many bytes you need to read total
            //  Ex: if positionInBlock is 0 then we want to read SOFTWARE_DISK_BLOCK_SIZE size data, not positionInBlock size data
            unsigned char *currBuf = read_data_from_disk((uint16_t)file->inode->blocks[currBlock], positionInBlock);
            
            // printf("READ FILE at block %d:  %s\n", currBlock, currBuf);


            // for every iteration other than the first iteration the position is 0
            if (currBuf == NULL)
            {
                fserror = FS_IO_ERROR;
                break;
            }
            uint64_t currLength = strlen((const char *) currBuf);
            int32_t bytesLeftToRead = numbytes - bytesRead;
            // going to read more than the length of the file
            if (bytesLeftToRead + (int32_t)file->filePosition > file->inode->size)
            {
                bytesLeftToRead = file->inode->size - file->filePosition;
            }

            if (currLength + bytesRead < numbytes)
            { // not done reading numbytes

                // reads the smaller amount of data
                if (SOFTWARE_DISK_BLOCK_SIZE - positionInBlock < bytesLeftToRead)
                {
                    bytesLeftToRead = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;
                    strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                    bytesRead += bytesLeftToRead;
                }
                else
                {
                    // WE ARE DONE READING RAN OUT OF SPACE
                    strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                    bytesRead += bytesLeftToRead;
                    break;
                }
            }
            else
            { // last read of numbytes

                // numbytes - bytesRead will never be greater than SOFTWARE_DISK_BLOCK_SIZE because of the previous if statement
                if ((uint64_t)bytesLeftToRead > numbytes - bytesRead)
                { // if the end of the file comes after how far I want to read
                    bytesLeftToRead = numbytes - bytesRead;
                }
                strncat((char *)tempBuf, (const char *) currBuf, bytesLeftToRead);
                bytesRead += bytesLeftToRead;
            }
            free(currBuf);
            positionInBlock = 0;
        }
        currBlock++;
    }
    file->filePosition += bytesRead;
    strcpy((char *)buf, (const char *)tempBuf);
    free(tempBuf);
    return bytesRead;
}

bool seek_file(File file, uint64_t bytepos)
{
    uint16_t blockIndex = bytepos / SOFTWARE_DISK_BLOCK_SIZE;

    if (blockIndex > MAX_DIRECT_BLOCK + MAX_INDIRECT_BLOCK || (uint64_t)file->inode->size < bytepos)
    {
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return false;
    }

    file->filePosition = bytepos;
    fserror = FS_NONE;
    return true;
}

bool delete_file(char *name)
{

    int16_t index = findDirEntry(name);
    if (index == -1)
    {
        fserror = FS_FILE_NOT_FOUND;
        // printf("cant find file with the name: %s\n", name);
        return false;
    }
    DirEntry *dirEntry;
    void *data = read_from_disk(DIRECTORY_ENTRY, (uint16_t)index);
    if (data == NULL)
    {
        fserror = FS_IO_ERROR;
        return false;
    }
    else
    {
        dirEntry = (DirEntry *)data;
    }
    if (dirEntry->isFileOpen)
    {
        fserror = FS_FILE_OPEN;
        return false;
    }
    clear_block((uint16_t)index);
    clear_bit((uint16_t)index); // set bitmap
    uint16_t inodeBlockNum = dirEntry->inodeBlockNum;
    Inode *inode;

    data = read_from_disk(INODE, inodeBlockNum);
    if (data == NULL)
    {
        fserror = FS_IO_ERROR;
        return false;
    }
    else
    {
        inode = (Inode *)data;
    }
    for (uint16_t i = 0; i < 14; i++)
    {
        if (inode->blocks[i])
        {
            clear_block(inode->blocks[i]);
            clear_bit(inode->blocks[i]); // set bitmap
        }
    }
    if (inode->blocks[NUM_DIRECT_INODE_BLOCKS-1])
    {
        uint16_t *indirectBlocks = (uint16_t *)read_from_disk(BLOCKS, inode->blocks[NUM_DIRECT_INODE_BLOCKS-1]);
        uint16_t i = 0;
        while (indirectBlocks[i] != (uint16_t)0)
        {
            clear_block(indirectBlocks[i]);
            clear_bit(indirectBlocks[i]); // set bitmap
            i++;
        }
        clear_block(inode->blocks[NUM_DIRECT_INODE_BLOCKS-1]);
        clear_bit(inode->blocks[NUM_DIRECT_INODE_BLOCKS-1]); // set bitmap
        free(indirectBlocks);
    }
    clear_block(inodeBlockNum);
    clear_bit(inodeBlockNum); // set bitmap

    // updates bitmap
    if (!write_sd_block(bitmap.map, 0))
    {
        fserror = FS_IO_ERROR;
        return false;
    }
    free(data);
    return true;
}

// starts writing at the current position and overwrites
uint64_t write_file(File file, void *buf, uint64_t numbytes) // I think we need to check bitmap and request space from the software disk and get that blocknum (pointer)
{
    // printf("WRITE\n");
    if (!file->directoryEntry->isFileOpen)
    {
        fserror = FS_FILE_NOT_OPEN;
        return 0;
    }
    if (file->fileMode != READ_WRITE)
    {
        fserror = FS_FILE_READ_ONLY;
        return 0;
    }

    uint64_t bytesWritten = 0;

    uint16_t blockIndex = file->filePosition / SOFTWARE_DISK_BLOCK_SIZE;
    uint16_t positionInBlock = file->filePosition % SOFTWARE_DISK_BLOCK_SIZE;
    uint16_t indirectBlockIndex = 0;
    if (blockIndex > NUM_DIRECT_INODE_BLOCKS)
    {
        indirectBlockIndex = blockIndex - NUM_DIRECT_INODE_BLOCKS;
        blockIndex = NUM_DIRECT_INODE_BLOCKS;
    }
    uint16_t currBlock = blockIndex;

    // 13 direct + 1 indirect block, so dont want to exceed. 0 is a not set block
    while (currBlock <= NUM_DIRECT_INODE_BLOCKS && indirectBlockIndex < MAX_INDIRECT_BLOCK && bytesWritten <= numbytes)
    {
        if (currBlock > NUM_DIRECT_INODE_BLOCKS) // at indirect block
        {
            uint16_t currIndirectBlock = indirectBlockIndex;
            if (currIndirectBlock >= MAX_INDIRECT_BLOCK)
            {
                fserror = FS_EXCEEDS_MAX_FILE_SIZE;
                break;
            }
            uint16_t *indirectBlocks = (uint16_t *)read_from_disk(BLOCKS, file->inode->blocks[NUM_DIRECT_INODE_BLOCKS]);

            while (currIndirectBlock < MAX_INDIRECT_BLOCK)
            {
                if (indirectBlocks[currIndirectBlock] == 0)
                {
                    if (!read_sd_block(bitmap.map, 0))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                    int16_t blockNum = findFreeDataSpace();
                    if (blockNum == -1)
                    {
                        fserror = FS_OUT_OF_SPACE;
                        break;
                    }
                    set_bit((uint16_t)blockNum);
                    if (!write_sd_block(bitmap.map, 0))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                    indirectBlocks[currIndirectBlock] = blockNum;
                    if (!write_to_disk(file->inode, INODE, file->directoryEntry->inodeBlockNum))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                }
                // unsigned char *currBuf = malloc(SOFTWARE_DISK_BLOCK_SIZE + 1);
                unsigned char *currBuf = read_data_from_disk(file->inode->blocks[currBlock], 0);
                if (currBuf == NULL)
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                int16_t bytesToWrite = numbytes - bytesWritten;
                int16_t remainingSpaceInBlock = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;

                // need to figure out how many we can write to the file

                // 2 options:
                // we can fit the entire numbytes left in this block
                if (bytesToWrite <= remainingSpaceInBlock)
                {
                    // fully writing then we're done
                    strncpy((char *)currBuf + positionInBlock, buf, bytesToWrite);
                    bytesWritten += bytesToWrite;
                    if (!write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                    positionInBlock = 0;
                    break;
                }
                else
                {
                    // cant fit all lof buf in remaining space
                    strncpy((char *)currBuf + positionInBlock, buf, remainingSpaceInBlock);
                    bytesWritten += remainingSpaceInBlock;
                    if (!write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                    positionInBlock = 0;
                }

                positionInBlock = 0;

                currIndirectBlock++;
            }
        }
        else
        { // direct blocks
            // check if we need to allocate this block
            if (file->inode->blocks[currBlock] == 0)
            {
                if (!read_sd_block(bitmap.map, 0))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                int16_t index = findFreeDataSpace();
                if (index == -1)
                {
                    fserror = FS_OUT_OF_SPACE;
                    break;
                }
                set_bit((uint16_t)index);
                if (!write_sd_block(bitmap.map, 0))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                file->inode->blocks[currBlock] = index;
                if (!write_to_disk(file->inode, INODE, file->directoryEntry->inodeBlockNum))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
            }
            // unsigned char *currBuf = malloc(SOFTWARE_DISK_BLOCK_SIZE + 1);
            unsigned char *currBuf = read_data_from_disk(file->inode->blocks[currBlock], 0);
            if (currBuf == NULL)
            {
                fserror = FS_IO_ERROR;
                break;
            }
            int16_t bytesToWrite = numbytes - bytesWritten;
            int16_t remainingSpaceInBlock = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;

            // need to figure out how many we can write to the file

            // 2 options:
            // we can fit the entire numbytes left in this block
            if (bytesToWrite <= remainingSpaceInBlock)
            {
                // fully writing then we're done

                strncpy((char *)currBuf + positionInBlock, buf, bytesToWrite);
                bytesWritten += bytesToWrite;
                if (!write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                positionInBlock = 0;
                break;
            }
            else
            {
                // cant fit all lof buf in remaining space
                strncpy((char *)currBuf + positionInBlock, buf, remainingSpaceInBlock);
                bytesWritten += remainingSpaceInBlock;
                if (!write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
            }
        }
        positionInBlock = 0;
        currBlock++;
    }
    file->filePosition = file->filePosition + bytesWritten;
    if ((int32_t)file->filePosition > file->inode->size)
    {
        file->inode->size = file->filePosition;
        if (!write_to_disk(file->inode, INODE, file->directoryEntry->inodeBlockNum))
        {
            fserror = FS_IO_ERROR;
        }
    }
    // printf("END WRITE\n");
    // printf("WRITE FILE name: %s", file->directoryEntry->name);
    return bytesWritten;
}

uint64_t file_length(File file)
{
    fserror = FS_NONE;
    // printf("FileName: %s\n", file->directoryEntry->name);
    return (uint64_t)file->inode->size;
}

bool file_exists(char *name)
{
    if (findDirEntry(name) != -1)
    {
        fserror = FS_NONE;
        return true;
    }
    else
    {
        fserror = FS_FILE_ALREADY_EXISTS;
        return false;
    }
}

void fs_print_error(void)
{
    switch (fserror)
    {
    case FS_NONE:
        fprintf(stderr, "No error.\n");
        break;
    case FS_OUT_OF_SPACE:
        fprintf(stderr, "Error: The operation caused the software disk to fill up.\n");
        break;
    case FS_FILE_NOT_OPEN:
        fprintf(stderr, "Error: Attempted read/write/close/etc. on a file that isn't open.\n");
        break;
    case FS_FILE_OPEN:
        fprintf(stderr, "Error: File is already open. Concurrent opens are not supported, and neither is deleting an open file.\n");
        break;
    case FS_FILE_NOT_FOUND:
        fprintf(stderr, "Error: Attempted open or delete of a file that doesn’t exist.\n");
        break;
    case FS_FILE_READ_ONLY:
        fprintf(stderr, "Error: Attempted write to a file opened for read-only access.\n");
        break;
    case FS_FILE_ALREADY_EXISTS:
        fprintf(stderr, "Error: Attempted creation of a file with an existing name.\n");
        break;
    case FS_EXCEEDS_MAX_FILE_SIZE:
        fprintf(stderr, "Error: Seek or write would exceed the maximum file size.\n");
        break;
    case FS_ILLEGAL_FILENAME:
        fprintf(stderr, "Error: The filename begins with a null character.\n");
        break;
    case FS_IO_ERROR:
        fprintf(stderr, "Error: An I/O error occurred. Something really bad happened.\n");
        break;
    default:
        fprintf(stderr, "Error: Unknown error code.\n");
        break;
    }
}

bool check_structure_alignment(void)
{
    printf("Expecting sizeof(Inode) = 32, actual = %lu\n", sizeof(Inode));
    printf("Expecting sizeof(IndirectBlock) = %d, actual = %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(IndirectBlock));
    printf("Expecting sizeof(InodeBlock) = %d, actual = %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(InodeBlock));
    printf("Expecting sizeof(DirEntry) = %d, actual = %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(DirEntry));
    printf("Expecting sizeof(FreeBitmap) = %d, actual = %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(FreeBitmap));

    if (sizeof(Inode) != 32 ||
        sizeof(IndirectBlock) != SOFTWARE_DISK_BLOCK_SIZE ||
        sizeof(InodeBlock) != SOFTWARE_DISK_BLOCK_SIZE ||
        sizeof(DirEntry) != SOFTWARE_DISK_BLOCK_SIZE ||
        sizeof(FreeBitmap) != SOFTWARE_DISK_BLOCK_SIZE)
    {
        return false;
    }
    else
    {
        return true;
    }
}
