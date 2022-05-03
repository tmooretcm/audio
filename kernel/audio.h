#ifndef _audio_h_
#define _audio_h_

#include "pci.h"

namespace audio {

    typedef struct alf {
        uint32_t addr;
        uint32_t length;
        uint32_t flags;
    } alf;

    typedef struct audio_stream {
        // registers
        uint32_t CTL;
        uint32_t LPIB;
        uint32_t CBL;
        uint32_t LVI;
        uint32_t FMT;
        uint32_t BDLP_lbase;
        uint32_t BDLP_rbase;

        // keeping track of alf state.
        alf* alf;
        uint32_t alf_entries;
        uint32_t alfsize; alfe; alfp;
    } audio_stream;

    typedef struct audio_state {

        PCI::pci_device* pci;
        char* name;
        // codecs here? how to make codec struct

        // registers
        uint32_t GCAP; // global capabilities
        uint32_t VMIN; // version minor
        uint32_t VMAJ; // version major 
        uint32_t OUTPAY; // output payload capability
        uint32_t INPAY; // input payload capability

        uint32_t GCTL; // global control
        uint32_t WAKEEN; // wake enable
        uint32_t STATESTS; // state change status
        uint32_t GSTS; // global status
        uint32_t OUTSTRMPAY; // output stream payload capability
        uint32_t INSTRMPAY; // input stream payload capability

        uint32_t INTCTL; // interrupt control
        uint32_t INTSTS; // interrupt status
        uint32_t COUNTER; // wall clock counter
        uint32_t SSYNC; // stream synchronization

        uint32_t CORBLBASE; // corb lower base address
        uint32_t CORBUBASE; // corb upper base address
        uint32_t CORBWP; // corb write pointer
        uint32_t CORBRP; // corb read pointer
        uint32_t CORBCTL; // corb control
        uint32_t CORBSTS; // corb status
        uint32_t CORBSIZE; // corb size

        uint32_t RIRBLBASE; // rirb lower base address
        uint32_t RIRBUBASE; // rirb upper base address
        uint32_t RIRBWP; // rirb write pointer
        uint32_t RIRBCNT; // response interrupt count
        uint32_t RIRBCTL; // rirb control
        uint32_t RIRBSTS; // rirb status
        uint32_t RIRBSIZE; // rirb size

        uint32_t DPLBASE; // dma position lower base address
        uint32_t DPUBASE; // dma position upper base address

        audio_stream streams[8];

    } audio_state;

}

#endif