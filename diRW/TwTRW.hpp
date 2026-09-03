#ifndef DIRW_TWTRW_H
#define DIRW_TWTRW_H

#include "baseRW.hpp"
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <dirent.h>
#include <limits.h>
#include <vector>
#include <utility>
#include <initializer_list>

namespace diRW {

// ============ ABI 定义（与 TwT 内核驱动保持一致，两端必须同步修改） ============

// 驱动 hook 了 reboot 系统调用，用魔数分支向调用方下发 anon_inode fd（arm64 专用）
#ifndef TWT_CALL
#define TWT_CALL(magic1, magic2, cmd, arg) ({  \
    long _ret;                                             \
    register long _x0 __asm__("x0") = (long)(magic1);      \
    register long _x1 __asm__("x1") = (long)(magic2);      \
    register long _x2 __asm__("x2") = (long)(cmd);         \
    register long _x3 __asm__("x3") = (long)(arg);         \
    register long _nr __asm__("x8") = __NR_reboot;         \
    __asm__ __volatile__(                                  \
        "svc #0"                                           \
        : "=r"(_x0)                                        \
        : "r"(_x0), "r"(_x1), "r"(_x2), "r"(_x3), "r"(_nr)\
        : "memory", "cc"                                   \
    );                                                     \
    _ret = _x0;                                            \
    _ret;                                                  \
})
#endif

#pragma pack(1)
struct TwtUserPtRegs {
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    uint64_t orig_x0;
    uint64_t syscallno;
    __uint128_t vregs[32];
};

struct TwtHitItem {
    uint64_t task_id;
    uint64_t hit_addr;
    uint64_t hit_time;
    TwtUserPtRegs regs_info;
};

struct TwtGetHitCountArg {
    uint64_t handle;
    uint64_t hit_total_count;
    uint64_t hit_item_arr_count;
};

struct TwtGetHitItemsArgs {
    uint64_t handle;
    uint64_t user_buffer_ptr;
    uint64_t max_bytes;
    uint64_t items_copied;
};

struct TwtInstArgs {
    int32_t pid;
    uint64_t addr;
    uint32_t bp_len;
    uint32_t bp_type;
    uint64_t reg_modify_mask;
    uint64_t fp_reg_modify_mask;
    uint64_t regs_to_set_ptr;
    uint32_t flags;
};

struct TwtModifyArgs {
    uint64_t handle;
    uint64_t reg_modify_mask;
    uint64_t fp_reg_modify_mask;
    uint64_t regs_to_set_ptr;
};
#pragma pack()

// ioctl 请求体（内存布局必须与驱动侧一致）
struct TwtRequest {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
};

struct TwtTouchEvent {
    int slot;
    int x;
    int y;
};

struct TwtGyroConfig {
    uint32_t enable;
    uint32_t x;
    uint32_t y;
};

