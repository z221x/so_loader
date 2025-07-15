#pragma once
#include "common.h"
#define R_GENERIC_NONE 0 // R_*_NONE is always 0
#if defined (__aarch64__)

#define R_GENERIC_JUMP_SLOT     R_AARCH64_JUMP_SLOT
// R_AARCH64_ABS64 is classified as a static relocation but it is common in DSOs.
#define R_GENERIC_ABSOLUTE      R_AARCH64_ABS64
#define R_GENERIC_GLOB_DAT      R_AARCH64_GLOB_DAT
#define R_GENERIC_RELATIVE      R_AARCH64_RELATIVE
#define R_GENERIC_IRELATIVE     R_AARCH64_IRELATIVE
#define R_GENERIC_COPY          R_AARCH64_COPY
#define R_GENERIC_TLS_DTPMOD    R_AARCH64_TLS_DTPMOD
#define R_GENERIC_TLS_DTPREL    R_AARCH64_TLS_DTPREL
#define R_GENERIC_TLS_TPREL     R_AARCH64_TLS_TPREL
#define R_GENERIC_TLSDESC       R_AARCH64_TLSDESC

#elif defined (__arm__)

#define R_GENERIC_JUMP_SLOT     R_ARM_JUMP_SLOT
// R_ARM_ABS32 is classified as a static relocation but it is common in DSOs.
#define R_GENERIC_ABSOLUTE      R_ARM_ABS32
#define R_GENERIC_GLOB_DAT      R_ARM_GLOB_DAT
#define R_GENERIC_RELATIVE      R_ARM_RELATIVE
#define R_GENERIC_IRELATIVE     R_ARM_IRELATIVE
#define R_GENERIC_COPY          R_ARM_COPY
#define R_GENERIC_TLS_DTPMOD    R_ARM_TLS_DTPMOD32
#define R_GENERIC_TLS_DTPREL    R_ARM_TLS_DTPOFF32
#define R_GENERIC_TLS_TPREL     R_ARM_TLS_TPOFF32
#define R_GENERIC_TLSDESC       R_ARM_TLS_DESC

#elif defined (__i386__)

#define R_GENERIC_JUMP_SLOT     R_386_JMP_SLOT
#define R_GENERIC_ABSOLUTE      R_386_32
#define R_GENERIC_GLOB_DAT      R_386_GLOB_DAT
#define R_GENERIC_RELATIVE      R_386_RELATIVE
#define R_GENERIC_IRELATIVE     R_386_IRELATIVE
#define R_GENERIC_COPY          R_386_COPY
#define R_GENERIC_TLS_DTPMOD    R_386_TLS_DTPMOD32
#define R_GENERIC_TLS_DTPREL    R_386_TLS_DTPOFF32
#define R_GENERIC_TLS_TPREL     R_386_TLS_TPOFF
#define R_GENERIC_TLSDESC       R_386_TLS_DESC

#elif defined (__riscv)

#define R_GENERIC_JUMP_SLOT     R_RISCV_JUMP_SLOT
#define R_GENERIC_ABSOLUTE      R_RISCV_64
#define R_GENERIC_GLOB_DAT      R_RISCV_64
#define R_GENERIC_RELATIVE      R_RISCV_RELATIVE
#define R_GENERIC_IRELATIVE     R_RISCV_IRELATIVE
#define R_GENERIC_COPY          R_RISCV_COPY
#define R_GENERIC_TLS_DTPMOD    R_RISCV_TLS_DTPMOD64
#define R_GENERIC_TLS_DTPREL    R_RISCV_TLS_DTPREL64
#define R_GENERIC_TLS_TPREL     R_RISCV_TLS_TPREL64
#define R_GENERIC_TLSDESC       R_RISCV_TLSDESC

#elif defined (__x86_64__)

#define R_GENERIC_JUMP_SLOT     R_X86_64_JUMP_SLOT
#define R_GENERIC_ABSOLUTE      R_X86_64_64
#define R_GENERIC_GLOB_DAT      R_X86_64_GLOB_DAT
#define R_GENERIC_RELATIVE      R_X86_64_RELATIVE
#define R_GENERIC_IRELATIVE     R_X86_64_IRELATIVE
#define R_GENERIC_COPY          R_X86_64_COPY
#define R_GENERIC_TLS_DTPMOD    R_X86_64_DTPMOD64
#define R_GENERIC_TLS_DTPREL    R_X86_64_DTPOFF64
#define R_GENERIC_TLS_TPREL     R_X86_64_TPOFF64
#define R_GENERIC_TLSDESC       R_X86_64_TLSDESC

#endif
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
template<typename T>
struct LinkedListEntry {
    LinkedListEntry<T>* next;
    T* element;
};
// ForwardInputIterator
template<typename T>
class LinkedListIterator {
public:
    LinkedListIterator() : entry_(nullptr) {}
    LinkedListIterator(const LinkedListIterator<T>& that) : entry_(that.entry_) {}
    explicit LinkedListIterator(LinkedListEntry<T>* entry) : entry_(entry) {}

    LinkedListIterator<T>& operator=(const LinkedListIterator<T>& that) {
        entry_ = that.entry_;
        return *this;
    }

    LinkedListIterator<T>& operator++() {
        entry_ = entry_->next;
        return *this;
    }

    T* const operator*() {
        return entry_->element;
    }

    bool operator==(const LinkedListIterator<T>& that) const {
        return entry_ == that.entry_;
    }

