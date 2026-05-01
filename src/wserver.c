#include "io_helper.h"
#include "request.h"

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_BUF_TASKS 1024
#define MAXBUF (8192)
#define TRUE 1
#define FALSE 0
#define NONE_FD -1
#define NONE_FILESIZE -2
#define NONE_SMALLEST_IDX -3
#define NONE_CONN_FD -4
#define NONE_STAT_BUF NULL
#define NONE_FILENAME NULL
#define NONE_IS_STATIC -5
char default_root[]     = ".";
char default_schedalg[] = "FIFO";

typedef struct {
  int             buf_tasks;                 // set default buffer size
  int             task_queue[MAX_BUF_TASKS]; // task queue
  int             q_head;                    // head pointer of task queue
  int             q_tail;                    // tail pointer of task queue
  int             q_count;                   // count number of task queue
  int             is_SFF;                    // whether is_SFF
  int             file_is_static;            // whether file is static
  pthread_mutex_t q_mutex;                   // task queue mutex init
  pthread_cond_t  q_empty;                   // task queue cond init
  pthread_cond_t  q_full;                    // task queue cond init
  struct stat*    stat_buf;                  //(SFF) buf info
  char*           filename;                  // filename
} task_queue_t;

// need an task_queue_t(sync)
task_queue_t q;

void task_queue_init(task_queue_t* q, int is_SFF) {
  q->buf_tasks      = 1;              // set default buffer size
  q->q_head         = 0;              // head pointer of task queue
  q->q_tail         = 0;              // tail pointer of task queue
  q->q_count        = 0;              // count number of task queue
  q->file_is_static = NONE_IS_STATIC; // whether file is static
  q->is_SFF         = is_SFF;         //(schedalg)is_SFF
  q->stat_buf       = NONE_STAT_BUF;
  q->filename       = NONE_FILENAME;

  pthread_mutex_init(&q->q_mutex, NULL); // task queue mutex init
  pthread_cond_init(&q->q_full, NULL);   // task queue cond init
  pthread_cond_init(&q->q_empty, NULL);  // task queue cond init
}

void request_parse(task_queue_t* q, int fd) {
  int         is_static;
  struct stat sbuf;
  char        buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
  char        filename[MAXBUF], cgiargs[MAXBUF];

  readline_or_die(fd, buf, MAXBUF);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("method:%s uri:%s version:%s\n", method, uri, version);

  if (strcasecmp(method, "GET")) {
    request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
    return;
  }
  request_read_headers(fd);

  is_static = request_parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    request_error(fd, filename, "404", "Not found", "server could not find this file");
    return;
  }

  /*need to add to queue elements*/
  q->file_is_static = is_static;
  q->stat_buf       = (struct stat*)malloc(sizeof(struct stat));
  *q->stat_buf      = sbuf;
  q->filename       = (char*)malloc(sizeof(filename));
  strcpy(q->filename, filename);
}

/*push the ready conn_fd to buffer which will be handled by worker threads in
 * threads pool latter*/
int queue_is_full(task_queue_t* q) {
  return q->q_count == q->buf_tasks;
}

int queue_is_empty(task_queue_t* q) {
  return q->q_count == 0;
}

void queue_wait_not_full(task_queue_t* q) {
  while (queue_is_full(q)) {
    pthread_cond_wait(&q->q_empty, &q->q_mutex);
  }
}

void queue_wait_not_empty(task_queue_t* q) {
  while (queue_is_empty(q)) {
    pthread_cond_wait(&q->q_full, &q->q_mutex);
  }
}

void task_queue_push(task_queue_t* q, int conn_fd) {
  pthread_mutex_lock(&q->q_mutex);

  /*when the buffer is full, just wait*/
  queue_wait_not_full(q);

  q->task_queue[q->q_tail] = conn_fd;
  q->q_tail                = (q->q_tail + 1) % MAX_BUF_TASKS;
  q->q_count++;

  /*q->filesize*/
  request_parse(q, conn_fd);

  pthread_cond_signal(&q->q_full);
  pthread_mutex_unlock(&q->q_mutex);
}

