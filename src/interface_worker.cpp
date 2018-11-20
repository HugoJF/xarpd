//
// Created by root on 21/10/18.
//

#include "../inc/utils.h"
#include "../inc/interface_worker.h"
#include "../inc/arp_table.h"
#include <net/if.h>         // ifreq
#include <net/ethernet.h>   // ETH_P_ALL
#include <linux/if_packet.h>// sockaddr_ll
#include <linux/if_arp.h>   // ARPHRD_ETHER
#include <sys/ioctl.h>      // SIOCGIFHWADDR
#include <sys/socket.h>     // socket()
#include <sys/types.h>      // socket()
#include <arpa/inet.h>      // htons
#include <string.h>         // strerror
#include <errno.h>          // errno
#include <unistd.h>         // close
#include <mutex>
#include "pthread.h"

#define ETH_ADDR_LEN 6
#define BUFFER_SIZE 1024
#define DEFAULT_MTU 1500

/**
 * Interface reader thread
 *
 * @param ctx - interface worker context
 *
 * @return void
 */
void *reader(void *ctx) {
    auto *ir = (interface_worker *) ctx;

    while (true) {
        // Prepare buffers
        char buffer[BUFFER_SIZE + 1];
        auto data = new std::string();
        unsigned int data_size = 0;
        memset(buffer, '\0', BUFFER_SIZE);

        // Keep reading until the end
        long segment_size = BUFFER_SIZE;
        while (segment_size == BUFFER_SIZE && data_size < BUFFER_SIZE) {
            // Read data to buffer
            segment_size = (unsigned long) read(ir->iface_data->sockfd, buffer, BUFFER_SIZE);

            // Check for errors
            if (segment_size < 0) {
                printf("ERROR: %s\n", strerror(errno));

                return nullptr;
            } else if (segment_size > BUFFER_SIZE) {
                // This should only happen with read() ignores max buffer size
                printf("Reading buffer overflow\n");
                exit(1);
            } else {
                // Push data to string buffer
                data_size += segment_size;
                data->append(buffer, (unsigned long) segment_size);
            }
        }

        // Process received packet
        ir->process_packet(data->c_str(), data_size);
    }
}

/**
 * Process raw packet data
 *
 * @param data - raw data
 * @param length - total data length
 */
void interface_worker::process_packet(const char *data, unsigned int length) {
    // Ethernet data
    auto *eth = (eth_hdr *) data;
    unsigned int eth_data_length = length - sizeof(eth_hdr);

    // If frame is directed to this interface
    bool me = eth_address_eq(this->iface_data->mac_addr, eth->ether_dhost);

    // Processing should only continue if packet is ARP
    if (ntohs(eth->ether_type) == ETH_P_ARP) {
        // Copy data from Ethernet frame to form ARP packet
        char *arp_data = new char[eth_data_length];
        memcpy(arp_data, data + sizeof(eth_hdr), eth_data_length);
        auto *arp = (arp_hdr *) arp_data;
        printf("Received ARP packet: %d\n", ntohs(arp->opcode));

        // Fix variable length fields
        build_arp_header(data, arp);

        // Check what kind of ARP operation we received
        if (ntohs(arp->opcode) == ARP_REQUEST) {
            // Debug
            print_ip_addr((char *) "Received ARP request for: ", ntohl(arp->destination_ip));
            printf("\n");

            // Check if we can reply this request
            arp_table_entry *entry = this->table->find_by_ip(ntohl(arp->destination_ip));

            // Reply request if we have an entry
            if (entry != nullptr) {
                printf("Found entry in ARP table\n");
                // Reply request if entry exists
                this->reply_arp(arp, entry);
            } else {
                printf("No entry found in ARP table\n");
            }
        } else if (ntohs(arp->opcode) == ARP_REPLY) {
            // Debug
            print_ip_addr((char *) "Received ARP reply from: ", ntohl(arp->sender_ip));
            printf("\n");

            // Learn new entry from reply
            this->table->add(ntohl(arp->sender_ip), arp->sender_mac);
        }

    }
}

/**
 * Dispatch interface worker thread
 *
 * @param ctx - reader context
 */
void dispatch_reader(interface_worker *ctx) {
    // Prepare thread data
    ctx->readerThread = new pthread_t();
    auto *attributes = new pthread_attr_t();

    // Dispatch thread
    if (pthread_create(ctx->readerThread, nullptr, reader, (void *) ctx)) {
        perror("pthreads()");
        exit(errno);
    }
}

/**
 * Constructor
 *
 * @param iface_name - interface name
 * @param main - arp table
 */
interface_worker::interface_worker(string *iface_name, arp_table *main) {
    this->iface_name = iface_name;
    this->iface_data = new iface;
    this->set_table(main);
}

/**
 * Bind worker to interface
 */
