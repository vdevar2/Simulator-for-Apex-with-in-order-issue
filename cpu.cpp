/*
 *  cpu.c
 *  Contains APEX cpu pipeline implementation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "ROB.h"
#include "LSQ.h"
#include "BTB.h"
#include "helper.h"
#include "lsq_entry.h"
#include<map>

using namespace std;

/* Set this flag to 1 to enable debug messages */
#define ENABLE_DEBUG_MESSAGES 1
int iMulCycleSpent = 0;
int memCycleSpent = 0;
int isHalt = 0;
int AnyInstructionRetired = 0;

// Bus Logic

int comparator_rs1(APEX_CPU *cpu, CPU_Stage *stage) {
    if (stage->u_rs1 == cpu->int_bus.r) {
        return cpu->int_bus.r_value;
    } else if (stage->u_rs1 == cpu->mul_bus.r) {
        return cpu->mul_bus.r_value;
    } else if (cpu->urf->URF_TABLE_valid[stage->u_rs1] == 1) { //valid
        return cpu->urf->URF_Table[stage->u_rs1];
    } else
        return GARBAGE;
}

int comparator_rs2(APEX_CPU *cpu, CPU_Stage *stage) {
    if (stage->u_rs2 == cpu->int_bus.r) {
        return cpu->int_bus.r_value;
    } else if (stage->u_rs2 == cpu->mul_bus.r) {
        return cpu->mul_bus.r_value;
    } else if (cpu->urf->URF_TABLE_valid[stage->u_rs2] == 1) { //valid
        return cpu->urf->URF_Table[stage->u_rs2];
    } else
        return GARBAGE;
}

/*
 * This function creates and initializes APEX cpu.
 *
 * Note : You are free to edit this function according to your
 * 				implementation
 */

APEX_CPU *
APEX_cpu_init(const char *filename) {
    if (!filename) {
        return NULL;
    }

    APEX_CPU *cpu = (APEX_CPU *) malloc(sizeof(*cpu));
    if (!cpu) {
        return NULL;
    }

    cpu->imap = new map<int, APEX_Instruction *>();

    /* Initialize IssueQueue */
    cpu->iq = new IQ();

    /* Initialize ROB */
    cpu->rob = new ROB();

    /*Initialize URF */
    cpu->urf = new URF();

    /*Initialize LSQ */
    cpu->lsq = new LSQ();

    /*Initialize BTB*/
    cpu->btb = new BTB();

    /* Initialize PC, Registers and all pipeline stages */
    cpu->pc = 4000;
    memset(cpu->stage, 0, sizeof(CPU_Stage) * NUM_STAGES);
    memset(cpu->data_memory, 0, sizeof(int) * 4000);

    memset(&cpu->int_bus, -1, sizeof(Int_Bus));
    memset(&cpu->mem_bus, -1, sizeof(Mem_Bus));
    memset(&cpu->mul_bus, -1, sizeof(Mul_Bus));

    /* Parse input file and create code memory */
    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);

    if (!cpu->code_memory) {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES) {
        fprintf(stderr,
                "APEX_CPU : Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU : Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode", "rd", "rs1", "rs2",
               "imm");

        for (int i = 0; i < cpu->code_memory_size; ++i) {
            printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode,
                   cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
        }
    }

    /* Make all stages busy except Fetch stage, initally to start the pipeline */
    for (int i = 1; i < NUM_STAGES; ++i) {
        cpu->stage[i].busy = 1;
    }

    return cpu;
}

/*
 * This function de-allocates APEX cpu.
 *
 * Note : You are free to edit this function according to your
 * 				implementation
 */
void APEX_cpu_stop(APEX_CPU *cpu) {
    printf("\n\n\n\n");

    printf(
            "#--#--#--#--#--#--#--#--#--#--#--#--#--#-- SIMULATION FINISHED #--#--#--#--#--#--#--#--#--#--#--#--#\n\n");
    /*for (int i = 0; i < 16; i++) {
     char* status;
     status = cpu->regs_valid[i] == 1 ? "INVALID" : "VALID";
     printf("|\tREG[%d]\t|\tValue = %d\t|\tStatus=%s\t|\n", i, cpu->regs[i],
     status);
     }*/

    //printf("=============== STATE OF ROB ==========\n\n");
    //cpu->rob->print_rob(10); // print only 10 entries.

    /* printf("=============== LSQ ==========\n\n");
     cout << cpu->lsq << endl;*/

    //printf("=============== STATE OF UNIFIED REGISTER FILE ==========\n\n");
    //cpu->urf->print_urf();

    /*printf("=============== STATE OF ISSUE QUEUE ==========\n\n");
     for (int i = 0; i < IQ_SIZE; i++) {
     (cpu->iq->issueQueue[i]).printIQEntry();
     cout << endl;
     }*/

    //printf("\n\n============== STATE OF DATA MEMORY =============\n\n");
    //for (int i = 0; i < 15; i++) {
//        printf("|\tMEM[%d]\t|\tData Value = %d\t|\n", i, cpu->data_memory[i]);
  //  }

    // delete IQ and ROB
    delete cpu->urf;
    delete cpu->iq;
    delete cpu->lsq;
    delete cpu->rob;
    delete cpu->btb;
    delete cpu->imap;

    free(cpu->code_memory);
    free(cpu);
    printf("ALL CLEAR!!");
}

/* Converts the PC(4000 series) into
 * array index for code memory
 *
 * Note : You are not supposed to edit this function
 *
 */
int get_code_index(int pc) {
    return (pc - 4000) / 4;
}

static void print_instruction(CPU_Stage *stage, int is_fetch) {

    if (strcmp(stage->opcode, "STORE") == 0) {
        printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rs1, stage->rs2,
               stage->imm);
    }

    if (strcmp(stage->opcode, "HALT") == 0) {
        printf("%s ", stage->opcode);
    }

    if (strcmp(stage->opcode, "NOP") == 0) {
        printf("%s ", stage->opcode);
    }

    if (strcmp(stage->opcode, "BZ") == 0) {
        printf("%s,#%d ", stage->opcode, stage->imm);
    }

    if (strcmp(stage->opcode, "BNZ") == 0) {
        printf("%s,#%d ", stage->opcode, stage->imm);
    }

    if (strcmp(stage->opcode, "JUMP") == 0) {
        printf("%s,R%d,#%d ", stage->opcode, stage->rs1, stage->imm);
    }

    if (strcmp(stage->opcode, "LOAD") == 0) {
        printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1,
               stage->imm);
    }

    if (strcmp(stage->opcode, "JAL") == 0) {
        printf("%s,R%d,R%d,#%d ", stage->opcode, stage->u_rd, stage->u_rs1,
               stage->imm);
    }

    if (strcmp(stage->opcode, "MOVC") == 0) {
        if (is_fetch == 1)
            printf("%s,R%d,#%d", stage->opcode, stage->rd, stage->imm);
        else
            printf("%s,R%d,#%d \t [%s,U%d,#%d]", stage->opcode, stage->rd,
                   stage->imm, stage->opcode, stage->u_rd, stage->imm);
    }

    if (strcmp(stage->opcode, "ADDL") == 0
        || strcmp(stage->opcode, "SUBL") == 0) {
        if (is_fetch == 1)
            printf("%s,R%d,R%d,#%d", stage->opcode, stage->rd, stage->rs1,
                   stage->imm);
        else
            printf("%s,R%d,R%d,#%d \t [%s,U%d,U%d,#%d]", stage->opcode,
                   stage->rd, stage->rs1, stage->imm, stage->opcode,
                   stage->u_rd, stage->u_rs1, stage->imm);
    }

    if (strcmp(stage->opcode, "SUB") == 0 || strcmp(stage->opcode, "MUL") == 0
        || strcmp(stage->opcode, "ADD") == 0
        || strcmp(stage->opcode, "EX-OR") == 0
        || strcmp(stage->opcode, "OR") == 0
        || strcmp(stage->opcode, "AND") == 0) {
        if (is_fetch == 1)
            printf("%s,R%d,R%d,R%d", stage->opcode, stage->rd, stage->rs1,
                   stage->rs2);
        else
            printf("%s,R%d,R%d,R%d \t [%s,U%d,U%d,U%d]", stage->opcode,
                   stage->rd, stage->rs1, stage->rs2, stage->opcode,
                   stage->u_rd, stage->u_rs1, stage->u_rs2);
    }
}

