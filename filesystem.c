#include <stdint.h>
#include "softwaredisk.c"
#include "filesystem.h"
#include <string.h>

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
525 blocks * 256 files =
= 134,400 blocks

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
#define MAX_FILE_SIZE 256

// FSError Error;

typedef enum DataType
{
    INODE,
    DIRECTORY_ENTRY,
    BLOCKS,
} DataType;

struct FileInternals
{
    uint32_t filePosition;    // 4 bytes
    _FileMode fileMode;       // 4 byte
    DirEntry *directoryEntry; // 300 bytes
    Inode *inode;
};
// file type used by user code
typedef struct FileInternals *File;

typedef struct DirEntry
{                           // 1024 total
    char *name;             // min 257 actually 1021
    bool isFileOpen;        // 1 byte
    uint16_t inodeBlockNum; // 2 bytes
} DirEntry;

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

typedef struct IndirectBlock
{
    uint16_t blocks[NUM_INDIRECT_INODE_BLOCKS];
} IndirectBlock;

typedef uint32_t _FileMode; // 4 byte

typedef struct FreeBitmap
{
    // bitmap is the size of all structures' blocks
    unsigned char map[SOFTWARE_DISK_BLOCK_SIZE]; // array that you can access each bit to check for file system availability
} FreeBitmap;

FreeBitmap bitmap;

// InodeBlock inodeBlockArray[INODE_ARRAY_BLOCK_SIZE];

struct FileInternals files[MAX_NUMBER_OF_FILES]; // array of all the fileinternal pointers

// MAIN FUNCTIONS:

File create_file(char *name) // might need to add a null terminating character during file creation
{
    if (name[0] == '\0' || strlen(name) > 257)
    {
        fserror = FS_ILLEGAL_FILENAME;
        return NULL;
    }

    int32_t index = -1; // tracks the index

    char buf[SOFTWARE_DISK_BLOCK_SIZE];
    read_sd_block(bitmap.map, 0);
    int index = -1;
    bool found = true;
    // reading dir entries to check for duplicate names
    for (uint16_t i = DIR_ENTRY_FIRST_BLOCKNUM; i < DIR_ENTRY_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; i++)
    {
        if (is_bit_set(bitmap.map, i))
        {
            read_sd_block(buf, i);
            int len = strlen(name);
            if (!strcmp(buf[0], name))
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
        return NULL;
    }

    File file;

    Inode newInode;
    newInode.size = 0;
    bzero(newInode.blocks);
    file->inode = &newInode;

    // find the first data block that is available
    int16_t blocknum = findFreeSpace();
    if (blocknum == -1)
    {
        fserror = FS_OUT_OF_SPACE;
        return NULL;
    }
    file->inode->blocks[0] = (uint16_t)blocknum;
    clear_block(blocknum);
    set_bit((uint16_t)blocknum); // setting bitmap for when we allocate a block to newly opened file data

    // find a free space for the inode and dir entry
    int inodeBlockNum = -1;
    for (uint16_t i = inodeBlockNum; i < INODE_FIRST_BLOCKNUM + INODE_BLOCK_SIZE; i++)
    {
        if (!is_bit_set(bitmap.map, i))
        {
            inodeBlockNum = i;
            break;
        }
    }
    if (inodeBlockNum == -1)
    {
        fserror = FS_OUT_OF_SPACE;
        return NULL;
    }
    set_bit(inodeBlockNum); // set the new Inode in bitmapp

    // write the inode and dir entry
    if (write_to_disk(&newInode, INODE, inodeBlockNum))
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }

    DirEntry newDirEntry;
    newDirEntry.inodeBlockNum = inodeBlockNum;
    newDirEntry.name = getFullFileName(name, strlen(name));
    file->directoryEntry = &newDirEntry;

    int dirEntryBlockNum = inodeBlockNum + MAX_FILE_SIZE;
    if (write_to_disk(&newDirEntry, DIRECTORY_ENTRY, dirEntryBlockNum))
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }
    set_bit(dirEntryBlockNum); // marks the directory entry as taken

    file->filePosition = (uint32_t)0;

    // updates bitmap
    if (!write_sd_block(bitmap.map, 0))
    {
        fserror = FS_IO_ERROR;
        return false;
    }

    fserror = FS_NONE;
    return open_file(name, READ_WRITE);
}

