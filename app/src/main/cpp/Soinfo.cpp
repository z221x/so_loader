#include "Soinfo.h"
uint64_t jni_onloadF=0;
Elf64_Xword need[100]={0};
uint32_t needed_count=0;
void soinfo::call_constructors() {
    if(init_func_) {
        init_func_(0, nullptr, nullptr);
    }
    if(init_array_) {
        for(int i = 0; i < init_array_count_; i++) {
            if(!init_array_[i])continue;
            init_array_[i](0, nullptr, nullptr);
        }
    }
}
bool soinfo::prelink_image(bool deterministic_memtag_globals)  {
    ElfW(Word) dynamic_flags = 0;
    phdr_table_get_dynamic_section(phdr, phnum, load_bias, &dynamic, &dynamic_flags);
    if (dynamic == nullptr) {
        return false;
    }
    for (ElfW(Dyn) *d = dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SONAME:
                // this is parsed after we have strtab initialized (see below).
                break;
            case DT_HASH:
                nbucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[0];
                nchain_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[1];
                bucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr + 8);
                chain_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr + 8 + nbucket_ * 4);
                break;
            case DT_GNU_HASH:
                gnu_nbucket_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[0];
                // skip symndx
                gnu_maskwords_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[2];
                gnu_shift2_ = reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[3];

                gnu_bloom_filter_ = reinterpret_cast<ElfW(Addr) *>(load_bias + d->d_un.d_ptr + 16);
                gnu_bucket_ = reinterpret_cast<uint32_t *>(gnu_bloom_filter_ + gnu_maskwords_);
                // amend chain for symndx = header[1]
                gnu_chain_ = gnu_bucket_ + gnu_nbucket_ -
                             reinterpret_cast<uint32_t *>(load_bias + d->d_un.d_ptr)[1];

                if (!powerof2(gnu_maskwords_)) {
                    return false;
                }
                --gnu_maskwords_;
                flags_ |= FLAG_GNU_HASH;
                break;

            case DT_STRTAB:
                strtab_ = reinterpret_cast<const char *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_STRSZ:
                strtab_size_ = d->d_un.d_val;
                break;

            case DT_SYMTAB:
                symtab_ = reinterpret_cast<ElfW(Sym) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_SYMENT:
                if (d->d_un.d_val != sizeof(ElfW(Sym))) {
                    return false;
                }
                break;

            case DT_PLTREL:
                if (d->d_un.d_val != DT_RELA) {
                    return false;
                }
                break;

            case DT_JMPREL:
                plt_rela_ = reinterpret_cast<ElfW(Rela) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_PLTRELSZ:
                plt_rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
                break;

            case DT_PLTGOT:
                // Ignored (because RTLD_LAZY is not supported).
                break;

            case DT_DEBUG:
                // Set the DT_DEBUG entry to the address of _r_debug for GDB
                // if the dynamic table is writable
                if ((dynamic_flags & PF_W) != 0) {
                }
                break;
            case DT_RELA:
                rela_ = reinterpret_cast<ElfW(Rela) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_RELASZ:
                rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
                break;

            case DT_ANDROID_RELA:
                android_relocs_ = reinterpret_cast<uint8_t *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_ANDROID_RELASZ:
                android_relocs_size_ = d->d_un.d_val;
                break;

            case DT_ANDROID_REL:
                return false;

            case DT_ANDROID_RELSZ:
                return false;

            case DT_RELAENT:
                if (d->d_un.d_val != sizeof(ElfW(Rela))) {
                    return false;
                }
                break;

                // Ignored (see DT_RELCOUNT comments for details).
            case DT_RELACOUNT:
                break;

            case DT_REL:
                return false;

            case DT_RELSZ:
                return false;
            case DT_INIT:
                init_func_ = reinterpret_cast<linker_ctor_function_t>(load_bias + d->d_un.d_ptr);
                break;

            case DT_FINI:
                fini_func_ = reinterpret_cast<linker_dtor_function_t>(load_bias + d->d_un.d_ptr);
                break;

            case DT_INIT_ARRAY:
                init_array_ = reinterpret_cast<linker_ctor_function_t *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_INIT_ARRAYSZ:
                init_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_FINI_ARRAY:
                fini_array_ = reinterpret_cast<linker_dtor_function_t *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_FINI_ARRAYSZ:
                fini_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_PREINIT_ARRAY:
                preinit_array_ = reinterpret_cast<linker_ctor_function_t *>(load_bias +
                                                                            d->d_un.d_ptr);
                break;

            case DT_PREINIT_ARRAYSZ:
                preinit_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
                break;

            case DT_TEXTREL:
                return false;
            case DT_SYMBOLIC:
                has_DT_SYMBOLIC = true;
                break;

            case DT_NEEDED:
                need[needed_count] = d->d_un.d_val;
                ++needed_count;
                break;

            case DT_FLAGS:
                if (d->d_un.d_val & DF_TEXTREL) {
                    return false;
                }
                if (d->d_un.d_val & DF_SYMBOLIC) {
                    has_DT_SYMBOLIC = true;
                }
                break;

            case DT_FLAGS_1:
                set_dt_flags_1(d->d_un.d_val);

                if ((d->d_un.d_val & ~SUPPORTED_DT_FLAGS_1) != 0) {
                }
                break;

                // Ignored: "Its use has been superseded by the DF_BIND_NOW flag"
            case DT_BIND_NOW:
                break;

            case DT_VERSYM:
                versym_ = reinterpret_cast<ElfW(Versym) *>(load_bias + d->d_un.d_ptr);
                break;

            case DT_VERDEF:
                verdef_ptr_ = load_bias + d->d_un.d_ptr;
                break;
            case DT_VERDEFNUM:
                verdef_cnt_ = d->d_un.d_val;
                break;

            case DT_VERNEED:
                verneed_ptr_ = load_bias + d->d_un.d_ptr;
                break;

            case DT_VERNEEDNUM:
                verneed_cnt_ = d->d_un.d_val;
                break;

            case DT_RUNPATH:
                // this is parsed after we have strtab initialized (see below).
                break;

            case DT_TLSDESC_GOT:
            case DT_TLSDESC_PLT:
                // These DT entries are used for lazy TLSDESC relocations. Bionic
                // resolves everything eagerly, so these can be ignored.
                break;
            default:
                const char *tag_name;
                if (d->d_tag == DT_RPATH) {
                    tag_name = "DT_RPATH";
                } else if (d->d_tag == DT_ENCODING) {
                    tag_name = "DT_ENCODING";
                } else if (d->d_tag >= DT_LOOS && d->d_tag <= DT_HIOS) {
                    tag_name = "unknown OS-specific";
                } else if (d->d_tag >= DT_LOPROC && d->d_tag <= DT_HIPROC) {
                    tag_name = "unknown processor-specific";
                } else {
                    tag_name = "unknown";
                }

                break;
        }
    }
    // Validity checks.
    if (nbucket_ == 0 && gnu_nbucket_ == 0) {
        return false;
    }
    if (strtab_ == nullptr) {
        return false;
    }
    if (symtab_ == nullptr) {
        return false;
    }
    // Second pass - parse entries relying on strtab. Skip this while relocating the linker so as to
    // avoid doing heap allocations until later in the linker's initialization.
    for (ElfW(Dyn) *d = dynamic; d->d_tag != DT_NULL; ++d) {
        switch (d->d_tag) {
            case DT_SONAME:
                set_soname(get_string(d->d_un.d_val));
                break;
            case DT_RUNPATH:
                break;
        }
    }
    flags_ |= FLAG_PRELINKED;
    return true;
}

