#ifndef _pci_h
#define _pci_h

#include "machine.h"
#include "idt.h"
#include "libk.h"
#include "config.h"
#include "debug.h"
#include "stdint.h"


// this code is from osdev

namespace PCI {

    void init();

    uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

    uint16_t pciCheckVendor(uint8_t bus, uint8_t slot);

}

#endif