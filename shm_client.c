#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>     // shm
#include <fcntl.h>        // shm
#include <unistd.h>
#include <signal.h>       // sigaction
#include <sys/types.h>    // ftruncate
#include <unistd.h>       // ftruncate
#include <string.h>
// TODO(student): Include necessary headers
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

#define SHM_NAME "llin"
#define SEGSIZE 64
#define PAGESIZE 4096
#define PAGE_NO 64



typedef struct stats_t {
  int pid;
  char birth[25];
  int elapsed_sec;
  double elapsed_msec;
} stats_t;

sem_t * mutex;
struct stats_t* my_client;
struct timeval start;

void set_elapsed(struct stats_t* client) {
//      clock_t end_t;
//      end_t = clock();
        struct timeval end;
        gettimeofday(&end, NULL);
//      printf("end %f\tstart %f\t\n",
//        (double)end.tv_usec, (double)start.tv_usec);
//      printf("end %d\tstart %d\t",(int)end.tv_sec,(int)start.tv_sec);
        client->elapsed_sec = (int)(end.tv_sec - start.tv_sec);
        client->elapsed_msec =((double)(end.tv_usec - start.tv_usec)/1000);
}

void client_exit(struct stats_t* client) {
    client->pid = -1;
    client->elapsed_msec = 0;
    client->elapsed_sec = 0;
//  client->birth = NULL;
}

void set_client(struct stats_t* client) {
    if (client->pid != -1) {
           return;
    }
    client->pid = (int)getpid();
    time_t current_time;
    current_time = time(NULL);
    if (current_time == ((time_t)-1)) {
//        (void) fprintf(stderr, "Failure to obtain the current time.\n");
        exit(EXIT_FAILURE);
    }
    char* rt =  ctime(&current_time);
    if (rt == NULL) {
        perror(NULL);
    }
    strncpy(client->birth, ctime(&current_time), 25);
    client->birth[24] = '\0';

    if (client->birth == NULL) {
        (void) fprintf(stderr, "Failure to convert the current time.\n");
        exit(EXIT_FAILURE);
    }
    client->elapsed_sec = 0;
    client->elapsed_msec = 0;
}

void print_active_clients(struct stats_t* client) {
     if (client->pid == -1)
        return;
     printf("%d ", client->pid);
//     fprintf(stderr, "%d ", client->pid);
}

struct stats_t* check_empty(struct stats_t* client) {
    if (client->pid == -1) {
         return client;
    }
    return NULL;
}

void exit_handler(int sig) {
    // new routine defined here specified by sigaction() in main
    // TODO(student): critical section begins
        sem_wait(mutex);
    // client reset its segment, or mark its segment as valid
    // so that the segment can be used by another client later.
        client_exit(my_client);
        sem_post(mutex);
    //    munmap(NULL, PAGESIZE);
    // critical section ends
    exit(0);
}

int main(int argc, char *argv[]) {
    // TODO(student): Signal handling
    // Use sigaction() here and call exit_handler
    gettimeofday(&start, NULL);
    struct sigaction act;
    act.sa_handler = exit_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    // TODO(student): Open the preexisting shared memory segment created by
    // shm_server
    int fd = shm_open(SHM_NAME, O_RDWR , 0660);
    if (fd < 0)
        exit(1);
//  printf( "===test===\n");

    ftruncate(fd, PAGESIZE);
    // TODO(student): point the mutex to the particular segment of the shared
    // memory page
    void* shm_ptr = mmap(NULL, PAGESIZE, PROT_READ \
    | PROT_WRITE, MAP_SHARED, fd, 0);
    mutex = (sem_t*) shm_ptr;

    // TODO(student): critical section begins
        sem_wait(mutex);    // wait until unlock
        // client searching through the segments of the page to find a valid
        // (or available) segment and then mark this segment as invalid

        int clientIndex = 1;

        while (clientIndex < (PAGE_NO)) {
             my_client = (stats_t*)(shm_ptr + (clientIndex) * SEGSIZE);
             my_client = check_empty(my_client);
             if (my_client != NULL) {
                set_client(my_client);
                break;
              }
             clientIndex++;
//  if no empty segment, should we infinitely find a segment?
        }
        if (my_client == NULL) {
             sem_post(mutex);
             perror("error");
             exit(1);
        }
        sem_post(mutex);
    // critical section ends

    struct stats_t* client_ptr;
    while (1) {
        // TODO(student): fill in fields in stats_t
        clientIndex = 1;
        set_elapsed(my_client);
        sleep(1);
        printf("Active clients : ");
//        fprintf(stderr, "Active clients (%d) : ", getpid());
//        sem_wait(mutex);
        while (clientIndex < PAGE_NO) {
             client_ptr = (stats_t*)(shm_ptr + clientIndex * SEGSIZE);
             print_active_clients(client_ptr);
             clientIndex++;
        }
//        sem_post(mutex);
        printf("\n");
//        fprintf(stderr,"\n");
        // display the PIDs of all the active clients
    }

    return 0;
}




