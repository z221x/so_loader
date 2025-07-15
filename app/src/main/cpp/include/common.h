#pragma once
#include <iostream>
#include <fstream>
#include <android/log.h>
#include <jni.h>
#include <elf.h>
#include <link.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "linked.h"
#define LOGE(LogTag, ...) __android_log_print(ANDROID_LOG_ERROR, LogTag, __VA_ARGS__)
#define LOGI(LogTag, ...) __android_log_print(ANDROID_LOG_INFO, LogTag, __VA_ARGS__)
#define LOADLIBIARY 1
#define LOAD 2
#define MAYBE_MAP_FLAG(x, from, to)  (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))
#define powerof2(x)                                               \
  ({                                                              \
    __typeof__(x) _x = (x);                                       \
    __typeof__(x) _x2;                                            \
    __builtin_add_overflow(_x, -1, &_x2) ? 1 : ((_x2 & _x) == 0); \
  })
#define FLAG_LINKED           0x00000001
#define FLAG_EXE              0x00000004 // The main executable
#define FLAG_LINKER           0x00000010 // The linker itself
#define FLAG_GNU_HASH         0x00000040 // uses gnu hash
#define FLAG_MAPPED_BY_CALLER 0x00000080 // the map is reserved by the caller
#define FLAG_IMAGE_LINKED     0x00000100 // Is image linked - this is a guard on link_image..
#define FLAG_RESERVED         0x00000200 // This flag was set when there is at least one
#define FLAG_PRELINKED        0x00000400 // prelink_image has successfully processed this soinfo
#define FLAG_GLOBALS_TAGGED   0x00000800 // globals have been tagged by MTE.
#define FLAG_NEW_SOINFO       0x40000000 // new soinfo format
#define SUPPORTED_DT_FLAGS_1 (DF_1_NOW | DF_1_GLOBAL | DF_1_NODELETE | DF_1_PIE | DF_1_ORIGIN)
#if defined(__LP64__)
#define USE_RELA 1
#endif
enum class RelocMode {
    // Fast path for JUMP_SLOT relocations.
    JumpTable,
    // Fast path for typical relocations: ABSOLUTE, GLOB_DAT, or RELATIVE.
    Typical,
    // Handle all relocation types, relocations in text sections, and statistics/tracing.
    General,
};
class ElfReader;
class SoLoader;

extern JavaVM*  javaVm_;
extern JNIEnv* env_;
extern jobject application;
extern uint64_t jni_onloadF;
extern Elf64_Xword need[100];
extern uint32_t needed_count;

inline size_t page_size() {
#if defined(PAGE_SIZE)
    return PAGE_SIZE;
#else
    static const size_t page_size = getauxval(AT_PAGESZ);
  return page_size;
#endif
}
inline uintptr_t page_start(uintptr_t x) {
    return x & ~(page_size() - 1);
}
inline uintptr_t page_offset(uintptr_t x) {
    return x & (page_size() - 1);
}
inline uintptr_t page_end(uintptr_t x) {
    return page_start(x + page_size() - 1);
}
uint64_t GetFuncFromSymbol(std::string libname, std::string funcname);
uint64_t getlibbase(std::string libname);
