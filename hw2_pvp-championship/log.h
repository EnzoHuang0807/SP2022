#include "status.h"
#include <unistd.h>
#include <stdio.h> 
#include <string.h>

void writeLog(int file_fd, int PSSM_fd, char * ID, char * target, int target_pid, Status * status);
void readLog(int file_fd, int PSSM_fd, char * ID, char * target, int target_pid, Status * status);
