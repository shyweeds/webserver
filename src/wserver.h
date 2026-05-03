#ifndef __WSERVER_H__
#define __WSERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#define MAX_CAPACITY 1024
#define MAXBUF (8192)
#define NONE_FD -1
#define NONE_FILESIZE -2
#define NONE_SMALLEST_IDX -3
#define NONE_CONN_FD -4
#define NONE_STAT_BUF NULL
#define NONE_FILENAME NULL
#define NONE_IS_STATIC -5
#define LOG(fmt, ...)                                                                              \
  do {                                                                                             \
    fprintf(stderr, "[T%lu][%s:%d]" fmt "\n", (unsigned long)pthread_self(), __FILE__, __LINE__,   \
            ##__VA_ARGS_);                                                                         \
  } while (0)

typedef struct {
  int         conn_fd;          // connected socket fd
  bool        error_occur_flag; // error flag(true:error occur)
  bool        file_is_static;   // file is static
  char        filename[MAXBUF]; // request handle
  char        cgiargs[MAXBUF];  // request handle
  struct stat sbuf;             // request handle
} task_t;

typedef struct {
  bool            is_SFF;                   // whether is_SFF
  size_t          capacity;                 // set default buffer size
  size_t          q_head;                   // head pointer of task queue
  size_t          q_tail;                   // tail pointer of task queue
  size_t          q_count;                  // count number of task queue
  task_t          q_current_task;           // state machine(current task to handle)
  task_t          task_queue[MAX_CAPACITY]; // task queue
  pthread_mutex_t q_mutex;                  // task queue mutex init
  pthread_cond_t  q_empty;                  // task queue cond init
  pthread_cond_t  q_full;                   // task queue cond init
} task_queue_t;

void my_die(const char* msg);

#endif
