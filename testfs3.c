//
// This is a standalone test.  RUN formatfs before conducting this
// test!  This test attempts to determine how large a single file can
// be in your filesystem.  Read the code below to understand how this
// works.
//

#include "filesystem.h"
  
int main(int argc, char *argv[]) {
  
  uint64_t i, ret, maxlen;
  File f;
  char c, *buf, *buf2, cbuf[1024];

  // see how large a single file can be
  f = create_file("superfile");
  fs_print_error();

  if (! f) {
    goto fail;
  }

  printf("Please wait, finding ~max file size.  This may take a while.\n");

  c = 'A';
  for (i = 0; i < 1024; i++) {
    cbuf[i] = c;
    c++;
    if (c > 'Z') {
      c = 'A';
    }
  }
    
  while (1) {
    ret = write_file(f, cbuf, 1024);
    maxlen = file_length(f);
    
    if (! ret) {
      fs_print_error();
      printf("Maximum file size appears to be about %" PRIu64 " bytes.\n", maxlen);
      break;
    }
  }

  close_file(f);
  printf("Executed close_file(f).\n");
  fs_print_error();

  delete_file("superfile");
  printf("Executed delete_file(\"superfile\").\n");
  fs_print_error();

  // now create one file of maximum length, with a single write
  f = create_file("superfile-max");
  fs_print_error();
  if (! f) {
    goto fail;
  }
  
  buf = malloc(maxlen+1);
  buf2 = malloc(maxlen+1);
  c = 'A';
  for (i = 0; i < maxlen; i++) {
    buf[i] = c;
    c++;
    if (c > 'Z') {
      c = 'A';
    }
  }

  ret = write_file(f, buf, maxlen);
  printf("ret from write_file(f, buf, %" PRIu64 ") = %" PRIu64 "\n",
	 maxlen, ret);
    fs_print_error();

  printf("Seeking to beginning of file.\n");
  seek_file(f, 0);
  fs_print_error();

  ret = read_file(f, buf2, maxlen);
  printf("ret from read_file(f, buf2, %" PRIu64 ") = %" PRIu64 "\n",
	 maxlen, ret);
	 fs_print_error();
  
  close_file(f);
  printf("Executed close_file(f).\n");
  fs_print_error();

  printf("Read / write buffers for large file %s.\n",
	 ! memcmp(buf, buf2, maxlen) ? "match" : "don't match");

  return 0;

 fail:
  printf("FAIL. Was formatfs run before this test?.\n");
  return 0;
}
