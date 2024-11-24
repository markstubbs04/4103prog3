//
// Software disk implementation for LSU 4103 filesystem assignment.
// (@nolaforensix).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#if ! defined(SOFTWARE_DISK_BLOCK_SIZE)
#define SOFTWARE_DISK_BLOCK_SIZE 1024

// software disk error codes
typedef enum  {
  SD_NONE,
  SD_NOT_INIT,               // software disk not initialized
  SD_ILLEGAL_BLOCK_NUMBER,   // specified block number exceeds size of software disk
  SD_INTERNAL_ERROR          // the software disk has failed
} SDError;

// function prototypes for software disk API

// initializes the software disk to all zeros, destroying any existing
// data.  Returns true on success, otherwise false. Always sets global
// 'sderror'.
bool init_software_disk();

// returns the size of the SoftwareDisk in multiples of
// SOFTWARE_DISK_BLOCK_SIZE
uint16_t software_disk_size();

// writes a block of data from 'buf' at location 'blocknum'.  Blocks
// are numbered from 0.  The buffer 'buf' must be of size
// SOFTWARE_DISK_BLOCK_SIZE.  Returns true on success or false on failure.
// Always sets global 'sderror'.
bool write_sd_block(void *buf, uint16_t blocknum);

// reads a block of data into 'buf' from location 'blocknum'.  Blocks
// are numbered from 0.  The buffer 'buf' must be of size
// SOFTWARE_DISK_BLOCK_SIZE.  Returns true on success or false on
// failure.  Always sets global 'sderror'.
bool read_sd_block(void *buf, uint16_t blocknum);

// describe current software disk error code by printing a descriptive
// message to standard error
void sd_print_error(void);

// software disk  error code set (set by each software disk function).
extern SDError sderror;
#endif