/* Debug function which dumps the cpu stage
 * content
 *
 * Note : You are not supposed to edit this function
 *
 */
static void print_stage_content(char *name, CPU_Stage *stage) {
    printf("%-15s: pc(%d) ", name, stage->pc);
    if (strcmp("Fetch", name) == 0) {
        print_instruction(stage, 1);
    } else
        print_instruction(stage, 0);

    printf("\n");
}

void print_register_status(APEX_CPU *cpu) {
    if (ENABLE_DEBUG_MESSAGES) {
        cout << "Details of RENAME TABLE (F-RAT) State --" << endl;
        cpu->urf->print_f_rat();
        cout << "---------------------------------------------" << endl;
        cout << "Details of RENAME TABLE (R-RAT) State --" << endl;
        cpu->urf->print_r_rat();
        cout << "---------------------------------------------" << endl;
        cout << "Details of IQ (Issue Queue) State --" << endl;
        cpu->iq->printIssueQueue();
    }
}

/*
 *  Fetch Stage of APEX Pipeline
 *
 *  Note : You are free to edit this function according to your
 * 				 implementation
 */
int fetch(APEX_CPU *cpu) {
    CPU_Stage *stage = &cpu->stage[F];
    CPU_Stage *drf_stage = &cpu->stage[DRF];
    if (!stage->busy && !stage->stalled) {
        /* Store current PC in fetch latch */
        stage->pc = cpu->pc;


        int MaxCodeSize = 4000 + (4 * cpu->code_memory_size);
        if(cpu->pc >= MaxCodeSize)
            return 0;

        /* Index into code memory using this pc and copy all instruction fields into
         * fetch latch
         */
        APEX_Instruction* current_ins = &cpu->code_memory[get_code_index(cpu->pc)];

        strcpy(stage->opcode, current_ins->opcode);
        stage->rd = current_ins->rd;
        stage->rs1 = current_ins->rs1;
        stage->rs2 = current_ins->rs2;
        stage->imm = current_ins->imm;

        cpu->imap->insert(make_pair(cpu->pc, current_ins));

        /* Update PC for next instruction */
        if (!drf_stage->stalled) {
            cpu->pc += 4;
            cpu->stage[DRF] = cpu->stage[F];
            if (ENABLE_DEBUG_MESSAGES) {
                print_stage_content("Fetch", stage);
            }
            return 0;
        } else {
            stage->stalled = 1;

        }
    }
    return 0;
}

int renamer(APEX_CPU *cpu) {
    CPU_Stage *stage = &cpu->stage[DRF];
    if (strcmp(stage->opcode, "MOVC") == 0) {
        int urfRd = cpu->urf->get_next_free_register();
        if (urfRd != -1) {
            stage->u_rd = urfRd;
            //Mark destination register invalid
            cpu->urf->URF_TABLE_valid[urfRd] = 0;
            //Update F-RAT
            cpu->urf->F_RAT[stage->rd] = urfRd;
            //Add entry of newly renamed register to F-RAT
            cpu->urf->F_RAT[stage->rd] = urfRd;
            return 1;
        }
    }

    if (strcmp(stage->opcode, "JUMP") == 0) {
        int urfsrc1 = cpu->urf->F_RAT[stage->rs1];
        stage->u_rs1 = urfsrc1;
        return 1;
    }

    if (strcmp(stage->opcode, "ADD") == 0 || strcmp(stage->opcode, "SUB") == 0
        || strcmp(stage->opcode, "AND") == 0
        || strcmp(stage->opcode, "OR") == 0
        || strcmp(stage->opcode, "EX-OR") == 0
        || strcmp(stage->opcode, "MUL") == 0) {
        int urfRd = cpu->urf->get_next_free_register();
        if (urfRd != -1) {
            stage->u_rd = urfRd;

            int urfsrc1 = cpu->urf->F_RAT[stage->rs1];
            int urfsrc2 = cpu->urf->F_RAT[stage->rs2];
            cpu->urf->F_RAT[stage->rd] = urfRd;

            stage->u_rs1 = urfsrc1;
            stage->u_rs2 = urfsrc2;
            //Mark destination register invalid
            cpu->urf->URF_TABLE_valid[urfRd] = 0;
            //Add entry of newly renamed register to F-RAT
            cpu->urf->F_RAT[stage->rd] = urfRd;
            return 1;
        }
    }

    if (strcmp(stage->opcode, "ADDL") == 0
        || strcmp(stage->opcode, "SUBL") == 0) {
        int urfRd = cpu->urf->get_next_free_register();
        if (urfRd != -1) {
            stage->u_rd = urfRd;

            int urfsrc1 = cpu->urf->F_RAT[stage->rs1];
            cpu->urf->F_RAT[stage->rd] = urfRd;

            stage->u_rs1 = urfsrc1;
            //Mark destination register invalid
            cpu->urf->URF_TABLE_valid[urfRd] = 0;
            //Add entry of newly renamed register to F-RAT
            cpu->urf->F_RAT[stage->rd] = urfRd;
            return 1;
        }
    }

    if (strcmp(stage->opcode, "STORE") == 0) {
        int urfsrc1 = cpu->urf->F_RAT[stage->rs1];  //ADDRESS
        int urfsrc2 = cpu->urf->F_RAT[stage->rs2];  //ADDRESS
        stage->u_rs1 = urfsrc1;
        stage->u_rs2 = urfsrc2;
        return 1;
    }

    if (strcmp(stage->opcode, "LOAD") == 0 || strcmp(stage->opcode, "JAL") == 0) {
        int urfRd = cpu->urf->get_next_free_register();
        if (urfRd != -1) {
            stage->u_rd = urfRd;

            int urfsrc1 = cpu->urf->F_RAT[stage->rs1];  //ADDRESS
            cpu->urf->F_RAT[stage->rd] = urfRd;

            stage->u_rs1 = urfsrc1;
            cpu->urf->URF_TABLE_valid[urfRd] = 0;
            cpu->urf->F_RAT[stage->rd] = urfRd;
            return 1;
        }
    }
    return 0;
}

