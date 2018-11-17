//
// Created by root on 21/10/18.
//

#include <iostream>
#include <unistd.h>
#include "../inc/arp_table.h"
#include "../inc/utils.h"

arp_table::arp_table() {
    this->defaultTtl = 60;
    this->table = new vector<arp_table_entry *>();
    this->dispatch_timer_thread(this);
};

void *timer(void *ctx) {
    // Cast context variable
    auto *at = (arp_table *) ctx;
    arp_table_entry *ent;

    while (true) {
        // Iterate over each entry
        for (int i = 0; i < at->count(); ++i) {
            // Check if we have a nullptr or permanent entry
            if (ent == nullptr || ent->ttl == -1) continue;

            // Update TTL
            ent = at->get(i);
            ent->ttl--;

            // If entry expired, remove it
            if (ent->ttl == 0) {
                at->remove(ent->ipAddress);
            }

            // Sleep for 1 second
            usleep(1 * 1000 * 1000);
        }
    }
}

void arp_table::dispatch_timer_thread(arp_table *ctx) {
    // Build thread information
    ctx->timer_thread = new pthread_t();
    auto *attributes = new pthread_attr_t();

    // Dispatch thread with context
    if (pthread_create(ctx->timer_thread, nullptr, timer, (void *) ctx)) {
        perror("pthreads()");
        exit(errno);
    }
}

arp_table_entry *arp_table::get(unsigned int index) {
    return this->table->at(index);
}

arp_table_entry *arp_table::find_by_ip(unsigned int ip) {
    // Iterate over each entry in table
    for (int i = 0; i < this->count(); ++i) {
        // Check if entry matched target IP address
        arp_table_entry *en = this->table->at(i);
        if (en->ipAddress == ip) {
            return this->table->at(i);
        }
    }

    return nullptr;
}

arp_table_entry *arp_table::find_by_eth(unsigned char eth[]) {
    // Iterate over each entry in table
    for (int i = 0; i < this->table->size(); ++i) {
        // Check if entry matched target Ethernet address
        if (eth_address_eq(eth, this->table->at(i)->ethAddress)) {
            return this->table->at(i);
        }
    }

    return nullptr;
}

void arp_table::add(unsigned int ip_address, unsigned char eth_address[], unsigned int ttl) {
    // Debugging
    print_ip_addr((char *) "Adding ARP entry from: ", ip_address);
    printf("\n");

    // Check if entry already exists
    if (this->find_by_ip(ip_address) != nullptr) {
        printf("Entry already exists, aborting...\n");
        return;
    }

    // Build ARP table entry
    auto entry = new arp_table_entry();
    entry->ipAddress = ip_address;
    entry->ttl = ttl;
    memcpy(entry->ethAddress, eth_address, sizeof(char) * 6);

    // Add to vector
    this->table->push_back(entry);

    // Debug to console
    printf("Added: ");
    print_arp_table_entry(entry);
    printf("\n");
}

unsigned long arp_table::count() {
    return this->table->size();
}

void arp_table::add(unsigned int ip_address, unsigned char *eth_address) {
    this->add(ip_address, eth_address, defaultTtl);
}

bool arp_table::remove(unsigned int ip) {
    arp_table_entry *x;

    // Iterate over each entry in table
    for (unsigned long i = 0; i < this->table->size(); ++i) {
        x = this->table->at(i);

        // Return true since something was erased
        if (x->ipAddress == ip) {
            this->table->erase(this->table->begin() + i);
            return true;
        }
    }

    // Nothing was erased
    return false;
}

void arp_table::setTtl(unsigned int ip) {
    this->defaultTtl = ip < 0 ? -1 : ip;
}
