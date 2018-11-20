//
// Created by root on 21/10/18.
//

#ifndef XARPD_INTERFACE_READER_H
#define XARPD_INTERFACE_READER_H


#include "types.h"
#include "pthread.h"
#include "arp_table.h"
#include <string>



using namespace std;

class interface_worker {
private:
    arp_table *table;
    string *iface_name;
    int rawsockfd;

    int bind_iface_name(int fd, char *iface_name);
    void get_iface_info(int sockfd, char *ifname, iface *ifn);
public:
    iface *iface_data;
    pthread_t *readerThread;

    interface_worker(string *iface_name, arp_table* main);

    void set_table(arp_table *table);
    void bind();
    void process_packet(const char *data, unsigned int length);

    void reply_arp(arp_hdr *arp, arp_table_entry *pEntry);
    void arp_request(unsigned int ip);

    void resolve_ip(unsigned int i);

};


#endif //XARPD_INTERFACE_READER_H
