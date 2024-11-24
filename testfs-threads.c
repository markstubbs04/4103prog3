//
// IMPORTANT: This test will ONLY work if you have a correct,
// thread-safe implementation of filesystem.c.  That is NOT required
// for full credit in the assignment.
//
// This test is multithreaded and if the filesystem.c implementation
// isn't thread-safe, it's going to die.
//
// RUN formatfs before conducting this test!
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "filesystem.h"


// each thread runs this code
void *thread(void *args) {

  int id = atoi((char *)args);
  File f;
  uint64_t ret;
  char buf1[1000], buf2[1000];
  
  sprintf(buf1, "threadedcrasher-%1d\n", id);
  printf("Thread %d opening output file %s.\n", id, buf1);
  // should succeed
  f = create_file(buf1);
  
  if (! f) {
    fs_print_error();
    printf("FAIL.  Was formatfs run before this test?\n");
    exit(1);
  }
  
  for (int i = 0; i < 100; i++) {
    // should succeed
    printf("In thread %d, seeking to beginning of file.\n", id);
    seek_file(f, 0);
    fs_print_error();
    
    // should succeed
    sprintf(buf1, "Now is the time...or is it? %d %d", id, i);
    printf("In thread %d, writing \"%s\".\n", id, buf1);
    ret = write_file(f, buf1, strlen(buf1) + 1);
    printf("In thread %d, ret from write_file() = %" PRId64 "\n",
	   id, ret);
    fs_print_error();
    
    // should succeed
    printf("In thread %d, seeking to beginning of file.\n", id);
    seek_file(f, 0);
    fs_print_error();
    
    // should succeed
    bzero(buf2, 1000);
    ret = read_file(f, buf2, strlen(buf1) + 1);
    printf("In thread %d, ret from read_file() = %" PRId64 "\n",
	   id, ret);
    if (strcmp(buf1, buf2)) {
      printf("** DATA CORRUPTION ** in thread %d for iteration %d, \"%s\" != \"%s\".\n",
	     id, i, buf1, buf2);
      fs_print_error();
      exit(1);
    }
  }

  printf("Thread %d is complete.\n", id);
  
  return 0;
}

int main(int argc, char *argv[]) {
  
  uint64_t ret;
  char buf[1000];
  
  pthread_t thread1, thread2, thread3;
  
  printf("Creating threads...it's about to get real.\n");
  
  pthread_create(&thread1, NULL, thread, "1");
  pthread_create(&thread2, NULL, thread, "2");
  pthread_create(&thread3, NULL, thread, "3");

  // wait for threads to exit
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  pthread_join(thread3, NULL);

  return 0;
}

  
  
  
  
