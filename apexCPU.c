#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "apexCPU.h"
#include "apexMem.h"
#include "apexOpcodes.h"
#include "rob.c" 
#include "lsq.c"
#include "free_prf.c"
#include "issue.c"

/*---------------------------------------------------------
   Internal function declarations
---------------------------------------------------------*/
void cycle_fetch(cpu cpu);
void cycle_decode(cpu cpu);
void dispatchToIssue(cpu cpu);
void cycle_stage(cpu cpu,int stage);
char * getInum(cpu cpu,int pc);
void reportReg(cpu cpu,int r);
char * statusString(enum stageStatus_enum st);

/*---------------------------------------------------------
   Global Variables
---------------------------------------------------------*/
char *stageName[retire+1]={"fetch","dec_ren1","ren2_disp","issue",
	"alu",
	"mul1","mul2","mul3",
	"lsa",
	"br",
	"retire"};

extern opStageFn opFns[retire+1][NUMOPS]; // Array of function pointers, one for each stage/opcode combination
enum stage_enum pipeEnd[retire+1]={
	fetch, 
	decode_rename1,
	rename2_dispatch,
	issue_instruction,
	fu_alu,
	fu_mul3, fu_mul3, fu_mul3,
	fu_lsa,
	fu_br,
	retire
};

/*---------------------------------------------------------
   External Function definitions
---------------------------------------------------------*/

void initCPU(cpu cpu) {
	cpu->pc=0x4000;
	cpu->numInstructions=0;
	cpu->lowMem=128;
	cpu->highMem=-1;
	for(int i=0;i<16;i++) {
		cpu->reg[i]=0xdeadbeef;
		cpu->regValid[i]=1; // all registers start out as "valid"
	}
	cpu->cc.z=cpu->cc.p = 0;
	
	cpu->t=0;
	cpu->instr_retired=0;
	cpu->halt_fetch=0;
	cpu->stop=0;
	for(enum stage_enum i=fetch;i<=retire;i++) {
		cpu->stage[i].status=stage_squashed;
		cpu->stage[i].report[0]='\0';
		reportStage(cpu,i,"---");
		cpu->stage[i].instruction=0;
		cpu->stage[i].opcode=0;
		cpu->stage[i].pc=-1;
		cpu->stage[i].branch_taken=0;
		//cpu->stage[i].fu=no_fu;
		for(int op=0;op<NUMOPS;op++) opFns[i][op]=NULL;
	}
	registerAllOpcodes();
	for(int i=0;i<32;i++){
		enqueue(i);
	}
	for(int i=0;i<16;i++){
		cpu->rat[i].arf = i;
		cpu->rat[i].valid = 0;
		cpu->rat[i].prf = -1;
	}
	for(int i=0;i<32;i++){
		cpu->prf[i].prf = i;
		cpu->prf[i].valid = 0;
		cpu->prf[i].value = -1;
		cpu->prf[i].z = -1;
		cpu->prf[i].p = -1;
	}
	for(int i=0;i<32;i++){
		cpu->iq[i].free=-1;
		cpu->iq[i].opcode=0;
		cpu->iq[i].fu=-1;
		cpu->iq[i].src1_valid=0;
		cpu->iq[i].src1_tag=-1;
		cpu->iq[i].src1_value=-1;
		cpu->iq[i].src2_valid=0;
		cpu->iq[i].src2_tag=-1;
		cpu->iq[i].src2_value=-1;
		cpu->iq[i].lsq_prf=-1;
		cpu->iq[i].dest=-1;
	}
}

void loadCPU(cpu cpu,char * objFileName) {
	char cmtBuf[128];
	FILE * objF=fopen(objFileName,"r");
	if (objF==NULL) {
		perror("Error - unable to open object file for read");
		printf("...Trying to read from object file %s\n",objFileName);
		return;
	}

	int nread=0;
	while(!feof(objF)) {
		int newInst;
		if (1==fscanf(objF," %08x",&newInst)) {
			cpu->codeMem[nread++]=newInst;
		} else if (1==fscanf(objF,"; %127[^\n]\n",cmtBuf)) {
			// Ignore comments on the same line
			// printf("Ignoring commment: %s\n",cmtBuf);
		} else {
			fscanf(objF," %s ",cmtBuf);
			printf("Load aborted, unrecognized object code: %s\n",cmtBuf);
			return;
		}
	}
	cpu->numInstructions=nread;
	cpu->pc=0x4000;
	cpu->halt_fetch=cpu->stop=0;
	printf("Loaded %d instructions starting at adress 0x4000\n",nread);
}

