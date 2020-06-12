/*
 * ForwardBus.h
 *
 *  Created on: 02-Dec-2018
 *      Author: sagar
 */

#ifndef FORWARDBUS_H_
#define FORWARDBUS_H_
#include "helper.h"
using namespace std;
class ForwardBus {
public:
	int urfId_intFU;
	int urfValue_intFU;
	int urfId_memFU;
	int urfValue_memFU;
	int urfId_mulFU;
	int urfValue_mulFU;
	ForwardBus() {
		urfId_intFU = GARBAGE;
		urfValue_intFU = GARBAGE;
		urfId_memFU = GARBAGE;
		urfValue_memFU = GARBAGE;
		urfId_mulFU = GARBAGE;
		urfValue_mulFU = GARBAGE;
	}
};

#endif /* FORWARDBUS_H_ */
