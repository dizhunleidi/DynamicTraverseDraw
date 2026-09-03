#ifndef DIRW_SYSCALLRW_H
#define DIRW_SYSCALLRW_H

#include "baseRW.hpp"
#include <cstdio>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>

namespace diRW {

class syscallRW : public baseRW {
public:
    syscallRW(PidMode mode, int tpid = 0);
    bool readv(uintptr_t address, void *buffer, size_t size) override;
    bool writev(uintptr_t address, void *buffer, size_t size) override;
    uintptr_t get_module_base(const char* name) override;

protected:
    int process_vm_readv_syscall;
    int process_vm_writev_syscall;
    inline ssize_t process_v(pid_t __pid, const struct iovec *__local_iov, unsigned long __local_iov_count,
                             const struct iovec *__remote_iov, unsigned long __remote_iov_count,
                             unsigned long __flags, bool iswrite);
    inline bool pvm(void *address, void *buffer, size_t size, bool iswrite);
};

// --- inline implementations ---

inline syscallRW::syscallRW(PidMode mode, int tpid) : baseRW(mode, tpid) {
    process_vm_readv_syscall = 270;
    process_vm_writev_syscall = 271;
    // 系统调用后端没有连接概念，通道始终可用
    connected = true;
}

inline bool syscallRW::readv(uintptr_t address, void *buffer, size_t size) {
    return pvm(reinterpret_cast<void*>(address), buffer, size, false);
}

inline bool syscallRW::writev(uintptr_t address, void *buffer, size_t size) {
    return pvm(reinterpret_cast<void*>(address), buffer, size, true);
}

inline ssize_t syscallRW::process_v(pid_t __pid, const struct iovec *__local_iov, unsigned long __local_iov_count,
                                    const struct iovec *__remote_iov, unsigned long __remote_iov_count,
                                    unsigned long __flags, bool iswrite) {
    return syscall((iswrite ? process_vm_writev_syscall : process_vm_readv_syscall),
                   __pid, __local_iov, __local_iov_count, __remote_iov, __remote_iov_count, __flags);
}

inline bool syscallRW::pvm(void *address, void *buffer, size_t size, bool iswrite) {
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = buffer;
    local[0].iov_len = size;
    remote[0].iov_base = address;
    remote[0].iov_len = size;
    if (getPid() < 0)
        return false;
    ssize_t bytes = process_v(getPid(), local, 1, remote, 1, 0, iswrite);
    return bytes == (ssize_t)size;
}

inline uintptr_t syscallRW::get_module_base(const char* name) {
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