bool soinfo::link_image() {
    if (rela_ != nullptr) {
        if (!plain_relocate(plain_reloc_iterator(rela_, rela_count_))) {
            return false;
        }
    }
    if (plt_rela_ != nullptr) {
        if (!plain_relocate(plain_reloc_iterator(plt_rela_, plt_rela_count_))) {
            return false;
        }
    }
    return true;
}

bool is_symbol_global_and_defined(const soinfo *si, const ElfW(Sym) *s) {
    if (__predict_true(ELF_ST_BIND(s->st_info) == STB_GLOBAL ||
                       ELF_ST_BIND(s->st_info) == STB_WEAK)) {
        return s->st_shndx != SHN_UNDEF;
    } else if (__predict_false(ELF_ST_BIND(s->st_info) != STB_LOCAL)) {

    }
    return false;
}

uint32_t gnu_hash(const char *name) {
    uint32_t h = 5381;
    const uint8_t *name_bytes = reinterpret_cast<const uint8_t *>(name);
#pragma unroll 8
    while (*name_bytes != 0) {
        h += (h << 5) + *name_bytes++; // h*33 + c = h + h * 32 + c = h + h << 5 + c
    }
    return h;
}

uint32_t elf_hash(const char *name) {
    const uint8_t *name_bytes = reinterpret_cast<const uint8_t *>(name);
    uint32_t h = 0, g;
    while (*name_bytes) {
        h = (h << 4) + *name_bytes++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }
    return h;
}

