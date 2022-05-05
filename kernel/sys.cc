#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "machine.h"
#include "config.h"
#include "ext2.h"
#include "elf.h"
#include "kernel.h"
#include "semaphore.h"
#include "shared.h"
#include "future.h"
#include "vmm.h"
#include "physmem.h"
#include "process.h"

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    auto me = gheith::current();
    auto my_pcb = me->pcb;
    uint32_t* user_esp = (uint32_t*)frame[3];
    auto root = fs->root;
    switch (eax) 
    {
        case 0: // exit process
        {
            // grab exit code off stack
            int rc = user_esp[1];
            // set future to exit code
            my_pcb->future->set(rc);
            // stop should handle all deallocation through zombie queue deletion
            stop();
        }
        case 1: // write
        {
            // get file descriptor id
            uint32_t fnum = user_esp[1];
            // check that there's actually a file open here
            if (my_pcb->fd[fnum] == nullptr) {
                return -1;
            }
            // check if a previously reserved spot was closed
            if (fnum < 3 && !my_pcb->fd[fnum]->reserved) {
                return -1;
            }
            char* buffer = (char*)user_esp[2];
            uint32_t buffer_addr = user_esp[2];
            // checking to make sure buffer is entirely in private space
            if (buffer_addr < 0x80000000 || (buffer_addr >= kConfig.localAPIC && buffer_addr < kConfig.localAPIC + 4096) 
                || (buffer_addr >= kConfig.ioAPIC && buffer_addr < kConfig.ioAPIC + 4096)) {
                return -1;
            }
            size_t nbyte = user_esp[3];
            for (size_t i = 0; i < nbyte; i++) {
                Debug::printf("%c", buffer[i]);
            }
            return nbyte;
        }
        case 2: // fork
        {
            // First check if there's any room for a new child process.
            int pid = -1;
            bool found = false;
            while (!found && pid < 10) {
                pid++;
                if (my_pcb->cp[pid] == nullptr) {
                    pid += 20;
                    found = true;
                }
            }
            // no space for another child; return -1
            if (!found) return -1;
            
            // args for switch to user
            auto eip = frame[0];
            auto esp = frame[3];
            // Time to create the new child process. Make a new child TCB.
            auto child_tcb = get_thread([eip, esp] {
                // what arguments here? EIP and ESP
                switchToUser(eip, esp, 0);
            });
            // time to set up new child process' values. copy over file descriptors and semaphores.
            for (int i = 0; i < 10; i++) {
                child_tcb->pcb->fd[i] = my_pcb->fd[i];
                child_tcb->pcb->sp[i] = my_pcb->sp[i];
            }
            // keep the same future between parent and child
            my_pcb->cp[pid - 20] = child_tcb->pcb->future;

            // now need to deep copy memory. traverse parent tree
            int page_size = 4096;
            int num_entries = 1024;
            int treeStart = kConfig.memSize / page_size / num_entries + 1;
            for (int pdi = treeStart; pdi < 1024; pdi++) {
                // looping through page directory
                auto parent_pde = me->pd[pdi];
                if (parent_pde & 1) {
                    // time to loop through page table. mask top ten bits to get start address correctly and not offsetted by present bits.
                    uint32_t* parent_pt = (uint32_t*)(parent_pde & 0xFFFFF000);
                    for (int pti = 0; pti < num_entries; pti++) {
                        // grab the physical frames/ptes
                        uint32_t parent_frame = parent_pt[pti];
                        uint32_t child_frame;
                        // get the virtual address. shift and add pdi + pti
                        uint32_t va = (pdi << 22) | (pti << 12);
                        // share mappings for apics
                        if (va == kConfig.ioAPIC) {
                            // dont forget to set present bits
                            child_frame = kConfig.ioAPIC | 7;
                            gheith::map(child_tcb->pd, kConfig.ioAPIC, child_frame);
                        } else if (va == kConfig.localAPIC) {
                            child_frame = kConfig.localAPIC | 7;
                            gheith::map(child_tcb->pd, kConfig.localAPIC, child_frame);
                        } else if (parent_frame & 1) {
                            // parent frame present and not an apic - deep copy physical frame and map
                            child_frame = PhysMem::alloc_frame() | 7;
                            memcpy((uint32_t*)(child_frame & 0xFFFFF000), (uint32_t*)(parent_frame & 0xFFFFF000), page_size);
                            gheith::map(child_tcb->pd, va, child_frame);
                        }
                    }
                }
            }
            // schedule child thread to run
            gheith::schedule(child_tcb);
            return pid;
        }
        case 3: // sem
        {
            // create a new semaphore and return index in array + 10
            uint32_t initial = user_esp[1];
            for (int i = 0; i < 10; i++) {
                if (my_pcb->sp[i] == nullptr) {
                    my_pcb->sp[i] = new Semaphore(initial);
                    return i + 10;
                }
            }
            // no semaphores available; already made ten
            return -1;
        }
        case 4: // up on sem
        {
            uint32_t id = user_esp[1];
            if (id > 19 || id < 10) {
                return -1;
            }
            if (my_pcb->sp[id-10] == nullptr) {
                return -1;
            }
            my_pcb->sp[id-10]->up();
            return 0;
        }
        case 5: // down on sem
        {
            uint32_t id = user_esp[1];
            if (id > 19 || id < 10) {
                return -1;
            }
            if (my_pcb->sp[id-10] == nullptr) {
                return -1;
            }
            my_pcb->sp[id-10]->down();
            return 0;
        }
        case 6: // close something
        {
            uint32_t num = user_esp[1];

            // validity bounds checking - is num in range of our arrays or not?
            if (num < 0 || num > 29) {
                return -1;
            }
            if (num < 10) {
                // file
                if (my_pcb->fd[num] == nullptr) {
                    // shouldn't be closing something already closed
                    return -1;
                }
                // reset values
                my_pcb->fd[num]->offset = 0;
                if (num < 3) {
                    // closing reserved spaces - this space is no longer reserved
                    my_pcb->fd[num]->reserved = false;
                }
                my_pcb->fd[num] = nullptr;
            } else if (num < 20) {
                // semaphores
                if (my_pcb->sp[num - 10] == nullptr) {
                    return -1;
                }
                my_pcb->sp[num - 10] = nullptr;
            } else if (num < 30) {
                // process
                if (my_pcb->cp[num - 20] == nullptr) {
                    return -1;
                }
                my_pcb->cp[num - 20] = nullptr;
            }
            return 0;
        }
        case 7: // shutdown 
        {
            Debug::shutdown();
        }
        case 8: // wait for child to start running; use future
        {
            // grab the status off the stack
            uint32_t* status = (uint32_t*)user_esp[2];
            // bounds checking for valid status in private space
            if ((uint32_t)status < 0x80000000 || ((uint32_t)status >= kConfig.ioAPIC && (uint32_t)status < kConfig.ioAPIC + 4096) 
                || ((uint32_t)status >= kConfig.localAPIC && (uint32_t)status < kConfig.localAPIC + 4096)) {
                return -1;
            }
            uint32_t id = user_esp[1];
            // bounds checking for valid id
            if (id < 20 || id > 29) {
                return -1;
            }
            // overwrite status with the child's rc; child's rc should be set whenever the child process exits (block until child exits).
            *status = my_pcb->cp[id - 20]->get();
            return 0;
        }   
        case 9: // exec
        {
            // First we need to figure out how many arguments there are. Loop through the stack until there's a zero to find out.
            int argc = 0;
            // start at index 1 - index 0 should be the RA.
            uint32_t index = 1;
            while (user_esp[index] != 0) {
                argc++;
                index++;
            }
            // set up all the arguments. we need to save them in kernel space before we clear private space. let's make copies
            char* arguments[argc];
            // keep track of argument lengths too so we know how much to copy.
            int arg_lengths[argc];
            int total_length = 0;
            // loop through all the arguments
            for (int i = 0; i < argc; i++) {
                index = 0;
                // look at the current argument. figure out how long it is
                char* arg = (char*)user_esp[i + 1];
                while (arg[index] != 0) {
                    index++;
                }
                // save the arguments. adjust by +1 to account for the null terminator.
                arguments[i] = new char[index + 1];
                arg_lengths[i] = index + 1;
                total_length += index + 1;
                memcpy(arguments[i], arg, arg_lengths[i]);
            }

            // clear out all of private space
            for (uint32_t address = 0x80000000; address < kConfig.memSize; address += 4096){
                // skip over apics
                if (address != kConfig.localAPIC && address != kConfig.ioAPIC) {
                    gheith::unmap(me->pd, address);
                }
            }

            // now that we have cleared VM space, we can start setting up the stack of our new program. top half = values, bottom half = parameters for function
            uint32_t new_user_esp = 0xefffe000 - total_length;
            // stack for our parameters - keeps track of the pointers (argvs)
            uint32_t argvs[argc];
            // setting up top half of stack
            for (int i = 0; i < argc - 1; i++) {
                memcpy((void*)new_user_esp, arguments[i + 1], arg_lengths[i + 1]);
                argvs[i] = new_user_esp;
                new_user_esp += arg_lengths[i + 1];
            }

            // now that we've set up the top half of the stack (the actual values of the arguments), we need to set up the bottom half (the parameters)
            // align against lowest frame, then subtract by however many arguments we have in argvs
            new_user_esp = PhysMem::framedown(0xefffe000 - total_length) - (argc * 4);
            // copy the argvs into the stack
            memcpy((void*) new_user_esp, argvs, argc * 4);
            // now we have to set up the parameters we pass into the function
            uint32_t argvpp = new_user_esp;
            uint32_t* args = (uint32_t*)(new_user_esp - 12);
            // argc - subtract one for argv**
            args[0] = argc - 1;
            // argv**
            args[1] = argvpp;
            // null terminator
            args[2] = 0;
            // after this point, args[3] should then point to argv[0] - beginning of the actual values of the arguments
            
            // can load program and switch to user now that VM has been cleared and setup with our new stack!
            auto program = fs->find(root, (char*)user_esp[1]);
            switchToUser(ELF::load(program), new_user_esp - 12, 0);
            return -1;
        }
        case 10: // open file
        {
            char* filename = (char*)user_esp[1];
            if ((uint32_t) filename < 0x80000000) {
                return -1;
            }
            // checking if there's a valid filename
            auto file = fs->find(root, filename);
            if (file == nullptr) {
                return -1;
            }
            // find smallest fd
            for (uint32_t i = 0; i < 10; i++) {
                if (my_pcb->fd[i] == nullptr) {
                    my_pcb->fd[i] = new FileDescriptor();
                    my_pcb->fd[i]->file = file;
                    return i;
                }
            }
            // return -1 if ten files are open
            return -1;
        }
        case 11: // len (file)
        {
            uint32_t fnum = user_esp[1];
            // validity checks
            if (fnum < 0 || fnum > 9) {
                return -1;
            }
            if (my_pcb->fd[fnum] == nullptr) {
                return -1;
            }
            // len for stdin, stdout, stderr should return 0
            if (my_pcb->fd[fnum]->reserved) {
                return 0;
            }
            return my_pcb->fd[fnum]->file->size_in_bytes();
        }
        case 12: // read from file into buffer
        {
            // validity checks
            uint32_t fnum = user_esp[1];
            char* buffer = (char*) user_esp[2];
            // size_t nbyte = (size_t) user_esp[3];
            if (fnum < 0 || fnum > 9) {
                return -1;
            }
            auto descriptor = my_pcb->fd[fnum];
            if (descriptor == nullptr) {
                return -1;
            }
            // read for stdin, stdout, stderr should return -1
            if (descriptor->reserved) {
                return -1;
            }
            uint32_t buffer_addr = (uint32_t)buffer;
            // checking for private space
            if (buffer_addr < 0x80000000 || (buffer_addr >= kConfig.localAPIC && buffer_addr < kConfig.localAPIC + 4096) 
                || (buffer_addr >= kConfig.ioAPIC && buffer_addr < kConfig.ioAPIC + 4096)) {
                return -1;
            }
            uint32_t num_to_read = user_esp[3];
            // check to see if we hit end of file
            if (descriptor->offset + num_to_read >= descriptor->file->size_in_bytes()) {
                num_to_read = descriptor->file->size_in_bytes() - descriptor->offset;         
            }
            // start reading at offset. check to see if we hit apics
            if (buffer_addr + num_to_read >= kConfig.ioAPIC && buffer_addr + num_to_read < kConfig.ioAPIC + 4096) {
                num_to_read = kConfig.ioAPIC - buffer_addr;
            }
            if (buffer_addr + num_to_read >= kConfig.localAPIC && buffer_addr + num_to_read < kConfig.localAPIC + 4096) {
                num_to_read = kConfig.localAPIC - buffer_addr;
            }
            // no edge cases; read and update offset
            uint32_t bytes_read = descriptor->file->read_all(descriptor->offset, num_to_read, buffer);
            descriptor->offset += bytes_read;
            return bytes_read;         
        }
        case 13: // seek, setting offset
        {
            uint32_t fnum = user_esp[1];
            // validity checks
            if (fnum < 0 || fnum > 9) {
                return -1;
            }
            if (my_pcb->fd[fnum] == nullptr) {
                return -1;
            }
            // seek for stdin, stdout, stderr should return -1
            if (my_pcb->fd[fnum]->reserved) {
                return -1;
            }
            int off = user_esp[2];
            my_pcb->fd[fnum]->offset = off;
            return off;
        }
        case 14: // play
        {
            
        }
        default:
        {
            return -1;
        }
    }
}

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}