// ioctl 命令（magic 'T'，数值由 _IOW/_IOWR 按 ABI 结构大小编码）
enum TwtOperations : unsigned int {
    TWT_GET_PID         = _IOW('T', 0, TwtRequest),        // 按进程名取 PID
    TWT_MODULE_BASE     = _IOW('T', 1, TwtRequest),        // 取模块基址
    TWT_MODULE_BSS      = _IOW('T', 3, TwtRequest),        // 取模块 .bss 基址
    TWT_READ_MEM        = _IOW('T', 4, TwtRequest),        // 读取内存
    TWT_WRITE_MEM       = _IOW('T', 5, TwtRequest),        // 写入内存
    TWT_READ_MEM_V2     = _IOW('T', 11, TwtRequest),       // 读取内存（V2 通道）
    TWT_TOUCH_INIT      = _IOW('T', 6, TwtTouchEvent),     // 触摸初始化
    TWT_TOUCH_DOWN      = _IOW('T', 7, TwtTouchEvent),     // 触摸按下
    TWT_TOUCH_UP        = _IOW('T', 8, TwtTouchEvent),     // 触摸抬起
    TWT_GYRO_INIT       = _IOW('T', 9, int),               // 陀螺仪初始化
    TWT_GYRO_CONFIG     = _IOWR('T', 10, TwtGyroConfig),   // 陀螺仪配置
    TWT_BP_INIT_CMD     = _IO('T', 19),                    // 断点子系统初始化
    TWT_BP_CHECK_INITED = _IO('T', 30),                    // 断点子系统是否已初始化
    TWT_BP_GET_NUM_BRPS = _IO('T', 20),                    // 硬件断点寄存器数量
    TWT_BP_GET_NUM_WRPS = _IO('T', 21),                    // 硬件观察点寄存器数量
    TWT_BP_INST         = _IOWR('T', 22, char*),           // 安装断点
    TWT_BP_UNINST       = _IOW('T', 23, char*),            // 卸载断点
    TWT_BP_SUSPEND      = _IOW('T', 24, char*),            // 挂起断点
    TWT_BP_RESUME       = _IOW('T', 25, char*),            // 恢复断点
    TWT_BP_GET_HIT_COUNT= _IOWR('T', 26, char*),           // 查询命中计数
    TWT_BP_MODIFY       = _IOW('T', 27, char*),            // 命中时修改寄存器
    TWT_BP_GET_HIT_ITEMS= _IOWR('T', 28, char*),           // 拉取命中记录
};

// 断点长度/类型/标志（与驱动侧常量一致）
enum TwtBpLen : uint32_t {
    TWT_BP_LEN_1 = 1, TWT_BP_LEN_2 = 2, TWT_BP_LEN_3 = 3, TWT_BP_LEN_4 = 4,
    TWT_BP_LEN_5 = 5, TWT_BP_LEN_6 = 6, TWT_BP_LEN_7 = 7, TWT_BP_LEN_8 = 8,
};

enum TwtBpType : uint32_t {
    TWT_BP_EMPTY = 0,
    TWT_BP_R     = 1,
    TWT_BP_W     = 2,
    TWT_BP_RW    = (TWT_BP_R | TWT_BP_W),
    TWT_BP_X     = 4,
};

constexpr uint64_t TWT_REG_X(int n) { return 1ULL << n; }
constexpr uint64_t TWT_REG_MODIFY_SP     = 1ULL << 31;
constexpr uint64_t TWT_REG_MODIFY_PC     = 1ULL << 32;
constexpr uint64_t TWT_REG_MODIFY_PSTATE = 1ULL << 33;
constexpr uint32_t TWT_BP_FLAG_RECORD    = 0x1;

// 断点命中时批量修改寄存器的辅助结构（链式设置后交给 bp_apply/bp_inst）
struct TwtRegBatch {
    TwtUserPtRegs regs = {};
    uint64_t reg_mask = 0;
    uint64_t fp_mask = 0;

    TwtRegBatch& x(int idx, uint64_t val) {
        if (idx >= 0 && idx <= 30) {
            regs.regs[idx] = val;
            reg_mask |= 1ULL << idx;
        }
        return *this;
    }

    TwtRegBatch& sp(uint64_t val) {
        regs.sp = val;
        reg_mask |= TWT_REG_MODIFY_SP;
        return *this;
    }

    TwtRegBatch& pc(uint64_t val) {
        regs.pc = val;
        reg_mask |= TWT_REG_MODIFY_PC;
        return *this;
    }

    TwtRegBatch& pstate(uint64_t val) {
        regs.pstate = val;
        reg_mask |= TWT_REG_MODIFY_PSTATE;
        return *this;
    }

    TwtRegBatch& vf(int idx, float val) {
        if (idx >= 0 && idx <= 31) {
            __uint128_t v = 0;
            memcpy(&v, &val, sizeof(float));
            regs.vregs[idx] = v;
            fp_mask |= 1ULL << idx;
        }
        return *this;
    }