ElfW(Sym) *gnu_lookup(soinfo *si, const char *name) {
    const uint32_t hash = gnu_hash(name);
    constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
    const uint32_t word_num = (hash / kBloomMaskBits) & si->gnu_maskwords_;
    const ElfW(Addr) bloom_word = si->gnu_bloom_filter_[word_num];
    const uint32_t h1 = hash % kBloomMaskBits;
    const uint32_t h2 = (hash >> si->gnu_shift2_) % kBloomMaskBits;
    // test against bloom filter
    if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) == 0) {
        return nullptr;
    }
    // bloom test says "probably yes"...
    uint32_t n = si->gnu_bucket_[hash % si->gnu_nbucket_];

    if (n == 0) {
        return nullptr;
    }

    do {
        ElfW(Sym) *s = si->symtab_ + n;
        if (((si->gnu_chain_[n] ^ hash) >> 1) == 0 &&
            strcmp(si->get_string(s->st_name), name) == 0 &&
            is_symbol_global_and_defined(si, s)) {
            return si->symtab_ + n;
        }
    } while ((si->gnu_chain_[n++] & 1) == 0);

    return nullptr;
}

ElfW(Sym) *elf_lookup(soinfo *si, const char *name) {
    uint32_t hash = elf_hash(name);
    for (uint32_t n = si->bucket_[hash % si->nbucket_]; n != 0; n = si->chain_[n]) {
        ElfW(Sym) *s = si->symtab_ + n;

        if (strcmp(si->get_string(s->st_name), name) == 0 &&
            is_symbol_global_and_defined(si, s)) {
            return si->symtab_ + n;
        }
    }
    return nullptr;
}

ElfW(Sym) *find_symbol_by_name(soinfo *si, const char *name) {
    return si->is_gnu_hash() ? gnu_lookup(si, name) : elf_lookup(si, name);
}

ElfW(Sym) *soinfo_do_lookup_impl(const char *name, soinfo **si_found_in) {
    typedef soinfo *(*FunctionPtr2)();
    soinfo *so_head;
    ElfW(Sym) *result;
    FunctionPtr2 solist_get_head = (FunctionPtr2) GetFuncFromSymbol("/system/bin/linker64",
                                                                    "solist_get_head");
    if (!solist_get_head) {
        LOGE("Soinfo", "get_head_off nullptr");
        return nullptr;
    }
    so_head = solist_get_head();
    while (so_head) {
        result = find_symbol_by_name(so_head, name);
        if (result) {
            *si_found_in = so_head;
            return result;
        }
        so_head = so_head->next;
    }

    return nullptr;
}

