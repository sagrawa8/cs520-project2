#include <stdio.h>
#include <stdlib.h>
#include "rob.h"

struct Rob_Node* addToEmpty(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf) {
  if (last != NULL) return last;

  // allocate memory to the new Rob_Node
  struct Rob_Node* newRob_Node = (struct Rob_Node*)malloc(sizeof(struct Rob_Node));

  // assign data to the new Rob_Node
  newRob_Node->free = free;
  newRob_Node->opcode = opcode;
  newRob_Node->pc = pc;
  newRob_Node->dest_arf = dest_arf;
  newRob_Node->dest_prf = dest_prf;

  // assign last to newRob_Node
  last = newRob_Node;

  // create link to iteself
  last->next = last;

  return last;
}

// add Rob_Node to the front
struct Rob_Node* addFront(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf) {
  // check if the list is empty
  if (last == NULL) return addToEmpty(last, free, opcode, pc, dest_arf ,dest_prf);

  // allocate memory to the new Rob_Node
  struct Rob_Node* newRob_Node = (struct Rob_Node*)malloc(sizeof(struct Rob_Node));

  // add data to the Rob_Node
  newRob_Node->free = free;
  newRob_Node->opcode = opcode;
  newRob_Node->pc = pc;
  newRob_Node->dest_arf = dest_arf;
  newRob_Node->dest_prf = dest_prf;

  // store the address of the current first Rob_Node in the newRob_Node
  newRob_Node->next = last->next;

  // make newRob_Node as head
  last->next = newRob_Node;

  return last;
}

// add Rob_Node to the end
struct Rob_Node* addEnd(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf) {
  // check if the Rob_Node is empty
  if (last == NULL) return addToEmpty(last, free, opcode, pc, dest_arf ,dest_prf);

  // allocate memory to the new Rob_Node
  struct Rob_Node* newRob_Node = (struct Rob_Node*)malloc(sizeof(struct Rob_Node));

  // add data to the Rob_Node
  newRob_Node->free = free;
  newRob_Node->opcode = opcode;
  newRob_Node->pc = pc;
  newRob_Node->dest_arf = dest_arf;
  newRob_Node->dest_prf = dest_prf;

  // store the address of the head Rob_Node to next of newRob_Node
  newRob_Node->next = last->next;

  // point the current last Rob_Node to the newRob_Node
  last->next = newRob_Node;

  // make newRob_Node as the last Rob_Node
  last = newRob_Node;

  return last;
}

void traverse(struct Rob_Node* last) {
  struct Rob_Node* p;

  if (last == NULL) {
  printf("The list is empty");
  return;
  }

  p = last->next;

  do {
  printf("%d ", p->free);
  printf("%d ", p->opcode);
  printf("%d ", p->pc);
  printf("%d ", p->dest_arf);
  printf("%d ", p->dest_prf);
  p = p->next;

  } while (p != last->next);
}