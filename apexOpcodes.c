#include "apexOpcodes.h"
#include "apexCPU.h"
#include "apexMem.h"
#include "apexOpInfo.h" // OpInfo definition
#include <string.h>
#include <stdio.h>
#include <assert.h>

/*---------------------------------------------------------
This file contains a C function for each opcode and
stage that needs to do work for that opcode in that
stage.

When functions for a specific opcode are registered,
the simulator will invoke that function for that operator
when the corresponding stage is cycled. A NULL
function pointer for a specific opcode/stage indicates
that no processing is required for that operation and
stage.

The registerAllOpcodes function should invoke the
registerOpcode function for EACH valid opcode. If
you add new opcodes to be simulated, code the
functions for each stage for that opcode, and add
an invocation of registerOpcode to the
registerAllOpcodes function.
---------------------------------------------------------*/

/*---------------------------------------------------------
  Helper Function Declarations
---------------------------------------------------------*/
void fetch_register1(cpu cpu);
void fetch_register2(cpu cpu);
void check_dest(cpu cpu);
void set_conditionCodes(cpu cpu,enum stage_enum stage);
void rob_insert(cpu cpu);
void updateIQ(cpu cpu,int dest);
/*---------------------------------------------------------
  Global Variables
---------------------------------------------------------*/
opStageFn opFns[retire+1][NUMOPS]={NULL}; // Array of function pointers, one for each stage/opcode combination