    TwtRegBatch& vd(int idx, double val) {
        if (idx >= 0 && idx <= 31) {
            __uint128_t v = 0;
            memcpy(&v, &val, sizeof(double));
            regs.vregs[idx] = v;
            fp_mask |= 1ULL << idx;
        }
        return *this;
    }

    TwtRegBatch& reset() {
        memset(&regs, 0, sizeof(regs));
        reg_mask = 0;
        fp_mask = 0;
        return *this;
    }

    bool empty() const { return reg_mask == 0 && fp_mask == 0; }
};

// ============ 后端实现 ============

// TwT 内核驱动后端：fd 由驱动的 reboot 魔数分支下发（anon_inode），ioctl 通信
// 需要 root + 设备上已加载 TwT 驱动；触摸/陀螺仪/断点为驱动附带功能，按需调用
class TwTRW : public baseRW {
public:
    // 读模式：驱动的两条读取通道，MOD1 走 READ_MEM，MOD2 走 READ_MEM_V2
    enum class ReadMode {
        MOD1,   // READ_MEM 通道（默认）
        MOD2    // READ_MEM_V2 通道
    };

    // gyro_mode / touch_mode：构造时独立初始化陀螺仪和触摸，互不影响
    //   -1（默认）不启用；0/1 为各自驱动的模式（陀螺仪 0=tracepoint 1=uprobe）
    // modMode: 读模式的作用域，与目标 pid 的 PidMode 是两回事，互不影响
    TwTRW(PidMode mode, int tpid = 0, PidMode modMode = PidMode::Global,
          int gyro_mode = -1, int touch_mode = -1);
    ~TwTRW() override;
    bool readv(uintptr_t addr, void* buffer, size_t size) override;
    bool writev(uintptr_t addr, void* buffer, size_t size) override;
    uintptr_t get_module_base(const char* name) override;

    // Global 实例改全局模式，Private 实例只改自己
    void setMod(ReadMode mod);
    ReadMode getMod() const; // 本实例当前生效的模式

    // 显式操作全局模式（所有 Global 实例生效，Private 实例不受影响）
    static void setGlobalMod(ReadMode mod);
    static ReadMode getGlobalMod();

    // 驱动侧按进程名取 PID（不依赖本实例的目标 pid）
    pid_t get_pid_by_name(const char* name);
    // 驱动侧取模块 .bss 段基址
    uintptr_t get_module_bss(const char* name);

    // ---- 触摸（先 touch_init，模式 0/1；slot 由 touch_init 决定可用范围） ----
    bool touch_init(int touch_mode);
    bool touch_down(int slot, int x, int y);
    bool touch_up(int slot);

    // ---- 陀螺仪（先 gyro_init：0=tracepoint 1=uprobe） ----
    bool gyro_init(int method);
    bool gyro_modify(float x, float y);
    bool gyro_disable();

    // ---- 硬件断点/观察点 ----
    bool bp_check_inited();
    bool bp_init_driver();
    int  bp_get_num_brps();
    int  bp_get_num_wrps();

    // 安装断点，返回句柄（0 = 失败）；regs/batch 非空时命中瞬间自动改寄存器
    uint64_t bp_inst(pid_t target_pid, uint64_t addr, uint32_t bp_len,
                     uint32_t bp_type, uint64_t reg_modify_mask = 0,
                     uint64_t fp_reg_modify_mask = 0,
                     TwtUserPtRegs* regs_to_set = nullptr,
                     uint32_t flags = 0);
    uint64_t bp_inst(pid_t target_pid, uint64_t addr, uint32_t bp_len,
                     uint32_t bp_type, TwtRegBatch& batch,
                     uint32_t flags = 0);

