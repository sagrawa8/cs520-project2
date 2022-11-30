#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "apexCPU.h"
#include "apexMem.h"

/*---------------------------------------------------------
   Internal function declarations
---------------------------------------------------------*/
void cycle_fetch(cpu cpu);
void cycle_decode(cpu cpu);
void cycle_dispatch(cpu cpu);
void cycle_stage(cpu cpu,int stage);
char * getInum(cpu cpu,int pc);
void reportReg(cpu cpu,int r);
char * statusString(enum stageStatus_enum st);

/*---------------------------------------------------------
   Global Variables
---------------------------------------------------------*/
char *stageName[writeback+1]={"fetch","decode",
	"alu1","alu2","alu3",
	"mul1","mul2","mul3",
	"load1","load2","load3",
	"store1","store2","store3",
	"branch1","branch2","branch3",
	"writeback"};
extern opStageFn opFns[writeback+1][NUMOPS]; // Array of function pointers, one for each stage/opcode combination
enum stage_enum pipeEnd[writeback+1]={
	decode, // For fetch
	decode, // For decode
	fu_alu3, fu_alu3, fu_alu3, // for alu fu
	fu_mul3, fu_mul3, fu_mul3, // for mul fu
	fu_ld3, fu_ld3, fu_ld3, // for load fu
	fu_st3, fu_st3, fu_st3, // for store fu
	fu_br3, fu_br3, fu_br3, // for br fu
	writeback // for writeback
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
	cpu->cc.z=cpu->cc.p=0;
	cpu->t=0;
	cpu->instr_retired=0;
	cpu->halt_fetch=0;
	cpu->stop=0;
	for(enum stage_enum i=fetch;i<=writeback;i++) {
		cpu->stage[i].status=stage_squashed;
		cpu->stage[i].report[0]='\0';
		reportStage(cpu,i,"---");
		cpu->stage[i].instruction=0;
		cpu->stage[i].opcode=0;
		cpu->stage[i].pc=-1;
		cpu->stage[i].branch_taken=0;
		cpu->stage[i].fu=no_fu;
		for(int op=0;op<NUMOPS;op++) opFns[i][op]=NULL;
	}
	for (int c=2; c>=0; c--) cpu->fwdBus[c].valid=0;
	registerAllOpcodes();
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
	for (int s=0;s<=writeback;s++) {
		if (cpu->stage[s].status==stage_noAction) continue;
		if (cpu->stage[s].status==stage_squashed) continue;
		// if (cpu->stage[s].status==stage_stalled) continue;
		printf("  %10s: pc=%05x %s",stageName[s],cpu->stage[s].pc,disassemble(cpu->stage[s].instruction,instBuf));
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

	printf("Forwarding buses:\n");
	for(int c=0;c<3;c++) {
		if (cpu->fwdBus[c].valid) {
			printf("   bus %d: R%d, value=%d\n",c,cpu->fwdBus[c].tag,cpu->fwdBus[c].value);
		}
	}
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

	// First, cycle stage data from FU to WB
	// Resolve which FU forwards to WB (if any)
	cpu->stage[writeback].status=stage_noAction;
	for(enum stage_enum fu=fu_alu3; fu<=fu_br3 && cpu->stage[writeback].status==stage_noAction; fu+=3) {
		if (cpu->stage[fu].status==stage_actionComplete) {
			cpu->stage[writeback]=cpu->stage[fu];
			cpu->stage[writeback].status=stage_ready;
			cpu->stage[fu].status=stage_noAction; // Available for new
		}
	}
	if (cpu->stage[writeback].status==stage_noAction) {
		cpu->stage[writeback].status=stage_stalled; // Nothing is ready to writeback
		cpu->stage[writeback].pc=-1;
	}


	// Next, forward through FU pipelines..
	for(enum fu_enum fs=alu_fu; fs<=br_fu; fs+=3) {
		if (cpu->stage[fs+2].status==stage_noAction ||
				cpu->stage[fs+2].status==stage_squashed) {
			cpu->stage[fs+2]=cpu->stage[fs+1];
			cpu->stage[fs+1]=cpu->stage[fs];
			cpu->stage[fs].status=stage_noAction; // Open up for issue
			cpu->stage[fs].instruction=0;
			cpu->stage[fs].opcode=0;
			cpu->stage[fs].pc=-1;
		}
	}

	cycle_dispatch(cpu); //  dispatch/issue any decoded instruction to an FU
	// Next, cycle stage data from Fetch to Decode and reset Fetch
	if (cpu->stage[decode].status!=stage_stalled) {
		if (cpu->stage[fetch].status==stage_stalled ) {
			cpu->stage[decode].status=stage_squashed;
			cpu->stage[decode].instruction=0;
			cpu->stage[decode].opcode=0;
		} else {
			cpu->stage[decode]=cpu->stage[fetch];
			// Reset the fetch stage
			cpu->stage[fetch].status=stage_squashed; // No valid instruction available yet
			cpu->stage[fetch].instruction=0;
			cpu->stage[fetch].opcode=0;
		} // end of fetch not stalled
	} // end of decode not stalled

	// Move data forward in the forwarding busses
	for (int c=2; c>0;c--) {
		cpu->fwdBus[c]=cpu->fwdBus[c-1]; // Copy all fields forward
	}
	cpu->fwdBus[0].valid=0;
	// NOTE: fowarding bus conflicts due to different FU completion times!
	//	Conflict always "won" by latest instruction in program order
	//   Hence, if a bus is valid, it should not be overwritten by an
	//   instruction which finished later, but was earlier in the program
	//   order.

	// Reset the reports and status as required for all stages
	for(int s=0;s<=writeback;s++) {
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
	if (!cpu->stop) cycle_decode(cpu); // Do the decode part of d/rf
	for(enum fu_enum fu=alu_fu; fu<=br_fu+2; fu++) {
		if (!cpu->stop) cycle_stage(cpu,fu);
	}


	if (!cpu->stop && cpu->stage[writeback].status==stage_ready) cycle_stage(cpu,writeback);

	if (!cpu->stop) cycle_stage(cpu,decode); // Do the rf part of d/rf

	cpu->t++; // update the clock tick - This cycle has completed
	if (cpu->t==1) {
		printf("      |ftch|deco|alu1|alu2|alu3|mul1|mul2|mul3|lod1|lod2|lod3|sto1|sto2|sto3|br1 |br2 |br3 | wb |\n");
	}

	// Report on all stages (move this before cycling the rf part of decode to match Kanad's results)
	printf ("t=%3d |",cpu->t);
	for(int s=0;s<=writeback;s++) {
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
	for(int s=1;s<5;s++) if (cpu->stage[s].status==stage_stalled) return;
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
	if (cpu->stage[decode].status==stage_squashed) return;
	if (cpu->stage[decode].status==stage_stalled) return; // Decode already done
	enum opFormat_enum fmt=opInfo[cpu->stage[decode].opcode].format;
	int inst=cpu->stage[decode].instruction;
	cpu->stage[decode].op1Valid=1;
	cpu->stage[decode].op2Valid=1;
	switch(fmt) {
		case fmt_nop:
			reportStage(cpu,decode,"decode(nop)");
			cpu->stage[decode].fu=alu_fu;
			cpu->stage[decode].status=stage_actionComplete;
			break; // No decoding required
		case fmt_dss:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].op1Valid=0;
			cpu->stage[decode].sr2=(inst&0x0000f000)>>12;
			cpu->stage[decode].op2Valid=0;
			cpu->stage[decode].fu=alu_fu;
			if (cpu->stage[decode].opcode==MUL) cpu->stage[decode].fu=mult_fu;
			reportStage(cpu,decode,"decode(dss)");
			break;
		case fmt_dsi:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].op1Valid=0;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode].op2=cpu->stage[decode].imm;
			cpu->stage[decode].op2Valid=1;
			if (cpu->stage[decode].opcode==LOAD) cpu->stage[decode].fu=load_fu;
			else cpu->stage[decode].fu=alu_fu;
			reportStage(cpu,decode,"decode(dsi) op2=%d",cpu->stage[decode].op2);
			break;
		case fmt_di:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16; // Shift left/right to propagate sign bit
			cpu->stage[decode].op1=cpu->stage[decode].imm;
			cpu->stage[decode].op1Valid=1;
			cpu->stage[decode].op2Valid=1;
			cpu->stage[decode].fu=alu_fu;
			reportStage(cpu,decode,"decode(di) op1=%d",cpu->stage[decode].op1);
			break;
		case fmt_ssi:
			cpu->stage[decode].sr1=(inst&0x00f00000)>>20;
			cpu->stage[decode].op1Valid=0;
			cpu->stage[decode].sr2=(inst&0x000f0000)>>16;
			cpu->stage[decode].op2Valid=0;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode].fu=store_fu;
			reportStage(cpu,decode,"decode(ssi) imm=%d",cpu->stage[decode].imm);
			break;
		case fmt_ss:
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].op1Valid=0;
			cpu->stage[decode].sr2=(inst&0x0000f000)>>12;
			cpu->stage[decode].op2Valid=0;
			reportStage(cpu,decode,"decode(ss)");
			cpu->stage[decode].fu=alu_fu;
			break;
		case fmt_off:
			cpu->stage[decode].offset=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode].op1Valid=1;
			cpu->stage[decode].op2Valid=1;
			reportStage(cpu,decode,"decode(off)");
			cpu->stage[decode].fu=br_fu;
			break;
		default :
			cpu->stop=1;
			sprintf(cpu->abend,"Decode format %d not recognized in cycle_decode",fmt);
	}
}

