#include "libc.h"

void one(int fd) {
}

int main(int argc, char** argv) {
    int fd = open("/carti.wav",0);
    printf("%d\n", fd);
    shutdown();
    return 0;
}