    bool bp_uninst(uint64_t handle);
    bool bp_suspend(uint64_t handle);
    bool bp_resume(uint64_t handle);
    bool bp_get_hit_count(uint64_t handle, uint64_t* total_count,
                          uint64_t* item_arr_count);
    size_t bp_get_hit_items(uint64_t handle, TwtHitItem* buffer, size_t max_bytes);
    std::vector<TwtHitItem> bp_get_hits(uint64_t handle, size_t max_items = 100);

    // 命中时修改寄存器（mask 指定要下发的寄存器集合）
    bool bp_modify(uint64_t handle, uint64_t reg_modify_mask,
                   uint64_t fp_reg_modify_mask, TwtUserPtRegs* regs_to_set);
    bool bp_apply(uint64_t handle, TwtRegBatch& batch);

    bool bp_set_reg(uint64_t handle, int reg_idx, uint64_t value);
    bool bp_set_regs(uint64_t handle,
                     std::initializer_list<std::pair<int, uint64_t>> regs);
    bool bp_set_pc(uint64_t handle, uint64_t pc);
    bool bp_set_pstate(uint64_t handle, uint64_t pstate);
    bool bp_set_vreg_float(uint64_t handle, int vreg_idx, float value);
    bool bp_set_vreg_double(uint64_t handle, int vreg_idx, double value);
    bool bp_set_vregs_float(uint64_t handle,
                            std::initializer_list<std::pair<int, float>> vregs);
    bool bp_set_vregs_double(uint64_t handle,
                             std::initializer_list<std::pair<int, double>> vregs);

    // 读取命中记录里的寄存器快照
    static uint64_t get_xregs(const TwtHitItem& hit, int reg_idx);
    static float get_vregs_float(const TwtHitItem& hit, int vreg_idx);
    static double get_vregs_double(const TwtHitItem& hit, int vreg_idx);
    static uint64_t get_pc(const TwtHitItem& hit);
    static uint64_t get_pstate(const TwtHitItem& hit);

protected:
    int fd = -1;

    // 读模式的管理复用 pid 的 Global/Private 语义（同 TGodRW）：
    //   Global  —— 存静态 globalMod，setMod 对所有 Global 实例（跨线程）生效
    //   Private —— 存实例 localMod，多线程各用各的，互不干扰
    bool useGlobalMod = true;
    ReadMode localMod = ReadMode::MOD1;
    // 多线程切换读模式时保证可见性；relaxed 足够（只同步标志本身）
    inline static std::atomic<ReadMode> globalMod = ReadMode::MOD1;

private:
    // 在 /proc/self/fd 里找驱动下发的 anon_inode 句柄（MY_CALL 失败时的兜底）
    int getFd(const char* str);
};

// --- inline implementations ---

inline int TwTRW::getFd(const char* str) {
    DIR* dir;
    struct dirent* entry;
    char path[PATH_MAX], link[PATH_MAX];
    int found_fd = -1;

    dir = opendir("/proc/self/fd");
    if (!dir)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name);

        ssize_t len = readlink(path, link, sizeof(link) - 1);
        if (len == -1)
            continue;
        link[len] = '\0';

        if (strstr(link, str) != NULL && strstr(link, "anon_inode:") != NULL) {
            found_fd = atoi(entry->d_name);
            break;
        }
    }
    closedir(dir);

    return found_fd;
}

inline TwTRW::TwTRW(PidMode mode, int tpid, PidMode modMode, int gyro_mode, int touch_mode)
    : baseRW(mode, tpid), useGlobalMod(modMode == PidMode::Global) {
    fd = -1;
    TWT_CALL(0x114514, 0x1919810, 0x2778, &fd);
    if (fd < 0)
        fd = getFd("TwT_driver");
    if (fd < 0) {
        printf("\n[-] TwT 驱动未加载或无 root 权限\n");
        return;
    }
    connected = true;
    printf("\n[+] TwT 驱动连接成功。\n");

    // 陀螺仪与触摸各自独立初始化，任一失败不影响另一个和后续读写
    if (gyro_mode >= 0 && gyro_mode <= 1) {
        if (gyro_init(gyro_mode))
            printf("陀螺仪初始化成功 (模式 %d)\n", gyro_mode);
        else
            printf("陀螺仪初始化失败\n");
    }
    if (touch_mode >= 0 && touch_mode <= 1) {
        if (touch_init(touch_mode))
            printf("触摸初始化成功 (模式 %d)\n", touch_mode);
        else
            printf("触摸初始化失败\n");
    }
}

