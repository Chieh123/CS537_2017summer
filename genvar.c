#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "sort.h"
#include <sys/types.h>
#include <sys/stat.h>

void
usage(char *prog) 
{
  fprintf(stderr, "usage: %s <-s random seed> <-n number of datas> <-o output file> <-m maxdatasize> <-v> \n", prog);
  exit(1);
}

int
main(int argc, char *argv[])
{
  // arguments
  int randomSeed  = 0;
  int recordsLeft = 1;
  int maxDataInts = 1;
  char *outFile   = "/no/such/file";
  int verbose = 0;

  // input params
  int c;
  opterr = 0;
  while ((c = getopt(argc, argv, "n:s:o:m:v")) != -1) {
    switch (c) {
    case 'm':
      maxDataInts = atoi(optarg);
      assert(maxDataInts <= MAX_DATA_INTS);
      break;
    case 'n':
      recordsLeft = atoi(optarg);
      break;
    case 's':
      randomSeed  = atoi(optarg);
      break;
    case 'o':
      outFile     = strdup(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      usage(argv[0]);
    }
  }

  // seed random number generator
  srand(randomSeed);

  if (verbose) {
    printf("Max data ints: %d %d\n", maxDataInts, MAX_DATA_INTS);
  }

  // open and create output file
  int fd = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // output the number of records as a header for this file
  int rc = write(fd, &recordsLeft, sizeof(recordsLeft));
  if (rc != sizeof(recordsLeft)) {
    perror("write");
    exit(1);
    // should probably remove file here but ...
  }

  rec_data_t r;
  int i;
  for (i = 0; i < recordsLeft; i++) {
    // assign record index
    r.index = i;
    // fill in random data records
    r.data_ints = (rand() % maxDataInts) + 1;
    int j;
    for (j = 0; j < r.data_ints; j++) {
      r.data[j] = rand();
    }
    int data_size = 2*sizeof(unsigned int) + 
      r.data_ints*sizeof(unsigned int);

    rc = write(fd, &r, data_size);

    if (rc != data_size ) {
      perror("write");
      exit(1);
      // should probably remove file here but ...
    }
    if (verbose) {
      printf("%u %u", r.index, r.data_ints);
      for (j = 0; j < r.data_ints; j++) {
        printf(" %u", r.data[j]);
      }
      printf("\n");
    }
  }

  // ok to ignore error code here, because we're done anyhow...
  (void) close(fd);

  return 0;
}

