#ifndef APEXCPU_H // Guard against recursive includes
#define APEXCPU_H
#include <stdarg.h>
// To enable reportStage

enum stageStatus_enum {
	stage_squashed,
	stage_stalled,
	stage_noAction,
	stage_ready,
	stage_actionComplete
};

enum stage_enum {
	fetch,
	decode_rename1,
	rename2_dispatch,
	issue_instruction,
	fu_alu,
	fu_mul1,fu_mul2,fu_mul3,
	fu_lsa,
	fu_br,
	retire
};


struct apexStage_struct {
	int pc;
	int instruction;
	int opcode;
	enum stage_enum fu;
	int dr;
	int sr1;
	int sr2;
	int imm;
	int offset;
	int op1Valid;
	int op1;
	int op2Valid;
	int op2;
	int result;
	int effectiveAddr;
	char report[128];
	enum stageStatus_enum status;
	int branch_taken;
};

struct CC_struct {
		int z;
		int p;
		int prf;
	};

struct iq {
	int free;
	int opcode;
	enum stage_enum fu;
	int src1_valid;
  	int src1_tag;
  	int src1_value;
  	int src2_valid;
  	int src2_tag;
  	int src2_value;
  	int lsq_prf;
  	int dest;
	int value;
};


struct rat_table {
	int arf;
	int valid;
  	int prf;
};

struct prf_table {
	int prf;
	int valid;
  	int value;
	int z;
	int p;
};

struct apexCPU_struct {
	int pc;
	int reg[16];
	int regValid[16];
	struct CC_struct cc;
	struct apexStage_struct stage[retire+1];
	int codeMem[128]; // addresses 0x4000 - 0x4200
	int dataMem[128]; // addresses 0x0000 - 0x0200
	int lowMem;
	int highMem;
	int t;
	int numInstructions;
	int instr_retired;
	int halt_fetch;
	int stop;
	char abend[64];
	struct iq iq[32];
	struct rat_table rat[16];
	struct prf_table prf[32];
	int cc_set;
	int pc_set;
};
typedef struct apexCPU_struct * cpu;

#define isFU(x) (x>=fu_alu1 && x<=fu_br3)

extern char *stageName[retire+1]; // defined/initialized in apexCPU.c
typedef void (*opStageFn)(cpu cpu); // Needed in apexOpcodes.h

#include "apexOpcodes.h"


void initCPU(cpu cpu);
void loadCPU(cpu cpu,char * objFileName);
void printState(cpu cpu);
void cycleCPU(cpu cpu);
void printStats(cpu cpu);
void reportStage(cpu cpu,enum stage_enum s,const char* fmt,...);

#endif