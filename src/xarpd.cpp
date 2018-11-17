#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "../inc/interface_worker.h"
#include "../inc/utils.h"

static const int RESOLVE_TIMEOUT_MS = 300;

void boot();

void build_socket();

void bind();

void listen();

int accept_con();

command_hdr *read_request(int conFd);

response_hdr *respond_request(command_hdr *cmd);
response_hdr *respond_show(command_hdr *cmd);
response_hdr *respond_res(command_hdr *cmd);
response_hdr *respond_add(command_hdr *cmd);
response_hdr *respond_del(command_hdr *cmd);
response_hdr *respond_ttl(command_hdr *cmd);

interface_worker *find_interface_worker(unsigned int ip);

struct sockaddr_in *serverAddress;
int listenFd;
unsigned short port = 5050;
arp_table *table;
interface_worker** workers;
int worker_count;

int main(int argc, char **args) {
    table = new arp_table();

    worker_count = argc - 1;
    workers = new interface_worker*[worker_count];

    for (int i = 0; i < worker_count; ++i) {
        printf("Creating worker for %s\n", args[i+1]);
        workers[i] = new interface_worker(new string(args[i + 1]), table);
        workers[i]->bind();
    }

    printf("Created interface workers for %d interfaces\n", worker_count);

    boot();

    build_socket();

    bind();

    listen();

    while (true) {
        int con = accept_con();

        command_hdr *cmd = read_request(con);

        response_hdr* res = respond_request(cmd);

        send(con, res, sizeof(response_hdr) + res->len, 0);
    }
}

void boot() {
    serverAddress = new sockaddr_in();
    memset(serverAddress, '\0', sizeof(sockaddr_in));

    serverAddress->sin_family = AF_INET;
    serverAddress->sin_addr.s_addr = INADDR_ANY;
    serverAddress->sin_port = htons(port);
}

void build_socket() {
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Built socket on descriptor: %d\n", listenFd);
}

void bind() {
    if (bind(listenFd, (struct sockaddr *) serverAddress, sizeof(sockaddr_in)) == -1) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }
}

void listen() {
    if (listen(listenFd, 3)) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", port);
}

int accept_con() {
    printf("Waiting new connections... ");
    int connectionFd = accept(listenFd, nullptr, nullptr);

    printf("ACCEPTED!\n");

    if (connectionFd == -1) {
        perror("Accept()");
        exit(EXIT_FAILURE);
    }

    return connectionFd;
}

command_hdr *read_request(int conFd) {

    auto bufferSize = (unsigned long) 1024;
    char buffer[bufferSize + 1];
    memset(buffer, '\0', bufferSize);
    auto requestBytesTotal = 0;
    auto requestBytes = bufferSize;
    auto request = new std::string();

    while (requestBytes == bufferSize && requestBytesTotal < bufferSize) {
        printf("Reading request... ");

        requestBytes = (unsigned long) read(conFd, buffer, bufferSize);

        if (requestBytes < 0) {
            printf("ERROR: %s\n", strerror(errno));

            exit(errno);
        } else if (requestBytes > bufferSize) {
            fprintf(stderr, "Request buffer overflow\n");
            exit(1);
        } else {
            printf("Read %d bytes\n", (int) requestBytes);
            requestBytesTotal += requestBytes;
            request->append(buffer, requestBytes);
        }
    }

    printf("Request total size: %d bytes.\n", (int) requestBytesTotal);

    auto *cmd = new command_hdr;
    memcpy(cmd, &buffer, sizeof(command_hdr));

    printf("Type: %d\nIP: %d\nETH: %d:%d:%d:%d:%d:%d\nTTL: %d\n", cmd->type, cmd->ip, cmd->eth[0], cmd->eth[1],
           cmd->eth[2], cmd->eth[3], cmd->eth[4], cmd->eth[5], cmd->ttl);

    return cmd;
}

response_hdr *respond_request(command_hdr *cmd) {
    if (cmd->type == COMMAND_SHOW) {
        return respond_show(cmd);
    } else if (cmd->type == COMMAND_RES) {
        return respond_res(cmd);
    } else if (cmd->type == COMMAND_ADD) {
        return respond_add(cmd);
    } else if (cmd->type == COMMAND_DEL) {
        return respond_del(cmd);
    } else if (cmd->type == COMMAND_TTL) {
        return respond_ttl(cmd);
    } else {
        return nullptr;
    }
}

