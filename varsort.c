#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "sort.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int TARGET_COLUMN = 0;

int compareRecPtr(const void *p1, const void *p2) {
  rec_dataptr_t* rec1_ptr = (rec_dataptr_t *)p1;
  rec_dataptr_t* rec2_ptr = (rec_dataptr_t *)p2;
  int col1 = MIN(TARGET_COLUMN, (rec1_ptr->data_ints)-1);
  int col2 = MIN(TARGET_COLUMN, (rec2_ptr->data_ints)-1);
  int data1 = rec1_ptr->data_ptr[col1];
  int data2 = rec2_ptr->data_ptr[col2];
  return data1 - data2;
}

void
usage(char *prog) 
{
  fprintf(stderr, "usage: %s <-i inputfile -o outputfile [-c column]>\n", prog);
  exit(1);
}


int
main(int argc, char *argv[])
{
  // arguments
  char *inFile = "testfile";
  char *outFile   = "/no/such/file";
  int c;

  opterr = 0;
  while ((c = getopt(argc, argv, "i:o:c:")) != -1) {
    switch (c) {
    case 'i':
      inFile = strdup(optarg);
      break;
    case 'o':
      outFile = strdup(optarg);
      break;
    case 'c':
      TARGET_COLUMN = atoi(strdup(optarg));
      //printf("%d\n",atoi(optarg));
      assert((TARGET_COLUMN-1) <= MAX_DATA_INTS);
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
  int cnt;
  int header;
  int rc;
  rc = read(fd, &header, sizeof(header));
  if (rc != sizeof(header)) {
    perror("read");
    exit(1);
  }
  cnt = header;
  int index = 0;
  int data_cnt = 0;
  rec_nodata_t r;
  rec_dataptr_t* buffer = malloc(header*sizeof(rec_dataptr_t));
  while (cnt) { 
	  
    // Read the fixed-sized portion of record: size of data
    rc = read(fd, &r, sizeof(rec_nodata_t));
    if (rc != sizeof(rec_nodata_t)) {
      perror("read");
      exit(1);
    }
    assert(r.data_ints <= MAX_DATA_INTS);
    buffer[index].index = r.index;
    buffer[index].data_ints = r.data_ints;
    buffer[index].data_ptr = malloc(buffer[index].data_ints*sizeof(unsigned int));
    // Read the variable portion of the record
    rc = read(fd, &buffer[index].data_ptr[0], buffer[index].data_ints * sizeof(unsigned int));
    if (rc !=  buffer[index].data_ints * sizeof(unsigned int)) {
      perror("read");
      exit(1);
    }
    index++;
    
    cnt--;
  }
  (void) close(fd);

  qsort(&buffer[0], header, sizeof(rec_dataptr_t), compareRecPtr);

//test output
  // open and create output file
  fd = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // output the number of records as a header for this file
  rc = write(fd, &header, sizeof(header));
  if (rc != sizeof(header)) {
    perror("write");
    exit(1);
    // should probably remove file here but ...
  }
  for (index = 0; index < header; index++) {
    int data_size = 2*sizeof(unsigned int);
    rc = write(fd, &buffer[index], data_size);

    if (rc != data_size ) {
      perror("write");
      exit(1);
      // should probably remove file here but ...
    }
    data_size = buffer[index].data_ints*sizeof(unsigned int);
       rc = write(fd, &buffer[index].data_ptr[0], data_size);

       if (rc != data_size ) {
         perror("write");
         exit(1);
         // should probably remove file here but ...
       }
    }

  // ok to ignore error code here, because we're done anyhow...
  (void) close(fd);

 printf("TARGET_COLUMN is %d \n",TARGET_COLUMN);
for( index = 0; index < header; index++ ){
  printf("index is %d, data_ints is %d, data is ",buffer[index].index,buffer[index].data_ints);
  for( data_cnt = 0; data_cnt < buffer[index].data_ints; data_cnt ++){
    	printf("%d, ",buffer[index].data_ptr[data_cnt]);

   }
   printf("\n");
  }

  return 0;
}


