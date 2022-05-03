#include "bus.h"

void codec_bus_init(codec_device* device, codec_bus* bus, uint32_t bus_size, codec_response_function response, codec_transfer_function transfer) {
    return;
}

codec_device* codec_find(codec_bus* bus, uint32_t codec_address) {
    return nullptr;
}

void codec_response(codec_device* device, bool solicited, uint32_t response) {
    return;
}

bool codec_transfer(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length) {
    return false;
}