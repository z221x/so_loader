#pragma once
#include "common.h"
typedef void (*linker_dtor_function_t)();
typedef void (*linker_ctor_function_t)(int, char**, char**);
struct soinfo;
class plain_reloc_iterator {
    typedef ElfW(Rela) rel_t;
public:
    plain_reloc_iterator(rel_t* rel_array, size_t count)
            : begin_(rel_array), end_(begin_ + count), current_(begin_) {}

    bool has_next() {
        return current_ < end_;
    }

    rel_t* next() {
        return current_++;
    }
private:
    rel_t* const begin_;
    rel_t* const end_;
    rel_t* current_;

};
class SoinfoListAllocator {
public:
    static LinkedListEntry<soinfo>* alloc();
    static void free(LinkedListEntry<soinfo>* entry);

private:
    // unconstructable
    DISALLOW_IMPLICIT_CONSTRUCTORS(SoinfoListAllocator);
};
typedef LinkedList<soinfo, SoinfoListAllocator> soinfo_list_t;
struct soinfo {
public:
    const ElfW(Phdr) *phdr;
    size_t phnum;
    ElfW(Addr) base;
    size_t size;
    ElfW(Dyn) *dynamic;
    soinfo *next;
public:
    uint32_t flags_;
    const char *strtab_;
    ElfW(Sym) *symtab_;
    size_t nbucket_;
    size_t nchain_;
    uint32_t *bucket_;
    uint32_t *chain_;
    ElfW(Rela)* plt_rela_;
  size_t plt_rela_count_;
  ElfW(Rela)* rela_;
  size_t rela_count_;

    linker_ctor_function_t *preinit_array_;
    size_t preinit_array_count_;

    linker_ctor_function_t *init_array_;
    size_t init_array_count_;
    linker_dtor_function_t *fini_array_;
    size_t fini_array_count_;

    linker_ctor_function_t init_func_;
    linker_dtor_function_t fini_func_;
    size_t ref_count_;
public:
    link_map link_map_head;
    bool constructors_called;
    // When you read a virtual address from the ELF file, add this
    // value to get the corresponding address in the process' address space.
    ElfW(Addr) load_bias;
    bool has_DT_SYMBOLIC;
    template <typename ElfRelIteratorT> bool plain_relocate(ElfRelIteratorT&& rel_iterator);

    void phdr_table_get_dynamic_section(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                                ElfW(Addr) load_bias, ElfW(Dyn)** dynamic,
                                                ElfW(Word)* dynamic_flags);
    void set_soname(const char *soname){
        soname_=soname;
    }
    void call_JNI_Onload();
    void call_constructors();
    bool prelink_image(bool deterministic_memtag_globals = false);
    bool link_image();
    ElfW(Addr) resolve_symbol_address(const ElfW(Sym)* s) const;
    ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr) const;
    bool is_gnu_hash() const;
public:
    // This part of the structure is only available
    // when FLAG_NEW_SOINFO is set in this->flags.
    uint32_t version_;
    // version >= 0
    dev_t st_dev_;
    ino_t st_ino_;
    // dependency graph
    soinfo_list_t children_;
    soinfo_list_t parents_;

    // version >= 1
    off64_t file_offset_;
    uint32_t rtld_flags_;
    uint32_t dt_flags_1_;
    size_t strtab_size_;

    // version >= 2
    size_t gnu_nbucket_;
    uint32_t *gnu_bucket_;
    uint32_t *gnu_chain_;
    uint32_t gnu_maskwords_;
    uint32_t gnu_shift2_;
    ElfW(Addr) *gnu_bloom_filter_;
    soinfo *local_group_root_;
    uint8_t *android_relocs_;
    size_t android_relocs_size_;
    std::string soname_;
    std::string realpath_;
    const ElfW(Versym) *versym_;
    ElfW(Addr) verdef_ptr_;
    size_t verdef_cnt_;
    ElfW(Addr) verneed_ptr_;
    size_t verneed_cnt_;
    int target_sdk_version_;

    // version >= 3
    std::vector<std::string> dt_runpath_;
    void *primary_namespace_;
    void *secondary_namespaces_;
    uintptr_t handle_;


    const char* get_string(ElfW(Word) index) const;
    void set_dt_flags_1(uint32_t dt_flags_1);


};