File open_file(char *name, FileMode mode)
{
    File file;
    uint32_t index = findDirEntry(name);
    if (index == -1)
    {
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }

    file->filePosition = 0;
    file->fileMode = mode;

    DirEntry *dirEntry;
    void *data = read_from_disk(DIRECTORY_ENTRY, index);
    if (data == NULL)
    {
        fserror = FS_IO_ERROR;
        return NULL;
    }
    else
    {
        dirEntry = (DirEntry *)data;
    }
    file->directoryEntry = dirEntry;

    Inode *inode;
    void *data = read_from_disk(INODE, index);
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

    fserror = FS_NONE;
    return file;
}

void close_file(File file)
{
    if (file == NULL)
    {
        fserror = FS_FILE_NOT_FOUND;
        return;
    }
    fserror = FS_NONE;
    free(file);
}

uint64_t read_file(File file, void *buf, uint64_t numbytes)
{
    char tempBuf = malloc(numbytes * sizeof(char));
    uint64_t bytesWritten = 0;

    int blockIndex = file->filePosition / SOFTWARE_DISK_BLOCK_SIZE;
    int positionInBlock = file->filePosition % SOFTWARE_DISK_BLOCK_SIZE;
    int indirectBlockIndex = 0;
    if (blockIndex > 14)
    {
        indirectBlockIndex = blockIndex - 14;
        blockIndex = 14;
    }
    int currBlock = blockIndex;

    // 13 direct + 1 indirect block, so dont want to exceed. 0 is a not set block
    while (currBlock < 15 && indirectBlockIndex < MAX_INDIRECT_BLOCK && file->inode->blocks[currBlock] != (uint16_t)0)
    {
        if (currBlock > 13) // at indirect block
        {
            int currIndirectBlock = indirectBlockIndex;
            uint16_t *indirectBlocks = (uint16_t *)read_from_disk(file->inode->blocks[14], BLOCKS);

            while (currIndirectBlock < MAX_INDIRECT_BLOCK && indirectBlocks[currIndirectBlock] != (uint16_t)0)
            {
                // READ WHAT IS IN FRONT STARTING AT POSITION IN BLOCK
                unsigned char *currBuf = malloc((SOFTWARE_DISK_BLOCK_SIZE - positionInBlock + 1) * sizeof(char));
                // Software disk block size is 1024, so subtract the current position to get how many bytes you need to read total
                //  Ex: if positionInBlock is 0 then we want to read SOFTWARE_DISK_BLOCK_SIZE size data, not positionInBlock size data
                currBuf = read_data_from_disk(currIndirectBlock, positionInBlock);
                // for every iteration other than the first iteration the position is 0
                if (currBuf == NULL)
                {
                    fserror = FS_IO_ERROR;
                    return NULL;
                }
                uint64_t currLength = strlen(currBuf);
                if (currLength + bytesWritten <= numbytes)
                { // not last write
                    strcat_s(tempBuf, SOFTWARE_DISK_BLOCK_SIZE - positionInBlock, currBuf);
                    bytesWritten = +SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;
                }
                else
                { // last write
                    strcat_s(tempBuf, numbytes - bytesWritten, currBuf);
                    bytesWritten = numbytes;
                }

                positionInBlock = 0;

                currIndirectBlock++;
            }
        }
        else
        { // direct blocks
            unsigned char *currBuf = malloc((SOFTWARE_DISK_BLOCK_SIZE - positionInBlock + 1) * sizeof(char));
            currBuf = read_data_from_disk(currBlock, positionInBlock);
            if (currBuf == NULL)
            {
                fserror = FS_IO_ERROR;
                return NULL;
            }
            uint64_t currLength = strlen(currBuf);
            if (currLength + bytesWritten <= numbytes)
            { // not last write
                strcat_s(tempBuf, SOFTWARE_DISK_BLOCK_SIZE - positionInBlock, currBuf);
                bytesWritten = +SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;
            }
            else
            { // last write
                strcat_s(tempBuf, numbytes - bytesWritten, currBuf);
                bytesWritten = numbytes;
            }

            positionInBlock = 0;
        }
        currBlock++;
    }
    fserror = FS_NONE;
    return bytesWritten;
}

