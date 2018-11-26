#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../inc/types.h"
#include "../inc/utils.h"

/*
 * Socket
 */
void build_socket();

void connect();

/*
 * Commands
 */

void send_command(command_hdr *cmd);

void send_if_show();

void send_if_config(char ifn[MAX_IFNAME_LEN], unsigned int ip, unsigned int mask);

void send_if_mtu(char ifn[MAX_IFNAME_LEN], int mtu);

/*
 * Utils
 */
command_hdr *get_fresh_cmd();

/*
 * Variables
 */
int listenFd;
sockaddr_in addr{};
unsigned short port = 5050;
unsigned int buffer_size = 1024 * 64;
unsigned int received_bytes;
unsigned char *buffer;

int main(int argc, char **args) {
    printf("Commands:\n"
           "1. xifconfig\n"
           "2. xifconfig <interface> <ip> <mask>\n"
           "3. xifconfig <interface> <mtu>\n\n\n");

    /*
     * Prepare socket
     */
    build_socket();
    connect();

    /*
     * Boot
     */
    buffer = new unsigned char[buffer_size];

    /**
     * Call function according to arguments
     */
    if (argc == 1) {
        send_if_show();
    } else if (argc == 4) {
        char *name = args[1];
        unsigned ip = parse_ip_addr(args[2]);
        unsigned mask = parse_ip_addr(args[3]);

        send_if_config(name, ip, mask);
    } else if (argc == 3) {
        char *name = args[1];
        int mtu = (int) strtol(args[2], nullptr, 10);

        send_if_mtu(name, mtu);
    }
}

/**
 * Send show command
 */
void send_if_show() {
    // Get new command
    auto cmd = get_fresh_cmd();

    // Set command to show
    cmd->type = COMMAND_IF_SHOW;

    // Send command
    send_command(cmd);

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        unsigned int entry_count = res->len / sizeof(iface);
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);
//        printf("Received %d interface entries\n", entry_count);

        // Print each ARP entry
        for (int i = 0; i < entry_count; ++i) {
            auto *ent = (iface *) (buffer + sizeof(response_hdr) + (sizeof(iface) * i));

            print_iface(ent);
        }

    }
}

/**
 * Send TTL update command
 *
 * @param ttl - update ttl
 */
void send_if_config(char ifn[MAX_IFNAME_LEN], unsigned int ip, unsigned int mask) {
    // Allocate memory
    auto cmd = get_fresh_cmd();
    auto config = new config_hdr;
    unsigned char data[sizeof(command_hdr) + sizeof(config_hdr)];

    // Set config
    memcpy(&config->eth, ifn, strlen(ifn) + 1);
    config->ip = ip;
    config->mask = mask;
    config->length = 0;

    // Set command to TTL
    cmd->type = COMMAND_IF_CONFIG;

    memcpy(data, cmd, sizeof(command_hdr));
    memcpy(data + sizeof(command_hdr), config, sizeof(config_hdr));

    // Send data
    sendto(listenFd, data, sizeof(command_hdr) + sizeof(config_hdr), 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // Feedback user
        if (res->type == COMMAND_IF_CONFIG) {
            printf("Interface configured successfully!\n");
        } else {
            printf("ERROR setting updating interface\n");
        }
    }
}

/**
 * Send delete command
 *
 * @param ip - ip to delete
 */
void send_if_mtu(char ifn[MAX_IFNAME_LEN], int mtu) {
    // Allocate memory
    auto cmd = get_fresh_cmd();
    auto config = new config_hdr;
    unsigned char data[sizeof(command_hdr) + sizeof(config_hdr)];

    // Set config
    memcpy(&config->eth, ifn, strlen(ifn) + 1);
    config->ip = (unsigned int) mtu;
    config->mask = 0;
    config->length = 0;

    // Set command to TTL
    cmd->type = COMMAND_IF_MTU;

    memcpy(data, cmd, sizeof(command_hdr));
    memcpy(data + sizeof(command_hdr), config, sizeof(config_hdr));

    // Send data
    sendto(listenFd, data, sizeof(command_hdr) + sizeof(config_hdr), 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // Feedback user
        if (res->type == COMMAND_IF_MTU) {
            printf("MTU updated successfully!\n");
        } else {
            printf("ERROR setting new MTU\n");
        }
    }
}

/**
 * Send command header to daemon
 *
 * @param cmd - command header to send
 */
void send_command(command_hdr *cmd) {
    // Send command header to daemon
    sendto(listenFd, cmd, sizeof(command_hdr), 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
}

/**
 * Build a new command header
 *
 * @return - zero filled command header
 */
command_hdr *get_fresh_cmd() {
    auto *cmd = new command_hdr;

    // Reset structure
    memset(cmd, '\0', sizeof(command_hdr));

    return cmd;
}

/**
 * Connect to daemon
 */
void connect() {
    // Connect socket
    if (connect(listenFd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        perror("ERROR connecting to socket");
        exit(errno);
    }
}

/**
 * Build socket to daemon
 */
void build_socket() {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

    // Set address configurations
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
}