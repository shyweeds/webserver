#include "wserver.h"

#include "io_helper.h"
#include "request.h"

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char   default_root[]     = ".";
static char   default_schedalg[] = "FIFO";
static task_t initializer        = {0}; // init the q->q_target_task

void task_queue_init(task_queue_t* q, int is_SFF) {
  q->q_head  = 0;      // head pointer of task queue
  q->q_tail  = 0;      // tail pointer of task queue
  q->q_count = 0;      // count number of task queue
  q->is_SFF  = is_SFF; //(schedalg)is_SFF

  pthread_mutex_init(&q->q_mutex, NULL); // task queue mutex init
  pthread_cond_init(&q->q_full, NULL);   // task queue cond init
  pthread_cond_init(&q->q_empty, NULL);  // task queue cond init
}

bool request_parse(task_queue_t* q, task_t* local_task) {
  char        buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
  char        filename[MAXBUF], cgiargs[MAXBUF];
  int         is_static;
  struct stat sbuf;

  readline_or_die(local_task->conn_fd, buf, MAXBUF);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("method:%s uri:%s version:%s\n", method, uri, version);

  if (strcasecmp(method, "GET")) {
    request_error(local_task->conn_fd, method, "501", "Not Implemented",
                  "server does not implement this method");
    return false;
  }

  is_static = request_parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    request_error(local_task->conn_fd, filename, "404", "Not found",
                  "server could not find this file");
    return false;
  }

  local_task->file_is_static = is_static;
  strcpy(local_task->filename, filename);
  strcpy(local_task->cgiargs, cgiargs);
  local_task->sbuf = sbuf;
  return true;
  /*request_read_headers(fd);

  is_static = request_parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    request_error(fd, filename, "404", "Not found", "server could not find this file");
    return;
  }*/
}

/*push the ready conn_fd to buffer which will be handled by worker threads in
 * threads pool latter*/
bool queue_is_full(task_queue_t* q) {
  return q->q_count == q->capacity;
}

