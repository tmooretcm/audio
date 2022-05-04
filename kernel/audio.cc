#include "audio.h"
#include "debug.h"

static audio_driver* driver;

// update interrupt status
using namespace audio;

// Handles any interrupts - not technically needed to implement audio but since we have interrupts enabled we must have this
static void handle_interrupt(hda_audio_device* device) {
    // Grab current values
    uint32_t curr_int_sts = REG_INL(device, REG_INTSTS);
    uint8_t curr_out_sts = REG_INB(device, REG_O0_STS);

    Debug::printf("Handling interrupt: intsts: %x, outsts: %x, num bufs completed: %d\n", curr_int_sts, curr_out_sts, device->num_buffs_completed);

    // Interrupt for stream 1 = buffer finished
    if (curr_out_sts & 0x4) {
        audio_buffer_complete(device->output->stream, device->num_buffs_completed);
        device->num_buffs_completed++;
        device->num_buffs_completed %= BDL_SIZE;
    }

    // Reset values
    REG_OUTL(device, REG_INTSTS, curr_int_sts);
    REG_OUTB(device, REG_O0_STS, curr_out_sts);
}

// Initialize the CORB data structure
static void init_corb(hda_audio_device* device) {
    uint8_t reg;
    uint32_t corb_base_addr;

    reg = REG_INB(device, REG_CORBSIZE);

    // checking corb sizes
    if (reg & (1 << 6)) {
        device->corb_entries = 256;
        reg |= 0x2;
    } else if (reg & (1 << 5)) {
        device->corb_entries = 16;
        reg |= 0x1;
    } else if (reg & (1 << 4)) {
        device->corb_entries = 2;
        reg |= 0x0;
    } else {
        Debug::printf("No supported CORB sizes.\n");
    }

    REG_OUTB(device, REG_CORBSIZE, reg);

    // initialize corb base address
    corb_base_addr = (uintptr_t)device->rings->pa[0];
    REG_OUTL(device, REG_CORBLBASE, corb_base_addr & 0xFFFFFFFF);
    REG_OUTL(device, REG_CORBUBASE, corb_base_addr >> 32);

    // start DMA
    REG_OUTB(device, REG_CORBCTL, 0x02);
}

// Initialize the RIRB data structure
static void init_rirb(hda_audio_device* device) {
    uint8_t reg;
    uint32_t rirb_base_addr;

    reg = REG_INB(device, REG_RIRBSIZE);

    if (reg & (1 << 6)) {
        device->rirb_entries = 256;
        reg |= 0x2;
    } else if (reg & (1 << 5)) {
        device->rirb_entries = 16;
        reg |= 0x1;
    } else if (reg & (1 << 4)) {
        device->rirb_entries = 2;
        reg |= 0x0;
    } else {
        Debug::printf("No supported RIRB sizes.\n");
    }

    REG_OUTB(device, REG_RIRBSIZE, reg);

    rirb_base_addr = (uintptr_t)device->rings->pa[0] + 1024; // should be pa[256] (?)
    REG_OUTL(device, REG_RIRBLBASE, rirb_base_addr & 0xFFFFFFFF);
    REG_OUTL(device, REG_RIRBUBASE, rirb_base_addr >> 32);

    // set interrupt cnt register
    REG_OUTB(device, REG_RINTCNT, 0x42);

    // start DMA
    REG_OUTB(device, REG_RIRBCTL, 0x02);
}

// Write the given verb to the CORB at the next available spot
static void corb_write(hda_audio_device* device, uint32_t verb) {
    uint16_t write_pointer = REG_INW(device, REG_CORBWP) & 0xFF;
    uint16_t read_pointer, next;

    // next available spot in corb - take modulus for the ring 
    next = (write_pointer + 1) % device->corb_entries;

    // wait until there's space in corb to write 
    do {
        read_pointer = REG_INW(device, REG_CORBRP) & 0xFF;
    } while (next == read_pointer);

    // found space, write to corb
    device->corb[next] = verb;
    REG_OUTW(device, REG_CORBWP, next);
}

// Waits for an available RIRB entry to read from.
// When an entry is found, increments rirb's read pointer,
// and reads entry into *read.
static void rirb_read(hda_audio_device* device, uint64_t* read){
    uint16_t write_pointer;
    // last accessed read_pointer read from RIRB
    uint16_t read_pointer = device->rirb_rp;

    // do while searches for an entry in RIRB
    do {
        write_pointer = REG_INW(device, REG_RIRBWP) & 0xFF;
    } while (write_pointer == read_pointer);

    // go to next unread read pointer in RIRB to read
    read_pointer = (read_pointer + 1) % device->rirb_entries;
    device->rirb_rp = read_pointer;

    // read from RIRB into uint64_t ptr
    *read = device->rirb[read_pointer];
    REG_OUTB(device, REG_RIRBSTS, 0x5);
}


// Facilitates communication between corb and rirb. codex, widget, and payload
// parameters define the communication. Using these instructions, write to corb, then
// read from rirb and return the response.
static uint32_t codec_transmission(hda_audio_device* device, int codec, int widget_id, uint32_t payload){
    uint64_t read;
    // Generate corb input with codec, widget id, and payload
    uint32_t verb = ((codec & 0xf) << 28) |
                    ((widget_id & 0xff) << 20) |
                    (payload & 0xfffff);

    // write to corb with generated input
    corb_write(device, verb);
    // read from rirb; should be able receive communication from corb
    rirb_read(device, &read);

    return read & 0xffffffff;
}


static void output_widget_config(hda_audio_device* device){
    uint16_t format = BITS_16 | device->output->sample_rate |
                      (device->output->num_channels - 1);

    codec_transmission(device, device->output->codec, device->output->node_id,
        VERB_SET_FORMAT | format);

    REG_OUTL(device, REG_O0_FMT, format);

}

static void init_output_widget(hda_audio_device* device){
    device->output->stream->device = device->audio;
    device->output->stream->num_buffers = BDL_SIZE;
    device->output->stream->buffer_size = BUFFER_SIZE / 2;
    //device->output->stream->sample_format = CDI_AUDIO_16SI; have no idea where this is from


    codec_transmission(device, device->output->codec, device->output->node_id, VERB_SET_STREAM_CHANNEL | 0x10);

    device->output->sample_rate = SR_48_KHZ;
    device->output->num_channels = 2;
    output_widget_config(device);
}