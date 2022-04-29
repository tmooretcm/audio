#include "pci.h"

namespace PCI {

    #define BAR0 0x10
    #define BAR1 0x14
    #define BAR2 0x18
    #define BAR3 0x1C
    #define BAR4 0x20
    #define BAR5 0x24

    struct PCIDevice {
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t command;
        uint16_t status;
        uint8_t rev_id;
        uint8_t prog_if;
        uint8_t subclass;
        uint8_t class_code;
        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t bist;
        uint32_t bar0;
        uint32_t bar1;
        uint32_t bar2;
        uint32_t bar3;
        uint32_t bar4;
        uint32_t bar5;
        uint32_t cardbus_cis_pointer;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_id;
        uint32_t expansion_rom_base;
        uint8_t capabilities;
        char* reserved[56];
        uint8_t int_line;
        uint8_t int_pin;
        uint8_t min_grant;
        uint8_t max_latency;
    };

    void init() {
    } 
    
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