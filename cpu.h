#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_
#include "IQ.h"
#include "ROB.h"
#include "URF.h"
#include "LSQ.h"
#include "BTB.h"
#include "helper.h"
#include <map>

/**
 *  cpu.h
 *  Contains various CPU and Pipeline Data structures
 *
 *  Author :
 *  Gaurav Kothari (gkothar1@binghamton.edu)
 *  State University of New York, Binghamton
 */

enum {
	F, DRF, QUEUE, INT_EX, MUL_EX, MEM_EX, WB, NUM_STAGES
};

enum {
	INT_FU, MUL_FU, LS_FU
};

/* Format of an APEX instruction  */
typedef struct APEX_Instruction {
	char opcode[128];	// Operation Code
	int rd;		    // Destination Register Address
	int rs1;		    // Source-1 Register Address
	int rs2;		    // Source-2 Register Address
	int imm;		    // Literal Value
} APEX_Instruction;

typedef struct Int_BUS {
    int r;
    int r_value;
    int zeroFlag;
} Int_Bus;

typedef struct Mul_BUS {
    int r;
    int r_value;
    int zeroFlag;
} Mul_Bus;

typedef struct Mem_BUS {
    int r;
    int r_value;
    int zeroFlag;
} Mem_Bus;


/* Model of CPU stage latch */
typedef struct CPU_Stage {
	int pc;		    // Program Counter
	char opcode[128];	// Operation Code
	int rs1;		    // Source-1 Register Address
	int u_rs1;
	int u_rs1_valid;
	int rs2;		    // Source-2 Register Address
	int u_rs2;
	int u_rs2_valid;
	int rd;		    // Destination Register Address
	int u_rd;
	int imm;		    // Literal Value
	int rs1_value;	// Source-1 Register Value
	int rs2_value;	// Source-2 Register Value
	int buffer;		// Latch to hold some value
	int mem_address;	// Computed Memory Address
	int busy;		    // Flag to indicate, stage is performing some action
	int stalled;		// Flag to indicate, stage is stalled
	int fuType;       // Function Unit Type ENUM value
	int zeroFlag;
	int CFID;
} CPU_Stage;


/*PC<-->Instruction Map*/

/* Model of APEX CPU */
typedef struct APEX_CPU {
	/* Clock cycles elasped */
	int clock;

	/* Current program counter */
	int pc;

	/* Array of 5 CPU_stage */
	CPU_Stage stage[5];

	/* Code Memory where instructions are stored */
	APEX_Instruction* code_memory;
	int code_memory_size;

	/* Data Memory */
	int data_memory[4096];

	/* Some stats */
	int ins_completed;

	/* Issue Queue */
	IQ* iq;

	/* ROB */
	ROB * rob;

	/* URF */
	URF* urf;

	/* LSQ */
	LSQ* lsq;

	/*BTS / BTB*/
	BTB* btb;

	/*ZERO FLAG*/
	int zero_flag;


	map<int,APEX_Instruction*> *imap;

	Int_Bus int_bus;
	Mul_Bus mul_bus;
	Mem_Bus mem_bus;

} APEX_CPU;


APEX_Instruction*
create_code_memory(const char* filename, int* size);

APEX_CPU*
APEX_cpu_init(const char* filename);

int
APEX_cpu_run(APEX_CPU* cpu);

int APEX_cpu_run_for_cycles(APEX_CPU* cpu, int cycles, int action);

void
APEX_cpu_stop(APEX_CPU* cpu);

int
fetch(APEX_CPU* cpu);

int
decode(APEX_CPU* cpu);

int addToQueues(APEX_CPU* cpu);

int
intFU(APEX_CPU* cpu);

int
memFU(APEX_CPU* cpu);

int
writeback(APEX_CPU* cpu);

#endif
