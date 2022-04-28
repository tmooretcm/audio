#ifndef _process_h_
#define _process_h_

#include "ext2.h"
#include "atomic.h"
#include "future.h"

struct FileDescriptor{
    Shared<Node> file;
    uint32_t offset;
    bool reserved;
    Atomic<int> ref_count;
    FileDescriptor() : file(nullptr), offset(0), reserved(false), ref_count(0) {}
};

struct PCB {
    // 0 to 9
    Shared<FileDescriptor> fd[10];
    // 10 to 19
    Shared<Semaphore> sp[10];
    // 20 to 29
    Shared<Future<int>> cp[10];

    Shared<Future<int>> future;

    PCB() {
        for (int i = 0; i < 10; i++) {
            fd[i] = nullptr;
            cp[i] = nullptr;
            sp[i] = nullptr;
        }
        fd[0] = new FileDescriptor();
        fd[1] = new FileDescriptor();
        fd[2] = new FileDescriptor();
        fd[0]->reserved = true;
        fd[1]->reserved = true;
        fd[2]->reserved = true;
        future = Shared<Future<int>>::make();
    }
};

#endif