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
void cycle_stage(cpu cpu,int stage);
char * getInum(cpu cpu,int pc);
void reportReg(cpu cpu,int r);

/*---------------------------------------------------------
   Global Variables
---------------------------------------------------------*/
char *stageName[5]={"fetch","decode","execute","memory","writeback"};
extern opStageFn opFns[5][NUMOPS]; // Array of function pointers, one for each stage/opcode combination

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
	for(int i=0;i<5;i++) {
		cpu->stage[i].status=stage_squashed;
		cpu->stage[i].report[0]='\0';
		reportStage(cpu,i,"---");
		cpu->stage[i].instruction=0;
		cpu->stage[i].opcode=0;
		cpu->stage[i].pc=-1;
		for(int op=0;op<NUMOPS;op++) opFns[i][op]=NULL;
	}
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
	for (int s=0;s<5;s++) {
		printf("  %10s: pc=%05x %s",stageName[s],cpu->stage[s].pc,disassemble(cpu->stage[s].instruction,instBuf));
		if (cpu->stage[s].status==stage_squashed) printf(" squashed");
		if (cpu->stage[s].status==stage_stalled) printf(" stalled");
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

	// Move register information down one stage
	//    backwards so that you don't overwrite
	if (cpu->stage[writeback].status==stage_stalled) {
		cpu->stop=1;
		strcpy(cpu->abend,"Writeback stalled - no progress possible");
	} else {
		if (cpu->stage[memory].status==stage_stalled) {
			cpu->stage[writeback].status=stage_squashed;
			cpu->stage[writeback].instruction=0;
			cpu->stage[writeback].opcode=0;
		} else {
			cpu->stage[writeback]=cpu->stage[memory];
			if (cpu->stage[execute].status==stage_stalled) {
				cpu->stage[memory].status=stage_squashed;
				cpu->stage[memory].instruction=0;
				cpu->stage[memory].opcode=0;
			} else {
				cpu->stage[memory]=cpu->stage[execute];
				if (cpu->stage[decode].status==stage_stalled) {
					cpu->stage[execute].status=stage_squashed;
					cpu->stage[execute].instruction=0;
					cpu->stage[execute].opcode=0;
				} else {
					cpu->stage[execute]=cpu->stage[decode];
					if (cpu->stage[fetch].status==stage_stalled) {
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
			} // end of execute not stalled
		} // end of memory not stalled
	} // end of writeback not stalled

	// Reset the reports and status as required for all stages
	for(int s=0;s<5;s++) {
		cpu->stage[s].report[0]='\0';
		switch (cpu->stage[s].status) {
			case stage_squashed:
			case stage_stalled:
			case stage_noAction:
				break; // No change required
			case stage_actionComplete:
				cpu->stage[s].status=stage_noAction; // Overwrite previous stages status
		}
	}

	// Cycle all five stages
	if (!cpu->stop) cycle_fetch(cpu);
	if (!cpu->stop) cycle_decode(cpu); // Do the decode part of d/rf
	if (!cpu->stop) cycle_stage(cpu,execute);
	if (!cpu->stop) cycle_stage(cpu,memory);
	if (!cpu->stop) cycle_stage(cpu,writeback);

	if (!cpu->stop) cycle_stage(cpu,decode); // Do the rf part of d/rf

	cpu->t++; // update the clock tick - This cycle has completed

	// Report on all five stages (move this before cycling the rf part of decode to match Kanad's results)
	printf ("t=%3d |",cpu->t);
	for(int s=0;s<5;s++) {
		int stalled=0;
		for(int f=s;f<5;f++) if (cpu->stage[f].status==stage_stalled) stalled=1;
		if (stalled) printf (" %3s st |", getInum(cpu,cpu->stage[s].pc));
		else {
			switch(cpu->stage[s].status) {
				case stage_squashed: printf("     sq |"); break;
				case stage_stalled: break; // printed stalled above
				case stage_noAction: printf (" %3s -  |", getInum(cpu,cpu->stage[s].pc)); break;
				case stage_actionComplete: printf(" %3s +  |", getInum(cpu,cpu->stage[s].pc)); break;
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
	switch(fmt) {
		case fmt_nop:
			reportStage(cpu,decode,"decode(nop)");
			break; // No decoding required
		case fmt_dss:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].sr2=(inst&0x0000f000)>>12;
			reportStage(cpu,decode,"decode(dss)");
			break;
		case fmt_dsi:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16;
			cpu->stage[decode].op2=cpu->stage[decode].imm;
			reportStage(cpu,decode,"decode(dsi) op2=%d",cpu->stage[decode].op2);
			break;
		case fmt_di:
			cpu->stage[decode].dr=(inst&0x00f00000)>>20;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16; // Shift left/right to propagate sign bit
			cpu->stage[decode].op1=cpu->stage[decode].imm;
			reportStage(cpu,decode,"decode(di) op1=%d",cpu->stage[decode].op1);
			break;
		case fmt_ssi:
			cpu->stage[decode].sr2=(inst&0x00f00000)>>20;
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].imm=((inst&0x0000ffff)<<16)>>16;
			reportStage(cpu,decode,"decode(ssi) imm=%d",cpu->stage[decode].imm);
			break;
		case fmt_ss:
			cpu->stage[decode].sr1=(inst&0x000f0000)>>16;
			cpu->stage[decode].sr2=(inst&0x0000f000)>>12;
			reportStage(cpu,decode,"decode(ss)");
			break;
		case fmt_off:
			cpu->stage[decode].offset=((inst&0x0000ffff)<<16)>>16;
			reportStage(cpu,decode,"decode(off)");
			break;
		default :
			cpu->stop=1;
			sprintf(cpu->abend,"Decode format %d not recognized in cycle_decode",fmt);
	}
}

void cycle_stage(cpu cpu,int stage) {
	for(int s=stage+1;s<5;s++) if (cpu->stage[s].status==stage_stalled) return;
	if (cpu->stage[stage].status==stage_squashed) return;
	assert(stage>=0 && stage<=writeback);
	assert(cpu->stage[stage].opcode>=0 && cpu->stage[stage].opcode<=HALT);
	opStageFn stageFn=opFns[stage][cpu->stage[stage].opcode];
	if (stageFn) {
		stageFn(cpu);
		if (cpu->stage[stage].status==stage_noAction)
			cpu->stage[stage].status=stage_actionComplete;
	} else {
		cpu->stage[stage].status=stage_noAction;
	}
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
