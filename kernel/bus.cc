#include "bus.h"
#include "pci.h"
#include "audio.h"

//codec_bus* cbus;

void codec_bus_init(codec_device* device, codec_bus* bus, uint32_t bus_size, codec_response_function response, codec_transfer_function transfer) {
    //cbus = new codec_bus();
    qbus_init(bus, bus_size, device, nullptr);
    cbus->response = response;
    cbus->transfer = transfer;
}

codec_device* codec_find(codec_bus* bus, uint32_t codec_address) {
    codec_device* cdev;
    return nullptr;
}

void codec_response(codec_device* device, bool solicited, uint32_t response) {
    // unsure about line 20, was looking for a function I could pass device->parent bus into, and set bus equal to that
    // something like codec_bus* = HDA_BUS(devvice->parent_bus)
    codec_bus* bus = device->parent_bus;
    bus->response(device, solicited, response);
}

bool codec_transfer(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length) {
    // same thing here on line 27
    codec_bus* bus = device->parent_bus;
    return bus->transfer(device, start_node, output, buffer, length);
}