bool seek_file(File file, uint64_t bytepos)
{
    int blockIndex = bytepos / SOFTWARE_DISK_BLOCK_SIZE;
    int positionInBlock = bytepos % SOFTWARE_DISK_BLOCK_SIZE;

    if (blockIndex > MAX_DIRECT_BLOCK + MAX_INDIRECT_BLOCK || file->inode->size < bytepos)
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
    clear_block((uint16_t)index);
    clear_bit(index); // set bitmap
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
    for (int i = 0; i < 14; i++)
    {
        if (inode->blocks[i])
        {
            clear_block(inode->blocks[i]);
            clear_bit(inode->blocks[i]); // set bitmap
        }
    }
    if (inode->blocks[14])
    {
        uint16_t *indirectBlocks = (uint16_t *)read_from_disk(inode->blocks[14], BLOCKS);
        int i = 0;
        while (indirectBlocks[i] != (uint16_t)0)
        {
            clear_block(indirectBlocks[i]);
            clear_bit(indirectBlocks[i]); // set bitmap
            i++;
        }
        clear_block(inode->blocks[14]);
        clear_bit(inode->blocks[14]); // set bitmap
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
    // need to use the filePosition in the file and find the remainder / 1024 to get the index in the blocks to write to
    // char *buffer = (char *)buf;
    // int currBuffSize = 0;
    // int positionInBuffer = 0;
    // int bytesWritten = 0;

    // int blockIndex = file->filePosition / SOFTWARE_DISK_BLOCK_SIZE;
    // int positionInBlock = file->filePosition % SOFTWARE_DISK_BLOCK_SIZE;

    // char tempBuffer[SOFTWARE_DISK_BLOCK_SIZE];

    // if (blockIndex < 14)
    // {
    //     read_sd_block(tempBuffer, file->inode->blocks[blockIndex]); // read what is currently in the block
    // }
    // else
    // {
    //     // move to indiret block
    // }

    // // need to make sure buf + positionInBlock < 1024

    // while (positionInBlock < 1024 && numbytes > 0)
    // {
    //     if (positionInBlock + numbytes > 1024)
    //     {
    //         currBuffSize = 1024 - positionInBlock;
    //         strncat(tempBuffer, buf + positionInBuffer, currBuffSize);
    //         positionInBuffer += currBuffSize;
    //         numbytes -= currBuffSize;
    //         bytesWritten += currBuffSize;
    //         file->filePosition += currBuffSize;

    //         if (!write_sd_block(tempBuffer, file->inode->blocks[blockIndex]))
    //         {
    //             fserror = FS_IO_ERROR;
    //             return bytesWritten; // make sure that this is correct might cause problems
    //         }

    //         // go to next block
    //         blockIndex++;
    //         positionInBlock = 0;

    //         if (blockIndex == 14)
    //         {
    //             // move to indiret block
    //         }

    //         read_sd_block(tempBuffer, file->inode->blocks[blockIndex++]);
    //     }
    //     else
    //     {
    //         file->filePosition += numbytes;
    //         bytesWritten += numbytes;
    //         strcat(tempBuffer, buf);
    //         if (!write_sd_block(tempBuffer, file->inode->blocks[blockIndex]))
    //         {
    //             fserror = FS_IO_ERROR;
    //             return bytesWritten; // make sure that this is correct might cause problems
    //         }
    //         return bytesWritten;
    //     }
    // }
    // fserror = FS_NONE;
    // return bytesWritten;

    // char tempBuf = malloc(numbytes * sizeof(char));
    uint64_t bytesWritten = 0;
    // uint64_t bytesToWrite = numbytes;

    int blockIndex = file->filePosition / SOFTWARE_DISK_BLOCK_SIZE;
    int positionInBlock = file->filePosition % SOFTWARE_DISK_BLOCK_SIZE;
    int indirectBlockIndex = 0;
    if (blockIndex > 14)
    {
        indirectBlockIndex = blockIndex - 14;
        blockIndex = 14;
    }
    int currBlock = blockIndex;

    // 13 direct + 1 indirect block, so dont want to exceed. 0 is a not set block
    while (currBlock < 15 && indirectBlockIndex < MAX_INDIRECT_BLOCK && bytesWritten <= numbytes)
    {
        if (currBlock > 13) // at indirect block
        {
            int currIndirectBlock = indirectBlockIndex;
            if (currIndirectBlock >= MAX_INDIRECT_BLOCK)
            {
                fserror = FS_EXCEEDS_MAX_FILE_SIZE;
                break;
            }
            uint16_t *indirectBlocks = (uint16_t *)read_from_disk(file->inode->blocks[14], BLOCKS);

            while (currIndirectBlock < MAX_INDIRECT_BLOCK)
            {
                if (indirectBlocks[currIndirectBlock] == NULL)
                {
                    if (!read_sd_block(bitmap.map, 0))
                    {
                        fserror = FS_IO_ERROR;
                        break;
                    }
                    int16_t blockNum = findFreeSpace();
                    if (blockNum == -1)
                    {
                        fserror = FS_OUT_OF_SPACE;
                        break;
                    }
                    set_bit(blockNum);
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
                unsigned char *currBuf = malloc(SOFTWARE_DISK_BLOCK_SIZE);
                currBuf = read_data_from_disk(file->inode->blocks[currBlock], 0);
                if (currBuf == NULL)
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                int bytesToWrite = numbytes - bytesWritten;
                int remainingSpaceInBlock = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;

                // need to figure out how many we can write to the file

                // 2 options:
                // we can fit the entire numbytes left in this block
                if (bytesToWrite <= remainingSpaceInBlock)
                {
                    // fully writing then we're done
                    strncpy(currBuf + positionInBlock, buf, bytesToWrite);
                    bytesWritten += bytesToWrite;
                    write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]);
                    positionInBlock = 0;
                    break;
                }
                else
                {
                    // cant fit all lof buf in remaining space
                    strncpy(currBuf + positionInBlock, buf, remainingSpaceInBlock);
                    bytesWritten += remainingSpaceInBlock;
                    write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]);
                    positionInBlock = 0;
                }

                positionInBlock = 0;

                currIndirectBlock++;
            }
        }
        else
        { // direct blocks
            // check if we need to allocate this block
            if (file->inode->blocks[currBlock] == NULL)
            {
                if (!read_sd_block(bitmap.map, 0))
                {
                    fserror = FS_IO_ERROR;
                    break;
                }
                int16_t index = findFreeSpace();
                if (index == -1)
                {
                    fserror = FS_OUT_OF_SPACE;
                    break;
                }
                set_bit(index);
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
            unsigned char *currBuf = malloc(SOFTWARE_DISK_BLOCK_SIZE);
            currBuf = read_data_from_disk(file->inode->blocks[currBlock], 0);
            if (currBuf == NULL)
            {
                fserror = FS_IO_ERROR;
                break;
            }
            int bytesToWrite = numbytes - bytesWritten;
            int remainingSpaceInBlock = SOFTWARE_DISK_BLOCK_SIZE - positionInBlock;

            // need to figure out how many we can write to the file

            // 2 options:
            // we can fit the entire numbytes left in this block
            if (bytesToWrite <= remainingSpaceInBlock)
            {
                // fully writing then we're done

                strncpy(currBuf + positionInBlock, buf, bytesToWrite);
                bytesWritten += bytesToWrite;
                write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]);
                positionInBlock = 0;
                break;
            }
            else
            {
                // cant fit all lof buf in remaining space
                strncpy(currBuf + positionInBlock, buf, remainingSpaceInBlock);
                bytesWritten += remainingSpaceInBlock;
                write_to_disk(currBuf, BLOCKS, file->inode->blocks[currBlock]);
            }
        }
        positionInBlock = 0;
        currBlock++;
    }
    file->filePosition = file->filePosition + bytesWritten;
    return bytesWritten;
}