void printState(cpu cpu) {

	printf("\nCPU state at cycle %d, pc=0x%08x, cc.z=%s cc.p=%s\n",
		cpu->t,cpu->pc,cpu->cc.z?"true":"false",cpu->cc.p?"true":"false");

	printf("Stage Info:\n");
	char instBuf[32];
	for (int s=0;s<=retire;s++) {
		if (cpu->stage[s].status==stage_noAction) continue;
		if (cpu->stage[s].status==stage_squashed) continue;
		// if (cpu->stage[s].status==stage_stalled) continue;
		printf("  %10s: ",stageName[s]);
		if (cpu->stage[s].pc!=-1)
			printf("pc=%05x %s %s",cpu->stage[s].pc,getInum(cpu,cpu->stage[s].pc),disassemble(cpu->stage[s].instruction,instBuf));
		else printf("pc=-1");
		//f (cpu->stage[s].status==stage_squashed) printf(" squashed");
		//if (cpu->stage[s].status==stage_stalled) printf(" stalled");
		printf(" %s\n",cpu->stage[s].report);
	}

   printf("\n Int Regs: \n   ");
   for(int r=0;r<16;r++) reportReg(cpu,r);
	printf("\n");

	if ((cpu->lowMem<128) && (cpu->highMem>-1)) {
		printf("Modified memory:\n");
		for(int i=cpu->lowMem;i<=cpu->highMem;i++) {
			printf("MEM[%04x]=%d\n",i*4,cpu->dataMem[i]);
		}
		printf("\n");
	}


	printf("\n");

	printf("  Reorder Buffer: ");
	displayROB();

	printf("\n");

	show();

	printf("\n");
	printf(" Rename Table: ");
	printf("\n");
	printf("|%5s", "arf");
    printf("|%5s", "valid");
    printf("|%5s", "prf");
    printf("|\n");
	int array_length = sizeof(cpu->rat)/sizeof(cpu->rat[0]);
	for(int i=0;i<10;i++) {
		printf("|   R%d", cpu->rat[i].arf);
		printf("|%5d", cpu->rat[i].valid);
		printf("|%5d", cpu->rat[i].prf);
    	printf("|\n");
		}
	for(int i=10;i<array_length;i++) {
		printf("|  R%d", cpu->rat[i].arf);
		printf("|%5d", cpu->rat[i].valid);
		printf("|%5d", cpu->rat[i].prf);
    	printf("|\n");
		}

	printf("\n");
	printf(" PRF Table: ");
	printf("\n");
	printf("|%5s", "prf");
    printf("|%5s", "valid");
    printf("|%5s", "value");
	printf("|%5s", "z");
    printf("|%5s", "p");
    printf("|\n");
	array_length = sizeof(cpu->prf)/sizeof(cpu->prf[0]);
	for(int i=0;i<10;i++) {
		printf("|   P%d", cpu->prf[i].prf);
		printf("|%5d", cpu->prf[i].valid);
		printf("|%5d", cpu->prf[i].value);
		printf("|%5d", cpu->prf[i].z);
		printf("|%5d", cpu->prf[i].p);
    	printf("|\n");
		}
	for(int i=10;i<array_length;i++) {
		printf("|  P%d", cpu->prf[i].prf);
		printf("|%5d", cpu->prf[i].valid);
		printf("|%5d", cpu->prf[i].value);
		printf("|%5d", cpu->prf[i].z);
		printf("|%5d", cpu->prf[i].p);
    	printf("|\n");
		}

	printf("\n");
	printf(" Instruction Queue: ");
	printf("\n");
	printf("|%10s", "free");
    printf("|%10s", "opcode");
    printf("|%10s", "fu");
	printf("|%10s", "src1_valid");
    printf("|%10s", "src1_tag");
	printf("|%10s", "src1_value");
	printf("|%10s", "src2_valid");
    printf("|%10s", "src2_tag");
	printf("|%10s", "src2_value");
	printf("|%10s", "L/P");
	printf("|%10s", "dest");
    printf("|\n");
	array_length = sizeof(cpu->iq)/sizeof(cpu->iq[0]);
	for(int i=0;i<array_length;i++) {
		printf("|%10d", cpu->iq[i].free);
		printf("|%10d", cpu->iq[i].opcode);
		printf("|%10d", cpu->iq[i].fu);
		printf("|%10d", cpu->iq[i].src1_valid);
		printf("|%10d", cpu->iq[i].src1_tag);
		printf("|%10d", cpu->iq[i].src1_value);
		printf("|%10d", cpu->iq[i].src2_valid);
		printf("|%10d", cpu->iq[i].src2_tag);
		printf("|%10d", cpu->iq[i].src2_value);
		printf("|%10d", cpu->iq[i].lsq_prf);
		printf("|%10d", cpu->iq[i].dest);
    	printf("|\n");
		}
	
	printf("\n");
	printf("  Load/Store Queue: ");
	displayLSQ();
	printf("\n");

	printf("  Issue Queue: ");
	printf("\n");
	displayIssueQueue();
	printf("\n");
	

	if (cpu->halt_fetch) {
		printf("Instruction fetch is halted.\n");
	}
	if (cpu->stop) {
		printf("CPU is stopped because %s\n",cpu->abend);
	}
}