/*---------------------------------------------------------
  Decode stage functions
---------------------------------------------------------*/
void dss_decode_ren1(cpu cpu) {
	check_dest(cpu);
	rob_insert(cpu);
	fetch_register1(cpu);
	fetch_register2(cpu);
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

void dsi_decode_ren1(cpu cpu) {
	check_dest(cpu);
	rob_insert(cpu);
	fetch_register1(cpu);
	// Second operand is immediate... no fetch required
	if(cpu->stage[rename2_dispatch].opcode == LOAD){
			enQueueLSQ(1,LOAD,0,0,0,0,0,0,0,cpu->stage[rename2_dispatch].sr1);
	}
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

void ssi_decode_ren1(cpu cpu) {
	rob_insert(cpu);
	fetch_register1(cpu);
	fetch_register2(cpu);
	enQueueLSQ(1,STORE,0,0,0,0,0,0,0,cpu->stage[rename2_dispatch].sr1);
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

void movc_decode_ren1(cpu cpu) {
	check_dest(cpu);
	rob_insert(cpu);
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

void cbranch_decode_ren1(cpu cpu) {
	rob_insert(cpu);
	cpu->stage[decode_rename1].branch_taken=0;
	cpu->pc_set = cpu->stage[decode_rename1].pc;
	if (cpu->stage[decode_rename1].opcode==JUMP) cpu->stage[decode_rename1].branch_taken=1;
	if (cpu->stage[decode_rename1].opcode==BZ && cpu->cc.z) cpu->stage[decode_rename1].branch_taken=1;
	if (cpu->stage[decode_rename1].opcode==BNZ && !cpu->cc.z) cpu->stage[decode_rename1].branch_taken=1;
	if (cpu->stage[decode_rename1].opcode==BP && cpu->cc.p) cpu->stage[decode_rename1].branch_taken=1;
	if (cpu->stage[decode_rename1].opcode==BNP && !cpu->cc.p) cpu->stage[decode_rename1].branch_taken=1;
	if (cpu->stage[decode_rename1].branch_taken) {
		// Squash instruction currently in fetch
		cpu->stage[fetch].instruction=0;
		cpu->stage[fetch].status=stage_squashed;
		reportStage(cpu,fetch," squashed by previous branch");
		cpu->halt_fetch=1;
		reportStage(cpu,decode_rename1," branch taken");
	} else {
	  reportStage(cpu,decode_rename1," branch not taken");
  }
  cpu->stage[decode_rename1].status=stage_actionComplete;

}

void ren2_dis(cpu cpu) {
	//printf("This is a rename2_dispatch stage.");
	for(int i=0;i<32;i++){
		//printf("This is a rename2_dispatch stage. Inside loop.");
		if(!cpu->iq[i].opcode){
			cpu->iq[i].free= 1;
			cpu->iq[i].opcode= cpu->stage[rename2_dispatch].opcode ;
			cpu->iq[i].fu = cpu->stage[rename2_dispatch].fu;
			cpu->iq[i].src1_valid = cpu->stage[rename2_dispatch].op1Valid ;
  			cpu->iq[i].src1_tag = cpu->rat[cpu->stage[rename2_dispatch].sr1].prf;
  			cpu->iq[i].src1_value = cpu->stage[rename2_dispatch].op1;
  			cpu->iq[i].src2_valid = cpu->stage[rename2_dispatch].op2Valid ;
  			cpu->iq[i].src2_tag = cpu->rat[cpu->stage[rename2_dispatch].sr2].prf;
  			cpu->iq[i].src2_value = cpu->stage[rename2_dispatch].op2;
			cpu->iq[i].dest = cpu->rat[cpu->stage[rename2_dispatch].dr].prf;
			if(cpu->stage[rename2_dispatch].opcode == LOAD || cpu->stage[rename2_dispatch].opcode == STORE){
				cpu->iq[i].lsq_prf = 1;
			}
  			else{
				cpu->iq[i].lsq_prf = 0;
			}
			if(cpu->stage[rename2_dispatch].opcode == BZ 
			|| cpu->stage[rename2_dispatch].opcode == BNZ 
			|| cpu->stage[rename2_dispatch].opcode == BP 
			|| cpu->stage[rename2_dispatch].opcode == BNP){
				if(cpu->cc_set){
					cpu->iq[i].src1_valid = 1;
					cpu->iq[i].src1_tag = cpu->cc.prf;
					cpu->iq[i].dest = getPCFromROB(cpu->pc_set);
				}
			}
			break;
		}
	}
}

void issueInIq(cpu cpu) {
	printf("Issue function called");
	int array_length = sizeof(cpu->iq)/sizeof(cpu->iq[0]);
	for(int i=0;i<array_length;i++){
		if(cpu->iq[i].free && cpu->iq[i].src1_valid && cpu->iq[i].src2_valid){
			enQueueIssue(
			cpu->iq[i].opcode,
			cpu->iq[i].fu,
			cpu->iq[i].src1_value,
			cpu->iq[i].src2_value,
			cpu->iq[i].dest);
			cpu->iq[i].free = 0;
		}
	}
}


retire_ins(cpu);

/*---------------------------------------------------------
  Execute stage functions
---------------------------------------------------------*/
void add_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1+cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"res=%d+%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	set_conditionCodes(cpu,fu_alu);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void sub_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1-cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"res=%d-%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	set_conditionCodes(cpu,fu_alu);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void cmp_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1-cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"cc based on %d-%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	set_conditionCodes(cpu,fu_alu);
	// exForward(cpu);
}

void mul_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_mul1].result=cpu->stage[fu_mul1].op1*cpu->stage[fu_mul1].op2;
	reportStage(cpu,fu_mul1,"res=%d*%d",cpu->stage[fu_mul1].op1,cpu->stage[fu_mul1].op2);
	set_conditionCodes(cpu,fu_mul1);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_mul1].result; 
	if (cpu->stage[fu_mul1].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_mul1].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void and_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1&cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"res=%d&%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void or_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1|cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"res=%d|%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void xor_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1^cpu->stage[fu_alu].op2;
	reportStage(cpu,fu_alu,"res=%d^%d",cpu->stage[fu_alu].op1,cpu->stage[fu_alu].op2);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void movc_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_alu].result=cpu->stage[fu_alu].op1;
	reportStage(cpu,fu_alu,"res=%d",cpu->stage[fu_alu].result);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_alu].result; 
	if (cpu->stage[fu_alu].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_alu].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void load_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_lsa].effectiveAddr =
		cpu->stage[fu_lsa].op1 + cpu->stage[fu_lsa].offset;
	reportStage(cpu,fu_lsa,"effAddr=%08x",cpu->stage[fu_lsa].effectiveAddr);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_lsa].result; 
	if (cpu->stage[fu_lsa].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_lsa].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void store_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	cpu->stage[fu_lsa].effectiveAddr =
		cpu->stage[fu_lsa].op2 + cpu->stage[fu_lsa].imm;
	reportStage(cpu,fu_lsa,"effAddr=%08x",cpu->stage[fu_lsa].effectiveAddr);
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_lsa].result; 
	if (cpu->stage[fu_lsa].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_lsa].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void cbranch_execute(cpu cpu) {
	int dest = issueToFu(cpu);
	if (cpu->stage[fu_br].branch_taken) {
		// Update PC
		cpu->pc=cpu->stage[fu_br].pc+cpu->stage[fu_br].offset;
		reportStage(cpu,fu_br,"pc=%06x",cpu->pc);
		cpu->halt_fetch=0; // Fetch can start again next cycle
	} else {
		reportStage(cpu,fu_br,"No action... branch not taken");
	}
	cpu->prf[dest].valid = 1; 
	cpu->prf[dest].value = cpu->stage[fu_br].result; 
	if (cpu->stage[fu_br].result==0) cpu->prf[dest].z=1;
	else cpu->prf[dest].z=0;
	if (cpu->stage[fu_br].result > 0) cpu->prf[dest].p=1;
	else cpu->prf[dest].p=0;
	updateIQ(cpu,dest);
}

