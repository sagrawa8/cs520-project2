#include <stdio.h>

#define ISSUE_SIZE 32

typedef struct Issue {
  enum opcode_enum opcode;
  enum stage_enum fu;
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
  enum stage_enum fu,
  int value1,
  int value2,
  int dest) {
  if (isFullIssue())
    printf("\n Queue is full!! \n");
  else {
    if (front_issue == -1) front_issue = 0;
  rear_issue = (rear_issue + 1);
  issue_queue[rear_issue].opcode = opcode;
  issue_queue[rear_issue].fu = fu;
  issue_queue[rear_issue].value1 = value1;
  issue_queue[rear_issue].value2 = value2;
  issue_queue[rear_issue].dest = dest;
  //printf("\n Inserted -> %d", issue_queue[rear_issue].opcode);
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
  if(issue_queue[front_issue].fu == fu_alu) {
			cpu->stage[fu_alu].opcode = issue_queue[front_issue].opcode;
			cpu->stage[fu_alu].op1 = issue_queue[front_issue].value1;
			cpu->stage[fu_alu].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == fu_mul1 && cpu->stage[fu_mul1].status != stage_stalled) {
			cpu->stage[fu_mul1].opcode = issue_queue[front_issue].opcode;
			cpu->stage[fu_mul1].op1 = issue_queue[front_issue].value1;
			cpu->stage[fu_mul1].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == fu_lsa && cpu->stage[fu_lsa].status != stage_stalled) {
			cpu->stage[fu_lsa].opcode = issue_queue[front_issue].opcode;
			cpu->stage[fu_lsa].op1 = issue_queue[front_issue].value1;
			cpu->stage[fu_lsa].imm = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    if(issue_queue[front_issue].fu == fu_br && cpu->stage[fu_br].status != stage_stalled) {
			cpu->stage[fu_br].opcode = issue_queue[front_issue].opcode;
			cpu->stage[fu_br].op1 = issue_queue[front_issue].value1;
			cpu->stage[fu_br].op2 = issue_queue[front_issue].value2;
			int dest =  issue_queue[front_issue].dest;
      deQueueIssue();
      return dest;
		}
    }


void displayIssueQueue() {
  int i;
  if (isEmptyIssue()){
    //printf(" \n Empty Queue\n");
  }
  else {
    printf("|%10s", "opcode");
    printf("|%10s","fu");
    printf("|%10s", "value1");
    printf("|%10s", "value2");
    printf("|%10s", "dest");
    printf("|\n");
    for (i = front_issue; i != rear_issue; i = (i + 1) % ISSUE_SIZE) {
      printf("|%10d", issue_queue[i].opcode);
      printf("|%10d", issue_queue[i].fu);
      printf("|%10d", issue_queue[i].value1);
      printf("|%10d", issue_queue[i].value2);
      printf("|%10d", issue_queue[i].dest);
      printf("|\n");
    }
  }
}




