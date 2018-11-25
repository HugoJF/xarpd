#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "../inc/interface_worker.h"
#include "../inc/utils.h"

/*
 * Constants
 */
static const int RESOLVE_TIMEOUT_MS = 300;

/*
 * Socket functions
 */
void boot();
void build_socket();
void bind();
void listen();
int accept_con();

/*
 * Daemon communication functions
 */
command_hdr *read_request(int conFd);
response_hdr *respond_request(command_hdr *cmd);
response_hdr *respond_show(command_hdr *cmd);
response_hdr *respond_res(command_hdr *cmd);
response_hdr *respond_add(command_hdr *cmd);
response_hdr *respond_del(command_hdr *cmd);
response_hdr *respond_ttl(command_hdr *cmd);

/*
 * xifconfig functions
 */
response_hdr *respond_if_show(command_hdr *pHdr);
response_hdr *respond_if_config(command_hdr *pHdr);
/*
 * Variables
 */

// Address structure to receive commands
struct sockaddr_in *daemonAddress;

// Daemon communication file descriptor
int listenFd;

// What port daemon should listen
unsigned short port = 5050;

// Main arp entry table
arp_table *table;

// List of interface workers handled by daemon
interface_worker** workers;

// Amount of workers listed
int worker_count;

/*
 * Main
 */
int main(int argc, char **args) {
    // Create main ARP table
    table = new arp_table();

    // Allocates workers for each interface in arguments
    worker_count = argc - 1;
    workers = new interface_worker *[worker_count];

    // Create and bind workers
    for (int i = 0; i < worker_count; ++i) {
        printf("Creating worker for %s\n", args[i+1]);
        workers[i] = new interface_worker(new string(args[i + 1]), table, workers, worker_count);
        workers[i]->bind();
    }

    /*
     * Daemon startup
     */
    printf("Created interface workers for %d interfaces\n", worker_count);
    boot();
    build_socket();
    bind();
    listen();

    // Listen forever
    while (true) {
        int con = accept_con();

        command_hdr *cmd = read_request(con);

        response_hdr* res = respond_request(cmd);

        if(res != nullptr) {
            send(con, res, sizeof(response_hdr) + res->len, 0);
        } else {
            fprintf(stderr, "Request response is 'nullptr', aborting...\n");
            exit(1);
        }
    }
}

/**
 * Create and setup address structure
 */
void boot() {
    daemonAddress = new sockaddr_in();

    // Fill struct and set fields
    memset(daemonAddress, '\0', sizeof(sockaddr_in));
    daemonAddress->sin_family = AF_INET;
    daemonAddress->sin_addr.s_addr = INADDR_ANY;
    daemonAddress->sin_port = htons(port);
}

/**
 * Build socket for command socket
 */
void build_socket() {
    // Creates socket
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Built socket on descriptor: %d\n", listenFd);
}

/**
 * Bind command socket to address and check for errors
 */
void bind() {
    if (bind(listenFd, (struct sockaddr *) daemonAddress, sizeof(sockaddr_in)) == -1) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }
}

/**
 * Start socket listening
 */
void listen() {
    if (listen(listenFd, 3)) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", port);
}

/**
 * Waits and returns connection descriptor
 *
 * @return - connection descriptor
 */
int accept_con() {
    // Wait for connection
    printf("Waiting new connections... ");
    int connectionFd = accept(listenFd, nullptr, nullptr);

    printf("ACCEPTED!\n");

    // Check for errors
    if (connectionFd == -1) {
        perror("Accept()");
        exit(EXIT_FAILURE);
    }

    return connectionFd;
}

/**
 * Reads entire incoming request from 'conFd'
 *
 * @param conFd - what connection to read request
 *
 * @return - command header structure
 */
