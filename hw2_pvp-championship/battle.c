#include "graph.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>

Node graph[80];

int openFile(char battle_ID){
  char fileName[80];
  sprintf(fileName, "log_battle%c.txt", battle_ID);
  mode_t flag = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  return open(fileName, O_WRONLY | O_CREAT, flag);
}

void processFD(int fd[2][2]){
  dup2(fd[0][0], STDIN_FILENO);
  dup2(fd[1][1], STDOUT_FILENO);
  close(fd[0][1]); close(fd[1][0]);
}

bool compare(Status * stat1, Status * stat2){
  if (stat1 -> HP < stat2 -> HP)
    return true;
  else if ((stat1 -> HP == stat2 -> HP) &&
      stat1 -> real_player_id < stat2 -> real_player_id)
    return true;
  return false;
}

bool battleEnd(Status * stat1, Status * stat2){
  if (stat1 -> HP <= 0 || stat2 -> HP <= 0){
    stat1 -> battle_ended_flag = 1;
    stat2 -> battle_ended_flag = 1;
    return true;
  }
  return false;
}

void Battle(Status * stat1, Status * stat2, char battle_ID){
  int atk1 = (graph[battle_ID].attr == stat1 -> attr ? (stat1 -> ATK) * 2 : stat1 -> ATK);
  int atk2 = (graph[battle_ID].attr == stat2 -> attr ? (stat2 -> ATK) * 2 : stat2 -> ATK);
  stat2 -> HP -= atk1;
  if (battleEnd(stat1, stat2))
    return;

  stat1 -> HP -= atk2;
  battleEnd(stat1, stat2);
  return;
}

int main (int argc, char * argv[]) {

  init();
  char battle_ID = argv[1][0];
  int parent_pid = atoi(argv[2]);
  char cur_pid[8];
  sprintf(cur_pid, "%d", getpid());

  int log_fd = openFile(battle_ID);
  pid_t pid1, pid2;
  int pfd1[2][2], pfd2[2][2];
  
  pipe(pfd1[0]); pipe(pfd2[0]);
  pipe(pfd1[1]); pipe(pfd2[1]);
  ((pid1 = fork()) && (pid2 = fork()));

  if (pid1 == 0){
    processFD(pfd1);
    if (graph[battle_ID].left_player)
      execl("./player", "player", graph[battle_ID].left, cur_pid, (char *)0);
    else 
      execl("./battle", "battle", graph[battle_ID].left, cur_pid, (char *)0);
    exit(0);
  }
  else if (pid2 == 0){
    processFD(pfd2);
    if (graph[battle_ID].right_player)
      execl("./player", "player", graph[battle_ID].right, cur_pid, (char *)0);
    else
      execl("./battle", "battle", graph[battle_ID].right, cur_pid, (char *)0);
    exit(0);
  }
  else{

    //Waiting mode
    Status stat1, stat2;  
    close(pfd1[0][0]); close(pfd1[1][1]);
    close(pfd2[0][0]); close(pfd2[1][1]);  

    //Playing Mode
    while (stat1.battle_ended_flag != 1){

      readLog(log_fd, pfd1[1][0], argv[1], graph[battle_ID].left, pid1, &stat1);
      readLog(log_fd, pfd2[1][0], argv[1], graph[battle_ID].right, pid2, &stat2); 
     
      stat1.current_battle_id = battle_ID;
      stat2.current_battle_id = battle_ID;

      (compare(&stat1, &stat2) ? 
       Battle(&stat1, &stat2, battle_ID) : Battle(&stat2, &stat1, battle_ID));
      
      writeLog(log_fd, pfd1[0][1], argv[1], graph[battle_ID].left, pid1, &stat1);
      writeLog(log_fd, pfd2[0][1], argv[1], graph[battle_ID].right, pid2, &stat2);
    }
    wait(NULL);

    //Passing Mode
    bool flag = stat1.HP <= 0;
    char * wonBattle = (flag ? graph[battle_ID].right : graph[battle_ID].left);
    int pid = (flag ? pid2 : pid1);
    int (* pfd)[2] = (flag ? pfd2 : pfd1);
    Status * stat = (flag ? &stat2 : &stat1);

    if (parent_pid != 0){
      
      while(stat -> HP > 0){

        readLog(log_fd, pfd[1][0], argv[1], wonBattle, pid, stat); 

	writeLog(log_fd, STDOUT_FILENO, argv[1], graph[battle_ID].parent, parent_pid, stat);

	readLog(log_fd, STDIN_FILENO, argv[1], graph[battle_ID].parent, parent_pid, stat);
	writeLog(log_fd, pfd[0][1], argv[1], wonBattle, pid, stat);

	if (stat -> current_battle_id == 'A' && stat -> battle_ended_flag)
	  break;
      }
    }

    else{
      char msg[80];
      int winner = (flag ? stat2.real_player_id : stat1.real_player_id);
      sprintf(msg, "Champion is P%d\n", winner);
      write(STDOUT_FILENO, msg, strlen(msg));
    }

    wait(NULL);
  }
  exit(0);
}
