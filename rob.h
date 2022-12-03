#include <stdio.h>
#include <stdlib.h>
#include "apexOpcodes.h"

struct Rob_Node {
  int free;
  enum opcode_enum opcode;
  int pc;
  int dest_arf;
  int dest_prf;
  // Implement LSQ later
  struct Rob_Node* next;
};

struct Rob_Node* addToEmpty(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf);
struct Rob_Node* addFront(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf);
struct Rob_Node* addEnd(struct Rob_Node* last, int free, enum opcode_enum opcode, int pc, int dest_arf , int dest_prf);