inline TwTRW::~TwTRW() {
    if (fd > 0)
        close(fd);
}

inline void TwTRW::setMod(ReadMode mod) {
    if (useGlobalMod)
        globalMod.store(mod, std::memory_order_relaxed);
    else
        localMod = mod;
}

inline TwTRW::ReadMode TwTRW::getMod() const {
    return useGlobalMod ? globalMod.load(std::memory_order_relaxed) : localMod;
}

inline void TwTRW::setGlobalMod(ReadMode mod) {
    globalMod.store(mod, std::memory_order_relaxed);
}

inline TwTRW::ReadMode TwTRW::getGlobalMod() {
    return globalMod.load(std::memory_order_relaxed);
}

inline bool TwTRW::readv(uintptr_t addr, void* buffer, size_t size) {
    if (!connected || getPid() <= 0)
        return false;
    TwtRequest req = {};
    req.pid = getPid();
    req.addr = addr & 0xFFFFFFFFFFFFULL;
    req.buffer = buffer;
    req.size = size;
    // MOD1 走 READ_MEM，MOD2 走 READ_MEM_V2
    ReadMode mod = useGlobalMod ? globalMod.load(std::memory_order_relaxed) : localMod;
    unsigned int cmd = (mod == ReadMode::MOD2) ? TWT_READ_MEM_V2
                                               : TWT_READ_MEM;
    return ioctl(fd, cmd, &req) == 0;
}

inline bool TwTRW::writev(uintptr_t addr, void* buffer, size_t size) {
    if (!connected || getPid() <= 0)
        return false;
    TwtRequest req = {};
    req.pid = getPid();
    req.addr = addr & 0xFFFFFFFFFFFFULL;
    req.buffer = buffer;
    req.size = size;
    return ioctl(fd, TWT_WRITE_MEM, &req) == 0;
}

inline uintptr_t TwTRW::get_module_base(const char* name) {
    if (!connected || getPid() <= 0 || !name)
        return 0;
    TwtRequest req = {};
    char buf[0x100];
    snprintf(buf, sizeof(buf), "%s", name);
    req.pid = getPid();
    req.addr = 0;
    req.buffer = buf;
    if (ioctl(fd, TWT_MODULE_BASE, &req) != 0)
        return 0;
    return req.addr;
}

inline uintptr_t TwTRW::get_module_bss(const char* name) {
    if (!connected || getPid() <= 0 || !name)
        return 0;
    TwtRequest req = {};
    char buf[0x100];
    snprintf(buf, sizeof(buf), "%s", name);
    req.pid = getPid();
    req.addr = 0;
    req.buffer = buf;
    if (ioctl(fd, TWT_MODULE_BSS, &req) != 0)
        return 0;
    return req.addr;
}

inline pid_t TwTRW::get_pid_by_name(const char* name) {
    if (!connected || !name)
        return -1;
    TwtRequest req = {};
    char buf[0x100];
    snprintf(buf, sizeof(buf), "%s", name);
    req.pid = 0;
    req.buffer = buf;
    if (ioctl(fd, TWT_GET_PID, &req) != 0)
        return -1;
    return req.pid;
}

inline bool TwTRW::touch_init(int touch_mode) {
    if (!connected)
        return false;
    if (touch_mode > 1 || touch_mode < 0) {
        printf("不支持的触摸模式\n");
        return false;
    }
    TwtTouchEvent teb = {};
    teb.slot = touch_mode;
    if (ioctl(fd, TWT_TOUCH_INIT, &teb) != 0) {
        if (errno == EALREADY) {
            printf("触摸已开启，当前模式: %d\n", teb.slot);
            return true;
        }
        return false;
    }
    return true;
}