command_hdr *read_request(int conFd) {

    auto bufferSize = (unsigned long) 1024;
    char buffer[bufferSize + 1];
    auto request_total_size = 0;
    auto request_partial_size = bufferSize;
    auto request_data = new std::string();
    memset(buffer, '\0', bufferSize);

    // Keeps reading while read returns data
    while (request_partial_size == bufferSize && request_total_size < bufferSize) {
        // Reads partial request
        printf("Reading request... ");
        request_partial_size = (unsigned long) read(conFd, buffer, bufferSize);

        // Check for error and append data
        if (request_partial_size < 0) {
            printf("ERROR: %s\n", strerror(errno));

            exit(errno);
        } else if (request_partial_size > bufferSize) {
            fprintf(stderr, "Request buffer overflow\n");
            exit(1);
        } else {
            printf("Read %d bytes\n", (int) request_partial_size);
            request_total_size += request_partial_size;
            request_data->append(buffer, request_partial_size);
        }
    }

    printf("Request total size: %d bytes.\n", (int) request_total_size);

    // Cast data to command header
    // TODO: use string
    auto *cmd = new command_hdr;
    memcpy(cmd, &buffer, sizeof(command_hdr));

    // TODO: remove
    printf("Type: %d\nIP: %d\nETH: %d:%d:%d:%d:%d:%d\nTTL: %d\n", cmd->type, cmd->ip, cmd->eth[0], cmd->eth[1],
           cmd->eth[2], cmd->eth[3], cmd->eth[4], cmd->eth[5], cmd->ttl);

    return cmd;
}

/**
 * Handles request according to type
 *
 * @param cmd - command header to respond
 *
 * @return - response header
 */
response_hdr *respond_request(command_hdr *cmd) {
    printf("Receiving: %d\n", cmd->type);
    // Calls responder according to command type
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
    } else if (cmd->type == COMMAND_IF_SHOW) {
        return respond_if_show(cmd);
    } else if (cmd->type == COMMAND_IF_CONFIG) {
        return respond_if_config(cmd);
    } else {
        return nullptr;
    }
}

/**
 * Configures interface
 *
 * @param pHdr - command header
 *
 * @return - response header
 */
response_hdr *respond_if_config(command_hdr *pHdr) {
    printf("=== RESPONDING CONFIG INTERFACES COMMAND ===\n");

    auto *cfg = (config_hdr*) (pHdr + sizeof(command_hdr));
    auto *res = new response_hdr();

    // Find and update iface
    interface_worker *w = find_interface_worker_by_name(cfg->eth, workers, worker_count);
    w->iface_data->ip_addr = cfg->ip;
    w->iface_data->netmask = cfg->mask;

    // Fill header
    res->type = COMMAND_IF_CONFIG;
    res->len = 0;

    return res;
}

/**
 * Builds response header with list of iface entries in table
 *
 * @param cmd - command header
 *
 * @return - response header with iface entries appended
 */
response_hdr *respond_if_show(command_hdr *pHdr) {
    printf("=== RESPONDING SHOW INTERFACES COMMAND ===\n");

    // Calculates count and size of entries
    auto entry_count = (unsigned int) worker_count;
    unsigned short entries_size = sizeof(iface) * entry_count;
    printf("Responding %d entries (%d bytes)\n", entry_count, entries_size);

    // Allocate and populate iface entries
    iface entries[entry_count];
    for (int i = 0; i < entry_count; ++i) {
        memcpy(&entries[i], workers[i]->iface_data, sizeof(iface));
    }

    // Prepare response data
    auto *data = new unsigned char[sizeof(response_hdr) + entries_size];
    auto *res = (response_hdr *) data;

    // Fill header
    res->type = COMMAND_IF_SHOW;
    res->len = entries_size;

    // Copy header data
    memcpy(data + sizeof(response_hdr), entries, entries_size);

    return res;
}

/**
 * Builds response header with list of ARP entries in table
 *
 * @param cmd - command header
 *
 * @return - response header with ARP entries appended
 */
