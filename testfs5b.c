//
// This is NOT a standalone test.  You must run this test *after*
// testfs5a.  Do not run formatfs again after running testfs5a.
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
  
  uint64_t ret, i, size = 0, seekpos = 0;
  File f;
  char buf[50+1];

  f = open_file("poetry", READ_ONLY);
  printf("ret from open_file(\"poetry\") = %p\n",
	 f);
  fs_print_error();

  printf("FORWARD.\n");
  if (f) {
    for (i=0; i < 6990; i++) {
      ret = seek_file(f, seekpos);
      printf("ret from seek_file(f, %" PRIu64") = %" PRIu64 "\n",
	     seekpos, ret);
      fs_print_error();
      
      ret = read_file(f, buf, 50);
      printf("ret from read_file(f, buf, 50) = %" PRIu64 "\n",
	     ret);
      fs_print_error();
      
      buf[50]=0;
      puts(buf);
      
      if (! strncmp(poetry + (seekpos % strlen(poetry)), buf, 50)) {
	printf("MATCH!\n");
      }
      else {
	printf("FAIL!\n");
	break;
      }
      do {
	seekpos += 57;
      } while (seekpos % strlen(poetry) > 800);
    }

    printf("BACKWARD.\n");

    for (i=0; i < 6530; i++) {
      do {
	seekpos -= 61;
      } while (seekpos % strlen(poetry) > 800);
      
      ret = seek_file(f, seekpos);
      printf("ret from seek_file(f, %" PRIu64 ") = %" PRIu64 "\n",
	     seekpos, ret);
      fs_print_error();
      
      ret = read_file(f, buf, 50);
      printf("ret from read_file(f, buf, 50) = %" PRIu64 "\n",
	     ret);
      fs_print_error();
      
      buf[50]=0;
      puts(buf);
      
      if (! strncmp(poetry + (seekpos % strlen(poetry)), buf, 50)) {
	printf("MATCH!\n");
      }
      else {
	printf("FAIL!\n");
	break;
      }
    }
    
    close_file(f);
    printf("Executed close_file(f).\n");
    fs_print_error();
  }
  else {
    printf("FAIL.  Did you run testfs5a before this test?\n");
  }
  
  return 0;
}