/*
 *  Decode Stage of APEX Pipeline
 */
int decode(APEX_CPU *cpu) {
    CPU_Stage *stage = &cpu->stage[DRF];

    // as per specification, HALT stalls the D/RF stage and adds entry in ROB. No entry in IQ is needed
    if (strcmp(stage->opcode, "HALT") == 0) {

        stage->CFID = cpu->btb->last_control_flow_instr;
        Rob_entry rob_entry;
        rob_entry.setPc_value(stage->pc);
        rob_entry.setCFID(stage->CFID);
        if (cpu->rob->add_instruction_to_ROB(rob_entry)) {      // Adding to ROB
//			cout << "HALT is  added to ROB" << endl;
            memset(stage, 0, sizeof(CPU_Stage));
        }
        if (ENABLE_DEBUG_MESSAGES)
            print_stage_content("Decode/RF", stage);
        return 0;
    }

    if (!stage->busy && !stage->stalled) {

        /* No Register file read needed for MOVC */
        if (strcmp(stage->opcode, "MOVC") == 0) {
            stage->fuType = INT_FU;
            if (renamer(cpu) == 1) {
                // Go to next stage
                stage->CFID = cpu->btb->last_control_flow_instr;;
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "JUMP") == 0) {
            stage->fuType = INT_FU;
            int cfid = cpu->btb->get_next_free_CFID();
            if (cfid != -1) {
                if (renamer(cpu) == 1) {
                    stage->CFID = cfid;
                    cpu->btb->add_cfid(cfid);
                    cpu->stage[QUEUE] = cpu->stage[DRF];
                    stage->rs1_value = comparator_rs1(cpu, stage);
                    if (ENABLE_DEBUG_MESSAGES)
                        print_stage_content("Decode/RF", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                    return 0;
                }
            }
        }

        if (strcmp(stage->opcode, "JAL") == 0) {
            stage->fuType = INT_FU;
            if (renamer(cpu) == 1) {
                int cfid = cpu->btb->get_next_free_CFID();
                if (cfid != -1) {
                    stage->rs1_value = comparator_rs1(cpu, stage);
                    cpu->stage[QUEUE] = cpu->stage[DRF];

                    if (ENABLE_DEBUG_MESSAGES)
                        print_stage_content("Decode/RF", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                    return 0;
                }
            }
        }


        if (strcmp(stage->opcode, "BZ") == 0
            || strcmp(stage->opcode, "BNZ") == 0) {
            stage->fuType = INT_FU;
            int cfid = cpu->btb->get_next_free_CFID();
            if (cfid != -1) {
                stage->CFID = cfid;
                cpu->btb->add_cfid(cfid);
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "ADD") == 0
            || strcmp(stage->opcode, "SUB") == 0
            || strcmp(stage->opcode, "AND") == 0
            || strcmp(stage->opcode, "OR") == 0
            || strcmp(stage->opcode, "EX-OR") == 0) {
            stage->fuType = INT_FU;
            if (renamer(cpu) == 1) {

                // check bus values: If available, take otherwise put in issue Q.
                // Becos, it will eventually have updated entries.

                stage->rs1_value = comparator_rs1(cpu, stage);
                stage->rs2_value = comparator_rs2(cpu, stage);
                stage->CFID = cpu->btb->last_control_flow_instr;
                // Go to next

                cpu->stage[QUEUE] = cpu->stage[DRF];

                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "ADDL") == 0
            || strcmp(stage->opcode, "SUBL") == 0) {
            stage->fuType = INT_FU;
            if (renamer(cpu) == 1) {
                stage->rs1_value = comparator_rs1(cpu, stage);
                stage->CFID = cpu->btb->last_control_flow_instr;
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "MUL") == 0) {
            stage->fuType = MUL_FU;
            if (renamer(cpu) == 1) {
                // check bus values.
                stage->rs1_value = comparator_rs1(cpu, stage);
                stage->rs2_value = comparator_rs2(cpu, stage);

                // Go to next stage
                stage->CFID = cpu->btb->last_control_flow_instr;
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "STORE") == 0) {
            stage->fuType = LS_FU;
            if (renamer(cpu) == 1) {
                stage->rs1_value = comparator_rs1(cpu, stage); //Source
                stage->rs2_value = comparator_rs2(cpu, stage);

                // Go to next stage
                stage->CFID = cpu->btb->last_control_flow_instr;
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

        if (strcmp(stage->opcode, "LOAD") == 0) {
            stage->fuType = LS_FU;
            if (renamer(cpu) == 1) {
                stage->rs1_value = comparator_rs1(cpu, stage); //Source

                // Go to next stage
                stage->CFID = cpu->btb->last_control_flow_instr;
                cpu->stage[QUEUE] = cpu->stage[DRF];
                if (ENABLE_DEBUG_MESSAGES)
                    print_stage_content("Decode/RF", stage);
                memset(stage, 0, sizeof(CPU_Stage));
                return 0;
            }
        }

    }

    return 0;
}

/*
 * Make the entry in IQ, ROB and LSQ(If neeeded)
 * */
int addToQueues(APEX_CPU *cpu) {
    CPU_Stage *stage = &cpu->stage[QUEUE];
    CPU_Stage *int_stage = &cpu->stage[INT_EX];
    CPU_Stage *mul_stage = &cpu->stage[MUL_EX];

    if (!stage->busy && !stage->stalled) {

        if (strcmp(stage->opcode, "JAL") == 0) {
            IQEntry entry;
            entry.pc = stage->pc;
            entry.fuType = stage->fuType;
            entry.src1Value = comparator_rs1(cpu, stage);
            entry.src1Valid = cpu->urf->URF_TABLE_valid[stage->u_rs1];
            entry.src1 = stage->u_rs1;
            entry.src2Valid = -1;
            entry.src2Value = -1;
            entry.src2 = -1;
            entry.rd = stage->u_rd;
            entry.literal = stage->imm;
            entry.clock = cpu->clock;
            entry.CFID = stage->CFID;
            strcpy(entry.opcode, "JAL");
            entry.lsqIndex = -1;
            if (entry.src1Valid)
                entry.setStatus();
            if (cpu->iq->addToIssueQueue(&entry, stage->fuType) == 1) {
                Rob_entry rob_entry;
                rob_entry.setPc_value(stage->pc);
                rob_entry.setExcodes(-1);
                rob_entry.setResult(stage->imm);
                rob_entry.setArchiteture_register(stage->rd);
                rob_entry.setM_unifier_register(entry.rd);
                rob_entry.setCFID(entry.CFID);
                URF_data *savedInfo = cpu->urf->takeSnapshot(entry.CFID);
                rob_entry.setPv_saved_info(savedInfo);
                cpu->rob->add_instruction_to_ROB(rob_entry);
                int_stage->busy = 0;
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("QUEUE", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                }
                return 0;
            }
        }

        if (strcmp(stage->opcode, "JUMP") == 0) {
            IQEntry entry;
            entry.pc = stage->pc;
            entry.fuType = stage->fuType;
            entry.src1Value = comparator_rs1(cpu, stage);
            entry.src1Valid = cpu->urf->URF_TABLE_valid[stage->u_rs1];
            entry.src2Valid = -1;
            entry.src2Value = -1;
            entry.src1 = -1;
            entry.src2 = -1;
            entry.rd = -1;
            entry.literal = stage->imm;
            entry.clock = cpu->clock;
            entry.CFID = stage->CFID;
            strcpy(entry.opcode, "JUMP");
            entry.lsqIndex = -1;
            if (entry.src1Valid)
                entry.setStatus();
            if (cpu->iq->addToIssueQueue(&entry, stage->fuType) == 1) {
                Rob_entry rob_entry;
                rob_entry.setPc_value(stage->pc);
                rob_entry.setExcodes(-1);
                rob_entry.setResult(stage->imm);
                rob_entry.setArchiteture_register(stage->rd);
                rob_entry.setM_unifier_register(entry.rd);
                rob_entry.setCFID(entry.CFID);
                URF_data *savedInfo = cpu->urf->takeSnapshot(entry.CFID);
                rob_entry.setPv_saved_info(savedInfo);
                cpu->rob->add_instruction_to_ROB(rob_entry);
                int_stage->busy = 0;
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("QUEUE", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                }
                return 0;
            }
        }

        if (strcmp(stage->opcode, "BZ") == 0
            || strcmp(stage->opcode, "BNZ") == 0) {
            IQEntry entry;
            entry.setStatus();
            entry.pc = stage->pc;
            entry.fuType = stage->fuType;
            entry.src1Valid = -1;
            entry.src2Valid = -1;
            entry.src1Value = -1;
            entry.src2Value = -1;
            entry.src1 = -1;
            entry.src2 = -1;
            entry.rd = -1;
            entry.literal = stage->imm;
            entry.clock = cpu->clock;
            entry.CFID = stage->CFID;
            if (strcmp(stage->opcode, "BZ") == 0)
                strcpy(entry.opcode, "BZ");
            else
                strcpy(entry.opcode, "BNZ");
            entry.lsqIndex = -1;
            if (cpu->iq->addToIssueQueue(&entry, stage->fuType) == 1) {
                Rob_entry rob_entry;
                rob_entry.setPc_value(stage->pc);
                rob_entry.setExcodes(-1);
                rob_entry.setResult(stage->imm);
                rob_entry.setArchiteture_register(entry.rd);
                rob_entry.setM_unifier_register(entry.rd);
                rob_entry.setCFID(entry.CFID);
                URF_data *savedInfo = cpu->urf->takeSnapshot(entry.CFID);
                rob_entry.setPv_saved_info(savedInfo);
                cpu->rob->add_instruction_to_ROB(rob_entry);
                int_stage->busy = 0;
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("QUEUE", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                }
                return 0;
            }
        }

        if (strcmp(stage->opcode, "MOVC") == 0) {
//			IQEntry entry = IQEntry(stage->u_rd, stage->u_rs1, stage->u_rs2,
//					stage->imm, stage->pc, stage->fuType, "MOVC", cpu->clock);
            IQEntry entry;
            entry.setStatus();
            entry.pc = stage->pc;
            entry.fuType = stage->fuType;
            entry.src1Valid = -1;
            entry.src2Valid = -1;
            entry.src1Value = -1;
            entry.src2Value = -1;
            entry.src1 = -1;
            entry.src2 = -1;
            entry.rd = stage->u_rd;
            entry.literal = stage->imm;
            entry.clock = cpu->clock;
            entry.CFID = stage->CFID;
            strcpy(entry.opcode, "MOVC");
            entry.lsqIndex = -1;
            if (cpu->iq->addToIssueQueue(&entry, stage->fuType) == 1) { // Adding to IQ
//				cout << "entry added to IQ" << endl;
                Rob_entry rob_entry;
                rob_entry.setPc_value(stage->pc);
                rob_entry.setExcodes(-1);
                rob_entry.setResult(stage->imm);
                rob_entry.setArchiteture_register(stage->rd);
                rob_entry.setM_unifier_register(stage->u_rd);
                rob_entry.setCFID(entry.CFID);

                if (cpu->rob->add_instruction_to_ROB(rob_entry)) { // Adding to ROB
//					cout << "entry added to ROB" << endl;
                }
                int_stage->busy = 0;
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("QUEUE", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                }
                return 0;
            }
        }

        if (strcmp(stage->opcode, "ADD") == 0
            || strcmp(stage->opcode, "SUB") == 0
            || strcmp(stage->opcode, "AND") == 0
            || strcmp(stage->opcode, "OR") == 0
            || strcmp(stage->opcode, "EX-OR") == 0
            || strcmp(stage->opcode, "LOAD") == 0
            || strcmp(stage->opcode, "STORE") == 0
            || strcmp(stage->opcode, "ADDL") == 0
            || strcmp(stage->opcode, "SUBL") == 0
            || strcmp(stage->opcode, "MUL") == 0) {
            //Take data from bus
            IQEntry entry;
            entry.src1Value = comparator_rs1(cpu, stage);
            entry.src2Value = comparator_rs2(cpu, stage);
            entry.pc = stage->pc;
            entry.fuType = stage->fuType;
            // If URF table has valid bit set, means these 'Sources' are valid too..!
            entry.src1Valid = cpu->urf->URF_TABLE_valid[stage->u_rs1];
            entry.src2Valid = cpu->urf->URF_TABLE_valid[stage->u_rs2];
            // What if both sources have valid data?--> if yes, mark entry as valid, 'READY' to execute
            if (entry.src1Valid && entry.src2Valid)
                entry.setStatus();
            entry.src1 = stage->u_rs1;
            entry.src2 = stage->u_rs2;
            entry.rd = stage->u_rd;
            entry.literal = stage->imm;
            entry.CFID = stage->CFID;
            entry.clock = cpu->clock;
            char opcodeTemp[128];
            strcpy(opcodeTemp, stage->opcode);
            strcpy(entry.opcode, opcodeTemp);
            if (strcmp(stage->opcode, "LOAD") == 0
                || strcmp(stage->opcode, "STORE") == 0) {
                //Create an LSQ entry
                LSQ_entry lsq_entry;
                lsq_entry.setM_pc(stage->pc);
                lsq_entry.setM_status(0);
                lsq_entry.allocated = UNALLOCATED;
                int which = strcmp(stage->opcode, "LOAD") == 0 ? LOAD : STORE;
                lsq_entry.setM_which_ins(which);
                lsq_entry.setM_memory_addr(-1);
                lsq_entry.setM_is_memory_addr_valid(INVALID);
                int dest = strcmp(stage->opcode, "LOAD") == 0 ? entry.rd : -1;
                lsq_entry.setM_dest_reg(dest);
                int store_reg =
                        strcmp(stage->opcode, "STORE") == 0 ? entry.src1 : -1;
                lsq_entry.setM_store_reg(store_reg);
                lsq_entry.setM_store_src1_data_valid(entry.src1Valid);
                lsq_entry.setM_store_reg_value(entry.src1Value);
                lsq_entry.CFID = entry.CFID;
                int lsq_index = cpu->lsq->add_instruction_to_LSQ(lsq_entry);

                if (lsq_index != -1) {
//					cout << "Added to LSQ" << endl;
                    entry.lsqIndex = lsq_index;
                }

            } else
                entry.lsqIndex = -1;
            if (cpu->iq->addToIssueQueue(&entry, stage->fuType) == 1) { // Adding to IQ
//				cout << "entry added to IQ" << endl;

                Rob_entry rob_entry;
                rob_entry.setStatus(0);
                rob_entry.setPc_value(stage->pc);
                rob_entry.setExcodes(-1);
                rob_entry.setArchiteture_register(stage->rd);
                rob_entry.setM_unifier_register(stage->u_rd);
                rob_entry.setCFID(entry.CFID);

                cpu->rob->add_instruction_to_ROB(rob_entry);

                int_stage->busy = 0;
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("QUEUE", stage);
                    memset(stage, 0, sizeof(CPU_Stage));
                }
            }
        }
    }
    return 0;
}

/*
 *  INT Function Unit Stage of APEX Pipeline
 */
int intFU(APEX_CPU *cpu) {
    CPU_Stage *int_stage = &cpu->stage[INT_EX];
    CPU_Stage *mem_stage = &cpu->stage[MEM_EX];
    CPU_Stage *queue_stage = &cpu->stage[QUEUE];
    CPU_Stage *drf_stage = &cpu->stage[DRF];
    CPU_Stage *fetch_stage = &cpu->stage[F];

    drf_stage->stalled = 0;
    queue_stage->stalled = 0;
    fetch_stage->stalled = 0;

    if (!int_stage->busy && !int_stage->stalled) {

        //MEMORY Type Instruction
        IQEntry mem_instruction = cpu->iq->getNextInstructionToIssue(LS_FU);
        if (mem_instruction.fuType == LS_FU
            && mem_instruction.getStatus() == 1) {
            printf("ALLOCATED IQ : %d\n", mem_instruction.allocated);
            int_stage->pc = mem_instruction.pc;
            strcpy(int_stage->opcode, mem_instruction.opcode);
            int_stage->u_rs1 = mem_instruction.src1;
            int_stage->rs1_value = mem_instruction.src1Value;
            int_stage->u_rs1_valid = mem_instruction.src1Valid;
            int_stage->u_rs2 = mem_instruction.src2;
            int_stage->rs2_value = mem_instruction.src2Value;
            int_stage->u_rs2_valid = mem_instruction.src2Valid;
            int_stage->u_rd = mem_instruction.rd;
            int_stage->imm = mem_instruction.literal;
            int_stage->buffer = -1;
            int_stage->busy = 0;

            int_stage->busy = 0;

            //Print before removing it
            print_register_status(cpu);
            int res = cpu->iq->removeEntry(&mem_instruction);
            int mem_address;
            if (strcmp(int_stage->opcode, "STORE") == 0)
                mem_address = int_stage->rs2_value + int_stage->imm;
            else
                mem_address = int_stage->rs1_value + int_stage->imm;
            //Update lsq with memory address.
            cpu->lsq->update_LSQ_index(mem_instruction.lsqIndex, 1,
                                       mem_address);
            mem_stage->busy = 0;
            if (ENABLE_DEBUG_MESSAGES) {
                print_stage_content("INT FU", int_stage);
                // result is calculated so just memset this.
                memset(int_stage, 0, sizeof(CPU_Stage));

                return 0;
            }
        }

        //INTEGER type instruction
        IQEntry insToExec = cpu->iq->getNextInstructionToIssue(INT_FU);
        if (strlen(insToExec.opcode) > 0) {
            if (insToExec.fuType == INT_FU && insToExec.getStatus() == 1) {
                int_stage->pc = insToExec.pc;
                strcpy(int_stage->opcode, insToExec.opcode);
                int_stage->u_rs1 = insToExec.src1;
                int_stage->rs1_value = insToExec.src1Value;
                int_stage->u_rs1_valid = insToExec.src1Valid;
                int_stage->u_rs2 = insToExec.src2;
                int_stage->rs2_value = insToExec.src2Value;
                int_stage->u_rs2_valid = insToExec.src2Valid;
                int_stage->u_rd = insToExec.rd;
                int_stage->imm = insToExec.literal;
                int_stage->buffer = -1;
                int_stage->busy = 0;

                //Print before removing it
                print_register_status(cpu);
                cpu->iq->removeEntry(&insToExec);
            }

            if (strcmp(int_stage->opcode, "BZ") == 0) {
                int flag;
                int tempSID = cpu->rob->get_slot_id_from_cfid(insToExec.CFID,
                                                              int_stage->pc);
                Rob_entry *thisEntry = &cpu->rob->rob_queue[tempSID];
                if (cpu->rob->check_with_rob_head(int_stage->pc)) {
                    //TRUE: Branch is on head. take zero flag from cpu
                    flag = cpu->zero_flag;
                } else {
                    flag = cpu->rob->get_zero_flag_at_slot_id(tempSID);
                    //@TODO If MOVC is in between arithmetic and branch in rob
                }

                if (flag == 1) {
                    //Take the branch
                    memset(drf_stage, 0, sizeof(CPU_Stage));
                    memset(queue_stage, 0, sizeof(CPU_Stage));
                    drf_stage->stalled = 1;
                    queue_stage->stalled = 1;
                    fetch_stage->stalled = 1;
                    //FLUSH ROB
                    cpu->rob->flush_ROB_entries(tempSID, cpu);
                    //FLUSH not only IQ but also LSQ
                    int mostRecentCFID = cpu->btb->last_control_flow_instr;

                    deque<int> cfidDeque = cpu->btb->CF_instn_order;
                    deque<int>::iterator itr;
                    itr = find(cfidDeque.begin(), cfidDeque.end(), insToExec.CFID);
                    for (; itr != cfidDeque.end(); itr++) {
                        int tempCFID = *itr;
                        cpu->iq->flushIQEntries(tempCFID, insToExec.pc);
                        cpu->lsq->flushLSQEntries(tempCFID);
                    }

                    //Restoring Snapshot
                    URF_data *temp;
                    temp = (URF_data *) thisEntry->getPv_saved_info();
                    cpu->urf->restoreSnapshot(*temp);


                    cpu->pc = int_stage->pc + int_stage->imm;
                }

                // update
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID, -1,
                                          VALID, int_stage->imm);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }

            if (strcmp(int_stage->opcode, "JUMP") == 0) {
                int tempSID = cpu->rob->get_slot_id_from_cfid(insToExec.CFID,
                                                              int_stage->pc);
                Rob_entry *thisEntry = &cpu->rob->rob_queue[tempSID];
                memset(drf_stage, 0, sizeof(CPU_Stage));
                memset(queue_stage, 0, sizeof(CPU_Stage));
                drf_stage->stalled = 1;
                queue_stage->stalled = 1;
                fetch_stage->stalled = 1;

                //FLUSH ROB
                cpu->rob->flush_ROB_entries(tempSID, cpu);
                //FLUSH not only IQ but also LSQ
                int mostRecentCFID = cpu->btb->last_control_flow_instr;
                deque<int> cfidDeque = cpu->btb->CF_instn_order;
                deque<int>::iterator itr;
                itr = find(cfidDeque.begin(), cfidDeque.end(), insToExec.CFID);
                for (; itr != cfidDeque.end(); itr++) {
                    int tempCFID = *itr;
                    cpu->iq->flushIQEntries(tempCFID, insToExec.pc);
                    cpu->lsq->flushLSQEntries(tempCFID);
                }

                //Restoring Snapshot
                URF_data *temp;
                temp = (URF_data *) thisEntry->getPv_saved_info();
                cpu->urf->restoreSnapshot(*temp);

                cpu->pc = int_stage->rs1_value + int_stage->imm;
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID, -1,
                                          VALID, int_stage->imm);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }

            if (strcmp(int_stage->opcode, "JAL") == 0) {
                int tempSID = cpu->rob->get_slot_id_from_cfid(insToExec.CFID,
                                                              int_stage->pc);
                Rob_entry *thisEntry = &cpu->rob->rob_queue[tempSID];
                memset(drf_stage, 0, sizeof(CPU_Stage));
                memset(queue_stage, 0, sizeof(CPU_Stage));
                drf_stage->stalled = 1;
                queue_stage->stalled = 1;
                //FLUSH ROB
                cpu->rob->flush_ROB_entries(tempSID, cpu);
                //FLUSH not only IQ but also LSQ
                int mostRecentCFID = cpu->btb->last_control_flow_instr;
                deque<int> cfidDeque = cpu->btb->CF_instn_order;
                deque<int>::iterator itr;
                itr = find(cfidDeque.begin(), cfidDeque.end(), insToExec.CFID);
                for (; itr != cfidDeque.end(); itr++) {
                    int tempCFID = *itr;
                    cpu->iq->flushIQEntries(tempCFID, insToExec.pc);
                    cpu->lsq->flushLSQEntries(tempCFID);
                }
                //Restoring Snapshot
                URF_data *temp;
                temp = (URF_data *) thisEntry->getPv_saved_info();
                cpu->urf->restoreSnapshot(*temp);

                cpu->pc = int_stage->rs1_value + int_stage->imm;
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID, -1,
                                          VALID, int_stage->imm);

                int buffer = int_stage->pc + 4;
                cpu->urf->URF_Table[int_stage->u_rd] = buffer;
                cpu->urf->URF_TABLE_valid[int_stage->u_rd] = 1;

                //updating bus
                cpu->int_bus.r = int_stage->u_rd;
                cpu->int_bus.r_value = buffer;
                if (buffer == 0) {
                    cpu->int_bus.zeroFlag = 1;
                    int_stage->zeroFlag = 1;
                }
                cpu->urf->URF_Z[int_stage->u_rd] = int_stage->zeroFlag;
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID,
                                          int_stage->zeroFlag, VALID, buffer);
                cpu->iq->updateIssueQueueEntries(int_stage->u_rd, buffer);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }

            if (strcmp(int_stage->opcode, "BNZ") == 0) {
                int flag;
                int tempSID = cpu->rob->get_slot_id_from_cfid(insToExec.CFID,
                                                              int_stage->pc);
                Rob_entry *thisEntry = &cpu->rob->rob_queue[tempSID];
                if (cpu->rob->check_with_rob_head(int_stage->pc)) {
                    //TRUE: Branch is on head. take zero flag from cpu
                    flag = cpu->zero_flag;
                } else {
                    int prev_Slot = tempSID - 1;
                    flag = cpu->rob->get_zero_flag_at_slot_id(prev_Slot);
                    //@TODO If MOVC is in between arithmetic and branch in rob
                }

                if (flag == 0) {        // if zero flag is not set, take branch
                    //Take the branch
                    memset(drf_stage, 0, sizeof(CPU_Stage));
                    memset(queue_stage, 0, sizeof(CPU_Stage));
                    drf_stage->stalled = 1;
                    queue_stage->stalled = 1;
                    fetch_stage->stalled = 1;
                    //FLUSH ROB
                    cpu->rob->flush_ROB_entries(tempSID, cpu);
                    //FLUSH not only IQ but also LSQ
                    int mostRecentCFID = cpu->btb->last_control_flow_instr;
                    deque<int> cfidDeque = cpu->btb->CF_instn_order;
                    deque<int>::iterator itr;
                    itr = find(cfidDeque.begin(), cfidDeque.end(), insToExec.CFID);
                    for (; itr != cfidDeque.end(); itr++) {
                        int tempCFID = *itr;
                        cpu->iq->flushIQEntries(tempCFID, insToExec.pc);
                        cpu->lsq->flushLSQEntries(tempCFID);
                    }

                    //Restoring Snapshot
                    URF_data *temp;
                    temp = (URF_data *) thisEntry->getPv_saved_info();
                    cpu->urf->restoreSnapshot(*temp);

                    cpu->pc = int_stage->pc + int_stage->imm;
                }

                // update
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID, -1,
                                          VALID, int_stage->imm);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }


            if (strcmp(int_stage->opcode, "MOVC") == 0) {
                //Move the contents to the res[ective Unified register
                cpu->urf->URF_Table[int_stage->u_rd] = int_stage->imm;
                cpu->urf->URF_TABLE_valid[int_stage->u_rd] = 1;

                cpu->int_bus.r = int_stage->u_rd;
                cpu->int_bus.r_value = int_stage->imm;

                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID, -1,
                                          VALID, int_stage->imm);
                cpu->iq->updateIssueQueueEntries(int_stage->u_rd,
                                                 int_stage->imm);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }

            }

            if (strcmp(int_stage->opcode, "ADDL") == 0) {
                int buffer = int_stage->rs1_value + int_stage->imm;
                cpu->urf->URF_Table[int_stage->u_rd] = buffer;
                cpu->urf->URF_TABLE_valid[int_stage->u_rd] = 1;

                //updating bus
                cpu->int_bus.r = int_stage->u_rd;
                cpu->int_bus.r_value = buffer;
                if (buffer == 0) {
                    cpu->int_bus.zeroFlag = 1;
                    int_stage->zeroFlag = 1;
                }
                cpu->urf->URF_Z[int_stage->u_rd] = int_stage->zeroFlag;
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID,
                                          int_stage->zeroFlag, VALID, buffer);
                cpu->iq->updateIssueQueueEntries(int_stage->u_rd, buffer);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }

            if (strcmp(int_stage->opcode, "SUBL") == 0) {
                int buffer = int_stage->rs1_value + int_stage->imm;
                cpu->urf->URF_Table[int_stage->u_rd] = buffer;
                cpu->urf->URF_TABLE_valid[int_stage->u_rd] = 1;

                //updating bus
                cpu->int_bus.r = int_stage->u_rd;
                cpu->int_bus.r_value = buffer;
                if (buffer == 0) {
                    cpu->int_bus.zeroFlag = 1;
                    int_stage->zeroFlag = 1;
                }
                cpu->urf->URF_Z[int_stage->u_rd] = int_stage->zeroFlag;
                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID,
                                          int_stage->zeroFlag, VALID, buffer);
                cpu->iq->updateIssueQueueEntries(int_stage->u_rd, buffer);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }

            if (strcmp(int_stage->opcode, "ADD") == 0
                || strcmp(int_stage->opcode, "SUB") == 0
                || strcmp(int_stage->opcode, "AND") == 0
                || strcmp(int_stage->opcode, "OR") == 0
                || strcmp(int_stage->opcode, "EX-OR") == 0) {

                int buffer;

                if (strcmp(int_stage->opcode, "ADD") == 0)
                    buffer = int_stage->rs1_value + int_stage->rs2_value;
                else if (strcmp(int_stage->opcode, "SUB") == 0)
                    buffer = int_stage->rs1_value - int_stage->rs2_value;
                else if (strcmp(int_stage->opcode, "AND") == 0)
                    buffer = int_stage->rs1_value & int_stage->rs2_value;
                else if (strcmp(int_stage->opcode, "OR") == 0)
                    buffer = int_stage->rs1_value | int_stage->rs2_value;
                else if (strcmp(int_stage->opcode, "EX-OR") == 0)
                    buffer = int_stage->rs1_value ^ int_stage->rs2_value;

                cpu->urf->URF_Table[int_stage->u_rd] = buffer;
                cpu->urf->URF_TABLE_valid[int_stage->u_rd] = 1;

                //updating bus
                cpu->int_bus.r = int_stage->u_rd;
                cpu->int_bus.r_value = buffer;
                if (buffer == 0) {
                    cpu->int_bus.zeroFlag = 1;
                    int_stage->zeroFlag = 1;
                } else{
                    int_stage->zeroFlag = 0;
                    cpu->int_bus.zeroFlag = 0;
                }
                cpu->urf->URF_Z[int_stage->u_rd] = int_stage->zeroFlag;

                cpu->rob->update_ROB_slot(int_stage->pc, insToExec.CFID,
                                          int_stage->zeroFlag, VALID, buffer);
                cpu->iq->updateIssueQueueEntries(int_stage->u_rd, buffer);
                if (ENABLE_DEBUG_MESSAGES) {
                    print_stage_content("INT FU", int_stage);
                    // result is calculated so just memset this.
                    memset(int_stage, 0, sizeof(CPU_Stage));
                }
            }



            // Update ROB here.

            //@discuss:
            //  - Once FU is free, shall it take 'next instruction directly from IQ?'
        }

    }
    return 0;
}

