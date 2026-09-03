#ifndef DIRW_BASERW_H
#define DIRW_BASERW_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <atomic>

namespace diRW {

class baseRW {
public:
    enum class PidMode {
        Global,
        Private
    };

protected:
    baseRW(PidMode mode, int tpid = 0);
    int getPid() const;

private:
    // 多线程读写目标 pid 时保证可见性；relaxed 足够（只同步标志本身）
    inline static std::atomic<int> globalPid = 0;
    int localPid = 0;
    bool useGlobalPid = false;

protected:
    // 后端就绪状态：默认 false，各后端构造成功时自行置 true。
    // 驱动对接失败不退出进程，由调用方查询 isConnected() 决定是否继续。
    bool connected = false;

public:
    bool isConnected() const { return connected; }
    virtual ~baseRW();
    virtual bool readv(uintptr_t address, void *buffer, size_t size) = 0;
    virtual bool writev(uintptr_t address, void *buffer, size_t size) = 0;
    virtual uintptr_t get_module_base(const char* name) = 0;

    static int getGlobalPid();
    static void setGlobalPid(int tpid);

    int getProcessPid();
    float getFloat(uintptr_t addr);
    int getDword(uintptr_t addr);
    bool getBool(uintptr_t addr);
    char* getUTF8(uintptr_t addr);
    uintptr_t getPtr64(uintptr_t addr);
    uintptr_t getPtr32(uintptr_t addr);
    bool writeFloat(uintptr_t addr, float data);

    template <typename... Args>
    uintptr_t jumpPoint(uintptr_t addr, Args... args) {
        static_assert(
            (std::is_same_v<Args, int> && ...),
            "jumpPoint offset type must be int"
        );
        uintptr_t result = addr;
        if constexpr (sizeof...(args) > 0) {
            (..., (readv((result + args) & 0xFFFFFFFFFF, &result, 8)));
        }
        return result;
    }
};

// --- inline implementations ---

inline baseRW::baseRW(PidMode mode, int tpid)
    : localPid(0), useGlobalPid(mode == PidMode::Global) {
    if (tpid > 0) {
        if (useGlobalPid)
            globalPid.store(tpid, std::memory_order_relaxed);
        else
            localPid = tpid;
    }
}

inline baseRW::~baseRW() {
    printf("~baseRW pid:%d\n", getPid());
}

inline int baseRW::getPid() const {
    return useGlobalPid ? globalPid.load(std::memory_order_relaxed) : localPid;
}

inline int baseRW::getProcessPid() {
    return getPid();
}

inline int baseRW::getGlobalPid() {
    return globalPid.load(std::memory_order_relaxed);
}

inline void baseRW::setGlobalPid(int tpid) {
    globalPid.store(tpid, std::memory_order_relaxed);
}

inline float baseRW::getFloat(uintptr_t addr) {
    float var = 0;
    readv(addr, &var, 4);
    return var;
}

inline int baseRW::getDword(uintptr_t addr) {
    int var = 0;
    readv(addr, &var, 4);
    return var;
}

inline bool baseRW::getBool(uintptr_t addr) {
    bool var = false;
    readv(addr, &var, 1);
    return var;
}

inline uintptr_t baseRW::getPtr32(uintptr_t addr) {
    unsigned int var = 0;
    readv(addr & 0xFFFFFFFFFF, &var, 4);
    return var & 0xFFFFFFFFFF;
}

inline uintptr_t baseRW::getPtr64(uintptr_t addr) {
    uintptr_t var = 0;
    readv(addr & 0xFFFFFFFFFF, &var, 8);
    return var & 0xFFFFFFFFFF;
}

inline char* baseRW::getUTF8(uintptr_t addr) {
    static char buf[64];
    unsigned short buf16[16] = { 0 };
    readv(addr, buf16, 28);
    unsigned short *pTempUTF16 = buf16;
    char *pTempUTF8 = buf;
    char *pUTF8End = pTempUTF8 + 32;
    while (pTempUTF16 < pTempUTF16 + 28) {
        if (*pTempUTF16 <= 0x007F && pTempUTF8 + 1 < pUTF8End) {
            *pTempUTF8++ = (char)*pTempUTF16;
        } else if (*pTempUTF16 >= 0x0080 && *pTempUTF16 <= 0x07FF && pTempUTF8 + 2 < pUTF8End) {
            *pTempUTF8++ = (*pTempUTF16 >> 6) | 0xC0;
            *pTempUTF8++ = (*pTempUTF16 & 0x3F) | 0x80;
        } else if (*pTempUTF16 >= 0x0800 && *pTempUTF16 <= 0xFFFF && pTempUTF8 + 3 < pUTF8End) {
            *pTempUTF8++ = (*pTempUTF16 >> 12) | 0xE0;
            *pTempUTF8++ = ((*pTempUTF16 >> 6) & 0x3F) | 0x80;
            *pTempUTF8++ = (*pTempUTF16 & 0x3F) | 0x80;
        } else {
            break;
        }
        pTempUTF16++;
    }
    return buf;
}

inline bool baseRW::writeFloat(uintptr_t addr, float data) {
    return writev(addr, &data, 4);
}

} // namespace diRW

#endif