void cycleCPU(cpu cpu) {
	if (cpu->stop) {
		printf("CPU is stopped for %s. No cycles allowed.\n",cpu->abend);
		return;
	}
	/*for(int i=0;i<32;i++) {
		if(cpu->prf[i].prf == rob_queue[front_rob].dest_prf && cpu->prf[i].valid == 1) {
			cpu->stage[retire].dr = cpu->prf[i].prf;
			cpu->stage[retire].result = cpu->prf[i].value;
			cpu->stage[retire].status=stage_ready;
		}
	}*/

    cpu->stage[retire]=cpu->stage[fu_alu];
	cpu->stage[fu_alu]=cpu->stage[issue_instruction];
	//cpu->stage[mult_fu]=cpu->stage[issue_instruction];
	//cpu->stage[store_fu]=cpu->stage[issue_instruction];
	//cpu->stage[load_fu]=cpu->stage[issue_instruction];
	//cpu->stage[br_fu]=cpu->stage[issue_instruction];
	cpu->stage[issue_instruction]=cpu->stage[rename2_dispatch];
	cpu->stage[rename2_dispatch]=cpu->stage[decode_rename1];


	if (cpu->stage[decode_rename1].status!=stage_stalled) {
		if (cpu->stage[fetch].status==stage_stalled ) {
			cpu->stage[decode_rename1].status=stage_squashed;
			cpu->stage[decode_rename1].instruction=0;
			cpu->stage[decode_rename1].opcode=0;
		} else {
			cpu->stage[decode_rename1]=cpu->stage[fetch];
			cpu->stage[fetch].status=stage_squashed;
			cpu->stage[fetch].instruction=0;
			cpu->stage[fetch].opcode=0;
		}
	}
	

	// Reset the reports and status as required for all stages
	for(int s=0;s<=decode_rename1;s++) {
		cpu->stage[s].report[0]='\0';
		switch (cpu->stage[s].status) {
			case stage_squashed:
			case stage_stalled:
			case stage_noAction:
			case stage_ready:
				break; // No change required
			case stage_actionComplete:
				cpu->stage[s].status=stage_noAction; // Overwrite previous stages status
		}
	}

	if (!cpu->stop) cycle_fetch(cpu);
	if (!cpu->stop) cycle_decode(cpu);
	if (!cpu->stop) cycle_stage(cpu,rename2_dispatch);
	if (!cpu->stop) cycle_stage(cpu,issue_instruction);
	if (!cpu->stop) cycle_stage(cpu,fu_alu);
	//if (!cpu->stop) cycle_stage(cpu,mult_fu);
	//if (!cpu->stop) cycle_stage(cpu,load_fu);
	//if (!cpu->stop) cycle_stage(cpu,store_fu);
	//if (!cpu->stop) cycle_stage(cpu,br_fu);
	if (!cpu->stop) cycle_stage(cpu,retire);

	if (!cpu->stop) cycle_stage(cpu,decode_rename1);
	
	 // Do the rf part of d/rf

	cpu->t++; // update the clock tick - This cycle has completed
	if (cpu->t==1) {
		printf("      |ftch|deco|alu1|alu2|alu3|mul1|mul2|mul3|lod1|lod2|lod3|sto1|sto2|sto3|br1 |br2 |br3 | wb |\n");
	}

	// Report on all stages (move this before cycling the rf part of decode to match Kanad's results)
	printf ("t=%3d |",cpu->t);
	for(int s=0;s<=retire;s++) {
		int stalled=0;
		for(int f=s;f<=pipeEnd[s];f++) if (cpu->stage[f].status==stage_stalled) stalled=1;
		if (stalled) printf ("%3ss|", getInum(cpu,cpu->stage[s].pc));
		else {
			switch(cpu->stage[s].status) {
				case stage_squashed: printf("   q|"); break;
				case stage_stalled: break; // printed stalled above
				case stage_noAction: printf ("%3s-|", getInum(cpu,cpu->stage[s].pc)); break;
				case stage_ready: printf ("%3sr|", getInum(cpu,cpu->stage[s].pc)); break;
				case stage_actionComplete: printf("%3s+|", getInum(cpu,cpu->stage[s].pc)); break;
			}
		}

	}
	printf("\n");

	// if (!cpu->stop) cycle_stage(cpu,decode); // Do the rf part of d/rf

	if (cpu->stop) {
		printf("CPU stopped because %s\n",cpu->abend);
	}
}