bool queue_is_empty(task_queue_t* q) {
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

void add_current_task_to_queue(task_queue_t* q) {
  q->task_queue[q->q_tail] = q->q_current_task;
}

void set_current_task_error_flag(task_queue_t* q) {
  q->q_current_task.error_occur_flag = true;
}

void task_queue_push(task_queue_t* q, task_t* local_task) {
  pthread_mutex_lock(&q->q_mutex);

  /*when the buffer is full, just wait*/
  queue_wait_not_full(q);

  if (request_parse(q, local_task) == true) {
    add_current_task_to_queue(q);
    q->q_tail = (q->q_tail + 1) % MAX_CAPACITY;
    q->q_count++;
  } else {
    set_current_task_error_flag(q);
  }

  pthread_cond_signal(&q->q_full);
  pthread_mutex_unlock(&q->q_mutex);
}

int task_queue_pop(task_queue_t* q) {
  pthread_mutex_lock(&q->q_mutex);
  task_t* current_task = &q->q_current_task;

  /*when the buffer is empty, just wait*/
  queue_wait_not_empty(q);

  /*set target tasks to be handle*/
  *current_task = q->task_queue[q->q_head];

  /*update struct's state*/
  q->q_head = (q->q_head + 1) % MAX_CAPACITY;
  q->q_count--;

  pthread_cond_signal(&q->q_empty);
  pthread_mutex_unlock(&q->q_mutex);
  return current_task->conn_fd;
}

int task_queue_not_empty(task_queue_t* q) {
  return q->q_count > 0;
}

int find_smallest_idx(task_queue_t* q) {
  int smallest_idx = q->q_head;

  assert(task_queue_not_empty(q));
  int smallest_element = q->task_queue[q->q_head].sbuf.st_size;

  for (int i = q->q_head + 1; i < q->q_tail; i++) {
    if (q->task_queue[i].sbuf.st_size <= smallest_element) {
      smallest_idx = i;
    }
  }
  return smallest_idx;
}

void delete_smallest_task(task_queue_t* q, int smallest_idx) {
  for (int i = smallest_idx; i < q->q_tail; i++) {
    q->task_queue[i] = q->task_queue[i + 1];
  }
  q->q_count--;
  q->q_tail = (q->q_tail - 1) % MAX_CAPACITY;
}

int task_queue_pop_SFF(task_queue_t* q) {
  pthread_mutex_lock(&q->q_mutex);
  int     smallest_idx = find_smallest_idx(q);
  task_t* current_task = &q->q_current_task;

  /*when the buffer is empty, just wait*/
  queue_wait_not_empty(q);

  /*update target task to be handle*/
  *current_task = q->task_queue[smallest_idx];

  /*delete the smallest filesize(by index) and pop*/
  delete_smallest_task(q, smallest_idx);

  pthread_cond_signal(&q->q_empty);
  pthread_mutex_unlock(&q->q_mutex);
  return current_task->conn_fd;
}

/*define the work thread*/
void* worker_thread(void* arg) {
  while (1) {
    /* already handle the sync in the task_queue_pop()*/
    /*thread will wait because of the q_cond*/
    int           poped_fd = NONE_FD;
    task_queue_t* q        = (task_queue_t*)arg;

    if (q->is_SFF == true) {
      poped_fd = task_queue_pop_SFF(q);
    } else if (q->is_SFF == false) {
      poped_fd = task_queue_pop(q);
    }

    /*handle the poped task*/
    request_handle(q);

    close_or_die(poped_fd);
  }
  return NULL;
}

void task_queue_push_handle_error(task_queue_t* q) {
  bool   error_occur  = q->q_current_task.error_occur_flag;
  task_t current_task = q->q_current_task;
  if (error_occur == true) {
    close_or_die(current_task.conn_fd);
    error_occur = false;
  } else if (error_occur == false) {
    return; // do nothing
  }
}

//
// prompt> ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s
// schedalg]
//
int main(int argc, char* argv[]) {
  int           c;
  char*         root_dir   = default_root;
  char*         schedalg   = default_schedalg;
  size_t        port       = 10000;
  size_t        thread_num = 1;   // set default thread num to 1
  task_t        local_task = {0}; // local task to be handle
  task_queue_t* q          = (task_queue_t*)malloc(sizeof(*q));
  ; // need an task_queue_t(sync)

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
      q->capacity = atoi(optarg);
      break;
    case 's':
      schedalg = optarg;
      break;
    default:
      fprintf(stderr, "usage: prompt> ./wserver [-d basedir] [-p port] [-t "
                      "threads] [-b buffers] [-s schedalg]\n");
      exit(1);
    }

  // run at root_dir directory
  chdir_or_die(root_dir);

  // create thead pool(able to handle requests)
  if (strcmp(schedalg, "FIFO") == 0) {
    task_queue_init(q, false); // init the task_queue
  } else if (strcmp(schedalg, "SFF") == 0) {
    task_queue_init(q, true);
  } else {
    task_queue_init(q, false);
  }

  /* pthread_t tids[thread_num];
    for (int i = 0; i < thread_num; i++) {
      pthread_create(&tids[i], NULL, worker_thread, (void*)q);
    }
    */

  // now, get to work
  int listen_fd = open_listen_fd_or_die(port);
  while (1) {
    struct sockaddr_in client_addr;
    int                client_len = sizeof(client_addr);
    int conn_fd = accept_or_die(listen_fd, (sockaddr_t*)&client_addr, (socklen_t*)&client_len);
    /******test*****/
    LOG("main():conn_fd = %d(>0)\n", conn_fd);

    local_task.conn_fd          = conn_fd;
    local_task.error_occur_flag = false;
    task_queue_push(q, &local_task); // add conn_fd to buf_tasks(queue)
    task_queue_push_handle_error(q);

    /*Should not handle the data in master thread*/
    //    request_handle(conn_fd);
    // close_or_die(conn_fd);
  }
  free(q);
  return 0;
}
