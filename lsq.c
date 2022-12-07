#include <stdio.h>
#include <stdlib.h>

struct LSQ_Node {
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
  struct LSQ_Node* next;
};

struct LSQ_Node* addToEmptyLSQ(struct LSQ_Node* last_lsq_node, int index,int free,enum opcode_enum opcode,enum fu_enum fu, 
int src1_valid,
  int src1_tag,
  int src1_value,
  int src2_valid,
  int src2_tag,
  int src2_value,
  int lsq_prf,
  int dest) {
  if (last_lsq_node != NULL) return last_lsq_node;

  // allocate memory to the new LSQ_Node
  struct LSQ_Node* newLSQ_Node = (struct LSQ_Node*)malloc(sizeof(struct LSQ_Node));

  // assign data to the new LSQ_Node
  newLSQ_Node->index = index;
  newLSQ_Node->free = free;
  newLSQ_Node->opcode = opcode;
  newLSQ_Node->fu = fu;
  newLSQ_Node->src1_valid = src1_valid;
  newLSQ_Node->src1_tag=src1_tag ;
  newLSQ_Node->src1_value= src1_value;
  newLSQ_Node->src2_valid = src2_valid;
  newLSQ_Node->src2_tag=src2_tag;
  newLSQ_Node->src2_value=src2_value;
  newLSQ_Node->lsq_prf=lsq_prf;
  newLSQ_Node->dest=dest;
  // assign last_lsq_node to newLSQ_Node
  last_lsq_node = newLSQ_Node;

  // create link to iteself
  last_lsq_node->next = last_lsq_node;

  return last_lsq_node;
}

// add LSQ_Node to the front
struct LSQ_Node* addFrontLSQ(struct LSQ_Node* last_lsq_node, int index,int free,enum opcode_enum opcode,enum fu_enum fu, 
int src1_valid,
  int src1_tag,
  int src1_value,
  int src2_valid,
  int src2_tag,
  int src2_value,
  int lsq_prf,
  int dest) {
  // check if the list is empty
  if (last_lsq_node == NULL) 
  return addToEmptyLSQ(last_lsq_node,index,free,opcode,fu,src1_valid,src1_tag,src1_value,src2_valid,src2_tag,src2_value,lsq_prf,dest);

  // allocate memory to the new LSQ_Node
  struct LSQ_Node* newLSQ_Node = (struct LSQ_Node*)malloc(sizeof(struct LSQ_Node));

  // add data to the LSQ_Node
  newLSQ_Node->index = index;
  newLSQ_Node->free = free;
  newLSQ_Node->opcode = opcode;
  newLSQ_Node->fu = fu;
  newLSQ_Node->src1_valid = src1_valid;
  newLSQ_Node->src1_tag=src1_tag ;
  newLSQ_Node->src1_value= src1_value;
  newLSQ_Node->src2_valid = src2_valid;
  newLSQ_Node->src2_tag=src2_tag;
  newLSQ_Node->src2_value=src2_value;
  newLSQ_Node->lsq_prf=lsq_prf;
  newLSQ_Node->dest=dest;

  // store the address of the current first LSQ_Node in the newLSQ_Node
  newLSQ_Node->next = last_lsq_node->next;

  // make newLSQ_Node as head
  last_lsq_node->next = newLSQ_Node;

  return last_lsq_node;
}

// add LSQ_Node to the end
struct LSQ_Node* addEndLSQ(struct LSQ_Node* last_lsq_node, int index,int free,enum opcode_enum opcode,enum fu_enum fu, 
int src1_valid,
  int src1_tag,
  int src1_value,
  int src2_valid,
  int src2_tag,
  int src2_value,
  int lsq_prf,
  int dest) {
  // check if the LSQ_Node is empty
  if (last_lsq_node == NULL)
  return addToEmptyLSQ(last_lsq_node,index,free,opcode,fu,src1_valid,src1_tag,src1_value,src2_valid,src2_tag,src2_value,lsq_prf,dest);

  // allocate memory to the new LSQ_Node
  struct LSQ_Node* newLSQ_Node = (struct LSQ_Node*)malloc(sizeof(struct LSQ_Node));

  // add data to the LSQ_Node
  newLSQ_Node->index = index;
  newLSQ_Node->free = free;
  newLSQ_Node->opcode = opcode;
  newLSQ_Node->fu = fu;
  newLSQ_Node->src1_valid = src1_valid;
  newLSQ_Node->src1_tag=src1_tag ;
  newLSQ_Node->src1_value= src1_value;
  newLSQ_Node->src2_valid = src2_valid;
  newLSQ_Node->src2_tag=src2_tag;
  newLSQ_Node->src2_value=src2_value;
  newLSQ_Node->lsq_prf=lsq_prf;
  newLSQ_Node->dest=dest;

  // store the address of the head LSQ_Node to next of newLSQ_Node
  newLSQ_Node->next = last_lsq_node->next;

  // point the current last_lsq_node LSQ_Node to the newLSQ_Node
  last_lsq_node->next = newLSQ_Node;

  // make newLSQ_Node as the last_lsq_node LSQ_Node
  last_lsq_node = newLSQ_Node;

  return last_lsq_node;
}

void traverseLSQ(struct LSQ_Node* last_lsq_node) {
  struct LSQ_Node* p;

  if (last_lsq_node == NULL) {
  printf("The list is empty");
  return;
  }

  p = last_lsq_node->next;

  do {
  printf("%d ", p->index);
  printf("%d ", p->free);
  printf("%d ", p->opcode);
  p = p->next;

  } while (p != last_lsq_node->next);
}