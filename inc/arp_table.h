//
// Created by root on 21/10/18.
//

#ifndef XARPD_ARPTABLE_H
#define XARPD_ARPTABLE_H

#include <vector>
#include <string.h>
#include "types.h"
#include "pthread.h"

using namespace std;

class arp_table {
private:
    vector<arp_table_entry*> *table;
    pthread_t *timer_thread;

    void dispatch_timer_thread(arp_table *ctx);

    unsigned int defaultTtl;

public:
    arp_table();

    arp_table_entry *get(unsigned int index);
    arp_table_entry *find_by_ip(unsigned int ip);
    arp_table_entry *find_by_eth(unsigned char eth[]);

    void add(unsigned int ip_address, unsigned char eth_address[], unsigned int ttl);
    void add(unsigned int ip_address, unsigned char eth_address[]);

    bool remove(unsigned int ip);

    void setTtl(unsigned int ip);

    unsigned long count();
};

#endif //XARPD_ARPTABLE_H