response_hdr *respond_show(command_hdr *cmd) {
    printf("=== RESPONDING SHOW COMMAND ===\n");
    auto entry_count = (unsigned int) table->count();
    unsigned short entries_size = sizeof(arp_table_entry) * entry_count;
    printf("Responding %d entries (%d bytes)\n", entry_count, entries_size);

    arp_table_entry entries[entry_count];

    for (int i = 0; i < entry_count; ++i) {
        entries[i] = (*table->get((unsigned int) i));
    }

    auto *data = new unsigned char[sizeof(response_hdr) + entries_size];
    auto *res = (response_hdr *) data;

    res->type = COMMAND_SHOW;
    res->len = entries_size;

    memcpy(data + sizeof(response_hdr), entries, entries_size);

    return res;
}

response_hdr *respond_res(command_hdr *cmd) {
    printf("=== RESPONDING RESOLVE ===\n");
    auto *data = new char[sizeof(response_hdr) + sizeof(arp_table_entry)];
    auto *res = (response_hdr*) data;
    arp_table_entry *ent = nullptr;
    
    interface_worker *ifw = find_interface_worker(cmd->ip);

    if(ifw != nullptr) {
        ifw->resolve_ip(cmd->ip);

        unsigned int sleeps = 0;

        while (sleeps < RESOLVE_TIMEOUT_MS) {
            ent = table->find_by_ip(cmd->ip);
            if (ent != nullptr) {
                printf("Found entry: ");
                print_arp_table_entry(ent);
                break;
            }
            usleep(10000);
            sleeps++;
        }
    } else {
        printf("Could not find interface for IP\n");
    }
    
    res->type = COMMAND_RES;
    if(ent != nullptr) {
        printf("Found ARP table entry, responding...\n");
        res->len = sizeof(arp_table_entry);
        memcpy(data + sizeof(response_hdr), ent, sizeof(arp_table_entry));
    } else {
        printf("ARP table entry could not be found\n");
        res->len = 0;
    }
    
    return res;
}

interface_worker *find_interface_worker(unsigned int ip) {
    for (int i = 0; i < worker_count; ++i) {
        interface_worker *ifw = workers[i];

        unsigned int mask = ifw->iface_data->netmask;
        unsigned int net_if =mask & ifw->iface_data->ip_addr;
        unsigned int net_ip = mask & ip;

        printf("Attempting to solve: ");
        print_ip_addr((char*) "", net_if);
        printf(" == ");
        print_ip_addr((char*) "", net_ip);
        printf("\n");

        if(net_if == net_ip) {
            return ifw;
        }
    }

    return nullptr;
}

response_hdr *respond_add(command_hdr *cmd) {
    printf("=== RESPONDING ADD COMMAND ===\n");
    auto *res = new response_hdr;

    auto *ent = new arp_table_entry;
    ent->ipAddress = cmd->ip;
    memcpy(&ent->ethAddress, &cmd->eth, sizeof(char) * 6);
    ent->ttl = cmd->ttl;

    printf("Added entry: ");
    print_arp_table_entry(ent);
    printf("\n");
    table->add(cmd->ip, cmd->eth, cmd->ttl);

    res->type = COMMAND_ADD;
    res->len = 0;

    return res;
}

response_hdr *respond_del(command_hdr *cmd) {
    printf("=== RESPONDING DEL COMMAND ===\n");
    auto *res = new response_hdr;

    printf("Deleting IP: %d\n", cmd->ip);
    bool deleted = table->remove(cmd->ip);

    if(deleted) {
        res->type = COMMAND_DEL;
    } else {
        res->type = COMMAND_DEL_NOT_FOUND;
    }
    res->len = 0;

    return res;
}

response_hdr *respond_ttl(command_hdr *cmd) {
    printf("=== RESPONDING TTL COMMAND ===\n");
    auto *res = new response_hdr;
    
    printf("Setting TTL to: %d\n", cmd->ttl);
    table->setTtl(cmd->ttl);

    res->type = COMMAND_TTL;
    res->len = 0;

    return res;
}