void fwd_execute(cpu cpu) {
	return;
}

/*---------------------------------------------------------
  Memory  stage functions
---------------------------------------------------------*/
void load_memory(cpu cpu) {
	cpu->stage[fu_lsa].result = dfetch(cpu,cpu->stage[fu_lsa].effectiveAddr);
	reportStage(cpu,fu_lsa,"res=MEM[%06x]",cpu->stage[fu_lsa].effectiveAddr);
}

void store_memory(cpu cpu) {
	dstore(cpu,cpu->stage[fu_lsa].effectiveAddr,cpu->stage[fu_lsa].op1);
	reportStage(cpu,fu_lsa,"MEM[%06x]=%d",
		cpu->stage[fu_lsa].effectiveAddr,
		cpu->stage[fu_lsa].op1);
}

/*---------------------------------------------------------
  Writeback stage functions
---------------------------------------------------------*/
void dest_writeback(cpu cpu) {
	retire_ins(cpu);
}

void halt_writeback(cpu cpu) {
	cpu->stop=1;
	strcpy(cpu->abend,"HALT instruction retired");
	reportStage(cpu,retire,"cpu stopped");
}

/*---------------------------------------------------------
  Externally available functions
---------------------------------------------------------*/
void registerAllOpcodes() {
	// Invoke registerOpcode for EACH valid opcode here
	registerOpcode(ADD,fu_alu,dss_decode_ren1,ren2_dis,issueInIq,add_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(ADDL,fu_alu,dsi_decode_ren1,ren2_dis,issueInIq,add_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(SUB,fu_alu,dss_decode_ren1,ren2_dis,issueInIq,sub_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(SUBL,fu_alu,dsi_decode_ren1,ren2_dis,issueInIq,sub_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(MUL,fu_mul1,dss_decode_ren1,ren2_dis,issueInIq,mul_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(AND,fu_alu,dss_decode_ren1,ren2_dis,issueInIq,and_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(OR,fu_alu,dss_decode_ren1,ren2_dis,issueInIq,or_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(XOR,fu_alu,dss_decode_ren1,ren2_dis,issueInIq,xor_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(MOVC,fu_alu,movc_decode_ren1,ren2_dis,issueInIq,movc_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(LOAD,fu_lsa,dsi_decode_ren1,ren2_dis,issueInIq,load_execute,load_memory,fwd_execute,dest_writeback);
	registerOpcode(STORE,fu_lsa,ssi_decode_ren1,ren2_dis,issueInIq,store_execute,store_memory,fwd_execute,fwd_execute);
	registerOpcode(CMP,fu_alu,ssi_decode_ren1,ren2_dis,issueInIq,cmp_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(JUMP,fu_br,cbranch_decode_ren1,ren2_dis,issueInIq,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BZ,fu_br,cbranch_decode_ren1,ren2_dis,issueInIq,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BNZ,fu_br,cbranch_decode_ren1,ren2_dis,issueInIq,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BP,fu_br,cbranch_decode_ren1,ren2_dis,issueInIq,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BNP,fu_br,cbranch_decode_ren1,ren2_dis,issueInIq,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(HALT,fu_alu,NULL,NULL,issueInIq,fwd_execute,fwd_execute,fwd_execute,halt_writeback);
}

void registerOpcode(int opNum,enum stage_enum fu,
	opStageFn decode_ren1,opStageFn ren2_dis,opStageFn issueInIq,opStageFn executeFn1,
	opStageFn executeFn2,opStageFn executeFn3,
	opStageFn writebackFn) {
	opFns[decode_rename1][opNum]=decode_ren1;
	opFns[rename2_dispatch][opNum]=ren2_dis;
	opFns[issue_instruction][opNum]=issueInIq;
	opFns[fu][opNum]=executeFn1;
	opFns[fu+1][opNum]=executeFn2;
	opFns[fu+2][opNum]=executeFn3;
	opFns[retire][opNum]=writebackFn;
}

char * disassemble(int instruction,char *buf) {
	// assumes buf is big enough to hold the full disassemble string (max is probably 32)
	int opcode=(instruction>>24);
	if (opcode>HALT || opcode<0) {
		printf("In disassemble, invalid opcode: %d\n",opcode);
		strcpy(buf,"????");
		return buf;
	}
	buf[0]='\0';
	int dr,sr1,sr2,offset;
	short imm;
	enum opFormat_enum fmt=opInfo[opcode].format;
	switch(fmt) {
		case fmt_nop:
			sprintf(buf,"%s",opInfo[opcode].mnemonic);
			break;
		case fmt_dss:
			dr=(instruction&0x00f00000)>>20;
			sr1=(instruction&0x000f0000)>>16;
			sr2=(instruction&0x0000f000)>>12;
			sprintf(buf,"%s R%02d,R%02d,R%02d",opInfo[opcode].mnemonic,dr,sr1,sr2);
			break;
		case fmt_dsi:
			dr=(instruction&0x00f00000)>>20;
			sr1=(instruction&0x000f0000)>>16;
			imm=(instruction&0x0000ffff);
			sprintf(buf,"%s R%02d,R%02d,#%d",opInfo[opcode].mnemonic,dr,sr1,imm);
			break;
		case fmt_di:
			dr=(instruction&0x00f00000)>>20;
			imm=(instruction&0x0000ffff);
			sprintf(buf,"%s R%02d,#%d",opInfo[opcode].mnemonic,dr,imm);
			break;
		case fmt_ssi:
			sr2=(instruction&0x00f00000)>>20;
			sr1=(instruction&0x000f0000)>>16;
			imm=(instruction&0x0000ffff);
			sprintf(buf,"%s R%02d,R%02d,#%d",opInfo[opcode].mnemonic,sr2,sr1,imm);
			break;
		case fmt_ss:
			sr1=(instruction&0x000f0000)>>16;
			sr2=(instruction&0x0000f000)>>12;
			sprintf(buf,"%s R%02d,R%02d",opInfo[opcode].mnemonic,sr1,sr2);
			break;
		case fmt_off:
			offset=((instruction&0x00ffffff)<<8)>>8; // shift left 8, then right 8 to propagate sign bit
			sprintf(buf,"%s #%d",opInfo[opcode].mnemonic,offset);
			break;
		default :
			printf("In disassemble, format not recognized: %d\n",fmt);
			strcpy(buf,"????");
	}
	return buf;
}


int fetchRegister(cpu cpu,int reg,int *value) {
	int found_prf;
	if(cpu->rat[reg].valid){
		found_prf = cpu->rat[reg].prf;
		if(cpu->prf[found_prf].valid){
			(*value) = cpu->prf[found_prf].value;
			reportStage(cpu,decode_rename1," R%d=%d",reg,cpu->prf[found_prf].value);
			return 1;
		}}
	else{
		if (cpu->regValid[reg]) {
			(*value)=cpu->reg[reg];
			reportStage(cpu,decode_rename1," R%d=%d",reg,cpu->reg[reg]);
			return 1;
	}
	}
	return 0;
}

/*---------------------------------------------------------
  Internal helper functions
---------------------------------------------------------*/
void fetch_register1(cpu cpu) {
	cpu->stage[decode_rename1].op1Valid=fetchRegister(cpu,cpu->stage[decode_rename1].sr1,&cpu->stage[decode_rename1].op1);
	if (cpu->stage[decode_rename1].op1Valid) {
		reportStage(cpu,decode_rename1," R%d=%d",cpu->stage[decode_rename1].sr1,cpu->stage[decode_rename1].op1);
	} else {
		reportStage(cpu,decode_rename1," R%d invalid",cpu->stage[decode_rename1].sr1);
	}
}

void fetch_register2(cpu cpu) {
	cpu->stage[decode_rename1].op2Valid=fetchRegister(cpu,cpu->stage[decode_rename1].sr2,&cpu->stage[decode_rename1].op2);
	if (cpu->stage[decode_rename1].op2Valid) {
		reportStage(cpu,decode_rename1," R%d=%d",cpu->stage[decode_rename1].sr2,cpu->stage[decode_rename1].op2);
	} else {
		reportStage(cpu,decode_rename1," R%d invalid",cpu->stage[decode_rename1].sr2);
	}
}

void check_dest(cpu cpu) {
	int reg=cpu->stage[decode_rename1].dr;
	if(isFreeListEmpty()){
		cpu->stage[decode_rename1].status=stage_stalled;
	}
	else{
		cpu->rat[reg].arf = reg;
		cpu->rat[reg].valid = 1;
		cpu->rat[reg].prf = dequeue();
	}
	cpu->prf[cpu->rat[reg].prf].valid = 0;
}

void set_conditionCodes(cpu cpu,enum stage_enum stage) {
	// Condition codes always set during the execute phase
	cpu->cc_set = 1;
	if (cpu->stage[stage].result==0) cpu->cc.z=1;
	else cpu->cc.z=0;
	if (cpu->stage[stage].result>0) cpu->cc.p=1;
	else cpu->cc.p=0;
}



void rob_insert(cpu cpu){
	enQueueROB(1,cpu->stage[decode_rename1].opcode,cpu->stage[decode_rename1].pc,
	cpu->stage[decode_rename1].dr,cpu->rat[cpu->stage[decode_rename1].dr].prf);
}

void updateIQ(cpu cpu,int dest){
	int array_length = sizeof(cpu->iq)/sizeof(cpu->iq[0]);
	for(int i=0;i<array_length;i++) {
		if(cpu->iq[i].src1_tag == dest){
			cpu->iq[i].src1_valid = 1;
			cpu->iq[i].src1_value = cpu->prf[dest].value;
		}
		if(cpu->iq[i].src2_tag == dest){
			cpu->iq[i].src2_valid = 1;
			cpu->iq[i].src2_value = cpu->prf[dest].value;
		}
		}
}
