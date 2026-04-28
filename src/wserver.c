#include "io_helper.h"
#include "request.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#define MAX_TASKS 1024

int buf_tasks = 1;         // set default buffer size
int task_queue[MAX_TASKS]; // task queue
int q_head = 0;            // head pointer of task queue
int q_tail = 0;            // tail pointer of task queue
int q_count = 0;           // count number of task queue

pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER; // task queue mutex init
pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;    // task queue cond init

char default_root[] = ".";

/*push the ready conn_fd to buffer which will be handled by worker threads in
 * threads pool latter*/
void task_queue_push(int conn_fd) {
  pthread_mutex_lock(&q_mutex);

  /*when the buffer is full, just wait*/
  while (q_count == buf_tasks) {
    pthread_cond_wait(&q_cond, &q_mutex);
  }

  task_queue[q_tail] = conn_fd;
  q_tail = (q_tail + 1) % MAX_TASKS;
  q_count++;

  pthread_cond_signal(&q_cond);
  pthread_mutex_unlock(&q_mutex);
}

int task_queue_pop(void) {
  pthread_mutex_lock(&q_mutex);
  int conn_fd = -1; // set default to an error

  /*when the buffer is empty, just wait*/
  while (q_count == 0) {
    pthread_cond_wait(&q_cond, &q_mutex);
  }

  conn_fd = task_queue[q_head];
  assert(conn_fd != -1);
  q_head = (q_head + 1) % MAX_TASKS;
  q_count--;

  pthread_mutex_unlock(&q_mutex);
  return conn_fd;
}

//
// ./wserver [-d <basedir>] [-p <portnum>]
//
int main(int argc, char *argv[]) {
  int c;
  char *root_dir = default_root;
  int port = 10000;

  while ((c = getopt(argc, argv, "d:p:b:")) != -1)
    switch (c) {
    case 'd':
      root_dir = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'b':
      buf_tasks = atoi(optarg);
      break;
    default:
      fprintf(stderr, "usage: wserver [-d basedir] [-p port]\n");
      exit(1);
    }

  // run out of this directory
  chdir_or_die(root_dir);

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1) {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr,
                                (socklen_t *)&client_len);
    task_queue_push(conn_fd);
    /*Should not handle the data in master thread*/
    //    request_handle(conn_fd);
    //    close_or_die(conn_fd);
  }
  return 0;
}
