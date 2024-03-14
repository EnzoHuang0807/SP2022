#include "threadtools.h"

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO

    if (signo == SIGALRM){
      printf("caught SIGALRM\n");
      alarm(timeslice);
    }
    else
      printf("caught SIGTSTP\n");

    sigprocmask(SIG_SETMASK, &base_mask, NULL);
    siglongjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */

void move_queue(){
   for (int i = 0; i < nfds; i++){
     if (fdarray[i].revents == POLLIN){
     
       for (int j = 0; j < wq_size; j++){
	 if (waiting_queue[j] -> fd_index == i){
	   ready_queue[rq_size] = waiting_queue[j];
	   rq_size++;

	   for (int k = j; k < wq_size - 1; k++)
             waiting_queue[k] = waiting_queue[k + 1];
	   wq_size --;
	   break;
	 }
       }
       fdarray[i].fd = -1;
       fdarray[i].revents = 0;
     }
   }
}

void fill(){
  RUNNING = ready_queue[rq_size - 1];
  rq_size --;
  rq_current = (rq_current == rq_size)? 0 : rq_current;
}

void scheduler() {
    // TODO 
   int src;
   if ((src = sigsetjmp(sched_buf, 1)) == 0){
     rq_current = 0;
     siglongjmp(RUNNING -> environment, 1);
   }

   if (poll(fdarray, nfds, 0))
     move_queue();

   if (src == 1)
     rq_current = (rq_current == rq_size - 1)? 0 : rq_current + 1;
   else{
     if (src == 2){
       fdarray[RUNNING -> fd_index].fd = RUNNING -> fd;
       waiting_queue[wq_size] = RUNNING;
       wq_size ++;
     }
     else
       free(RUNNING);
     fill();
   }

   if (rq_size == 0){
     if (wq_size == 0)
       return;
     else{
       poll(fdarray, nfds, -1);
       move_queue();
       rq_current = 0;
     }
   }
   siglongjmp(RUNNING -> environment, 1);
}
