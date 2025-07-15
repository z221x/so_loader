#pragma once
#include "common.h"
#include "Soinfo.h"

class ElfReader {
public:
    ElfReader();

    ~ElfReader();

    bool Open(const char *path, int flag);

    bool Read();

    soinfo * Load();


private:
    bool OpenLibFromZip(const char *path);

    bool ReadElfHeader();

    bool ReadProgramHeaders();

    bool ReserveAddressSpace();

    bool LoadSegments();

    void ZeroFillSegment(const ElfW(Phdr) *phdr);

    bool MapBssSection(const ElfW(Phdr) *phdr, ElfW(Addr) seg_page_end, ElfW(Addr) seg_file_end);

    bool FindPhdr();

    bool CheckPhdr(ElfW(Addr) loaded);

    soinfo * GetSoinfo(std::string libname);


    void *start_addr_ = nullptr;
    bool did_read_;
    bool did_load_;
    std::string name_;
    int fd_;
    off64_t file_offset_;
    off64_t file_size_;
    ElfW(Ehdr) header_;
    size_t phdr_num_;
    //MappedFileFragment phdr_fragment_;
    ElfW(Phdr) *phdr_table_;
    //MappedFileFragment shdr_fragment_;
    ElfW(Shdr) *shdr_table_;
    size_t shdr_num_;
    //MappedFileFragment dynamic_fragment_;
    ElfW(Dyn) *dynamic_;
    //MappedFileFragment strtab_fragment_;
    const char *strtab_;
    size_t strtab_size_;
    // First page of reserved address space.
    void *load_start_;
    // Size in bytes of reserved address space.
    size_t load_size_;
    // First page of inaccessible gap mapping reserved for this DSO.
    void *gap_start_;
    // Size in bytes of the gap mapping.
    size_t gap_size_;
    // Load bias.
    ElfW(Addr) load_bias_;
    // Maximum and minimum alignment requirements across all phdrs.
    size_t max_align_;
    size_t min_align_;
    // Loaded phdr.
    const ElfW(Phdr) *loaded_phdr_;
    // Is map owned by the caller
    bool mapped_by_caller_;
    // Pad gaps between segments when memory mapping?
    bool should_pad_segments_ = false;
    // Use app compat mode when loading 4KiB max-page-size ELFs on 16KiB page-size devices?
    bool should_use_16kib_app_compat_ = false;
    // RELRO region for 16KiB compat loading
    ElfW(Addr) compat_relro_start_ = 0;
    ElfW(Addr) compat_relro_size_ = 0;
    soinfo * si_;
};
