#include "audio.h"
#include "debug.h"
#include "bus.h"
#include "pci.h"
#include "physmem.h"
#include "config.h"
#include "vmm.h"

typedef struct audio_driver* driver;

// update interrupt status
using namespace audio;

// Handles any interrupts - not technically needed to implement audio but since we have interrupts enabled we must have this
void handle_interrupt(hda_audio_device* device) {
    // Grab current values
    uint32_t curr_int_sts = REG_INL(device, REG_INTSTS);
    uint8_t curr_out_sts = REG_INB(device, REG_O0_STS);

    Debug::printf("Handling interrupt: intsts: %x, outsts: %x, num bufs completed: %d\n", curr_int_sts, curr_out_sts, device->num_buffs_completed);

    // Interrupt for stream 1 = buffer finished
    if (curr_out_sts & 0x4) {
        //audio_buffer_complete(device->output->stream, device->num_buffs_completed); Method not defined anywhere??
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
    REG_OUTL(device, REG_CORBUBASE, (uint32_t)((uint64_t)corb_base_addr >> 32));

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
    REG_OUTL(device, REG_RIRBUBASE, (uint32_t)((uint64_t)rirb_base_addr >> 32));

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

void init_output_widget(hda_audio_device* device){
    device->output->stream->device = device->audio;
    device->output->stream->num_buffers = BDL_SIZE;
    device->output->stream->buffer_size = BUFFER_SIZE / 2;
    device->output->stream->sample_format = AUDIO_16SI;


    codec_transmission(device, device->output->codec, device->output->node_id, VERB_SET_STREAM_CHANNEL | 0x10);

    device->output->sample_rate = SR_48_KHZ;
    device->output->num_channels = 2;
    output_widget_config(device);
}

// Debug tool to see what widgets are connected to the audio device.
static void debug_widget_connections(hda_audio_device* device, int codec, int node_id) {
    uint32_t num_connections = codec_transmission(device, codec, node_id, VERB_GET_PARAMETER | PARAM_CONN_LIST_LEN);
    if (num_connections == 0) {
        return;
    }
    uint32_t selected_connection;
    for (uint32_t i = 0; i < (num_connections & 0x7F); i++) {
        uint32_t current_connection;
        int index, shift;
        if (num_connections & 0x80) {
            index = i & ~3;
            shift = 8 * (i & 3);
        } else {
            index = i & ~1;
            shift = 8 * (i & 1);
        }
        current_connection = codec_transmission(device, codec, node_id, VERB_GET_CONN_LIST | index);
        current_connection >>= shift;

        bool range;
        if (num_connections & 0x80) {
            range = current_connection & 0x8000;
            current_connection &= 0x7FFF;
        } else {
            range = current_connection & 0x80;
            current_connection &= 0x7F;
        }

        Debug::printf("Widget Connection: %c%d\n", range ? '-' : ' ', current_connection);
    }

    selected_connection = codec_transmission(device, codec, node_id, VERB_GET_CONN_SELECT);
    Debug::printf("Currently selected widget: %d\n", selected_connection);
}

// start setting up afg codec and widgets
static void init_widget(hda_audio_device* device, int codec, int node_id) {
    uint32_t widget_capability, amp_capability, eapd_capability;
    WIDGET_TYPE type;

    widget_capability = codec_transmission(device, codec, node_id, VERB_GET_PARAMETER | PARAM_AUDIO_WID_CAP);
    // not ready to initialize
    if (widget_capability == 0) {
        return;
    }
    type = (WIDGET_TYPE)((widget_capability & WIDGET_CAP_TYPE_MASK) >> WIDGET_CAP_TYPE_SHIFT);
    amp_capability = codec_transmission(device, codec, node_id, VERB_GET_PARAMETER | PARAM_OUT_AMP_CAP);
    eapd_capability = codec_transmission(device, codec, node_id, VERB_GET_EAPD_BTL);

    const char* widget_name;
    switch (type) {
        case 0: widget_name = "WIDGET: Output"; break;
        case 1: widget_name = "WIDGET: Input"; break;
        case 2: widget_name = "WIDGET: Mixer"; break;
        case 3: widget_name = "WIDGET: Selector"; break;
        case 4: widget_name = "WIDGET: Pin Complex"; break;
        case 5: widget_name = "WIDGET: Power"; break;
        case 6: widget_name = "WIDGET: Volume"; break;
        case 7: widget_name = "WIDGET: Beep"; break;
        case 15: widget_name = "WIDGET: Vendor"; break;
        default: widget_name = "WIDGET: Unknown"; break;
    }

    uint32_t gain = codec_transmission(device, codec, node_id, VERB_GET_AMP_GAIN_MUTE | 0x8000) << 8;
    gain |= codec_transmission(device, codec, node_id, VERB_GET_AMP_GAIN_MUTE | 0xA000);

    // debug statements; can comment out these two lines
    Debug::printf("INIT WIDGET  Widget type: %s; Node_ID: %d; WidgetCap: %x, EAPDCap: %x, Amp: %x/%x\n", 
                widget_name, node_id, widget_capability, eapd_capability, gain, amp_capability);
    debug_widget_connections(device, codec, node_id);

    // actually setting widget registers
    switch (type) {
        case WIDGET_OUTPUT:
        {
            if (!device->output->node_id) {
                device->output->codec = codec;
                device->output->node_id = node_id;
                device->output->amp_gain = (amp_capability >> 8) & 0x7F;
            }
            codec_transmission(device, codec, node_id, VERB_SET_EAPD_BTL | eapd_capability | 0x2);
            break;
        }
        case WIDGET_PIN:
        {
            uint32_t pin_capability, control;
            pin_capability = codec_transmission(device, codec, node_id, VERB_GET_PARAMETER | PARAM_PIN_CAP);
            if ((pin_capability & PIN_CAP_OUTPUT) == 0) {
                return;
            }
            control = codec_transmission(device, codec, node_id, VERB_GET_PIN_CONTROL) | PIN_CTL_ENABLE_OUTPUT;
            codec_transmission(device, codec, node_id, VERB_SET_PIN_CONTROL | control);
            codec_transmission(device, codec, node_id, VERB_SET_EAPD_BTL | eapd_capability | 0x2);
            break;
        }
        default: return;
    }

    // enable power control
    if (widget_capability & WIDGET_CAP_POWER_CNTRL) {
        codec_transmission(device, codec, node_id, VERB_SET_POWER_STATE | 0x0);
    }
}

// Enumerates and initializes widgets for the codec. Returns 0 on successly initializing at least one widget, -1 on failure.
static int codec_init_widgets(hda_audio_device* device, int codec) {
    uint32_t parameter = codec_transmission(device, codec, 0, VERB_GET_PARAMETER | PARAM_NODE_COUNT);
    int num_func_groups, num_widgets;
    num_func_groups = parameter & 0xFF;
    int func_group_start, widget_start;
    func_group_start = (parameter >> 16) & 0xFF;

    for (int i = 0; i < num_func_groups; i++) {
        parameter = codec_transmission(device, codec, func_group_start + i, VERB_GET_PARAMETER | PARAM_NODE_COUNT);
        num_widgets = parameter & 0xFF;
        widget_start = (parameter >> 16) & 0xFF;
        parameter = codec_transmission(device, codec, func_group_start + i, VERB_GET_PARAMETER | PARAM_FN_GROUP_TYPE) & 0x7F;
        if (parameter != GROUP_AUDIO) {
            continue;
        }
        codec_transmission(device, codec, func_group_start + i, VERB_SET_POWER_STATE | 0x0);
        for (int j = 0; j < num_widgets; j++) {
            init_widget(device, codec, widget_start + j);
        }
    }
    return device->output->node_id ? 0 : -1;
}

// Picks the first available output widget to use as our output codec. If we structure this better, we can have multiple outputs,
// but since we didn't structure this to use an audio device on a bus provided by the PCI, we can't. We are now simply using the PCI slot for one device.
static void audio_init_codec(hda_audio_device* device) {
    uint16_t state_status = REG_INW(device, REG_STATESTS);
    for (int i = 0; i < 15; i++) {
        if (state_status & (1 << i)) {
            if (codec_init_widgets(device, i)) {
                return;
            }
        }
    }
}

void audio_reset(hda_audio_device* device) {
    // clear registers and ring buffers
    REG_OUTL(device, REG_CORBCTL, 0);
    REG_OUTL(device, REG_RIRBCTL, 0);
    // wait for the corb and rirb buffers to stop running
    while (REG_INL(device, REG_CORBCTL) & CORBCTL_CORBRUN || REG_INL(device, REG_RIRBCTL) & RIRBCTL_RIRBRUN);
    // may need some delay here
    // reset hardware
    REG_OUTL(device, REG_GCTL, 0);
    while (REG_INL(device, REG_GCTL) & GCTL_RESET);
    REG_OUTL(device, REG_GCTL, GCTL_RESET);
    while ((REG_INL(device, REG_GCTL) & GCTL_RESET) == 0);
    // clear interrupts
    REG_OUTW(device, REG_WAKEEN, 0xFFFF);
    REG_OUTL(device, REG_INTCTL, 0x800000FF);

    // restart audio - set up buffers again
    init_corb(device);
    init_rirb(device);

    // may need some delay here? might have to use jiffies in pit.h
    audio_init_codec(device);
}

void stream_descriptor_init(hda_audio_device* device) {
    uint32_t bld_b, dma_pos;
    int i;

    //set reg for ouput 
    REG_OUTB(device, REG_O0_CTLU, 0x10);
    REG_OUTL(device, REG_O0_CBL, BDL_SIZE * BUFFER_SIZE);
    REG_OUTW(device, REG_O0_STLVI, BDL_SIZE -1);

    // Set buffer list 
    bld_b = (uintptr_t) device->rings->pa[0] + 3072;
    REG_OUTL(device, REG_O0_BDLPL, bld_b & 0xFFFFFFFF);
    REG_OUTL(device, REG_O0_BDLPU, (uint32_t)((uint64_t)bld_b >> 32));

    for(i = 0; i < BDL_SIZE; i++) {
        device->bdl[i].addr = device->completed_buffers->pa[0] + (i * BUFFER_SIZE);
        device->bdl[i].length = BUFFER_SIZE;
        device->bdl[i].flags = 1;
    }
    //memset(device->completed_buffers->va, 0, BDL_SIZE * BUFFER_SIZE); from "string.h" but causes problems so commented out temporarily

    // init DMA pos in buffer
    for(i = 0; i < 8; i++) {
        device->dma_pos[i] = 0;
    }

    dma_pos = (uintptr_t) device->rings->pa[0] + 3072 + ROUNDED_BDL_BYTES;
    REG_OUTL(device, REG_DPLBASE, (dma_pos & 0xffffffff) | 0x1); // marked as TODO needs another look
    REG_OUTL(device, REG_DPUBASE, (uint32_t)((uint64_t)dma_pos >> 32));
}

/* not sure what to do w this
static struct audio_driver* driver = {

};
*/

static struct audio_device* init_dev(PCI::pci_device* device) {

    PCI::pci_device* pci = device;
    hda_audio_device* hda;

    hda = (hda_audio_device*)malloc(sizeof(hda_audio_device));
    hda->rings = new mem_area();
    hda->rings->pa = PhysMem::alloc_frame() | 7;
    hda->rings->va = (void*) 0xFEC01000;
    gheith::map(gheith::current()->pd, hda->rings->va, hda->rings->pa);
    hda->corb = (uint32_t*) ((uintptr_t) hda->rings->va + 0);
    hda->rirb = (uint32_t*) ((uintptr_t) hda->rings->va + 1024);
    hda->bdl = (audio_bdl_entry*) ((uintptr_t) hda->rings->va + 3072);
    hda->dma_pos = (uint32_t*) ((uintptr_t) hda->rings->va + 3072 ROUNDED_BDL_BYTES);

    if(hda->rings == nullptr) {
        PhysMem::dealloc_frame(hda->rings);
        free(hda);
        return nullptr;
    }
    hda->mmio->pa = PhysMem::alloc_frame();
    hda->completed_buffers = malloc(4096);
    if(hda->completed_buffers == nullptr) return nullptr;

    hda->mmio->pa = PhysMem::alloc_frame();

    audio_reset(hda);
    init_output_widget(hda);
    stream_descriptor_init(hda);

    hda->audio->recorder = 0;
    //hda->audio->streams = 

    return &hda->audio;    
}

void audio_set_volume(audio_stream* stream, uint8_t volume) {

    hda_audio_device* hda = (hda_audio_device*) stream->device;
    int meta = 0xB000; // output amp

    if(volume == 0) {
        //set mute bit
        volume = 0x80;
    } else {
        // scale volume to amp_gain
        volume = volume * hda->output->amp_gain / 255;
    }
    codec_transmission(hda, hda->output->codec, hda->output->node_id, VERB_SET_AMP_GAIN_MUTE | meta | volume);  
}

int audio_set_sample_rate(audio_stream* stream, int sr) {

    hda_audio_device* hda = (hda_audio_device*) stream->device;

    switch(sr) {
        case 44100:
            hda->output->sample_rate = SR_44_KHZ;
            break;
        default:
            sr = 48000;
            hda->output->sample_rate = SR_48_KHZ;
            break;
    }
    output_widget_config(hda);
    return sr;  
}

int audio_set_chnl_ct(audio_device* dev, int channels) {
    hda_audio_device* hda = (hda_audio_device*) dev->device;
    if(channels < 1 || channels > 2) {
        channels = 2;
    }

    hda->output->num_channels = channels;
    output_widget_config(hda);

    return channels;
}

void get_audio_pos(audio_stream* stream, audio_position* pos) {
    
    hda_audio_device* hda = (hda_audio_device*)stream->device;
    uint32_t position = hda->dma_pos[4] & 0xffffffff;

    pos->buffer = position / BUFFER_SIZE;
    pos->frame = (position % BUFFER_SIZE) / 2;

}