inline bool TwTRW::touch_down(int slot, int x, int y) {
    if (!connected || slot < 0)
        return false;
    TwtTouchEvent teb = {};
    teb.slot = slot;
    teb.x = x;
    teb.y = y;
    return ioctl(fd, TWT_TOUCH_DOWN, &teb) == 0;
}

inline bool TwTRW::touch_up(int slot) {
    if (!connected || slot < 0)
        return false;
    TwtTouchEvent teb = {};
    teb.slot = slot;
    teb.x = 0;
    teb.y = 0;
    return ioctl(fd, TWT_TOUCH_UP, &teb) == 0;
}

inline bool TwTRW::gyro_init(int method) {
    if (!connected)
        return false;
    if (method < 0 || method > 1) {
        printf("不支持的陀螺仪模式\n");
        return false;
    }
    int current_method = method;
    if (ioctl(fd, TWT_GYRO_INIT, &current_method) != 0) {
        if (errno == EALREADY) {
            printf("陀螺仪已开启，当前模式: %d\n", current_method);
            return true;
        }
        return false;
    }
    return true;
}

inline bool TwTRW::gyro_modify(float x, float y) {
    if (!connected)
        return false;
    TwtGyroConfig config = {};
    config.enable = 1;
    memcpy(&config.x, &x, sizeof(uint32_t));
    memcpy(&config.y, &y, sizeof(uint32_t));
    return ioctl(fd, TWT_GYRO_CONFIG, &config) == 0;
}

inline bool TwTRW::gyro_disable() {
    if (!connected)
        return false;
    TwtGyroConfig config = {};
    config.enable = 0;
    config.x = 0;
    config.y = 0;
    return ioctl(fd, TWT_GYRO_CONFIG, &config) == 0;
}

inline bool TwTRW::bp_check_inited() {
    if (!connected)
        return false;
    return ioctl(fd, TWT_BP_CHECK_INITED, 0) == 0;
}

inline bool TwTRW::bp_init_driver() {
    if (!connected)
        return false;
    return ioctl(fd, TWT_BP_INIT_CMD, 0) == 0;
}

inline int TwTRW::bp_get_num_brps() {
    if (!connected)
        return -1;
    return ioctl(fd, TWT_BP_GET_NUM_BRPS, 0);
}

inline int TwTRW::bp_get_num_wrps() {
    if (!connected)
        return -1;
    return ioctl(fd, TWT_BP_GET_NUM_WRPS, 0);
}

inline uint64_t TwTRW::bp_inst(pid_t target_pid, uint64_t addr, uint32_t bp_len,
                               uint32_t bp_type, uint64_t reg_modify_mask,
                               uint64_t fp_reg_modify_mask,
                               TwtUserPtRegs* regs_to_set,
                               uint32_t flags) {
    if (!connected)
        return 0;
    TwtInstArgs args = {};
    args.pid = target_pid;
    args.addr = addr;
    args.bp_len = bp_len;
    args.bp_type = bp_type;
    args.reg_modify_mask = reg_modify_mask;
    args.fp_reg_modify_mask = fp_reg_modify_mask;
    args.regs_to_set_ptr = (uint64_t)regs_to_set;
    args.flags = flags;
    if (ioctl(fd, TWT_BP_INST, &args) != 0)
        return 0;
    return args.addr;
}

inline uint64_t TwTRW::bp_inst(pid_t target_pid, uint64_t addr, uint32_t bp_len,
                               uint32_t bp_type, TwtRegBatch& batch,
                               uint32_t flags) {
    return bp_inst(target_pid, addr, bp_len, bp_type,
                   batch.reg_mask, batch.fp_mask,
                   batch.empty() ? nullptr : &batch.regs, flags);
}

