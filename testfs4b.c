//
// This is NOT a standalone test.  RUN testfs4a before this test, but
// do NOT execute formatfs again before conducting this test!
//

#include "filesystem.h"
#include "softwaredisk.h"
  
int main(int argc, char *argv[]) {
  
  uint64_t ret, i;
  int match;
  File f;
  char buf[SOFTWARE_DISK_BLOCK_SIZE * 2], buf2[SOFTWARE_DISK_BLOCK_SIZE * 2];

  // tests block alignment, along with testfs4a
  
  f = open_file("line-em-up", READ_WRITE);
  printf("ret from open_file(\"line-em-up\", READ_WRITE) = %p\n",
	 f);
  fs_print_error();

  if (f) {
    for (i = 0; i < 50; i += 2) {
      memset(buf, 'A' + i, SOFTWARE_DISK_BLOCK_SIZE);
      memset(buf + SOFTWARE_DISK_BLOCK_SIZE, 'A' + i + 1,
	     SOFTWARE_DISK_BLOCK_SIZE);
      ret = read_file(f, buf2, SOFTWARE_DISK_BLOCK_SIZE * 2);
      printf("ret from read_file(f, buf2, SOFTWARE_DISK_BLOCK_SIZE * 2) = %" PRIu64 "\n",
	     ret);
      fs_print_error();
      match = ! memcmp(buf, buf2, 1024);
      printf("Expected buffer value %s.\n",
	     match ? "matches" : "doesn't  match");
      
      if (! match) {
	printf("FAIL.\n");
	return 0;
      }
    }
    
    close_file(f);
    printf("Executed close_file(f).\n");
    fs_print_error();
  }
  else {
    printf("FAIL. Did you run testfs4a before this test?\n");
  }

  return 0;
}
