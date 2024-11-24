//
// Simple filesystem API for LSU 4103 filesystem assignment.
// (@nolaforensix).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>

#if ! defined(__FILESYSTEM_4103_H__)
#define __FILESYSTEM_4103_H__

// private
struct FileInternals;

// file type used by user code
typedef struct FileInternals* File; 

// access mode for open_file() 
typedef enum {
  READ_ONLY, READ_WRITE
} FileMode;

// error codes set in global 'fserror' by filesystem functions
typedef enum  {
  FS_NONE, 
  FS_OUT_OF_SPACE,         // the operation caused the software disk to fill up
  FS_FILE_NOT_OPEN,  	   // attempted read/write/close/etc. on file that isn't open
  FS_FILE_OPEN,      	   // file is already open. Concurrent opens are not
                           // supported and neither is deleting a file that is open.
  FS_FILE_NOT_FOUND, 	   // attempted open or delete of file that doesn’t exist
  FS_FILE_READ_ONLY, 	   // attempted write to file opened for READ_ONLY
  FS_FILE_ALREADY_EXISTS,  // attempted creation of file with existing name
  FS_EXCEEDS_MAX_FILE_SIZE,// seek or write would exceed max file size
  FS_ILLEGAL_FILENAME,     // filename begins with a null character
  FS_IO_ERROR              // something really bad happened
} FSError;

// function prototypes for filesystem API

// open existing file with pathname 'name' and access mode 'mode'.
// Current file position is set to byte 0.  Returns NULL on
// error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode);

// create and open new file with pathname 'name' and (implied) access
// mode READ_WRITE.  Current file position is set to byte 0.  Returns
// NULL on error. Always sets 'fserror' global.
File create_file(char *name);

// close 'file'.  Always sets 'fserror' global.
void close_file(File file);

// read at most 'numbytes' of data from 'file' into 'buf', starting at
// the current file position.  Returns the number of bytes read. If
// end of file is reached, then a return value less than 'numbytes'
// signals this condition. Always sets 'fserror' global.
uint64_t read_file(File file, void *buf, uint64_t numbytes);

// write 'numbytes' of data from 'buf' into 'file' at the current file
// position.  Returns the number of bytes written. On an out of space
// error, the return value may be less than 'numbytes'.  Always sets
// 'fserror' global.
uint64_t write_file(File file, void *buf, uint64_t numbytes);

// sets current position in file to 'bytepos', always relative to the
// beginning of file.  Seeks past the current end of file should
// extend the file. Returns true on success and false on failure.
// Always sets 'fserror' global.
bool seek_file(File file, uint64_t bytepos);

// returns the current length of the file in bytes. Always sets
// 'fserror' global.
uint64_t file_length(File file);

// deletes the file named 'name', if it exists. Returns true on
// success, false on failure.  Always sets 'fserror' global.
bool delete_file(char *name); 

// determines if a file with 'name' exists and returns true if it
// exists, otherwise false.  Always sets 'fserror' global.
bool file_exists(char *name);

// describe current filesystem error code by printing a descriptive
// message to standard error.
void fs_print_error(void);

// extra function to make sure structure alignment, data structure
// sizes, etc. on target platform are correct.  Returns true on
// success, false on failure.
bool check_structure_alignment(void);

// filesystem error code set (set by each filesystem function)
extern FSError fserror;

#endif
