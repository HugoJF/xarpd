#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include "../inc/utils.h"
#include "../inc/types.h"

/**
 * Parses Ethernet address to 6 bytes
 *
 * @param addr - ethernet address in string form
 *
 * @return - ethernet address in byte form
 */
unsigned char *parse_eth_addr(char *addr) {
    // 6 bytes used by an Ethernet Address
    auto *ether = new unsigned char[(sizeof(unsigned char) * 6)];

    // Copy string to use in strtok
    auto *copy = new char[(sizeof(char) * (strlen(addr) + 1))];
    strcpy(copy, addr);

    // Parse text value to byte
    ether[0] = (unsigned char) strtol(strtok(copy, ":"), nullptr, 16);
    for (int j = 1; j < 6; ++j) {
        ether[j] = (unsigned char) strtol(strtok(nullptr, ":"), nullptr, 16);
    }

    return ether;
}

/**
 * Parses IP address to int form
 *
 * @param filter - ip address in string form
 *
 * @return - ip address in int form
 */
unsigned int parse_ip_addr(char *filter) {
    // Resulting int IP
    int unsigned result = 0;

    // Parsed IP segments
    int *ipp = new int[(sizeof(int) * 4)];

    // 4 IP address segments
    char **ip = new char *[(sizeof(char *) * 4)];

    // Copy string to use in strtok
    char *copy = new char[(sizeof(char) * (strlen(filter) + 1))];
    strcpy(copy, filter);

    // Split IP String
    ip[0] = strtok(copy, ".");
    for (int i = 1; i < 4; ++i) {
        ip[i] = strtok(nullptr, ".");
    }

    // Parse to int
    for (int i = 0; i < 4; ++i) {
        ipp[i] = (int) strtol(ip[i], nullptr, 10);
    }

    for (int i = 0; i < 4; ++i) {
        result += ipp[i] << ((3 - i) * 8);
    }

    // Return IP number array
    return result;
}

void print_eth_address(char *s, unsigned char eth_addr[]) {
    printf("%s%02X:%02X:%02X:%02X:%02X:%02X", s,
           eth_addr[0], eth_addr[1], eth_addr[2],
           eth_addr[3], eth_addr[4], eth_addr[5]);
}

void print_ip_addr(char *s, unsigned int ip_addr) {
    printf("%s%u.%u.%u.%u", s,
           (unsigned char) (ip_addr >> 24),
           (unsigned char) (ip_addr >> 16),
           (unsigned char) (ip_addr >> 8),
           (unsigned char) (ip_addr >> 0));
}

void print_arp_table_entry(arp_table_entry *ent) {
    print_ip_addr((char *) "(", ent->ipAddress);
    printf(", ");
    print_eth_address((char *) "", ent->ethAddress);
    printf(", %d)\n", ent->ttl);
}

bool eth_address_eq(unsigned char *eth_a, unsigned char *eth_b) {
    for (int i = 0; i < 6; ++i) {
        if (eth_a[i] != eth_b[i]) {
            return false;
        }
    }

    return true;
}

void build_arp_header(const char *data, arp_hdr *hdr) {
    char hl = hdr->hardware_length;
    char pl = hdr->protocol_length;
    unsigned int off = 8 + sizeof(eth_hdr);

    memcpy(hdr->sender_mac, data + off, hl);
    memcpy(&hdr->sender_ip, data + off + hl, pl);
    memcpy(hdr->destination_mac, data + off + hl + pl, hl);
    memcpy(&hdr->destination_ip, data + off + hl + pl + hl, pl);
}