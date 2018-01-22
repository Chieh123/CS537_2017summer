#include "server_impl.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int numfull = 0;
int useptr = 0;
int bound;
int threads; // max work_thread
int buffers; // max
pthread_mutex_t m;
pthread_cond_t empty;
pthread_cond_t invalid;
pthread_cond_t full;
pthread_t* buffer;


int* work_thread;

void init_entry(int* entry) {
    *entry = -1;
}

void set_entry(int* entry, int connfd) {
    *entry = connfd;
}

void *thread_func(void * temp){
    while(1) {

        pthread_mutex_lock(&m);
        while(*(int *)temp < 0) {
            pthread_cond_wait( &invalid, &m);
        }
        pthread_mutex_unlock(&m);

        if(*(int *)temp >= 0) {
            pthread_mutex_lock(&m);
            while(numfull == 0) {
                pthread_cond_wait( &empty, &m);
            }
            pthread_mutex_unlock(&m);

            requestHandle(*(int *)temp);

            pthread_mutex_lock(&m);
            *(int*)temp = -1;
            numfull--;
            pthread_cond_signal(&empty);
            pthread_cond_signal(&full);
            pthread_mutex_unlock(&m);
        }
    }
    return NULL;
}

void server_init(int argc, char* argv[]) {
    if (argc > 4 || argc < 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <buffers>\n",\
                argv[0]);
        exit(1);
    }
    threads = atoi(argv[2]);
    buffers = atoi(argv[3]);
    if (threads <= 0 || buffers <= 0) {
        exit(1);
    }
    bound = buffers + threads;
    buffer = malloc(sizeof(pthread_t) * threads);
    work_thread = malloc (sizeof(int) * threads);
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&empty,NULL);
    pthread_cond_init(&full,NULL);
    pthread_cond_init(&invalid,NULL);
    int i;
    for(i = 0; i < threads; i++) {
        init_entry(&(work_thread[i]));
        pthread_create( &(buffer[i]), NULL, thread_func,\
                (void*)&(work_thread[i]));
    }
}

void server_dispatch(int connfd) {
    // Handle the request in the main thread.
    int i = 0;
    for(i = 0; i < threads; ) {

        if((work_thread[i]) < 0) {
            pthread_mutex_lock(&m);
            while(numfull == bound){
                pthread_cond_wait(&full, &m);
            }
            set_entry(&(work_thread[i]), connfd);
            numfull++;
            pthread_cond_broadcast(&invalid);
            pthread_mutex_unlock(&m);
            break;
        }
        i = (i + 1) % threads;
    }
}
