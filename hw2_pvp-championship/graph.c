#include "graph.h"
#include <string.h>

void setBattle(int n, bool leftP, bool rightP, 
    char * left, char * right, char * parent, Attribute attr, 
    int loser, char loser_parent){

  graph[n].left_player = leftP;
  graph[n].right_player = rightP;

  strcpy(graph[n].left, left);
  strcpy(graph[n].right, right);
  strcpy(graph[n].parent, parent);

  graph[n].attr = attr;
  graph[n].loser = loser;
  graph[n].loser_parent = loser_parent;
}

void init(){
  setBattle('A', false, false, "B", "C", " ", FIRE, -1, ' ');
  setBattle('B', false, false, "D", "E", "A", GRASS, 14, 'C');
  setBattle('C', false, true, "F", "14", "A", WATER, -1, ' ');
  setBattle('D', false, false, "G", "H", "B", WATER, 10, 'K');
  setBattle('E', false, false, "I", "J", "B", FIRE, 13, 'L');
  setBattle('F', false, false, "K", "L", "C", FIRE, -1, ' ');
  setBattle('G', true, true, "0", "1", "D", FIRE, 8, 'M');
  setBattle('H', true, true, "2", "3", "D", GRASS, 11, 'N');
  setBattle('I', true, true, "4", "5", "E", WATER, 9, 'M');
  setBattle('J', true, true, "6", "7", "E", GRASS, 12, 'N');
  setBattle('K', false, true, "M", "10", "F", GRASS, -1, ' ');
  setBattle('L', false, true, "N", "13", "F", GRASS, -1, ' ');
  setBattle('M', true, true, "8", "9", "K", FIRE, -1, ' ');
  setBattle('N', true, true, "11", "12", "L", WATER, -1, ' ');


}


