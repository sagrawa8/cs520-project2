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
void set_conditionCodes(cpu cpu);

/*---------------------------------------------------------
  Global Variables
---------------------------------------------------------*/
opStageFn opFns[5][NUMOPS]; // Array of function pointers, one for each stage/opcode combination


/*---------------------------------------------------------
  Decode stage functions
---------------------------------------------------------*/
void dss_decode(cpu cpu) {
	cpu->stage[decode].status=stage_noAction;
	fetch_register1(cpu);
	fetch_register2(cpu);
	check_dest(cpu);
}

void dsi_decode(cpu cpu) {
	cpu->stage[decode].status=stage_noAction;
	fetch_register1(cpu);
	fetch_register2(cpu);
	check_dest(cpu);
}


void movc_decode(cpu cpu) {
	cpu->stage[decode].status=stage_noAction;
	check_dest(cpu);
}

/*---------------------------------------------------------
  Execute stage functions
---------------------------------------------------------*/
void add_execute(cpu cpu) {
	cpu->stage[execute].result=cpu->stage[execute].op1+cpu->stage[execute].op2;
	reportStage(cpu,execute,"res=%d+%d",cpu->stage[execute].op1,cpu->stage[execute].op2);
	set_conditionCodes(cpu);
}

void addl_execute(cpu cpu) {
	cpu->stage[execute].result=cpu->stage[execute].op1+cpu->stage[execute].imm;
	reportStage(cpu,execute,"res=%d+%d",cpu->stage[execute].op1,cpu->stage[execute].imm);
	set_conditionCodes(cpu);
}

void movc_execute(cpu cpu) {
	cpu->stage[execute].result=cpu->stage[execute].op1;
	reportStage(cpu,execute,"res=%d",cpu->stage[execute].result);
}

/*---------------------------------------------------------
  Memory  stage functions
---------------------------------------------------------*/

/*---------------------------------------------------------
  Writeback stage functions
---------------------------------------------------------*/
void dest_writeback(cpu cpu) {
	int reg=cpu->stage[writeback].dr;
	cpu->reg[reg]=cpu->stage[writeback].result;
	cpu->regValid[reg]=1;
	reportStage(cpu,writeback,"R%02d<-%d",reg,cpu->stage[writeback].result);
}

void halt_writeback(cpu cpu) {
	cpu->stop=1;
	strcpy(cpu->abend,"HALT instruction retired");
	reportStage(cpu,writeback,"cpu stopped");
}

/*---------------------------------------------------------
  Externally available functions
---------------------------------------------------------*/
void registerAllOpcodes() {
	// Invoke registerOpcode for EACH valid opcode here
	registerOpcode(ADD,dss_decode,add_execute,NULL,dest_writeback);
	registerOpcode(ADDL,dsi_decode,addl_execute,NULL,dest_writeback);
	registerOpcode(MOVC,movc_decode,movc_execute,NULL,dest_writeback);
	registerOpcode(HALT,NULL,NULL,NULL,halt_writeback);
}

void registerOpcode(int opNum,
	opStageFn decodeFn,opStageFn executeFn,
	opStageFn memoryFn,opStageFn writebackFn) {

	opFns[decode][opNum]=decodeFn;
	opFns[execute][opNum]=executeFn;
	opFns[memory][opNum]=memoryFn;
	opFns[writeback][opNum]=writebackFn;
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

/*---------------------------------------------------------
  Internal helper functions
---------------------------------------------------------*/
void fetch_register1(cpu cpu) {
	int reg=cpu->stage[decode].sr1;
	if (cpu->regValid[reg]) {
		cpu->stage[decode].op1=cpu->reg[reg];
		reportStage(cpu,decode," R%d=%d",reg,cpu->reg[reg]);
		return;
	}
	// Register value cannot be found
	cpu->stage[decode].status=stage_stalled;
	reportStage(cpu,decode," R%d invalid",reg);
	return;
}

void fetch_register2(cpu cpu) {
	int reg=cpu->stage[decode].sr2;
	if (cpu->regValid[reg]) {
		cpu->stage[decode].op2=cpu->reg[reg];
		reportStage(cpu,decode," R%d=%d",reg,cpu->reg[reg]);
		return;
	}
	// reg2 value cannot be found
	cpu->stage[decode].status=stage_stalled;
	reportStage(cpu,execute," R%d invalid",reg);
}

void check_dest(cpu cpu) {
	int reg=cpu->stage[decode].dr;
	if (!cpu->regValid[reg]) {
		cpu->stage[decode].status=stage_stalled;
		reportStage(cpu,decode," R%d invalid",reg);
	}
	if (cpu->stage[decode].status!=stage_stalled)  {
		 cpu->regValid[cpu->stage[decode].dr]=0;
		 reportStage(cpu,decode," invalidate R%d",reg);
	}
}

void set_conditionCodes(cpu cpu) {
	// Condition codes always set during the execute phase
	if (cpu->stage[execute].result==0) cpu->cc.z=1;
	else cpu->cc.z=0;
	if (cpu->stage[execute].result>0) cpu->cc.p=1;
	else cpu->cc.p=0;
}