int mulFU(APEX_CPU *cpu) {
    CPU_Stage *mul_stage = &cpu->stage[MUL_EX];

// Here, we are giving instruction to respective 'Function Units.'
//  @discuss:
//          - Do we need to check whether FU is free or not?
//          - If 2 MULs come back to back, what to do? Need to check FU status.
//

// @discuss: here, we r just checking FU is stalled or not

    if (!mul_stage->stalled && iMulCycleSpent == 0) { // Only transfer if FU is free.
        IQEntry insToExec = cpu->iq->getNextInstructionToIssue(MUL_FU);
        if (insToExec.fuType == MUL_FU && insToExec.getStatus() == 1) {
            //Send instruction to int function unit
            memset(mul_stage, 0, sizeof(CPU_Stage));

            mul_stage->pc = insToExec.pc;
            mul_stage->u_rd = insToExec.rd;
            mul_stage->imm = insToExec.literal;
            strcpy(mul_stage->opcode, insToExec.opcode);
            mul_stage->u_rs1 = insToExec.src1;
            mul_stage->u_rs2 = insToExec.src2;
            mul_stage->rs1_value = insToExec.src1Value;
            mul_stage->rs2_value = insToExec.src2Value;
            mul_stage->u_rs1_valid = insToExec.src1Valid;
            mul_stage->u_rs2_valid = insToExec.src2Valid;
            mul_stage->CFID = insToExec.CFID;

            //Print before removing it
            print_register_status(cpu);
            cpu->iq->removeEntry(&insToExec);
        }

    }

// Mul instruction spends 2-cycles.
    if (!mul_stage->busy && !mul_stage->stalled) {

        if (strcmp(mul_stage->opcode, "MUL") == 0) {
            iMulCycleSpent++;
        }
        // This is 2nd cycle we are done.
        if (iMulCycleSpent == 2) {
            int buffer = cpu->urf->URF_Table[mul_stage->u_rs1]
                         * cpu->urf->URF_Table[mul_stage->u_rs2];
            cpu->urf->URF_Table[mul_stage->u_rd] = buffer;
            cpu->urf->URF_TABLE_valid[mul_stage->u_rd] = 1;

            //updating bus
            cpu->mul_bus.r = mul_stage->u_rd;
            cpu->mul_bus.r_value = buffer;
            if (buffer == 0) {
                cpu->mul_bus.zeroFlag = 1;
                mul_stage->zeroFlag = 1;
            }

            cpu->urf->URF_Z[mul_stage->u_rd] = mul_stage->zeroFlag;

            cpu->rob->update_ROB_slot(mul_stage->pc, mul_stage->CFID, mul_stage->zeroFlag,
                                      VALID, buffer);
            cpu->iq->updateIssueQueueEntries(mul_stage->u_rd, buffer);

            mul_stage->stalled = 0;
            iMulCycleSpent = 0;

            memset(mul_stage, 0, sizeof(CPU_Stage));
        }

        if (ENABLE_DEBUG_MESSAGES) {
            print_stage_content("MUL FU", mul_stage);
        }
    }
    return 0;
}

