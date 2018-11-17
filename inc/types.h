//
// Created by root on 21/10/18.
//

#ifndef XARPD_TYPES_H
#define XARPD_TYPES_H

#define MAX_IFNAME_LEN 22
#define HW_ADDR_LEN 6

#define ARP_REQUEST 1
#define ARP_REPLY 2

struct iface {
    int sockfd;
    int ttl;
    int mtu;
    char ifname[MAX_IFNAME_LEN];
    unsigned char mac_addr[HW_ADDR_LEN];
    unsigned int ip_addr;
    unsigned int rx_pkts;
    unsigned int rx_bytes;
    unsigned int tx_pkts;
    unsigned int tx_bytes;
    int index;
    unsigned int netmask;
};

typedef struct _ether_hdr {
    unsigned char ether_dhost[HW_ADDR_LEN];   // Destination address
    unsigned char ether_shost[HW_ADDR_LEN];   // Source address
    unsigned short ether_type;      // Type of the payload
} eth_hdr;

// TODO: needs fix to make it support variable length fields
typedef struct _arp_hdr {
    unsigned short hardware_type;
    unsigned short protocol_type;
    unsigned char hardware_length;
    unsigned char protocol_length;
    unsigned short opcode;
    unsigned char sender_mac[HW_ADDR_LEN];
    unsigned int sender_ip;
    unsigned char destination_mac[HW_ADDR_LEN];
    unsigned int destination_ip;
} arp_hdr;

static unsigned short COMMAND_SHOW = 1;
static unsigned short COMMAND_RES = 2;
static unsigned short COMMAND_ADD = 3;
static unsigned short COMMAND_DEL = 4;
static unsigned short COMMAND_TTL = 5;
static unsigned short COMMAND_DEL_NOT_FOUND = 6;

typedef struct _command_hdr {
    unsigned short type;
    unsigned int ip;
    unsigned char eth[8];
    unsigned int ttl;
} command_hdr;

typedef struct _response_hdr {
    unsigned short type;
    unsigned short len;
} response_hdr;


typedef struct _arpTableEntry{
    unsigned int ipAddress;
    unsigned int ttl;
    unsigned char ethAddress[6];
} arp_table_entry;

#endif //XARPD_TYPES_H