response_hdr *respond_show(command_hdr *cmd) {
    printf("=== RESPONDING SHOW COMMAND ===\n");

    // Calculates count and size of entries
    auto entry_count = (unsigned int) table->count();
    unsigned short entries_size = sizeof(arp_table_entry) * entry_count;
    printf("Responding %d entries (%d bytes)\n", entry_count, entries_size);

    // Allocate and populate ARP entries
    arp_table_entry entries[entry_count];
    for (int i = 0; i < entry_count; ++i) {
        entries[i] = (*table->get((unsigned int) i));
    }

    // Prepare response data
    auto *data = new unsigned char[sizeof(response_hdr) + entries_size];
    auto *res = (response_hdr *) data;

    // Fill header
    res->type = COMMAND_SHOW;
    res->len = entries_size;

    // Copy header data
    memcpy(data + sizeof(response_hdr), entries, entries_size);

    return res;
}

/**
 * Resolves IP and creates response header with ARP entry appened
 *
 * @param cmd - command header
 *
 * @return - response header with ARP entry appended
 */
response_hdr *respond_res(command_hdr *cmd) {
    printf("=== RESPONDING RESOLVE ===\n");

    // Allocate response header
    auto *data = new char[sizeof(response_hdr) + sizeof(arp_table_entry)];
    auto *res = (response_hdr*) data;
    arp_table_entry *ent = nullptr;

    // Find worker that should handle requested IP
    interface_worker *ifw = find_interface_worker(cmd->ip, workers, worker_count);

    // Check if worker exists
    if(ifw != nullptr) {
        // Send resolve request
        ifw->resolve_ip(cmd->ip);

        // Sleeps x times waiting for resolution
        // TODO: interruption instead of polling
        unsigned int sleeps = 0;
        while (sleeps < RESOLVE_TIMEOUT_MS) {
            // Check if requested entry is present on table
            ent = table->find_by_ip(cmd->ip);
            if (ent != nullptr) {
                printf("Found entry: ");
                print_arp_table_entry(ent);
                break;
            }

            // Sleep for 10ms
            usleep(10000);
            sleeps++;
        }
    } else {
        printf("Could not find interface for IP\n");
    }

    // Fill header
    res->type = COMMAND_RES;

    // Check if response was filled
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


/**
 * Adds new IP to ARP table
 *
 * @param cmd - command header containing IP
 *
 * @return - response header
 */
response_hdr *respond_add(command_hdr *cmd) {
    printf("=== RESPONDING ADD COMMAND ===\n");
    auto *res = new response_hdr;
    auto *ent = new arp_table_entry;

    // Fill ARP entry
    ent->ipAddress = cmd->ip;
    memcpy(&ent->ethAddress, &cmd->eth, sizeof(char) * 6);
    ent->ttl = cmd->ttl;

    // Debug
    printf("Added entry: ");
    print_arp_table_entry(ent);
    printf("\n");

    // Push to table
    table->add(cmd->ip, cmd->eth, cmd->ttl);

    // Fill response
    res->type = COMMAND_ADD;
    res->len = 0;

    return res;
}

/**
 * Removes IP from table
 *
 * @param cmd - command header containing IP to be removed
 *
 * @return - response header
 */
response_hdr *respond_del(command_hdr *cmd) {
    printf("=== RESPONDING DEL COMMAND ===\n");
    printf("Deleting IP: %d\n", cmd->ip);

    // Allocate response header
    auto *res = new response_hdr;

    // Try to delete given IP and store if something was modified
    bool deleted = table->remove(cmd->ip);

    // Set type correctly
    if(deleted) {
        res->type = COMMAND_DEL;
    } else {
        res->type = COMMAND_DEL_NOT_FOUND;
    }
    res->len = 0;

    return res;
}

/**
 * Sets a new TTL
 *
 * @param cmd - command header containing new TTL
 *
 * @return - response header
 */
response_hdr *respond_ttl(command_hdr *cmd) {
    printf("=== RESPONDING TTL COMMAND ===\n");
    auto *res = new response_hdr;

    // Update TTL
    printf("Setting TTL to: %d\n", cmd->ttl);
    table->setTtl(cmd->ttl);

    // Set response
    res->type = COMMAND_TTL;
    res->len = 0;

    return res;
}