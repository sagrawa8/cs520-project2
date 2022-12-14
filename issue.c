#include <stdio.h>

#define ISSUE_SIZE 32

typedef struct Issue {
  enum opcode_enum opcode;
  enum fu_enum fu;
  int value1;
  int value2;
  int dest;
}issue;

issue issue_queue[ISSUE_SIZE];
int front_issue = -1, rear_issue = -1;

// Check if the queue is full
int isFullIssue() {
  if ((front_issue == rear_issue + 1) || (front_issue == 0 && rear_issue == ISSUE_SIZE - 1)) return 1;
  return 0;
}

// Check if the queue is empty
int isEmptyIssue() {
  if (front_issue == -1) return 1;
  return 0;
}

// Adding an element
void enQueueIssue(enum opcode_enum opcode,
  enum fu_enum fu,
  int value1,
  int value2,
  int dest) {
  if (isFullIssue())
    printf("\n Queue is full!! \n");
  else {
    if (front_issue == -1) front_issue = 0;
  rear_issue = (rear_issue + 1) % ISSUE_SIZE;
  issue_queue[rear_issue].opcode = opcode;
  issue_queue[rear_issue].fu = fu;
  issue_queue[rear_issue].value1 = value1;
  issue_queue[rear_issue].value2 = value2;
  issue_queue[rear_issue].dest = dest;
    printf("\n Inserted -> %d", issue_queue[rear_issue].opcode);
  }
}

// Removing an element
void deQueueIssue() {
  if (isEmptyIssue()) {
    printf("\n Queue is empty !! \n");
  } else {
    if (front_issue == rear_issue) {
      front_issue = -1;
      rear_issue = -1;
    } 
    // Q has only one element, so we reset the 
    // queue after dequeing it. ?
    else {
      front_issue = (front_issue + 1) % ISSUE_SIZE;
    }
  }

}


int issueToFu(cpu cpu){
  for(int i=0; i<rear_issue; i++){
  if(issue_queue[front_issue].fu == alu_fu && cpu->stage[alu_fu].status != stage_stalled) {
			cpu->stage[alu_fu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[alu_fu].op1 = issue_queue[front_issue].value1;
			cpu->stage[alu_fu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == mult_fu && cpu->stage[mult_fu].status != stage_stalled) {
			cpu->stage[mult_fu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[mult_fu].op1 = issue_queue[front_issue].value1;
			cpu->stage[mult_fu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == load_fu && cpu->stage[load_fu].status != stage_stalled) {
			cpu->stage[load_fu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[load_fu].op1 = issue_queue[front_issue].value1;
			cpu->stage[load_fu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == store_fu && cpu->stage[fu_lsa].status != stage_stalled) {
			cpu->stage[store_fu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[store_fu].op1 = issue_queue[front_issue].value1;
			cpu->stage[store_fu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == br_fu && cpu->stage[br_fu].status != stage_stalled) {
			cpu->stage[br_fu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[br_fu].op1 = issue_queue[front_issue].value1;
			cpu->stage[br_fu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    }
}


