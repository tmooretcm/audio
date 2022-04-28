#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "elf.h"
#include "machine.h"
#include "libk.h"
#include "config.h"

Shared<Node> checkFile(const char* name, Shared<Node> node) {
    // CHECK(node != nullptr);
    // CHECK(node->is_file());
    Debug::printf("file %s is ok\n",name);
    return node;
}

Shared<Node> getFile(Shared<Ext2> fs, Shared<Node> node, const char* name) {
    return checkFile(name,fs->find(node,name));
}

Shared<Node> checkDir(const char* name, Shared<Node> node) {
    Debug::printf("checking %s\n",name);
    // CHECK (node != nullptr);
    // CHECK (node->is_dir());
    Debug::printf("directory %s is ok\n",name);
    return node;
}

Shared<Node> getDir(Shared<Ext2> fs, Shared<Node> node, const char* name) {
    return checkDir(name,fs->find(node,name));
}

Shared<Ext2> fs;

void kernelMain(void) {
    auto d = Shared<Ide>::make(1);
    Debug::printf("mounting drive 1\n");
    fs = Shared<Ext2>::make(d);
    auto root = checkDir("/",fs->root);
    auto sbin = getDir(fs,root,"sbin");
    auto init = getFile(fs,sbin,"init");

    Debug::printf("loading init\n");
    uint32_t e = ELF::load(init);
    Debug::printf("entry %x\n",e);
    auto userEsp = 0xefffe000;
    Debug::printf("user esp %x\n",userEsp);
    // init\0 = 5 chars, + 1 extra char for argc = 6 * 4 = 24
    userEsp -= 20;
    char** argv = new char*[1];
    argv[0] = (char*)"init\0";
    memcpy((void*)(userEsp), &argv, 5);
    userEsp -= 4;
    uint32_t argc = 1;
    memcpy((void*)userEsp, &argc, 4);
    // Current state:
    //     - %eip points somewhere in the middle of kernelMain
    //     - %cs contains kernelCS (CPL = 0)
    //     - %esp points in the middle of the thread's kernel stack
    //     - %ss contains kernelSS
    //     - %eflags has IF=1
    // Desired state:
    //     - %eip points at e
    //     - %cs contains userCS (CPL = 3)
    //     - %eflags continues to have IF=1
    //     - %esp points to the bottom of the user stack
    //     - %ss contain userSS
    // User mode will never "return" from switchToUser. It will
    // enter the kernel through interrupts, exceptions, and system
    // calls.
    switchToUser(e,userEsp,0);
    Debug::panic("implement switchToUser in machine.S\n");
}

