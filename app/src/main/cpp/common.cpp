#include "common.h"

uint64_t getlibbase(std::string libname) {
    std::fstream mapsfile("/proc/self/maps", std::ios::in);
    std::string line;
    uint64_t start_addr = 0;
    //从maps文件中获取到lib地址
    if (mapsfile.is_open()) {
        while (std::getline(mapsfile, line)) {
            if (line.find(libname) != -1) {
                unsigned long long start, end;
                if (sscanf(line.c_str(), "%llx-%llx", &start, &end) == 2) {
                    start_addr = start;
                    mapsfile.close();
                    return start_addr;
                }
            }
        }
    } else {
        LOGE("ElfReader", "maps open failed");
    }
    return 0;
}

uint64_t GetFuncFromSymbol(std::string libpath, std::string funcname) {
    std::string libname = libpath;
    uint64_t libbase = 0;
    struct stat file_data;
    int fd;
    if (libpath.find("/") != -1) {
        libname = libpath.substr(libpath.find_last_of("/") + 1);
    }
    libbase = getlibbase(libname);
    fd = open(libpath.c_str(), O_RDONLY);
    fstat(fd, &file_data);
    void *base = mmap(NULL, file_data.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE , fd, 0);
    //获取lib的文件头
    ElfW(Ehdr) header;
    memcpy(&(header), base, sizeof(header));
    size_t size = header.e_shnum * sizeof(ElfW(Shdr));
    void *tmp = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //映射节表
    ElfW(Shdr) *shdr_table;
    memcpy(tmp, (void *) ((ElfW(Off)) base + header.e_shoff), size);
    shdr_table = static_cast<ElfW(Shdr) *>(tmp);
    //获取节区名
    char *shstrtab = reinterpret_cast<char *>(shdr_table[header.e_shstrndx].sh_offset + (ElfW(Off)) base);
    void *symtab = nullptr;
    char *strtab = nullptr;
    uint32_t symtab_size = 0;
    //获取symtab/strtab地址
    for (size_t i = 0; i < header.e_shnum; ++i) {
        const ElfW(Shdr) *shdr = &shdr_table[i];
        std::string section_name(shstrtab + shdr->sh_name);
        if (section_name == ".symtab") {
            symtab = reinterpret_cast<void *>(shdr->sh_offset + (ElfW(Off)) base);
            symtab_size = shdr->sh_size;
        }
        if (section_name == ".strtab") {
            strtab = reinterpret_cast<char *>(shdr->sh_offset + (ElfW(Off)) base);
        }
        if (strtab && symtab)break;
    }
    //映射Symbol table
    ElfW(Sym) *sym_table;
    tmp = mmap(nullptr, symtab_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
               0);
    memcpy(tmp, symtab, symtab_size);
    sym_table = static_cast<ElfW(Sym) *>(tmp);
    int sym_num = symtab_size / sizeof(ElfW(Sym));
    // 遍歷 Symbol table
    for (int i = 0; i < sym_num; i++) {
        const ElfW(Sym) *sym = &sym_table[i];
        std::string sym_name(strtab + sym->st_name);
        if (sym_name.find(funcname) != -1) {
            uint64_t result = libbase + sym->st_value;
            munmap(base,file_data.st_size);
            return result;
        }

    }
    return 0;
}
