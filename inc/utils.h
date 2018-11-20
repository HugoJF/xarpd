//
// Created by root on 22/10/18.
//

#ifndef XARPD_UTILS_H
#define XARPD_UTILS_H

#include "types.h"

unsigned char *parse_eth_addr(char *addr);

unsigned int parse_ip_addr(char *filter);

void print_eth_address(char *s, unsigned char eth_addr[]);

void print_ip_addr(char *s, unsigned int ip_addr) ;

void print_iface(iface *iff);

void print_arp_table_entry(arp_table_entry *ent) ;

bool eth_address_eq(unsigned char *eth_a, unsigned char *eth_b);

void build_arp_header(const char *data, arp_hdr *hdr);

#endif //XARPD_UTILS_H
