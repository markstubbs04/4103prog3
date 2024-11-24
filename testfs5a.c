// 
// RUN formatfs before conducting this test then run testfs5b *after*
// this test.
//

#include "filesystem.h"

char poetry[]="Do not go gentle into that good night,\n"
  "Old age should burn and rave at close of day;\n"
  "Rage, rage against the dying of the light.\n"
  ""
  "Though wise men at their end know dark is right,\n"
  "Because their words had forked no lightning they\n"
  "Do not go gentle into that good night.\n"
  ""
  "Good men, the last wave by, crying how bright\n"
  "Their frail deeds might have danced in a green bay,\n"
  "Rage, rage against the dying of the light.\n"
  ""
  "Wild men who caught and sang the sun in flight,\n"
  "And learn, too late, they grieved it on its way,\n"
  "Do not go gentle into that good night.\n"
  ""
  "Grave men, near death, who see with blinding sight\n"
  "Blind eyes could blaze like meteors and be gay,\n"
  "Rage, rage against the dying of the light.\n"
  ""
  "And you, my father, there on the sad height,\n"
  "Curse, bless, me now with your fierce tears, I pray.\n"
  "Do not go gentle into that good night.\n"
  "Rage, rage against the dying of the light.\n";


int main(int argc, char *argv[]) {
  
  uint64_t ret, i, size = 0;
  File f;

  f = create_file("poetry");
  printf("ret from create_file(\"line-em-up\") = %p\n",
	 f);
  fs_print_error();

  if (f) {
    for (i=0; i < 500; i++) {
      ret = write_file(f, poetry, strlen(poetry));
      size += ret;
      printf("ret from write_file(f, buf, SOFTWARE_DISK_BLOCK_SIZE) = %" PRIu64 "\n",
	     ret);
      fs_print_error();
    }
    
    close_file(f);
    printf("Executed close_file(f).\n");
    fs_print_error();
    printf("Wrote %" PRIu64 " bytes.  Now run ./testfs5b.\n", size);
  }
  else {
    printf("FAIL.  Did you run formatfs before this test?\n");
  }
  
  return 0;
}