/*
 *  Memory Stage of APEX Pipeline
 *
 *  Note : You are free to edit this function according to your
 * 				 implementation
 */
int memFU(APEX_CPU *cpu) {
    CPU_Stage *stage = &cpu->stage[MEM_EX];

// Memmory instruction spends 2-cycles.
    if(cpu->lsq->isempty())
        return 0;

    if (!stage->stalled && memCycleSpent == 0) {
        LSQ_entry *insToExecMem = cpu->lsq->check_head_instruction_from_LSQ();

        stage->pc = insToExecMem->m_pc;
        int m_which_ins = insToExecMem->m_which_ins;
        if (m_which_ins == STORE)
            strcpy(stage->opcode, "STORE");
        else
            strcpy(stage->opcode, "LOAD");
        stage->mem_address = insToExecMem->m_memory_addr;
        stage->u_rd = insToExecMem->m_dest_reg;
        stage->u_rs1 = insToExecMem->m_store_reg;
        stage->u_rs1_valid = insToExecMem->m_is_register_valid;
        stage->rs1_value = insToExecMem->m_store_reg_value;
        stage->busy = 0;
    }

    if (!stage->busy && !stage->stalled) {
        LSQ_entry *insToExecMem = cpu->lsq->check_head_instruction_from_LSQ();

        if (insToExecMem->getM_status() == 1
            && cpu->rob->check_with_rob_head(insToExecMem->m_pc)) {
            memCycleSpent++;
        }
        if (ENABLE_DEBUG_MESSAGES) {
            print_stage_content("MEMORY FU", stage);
        }

        // This is 3rd cycle we are done.
        if (memCycleSpent == 3) {

            if (strcmp(stage->opcode, "STORE") == 0) {
                //Store to the memory
                cpu->data_memory[stage->mem_address] = stage->rs1_value;
                cpu->lsq->retire_instruction_from_LSQ();
                if(AnyInstructionRetired<2) {
                    cpu->rob->retire_instruction_from_ROB();
                    AnyInstructionRetired++;
                }

                stage->stalled = 0;
                memCycleSpent = 0;
            } else if (strcmp(stage->opcode, "LOAD") == 0) {
                //First check from the previous checks if the load status is valid.
                int buffer = cpu->data_memory[insToExecMem->m_memory_addr];
                cpu->urf->URF_Table[insToExecMem->m_dest_reg] = buffer;
                cpu->urf->URF_TABLE_valid[insToExecMem->m_dest_reg] = 1;
                cpu->iq->updateIssueQueueEntries(insToExecMem->m_dest_reg,
                                                 buffer);
               // if(AnyInstructionRetired<2) {
                    cpu->rob->retire_instruction_from_ROB();
                  //  AnyInstructionRetired++;
               // }

                //updating bus
                cpu->mem_bus.r = insToExecMem->m_dest_reg;
                cpu->mem_bus.r_value = buffer;
                cpu->lsq->retire_instruction_from_LSQ();

                stage->stalled = 0;
                memCycleSpent = 0;

                memset(stage, 0, sizeof(CPU_Stage));
            }
        }

    }
    return 0;
}

