#include "bus.h"
#include "pci.h"
#include "audio.h"

void codec_bus_init(device_state* device, codec_bus* bus, uint32_t bus_size, codec_response_function response, codec_transfer_function transfer) {
    bus->bus_st->name = nullptr;
    if (device) {
        device->num_child_bus++;  
    }
    bus->bus_st->parent_device = device;
    bus->bus_st->size = bus_size;
    bus->response = response;
    bus->transfer = transfer;
}

codec_device* codec_find(codec_bus* bus, uint32_t codec_address) {
    // idk how to do this
    return nullptr;
}

void codec_response(codec_device* device, bool solicited, uint32_t response) {
    // unsure about line 20, was looking for a function I could pass device->parent bus into, and set bus equal to that
    // something like codec_bus* = HDA_BUS(devvice->parent_bus)
    codec_bus* bus{};
    bus->bus_st->parent_device = device->device;
    bus->response(device, solicited, response);
}

bool codec_transfer(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length) {
    // same thing here on line 27
    codec_bus* bus{};
    bus->bus_st->parent_device = device->device;
    return bus->transfer(device, start_node, output, buffer, length);
}