//
// @author: sonu 
// @date: 12/10/18.
//

#ifndef BTB_BTB_H
#define BTB_BTB_H

#include <iostream>
#include <deque>
#include "helper.h"
using namespace std;

struct BTS
{
    int pc;
    int last_prediction;

    BTS()
    {
        pc = GARBAGE;
        last_prediction = 1; // initially, branch is taken
    }
};

class BTB
{
public:
    deque<int> CF_instn_order;
    deque<int> free_CFID_list;
    int last_control_flow_instr;

    BTS prediction_table[7]; // Max 8 entries.

    // methods
    BTB();
    int get_next_free_CFID();
    bool add_CFID_to_free_list(int cfid);
    bool add_cfid(int cfid);                // This shud be called immediately after get_next_free_CFID()
    bool update_prediction(int cfid, int pc, int prediction);
    int get_last_prediction(int pc);
};

#endif //BTB_BTB_H