/*
 *  Retire Instruction Stage of APEX Pipeline (WB)
 */
int retireInstruction(APEX_CPU *cpu) {

    if (!cpu->rob->isempty()) {
        //Rob_entry *headEntry = &cpu->rob->rob_queue[cpu->rob->head];

        Rob_entry *headEntry = cpu->rob->get_head_instruction_from_ROB();

        // If its HALT
        map<int, APEX_Instruction *>::iterator itr = (cpu->imap)->find(
                headEntry->m_pc_value);
        if (strcmp(itr->second->opcode, "HALT") == 0) {
            isHalt = TRUE;
            headEntry->setslot_status(UNALLOCATED);
            cpu->rob->retire_instruction_from_ROB();
            cpu->ins_completed++;
            cout<<"HALT succesfull..!!"<<endl;

        } else {
            int rd_status = headEntry->m_status;
            if (rd_status == VALID) {

                // If retiring instruction is BRANCH, add back CFID to free list.
                if (strcmp(itr->second->opcode, "BZ") == 0
                    || strcmp(itr->second->opcode, "BNZ") == 0) {
                    cpu->btb->add_CFID_to_free_list(headEntry->m_CFID);
                }

                headEntry->setslot_status(UNALLOCATED);

                /* Following piece of code needs to be used during branch misprediction.
                 *
                 * It restores the contents of URF and rename table.
                 *
                 *
                 // delme: just testing whether correct snapshot can be copied or not. [ Control flow | Recovery]

                 URF_data *temp;
                 temp = (URF_data*)headEntry->getPv_saved_info();
                 cpu->urf->restoreSnapshot(*temp);

                 */

                //Dont free the register immediately unless it's renamer instruction comes.
                //Just update the register content to B-RAT
                cpu->urf->B_RAT[headEntry->m_architeture_register] =
                        headEntry->m_unified_register;
                //cpu->urf->URF_Table[headEntry->m_unified_register]; //@discuss: changed as per notes.
                cpu->rob->retire_instruction_from_ROB();
                cpu->zero_flag = headEntry->m_excodes;
                cpu->ins_completed++;

                if (ENABLE_DEBUG_MESSAGES) {
                    /*
                     CPU_Stage *stage;
                     int pc_value = headEntry->getPc_value();
                     APEX_Instruction *current_ins = cpu->imap->find(pc_value)->second;
                     strcpy(stage->opcode, current_ins->opcode);
                     stage->rd = current_ins->rd;
                     stage->rs1 = current_ins->rs1;
                     stage->rs2 = current_ins->rs2;
                     stage->imm = current_ins->imm;

                     print_stage_content("ROB Retired Instructions", stage);
                     */
                }
            }
        }
    }
    return 0;
}

