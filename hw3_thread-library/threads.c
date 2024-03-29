#include "threadtools.h"

struct pollfd fdarray[THREAD_MAX];
nfds_t nfds;

void fibonacci(int id, int arg) {
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) {
        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            int tmp = RUNNING->y;  
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void collatz(int id, int arg) {
    // TODO
    thread_setup(id, arg);

    for (RUNNING -> i = RUNNING -> arg ; ;) {
        if (RUNNING ->i % 2 == 0)
            RUNNING ->i /= 2;
        else{ 
	    RUNNING -> i *= 3;
	    RUNNING -> i ++;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->i);
        sleep(1);

        if (RUNNING -> i == 1) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void max_subarray(int id, int arg) {
    // TODO
    thread_setup(id, arg);

    for (RUNNING -> i = 1; ; RUNNING -> i++) {

        async_read(5);
	//printf("success read\n");
        int num = atoi(RUNNING -> buf);
        //printf("get num: %d\n", num );	
        RUNNING -> y = (num > RUNNING -> y + num)? num : RUNNING -> y + num;
        RUNNING -> x = (RUNNING -> x > RUNNING -> y)? RUNNING -> x : RUNNING -> y;

        printf("%d %d\n", RUNNING -> id, RUNNING -> x);
        sleep(1);

        if (RUNNING -> i == RUNNING -> arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}
