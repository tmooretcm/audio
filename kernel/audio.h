#ifndef _audio_h_
#define _audio_h_

#include "pci.h"

#define BDL_SIZE 4
#define BUFFER_SIZE 0x10000
#define ROUNDED_BDL_BYTES ((BDL_SIZE * sizeof(struct audio_bdl_entry) + 127) & ~127)
#define BUFFER_SAMPLES (BUFFER_SIZE / 2)

extern PCI::pci_device* init_dev(PCI::pci_device* device);

namespace audio {

    // HDA Memory Mapped Registers
    enum HDA_REGISTERS {
        REG_GCTL        = 0x08,     // Global Control
        REG_WAKEEN      = 0x0c,     // Wake Enable
        REG_STATESTS    = 0x0e,     // State Change Status
        REG_INTCTL      = 0x20,     // Interrupt Control
        REG_INTSTS      = 0x24,     // Interrupt Status

        REG_CORBLBASE   = 0x40,     // CORB Lower Base Address
        REG_CORBUBASE   = 0x44,     // CORB Upper Base Address
        REG_CORBWP      = 0x48,     // CORB Write Pointer
        REG_CORBRP      = 0x4a,     // CORB Read Pointer
        REG_CORBCTL     = 0x4c,     // CORB Control
        REG_CORBSIZE    = 0x4e,     // CORB size

        REG_RIRBLBASE   = 0x50,     // RIRB Lower Base Address
        REG_RIRBUBASE   = 0x54,     // RIRB Upper Base Address
        REG_RIRBWP      = 0x58,     // RIRB Write Pointer
        REG_RINTCNT     = 0x5a,     // Respone Interrupt Count
        REG_RIRBCTL     = 0x5c,     // RIRB Control
        REG_RIRBSTS     = 0x5d,     // RIRB Interrupt Status
        REG_RIRBSIZE    = 0x5e,     // RIRB size

        REG_DPLBASE     = 0x70,     // DMA Position Lower Base Address
        REG_DPUBASE     = 0x74,     // DMA Posiition Upper Base Address

        REG_O0_CTLL     = 0x100,    // Control Lower
        REG_O0_CTLU     = 0x102,    // Control Upper
        REG_O0_STS      = 0x103,    // Status
        REG_O0_CBL      = 0x108,    // Cyclic Buffer Length
        REG_O0_STLVI    = 0x10c,    // Last Valid Index
        REG_O0_FMT      = 0x112,    // Format
        REG_O0_BDLPL    = 0x118,    // BDL Pointer Lower
        REG_O0_BDLPU    = 0x11c,    // DL Pointer Upper
    };

    enum HDA_REG_GCTL_BITS {
        GCTL_RESET = (1 << 0),
    };

    enum HDA_REG_CORBCTL_BITS {
        CORBCTL_CORBRUN = (1 << 1),
    };

    enum HDA_REG_RIRBCTL_BITS {
        RIRBCTL_RIRBRUN = (1 << 1),
    };

    enum HDA_REG_SDCTL_BITS {
        SDCTL_RUN = 0x2, // enable dma engine
        SDCTL_IOCE = 0x4, // enable interrupt on complete
    };

    typedef enum {
        AUDIO_16SI = 0,
        AUDIO_8SI,
        AUDIO_32SI
    } audio_sample_format;

    typedef struct audio_position {
        uint32_t buffer;
        uint32_t frame;
    } audio_position;

    typedef enum {
        AUDIO_STOP = 0,
        AUDIO_PLAY = 1
    } audio_status;
    
    // Buffer Descriptor List
    typedef struct audio_bdl_entry {
        uint32_t addr;
        uint32_t length;
        uint32_t flags;
    } audio_bdl_entry;

    struct audio_stream;

    typedef struct audio_device {
        PCI::pci_device* device;
        // does this device record or not
        int recorder;
        // list of streams - should be 8
        audio_stream** streams;
    } audio_device;

    typedef struct audio_stream {
        audio_device* device;
        uint32_t num_buffers;
        uint32_t buffer_size;
        audio_sample_format sample_format;
    } audio_stream;

    // called by a driver whenever a buffer is finished (played or recorded through)
    void audio_buffer_complete(audio_stream* stream, uint32_t buffer);

    typedef struct hda_audio_output {
        audio_stream* stream;
        uint8_t codec;
        uint16_t node_id;
        uint32_t sample_rate;
        int amp_gain;
        int num_channels;
    } hda_audio_output;

    typedef struct mem_area {
        uint32_t size;
        void* va;
        uint32_t* pa;
        int mapped;
    } mem_area;

