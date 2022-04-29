#ifndef _pci_h
#define _pci_h

// this code is from osdev

namespace PCI {

    uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {};

    uint16_t pciCheckVendor(uint8_t bus, uint8_t slot) {};

}

#endif