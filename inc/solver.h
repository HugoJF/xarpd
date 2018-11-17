//
// Created by root on 21/10/18.
//

#ifndef XARPD_SOLVER_H
#define XARPD_SOLVER_H

#include "arp_table.h"

class solver {
private:
public:
    solver(arp_table *maintable);

    void solve(unsigned int ipAddress);
};


#endif //XARPD_SOLVER_H
