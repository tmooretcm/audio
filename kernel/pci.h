#ifndef _pci_h
#define _pci_h

#include "machine.h"
#include "idt.h"
#include "libk.h"
#include "config.h"
#include "debug.h"
#include "stdint.h"

// this code is from osdev 
// https://wiki.osdev.org/PCI

namespace PCI {

    struct pci_driver;

    typedef struct pci_device {
        uint32_t vendor;
        uint32_t device;
        uint32_t func; 
        struct pci_driver* driver;
    } pci_device;

    typedef struct device_id {
        uint32_t vendor_id;
        uint32_t device_id;
        uint32_t func_id;
    } device_id;

    typedef struct pci_driver {
        device_id* device_id_table;
        char* name;
        uint8_t (*init_device)(pci_device*);
        uint8_t (*init_driver)(void);   
        uint8_t (*exit_driver)(void);
    } pci_driver;

    extern void pci_init();

    extern void pci_debug();

}

#endif