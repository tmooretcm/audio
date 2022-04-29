#include "pci.h"

namespace PCI {

    pci_device** pci_devices = 0;
    uint32_t devices = 0;

    pci_driver** pci_drivers = 0;
    uint32_t drivers = 0;
    
    uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
        uint32_t address;
        uint32_t lbus  = (uint32_t)bus;
        uint32_t lslot = (uint32_t)slot;
        uint32_t lfunc = (uint32_t)func;
        uint16_t tmp = 0;
    
        // Create configuration address as per Figure 1
        address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
        // Write out the address
        outl(0xCF8, address);
        // Read in the data
        // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
        tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
        return tmp;
    }

    uint16_t pciCheckVendor(uint8_t bus, uint8_t slot) {
        uint16_t vendor, device;
        /* Try and read the first configuration register. Since there are no
        * vendors that == 0xFFFF, it must be a non-existent device. */
        if ((vendor = pciConfigReadWord(bus, slot, 0, 0)) != 0xFFFF) {
            device = pciConfigReadWord(bus, slot, 0, 2);
        } return (vendor);
    }
}