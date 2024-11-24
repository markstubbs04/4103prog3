//
// Software disk implementation for LSU 4103 filesystem assignment.
// (@nolaforensix).
//

#include "softwaredisk.h"

#define NUM_BLOCKS 8192
#define BACKING_STORE "sdprivate.sd"

// internals of software disk implementation
typedef struct SoftwareDiskInternals {
  FILE *fp;       
} SoftwareDiskInternals;

//
// GLOBALS
//

static SoftwareDiskInternals sd;

// software disk error code set (set by each software disk function).
SDError sderror;


// initializes the software disk to all zeros, destroying any existing
// data.  Returns true on success, otherwise false. Always sets global
// 'sderror'.
bool init_software_disk(void) {
  
  uint16_t i;
  char block[SOFTWARE_DISK_BLOCK_SIZE];
  sderror = SD_NONE;
  unlink(BACKING_STORE);
  sd.fp = fopen(BACKING_STORE, "w+");
  if (! sd.fp) {
    sderror = SD_INTERNAL_ERROR;
    return false;
  }
  
  bzero(block, SOFTWARE_DISK_BLOCK_SIZE);
  for (i = 0; i < NUM_BLOCKS; i++) {
    if (fwrite(block, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
      fclose(sd.fp);
      sd.fp = NULL;
      sderror = SD_INTERNAL_ERROR;
      return false;
    }
  }
  return true;
}

// returns the size of the SoftwareDisk in multiples of
// SOFTWARE_DISK_BLOCK_SIZE
uint16_t software_disk_size() {

  return NUM_BLOCKS;
}

// writes a block of data from 'buf' at location 'blocknum'.  Blocks
// are numbered from 0.  The buffer 'buf' must be of size
// SOFTWARE_DISK_BLOCK_SIZE.  Returns true on success or false on
// failure.  Always sets global 'sderror'.
bool write_sd_block(void *buf, uint16_t blocknum) {

  sderror = SD_NONE;
  if (! sd.fp) {
    sd.fp = fopen(BACKING_STORE, "r+");
    if (! sd.fp) {             
      sderror = SD_INTERNAL_ERROR;
      return false;
    }
    else {
      fseek(sd.fp, 0L, SEEK_END);
      if (ftell(sd.fp) != NUM_BLOCKS * SOFTWARE_DISK_BLOCK_SIZE) {
	fclose(sd.fp);
	sd.fp = NULL;
	sderror=SD_NOT_INIT;
	return false;
      }
    }
  }

  if (blocknum > NUM_BLOCKS-1) {
    sderror=SD_ILLEGAL_BLOCK_NUMBER;
    return false;
  }

  fseek(sd.fp, blocknum * SOFTWARE_DISK_BLOCK_SIZE, SEEK_SET);
  if (fwrite(buf, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
    sderror = SD_INTERNAL_ERROR;
    return false;
  }
  
  fflush(sd.fp);
  return true;
}

// reads a block of data into 'buf' from location 'blocknum'.  Blocks
// are numbered from 0.  The buffer 'buf' must be of size
// SOFTWARE_DISK_BLOCK_SIZE.  Returns true on success or false on failure.
// Always sets global 'sderror'.
bool read_sd_block(void *buf, uint16_t blocknum) {

  sderror = SD_NONE;
  if (! sd.fp) {
    sd.fp = fopen(BACKING_STORE, "r+");
    if (! sd.fp) {             
      sderror = SD_INTERNAL_ERROR;
      return false;
    }
    else {
      fseek(sd.fp, 0L, SEEK_END);
      if (ftell(sd.fp) != NUM_BLOCKS * SOFTWARE_DISK_BLOCK_SIZE) {
	fclose(sd.fp);
	sd.fp = NULL;
	sderror = SD_NOT_INIT;
	return false;
      }
    }
  }

  if (blocknum > NUM_BLOCKS - 1) {
    sderror = SD_ILLEGAL_BLOCK_NUMBER;
    return false;
  }

  fseek(sd.fp, blocknum * SOFTWARE_DISK_BLOCK_SIZE, SEEK_SET);
  if (fread(buf, SOFTWARE_DISK_BLOCK_SIZE, 1, sd.fp) != 1) {
    sderror = SD_INTERNAL_ERROR;
    return false;
  }
  
  fflush(sd.fp);
  return true;
}

// describe current software disk error code by printing a descriptive
// message to standard error.
void sd_print_error(void) {
  switch (sderror) {
  case SD_NONE:
    printf("SD: No error.\n");
    break;
  case SD_NOT_INIT:
    printf("SD: Software disk not initialized.\n");
    break;
  case SD_ILLEGAL_BLOCK_NUMBER:
    printf("SD: Illegal block number.\n");
    break;
  case SD_INTERNAL_ERROR:
    printf("SD: Internal error, software disk unusable.\n");
    break;
  default:
    printf("SD: Unknown error code %d.\n", sderror);
  }
}

// software disk  error code set (set by each software disk function).
SDError sderror;
