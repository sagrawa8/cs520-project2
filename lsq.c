#include <stdio.h>

#define LSQ_SIZE 32

typedef struct LSQ {
  int index;
  int free; 
  enum opcode_enum opcode;
  enum fu_enum fu;
  int src1_valid;
  int src1_tag;
  int src1_value;
  int src2_valid;
  int src2_tag;
  int src2_value;
  int lsq_prf;
  int dest;
}lsq;

lsq lsq_queue[LSQ_SIZE];
int front_lsq = -1, rear_lsq = -1;

// Check if the queue is full
int isFullLSQ() {
  if ((front_lsq == rear_lsq + 1) || (front_lsq == 0 && rear_lsq == LSQ_SIZE - 1)) return 1;
  return 0;
}

// Check if the queue is empty
int isEmptyLSQ() {
  if (front_lsq == -1) return 1;
  return 0;
}

// Adding an element
void enQueueLSQ(int index,int free,enum opcode_enum opcode,enum fu_enum fu, 
int src1_valid,
  int src1_tag,
  int src1_value,
  int src2_valid,
  int src2_tag,
  int src2_value,
  int lsq_prf,
  int dest)  {
  if (isFullLSQ())
    printf("\n Queue is full!! \n");
  else {
    if (front_lsq == -1) front_lsq = 0;
    rear_lsq = (rear_lsq + 1) % LSQ_SIZE;
  lsq_queue[rear_lsq].index = index;
  lsq_queue[rear_lsq].free = free;
  lsq_queue[rear_lsq].opcode = opcode;
  lsq_queue[rear_lsq].fu = fu;
  lsq_queue[rear_lsq].src1_valid = src1_valid;
  lsq_queue[rear_lsq].src1_tag=src1_tag ;
  lsq_queue[rear_lsq].src1_value= src1_value;
  lsq_queue[rear_lsq].src2_valid = src2_valid;
  lsq_queue[rear_lsq].src2_tag=src2_tag;
  lsq_queue[rear_lsq].src2_value=src2_value;
  lsq_queue[rear_lsq].lsq_prf=lsq_prf;
  lsq_queue[rear_lsq].dest=dest;
  }
}

// Removing an element
void deQueueLSQ() {

  if (isEmptyLSQ()) {
    printf("\n Queue is empty !! \n");
  } else {
    if (front_lsq == rear_lsq) {
      front_lsq = -1;
      rear_lsq = -1;
    } 
    // Q has only one element, so we reset the 
    // queue after dequeing it. ?
    else {
      front_lsq = (front_lsq + 1) % LSQ_SIZE;
    }
  }
}

// Display the queue
void displayLSQ() {
  int i;
  if (isEmptyLSQ())
    printf(" \n Empty Queue\n");
  else {
    printf("\n front_lsq -> %d ", front_lsq);
    printf("\n Items -> ");
    for (i = front_lsq; i != rear_lsq; i = (i + 1) % LSQ_SIZE) {
      printf("%d ", lsq_queue[i].index);
      printf("%d ", lsq_queue[i].free);
      printf("%d ", lsq_queue[i].opcode);
      printf("%d ", lsq_queue[i].fu);
      printf("%d ", lsq_queue[i].src1_valid);
      printf("%d ", lsq_queue[i].src1_tag);
      printf("%d ", lsq_queue[i].src1_value);
      printf("%d ", lsq_queue[i].src2_valid);
      printf("%d ", lsq_queue[i].src2_tag);
      printf("%d ", lsq_queue[i].src2_value);
      printf("%d ", lsq_queue[i].lsq_prf);
      printf("%d ", lsq_queue[i].dest);
    }
  }
}
