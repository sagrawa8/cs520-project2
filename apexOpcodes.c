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
void exForward(cpu cpu,enum stage_enum stage);
void rob_insert(cpu cpu);

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
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

void ssi_decode_ren1(cpu cpu) {
	rob_insert(cpu);
	fetch_register1(cpu);
	fetch_register2(cpu);
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
}

void issue(cpu cpu) {
	fetch_register1(cpu);
	fetch_register2(cpu);
	check_dest(cpu);
	cpu->stage[decode_rename1].status=stage_actionComplete;
}

/*---------------------------------------------------------
  Execute stage functions
---------------------------------------------------------*/
void add_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1+cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d+%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	set_conditionCodes(cpu,fu_alu1);
	exForward(cpu,fu_alu1);
}

void sub_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1-cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d-%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	set_conditionCodes(cpu,fu_alu1);
	exForward(cpu,fu_alu1);
}

void cmp_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1-cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"cc based on %d-%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	set_conditionCodes(cpu,fu_alu1);
	// exForward(cpu);
}

void mul_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1*cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d*%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	set_conditionCodes(cpu,fu_alu1);
	exForward(cpu,fu_alu1);
}

void and_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1&cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d&%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	exForward(cpu,fu_alu1);
}

void or_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1|cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d|%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	exForward(cpu,fu_alu1);
}

void xor_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1^cpu->stage[fu_alu1].op2;
	reportStage(cpu,fu_alu1,"res=%d^%d",cpu->stage[fu_alu1].op1,cpu->stage[fu_alu1].op2);
	exForward(cpu,fu_alu1);
}

void movc_execute(cpu cpu) {
	cpu->stage[fu_alu1].result=cpu->stage[fu_alu1].op1;
	reportStage(cpu,fu_alu1,"res=%d",cpu->stage[fu_alu1].result);
	exForward(cpu,fu_alu1);
}

void load_execute(cpu cpu) {
	cpu->stage[fu_ld1].effectiveAddr =
		cpu->stage[fu_ld1].op1 + cpu->stage[fu_ld1].offset;
	reportStage(cpu,fu_ld1,"effAddr=%08x",cpu->stage[fu_ld1].effectiveAddr);
}

void store_execute(cpu cpu) {
	cpu->stage[fu_st1].effectiveAddr =
		cpu->stage[fu_st1].op2 + cpu->stage[fu_st1].imm;
	reportStage(cpu,fu_st1,"effAddr=%08x",cpu->stage[fu_st1].effectiveAddr);
}

void cbranch_execute(cpu cpu) {
	if (cpu->stage[fu_br1].branch_taken) {
		// Update PC
		cpu->pc=cpu->stage[fu_br1].pc+cpu->stage[fu_br1].offset;
		reportStage(cpu,fu_br1,"pc=%06x",cpu->pc);
		cpu->halt_fetch=0; // Fetch can start again next cycle
	} else {
		reportStage(cpu,fu_br1,"No action... branch not taken");
	}
}

void fwd_execute(cpu cpu) {
	return;
}

/*---------------------------------------------------------
  Memory  stage functions
---------------------------------------------------------*/
void load_memory(cpu cpu) {
	cpu->stage[fu_ld2].result = dfetch(cpu,cpu->stage[fu_ld2].effectiveAddr);
	reportStage(cpu,fu_ld2,"res=MEM[%06x]",cpu->stage[fu_ld2].effectiveAddr);
	assert(cpu->fwdBus[1].valid==0); // load should not have used the ex forwarding bus
	cpu->fwdBus[1].tag=cpu->stage[fu_ld2].dr;
	cpu->fwdBus[1].value=cpu->stage[fu_ld2].result;
	cpu->fwdBus[1].valid=1;
}

void store_memory(cpu cpu) {
	dstore(cpu,cpu->stage[fu_st2].effectiveAddr,cpu->stage[fu_st2].op1);
	reportStage(cpu,fu_st2,"MEM[%06x]=%d",
		cpu->stage[fu_st2].effectiveAddr,
		cpu->stage[fu_st2].op1);
}

