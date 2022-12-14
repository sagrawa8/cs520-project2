#include <stdio.h>

#define LSQ_SIZE 32

typedef struct LSQ {
  int index;
  int free; 
  int lsa;
  int valid;
  int tag;
  int value;
  int valid_address;
  int value_address;
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
void enQueueLSQ( int index,
  int free,
  int lsa,
  int valid,
  int tag,
  int value,
  int valid_address,
  int value_address,
  int dest)  {
  if (isFullLSQ())
    printf("\n Queue is full!! \n");
  else {
    if (front_lsq == -1) front_lsq = 0;

  lsq_queue[rear_lsq].index = rear_lsq;
  lsq_queue[rear_lsq].free = free;
  lsq_queue[rear_lsq].lsa = lsa;
  lsq_queue[rear_lsq].valid = valid;
  lsq_queue[rear_lsq].tag = tag;
  lsq_queue[rear_lsq].value=value ;
  lsq_queue[rear_lsq].valid_address= valid_address;
  lsq_queue[rear_lsq].value_address = value_address;
  lsq_queue[rear_lsq].dest=dest;
  rear_lsq = (rear_lsq + 1) % LSQ_SIZE;
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

void updateLSQ(int value,int dest,int address){
	int array_length = sizeof(lsq_queue)/sizeof(lsq_queue[0]);
	for(int i=0;i<array_length;i++) {
		if(lsq_queue[i].tag == dest){
      lsq_queue[i].value = value;
      lsq_queue[i].value_address = address;
      lsq_queue[i].valid_address = 1;

		}
		}
}

// Display the queue
void displayLSQ() {
  int i;
  if (isEmptyLSQ()){
    //printf(" \n Empty Queue\n");
  }
  else {
    printf("|%10s", "index");
    printf("|%10s", "free");
    printf("|%10s", "lsa");
    printf("|%10s","valid");
    printf("|%10s", "tag");
    printf("|%10s", "value");
    printf("|%10s", "valid_add");
    printf("|%10s", "value_add");
    printf("|%10s", "dest");
    printf("|\n");
    for (i = front_lsq; i != rear_lsq; i = (i + 1) % LSQ_SIZE) {
      printf("|L%9d", lsq_queue[i].index);
      printf("|%10d", lsq_queue[i].free);
      printf("|%10d", lsq_queue[i].lsa);
      printf("|%10d", lsq_queue[i].valid);
      printf("|%10d", lsq_queue[i].tag);
      printf("|%10d", lsq_queue[i].value);
      printf("|%10d", lsq_queue[i].valid_address);
      printf("|%10d", lsq_queue[i].value_address);
      printf("|%10d", lsq_queue[i].dest);
      printf("|\n");
    }
  }
}