    bool operator!=(const LinkedListIterator<T>& that) const {
        return entry_ != that.entry_;
    }

private:
    LinkedListEntry<T> *entry_;
};

/*
 * Represents linked list of objects of type T
 */
template<typename T, typename Allocator>
class LinkedList {
public:
    typedef LinkedListIterator<T> iterator;
    typedef T* value_type;

    // Allocating the head/tail fields separately from the LinkedList struct saves memory in the
    // Zygote (e.g. because adding an soinfo to a namespace doesn't dirty the page containing the
    // soinfo).
    struct LinkedListHeader {
        LinkedListEntry<T>* head;
        LinkedListEntry<T>* tail;
    };

    // The allocator returns a LinkedListEntry<T>* but we want to treat it as a LinkedListHeader
    // struct instead.
    static_assert(sizeof(LinkedListHeader) == sizeof(LinkedListEntry<T>));
    static_assert(alignof(LinkedListHeader) == alignof(LinkedListEntry<T>));

    constexpr LinkedList() : header_(nullptr) {}
    ~LinkedList() {
        clear();
        if (header_ != nullptr) {
            Allocator::free(reinterpret_cast<LinkedListEntry<T>*>(header_));
        }
    }

    LinkedList(LinkedList&& that) noexcept {
        this->header_ = that.header_;
        that.header_ = nullptr;
    }

    bool empty() const {
        return header_ == nullptr || header_->head == nullptr;
    }

    void push_front(T* const element) {
        alloc_header();
        LinkedListEntry<T>* new_entry = Allocator::alloc();
        new_entry->next = header_->head;
        new_entry->element = element;
        header_->head = new_entry;
        if (header_->tail == nullptr) {
            header_->tail = new_entry;
        }
    }

    void push_back(T* const element) {
        alloc_header();
        LinkedListEntry<T>* new_entry = Allocator::alloc();
        new_entry->next = nullptr;
        new_entry->element = element;
        if (header_->tail == nullptr) {
            header_->tail = header_->head = new_entry;
        } else {
            header_->tail->next = new_entry;
            header_->tail = new_entry;
        }
    }

    T* pop_front() {
        if (empty()) return nullptr;

        LinkedListEntry<T>* entry = header_->head;
        T* element = entry->element;
        header_->head = entry->next;
        Allocator::free(entry);

        if (header_->head == nullptr) {
            header_->tail = nullptr;
        }

        return element;
    }

    T* front() const {
        return empty() ? nullptr : header_->head->element;
    }

    void clear() {
        if (empty()) return;

        while (header_->head != nullptr) {
            LinkedListEntry<T>* p = header_->head;
            header_->head = header_->head->next;
            Allocator::free(p);
        }

        header_->tail = nullptr;
    }

    template<typename F>
    void for_each(F action) const {
        visit([&] (T* si) {
            action(si);
            return true;
        });
    }

    template<typename F>
    bool visit(F action) const {
        for (LinkedListEntry<T>* e = head(); e != nullptr; e = e->next) {
            if (!action(e->element)) {
                return false;
            }
        }
        return true;
    }

    template<typename F>
    void remove_if(F predicate) {
        if (empty()) return;
        for (LinkedListEntry<T>* e = header_->head, *p = nullptr; e != nullptr;) {
            if (predicate(e->element)) {
                LinkedListEntry<T>* next = e->next;
                if (p == nullptr) {
                    header_->head = next;
                } else {
                    p->next = next;
                }

                if (header_->tail == e) {
                    header_->tail = p;
                }

                Allocator::free(e);

                e = next;
            } else {
                p = e;
                e = e->next;
            }
        }
    }

    void remove(T* element) {
        remove_if([&](T* e) {
            return e == element;
        });
    }

    template<typename F>
    T* find_if(F predicate) const {
        for (LinkedListEntry<T>* e = head(); e != nullptr; e = e->next) {
            if (predicate(e->element)) {
                return e->element;
            }
        }

        return nullptr;
    }

    iterator begin() const {
        return iterator(head());
    }

    iterator end() const {
        return iterator(nullptr);
    }

    iterator find(T* value) const {
        for (LinkedListEntry<T>* e = head(); e != nullptr; e = e->next) {
            if (e->element == value) {
                return iterator(e);
            }
        }

        return end();
    }

    size_t copy_to_array(T* array[], size_t array_length) const {
        size_t sz = 0;
        for (LinkedListEntry<T>* e = head(); sz < array_length && e != nullptr; e = e->next) {
            array[sz++] = e->element;
        }

        return sz;
    }

    bool contains(const T* el) const {
        for (LinkedListEntry<T>* e = head(); e != nullptr; e = e->next) {
            if (e->element == el) {
                return true;
            }
        }
        return false;
    }

    static LinkedList make_list(T* const element) {
        LinkedList<T, Allocator> one_element_list;
        one_element_list.push_back(element);
        return one_element_list;
    }

    size_t size() const {
        size_t result = 0;
        for_each([&](T*) { ++result; });
        return result;
    }

private:
    void alloc_header() {
        if (header_ == nullptr) {
            header_ = reinterpret_cast<LinkedListHeader*>(Allocator::alloc());
            header_->head = header_->tail = nullptr;
        }
    }

    LinkedListEntry<T>* head() const {
        return header_ != nullptr ? header_->head : nullptr;
    }

    LinkedListHeader* header_;
    DISALLOW_COPY_AND_ASSIGN(LinkedList);
};