#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include "../inc/utils.h"
#include "../inc/types.h"
#include "../inc/interface_worker.h"

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

/**
 * Prints Ethernet address
 *
 * @param s - prefix to print
 * @param eth_addr - ethernet address
 */
void print_eth_address(char *s, unsigned char eth_addr[]) {
    printf("%s%02X:%02X:%02X:%02X:%02X:%02X", s,
           eth_addr[0], eth_addr[1], eth_addr[2],
           eth_addr[3], eth_addr[4], eth_addr[5]);
}

/**
 * Prints IP address
 *
 * @param s - prefix to print
 * @param ip_addr - ip address
 */
void print_ip_addr(char *s, unsigned int ip_addr) {
    printf("%s%u.%u.%u.%u", s,
           (unsigned char) (ip_addr >> 24),
           (unsigned char) (ip_addr >> 16),
           (unsigned char) (ip_addr >> 8),
           (unsigned char) (ip_addr >> 0));
}

/**
 * Prints ARP table
 *
 * @param ent - entry to print
 */
void print_arp_table_entry(arp_table_entry *ent) {
    print_ip_addr((char *) "(", ent->ipAddress);
    printf(", ");
    print_eth_address((char *) "", ent->ethAddress);
    printf(", %d)\n", ent->ttl);
}
/**
 * Prints IFace data
 *
 * @param iff - iface object
 */
void print_iface(iface *iff) {
    printf("======== %s ========\n=>\n",iff->ifname);
    printf("=>\tLink encap: Ethernet\n");
    printf("=>\tMAC Address: ");
    print_eth_address((char*) "", iff->mac_addr);
    printf("\n");
    print_ip_addr((char*) "=>\tInet end: ", iff->ip_addr);
    printf("\n");
    print_ip_addr((char*) "=>\tBcast: ", iff->ip_addr | ~iff->netmask);
    printf("\n");
    print_ip_addr((char*) "=>\tNetmask: ", iff->netmask);
    printf("\n");
    printf("=>\tUP MTU: %d\n", iff->mtu);
    printf("=>\tRX packets: %d TX packets: %d\n", iff->rx_pkts, iff->tx_pkts);
    printf("=>\tRX bytes: %d TX bytes: %d\n", iff->rx_bytes, iff->tx_bytes);
    printf("=>\n======== %s ========\n\n",iff->ifname);
}

/**
 * Compares 2 Ethernet address
 *
 * @param eth_a - address a
 * @param eth_b - address b
 *
 * @return - true if equal, false if not
 */
bool eth_address_eq(unsigned char *eth_a, unsigned char *eth_b) {
    for (int i = 0; i < 6; ++i) {
        if (eth_a[i] != eth_b[i]) {
            return false;
        }
    }

    return true;
}

/**
 * Fixes variable length fields
 *
 * @param data - raw data
 * @param hdr - header to fix
 */
void build_arp_header(const char *data, arp_hdr *hdr) {
    char hl = hdr->hardware_length;
    char pl = hdr->protocol_length;
    unsigned int off = 8 + sizeof(eth_hdr);

    // Copies header data with correct header
    memcpy(hdr->sender_mac, data + off, hl);
    memcpy(&hdr->sender_ip, data + off + hl, pl);
    memcpy(hdr->destination_mac, data + off + hl + pl, hl);
    memcpy(&hdr->destination_ip, data + off + hl + pl + hl, pl);
}

/**
 * Finds Interface Worker object that contains handles network for given IP
 *
 * @param ip - ip to match interface
 *
 * @return - interface worker pointer
 */
interface_worker *find_interface_worker(unsigned int ip, interface_worker **workers, int worker_count) {
    // Loops for each worker
    for (int i = 0; i < worker_count; ++i) {
        // Grab reference
        interface_worker *ifw = workers[i];

        // Get need information
        unsigned int mask = ifw->iface_data->netmask;
        unsigned int net_if = mask & ifw->iface_data->ip_addr;
        unsigned int net_ip = mask & ip;

        // Debug
        printf("Attempting to solve: ");
        print_ip_addr((char*) "", net_if);
        printf(" == ");
        print_ip_addr((char*) "", net_ip);
        printf("\n");

        // Check if interface and ip are on the same network
        if(net_if == net_ip) {
            return ifw;
        }
    }

    return nullptr;
}


/**
 * Find interface by name
 *
 * @param eth - iface name
 *
 * @return - iface worker reference
 */
interface_worker *find_interface_worker_by_name(char eth[23], interface_worker **workers, int worker_count) {
    // Loops for each worker
    for (int i = 0; i < worker_count; ++i) {
        if(strcmp(eth, workers[i]->iface_data->ifname) == 0) {
            return workers[i];
        }
    }

    return nullptr;
}