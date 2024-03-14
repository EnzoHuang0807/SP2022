#include <stdbool.h>
#include "log.h"

typedef struct node{
  bool left_player, right_player;
  char left[4];
  char right[4];
  char parent[4];
  Attribute attr;
  int loser;
  char loser_parent;
}Node;

extern Node graph[80];
void init();

