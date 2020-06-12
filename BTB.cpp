//
// @author: sonu 
// @date: 12/10/18.
//

#include "BTB.h"
#include "helper.h"
#include <algorithm>
#include <queue>

BTB::BTB() {
    for(int i = 1; i<=CFID_SIZE;i++)
    {
        free_CFID_list.push_back(i);
    }
    last_control_flow_instr = -1;
}

int BTB::get_next_free_CFID() {
        int reg = -1;

        if (!free_CFID_list.empty()) {
            reg = free_CFID_list.front();
            free_CFID_list.pop_front();
        }
        return reg;
}

bool BTB::add_CFID_to_free_list(int cfid) {

    // remove it from instruction order and add to free list.
    deque<int> ::iterator itr = find(CF_instn_order.begin(), CF_instn_order.end(), cfid);

    if(itr!= CF_instn_order.end()) {
        CF_instn_order.erase(itr);
        free_CFID_list.push_back(cfid);
        return true;
    }
    else
        return false;
}

bool BTB::update_prediction(int cfid, int pc, int prediction) {

    // Before inserting check if cfid exists in instr_order_list.

    deque<int> ::iterator itr = find(CF_instn_order.begin(), CF_instn_order.end(), cfid);
    if(itr!= CF_instn_order.end()) {

        prediction_table[cfid].pc = pc;
        prediction_table[cfid].last_prediction = prediction;
        return true;
    }
    else
        return false;
}

bool BTB::add_cfid(int cfid) {
    if(CF_instn_order.size() <=CFID_SIZE) {
        CF_instn_order.push_back(cfid);
        last_control_flow_instr = cfid;     // this will act most recent instruction
        return true;
    }
    else
        return false;
}

int BTB::get_last_prediction(int pc) {

    //@discuss can there be conflicts????????????????????
    for(int i = 1; i<=CFID_SIZE;i++)
    {
        if(prediction_table[i].pc == pc)
            return prediction_table[i].last_prediction;
    }

    return GARBAGE;
}