void interface_worker::bind() {
    // Create socket
    this->rawsockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    // Check for errors
    if (rawsockfd < 0) {
        fprintf(stderr, "ERROR: %s\n", strerror(errno));
        exit(errno);
    }

    // Bind socket to interface
    if (bind_iface_name(rawsockfd, (char *) this->iface_name->c_str()) < 0) {
        perror("Server-setsockopt() error for SO_BINDTODEVICE");
        printf("%s\n", strerror(errno));
        close(this->rawsockfd);
        exit(errno);
    }

    // Query interface information
    this->get_iface_info(rawsockfd, (char *) this->iface_name->c_str(), this->iface_data);

    // Debug iface data
    print_iface(this->iface_data);

    // Print current interface Ethernet address
    print_eth_address(iface_data->ifname, iface_data->mac_addr);

    // Dispatch reader thread
    dispatch_reader(this);
}

/**
 * Bind interface by name
 *
 * @param fd - socket file descriptor
 * @param iface_name - interface name
 *
 * @return - return value of setsockopt function
 */
int interface_worker::bind_iface_name(int fd, char *iface_name) {
    return setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface_name, (socklen_t) strlen(iface_name));
}

/**
 * Queries interface information
 *
 * @param sockfd - socket file descriptor for interface
 * @param ifname - interface name
 * @param ifn  - interface information object
 */
void interface_worker::get_iface_info(int sockfd, char *ifname, iface *ifn) {
    /*
     * Structure allocations
     */
    struct ifreq s{};
    struct ifreq if_idx{};
    struct ifreq netmask{};
    struct ifreq ipaddr{};

    ifn->mtu = DEFAULT_MTU;

    /*
     * Network Mask
     */
    strcpy(netmask.ifr_name, ifname);
    if (0 == ioctl(sockfd, SIOCGIFNETMASK, &netmask)) {
        for (int i = 0; i < 4; ++i) {
            ifn->netmask += (unsigned char) netmask.ifr_netmask.sa_data[i + 2] << (24 - (i * 8));
        }
        printf("NetMask: %08X\n", ifn->netmask);
    } else {
        perror("Error getting NetMask");
        exit(errno);
    }

    /*
     * IP Address
     */
    strcpy(ipaddr.ifr_name, ifname);
    if (0 == ioctl(sockfd, SIOCGIFADDR, &ipaddr)) {
        for (int i = 0; i < 4; ++i) {
            ifn->ip_addr += (unsigned char) ipaddr.ifr_addr.sa_data[i + 2] << (24 - (i * 8));
        }
    } else {
        perror("Error getting IP Address");
        exit(errno);
    }

    /*
     * MAC Address
     */
    strcpy(s.ifr_name, ifname);
    if (0 == ioctl(sockfd, SIOCGIFHWADDR, &s)) {
        memcpy(&ifn->mac_addr, &s.ifr_addr.sa_data, ETH_ADDR_LEN);
        ifn->sockfd = sockfd;
        strcpy(ifn->ifname, ifname);
    } else {
        perror("Error getting MAC address");
        exit(errno);
    }

    /*
     * Interface index
     */
    strncpy(if_idx.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("SIOCGIFINDEX");
        exit(errno);
    }
    printf("IF is running at %d==%d\n", if_idx.ifr_ifindex, s.ifr_ifru.ifru_ivalue);
    ifn->index = if_idx.ifr_ifindex;
}

/**
 * Set reference to ARP table
 *
 * @param table - table pointer
 */
void interface_worker::set_table(arp_table *table) {
    this->table = table;
}

/**
 * Build reply ARP header
 *
 * @param arp - arp header to build
 * @param entry - arp entry to respond
 */
