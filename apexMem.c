#include "apexMem.h"
#include <string.h>
#include <stdio.h>

int ifetch(cpu cpu) {
	int addr=cpu->pc;
	int idx=(addr-0x4000)/4;
	if (idx<0 || idx>=cpu->numInstructions || 0!=addr%4) {
		cpu->stop=1;
		sprintf(cpu->abend,"Segmentation violation in ifetch pc=%08x",addr);
		return 0;
	}
	return cpu->codeMem[idx];
}

int dfetch(cpu cpu,int addr) {
	int idx=addr/4;
	if (idx<0 || idx>127 || 0!=addr%4) {
		cpu->halt_fetch=cpu->stop=1;
		strcpy("Segmentation violation in dfetch",cpu->abend);
		return 0;
	}
	if (idx<cpu->lowMem) {
		cpu->lowMem=idx;
		printf("Warning... accessing uninitialized memory at address %08x\n",addr);
		cpu->dataMem[idx]=0;
	}
	if (idx>cpu->highMem) {
		cpu->highMem=idx;
		printf("Warning... accessing uninitialized memory at address %08x\n",addr);
		cpu->dataMem[idx]=0;
	}
	return cpu->dataMem[idx];
}

void dstore(cpu cpu,int addr,int value) {
	int idx=addr/4;
	if (idx<0 || idx>127) {
		cpu->halt_fetch=cpu->stop=1;
		sprintf(cpu->abend,"dstore segmentation violation - invalid address=%08x",addr);
		return;
	}
	if (0!=addr%4) {
		cpu->stop=1;
		sprintf(cpu->abend,"dstore segmentation violation bad boundary address=%08x",addr);
		return;
	}
	if (idx<cpu->lowMem) cpu->lowMem=idx;
	if (idx>cpu->highMem) cpu->highMem=idx;
	cpu->dataMem[idx]=value;
}