/*
 *  APEX CPU simulation loop
 *
 *  Note : You are free to edit this function according to your
 * 				 implementation
 */
int APEX_cpu_run(APEX_CPU *cpu) {
    while (1) {

        /* All the instructions committed, so exit */
        if (isHalt == TRUE) {
            printf("(apex) >> Simulation Complete");
            break;
        }

        if (ENABLE_DEBUG_MESSAGES) {
            printf("\n\n\n\n\n");
            printf("--------------------------------\n");
            printf("Clock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------\n");
        }

        if(AnyInstructionRetired == 0)
        {
            retireInstruction(cpu);
            retireInstruction(cpu);

            AnyInstructionRetired+=2;

            } else if(AnyInstructionRetired == 1){
            retireInstruction(cpu);
            AnyInstructionRetired+=1;
        }

        retireInstruction(cpu);
        memFU(cpu);
        AnyInstructionRetired = 0; // reset
        intFU(cpu);
        mulFU(cpu);
        addToQueues(cpu);
        decode(cpu);
        fetch(cpu);
        cpu->clock++;
    }

    return 0;
}


void simulate(APEX_CPU* cpu)
{
    // STATE OF ARCHITECTURAL REGISTER FILE
    printf("\n");
    printf("=============== STATE OF URF ==========");
    printf("\n");

    cpu->urf->print_urf();

    // STATE OF DATA MEMORY
    printf("\n\n============== STATE OF DATA MEMORY =============\n\n");
    for (int i = 0; i < 15; i++) {
        printf("|\tMEM[%d]\t|\tData Value = %d\t|\n", i, cpu->data_memory[i]);
    }
}


int
APEX_cpu_run_for_cycles(APEX_CPU* cpu, int iNoOfCycles, int iAction) {
    while (1) {

        if(isHalt || (cpu->clock == iNoOfCycles))
        {
            simulate(cpu);
            break;
        }


        if (iAction == 2) //display
        {
            printf("\n");
            printf("---------------- CLOCK CYCLE %d ---------------------------\n", cpu->clock+1);
            printf("\n");
        }


        if(AnyInstructionRetired == 0)
        {
            retireInstruction(cpu);
            retireInstruction(cpu);

            AnyInstructionRetired+=2;

        } else if(AnyInstructionRetired == 1){
            retireInstruction(cpu);
            AnyInstructionRetired+=1;
        }

        memFU(cpu);
        AnyInstructionRetired = 0; // reset
        intFU(cpu);
        mulFU(cpu);
        addToQueues(cpu);
        decode(cpu);
        fetch(cpu);
        cpu->clock++;
    }

    return 0;
}

