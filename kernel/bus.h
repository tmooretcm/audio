#ifndef _bus_h_
#define _bus_h_

#include "stdint.h"
#include "queue.h"
#include "atomic.h"
#include "blocking_lock.h"

struct device_state;

typedef struct bus_child {
    device_state* child_device;
    int index;
    bus_child* next;
    Queue<bus_child, BlockingLock> siblings{};
} bus_child;

typedef struct bus_state {
    device_state* parent_device {};
    char* name;
    int max_index;
    int size;
    bool init;
    bool full;
    int num_children;

    bus_state* next;
    Queue<bus_child, BlockingLock> children{};
    Queue<bus_state, BlockingLock> siblings{};
} bus_state;

typedef struct device_state {
    char* name;
    char* path;
    bool init;
    int num_child_bus;
    uint32_t codec_address;
    bus_state* parent_bus {};
    Queue<bus_child, BlockingLock> child_bus{};
} device_state;

struct codec_device;

typedef void (*codec_response_function)(codec_device* device, bool solicited, uint32_t response);
typedef bool (*codec_transfer_function)(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length);

typedef struct codec_bus {
    bus_state* bus_st {};
    uint32_t next_codec_address;
    codec_response_function response;
    codec_transfer_function transfer;
} codec_bus;

typedef struct codec_device {
    device_state* device {};
    uint32_t codec_address;

    int (*device_init)(codec_device* device);
    void (*device_close)(codec_device* device);
    void (*device_command)(codec_device* device, uint32_t command_id, uint32_t data);
    void (*device_stream)(codec_device* device, uint32_t start_node, bool running, bool output); 
} codec_device;

void codec_bus_init(codec_device* device, codec_bus* bus, uint32_t bus_size, codec_response_function response, codec_transfer_function transfer);
codec_device* codec_find(codec_bus* bus, uint32_t codec_address);
void codec_response(codec_device* device, bool solicited, uint32_t response);
bool codec_transfer(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length);

#endif