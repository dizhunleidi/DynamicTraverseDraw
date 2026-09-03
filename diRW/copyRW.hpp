#ifndef DIRW_COPYRW_H
#define DIRW_COPYRW_H

#include "baseRW.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

namespace diRW {

namespace {
    bool isPageFault(uintptr_t addr) {
        static int pageSize = getpagesize();
        unsigned char vec = 0;
        unsigned long start = addr & (~(pageSize - 1));
    #if __aarch64__
        register int64_t x8 asm("x8") = __NR_mincore;
        register int64_t x0 asm("x0") = start;
        register int64_t x1 asm("x1") = pageSize;
        register int64_t x2 asm("x2") = (int64_t) &vec;

        asm volatile("svc 0"
        : "=r"(x0)
        : "r"(x8), "0"(x0), "r"(x1), "r"(x2)
        : "memory", "cc");
    #elif __arm__
    #endif
        return vec != 1;
    }
}

class copyRW : public baseRW {
public:
    copyRW();
    bool readv(uintptr_t address, void *buffer, size_t size) override;
    bool writev(uintptr_t address, void *buffer, size_t size) override;
    uintptr_t get_module_base(const char* name) override;
};

// --- inline implementations ---

inline copyRW::copyRW() : baseRW(PidMode::Private, getpid()) {
    // 本进程 memcpy 读写，没有连接概念，通道始终可用
    connected = true;
}

inline bool copyRW::readv(uintptr_t addr, void *buffer, size_t size) {
    if (addr <= 0 || isPageFault(addr))
        return false;
    if (memcpy(buffer, (void*)addr, size) == nullptr)
        return false;
    return true;
}

inline bool copyRW::writev(uintptr_t addr, void *buffer, size_t size) {
    if (addr <= 0 || isPageFault(addr))
        return false;
    if (memcpy((void*)addr, buffer, size) == nullptr)
        return false;
    return true;
}

inline uintptr_t copyRW::get_module_base(const char* name) {
    FILE *fp;
    unsigned long addr = 0;
    char filename[64];
    char line[1024];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", getPid());
    fp = fopen(filename, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, name)) {
                addr = strtoul(line, NULL, 16);
                if (addr == 0x8000)
                    addr = 0;
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

} // namespace diRW

#endif
