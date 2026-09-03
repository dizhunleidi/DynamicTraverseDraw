#ifndef DIRW_TGODRW_H
#define DIRW_TGODRW_H

#include "baseRW.hpp"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <atomic>

namespace diRW {

// 与内核模块 DiDevice 的 comm.h 保持一致的 ABI，两端必须同步修改
struct TGodCopyMemory {
    size_t size;
    pid_t pid;
    uintptr_t addr;
    void* buffer;
};

enum TGodOperations {
    TGod_CONNECT          = 0x11401, // 连接握手，驱动返回 114514
    TGod_READ_MEM         = 0x11402, // 普通读取，返回实际读取字节数
    TGod_READ_MEM_NOCACHE = 0x11403, // 无缓存读取，返回实际读取字节数
    TGod_WRITE_MEM        = 0x11404, // 写入内存，返回实际写入字节数
    TGod_GET_MODULE_BASE  = 0x11405, // 驱动暂未实现
};

// TGod 内核模块后端：模块 hook inet_ioctl，通过任意 inet socket 的 ioctl 通讯
// 需要 root（驱动侧过滤 current_uid != 0）+ 已加载 DiDevice.kpm
class TGodRW : public baseRW {
public:
    enum class ReadMode {
        NORMAL,   // 有缓存读取
        NO_CACHE  // 无缓存读取（页表 + vmap，绕过 cache）
    };

protected:
    int fd = -1;

    // 读模式的管理复用 pid 的 Global/Private 语义：
    //   Global  —— 存静态 globalMod，setMod 对所有 Global 实例（跨线程）生效
    //   Private —— 存实例 localMod，多线程各用各的，互不干扰
    bool useGlobalMod = true;
    ReadMode localMod = ReadMode::NORMAL;
    // 多线程切换读模式时保证可见性；relaxed 足够（只同步标志本身）
    inline static std::atomic<ReadMode> globalMod = ReadMode::NORMAL;

public:
    // modMode: 读模式的作用域，与目标 pid 的 PidMode 是两回事，互不影响
    TGodRW(PidMode mode, int tpid = 0, PidMode modMode = PidMode::Global);
    ~TGodRW() override;
    uintptr_t get_module_base(const char* name) override;
    bool readv(uintptr_t address, void *buffer, size_t size) override;
    bool writev(uintptr_t address, void *buffer, size_t size) override;

    // Global 实例改全局模式，Private 实例只改自己
    void setMod(ReadMode mod);
    ReadMode getMod() const; // 本实例当前生效的模式

    // 显式操作全局模式（所有 Global 实例生效，Private 实例不受影响）
    static void setGlobalMod(ReadMode mod);
    static ReadMode getGlobalMod();
};

// --- inline implementations ---

inline TGodRW::TGodRW(PidMode mode, int tpid, PidMode modMode)
    : baseRW(mode, tpid), useGlobalMod(modMode == PidMode::Global) {
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf("\n[-] TGod socket 创建失败\n");
        return;
    }

    long ret = ioctl(fd, TGod_CONNECT, nullptr);
    if (ret == 114514) {
        connected = true;
        printf("\n[+] TGod 驱动连接成功。\n");
    } else {
        printf("\n[-] TGod 驱动未加载或无 root 权限 (code:%ld)\n", ret);
    }
}

inline TGodRW::~TGodRW() {
    if (fd > 0)
        close(fd);
}

inline void TGodRW::setMod(ReadMode mod) {
    if (useGlobalMod)
        globalMod.store(mod, std::memory_order_relaxed);
    else
        localMod = mod;
}

inline TGodRW::ReadMode TGodRW::getMod() const {
    return useGlobalMod ? globalMod.load(std::memory_order_relaxed) : localMod;
}

inline void TGodRW::setGlobalMod(ReadMode mod) {
    globalMod.store(mod, std::memory_order_relaxed);
}

inline TGodRW::ReadMode TGodRW::getGlobalMod() {
    return globalMod.load(std::memory_order_relaxed);
}

inline bool TGodRW::readv(uintptr_t address, void *buffer, size_t size) {
    if (!connected)
        return false;
    TGodCopyMemory cm;
    cm.size = size;
    cm.pid = getPid();
    cm.addr = address;
    cm.buffer = buffer;
    ReadMode mod = useGlobalMod ? globalMod.load(std::memory_order_relaxed) : localMod;
    // 驱动返回实际读取字节数，0/负值 = 读取失败
    unsigned int cmd = (mod == ReadMode::NO_CACHE) ? TGod_READ_MEM_NOCACHE
                                                   : TGod_READ_MEM;
    long ret = ioctl(fd, cmd, &cm);
    return ret > 0;
}

inline bool TGodRW::writev(uintptr_t address, void *buffer, size_t size) {
    if (!connected)
        return false;
    TGodCopyMemory cm;
    cm.size = size;
    cm.pid = getPid();
    cm.addr = address;
    cm.buffer = buffer;
    // 驱动返回实际写入字节数，0/负值 = 写入失败
    long ret = ioctl(fd, TGod_WRITE_MEM, &cm);
    return ret > 0;
}

inline uintptr_t TGodRW::get_module_base(const char* name) {
    // 驱动的 GET_MODULE_BASE(0x11405) 尚未实现，先走 /proc/<pid>/maps
    char path[64];
    char line[1024];
    snprintf(path, sizeof(path), "/proc/%d/maps", getPid());
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
        return 0;
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, name)) {
            base = strtoul(line, NULL, 16);
            if (base == 0x8000)
                base = 0;
            break;
        }
    }
    fclose(fp);
    return base;
}

} // namespace diRW

#endif
