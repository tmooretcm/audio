#ifndef _bus_h_
#define _bus_h_

typedef struct bus_state {
    char* name;
    int max_index;
    bool init;
    bool full;
    int num_children;
} bus_state;

typedef struct device_state {
    char* name;
    char* path;
    bool init;
    bus_state* parent_bus;
} device_state;

typedef struct codec_device;

typedef void (*codec_response_function)(codec_device* device, bool solicited, uint32_t response);
typedef bool (*codec_transfer_function)(codec_device* device, uint32_t start_node, bool output, uint8_t* buffer, uint32_t length);

typedef struct codec_bus {
    bus_state* bus;
    uint32_t next_codec_address;
    codec_response_function response;
    codec_transfer_function transfer;
} codec_bus;

typedef struct codec_device {
    device_state* device;
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