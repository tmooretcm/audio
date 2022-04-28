#include "elf.h"
#include "debug.h"

uint32_t ELF::load(Shared<Node> file) {
    ElfHeader* e_head = new ElfHeader();
    file->read_all(0, 52, (char*) e_head);

    // check magic numbers
    unsigned char m0 = e_head->magic0;
    unsigned char m1 = e_head->magic1;
    unsigned char m2 = e_head->magic2;
    unsigned char m3 = e_head->magic3;
    if (m0 != 0x7F || m1 != 'E' || m2 != 'L' || m3 != 'F') {
        // panic or return -1?
        Debug::panic("file doesn't have correct magic numbers\n");
    }

    ProgramHeader p_head_table[e_head->phnum] {};
    file->read_all(e_head->phoff, 32 * e_head->phnum, (char*) p_head_table);

    for (ProgramHeader p_head : p_head_table) {
        if (p_head.type == 1) {
            file->read_all(p_head.offset, p_head.filesz, (char*)p_head.vaddr);
        }
    }

    return e_head->entry;
}

















