void printStats(cpu cpu) {
	printf("\nAPEX Simulation complete.\n");
	printf("    Total cycles executed: %d\n",cpu->t);
	printf("    Instructions retired: %d\n",cpu->instr_retired);
	printf("    Instructions per Cycle (IPC): %5.3f\n",((float)cpu->instr_retired)/cpu->t);
	printf("    Stop is %s\n",cpu->stop?"true":"false");
	if (cpu->stop) {
		printf("    Reason for stop: %s\n",cpu->abend);
	}
}

void reportStage(cpu cpu,enum stage_enum s,const char* fmt,...) {
	char msgBuf[1024]={0};
	va_list args;
	va_start(args,fmt);
	vsprintf(msgBuf,fmt,args);
	va_end(args);
	// Truncate msgBuf to fit
	if (strlen(msgBuf)+strlen(cpu->stage[s].report) >=128) {
		msgBuf[127-strlen(cpu->stage[s].report)]='\0';
	}
	strcat(cpu->stage[s].report,msgBuf);
}

/*---------------------------------------------------------
   Internal Function definitions
---------------------------------------------------------*/

void cycle_fetch(cpu cpu) {
	// Don't run if anything downstream is stalled
	for(int s=1;s<=retire;s++) if (cpu->stage[s].status==stage_stalled) return;
	if (cpu->halt_fetch) {
		cpu->stage[fetch].status=stage_squashed;
		cpu->stage[fetch].instruction=0;
		cpu->stage[fetch].opcode=0;
		return;
	}
	int inst=ifetch(cpu);
	if (!cpu->stop) {
		cpu->stage[fetch].status=stage_noAction;
		cpu->stage[fetch].instruction=inst;
		cpu->stage[fetch].opcode=(inst>>24);
		if (cpu->stage[fetch].opcode<0 || cpu->stage[fetch].opcode>HALT) {
			cpu->stop=1;
			sprintf(cpu->abend,"Invalid opcode %x after ifetch(%08x)",
				cpu->stage[fetch].opcode,cpu->pc);
			return;
		}
		reportStage(cpu,fetch,"ifetch %s",getInum(cpu,cpu->pc));
		if (cpu->stage[fetch].opcode==HALT) {
			cpu->halt_fetch=1; // Stop fetching when the HALT instruction is fetched
			reportStage(cpu,fetch," --- fetch halted");
		}
		cpu->stage[fetch].pc=cpu->pc;
		cpu->stage[fetch].status=stage_actionComplete;
		cpu->pc+=4;
	}
}