void cycle_dispatch(cpu cpu) {
	if (cpu->stage[decode].status!=stage_actionComplete) return;
	// Move decoded instruction onto fu
	enum fu_enum fu=cpu->stage[decode].fu;
	if (cpu->stage[fu].status==stage_noAction ||
			cpu->stage[fu].status==stage_squashed ||
			cpu->stage[fu].status==stage_actionComplete) {
		// fu is available...
		// Don't issue until operands are available
		if (cpu->stage[decode].op1Valid==0) {
			reportStage(cpu,decode,"Waiting for first operand");
			cpu->stage[decode].status=stage_stalled;
			return;
		}
		if (cpu->stage[decode].op2Valid==0) {
			reportStage(cpu,decode,"Waiting for second operand");
			cpu->stage[decode].status=stage_stalled;
			return;
		}
		// Operands are valid... issue to fu
		cpu->stage[fu]=cpu->stage[decode];
		cpu->stage[fu].status=stage_ready;
		cpu->stage[fu].report[0]='\0';
		reportStage(cpu,decode," issued to fu %d",fu);
		cpu->stage[decode].status=stage_actionComplete;
	} else {
		// fu is not available, issue needs to stall
		reportStage(cpu,decode,"Required FU busy, status=%s",
			statusString(cpu->stage[fu].status));
		cpu->stage[decode].status=stage_stalled;
	}
}

void cycle_stage(cpu cpu,int stage) {
	for(enum stage_enum s=stage+1;s<=pipeEnd[stage];s++) {
		if (cpu->stage[s].status==stage_stalled) return; // downstream is stalled
	}
	if (cpu->stage[stage].status==stage_squashed) return;
	assert(stage>=fetch && stage<=writeback);
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
	if (stage==writeback && cpu->stage[writeback].status!=stage_squashed) {
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