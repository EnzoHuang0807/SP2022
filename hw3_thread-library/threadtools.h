#ifndef THREADTOOL
#define THREADTOOL
#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> 

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char path[BUF_SIZE]; 
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
    int fd_index;
};

extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */

extern struct pollfd fdarray[THREAD_MAX];
extern nfds_t nfds;

#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {\
  func(id, arg); \
}

#define thread_setup(id, arg) {\
  struct tcb * t = malloc(sizeof(struct tcb));\
  t -> id = id;\
  t -> arg = arg;\
  ready_queue[rq_size] = t;\
  rq_size ++;\
  \
  t -> x = 0; t -> y = 0;\
  sprintf(t -> path, "%d_%s", id, __func__);\
  mkfifo(t -> path, 0666);\
  t -> fd = open(t -> path, O_RDONLY | O_NONBLOCK);\
  \
  fdarray[nfds].fd = -1;\
  fdarray[nfds].events = POLLIN;\
  t -> fd_index = nfds;\
  nfds ++;\
  if (sigsetjmp(t -> environment, 1) == 0)\
    return;\
}

#define thread_exit() {\
  remove(RUNNING -> path);\
  siglongjmp(sched_buf, 3);\
}

#define thread_yield() {\
  if (sigsetjmp(RUNNING -> environment, 1) == 0){\
    sigprocmask(SIG_SETMASK, &alrm_mask ,NULL);\
    sigprocmask(SIG_SETMASK, &tstp_mask, NULL);\
    sigprocmask(SIG_SETMASK, &base_mask, NULL);\
  }\
}

#define async_read(count) ({\
    if (sigsetjmp(RUNNING -> environment, 1) == 0)\
      siglongjmp(sched_buf, 2);\
    read(RUNNING -> fd, RUNNING -> buf, count);\
})
#endif // THREADTOOL