void cycle_decode(cpu cpu) {
	// Does the first half (the decode part) of the decode/fetch regs stage
	if (cpu->stage[decode_rename1].status==stage_squashed) return;
	//if (cpu->stage[decode_rename1].status==stage_stalled) return; // Decode already done
	enum opFormat_enum fmt=opInfo[cpu->stage[decode_rename1].opcode].format;
	int inst=cpu->stage[decode_rename1].instruction;
	cpu->stage[decode_rename1].op1Valid=1;
	cpu->stage[decode_rename1].op2Valid=1;
	switch(fmt) {
		case fmt_nop:
			reportStage(cpu,decode_rename1,"decode(nop)");
			cpu->stage[decode_rename1].fu=fu_alu;
			cpu->stage[decode_rename1].status=stage_actionComplete;
			break; // No decoding required
		case fmt_dss:
			cpu->stage[decode_rename1].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode_rename1].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode_rename1].op1Valid=0;
			cpu->stage[decode_rename1].sr2=(inst&0x0000f000)>>12;
			cpu->stage[decode_rename1].op2Valid=0;
			cpu->stage[decode_rename1].fu=fu_alu;
			if (cpu->stage[decode_rename1].opcode==MUL) cpu->stage[decode_rename1].fu=fu_mul1;
			reportStage(cpu,decode_rename1,"decode(dss)");
			break;
		case fmt_dsi:
			cpu->stage[decode_rename1].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode_rename1].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode_rename1].op1Valid=0;
			cpu->stage[decode_rename1].imm=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode_rename1].op2=cpu->stage[decode_rename1].imm;
			cpu->stage[decode_rename1].op2Valid=1;
			if (cpu->stage[decode_rename1].opcode==LOAD) cpu->stage[decode_rename1].fu=fu_lsa;
			else cpu->stage[decode_rename1].fu=fu_alu;
			reportStage(cpu,decode_rename1,"decode(dsi) op2=%d",cpu->stage[decode_rename1].op2);
			break;
		case fmt_di:
			cpu->stage[decode_rename1].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode_rename1].imm=((inst&0x0000ffff)<<16)>>16; // Shift left/right to propagate sign bit
			cpu->stage[decode_rename1].op1=cpu->stage[decode_rename1].imm;
			cpu->stage[decode_rename1].op1Valid=1;
			cpu->stage[decode_rename1].op2Valid=1;
			cpu->stage[decode_rename1].fu=fu_alu;
			reportStage(cpu,decode_rename1,"decode(di) op1=%d",cpu->stage[decode_rename1].op1);
			break;
		case fmt_ssi:
			cpu->stage[decode_rename1].sr1=(inst&0x00f00000)>>20;
			cpu->stage[decode_rename1].op1Valid=0;
			cpu->stage[decode_rename1].sr2=(inst&0x000f0000)>>16;
			cpu->stage[decode_rename1].op2Valid=0;
			cpu->stage[decode_rename1].imm=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode_rename1].fu=fu_lsa;
			reportStage(cpu,decode_rename1,"decode(ssi) imm=%d",cpu->stage[decode_rename1].imm);
			break;
		case fmt_ss:
			cpu->stage[decode_rename1].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode_rename1].op1Valid=0;
			cpu->stage[decode_rename1].sr2=(inst&0x0000f000)>>12;
			cpu->stage[decode_rename1].op2Valid=0;
			reportStage(cpu,decode_rename1,"decode(ss)");
			cpu->stage[decode_rename1].fu=fu_alu;
			break;
		case fmt_off:
			cpu->stage[decode_rename1].offset=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode_rename1].op1Valid=1;
			cpu->stage[decode_rename1].op2Valid=1;
			reportStage(cpu,decode_rename1,"decode(off)");
			cpu->stage[decode_rename1].fu=fu_br;
			break;
		default :
			cpu->stop=1;
			sprintf(cpu->abend,"Decode format %d not recognized in cycle_decode",fmt);
	}
}


void cycle_stage(cpu cpu,int stage) {
	for(enum stage_enum s=stage+1;s<=pipeEnd[stage];s++) {
		if (cpu->stage[s].status==stage_stalled) return; // downstream is stalled
	}
	if (cpu->stage[stage].status==stage_squashed) return;
	assert(stage>=fetch && stage<=retire);
	assert(cpu->stage[stage].opcode>=0 && cpu->stage[stage].opcode<=HALT);
	opStageFn stageFn=opFns[stage][cpu->stage[stage].opcode];
	if (stageFn) {
		stageFn(cpu);
		if (cpu->stage[stage].status==stage_noAction ||
				cpu->stage[stage].status==stage_ready)
			cpu->stage[stage].status=stage_actionComplete;
	} // else {
		// cpu->stage[stage].status=stage_noAction;
	// }
	if (stage==retire && cpu->stage[retire].status!=stage_squashed) {
		cpu->instr_retired++;
	}
}

char * getInum(cpu cpu,int pc) {
	static char inumBuf[5];
	inumBuf[0]=0x00;
	if (pc==-1) return inumBuf;
	int n=(pc-0x4000)/4;
	if (n<0 || n>cpu->numInstructions) {
		cpu->stop=1;
		sprintf(cpu->abend,"in getInum, pc was %x\n",pc);
		n=-1;
	}
	sprintf(inumBuf,"I%d",n);
	return inumBuf;
}

void reportReg(cpu cpu,int r) {
	int v=cpu->reg[r];
	printf("R%02d",r);
	if (cpu->regValid[r]) {
		if (v!=0xdeadbeef) printf("=%05d ",v);
	   else printf(" ----- ");
	} else printf(" xxxxx ");
	if (7==r%8) printf("\n   ");
}

char * statusString(enum stageStatus_enum st) {
	switch(st) {
		case stage_squashed: return "squashed";
		case stage_stalled: return "stalled";
		case stage_noAction: return "no action";
		case stage_ready: return "ready";
		case stage_actionComplete: return "action complete";
	}
	return "???";
}