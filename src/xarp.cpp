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

void print_usage();

void send_command(command_hdr *cmd);

void send_if_show();

void send_set_ttl(int ttl);

void send_del(unsigned int ip);

void send_add(unsigned int ip, unsigned char mac[], unsigned int ttl);

void send_res(unsigned int ip);

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
    if (strcmp(args[1], "show") == 0 && argc == 2) {
        send_if_show();
    } else if (strcmp(args[1], "ttl") == 0 && argc == 3) {
        auto ttl = (unsigned int) strtol(args[2], nullptr, 10);

        send_set_ttl(ttl);
    } else if (strcmp(args[1], "del") == 0 && argc == 3) {
        unsigned int ip = parse_ip_addr(args[2]);

        send_del(ip);
    } else if (strcmp(args[1], "add") == 0 && argc == 5) {
        unsigned int ip = parse_ip_addr(args[2]);
        unsigned char *eth = parse_eth_addr(args[3]);
        auto ttl = (unsigned int) strtol(args[4], nullptr, 10);

        send_add(ip, eth, ttl);
    } else if (strcmp(args[1], "res") == 0 && argc == 2) {
        unsigned int ip = parse_ip_addr(args[2]);

        send_res(ip);
    } else {
        printf("Unrecognized command: %s\n", args[1]);
        print_usage();
    }
}

void print_usage() {
    printf("Commands:\n"
           "1. xarp show\n"
           "2. xarp ttl <ttl>\n"
           "3. xarp del <ip>\n"
           "4. xarp add <ip> <mac> <ttl>\n"
           "5. xarp res <ip>\n");
}

/**
 * Send show command
 */
void send_if_show() {
    // Get new command
    auto cmd = get_fresh_cmd();

    // Set command to show
    cmd->type = COMMAND_SHOW;

    // Send command
    send_command(cmd);
//    printf("Sent\n");

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        unsigned int entry_count = res->len / sizeof(arp_table_entry);
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);
//        printf("Received %d table entries\n", entry_count);

        // Print each ARP entry
        for (int i = 0; i < entry_count; ++i) {
            auto *ent = (arp_table_entry *) (buffer + sizeof(response_hdr) + (sizeof(arp_table_entry) * i));

            print_arp_table_entry(ent);
        }

        if(entry_count == 0) {
            printf("ARP table is empty\n");
        }

    }
}

/**
 * Send TTL update command
 *
 * @param ttl - update ttl
 */
void send_set_ttl(int ttl) {
    // Get new command
    auto cmd = get_fresh_cmd();

    // Set command to TTL
    cmd->type = COMMAND_TTL;
    cmd->ttl = (unsigned short) ttl;

    // Send command
    send_command(cmd);
//    printf("Sent\n");

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // Feedback user
        if (res->type == COMMAND_TTL) {
            printf("TTL set successfully!\n");
        } else {
            printf("ERROR setting new TTL\n");
        }
    }
}

/**
 * Send delete command
 *
 * @param ip - ip to delete
 */
void send_del(unsigned int ip) {
    // Get new command
    auto cmd = get_fresh_cmd();

    // Set command to delete
    cmd->type = COMMAND_DEL;
    cmd->ip = ip;

    // Send command
    send_command(cmd);
//    printf("Sent\n");

    // Receive response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // Feedback user
        if (res->type == COMMAND_DEL) {
            printf("IP deleted successfully!\n");
        } else if (res->type == COMMAND_DEL_NOT_FOUND) {
            printf("IP could not be found\n");
        } else {
            printf("ERROR deleting IP from ARP table\n");
        }
    }
}

/**
 * Send add command
 *
 * @param ip - ip of ARP entry
 * @param mac - mac of ARP entry
 * @param ttl - ttl of ARP entry
 */
void send_add(unsigned int ip, unsigned char mac[], unsigned int ttl) {
    // Get new command header
    auto cmd = get_fresh_cmd();

    // Set command to add
    cmd->type = COMMAND_ADD;
    cmd->ip = ip;
    memcpy(cmd->eth, mac, sizeof(char) * 6);
    cmd->ttl = ttl;

    // Send to daemon
    send_command(cmd);
//    printf("Sent\n");

    // Wait for response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // Print feedback to user
        if (res->type == COMMAND_ADD) {
            printf("Table entry added successfully!\n");
        }
    }
}

/*
 * Send resolve command
 */
void send_res(unsigned int ip) {
    // Get new command header
    auto cmd = get_fresh_cmd();

    // Set to resolve
    cmd->type = COMMAND_RES;
    cmd->ip = ip;

    // Send to daemon
    send_command(cmd);
//    printf("Sent\n");

    // Wait response
    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
//        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        // If IP was resolved successfully, add new entry to ARP table
        if (res->len == sizeof(arp_table_entry)) {
            printf("Sucessfully resolved IP:\n");
            print_arp_table_entry((arp_table_entry *) (buffer + sizeof(response_hdr)));
        } else {
            printf("Could not resolve IP\n");
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
//    printf("Connecting...\n");

    // Connect socket
    if (connect(listenFd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        perror("ERROR connecting to socket");
        exit(errno);
    }

//    printf("Connected.\n");
}

/**
 * Build socket to daemon
 */
void build_socket() {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

//    printf("Socket: %d\n", listenFd);

    // Set address configurations
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
}
