#include"ElfReader.h"
ElfReader::ElfReader() {
    }
ElfReader::~ElfReader() {

    }
    jstring getBaseApkPath() {//调用getPackageCodePath
        LOGE("ElfReader","%lx",(uint64_t)application);
        jclass contextClass = env_->GetObjectClass(application);

        jmethodID getPackageCodePathMethod = env_->GetMethodID(
                contextClass, "getPackageCodePath", "()Ljava/lang/String;");
        if (!getPackageCodePathMethod) {
            env_->DeleteLocalRef(contextClass);
            return nullptr;
        }

        auto apkPathStr = (jstring) env_->CallObjectMethod(application, getPackageCodePathMethod);
        env_->DeleteLocalRef(contextClass);
        // 注意：此处不能立即释放 apkPathStr，需在调用者使用完后释放
        return apkPathStr;
    }

// 获取应用缓存目录
    jstring getAppCacheDir() {  //调用getCacheDir
        jclass contextClass = env_->GetObjectClass(application);
        jmethodID getCacheDirMethod = env_->GetMethodID(contextClass, "getCacheDir",
                                                        "()Ljava/io/File;");
        jobject cacheDirObj = env_->CallObjectMethod(application, getCacheDirMethod);
        env_->DeleteLocalRef(contextClass);

        jclass fileClass = env_->FindClass("java/io/File");
        jmethodID getPathMethod = env_->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
        auto cachePathStr = (jstring) env_->CallObjectMethod(cacheDirObj, getPathMethod);
        env_->DeleteLocalRef(fileClass);
        env_->DeleteLocalRef(cacheDirObj);
        // 注意：此处不能立即释放 cachePathStr，需在调用者使用完后释放
        return cachePathStr;
    }

    bool ElfReader::OpenLibFromZip(const char *path) {
        struct stat libstat;
        jstring apkPath = getBaseApkPath();
        if (!apkPath) {
            return false;
        }
        // 获取应用缓存目录
        jstring cachePath = getAppCacheDir();
        if (!cachePath) {
            env_->DeleteLocalRef(apkPath);
            return false;
        }
        // 加载 ApkExtractor 类
        jclass extractorClass = env_->FindClass("com/nobody/demo/ApkExtractor");
        if (!extractorClass) {
            env_->DeleteLocalRef(apkPath);
            env_->DeleteLocalRef(cachePath);
            return false;
        }
        // 获取 extractLibFromApk 方法
        jmethodID extractMethod = env_->GetStaticMethodID(
                extractorClass, "extractLibFromApk",
                "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z");
        if (!extractMethod) {
            env_->DeleteLocalRef(extractorClass);
            return false;
        }
        // 调用extractLibFromApk方法
        jboolean result = env_->CallStaticBooleanMethod(
                extractorClass, extractMethod, application, apkPath, cachePath);
        //判断abi
        std::string libpath =
                std::string(env_->GetStringUTFChars(cachePath, nullptr)) + "/arm64-v8a/" +
                std::string(path);
        int fd = open(libpath.c_str(), O_RDONLY | O_CLOEXEC);
        fstat(fd, &libstat);
        file_size_=libstat.st_size;
        start_addr_ = mmap(nullptr, libstat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (start_addr_ == MAP_FAILED) {
            env_->DeleteLocalRef(extractorClass);
            env_->DeleteLocalRef(apkPath);
            env_->DeleteLocalRef(cachePath);
            close(fd);
            return false;
        }
        // 释放资源
        env_->DeleteLocalRef(extractorClass);
        env_->DeleteLocalRef(apkPath);
        env_->DeleteLocalRef(cachePath);
        close(fd);
        return true;
    }

    bool ElfReader::Open(const char *path, int flag) {
        if (flag == LOADLIBIARY) {
            return OpenLibFromZip(path);
        } else if (flag == LOAD) {
            struct stat libstat;
            int fd = open(path, O_RDONLY | O_CLOEXEC);
            fstat(fd, &libstat);
            //map到内存
            start_addr_ = mmap(nullptr, libstat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
                               0);
            if (start_addr_ == MAP_FAILED) {
                close(fd);
                return false;
            }
            close(fd);
            return true;
        }
        return false;
    }

    bool ElfReader::Read() {
        if (ReadElfHeader() &&
            ReadProgramHeaders()) {
            did_read_ = true;
        }
        return did_read_;
    }

    bool ElfReader::ReadElfHeader() {
       return memcpy(&(header_), start_addr_, sizeof(header_));;
    }

    bool ElfReader::ReadProgramHeaders() {
        phdr_num_ = header_.e_phnum;
        if (phdr_num_ < 1 || phdr_num_ > 65536 / sizeof(ElfW(Phdr))) {
            return false;
        }
        size_t size = phdr_num_ * sizeof(ElfW(Phdr));
        if (header_.e_phoff + size > file_size_) {
            LOGE("ElfReader", "Program headers out of file bounds");
            return false;
        }
        phdr_table_ = static_cast<ElfW(Phdr) *>(malloc(size));
        if (phdr_table_ == nullptr) {
            LOGE("ElfReader", "Malloc memory failed");
            return false;
        }
        return memcpy(phdr_table_, (char *) start_addr_ + header_.e_phoff, size);;
    }

    soinfo * ElfReader::Load() {
        if (ReserveAddressSpace() && LoadSegments() && FindPhdr()) {
            did_load_ = true;
        }
        //获取so进行修正
        si_ = GetSoinfo("libc.so");
        mprotect((void*)(page_start(reinterpret_cast<ElfW(Addr)>(si_))), 0x1000, PROT_READ | PROT_WRITE);
        si_->base = (ElfW(Addr))(load_start_);
        si_->size = load_size_;
        si_->load_bias = load_bias_;
        si_->phnum = phdr_num_;
        si_->phdr = loaded_phdr_;
        si_->init_func_= nullptr;
        si_->init_array_= nullptr;
        si_->init_array_count_=0;
        si_->fini_array_= nullptr;
        si_->fini_func_= nullptr;
        si_->fini_array_count_=0;

        //遍历导出表，获取JNI_OnLoad偏移
        ElfW(Ehdr) header;
        memcpy(&(header), reinterpret_cast<const void *>(start_addr_), sizeof(header));
        size_t size = header.e_shnum * sizeof(ElfW(Shdr));
        void *tmp = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        //映射节表
        ElfW(Shdr) *shdr_table;
        memcpy(tmp, (void *) ((ElfW(Off)) start_addr_ + header.e_shoff), size);
        shdr_table = static_cast<ElfW(Shdr) *>(tmp);
        //获取节区名
        char *shstrtab = reinterpret_cast<char *>(shdr_table[header.e_shstrndx].sh_offset +
                                                  (ElfW(Off)) start_addr_);
        void *dynsym = nullptr;
        char *dynstr = nullptr;
        uint32_t dynsym_size = 0;
        for (size_t i = 0; i < header.e_shnum; ++i) {
            const ElfW(Shdr) *shdr = &shdr_table[i];
            std::string section_name(shstrtab + shdr->sh_name);
            if (section_name == ".dynsym") {
                dynsym = reinterpret_cast<void *>(shdr->sh_offset + (ElfW(Off)) start_addr_);
                dynsym_size = shdr->sh_size;
            }
            if (section_name == ".dynstr") {
                dynstr = reinterpret_cast<char *>(shdr->sh_offset + (ElfW(Off)) start_addr_);
            }
            if (dynstr && dynsym)break;
        }
        //映射Symbol table
        ElfW(Sym) *sym_table;
        tmp = mmap(nullptr, dynsym_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
                   0);
        memcpy(tmp, dynsym, dynsym_size);
        sym_table = static_cast<ElfW(Sym) *>(tmp);
        int sym_num = dynsym_size / sizeof(ElfW(Sym));
        for (int i = 0; i < sym_num; i++) {
            const ElfW(Sym) *sym = &sym_table[i];
            std::string sym_name(dynstr + sym->st_name);
            LOGE("ElfReader","%s",sym_name.c_str());
            if (sym_name.find("JNI_OnLoad") != -1) {
                jni_onloadF=(ElfW(Addr))load_start_+sym->st_value;
                break;
            }
        }
        return si_;
    }

    size_t phdr_table_get_load_size(const ElfW(Phdr) *phdr_table, size_t phdr_count,
                                    ElfW(Addr) *out_min_vaddr) {
        ElfW(Addr) min_vaddr = UINTPTR_MAX;
        ElfW(Addr) max_vaddr = 0;

        bool found_pt_load = false;
        for (size_t i = 0; i < phdr_count; ++i) {
            const ElfW(Phdr) *phdr = &phdr_table[i];

            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            found_pt_load = true;

            if (phdr->p_vaddr < min_vaddr) {
                min_vaddr = phdr->p_vaddr;
            }

            if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
                max_vaddr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
        if (!found_pt_load) {
            min_vaddr = 0;
        }

        min_vaddr = page_start(min_vaddr);
        max_vaddr = page_end(max_vaddr);

        if (out_min_vaddr != nullptr) {
            *out_min_vaddr = min_vaddr;
        }
        return max_vaddr - min_vaddr;
    }

    bool ElfReader::ReserveAddressSpace() {
        ElfW(Addr) min_vaddr;
        load_size_ = phdr_table_get_load_size(phdr_table_, phdr_num_, &min_vaddr);
        auto *addr = reinterpret_cast<uint8_t *>(min_vaddr);
        void *start;
        start = mmap(nullptr, load_size_, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        load_start_ = start;
        load_bias_ = reinterpret_cast<uint8_t *>(start) - addr;
        return true;
    }

    bool ElfReader::LoadSegments() {
        for (size_t i = 0; i < phdr_num_; ++i) {
            const ElfW(Phdr) *phdr = &phdr_table_[i];

            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            ElfW(Addr) p_memsz = phdr->p_memsz;
            ElfW(Addr) p_filesz = phdr->p_filesz;
            // Segment addresses in memory.
            ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
            ElfW(Addr) seg_end = seg_start + p_memsz;
            ElfW(Addr) seg_page_start = page_start(seg_start);
            ElfW(Addr) seg_page_end = page_end(seg_end);
            ElfW(Addr) seg_file_end = seg_start + p_filesz;
            // File offsets.
            ElfW(Addr) file_start = phdr->p_offset;
            ElfW(Addr) file_end = file_start + p_filesz;
            ElfW(Addr) file_page_start = page_start(file_start);
            ElfW(Addr) file_length = file_end - file_page_start;
            if (file_length != 0) {
                mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_end - seg_page_start,
                         PROT_WRITE);
                void *target = (char *) start_addr_ + file_page_start;
                void *res = memcpy(reinterpret_cast<void *>(seg_page_start), target, file_length);
                int prot = PFLAGS_TO_PROT(phdr->p_flags);
                mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_end - seg_page_start,
                         prot);
            }
            //初始化需要零填充的内存区域
            ZeroFillSegment(phdr);
            //处理bss段
            if (!MapBssSection(phdr, seg_page_end, seg_file_end)) {
                return false;
            }
        }
        return true;
    }

    void ElfReader::ZeroFillSegment(const ElfW(Phdr) *phdr) {
        ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
        uint64_t unextended_seg_file_end = seg_start + phdr->p_filesz;
        // If the segment is writable, and does not end on a page boundary,
        // zero-fill it until the page limit
        if ((phdr->p_flags & PF_W) != 0 && page_offset(unextended_seg_file_end) > 0) {
            memset(reinterpret_cast<void *>(unextended_seg_file_end), 0,
                   PAGE_SIZE - page_offset(unextended_seg_file_end));
        }
    }

    bool ElfReader::MapBssSection(const Elf64_Phdr *phdr, Elf64_Addr seg_page_end,
                       Elf64_Addr seg_file_end) {
        seg_file_end = page_end(seg_file_end);
        if (seg_page_end <= seg_file_end) {
            return true;
        }
        // If seg_page_end is larger than seg_file_end, we need to zero
        // anything between them. This is done by using a private anonymous
        // map for all extra pages
        size_t zeromap_size = seg_page_end - seg_file_end;
        void *zeromap =
                mmap(reinterpret_cast<void *>(seg_file_end), zeromap_size,
                     PFLAGS_TO_PROT(phdr->p_flags),
                     MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (zeromap == MAP_FAILED) {
            LOGE("ElfReader", "couldn't map .bss section");
            return false;
        }
        // Set the VMA name using prctl
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, zeromap, zeromap_size, ".bss");
        return true;
    }

    bool ElfReader::FindPhdr() {
        const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;

        // If there is a PT_PHDR, use it directly.
        for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
            if (phdr->p_type == PT_PHDR) {
                return CheckPhdr(load_bias_ + phdr->p_vaddr);
            }
        }
        // Otherwise, check the first loadable segment. If its file offset
        // is 0, it starts with the ELF header, and we can trivially find the
        // loaded program header from it.
        for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
            if (phdr->p_type == PT_LOAD) {
                if (phdr->p_offset == 0) {
                    ElfW(Addr) elf_addr = load_bias_ + phdr->p_vaddr;
                    const ElfW(Ehdr) *ehdr = reinterpret_cast<const ElfW(Ehdr) *>(elf_addr);
                    ElfW(Addr) offset = ehdr->e_phoff;
                    return CheckPhdr(reinterpret_cast<ElfW(Addr)>(ehdr) + offset);
                }
                break;
            }
        }

        LOGE("ElfReader", "can't find loaded phdr");
        return false;
    }

    bool ElfReader::CheckPhdr(ElfW(Addr) loaded) {
        const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;
        ElfW(Addr) loaded_end = loaded + (phdr_num_ * sizeof(ElfW(Phdr)));
        for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
            ElfW(Addr) seg_end = phdr->p_filesz + seg_start;
            if (seg_start <= loaded && loaded_end <= seg_end) {
                loaded_phdr_ = reinterpret_cast<const ElfW(Phdr) *>(loaded);
                return true;
            }
        }
        LOGE("ElfReader", "loaded phdr %p not in loadable segment",
             reinterpret_cast<void *>(loaded));
        return false;
    }

soinfo * ElfReader::GetSoinfo(std::string libname) {
    typedef soinfo* (*FunctionPtr)(ElfW(Addr));
    ElfW(Addr) so_addr=getlibbase(libname);
    FunctionPtr getsoinfo = (FunctionPtr)GetFuncFromSymbol("/system/bin/linker64", "find_containing_library");
    soinfo * result=getsoinfo(so_addr);
    return result;
}



