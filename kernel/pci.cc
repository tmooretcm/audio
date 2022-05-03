#include "pci.h"

// big help & reference: https://github.com/levex/osdev/blob/master/drivers/pci/pci.c

namespace PCI {

    pci_device** pci_devices;
    uint32_t devices;

    pci_driver** pci_drivers;
    uint32_t drivers;

    void add_device(pci_device* device) {
        pci_devices[devices] = device;
        devices++;
        // testing
        // burh
    }

    void add_driver(pci_driver* driver) {
        pci_drivers[drivers] = driver;
        drivers++;
    }
    
    // this is straight from osdev
    uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
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

    inline uint16_t get_vendor_id(uint16_t bus, uint16_t slot, uint16_t func) {
        return pci_config_read_word(bus, slot, func, 0);
    }

    inline uint16_t get_device_id(uint16_t bus, uint16_t slot, uint16_t func) {
        return pci_config_read_word(bus, slot, func, 2);
    }

    // from the PCI spec - http://www.cisl.columbia.edu/courses/spring-2004/ee4340/restricted_handouts/pci_23.pdf
    void parse_devices() {
        for (uint32_t bus = 0; bus < 256; bus++) {
            for (uint32_t slot = 0; slot < 32; slot++) {
                for (uint32_t func = 0; func < 8; func++) {
                    uint16_t vendor_id = get_vendor_id(bus, slot, func);
                    if (vendor_id == 0xFFFF) {
                        // invalid vendor id, go next
                        continue;
                    }
                    uint16_t device_id = get_device_id(bus, slot, func);
                    // Debug::printf("vendor: 0x%x device: 0x%x\n", vendor, device);
                    pci_device* device = new pci_device();
                    device->vendor = vendor_id;
                    device->device = device_id;
                    device->func = func;
                    device->driver = nullptr;
                    add_device(device);
                }
            }
        }
    }

    void pci_debug() {
        for (uint32_t i = 0; i < devices; i++) {
            pci_device* device = pci_devices[i];
            if (device->driver) {
                // shouldn't need to do anyhting in here for now
            } else {
                Debug::printf("PCI DUMP: vendor: %x, device: %x, function: %x\n", device->vendor, device->device, device->func);
            }
        }
    }

    void pci_init() {
        devices = 0;
        drivers = 0;
        pci_devices = new pci_device*[32];
        pci_drivers = new pci_driver*[32];
        parse_devices();
        // Debug::printf("PCI Initialized.\n");
        pci_debug();
    }
}