inline bool TwTRW::bp_uninst(uint64_t handle) {
    if (!connected || !handle)
        return false;
    return ioctl(fd, TWT_BP_UNINST, &handle) == 0;
}

inline bool TwTRW::bp_suspend(uint64_t handle) {
    if (!connected || !handle)
        return false;
    return ioctl(fd, TWT_BP_SUSPEND, &handle) == 0;
}

inline bool TwTRW::bp_resume(uint64_t handle) {
    if (!connected || !handle)
        return false;
    return ioctl(fd, TWT_BP_RESUME, &handle) == 0;
}

inline bool TwTRW::bp_get_hit_count(uint64_t handle, uint64_t* total_count,
                                    uint64_t* item_arr_count) {
    if (!connected || !handle)
        return false;
    TwtGetHitCountArg arg = {};
    arg.handle = handle;
    if (ioctl(fd, TWT_BP_GET_HIT_COUNT, &arg) != 0)
        return false;
    if (total_count)
        *total_count = arg.hit_total_count;
    if (item_arr_count)
        *item_arr_count = arg.hit_item_arr_count;
    return true;
}

inline bool TwTRW::bp_modify(uint64_t handle, uint64_t reg_modify_mask,
                             uint64_t fp_reg_modify_mask,
                             TwtUserPtRegs* regs_to_set) {
    if (!connected || !handle)
        return false;
    TwtModifyArgs args = {};
    args.handle = handle;
    args.reg_modify_mask = reg_modify_mask;
    args.fp_reg_modify_mask = fp_reg_modify_mask;
    args.regs_to_set_ptr = (uint64_t)regs_to_set;
    return ioctl(fd, TWT_BP_MODIFY, &args) == 0;
}

inline size_t TwTRW::bp_get_hit_items(uint64_t handle, TwtHitItem* buffer,
                                      size_t max_bytes) {
    if (!connected || !handle || !buffer || max_bytes == 0)
        return 0;
    TwtGetHitItemsArgs args = {};
    args.handle = handle;
    args.user_buffer_ptr = (uint64_t)buffer;
    args.max_bytes = max_bytes;
    args.items_copied = 0;
    if (ioctl(fd, TWT_BP_GET_HIT_ITEMS, &args) != 0)
        return 0;
    return (size_t)args.items_copied;
}

inline std::vector<TwtHitItem> TwTRW::bp_get_hits(uint64_t handle, size_t max_items) {
    std::vector<TwtHitItem> result;
    if (!connected || !handle || max_items == 0)
        return result;

    uint64_t total = 0, arr_count = 0;
    if (!bp_get_hit_count(handle, &total, &arr_count) || arr_count == 0)
        return result;

    size_t count = (arr_count < max_items) ? (size_t)arr_count : max_items;
    result.resize(count);
    size_t copied = bp_get_hit_items(handle, result.data(), count * sizeof(TwtHitItem));
    result.resize(copied);
    return result;
}

inline bool TwTRW::bp_apply(uint64_t handle, TwtRegBatch& batch) {
    if (batch.reg_mask == 0 && batch.fp_mask == 0)
        return false;
    return bp_modify(handle, batch.reg_mask, batch.fp_mask, &batch.regs);
}

inline bool TwTRW::bp_set_reg(uint64_t handle, int reg_idx, uint64_t value) {
    if (reg_idx < 0 || reg_idx > 30)
        return false;
    TwtUserPtRegs regs = {};
    regs.regs[reg_idx] = value;
    return bp_modify(handle, 1ULL << reg_idx, 0, &regs);
}

inline bool TwTRW::bp_set_regs(uint64_t handle,
                               std::initializer_list<std::pair<int, uint64_t>> regs) {
    TwtUserPtRegs pt_regs = {};
    uint64_t mask = 0;
    for (const auto& r : regs) {
        if (r.first < 0 || r.first > 30)
            continue;
        pt_regs.regs[r.first] = r.second;
        mask |= 1ULL << r.first;
    }
    if (mask == 0)
        return false;
    return bp_modify(handle, mask, 0, &pt_regs);
}