    typedef struct hda_audio_device {
        audio_device* audio;
        hda_audio_output* output;
        
        mem_area* mmio;
        uintptr_t mmio_base;

        mem_area* rings; // Memory space for each ring buffer
        uint32_t* corb; // corb buffer
        volatile uint32_t* rirb; // rirb buffer
        audio_bdl_entry** bdl; // buffer descriptor list
        volatile uint32_t* dma_pos; // dma position in buffer

        uint32_t corb_entries;
        uint32_t rirb_entries;
        uint16_t rirb_rp; // rirb read pointer

        mem_area* completed_buffers;
        int num_buffs_completed;

    } hda_audio_device;

    // place a given double word value into a register
    static inline void REG_OUTL(hda_audio_device* device, uint32_t reg, uint32_t val) {
        volatile uint32_t* mmio = (uint32_t*)((uint8_t*)device->mmio->va + reg);
        *mmio = val;
    }

    // read a double word value from a register
    static inline uint32_t REG_INL(hda_audio_device* device, uint32_t reg) {
        volatile uint32_t* mmio = (uint32_t*)((uint8_t*)device->mmio->va + reg);
        return *mmio;
    }

    // place a given word value into a register
    static inline void REG_OUTW(hda_audio_device* device, uint32_t reg, uint16_t val) {
        volatile uint16_t* mmio = (uint16_t*)((uint8_t*)device->mmio->va + reg);
        *mmio = val;
    }

    // read a word value from a register
    static inline uint16_t REG_INW(hda_audio_device* device, uint32_t reg) {
        volatile uint16_t* mmio = (uint16_t*)((uint8_t*)device->mmio->va + reg);
        return *mmio;
    }

    // place a given byte value into a register
    static inline void REG_OUTB(hda_audio_device* device, uint32_t reg, uint8_t val) {
        volatile uint8_t* mmio = (uint8_t*)((uint8_t*)device->mmio->va + reg);
        *mmio = val;
    }

    // read a byte value from a register
    static inline uint8_t REG_INB(hda_audio_device* device, uint32_t reg) {
        volatile uint8_t* mmio = (uint8_t*)((uint8_t*)device->mmio->va + reg);
        return *mmio;
    }

    // codec verbs
    enum VERBS {
        VERB_GET_PARAMETER      = 0xf0000,
        VERB_SET_STREAM_CHANNEL = 0x70600,
        VERB_SET_FORMAT         = 0x20000,
        VERB_GET_AMP_GAIN_MUTE  = 0xb0000,
        VERB_SET_AMP_GAIN_MUTE  = 0x30000,
        VERB_GET_CONFIG_DEFAULT = 0xf1c00,
        VERB_GET_CONN_LIST      = 0xf0200,
        VERB_GET_CONN_SELECT    = 0xf0100,
        VERB_GET_PIN_CONTROL    = 0xf0700,
        VERB_SET_PIN_CONTROL    = 0x70700,
        VERB_GET_EAPD_BTL       = 0xf0c00,
        VERB_SET_EAPD_BTL       = 0x70c00,
        VERB_GET_POWER_STATE    = 0xf0500,
        VERB_SET_POWER_STATE    = 0x70500,
    };

    // codec parameters
    enum PARAMETERS {
        PARAM_NODE_COUNT        = 0x04,
        PARAM_FN_GROUP_TYPE     = 0x05,
        PARAM_AUDIO_WID_CAP     = 0x09,
        PARAM_PIN_CAP           = 0x0c,
        PARAM_CONN_LIST_LEN     = 0x0e,
        PARAM_OUT_AMP_CAP       = 0x12,
    };

    enum GROUP_TYPE {
        GROUP_AUDIO             = 0x01,
    };

    enum WIDGET_TYPE {
        WIDGET_OUTPUT           = 0x0,
        WIDGET_INPUT            = 0x1,
        WIDGET_MIXER            = 0x2,
        WIDGET_SELECTOR         = 0x3,
        WIDGET_PIN              = 0x4,
        WIDGET_POWER            = 0x5,
        WIDGET_VOLUME_KNOB      = 0x6,
        WIDGET_BEEP_GEN         = 0x7,
        WIDGET_VENDOR_DEFINED   = 0xf,
    };

    enum WIDGET_CAP {
        WIDGET_CAP_POWER_CNTRL  = (1 << 10),
        WIDGET_CAP_TYPE_SHIFT   = 20,
        WIDGET_CAP_TYPE_MASK    = (0xf << 20),
    };

    enum PIN_CAP {
        PIN_CAP_OUTPUT          = (1 << 4),
        PIN_CAP_INPUT           = (1 << 5),
    };

    enum PIN_CTL_FLAGS {
        PIN_CTL_ENABLE_OUTPUT   = (1 << 6),
    };

    enum SAMPLE_FORMAT {
        SR_48_KHZ               = 0,
        SR_44_KHZ               = (1 << 14),
        BITS_32                 = (4 <<  4),
        BITS_16                 = (1 <<  4),
    };
}


#endif