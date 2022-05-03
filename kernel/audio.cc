#include "audio.h"
#include "debug.h"

// update interrupt status
using namespace audio;

static audio_driver* driver;

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


static void init_corb(hda_audio_device* device) {
    uint8_t reg;
    uint32_t corb_base_addr;

    // read byte from corb register
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

    // place reg byte into register
    REG_OUTB(device, REG_CORBSIZE, reg);

    // initialize corb base address
    corb_base_addr = (uintptr_t)device->rings->pa[0];
    REG_OUTL(device, REG_CORBLBASE, corb_base_addr & 0xffffffff);
    REG_OUTL(device, REG_CORBUBASE, corb_base_addr >> 32);

    // start DMA
    REG_OUTB(device, REG_CORBCTL, 0x02);
}

static void init_rirb(hda_audio_device* device) {
    uint8_t reg;
    uint32_t rirb_base_addr;

    // read byte from rirb register
    reg = REG_INB(device, REG_RIRBSIZE);

    // checking rirb sizes
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

    // place reg byte into register
    REG_OUTB(device, REG_RIRBSIZE, reg);


    // initialize rirb base addreses
    rirb_base_addr = (uintptr_t)device->rings->pa[0] + 1024; // should be pa[256] (?)
    REG_OUTL(device, REG_RIRBLBASE, rirb_base_addr & 0xffffffff);
    REG_OUTL(device, REG_RIRBUBASE, rirb_base_addr >> 32);

    // set interrupt cnt register
    REG_OUTB(device, REG_RINTCNT, 0x42);

    // start DMA
    REG_OUTB(device, REG_RIRBCTL, 0x02);
}
