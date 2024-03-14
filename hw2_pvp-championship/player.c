#include "graph.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/stat.h> 

Node graph[80];

Attribute translate(char * attr){
  if (strcmp(attr, "FIRE") == 0)
    return FIRE;
  else if (strcmp(attr, "GRASS") == 0)
    return GRASS;
  else 
    return WATER;
}

int createFile(int player_ID){
  char fileName[80];
  sprintf(fileName, "log_player%d.txt", player_ID);
  mode_t flag = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  return open(fileName, O_WRONLY | O_CREAT, flag);
}

int openFile(int player_ID){
  char fileName[80];
  sprintf(fileName, "log_player%d.txt", player_ID);
  return open(fileName, O_WRONLY | O_APPEND);
}

int createFIFO(int player_ID){
  char fileName[80];
  sprintf(fileName, "player%d.fifo", player_ID);
  if (access(fileName, F_OK) != 0){
    mode_t flag = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    mkfifo(fileName, flag);
  }
  return open(fileName, O_RDONLY);
}

int openFIFO(int player_ID){
  char fileName[80];
  sprintf(fileName, "player%d.fifo", player_ID);
  if (access(fileName, F_OK) != 0){
    mode_t flag = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    mkfifo(fileName, flag);
  }
  return open(fileName, O_WRONLY);
}

int main (int argc, char * argv[]) {

  init();
  int player_ID = atoi(argv[1]);
  int parent_pid = atoi(argv[2]);

  if (player_ID >= 0 && player_ID <= 7){

    // Initialize a PSSM
    Status status;
    status.real_player_id = player_ID;

    int fd = open("player_status.txt", O_RDONLY);
    char buf[80], attr[8];

    for (int i = 0; i <= player_ID; i++){
      char * ptr = buf;
      read(fd, buf, 1);
      while( *(ptr) != '\n'){
	ptr ++;
        read(fd, ptr, 1);
      }
      ptr ++;
      *(ptr) = '\0';
    }

    sscanf(buf, "%d %d %s %c %d\n", &status.HP, &status.ATK, 
        attr, &status.current_battle_id, &status.battle_ended_flag);
    status.attr = translate(attr);
    
    fd = createFile(player_ID);
   
    int originHP = status.HP;
    char target[4] = {status.current_battle_id, '\0'};

    //Playing Mode    
    while (true){


      while (status.battle_ended_flag != 1){
        writeLog(fd, STDOUT_FILENO, argv[1], target, parent_pid, &status);
        readLog(fd, STDIN_FILENO, argv[1], target, parent_pid, &status);
      }

      if (status.current_battle_id == 'A')
	break;

      if (status.HP > 0){
        status.HP += (originHP - status.HP) / 2;
        status.battle_ended_flag = 0;
      }
      else{

	status.HP = originHP;
	status.battle_ended_flag = 0;
	
        char buf[80];
        sprintf(buf, "%d,%d fifo to %d %d,%d\n", 
	  player_ID, getpid(), graph[status.current_battle_id].loser, status.real_player_id, status.HP);
    
	int pfd = openFIFO(graph[status.current_battle_id].loser);
	
	write(pfd, &status, sizeof(Status));
        write(fd, buf, strlen(buf));
        break;
      }
    }
  }
  else{

    int pfd = createFIFO(player_ID);
    Status status;
    read(pfd, &status, sizeof(Status));

    int fd = openFile(status.real_player_id);
    char buf[80];
    sprintf(buf, "%d,%d fifo from %d %d,%d\n", 
	player_ID, getpid(), status.real_player_id, status.real_player_id, status.HP);
    write(fd, buf, strlen(buf));

    
    int originHP = status.HP;
    char target[4] = {graph[status.current_battle_id].loser_parent, '\0'};

    //Playing Mode    
    while (true){

      while (status.battle_ended_flag != 1){
        writeLog(fd, STDOUT_FILENO, argv[1], target, parent_pid, &status);
        readLog(fd, STDIN_FILENO, argv[1], target, parent_pid, &status);
      }

      if (status.current_battle_id == 'A')
	break;

      if (status.HP > 0){
        status.HP += (originHP - status.HP) / 2;
        status.battle_ended_flag = 0;
      }
      else{
        break;
      }
    }
  }
  exit(0);
}
