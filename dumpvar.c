#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "sort.h"

void
usage(char *prog) 
{
  fprintf(stderr, "usage: %s <-i file>\n", prog);
  exit(1);
}


int
main(int argc, char *argv[])
{
  // arguments
  char *inFile = "testfile";
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "i:")) != -1) {
    switch (c) {
    case 'i':
      inFile = strdup(optarg);
      break;
    default:
      usage(argv[0]);
    }
  }

  // open input file
  int fd = open(inFile, O_RDONLY);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // output the number of records as a header for this file
  int recordsLeft;
  int rc;

  rc = read(fd, &recordsLeft, sizeof(recordsLeft));
  if (rc != sizeof(recordsLeft)) {
    perror("read");
    exit(1);
  }
  printf("Number of records: %d\n", recordsLeft);

  rec_nodata_t r;
  unsigned int data[MAX_DATA_INTS];

  while (recordsLeft) { 

    // Read the fixed-sized portion of record: size of data
    rc = read(fd, &r, sizeof(rec_nodata_t));
    if (rc != sizeof(rec_nodata_t)) {
      perror("read");
      exit(1);
    }
    assert(r.data_ints <= MAX_DATA_INTS);

    // Read the variable portion of the record
    rc = read(fd, &data, r.data_ints * sizeof(unsigned int));
    if (rc !=  r.data_ints * sizeof(unsigned int)) {
      perror("read");
      exit(1);
    }

    printf("index %d: data_ints: %u rec: ", r.index, r.data_ints);
    int j;
    for (j = 0; j < r.data_ints; j++) 
      printf("%u ", data[j]);
    printf("\n");
                
    recordsLeft--;
  }
    
  (void) close(fd);
  return 0;
}


