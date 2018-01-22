#include "server_impl.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

int numfull = 0;
int bound = 0;
int thread; // max work_thread
int buffer; // max wait
sem_t sem_full;
sem_t sem_empty;
sem_t mutex;
pthread_t* work_thread;

int* buffers;

void set_entry(int* entry, int connfd) {
    *entry = connfd;
}

void *thread_func(void * temp){
    while(1) {
        sem_wait(&sem_full);
        sem_wait(&mutex);
        int connfd = *(int*)temp;
        set_entry((int*)temp, -1);
        sem_post(&mutex);
        if(connfd >= 0) {
            sem_post(&sem_empty);
            requestHandle(connfd);
        } else {
            sem_post(&sem_full);
        }
    }
    return NULL;
}

void server_init(int argc, char* argv[]) {
    if (argc > 4 || argc < 4) {
        fprintf(stderr,\
                "Usage: %s <port> <threads> <buffers>\n", argv[0]);
        exit(1);
    }
    thread = atoi(argv[2]);
    buffer = atoi(argv[3]);
    if (thread <= 0 || buffer <= 0) {
        fprintf(stderr,\
                "Usage: %s <threads> or <buffers> is positive integer\n",\
                argv[0]);
        exit(1);
    }
    work_thread = malloc(sizeof(pthread_t) * thread);
    buffers = malloc (sizeof(int));
    *buffers = -1;
    bound = buffer + thread;
    sem_init(&sem_full, 0, 0);
    sem_init(&sem_empty, 0, bound);
    sem_init(&mutex, 0, 1);
    int i;
    for(i = 0; i < thread; i++) {
        pthread_create( &(work_thread[i]),\
                NULL, thread_func, (void*)(buffers));
    }
}

void server_dispatch(int connfd) {
    // Handle the request in the main thread.
    int i;
    for( i = 0; i < bound; i++) {
        sem_wait(&sem_empty);
        sem_wait(&mutex);
        if(*buffers < 0) {
            set_entry(buffers, connfd);
            sem_post(&sem_full);
            i = bound;
        }
        sem_post(&mutex);
    }
}