uint64_t file_length(File file)
{
    fserror = FS_NONE;
    return (uint64_t)file->inode->size;
}

bool file_exists(char *name)
{
    if (findDirEntry(name) > -1)
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

// HELPER FUNCTIONS:

int16_t findDirEntry(char *name)
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE];
    read_sd_block(bitmap.map, 0);
    for (uint16_t i = DIR_ENTRY_FIRST_BLOCKNUM; i < DIR_ENTRY_FIRST_BLOCKNUM + MAX_NUMBER_OF_FILES; i++)
    {
        if (is_bit_set(bitmap.map, i))
        {
            if (!read_sd_block(buf, i))
            {
                fserror = FS_IO_ERROR;
                return NULL;
            }
            int len = strlen(name);
            char nameBuf[1024];
            memcpy(nameBuf, buf, 1022); // get the entire file name
            if (!strcmp(nameBuf, name))
            {
                fserror = FS_NONE;
                return i + DIR_ENTRY_FIRST_BLOCKNUM;
            }
        }
    }
    fserror = FS_FILE_ALREADY_EXISTS;
    return -1;
}

unsigned char *read_data_from_disk(uint16_t blocknum, uint32_t position)
{
    if (position >= SOFTWARE_DISK_BLOCK_SIZE || position < 0)
    {
        return NULL;
    }
    char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    if (!read_sd_block(buf, blocknum))
    {
        free(buf);
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }
    char *data = malloc(sizeof(char) * ((SOFTWARE_DISK_BLOCK_SIZE - position) + 1));
    if (!data)
    {
        free(buf);
        return NULL;
    }
    memcpy(data, buf + position, sizeof(char) * (SOFTWARE_DISK_BLOCK_SIZE - position));
    data[((SOFTWARE_DISK_BLOCK_SIZE - position) + 1)] = '\0';
    free(buf);
    fserror = FS_NONE;
    return data;
}

