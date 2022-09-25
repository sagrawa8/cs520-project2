#ifndef APEXOPINFO_H
#define APEXOPINFO_H
#include "apexOpcodes.h" // To get NUMOPS

/*---------------------------------------------------------
  Array of information for each opcode
  		Includes mnemonic string and instruction format
  		See APEX ISA document for complete details

  NOTE: Declaration/initialization in apexOpcodes.c
  	must be 1-1 with the opcode_enum in apexOpcodes.h!
---------------------------------------------------------*/
struct opInfo_struct opInfo[NUMOPS]  ={
	{"ADD",fmt_dss},
	{"MOVC",fmt_di},
	{"HALT",fmt_nop}
};

#endif