inline bool TwTRW::bp_set_pc(uint64_t handle, uint64_t pc) {
    TwtUserPtRegs regs = {};
    regs.pc = pc;
    return bp_modify(handle, TWT_REG_MODIFY_PC, 0, &regs);
}

inline bool TwTRW::bp_set_pstate(uint64_t handle, uint64_t pstate) {
    TwtUserPtRegs regs = {};
    regs.pstate = pstate;
    return bp_modify(handle, TWT_REG_MODIFY_PSTATE, 0, &regs);
}

inline bool TwTRW::bp_set_vreg_float(uint64_t handle, int vreg_idx, float value) {
    if (vreg_idx < 0 || vreg_idx > 31)
        return false;
    TwtUserPtRegs regs = {};
    __uint128_t v = 0;
    memcpy(&v, &value, sizeof(float));
    regs.vregs[vreg_idx] = v;
    return bp_modify(handle, 0, 1ULL << vreg_idx, &regs);
}

inline bool TwTRW::bp_set_vreg_double(uint64_t handle, int vreg_idx, double value) {
    if (vreg_idx < 0 || vreg_idx > 31)
        return false;
    TwtUserPtRegs regs = {};
    __uint128_t v = 0;
    memcpy(&v, &value, sizeof(double));
    regs.vregs[vreg_idx] = v;
    return bp_modify(handle, 0, 1ULL << vreg_idx, &regs);
}

inline bool TwTRW::bp_set_vregs_float(uint64_t handle,
                                      std::initializer_list<std::pair<int, float>> vregs) {
    TwtUserPtRegs pt_regs = {};
    uint64_t mask = 0;
    for (const auto& r : vregs) {
        if (r.first < 0 || r.first > 31)
            continue;
        __uint128_t v = 0;
        memcpy(&v, &r.second, sizeof(float));
        pt_regs.vregs[r.first] = v;
        mask |= 1ULL << r.first;
    }
    if (mask == 0)
        return false;
    return bp_modify(handle, 0, mask, &pt_regs);
}

inline bool TwTRW::bp_set_vregs_double(uint64_t handle,
                                       std::initializer_list<std::pair<int, double>> vregs) {
    TwtUserPtRegs pt_regs = {};
    uint64_t mask = 0;
    for (const auto& r : vregs) {
        if (r.first < 0 || r.first > 31)
            continue;
        __uint128_t v = 0;
        memcpy(&v, &r.second, sizeof(double));
        pt_regs.vregs[r.first] = v;
        mask |= 1ULL << r.first;
    }
    if (mask == 0)
        return false;
    return bp_modify(handle, 0, mask, &pt_regs);
}

inline uint64_t TwTRW::get_xregs(const TwtHitItem& hit, int reg_idx) {
    if (reg_idx < 0 || reg_idx > 30)
        return 0;
    return hit.regs_info.regs[reg_idx];
}

inline float TwTRW::get_vregs_float(const TwtHitItem& hit, int vreg_idx) {
    float val = 0;
    if (vreg_idx < 0 || vreg_idx > 31)
        return val;
    memcpy(&val, &hit.regs_info.vregs[vreg_idx], sizeof(float));
    return val;
}

inline double TwTRW::get_vregs_double(const TwtHitItem& hit, int vreg_idx) {
    double val = 0;
    if (vreg_idx < 0 || vreg_idx > 31)
        return val;
    memcpy(&val, &hit.regs_info.vregs[vreg_idx], sizeof(double));
    return val;
}

inline uint64_t TwTRW::get_pc(const TwtHitItem& hit) {
    return hit.regs_info.pc;
}

inline uint64_t TwTRW::get_pstate(const TwtHitItem& hit) {
    return hit.regs_info.pstate;
}

} // namespace diRW

#endif