void interface_worker::reply_arp(arp_hdr *arp, arp_table_entry *entry) {
    auto *p = new arp_hdr;
    auto *sa = new sockaddr_ll;
    char *pck = new char[14 + sizeof(arp_hdr)]; // 14 = Ethernet header size

    /*
     * Socket address
     */
    sa->sll_ifindex = this->iface_data->index;
    sa->sll_family = PF_PACKET;
    sa->sll_halen = ETH_ALEN;
    memcpy(sa->sll_addr, arp->sender_mac, sizeof(char) * 6);

    /*
     * ARP header placeholder
     */
    p->hardware_type = htons(1);
    p->protocol_type = htons(ETH_P_IP);
    memcpy(p->sender_mac, entry->ethAddress, sizeof(char) * 6);
    memcpy(p->destination_mac, arp->sender_mac, sizeof(char) * 6);
    p->sender_ip = htonl(entry->ipAddress);
    p->destination_ip = htonl(arp->sender_ip);
    p->opcode = htons(ARP_REPLY);
    p->hardware_length = HW_ADDR_LEN;
    p->protocol_length = sizeof(this->iface_data->ip_addr);

    /*
     * Build Ethernet header
     */
    memcpy(pck, arp->sender_mac, sizeof(char) * 6);
    memcpy(pck + 6, entry->ethAddress, sizeof(char) * 6);
    pck[12] = ETH_P_ARP / 256;
    pck[13] = ETH_P_ARP % 256;

    // Point to end of Ethernet header
    char *pt = pck + 14;

    /*
     * Build ARP header
     */
    memcpy(pt, &p->hardware_type, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, &p->protocol_type, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, &p->hardware_length, sizeof(char));
    pt += sizeof(char);

    memcpy(pt, &p->protocol_length, sizeof(char));
    pt += sizeof(char);

    memcpy(pt, &p->opcode, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, p->sender_mac, sizeof(char) * 6);
    pt += sizeof(char) * 6;

    memcpy(pt, &p->sender_ip, sizeof(int));
    pt += sizeof(int);

    memcpy(pt, p->destination_mac, sizeof(char) * 6);
    pt += sizeof(char) * 6;

    memcpy(pt, &p->destination_ip, sizeof(int));
    pt += sizeof(int);

    /*
     * Send raw frame
     */
    if (sendto(rawsockfd, pck, sizeof(arp_hdr) + 14, 0, (struct sockaddr *) sa, sizeof(struct sockaddr_ll)) < 0) {
        perror("sendto");
        exit(errno);
    }

    printf("Sent!\n");
}

/**
 * Resolve Ethernet address of given IP
 *
 * @param ip - ip to solve
 */
void interface_worker::resolve_ip(unsigned int ip) {
    print_ip_addr((char *) "Resolving: ", ip);
    printf("\n");

    // Send ARP request
    this->arp_request(ip);
}

/**
 * Send ARP request to network
 *
 * @param ip - ip address to arp request
 */
void interface_worker::arp_request(unsigned int ip) {
    auto *p = new arp_hdr;
    auto *sa = new sockaddr_ll;
    char *pck = new char[14 + sizeof(arp_hdr)]; // 14 = Ethernet header size

    /*
     * Build fixed Ethernet addresses
     */
    unsigned char request_mac[6];
    unsigned char broadcast_mac[6];
    for (int i = 0; i < 6; ++i) {
        broadcast_mac[i] = (unsigned char) 255;
        request_mac[i] = (unsigned char) 0;
    }

    /*
     * Socket address
     */
    sa->sll_ifindex = this->iface_data->index;
    sa->sll_family = PF_PACKET;
    sa->sll_halen = ETH_ALEN;
    memcpy(sa->sll_addr, this->iface_data->mac_addr, sizeof(char) * 6);

    /*
     * Build ARP header placeholder
     */
    p->hardware_type = htons(1);
    p->protocol_type = htons(ETH_P_IP);
    memcpy(p->sender_mac, this->iface_data->mac_addr, sizeof(char) * 6);
    memcpy(p->destination_mac, request_mac, sizeof(char) * 6);
    p->sender_ip = htonl(this->iface_data->ip_addr);
    p->destination_ip = htonl(ip);
    p->opcode = htons(ARP_REQUEST);
    p->hardware_length = HW_ADDR_LEN;
    p->protocol_length = sizeof(this->iface_data->ip_addr);

    /*
     * Build Ethernet header
     */
    memcpy(pck, broadcast_mac, sizeof(char) * 6);
    memcpy(pck + 6, this->iface_data->mac_addr, sizeof(char) * 6);
    pck[12] = ETH_P_ARP / 256;
    pck[13] = ETH_P_ARP % 256;

    // Point to end of Ethernet header
    char *pt = pck + 14;

    /*
     * Build actual ARP header
     */
    memcpy(pt, &p->hardware_type, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, &p->protocol_type, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, &p->hardware_length, sizeof(char));
    pt += sizeof(char);

    memcpy(pt, &p->protocol_length, sizeof(char));
    pt += sizeof(char);

    memcpy(pt, &p->opcode, sizeof(short));
    pt += sizeof(short);

    memcpy(pt, p->sender_mac, sizeof(char) * 6);
    pt += sizeof(char) * 6;

    memcpy(pt, &p->sender_ip, sizeof(int));
    pt += sizeof(int);

    memcpy(pt, p->destination_mac, sizeof(char) * 6);
    pt += sizeof(char) * 6;

    memcpy(pt, &p->destination_ip, sizeof(int));
    pt += sizeof(int);

    /*
     * Send raw frame
     */
    if (sendto(rawsockfd, pck, sizeof(arp_hdr) + 14, 0, (struct sockaddr *) sa, sizeof(struct sockaddr_ll)) < 0) {
        perror("sendto");
        exit(errno);
    }

    printf("Sent!\n");
}
