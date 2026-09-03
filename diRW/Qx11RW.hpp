#ifndef DIRW_QX11RW_H
#define DIRW_QX11RW_H

#include "baseRW.hpp"
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dirent.h>

namespace diRW {

struct Qx11CopyMemory {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
};

struct Qx11ModuleBase {
    pid_t pid;
    const char* name;
    uintptr_t base;
};

enum Qx11Operations {
    QX11_OP_INIT_KEY = 0x800,
    QX11_OP_READ_MEM = 0x801,
    QX11_OP_WRITE_MEM = 0x802,
    QX11_OP_MODULE_BASE = 0x803,
};

class Qx11RW : public baseRW {
protected:
    int fd = -1;
    const char* DEVICE_PATH;

public:
    char* execCom(const char* shell);
    int findFirstMatchingPath(const char *path, regex_t *regex, char *result);
    void createDriverNode(const char *path, int major_number, int minor_number);
    void removeDeviceNode(const char *path);
    int get_dev();
    Qx11RW(PidMode mode, int tpid = 0);
    ~Qx11RW() override;
    uintptr_t get_module_base(const char* name) override;
    bool readv(uintptr_t addr, void* buffer, size_t size) override;
    bool writev(uintptr_t addr, void* buffer, size_t size) override;
};

// --- inline implementations ---

inline char* Qx11RW::execCom(const char* shell) {
    FILE* fp = popen(shell, "r");
    if (fp == nullptr) {
        perror("popen failed\n");
        return nullptr;
    }
    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        result += buffer;
    }
    pclose(fp);
    return strdup(result.c_str());
}

inline int Qx11RW::findFirstMatchingPath(const char* path, regex_t* regex, char* result) {
    DIR* dir = opendir(path);
    if (dir == nullptr) {
        perror("无法打开目录");
        return 0;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (entry->d_type == DT_LNK) {
            char linkpath[1024];
            ssize_t len = readlink(fullpath, linkpath, sizeof(linkpath) - 1);
            if (len != -1) {
                linkpath[len] = '\0';
                if (regexec(regex, linkpath, 0, nullptr, 0) == 0) {
                    strcpy(result, fullpath);
                    closedir(dir);
                    return 1;
                }
            } else {
                perror("read link\n");
            }
        }
    }
    closedir(dir);
    return 0;
}

inline void Qx11RW::createDriverNode(const char* path, int major_number, int minor_number) {
    std::string command = "mknod " + std::string(path) + " c " + std::to_string(major_number) + " " + std::to_string(minor_number);
    system(command.c_str());
}

inline void Qx11RW::removeDeviceNode(const char* path) {
    if (unlink(path) == 0) {
        printf("[-] 驱动安全守护已激活\n");
    } else {
        perror("[-] 驱动安全守护执行错误\n");
    }
}

inline int Qx11RW::get_dev() {
    char* output = execCom("ls -l /proc/*/exe 2>/dev/null | grep -E \"/data/[a-z]{6} \\(deleted\\)\"");
    if (output == nullptr) {
        printf("Error executing scripts.\n");
        return -1;
    }
    char filePath[256] = {0};
    char pid[56] = {0};
    char* procStart = strstr(output, "/proc/");
    if (procStart) {
        char* pidStart = procStart + 6;
        char* pidEnd = strchr(pidStart, '/');
        strncpy(pid, pidStart, pidEnd - pidStart);
        pid[pidEnd - pidStart] = '\0';
        char* arrowStart = strstr(output, "->");
        char* start = arrowStart + 3;
        char* end = strchr(output, '(') - 1;
        strncpy(filePath, start, end - start + 1);
        filePath[end - start] = '\0';
        char* replacePtr = strstr(filePath, "data");
        if (replacePtr != nullptr) {
            memmove(replacePtr + 2, replacePtr + 3, strlen(replacePtr + 3) + 1);
            memcpy(replacePtr, "dev", strlen("dev"));
        }
    }
    free(output);
    char fdPath[256] = {0};
    char pattern[100] = {0};
    snprintf(pattern, sizeof(pattern), ".*%s.*", filePath + 5);
    int major_number = 0;
    int minor_number = 0;
    snprintf(fdPath, sizeof(fdPath), "/proc/%s/fd", pid);
    regex_t regex;
    if (regcomp(&regex, pattern, 0) != 0) {
        fprintf(stderr, "Failed to compile regex\n");
        return -1;
    }
    char result[1024] = {0};
    if (findFirstMatchingPath(fdPath, &regex, result)) {
        char cmd[256];
        sprintf(cmd, "ls -AL -l  %s | grep -Eo '[0-9]{3},' | grep  -Eo '[0-9]{3}'", result);
        char* fdInfo = execCom(cmd);
        if (fdInfo) {
            fdInfo[strlen(fdInfo) - 1] = '\0';
            major_number = atoi(fdInfo);
            free(fdInfo);
        }
    } else {
        printf("找不到匹配的路径\n");
    }
    regfree(&regex);
    if (strcmp(filePath, "\0") != 0) {
        createDriverNode(filePath, major_number, 0);
        fd = open(filePath, O_WRONLY);
        if (fd == -1) {
            printf("\n[-] 驱动链接失败\n");
            removeDeviceNode(filePath);
            return -1;
        } else {
            connected = true;
            printf("\n[-] 驱动已经启动\n");
            removeDeviceNode(filePath);
            return 1;
        }
    }
    return -1;
}

inline Qx11RW::Qx11RW(PidMode mode, int tpid) : baseRW(mode, tpid) {
    get_dev();
}

inline Qx11RW::~Qx11RW() {
    if (fd > 0)
        close(fd);
}

inline uintptr_t Qx11RW::get_module_base(const char* name) {
    Qx11ModuleBase mb;
    mb.pid = getPid();
    mb.name = name;
    if (ioctl(fd, QX11_OP_MODULE_BASE, &mb) != 0)
        return 0;
    return mb.base;
}

inline bool Qx11RW::readv(uintptr_t addr, void* buffer, size_t size) {
    Qx11CopyMemory cm { getPid(), addr, buffer, size };
    return ioctl(fd, QX11_OP_READ_MEM, &cm) == 0;
}

inline bool Qx11RW::writev(uintptr_t addr, void* buffer, size_t size) {
    Qx11CopyMemory cm { getPid(), addr, buffer, size };
    return ioctl(fd, QX11_OP_WRITE_MEM, &cm) == 0;
}

} // namespace diRW

#endif
