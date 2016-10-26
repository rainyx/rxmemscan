//
// Created by rainyx on 16/8/26.
// Copyright (c) 2016 rainyx. All rights reserved.
//

#ifndef RXMEMSCAN_RX_MEM_SCAN_MINI_H
#define RXMEMSCAN_RX_MEM_SCAN_MINI_H

#include <mach/mach.h>
#include <libproc.h>
#include <memory>
#include <string>
#include <vector>
#include "lz4.h"

// #define RXDEBUG

#ifdef RXDEBUG
#   define _trace(s,...) (printf(s, __VA_ARGS__))
#else
#   define _trace(s,...)
#endif

typedef struct {
    boolean_t bf_nn;    // Base offset not null.
    uint32_t lf;        // Last offset.
    uint32_t bf;        // Base offset.
    uint32_t fc;        // Offset continuous.
} matched_offs_context_t;

typedef struct {
    uint32_t time_used;
    uint32_t memory_used;
    uint32_t matched;
} search_result_t;


class rx_region;

typedef uint8_t                     raw_data_t;
typedef raw_data_t *                raw_data_pt;

typedef uint32_t                    matched_off_t;
typedef uint32_t                    matched_off_ct;
typedef std::vector<matched_off_t>  matched_offs_t;
typedef matched_offs_t *            matched_offs_pt;

typedef rx_region                   region_t;
typedef std::vector<region_t>       regions_t;
typedef regions_t *                 regions_pt;

typedef std::vector<vm_address_t>   address_list_t;
typedef address_list_t *            address_list_pt;
typedef void *                      search_val_pt;
typedef uint8_t                     data_t;
typedef uint8_t *                   data_pt;

class rx_memory_page {
public:
    rx_memory_page() {
        addresses = NULL;
        data = NULL;
        data_size = 0;
    }
    ~rx_memory_page() {
        if (addresses) {
            delete addresses;
        }
        if (data) {
            delete[] data;
        }
    }

public:
    address_list_pt addresses;
    data_pt         data;
    size_t          data_size;
};
typedef rx_memory_page              rx_memory_page_t;
typedef rx_memory_page_t *          rx_memory_page_pt;

class rx_region {
public:
    rx_region() {
        compressed_region_data = NULL;
        matched_offs = NULL;
        compressed_region_data_size = 0;
    }

    vm_address_t address;
    vm_size_t size;

    raw_data_pt compressed_region_data;
    size_t compressed_region_data_size;

    matched_offs_pt matched_offs;
    uint32_t matched_count;

    size_t cal_memory_used() {
        return sizeof(vm_address_t) + sizeof(vm_address_t) + sizeof(uint32_t) + compressed_region_data_size
                + (matched_offs ?  (sizeof(matched_off_t) * matched_offs->size()) : 0);
    }
};

typedef enum {
    rx_compare_type_void,
    rx_compare_type_eq = 1,
    rx_compare_type_ne,
    rx_compare_type_lt,
    rx_compare_type_gt
} rx_compare_type;


class rx_comparator {
public:
    virtual ~rx_comparator() {}
    virtual boolean_t compare(void *a, void *b) = 0;
};

template <typename T>
class rx_comparator_typed_eq : public rx_comparator {
    boolean_t compare(void *a, void *b) { return *(T *)b == *(T *)a; }
};

template <typename T>
class rx_comparator_typed_ne : public rx_comparator {
    boolean_t compare(void *a, void *b) { return *(T *)b != *(T *)a; }
};

template <typename T>
class rx_comparator_typed_lt : public rx_comparator {
    boolean_t compare(void *a, void *b) { return *(T *)b < *(T *)a; }
};

template <typename T>
class rx_comparator_typed_gt : public rx_comparator {
    boolean_t compare(void *a, void *b) { return *(T *)b > *(T *)a; }
};

class rx_search_value_type {
public:
    ~rx_search_value_type() {}
    virtual size_t size_of_value() = 0;
    virtual rx_comparator *create_comparator(rx_compare_type ct) = 0;
};

template <typename T>
class rx_search_typed_value_type : public rx_search_value_type {
    size_t size_of_value() { return sizeof(T); }
    rx_comparator *create_comparator(rx_compare_type ct) {
        switch (ct) {
            case rx_compare_type_eq: return new rx_comparator_typed_eq<T>();
            case rx_compare_type_ne: return new rx_comparator_typed_ne<T>();
            case rx_compare_type_lt: return new rx_comparator_typed_lt<T>();
            case rx_compare_type_gt: return new rx_comparator_typed_gt<T>();
            default: return NULL;
        }
    }
};

#define rx_offc_mask                0x80000000
#define rx_add_offc_mask(i)         ((i) | rx_offc_mask)
#define rx_remove_offc_mask(i)      ((i) & ~rx_offc_mask)
#define rx_has_offc_mask(i)         (((i) & rx_offc_mask) == rx_offc_mask)

#define rx_search_fuzzy_val NULL
#define rx_is_fuzzy_search_val(_v)  ((_v)==rx_search_fuzzy_val)

#define rx_in_range(v, b, e)        ((v)>=(b) && (v)<(e))

class rx_mem_scan {

public:
    rx_mem_scan();
    ~rx_mem_scan();
    void free_memory();
    void reset();
    boolean_t attach(pid_t pid);
    boolean_t is_idle();
    search_result_t first_fuzzy_search();
    search_result_t fuzzy_search(rx_compare_type ct);
    search_result_t last_search_result();
    void set_search_value_type(rx_search_value_type *type_p);
    rx_memory_page_pt page_of_memory(vm_address_t address, uint32_t page_size);
    rx_memory_page_pt page_of_matched(uint32_t page_no, uint32_t page_size);
    kern_return_t write_val(vm_address_t address, search_val_pt val);
    void set_last_search_val(search_val_pt new_p);
    search_result_t search(search_val_pt search_val_p, rx_compare_type ct);
    pid_t target_pid();
    mach_port_t target_task();
private:
    matched_off_t offset_of_matched_offsets(matched_offs_t &vec, uint32_t off_idx);
    inline void free_region_memory(region_t &region);
    inline void matched_offs_flush(matched_offs_context_t &ctx, matched_offs_t &vec);
    inline void add_matched_offs_multi(matched_offs_t &vec, matched_off_t bi, matched_off_ct ic);
    inline void add_matched_off(matched_off_t matched_idx, matched_offs_context_t &ctx, matched_offs_t &vec);
    inline kern_return_t read_region(data_pt region_data, region_t &region, vm_size_t *read_count);
    void init_regions();
    void free_regions();
private:
    pid_t                       _target_pid;
    mach_port_t                 _target_task;
    regions_pt                  _regions_p;
    search_result_t             _last_search_result;
    search_val_pt               _last_search_val_p;
    boolean_t                   _unknown_last_search_val;
    boolean_t                   _idle;
    rx_search_value_type *      _search_value_type_p;
};


#endif //RXMEMSCAN_RX_MEM_SCAN_MINI_H