template<typename ElfRelIteratorT>
bool soinfo::plain_relocate(ElfRelIteratorT &&rel_iterator) {
    for (size_t idx = 0; rel_iterator.has_next(); ++idx) {
        const auto rel = rel_iterator.next();
        if (rel == nullptr) {
            return false;
        }
        const uint32_t r_type = (rel->r_info & 0xffffffff);
        const uint32_t r_sym = (rel->r_info >> 32);
        const ElfW(Sym) *sym = nullptr;
        soinfo *found_in = nullptr;
        ElfW(Addr) rel_target = static_cast<ElfW(Addr)>(rel->r_offset + load_bias);
        const char *sym_name = nullptr;
        ElfW(Addr) sym_addr = 0;
        ElfW(Addr) addend = rel->r_addend;
        if (r_sym != 0) {
            sym_name = get_string(symtab_[r_sym].st_name);
        }
        if (r_sym == 0) {
            // Do nothing.
        }else {
            sym = soinfo_do_lookup_impl(sym_name, &found_in);
            if (sym != nullptr) {
                sym_addr = found_in->resolve_symbol_address(sym);
            } else {
                for (int s = 0; s < needed_count; s++) {
                    char *needname= const_cast<char *>(get_string(need[s]));
                    void *handle = dlopen(needname, RTLD_NOW);
                    sym_addr = reinterpret_cast<Elf64_Addr>(dlsym(handle, sym_name));
                    if (sym_addr) {
                        break;
                    }
                }
            }
            if (!sym_addr) {
                if (symtab_[r_sym].st_value != 0) {
                    sym_addr = load_bias + symtab_[r_sym].st_value;
                } else {
                    LOGE("Soinfo", "%s find addr fail (sym: %lx)", sym_name, (uint64_t) sym);
                    continue;
                }
            }
        }
        switch (r_type) {
            case R_GENERIC_JUMP_SLOT:
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = (sym_addr + addend);
                break;
            case R_GENERIC_GLOB_DAT:
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = (sym_addr + addend);
                break;
            case R_GENERIC_RELATIVE:
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = (load_bias + addend);
                break;
            case R_GENERIC_IRELATIVE:
            {

                ElfW(Addr) ifunc_addr = call_ifunc_resolver(load_bias + addend);
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = ifunc_addr;
            }
                break;
            case R_AARCH64_ABS64:
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = sym_addr + addend;
                break;
            case R_AARCH64_ABS32:
            {
                const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT32_MIN);
                const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT32_MAX);
                if ((min_value <= (sym_addr + addend)) && ((sym_addr + addend) <= max_value)) {
                    *reinterpret_cast<ElfW(Addr)*>(rel_target) = sym_addr + addend;
                } else {
                    return false;
                }
            }
                break;
            case R_AARCH64_ABS16:
            {
                const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT16_MIN);
                const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT16_MAX);
                if ((min_value <= (sym_addr + addend)) && ((sym_addr + addend) <= max_value)) {
                    *reinterpret_cast<ElfW(Addr)*>(rel_target) = (sym_addr + addend);
                } else {
                    return false;
                }
            }
                break;
            case R_AARCH64_PREL64:
                *reinterpret_cast<ElfW(Addr)*>(rel_target) = sym_addr + addend - rel->r_offset;
                break;
            case R_AARCH64_PREL32:
            {
                const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT32_MIN);
                const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT32_MAX);
                if ((min_value <= (sym_addr + addend - rel->r_offset)) &&
                    ((sym_addr + addend - rel->r_offset) <= max_value)) {
                    *reinterpret_cast<ElfW(Addr)*>(rel_target) = sym_addr + addend - rel->r_offset;
                } else {
                    return false;
                }
            }
                break;
            case R_AARCH64_PREL16:
            {
                const ElfW(Addr) min_value = static_cast<ElfW(Addr)>(INT16_MIN);
                const ElfW(Addr) max_value = static_cast<ElfW(Addr)>(UINT16_MAX);
                if ((min_value <= (sym_addr + addend - rel->r_offset)) &&
                    ((sym_addr + addend - rel->r_offset) <= max_value)) {
                    *reinterpret_cast<ElfW(Addr)*>(rel_target) = sym_addr + addend - rel->r_offset;
                } else {
                    return false;
                }
            }
                break;
            case R_AARCH64_COPY:
                return false;
            case R_AARCH64_TLS_TPREL64:
                break;
            default:
                return false;
        }
    }
    return true;
}
void soinfo::phdr_table_get_dynamic_section(const ElfW(Phdr) *phdr_table, size_t phdr_count,
                                            ElfW(Addr) load_bias, ElfW(Dyn) **dynamic,
                                            ElfW(Word) *dynamic_flags) {
    *dynamic = nullptr;
    for (size_t i = 0; i < phdr_count; ++i) {
        const ElfW(Phdr) &phdr = phdr_table[i];
        if (phdr.p_type == PT_DYNAMIC) {
            *dynamic = reinterpret_cast<ElfW(Dyn) *>(load_bias + phdr.p_vaddr);
            if (dynamic_flags) {
                *dynamic_flags = phdr.p_flags;
            }
            return;
        }
    }
}

const char *soinfo::get_string(ElfW(Word) index) const {
    return strtab_ + index;
}

void soinfo::set_dt_flags_1(uint32_t dt_flags_1) {
    if ((dt_flags_1 & DF_1_GLOBAL) != 0) {
        rtld_flags_ |= RTLD_GLOBAL;
    }

    if ((dt_flags_1 & DF_1_NODELETE) != 0) {
        rtld_flags_ |= RTLD_NODELETE;
    }

    dt_flags_1_ = dt_flags_1;
}

ElfW(Addr) soinfo::resolve_symbol_address(const ElfW(Sym) *s) const {
    if (ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC) {
        return call_ifunc_resolver(s->st_value + load_bias);
    }
    return static_cast<ElfW(Addr)>(s->st_value + load_bias);
}

ElfW(Addr) soinfo::call_ifunc_resolver(ElfW(Addr) resolver_addr) const {
    typedef ElfW(Addr) (*ifunc_resolver_t)(void);
    ifunc_resolver_t ifunc_resolver = reinterpret_cast<ifunc_resolver_t>(resolver_addr);
    ElfW(Addr) ifunc_addr = ifunc_resolver();

    return ifunc_addr;
}

bool soinfo::is_gnu_hash() const {
    return (flags_ & FLAG_GNU_HASH) != 0;
}

void soinfo::call_JNI_Onload() {
    using JNI_OnLoadFn = int(*)(JavaVM*, void*);
    if(jni_onloadF)
    {
        JNI_OnLoadFn jni_on_load = reinterpret_cast<JNI_OnLoadFn>(jni_onloadF);
        int version=jni_on_load(javaVm_, nullptr);
        if (version == JNI_ERR) {
            LOGE("Soinfo","JNI_OnLoad Error");
        }
    }

}
