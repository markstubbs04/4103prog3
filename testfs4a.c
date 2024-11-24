//
// RUN formatfs before conducting this test then run testfs4b *after*
// this test.
//

#include "filesystem.h"
#include "softwaredisk.h"
  
int main(int argc, char *argv[]) {
  
  uint64_t ret, i;
  File f;
  char buf[SOFTWARE_DISK_BLOCK_SIZE];

  // tests block alignment, along with testfs4b
  
  f = create_file("line-em-up");
  printf("ret from create_file(\"line-em-up\") = %p\n",
	 f);
  fs_print_error();

  if (f) {
    for (i = 0; i < 50; i++) {
      memset(buf, 'A'+i, SOFTWARE_DISK_BLOCK_SIZE);
      ret = write_file(f, buf, SOFTWARE_DISK_BLOCK_SIZE);
      printf("ret from write_file(f, buf, SOFTWARE_DISK_BLOCK_SIZE) = %" PRIu64 "\n",
	     ret);
      fs_print_error();
    }
    
    close_file(f);
    printf("Executed close_file(f).\n");
    fs_print_error();
  }
  else {
    printf("FAIL.  Did you run formatfs before this test?\n");
  }
  
  return 0;
}
