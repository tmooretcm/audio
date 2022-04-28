#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "shared.h" 
#include "ext2.h"
void kernelMain(void);

extern Shared<Ext2> fs;

#endif