int task_queue_pop(task_queue_t* q) {
  pthread_mutex_lock(&q->q_mutex);
  int conn_fd = NONE_CONN_FD; // set default to an error

  /*when the buffer is empty, just wait*/
  queue_wait_not_empty(q);

  conn_fd = q->task_queue[q->q_head];
  assert(conn_fd != NONE_CONN_FD);
  q->q_head = (q->q_head + 1) % MAX_BUF_TASKS;
  q->q_count--;

  pthread_cond_signal(&q->q_empty);
  pthread_mutex_unlock(&q->q_mutex);
  return conn_fd;
}

int task_queue_not_empty(task_queue_t* q) {
  return q->q_count > 0;
}

int find_smallest_idx(task_queue_t* q) {
  int smallest_idx = q->q_head;
  assert(task_queue_not_empty(q));
  int smallest_element = q->task_queue[smallest_idx];

  for (int i = q->q_head + 1; i < q->q_tail; i++) {
    if (q->stat_buf->st_size <= smallest_element) {
      smallest_idx = i;
    }
  }

  return smallest_idx;
}

void delete_smallest_idx(task_queue_t* q, int smallest_idx) {
  for (int i = smallest_idx; i < q->q_tail; i++) {
    q->task_queue[i] = q->task_queue[i + 1];
  }
  q->q_count--;
  q->q_tail = (q->q_tail - 1) % MAX_BUF_TASKS;
}

int task_queue_pop_SFF(task_queue_t* q) {
  pthread_mutex_lock(&q->q_mutex);
  int conn_fd      = NONE_CONN_FD; // set default to an error
  int smallest_idx = NONE_SMALLEST_IDX;

  /*when the buffer is empty, just wait*/
  queue_wait_not_empty(q);

  /*find the smallest filesize(index)*/
  smallest_idx = find_smallest_idx(q);
  assert(smallest_idx != NONE_SMALLEST_IDX);

  /*delete the smallest filesize(by index) and pop*/
  delete_smallest_idx(q, smallest_idx);

  pthread_cond_signal(&q->q_empty);
  pthread_mutex_unlock(&q->q_mutex);
  return conn_fd;
}

/*define the work thread*/
void* worker_thread(void* arg) {
  while (1) {
    /* already handle the sync in the task_queue_pop()*/
    /*thread will wait because of the q_cond*/
    int conn_fd = NONE_FD;

    if (q.is_SFF == TRUE) {
      conn_fd = task_queue_pop_SFF(&q);
    } else if (q.is_SFF == FALSE) {
      conn_fd = task_queue_pop(&q);
    }
    assert(conn_fd != NONE_FD);

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
int main(int argc, char* argv[]) {
  int   c;
  char* root_dir   = default_root;
  char* schedalg   = default_schedalg;
  int   port       = 10000;
  int   thread_num = 1; // set default thread num to 1

  while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
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
    case 's':
      schedalg = optarg;
      break;
    default:
      fprintf(stderr, "usage: prompt> ./wserver [-d basedir] [-p port] [-t "
                      "threads] [-b buffers] [-s schedalg]\n");
      exit(1);
    }

  // run at this directory
  chdir_or_die(root_dir);

  // create thead pool(able to handle requests)
  if (strcmp(schedalg, "FIFO") == 0) {
    task_queue_init(&q, FALSE); // init the task_queue
  } else if (strcmp(schedalg, "SFF") == 0) {
    task_queue_init(&q, TRUE);
  } else {
    task_queue_init(&q, FALSE);
  }
  pthread_t tids[thread_num];
  for (int i = 0; i < thread_num; i++) {
    pthread_create(&tids[i], NULL, worker_thread, NULL);
  }

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1) {
    struct sockaddr_in client_addr;
    int                client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t*)&client_addr, (socklen_t*)&client_len);

    task_queue_push(&q, conn_fd); // add conn_fd to buf_tasks(queue)
    /*Should not handle the data in master thread*/
    //    request_handle(conn_fd);
    //    close_or_die(conn_fd);
  }
  return 0;
}
