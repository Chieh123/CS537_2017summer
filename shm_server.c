#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>         // shm
#include <fcntl.h>            // shm
#include <unistd.h>
#include <signal.h>           // sigaction
#include <sys/types.h>        // ftruncate
#include <unistd.h>           // ftruncate
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
// TODO(student): Include necessary headers

#define SHM_NAME "llin"
#define PAGESIZE 4096
#define SEGSIZE 64
#define PAGE_NO 64

sem_t * mutex;

typedef struct stats_t {    // sizeof(stat_t) = 48 * sizeof(char)
  int pid;
  char birth[25];
  int elapsed_sec;
  double elapsed_msec;
} stats_t;

void initial_client(struct stats_t* client) {
    client->pid = -1;
//  client->birth = NULL;
    client->elapsed_msec = 0;
    client->elapsed_sec = 0;
}



void exit_handler(int sig) {
    // TODO(student): Clear the shared memory segment
//  printf("exit handler");
    munmap(NULL, PAGESIZE);
    shm_unlink(SHM_NAME);
    sem_destroy(mutex);
    exit(0);
}

void print_client_information(struct stats_t* client) {
    if (client->pid != -1) {
         printf("pid : %d, birth : %s, elapsed : %d s %.4lf ms\n", \
         client->pid, client->birth, client->elapsed_sec, client->elapsed_msec);
    }
    return;
}

int main(int argc, char **argv) {
    // TODO(student): Signal handling
    // Use sigaction() here and call exit_handler
    struct sigaction act;
    act.sa_handler = exit_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    // TODO(student): Create a new shared memory segment
    int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0660);
    ftruncate(fd, PAGESIZE);
    // TODO(student): Point the mutex to the segment of the shared memory page
    // that you decide to store the semaphore
    void* shm_ptr = mmap(NULL, PAGESIZE, PROT_READ \
    | PROT_WRITE, MAP_SHARED, fd, 0);
    mutex = (sem_t*) shm_ptr;
    sem_init(mutex, 1, 1);  // Initialize semaphore
    // TODO(student): Some initialization of the rest of the segments in the
    // shared memory page here if necessary

    struct stats_t* clients[63];
    int clientIndex = 0;

    while (clientIndex < (PAGE_NO - 1)) {
            clients[clientIndex] = \
            (stats_t*)(shm_ptr + (clientIndex + 1) * SEGSIZE);
            initial_client(clients[clientIndex]);
            clientIndex++;
    }


    while (1) {
        // TODO(student): Display the statistics of active clients, i.e. valid
        // segments after some formatting
        clientIndex = 0;
//        sem_wait(mutex);
        while (clientIndex < 63) {
           print_client_information(clients[clientIndex]);
           clientIndex++;
        }
//        sem_post(mutex);
        sleep(1);
    }

    return 0;
}

