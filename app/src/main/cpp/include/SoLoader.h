#pragma once
#include "common.h"
#include "Soinfo.h"

class SoLoader {
public:
    SoLoader();

    ~SoLoader();

    void loadlibiary(const char *path);

    void load(const char *path);
private:
    ElfReader *reader;

    void nativeload(const char *path, int flag);
};
