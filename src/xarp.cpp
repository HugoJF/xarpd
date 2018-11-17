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

void send_show();

void send_ttl(int ttl);

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
    printf("Commands:\n"
           "1. xarp show\n");

    build_socket();

    connect();

    /*
     * Boot
     */
    buffer = new unsigned char[buffer_size];


    if (strcmp(args[1], "show") == 0) {
        send_show();
    } else if (strcmp(args[1], "ttl") == 0) {
        auto ttl = (unsigned int) strtol(args[2], nullptr, 10);

        send_ttl(ttl);
    } else if (strcmp(args[1], "del") == 0) {
        unsigned int ip = parse_ip_addr(args[2]);

        send_del(ip);
    } else if (strcmp(args[1], "add") == 0) {
        unsigned int ip = parse_ip_addr(args[2]);
        unsigned char *eth = parse_eth_addr(args[3]);
        auto ttl = (unsigned int) strtol(args[4], nullptr, 10);

        send_add(ip, eth, ttl);
    } else if (strcmp(args[1], "res") == 0) {
        unsigned int ip = parse_ip_addr(args[2]);

        send_res(ip);
    } else {
        printf("Unrecognized command: %s\n", args[1]);
    }
}

void send_show() {
    auto cmd = get_fresh_cmd();

    cmd->type = COMMAND_SHOW;

    send_command(cmd);

    printf("Sent\n");


    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);
        unsigned int entry_count = res->len / sizeof(arp_table_entry);
        printf("Received %d table entries\n", entry_count);

        for (int i = 0; i < entry_count; ++i) {
            auto *ent = (arp_table_entry *) (buffer + sizeof(response_hdr) + (sizeof(arp_table_entry) * i));

            print_arp_table_entry(ent);
        }

    }
}

void send_ttl(int ttl) {
    auto cmd = get_fresh_cmd();

    cmd->type = COMMAND_TTL;
    cmd->ttl = (unsigned short) ttl;

    send_command(cmd);

    printf("Sent\n");

    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        if (res->type == COMMAND_TTL) {
            printf("TTL set successfully!\n");
        } else {
            printf("ERROR setting new TTL\n");
        }
    }
}

void send_del(unsigned int ip) {
    auto cmd = get_fresh_cmd();

    cmd->type = COMMAND_DEL;
    cmd->ip = ip;

    send_command(cmd);

    printf("Sent\n");

    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        if (res->type == COMMAND_DEL) {
            printf("IP deleted successfully!\n");
        } else if (res->type == COMMAND_DEL_NOT_FOUND) {
            printf("IP could not be found\n");
        } else {
            printf("ERROR deleting IP from ARP table\n");
        }
    }
}

void send_add(unsigned int ip, unsigned char mac[], unsigned int ttl) {
    auto cmd = get_fresh_cmd();

    cmd->type = COMMAND_ADD;
    cmd->ip = ip;
    memcpy(cmd->eth, mac, sizeof(char) * 6);
    cmd->ttl = ttl;

    send_command(cmd);

    printf("Sent\n");

    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        if (res->type == COMMAND_ADD) {
            printf("Table entry added successfully!\n");
        }
    }
}

void send_res(unsigned int ip) {
    auto cmd = get_fresh_cmd();

    cmd->type = COMMAND_RES;
    cmd->ip = ip;

    send_command(cmd);

    printf("Sent\n");

    if ((received_bytes = (unsigned int) recv(listenFd, buffer, buffer_size, 0)) > 0) {
        auto *res = (response_hdr *) buffer;
        printf("Response received: %d with %d bytes (raw: %d)\n", res->type, res->len, received_bytes);

        if (res->len == sizeof(arp_table_entry)) {
            printf("Sucessfully resolved IP:\n");
            print_arp_table_entry((arp_table_entry*) (buffer + sizeof(response_hdr)));
        } else {
            printf("Could not resolve IP\n");
        }
    }
}


void send_command(command_hdr *cmd) {
    sendto(listenFd, cmd, sizeof(command_hdr), 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
}

command_hdr *get_fresh_cmd() {
    auto *cmd = new command_hdr;

    memset(cmd, '\0', sizeof(command_hdr));

    return cmd;
}

void connect() {
    printf("Connecting...\n");

    if (connect(listenFd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
        perror("ERROR connecting to socket");
        exit(errno);
    }

    printf("Connected.\n");
}

void build_socket() {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

    printf("Socket: %d\n", listenFd);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
}