/*---------------------------------------------------------
  Writeback stage functions
---------------------------------------------------------*/
void dest_writeback(cpu cpu) {
	int reg=cpu->stage[retire].dr;
	cpu->reg[reg]=cpu->stage[retire].result;
	cpu->regValid[reg]=1;
	reportStage(cpu,retire,"R%02d<-%d",reg,cpu->stage[retire].result);
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
	// New Code
	// kanchan git push
	registerOpcode(ADD,alu_fu,dss_decode_ren1,ren2_dis,issue,add_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(ADDL,alu_fu,dsi_decode_ren1,ren2_dis,issue,add_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(SUB,alu_fu,dss_decode_ren1,ren2_dis,issue,sub_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(SUBL,alu_fu,dsi_decode_ren1,ren2_dis,issue,sub_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(MUL,mult_fu,dss_decode_ren1,ren2_dis,issue,mul_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(AND,alu_fu,dss_decode_ren1,ren2_dis,issue,and_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(OR,alu_fu,dss_decode_ren1,ren2_dis,issue,or_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(XOR,alu_fu,dss_decode_ren1,ren2_dis,issue,xor_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(MOVC,alu_fu,movc_decode_ren1,ren2_dis,issue,movc_execute,fwd_execute,fwd_execute,dest_writeback);
	registerOpcode(LOAD,load_fu,dsi_decode_ren1,ren2_dis,issue,load_execute,load_memory,fwd_execute,dest_writeback);
	registerOpcode(STORE,store_fu,ssi_decode_ren1,ren2_dis,issue,store_execute,store_memory,fwd_execute,fwd_execute);
	registerOpcode(CMP,alu_fu,ssi_decode_ren1,ren2_dis,issue,cmp_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(JUMP,br_fu,cbranch_decode_ren1,ren2_dis,issue,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BZ,br_fu,cbranch_decode_ren1,ren2_dis,issue,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BNZ,br_fu,cbranch_decode_ren1,ren2_dis,issue,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BP,br_fu,cbranch_decode_ren1,ren2_dis,issue,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(BNP,br_fu,cbranch_decode_ren1,ren2_dis,issue,cbranch_execute,fwd_execute,fwd_execute,NULL);
	registerOpcode(HALT,alu_fu,NULL,NULL,issue,fwd_execute,fwd_execute,fwd_execute,halt_writeback);
}

void registerOpcode(int opNum,enum fu_enum fu,
	opStageFn decode_ren1,opStageFn ren2_dis,opStageFn issue,opStageFn executeFn1,
	opStageFn executeFn2,opStageFn executeFn3,
	opStageFn writebackFn) {
	opFns[decode_rename1][opNum]=decode_ren1;
	opFns[rename2_dispatch][opNum]=ren2_dis;
	opFns[issue_instruction][opNum]=issue;
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

	// Check forwarding busses in program order
	for (int fb=0;fb<3;fb++) {
		if (cpu->fwdBus[fb].valid && reg==cpu->fwdBus[fb].tag) {
			(*value)=cpu->fwdBus[fb].value;
			return 1;
		}
	}
	if (cpu->regValid[reg]) {
		(*value)=cpu->reg[reg];
		return 1;
	}
	int found_prf;
	if(cpu->rat[reg].valid)
		found_prf = cpu->rat[reg].prf;
		if(cpu->prf[found_prf].valid){
			(*value) = cpu->prf[found_prf].value;
			reportStage(cpu,decode_rename1," R%d=%d",reg,cpu->prf[found_prf].value);
			return 1;
		}
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
}

void set_conditionCodes(cpu cpu,enum stage_enum stage) {
	// Condition codes always set during the execute phase
	if (cpu->stage[stage].result==0) cpu->cc.z=1;
	else cpu->cc.z=0;
	if (cpu->stage[stage].result>0) cpu->cc.p=1;
	else cpu->cc.p=0;
}

void exForward(cpu cpu,enum stage_enum stage) {
	cpu->fwdBus[0].tag=cpu->stage[stage].dr;
	cpu->fwdBus[0].value=cpu->stage[stage].result;
	cpu->fwdBus[0].valid=1;
}


void rob_insert(cpu cpu){
	cpu->rob_node = addEndROB(cpu->rob_node, 
	1,cpu->stage[decode_rename1].opcode,cpu->stage[decode_rename1].pc,
	cpu->stage[decode_rename1].dr,cpu->rat[cpu->stage[decode_rename1].dr].prf);
}