// reads type DataType at blocknum with optionl position in file
void *read_from_disk(DataType type, uint16_t blocknum)
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    if (!read_sd_block(buf, blocknum))
    {
        free(buf);
        fserror = FS_FILE_NOT_FOUND;
        return NULL;
    }

    size_t size = get_data_size(type);
    if (size == 0)
    { // Unsupported type
        free(buf);
        return NULL;
    }

    void *object = malloc(size);
    if (!object)
    {
        free(buf);
        return NULL;
    }

    memcpy(object, buf, size);
    free(buf);
    fserror = FS_NONE;
    return object;
}

bool clear_block(uint16_t blocknum)
{
    char clear[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    fserror = FS_NONE;
    return write_to_disk(&clear, BLOCKS, blocknum);
}

bool write_to_disk(void *data, DataType type, uint16_t blocknum)
{
    char buf[SOFTWARE_DISK_BLOCK_SIZE] = {'\0'};

    size_t size = get_data_size(type);
    if (size == 0)
    { // Unsupported type
        free(buf);
        return false;
    }

    memcpy(buf, data, size);

    bool result = write_sd_block(buf, blocknum);
    free(buf);
    fserror = FS_NONE;
    return result;
}

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

char *getFullFileName(char *name, int length)
{

    // Allocate exactly 1022 bytes
    char *padded_name = (char *)malloc(1022);
    if (!padded_name)
    {
        return NULL;
    }

    strncpy(padded_name, name, length);

    // Fill the remaining bytes with '\0'
    memset(padded_name + length, '\0', 1022 - length);

    return padded_name;
}

// bitmap helper functions

// finds free space in the bitmap
int16_t findFreeSpace()
{
    for (uint16_t j = FILE_DATA_FIRST_BLOCKNUM; j < FILE_SYSTEM_SIZE; j++)
    {
        if (is_bit_set(bitmap.map, j))
        {
            return j;
        }
    }
    return -1;
}

// set jth bit in a bitmap composed of 8-bit integers
void set_bit(uint64_t j)
{
    bitmap.map[j / 8] != (1 << (j % 8));
}

// clear jth bit in a bitmap composed of 8-but integers
void clear_bit(uint64_t j)
{
    bitmap.map[j / 8] &= ~(1 << (j % 8));
}

// returns true if jth bit is set in a bitap of 8-bit integers,
// otherwise false
bool is_bit_set(unsigned char *bitmap, uint64_t j)
{
    return bitmap[j / 8] & (1 << (j % 8));
}

int main()
{
    formatfs();

    uint16_t numBlocks = software_disk_size();
};