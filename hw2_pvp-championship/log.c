#include "log.h"

void readLog(int file_fd, int PSSM_fd, char * ID, char * target, int target_pid, Status * status){
  
  read(PSSM_fd, status, sizeof(Status));

  char buf[80];
  sprintf(buf, ("%s,%d pipe from %s,%d %d,%d,%c,%d\n"), 
      ID, getpid(), target, target_pid, 
      status ->real_player_id, status -> HP,
      status -> current_battle_id, status -> battle_ended_flag);

  write(file_fd, buf, strlen(buf));
}

void writeLog(int file_fd, int PSSM_fd, char * ID, char * target, int target_pid, Status * status){ 

  char buf[80];
  sprintf(buf, "%s,%d pipe to %s,%d %d,%d,%c,%d\n", 
      ID, getpid(), target, target_pid, 
      status ->real_player_id, status -> HP,
      status -> current_battle_id, status -> battle_ended_flag);

  write(file_fd, buf, strlen(buf));
  write(PSSM_fd, status, sizeof(Status));
}
