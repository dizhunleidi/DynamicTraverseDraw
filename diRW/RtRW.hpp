#ifndef DIRW_RTRW_H
#define DIRW_RTRW_H

#include "baseRW.hpp"
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

namespace diRW {

struct RtCopyMemory {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
};

struct RtModuleBase {
    pid_t pid;
    char* name;
    uintptr_t base;
};

enum RtOperations {
    RT_OP_INIT_KEY = 0x800,
    RT_OP_READ_MEM = 0x801,
    RT_OP_WRITE_MEM = 0x802,
    RT_OP_MODULE_BASE = 0x803,
    RT_OP_HIDE_PROCESS = 0x804,
};

class RtRW : public baseRW {
protected:
    int has_upper = 0;
    int has_lower = 0;
    int has_symbol = 0;
    int has_digit = 0;
    int fd = -1;
    const char* DEVICE_PATH;

public:
    char *driver_path();
    RtRW(PidMode mode, int tpid = 0);
    ~RtRW() override;
    uintptr_t get_module_base(const char* name) override;
    bool readv(uintptr_t address, void *buffer, size_t size) override;
    bool writev(uintptr_t address, void *buffer, size_t size) override;
};

// --- inline implementations ---

inline char *RtRW::driver_path() {
    struct dirent *de;
    DIR *dr = opendir("/proc");
    char *device_path = NULL;
    if (dr == NULL) {
        printf("Could not open /proc directory");
        return NULL;
    }
    while ((de = readdir(dr)) != NULL) {
        if (strlen(de->d_name) != 6 ||
            strcmp(de->d_name, "NVTSPI") == 0 ||
            strcmp(de->d_name, "ccci_log") == 0 ||
            strcmp(de->d_name, "aputag") == 0 ||
            strcmp(de->d_name, "asound") == 0 ||
            strcmp(de->d_name, "clkdbg") == 0 ||
            strcmp(de->d_name, "crypto") == 0 ||
            strcmp(de->d_name, "modules") == 0 ||
            strcmp(de->d_name, "mounts") == 0 ||
            strcmp(de->d_name, "pidmap") == 0 ||
            strcmp(de->d_name, "phoenix") == 0 ||
            strcmp(de->d_name, "uptime") == 0 ||
            strcmp(de->d_name, "vmstat") == 0) {
            continue;
        }
        int is_valid = 1;
        for (int i = 0; i < 6; i++) {
            if (!isalnum(de->d_name[i])) {
                is_valid = 0;
                break;
            }
        }
        if (is_valid) {
            device_path = (char*)malloc(11 + strlen(de->d_name));
            sprintf(device_path, "/proc/%s", de->d_name);
            struct stat sb;
            if (stat(device_path, &sb) == 0 && S_ISREG(sb.st_mode)) {
                break;
            } else {
                free(device_path);
                device_path = NULL;
            }
        }
    }
    closedir(dr);
    return device_path;
}

inline RtRW::RtRW(PidMode mode, int tpid) : baseRW(mode, tpid) {
    char *Fname = driver_path();
    fd = open(Fname, O_RDWR);
    if (fd == -1) {
        printf("\n[-] 驱动连接失败\n[-] 找不到驱动路径：%s\n", Fname);
    } else {
        connected = true;
        printf("\n[-] 驱动连接成功。\n[-] 驱动路径：%s\n", Fname);
    }
}

inline RtRW::~RtRW() {
    if (fd > 0)
        close(fd);
}

inline bool RtRW::readv(uintptr_t address, void *buffer, size_t size) {
    RtCopyMemory cm;
    cm.pid = getPid();
    cm.addr = address;
    cm.buffer = buffer;
    cm.size = size;
    if (ioctl(fd, RT_OP_READ_MEM, &cm) != 0)
        return false;
    return true;
}

inline bool RtRW::writev(uintptr_t address, void *buffer, size_t size) {
    RtCopyMemory cm;
    cm.pid = getPid();
    cm.addr = address;
    cm.buffer = buffer;
    cm.size = size;
    if (ioctl(fd, RT_OP_WRITE_MEM, &cm) != 0)
        return false;
    return true;
}

inline uintptr_t RtRW::get_module_base(const char* name) {
    RtModuleBase mb;
    char buf[0x100];
    strcpy(buf, name);
    mb.pid = getPid();
    mb.name = buf;
    if (ioctl(fd, RT_OP_MODULE_BASE, &mb) != 0)
        return 0;
    return mb.base;
}

} // namespace diRW

#endif
