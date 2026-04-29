#include "io_helper.h"
#include "request.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#define MAX_BUF_TASKS 1024

typedef struct {
  int buf_tasks;                 // set default buffer size
  int task_queue[MAX_BUF_TASKS]; // task queue
  int q_head;                    // head pointer of task queue
  int q_tail;                    // tail pointer of task queue
  int q_count;                   // count number of task queue
  pthread_mutex_t q_mutex;       // task queue mutex init
  pthread_cond_t q_empty;        // task queue cond init
  pthread_cond_t q_full;         // task queue cond init
} task_queue_t;

// need an task_queue_t(sync)
task_queue_t q;

char default_root[] = ".";

void task_queue_init(task_queue_t *q) {
  q->buf_tasks = 1;                      // set default buffer size
  q->q_head = 0;                         // head pointer of task queue
  q->q_tail = 0;                         // tail pointer of task queue
  q->q_count = 0;                        // count number of task queue
  pthread_mutex_init(&q->q_mutex, NULL); // task queue mutex init
  pthread_cond_init(&q->q_full, NULL);   // task queue cond init
  pthread_cond_init(&q->q_empty, NULL);  // task queue cond init
}

/*push the ready conn_fd to buffer which will be handled by worker threads in
 * threads pool latter*/
void task_queue_push(task_queue_t *q, int conn_fd) {
  pthread_mutex_lock(&q->q_mutex);

  /*when the buffer is full, just wait*/
  while (q->q_count == q->buf_tasks) {
    pthread_cond_wait(&q->q_empty, &q->q_mutex);
  }

  q->task_queue[q->q_tail] = conn_fd;
  q->q_tail = (q->q_tail + 1) % MAX_BUF_TASKS;
  q->q_count++;

  pthread_cond_signal(&q->q_full);
  pthread_mutex_unlock(&q->q_mutex);
}

int task_queue_pop(task_queue_t *q) {
  pthread_mutex_lock(&q->q_mutex);
  int conn_fd = -1; // set default to an error

  /*when the buffer is empty, just wait*/
  while (q->q_count == 0) {
    pthread_cond_wait(&q->q_full, &q->q_mutex);
  }

  conn_fd = q->task_queue[q->q_head];
  assert(conn_fd != -1);
  q->q_head = (q->q_head + 1) % MAX_BUF_TASKS;
  q->q_count--;

  pthread_cond_signal(&q->q_empty);
  pthread_mutex_unlock(&q->q_mutex);
  return conn_fd;
}

/*define the work thread*/
void *worker_thread(void *arg) {
  while (1) {
    /* already handle the sync in the task_queue_pop()*/
    /*thread will wait because of the q_cond*/
    int conn_fd = task_queue_pop(&q);

    /*handle the things*/
    request_handle(conn_fd);
    close_or_die(conn_fd);
  }
  return NULL;
}

//
// prompt> ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s
// schedalg]
//
int main(int argc, char *argv[]) {
  int c;
  char *root_dir = default_root;
  int port = 10000;
  int thread_num = 1; // set default thread num to 1

  while ((c = getopt(argc, argv, "d:p:t:b:")) != -1)
    switch (c) {
    case 'd':
      root_dir = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      thread_num = atoi(optarg);
      break;
    case 'b':
      q.buf_tasks = atoi(optarg);
      break;
    default:
      fprintf(stderr, "usage: prompt> ./wserver [-d basedir] [-p port] [-t "
                      "threads] [-b buffers] [-s schedalg]\n");
      exit(1);
    }

  // run at this directory
  chdir_or_die(root_dir);

  // create thead pool(able to handle requests)
  task_queue_init(&q); // init the task_queue
  pthread_t tids[thread_num];
  for (int i = 0; i < thread_num; i++) {
    pthread_create(&tids[i], NULL, worker_thread, NULL);
  }

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1) {
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr,
                                (socklen_t *)&client_len);

    task_queue_push(&q, conn_fd); // add conn_fd to buf_tasks(queue)
    /*Should not handle the data in master thread*/
    //    request_handle(conn_fd);
    //    close_or_die(conn_fd);
  }
  return 0;
}
