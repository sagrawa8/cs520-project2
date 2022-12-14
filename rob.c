#include <stdio.h>
#include "apexCPU.h"

#define ROB_SIZE 32

typedef struct Rob {
  int free;
  enum opcode_enum opcode;
  int pc;
  int dest_arf;
  int dest_prf;
  int lsq_index;
}rob;

rob rob_queue[ROB_SIZE];
int front_rob = -1, rear_rob = -1;

// Check if the queue is full
int isFullROB() {
  if ((front_rob == rear_rob + 1) || (front_rob == 0 && rear_rob == ROB_SIZE - 1)) return 1;
  return 0;
}

// Check if the queue is empty
int isEmptyROB() {
  if (front_rob == -1) return 1;
  return 0;
}


void retire_ins(cpu cpu){
  printf("Retiring call");
  if(cpu->prf[rob_queue[front_rob].dest_prf].valid){
    int reg =  rob_queue[front_rob].dest_arf;
    printf("");
	  cpu->reg[reg] = cpu->prf[rob_queue[front_rob].dest_prf].value;
    rob_queue[front_rob].free=0;
	  deQueueROB();    
	  reportStage(cpu,retire,"R%02d<-%d",reg,cpu->reg[reg]);
  }
}

// Adding an element
void enQueueROB(int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf) {
  if (isFullROB())
    printf("\n Queue is full!! \n");
  else {
    if (front_rob == -1) front_rob = 0;
  rear_rob = (rear_rob + 1) % ROB_SIZE;
  rob_queue[rear_rob].free = free;
  rob_queue[rear_rob].opcode = opcode;
  rob_queue[rear_rob].pc = pc;
  rob_queue[rear_rob].dest_arf = dest_arf;
  rob_queue[rear_rob].dest_prf = dest_prf;
  //printf("\n Inserted -> %d", rob_queue[rear_rob].opcode);
  }
}

// Removing an element
void deQueueROB() {
  if (isEmptyROB()) {
    printf("\n Queue is empty !! \n");
  } else {
    if (front_rob == rear_rob) {
      front_rob = -1;
      rear_rob = -1;
    } 
    // Q has only one element, so we reset the 
    // queue after dequeing it. ?
    else {
      front_rob = (front_rob + 1) % ROB_SIZE;
    }
  }
}



// Display the queue
void displayROB() {
  int i;
  if (isEmptyROB()){
    //printf(" \n Empty Queue\n");
  }
  else {
    //printf("\n front_rob -> %d ", front_rob);
    //printf("\n Items -> ");
    printf("|\n");
    printf("|%8s", "free");
    printf("|%8s", "opcode");
    printf("|%8s", "pc");
    printf("|%8s","dest_arf");
    printf("|%8s", "dest_prf");
    printf("|%8s", "dest_prf");
    printf("|\n");
    for (i = front_rob; i != rear_rob; i = (i + 1) % ROB_SIZE) {
      printf("|%8d", rob_queue[i].free);
      printf("|%8d", rob_queue[i].opcode);
      printf("|%8x", rob_queue[i].pc);
      printf("|%8d", rob_queue[i].dest_arf);
      printf("|%8d", rob_queue[i].dest_prf);
      printf("|%8d", rob_queue[i].lsq_index);
      printf("|\n");
    }
  }
}


int getPCFromROB(int current_pc) {
  for(int i=0;i<ROB_SIZE;i++){
    if(rob_queue[i].pc==current_pc){
      return rob_queue[i].dest_prf;
    }
  }
  return 0;
}

