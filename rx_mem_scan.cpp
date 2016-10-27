//
// Created by rainyx on 16/9/9.
// Copyright (c) 2016 rainyx. All rights reserved.
//

#include "rx_mem_scan.h"
#include "lz4/lz4.h"

static long get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

rx_mem_scan::rx_mem_scan() {
    _idle                   = true;
    _regions_p              = NULL;
    _search_value_type_p    = NULL;
    _last_search_val_p      = NULL;
}

rx_mem_scan::~rx_mem_scan() {
    free_memory();
}

void rx_mem_scan::free_memory() {
    free_regions();
    if (_last_search_val_p) {
        free(_last_search_val_p);
        _last_search_val_p = NULL;
    }
}

void rx_mem_scan::reset() {
    free_memory();
    init_regions();
    _idle = true;
}

boolean_t rx_mem_scan::attach(pid_t pid) {
    _target_pid = pid;
    kern_return_t ret = task_for_pid(mach_task_self(), pid, &_target_task);
    if (ret != KERN_SUCCESS) {
        _trace("Attach to: %d Failed: %d %s\n", pid, ret, mach_error_string(ret));
        return false;
    }
    reset();
    return true;
}

pid_t rx_mem_scan::target_pid() {
    return _target_pid;
}

mach_port_t rx_mem_scan::target_task() {
    return _target_task;
}

boolean_t rx_mem_scan::is_idle() {
    return _idle;
}

search_result_t rx_mem_scan::first_fuzzy_search() {
    return search(rx_search_fuzzy_val, rx_compare_type_void);
}

search_result_t rx_mem_scan::fuzzy_search(rx_compare_type ct) {
    return search(rx_search_fuzzy_val, ct);
}

search_result_t rx_mem_scan::last_search_result() {
    return _last_search_result;
}

void rx_mem_scan::set_search_value_type(rx_search_value_type *type_p) {
    _search_value_type_p = type_p;
    if (!is_idle()) {
        reset();
    }
}

rx_memory_page_pt rx_mem_scan::page_of_memory(vm_address_t address, uint32_t page_size) {

    rx_memory_page_pt page = new rx_memory_page_t;
    page->addresses = new address_list_t;
    page->data_size = page_size * _search_value_type_p->size_of_value();
    page->data = new data_t[page->data_size];
    data_pt page_data_itor_p = page->data;

    for (uint32_t i = 0; i < _regions_p->size(); ++i) {

        region_t region = (*_regions_p)[i];
        data_pt region_data_p = NULL;
        data_pt region_data_itor_p = NULL;

        if (rx_in_range(region.address, address, address + page_size * _search_value_type_p->size_of_value())) {
            address = region.address;
        }
        while (rx_in_range(address, region.address, region.address + region.size) && page_size > 0) {
            if (region_data_p == NULL) {
                region_data_p = new data_t[region.size];
                vm_size_t read_count;
                kern_return_t ret = read_region(region_data_p, region, &read_count);
                if (ret != KERN_SUCCESS) {
                    // TODO
                }
                region_data_itor_p = region_data_p + (address - region.address);
            }

            page->addresses->push_back(address);
            memcpy(page_data_itor_p, region_data_itor_p, _search_value_type_p->size_of_value());

            address += _search_value_type_p->size_of_value();
            region_data_itor_p += _search_value_type_p->size_of_value();
            page_data_itor_p += _search_value_type_p->size_of_value();
            --page_size;
        }

        if (region_data_p) {
            delete[] region_data_p;
        }

        if (page_size == 0) {
            break;
        }
    }

    return page;

}

rx_memory_page_pt rx_mem_scan::page_of_matched(uint32_t page_no, uint32_t page_size) {

    rx_memory_page_pt page = new rx_memory_page_t;
    page->addresses = new address_list_t;
    page->data_size = page_size * _search_value_type_p->size_of_value();
    page->data = (data_pt) malloc(page->data_size);

    uint32_t page_idx_begin = page_no * page_size;
    uint32_t page_idx_end = page_idx_begin + page_size;

    uint32_t region_idx_begin = 0;
    data_pt page_data_itor_p = page->data;
    for (uint32_t i = 0; i < _regions_p->size(); ++i) {

        region_t region = (*_regions_p)[i];
        uint32_t region_idx_end = region_idx_begin + region.matched_count;

        data_pt region_data_p = NULL;
        while (rx_in_range(page_idx_begin, region_idx_begin, region_idx_end)) {
            if (region_data_p == NULL) {
                region_data_p = new data_t[region.size];
                vm_size_t read_count;
                kern_return_t ret = read_region(region_data_p, region, &read_count);
                if (ret != KERN_SUCCESS) {
                    // TODO
                }
            }

            uint32_t matched_idx_idx = page_idx_begin - region_idx_begin;
            matched_off_t matched_idx = offset_of_matched_offsets(*region.matched_offs, matched_idx_idx);

            vm_offset_t offset = matched_idx;
            vm_address_t address = region.address + offset;
            search_val_pt val_p = &(region_data_p[offset]);

            page->addresses->push_back(address);
            memcpy(page_data_itor_p, val_p, _search_value_type_p->size_of_value());

            ++ page_idx_begin;
            page_data_itor_p += _search_value_type_p->size_of_value();

            if (page_idx_begin == page_idx_end) {
                delete[] region_data_p;
                goto ret;
            }
        }

        if (region_data_p) {
            delete[] region_data_p;
        }

        region_idx_begin = region_idx_end;

    }

    ret:

    return page;

}


kern_return_t rx_mem_scan::write_val(vm_address_t address, search_val_pt val) {
    // TODO Check address is writable.
    kern_return_t ret = vm_write(_target_task, address, (vm_offset_t) val, _search_value_type_p->size_of_value());

    return ret;
}

void rx_mem_scan::set_last_search_val(search_val_pt new_p) {
    if (!_last_search_val_p) {
        _last_search_val_p = malloc(_search_value_type_p->size_of_value());
    }
    memcpy(_last_search_val_p, new_p, _search_value_type_p->size_of_value());
}


search_result_t rx_mem_scan::search(search_val_pt search_val_p, rx_compare_type ct) {

    search_result_t     result              = { 0 };
    long                begin_time          = get_timestamp();
    bool                is_fuzzy_search     = rx_is_fuzzy_search_val(search_val_p);
    regions_pt          used_regions        = new regions_t();
    rx_comparator *     comparator          = _search_value_type_p->create_comparator(ct);

    if (is_fuzzy_search) {
        if (_idle) {
            _unknown_last_search_val = true;
        } else if (!_unknown_last_search_val) {
            search_val_p = _last_search_val_p;
        }
    } else {
        _unknown_last_search_val = false;
        set_last_search_val(search_val_p);
    }

    for (uint32_t i = 0; i < _regions_p->size(); ++i) {

        region_t region = (*_regions_p)[i];
        //printf("Region address: %p, region size: %d\n", (void *)region.address, (int)region.size);
        size_t size_of_value = _search_value_type_p->size_of_value();
        size_t data_count = region.size / size_of_value;

        vm_size_t raw_data_read_count;
        data_pt region_data_p = new data_t[region.size];
        kern_return_t ret = read_region(region_data_p, region, &raw_data_read_count);

        if (ret == KERN_SUCCESS) {

            matched_offs_pt matched_offs_p = new matched_offs_t;

            data_pt data_itor_p = region_data_p;
            uint32_t matched_count = 0;

            matched_offs_context_t matched_offs_context = { 0 };
            bzero(&matched_offs_context, sizeof(matched_offs_context_t));

            if (_idle) {
                if (is_fuzzy_search) {
                    matched_count = data_count;
                    add_matched_offs_multi(*matched_offs_p, 0, data_count);
                } else {
                    matched_off_t idx = 0;
                    data_pt end_p = (region_data_p + region.size);
                    while (data_itor_p < end_p) {
                        if (comparator->compare(search_val_p, data_itor_p)) {
                            ++ matched_count;
                            add_matched_off(idx, matched_offs_context, *matched_offs_p);
                        }
                        data_itor_p += size_of_value;
                        idx += size_of_value;
                    }
                }

            } else {

                // Search again.
                matched_offs_pt old_matched_offs = region.matched_offs;
                data_pt old_region_data_p = NULL;
                if (_unknown_last_search_val) {
                    old_region_data_p = new data_t[region.size];
                    int result = LZ4_decompress_fast((const char *) region.compressed_region_data, (char *) old_region_data_p, region.size);
                    // TODO process result
                }

                for (uint32_t j = 0; j < old_matched_offs->size(); ++j) {
                    matched_off_t old_matched_bf = (*old_matched_offs)[j];

                    matched_off_ct old_matched_fc = 1;
                    if (rx_has_offc_mask(old_matched_bf)) {
                        old_matched_bf = rx_remove_offc_mask(old_matched_bf);
                        old_matched_fc = (*old_matched_offs)[++ j];
                    }

                    matched_off_t old_matched_off = old_matched_bf;
                    for (uint32_t k = 0; k < old_matched_fc; ++k, old_matched_off += size_of_value) {
                        data_pt data_itor_p = &(region_data_p[old_matched_off]);

                        if (_unknown_last_search_val) {
                            search_val_p = &(old_region_data_p[old_matched_off]);
                        }

                        if (comparator->compare(search_val_p, data_itor_p)) {
                            add_matched_off(old_matched_off, matched_offs_context, *matched_offs_p);
                            matched_count ++;
                        }

                    }

                }

                if (_unknown_last_search_val) {
                    delete[] old_region_data_p;
                }

            }

            free_region_memory(region);

            if (matched_count > 0) {

                if (_unknown_last_search_val) {

                    // Compress matched region_data_p, using lz4. https://cyan4973.github.io/lz4/
                    const size_t max_compressed_data_size = LZ4_compressBound(region.size);
                    uint8_t *compressed_data = new uint8_t[max_compressed_data_size];
                    int compressed_data_size = LZ4_compress_fast((char *) region_data_p, (char *) compressed_data, region.size, max_compressed_data_size, 1);

                    region.compressed_region_data = new data_t[compressed_data_size];
                    region.compressed_region_data_size = compressed_data_size;
                    memcpy(region.compressed_region_data, compressed_data, compressed_data_size);
                    delete[] compressed_data;
                }

                matched_offs_flush(matched_offs_context, *matched_offs_p);
                region.matched_offs = matched_offs_p;
                region.matched_count = matched_count;

                result.memory_used += region.cal_memory_used();
                result.matched += matched_count;

                used_regions->push_back(region);
            }

        }

        delete[] region_data_p;

    }

    delete comparator;
    delete _regions_p;
    _regions_p = used_regions;
    _idle = false;

    long end_time = get_timestamp();
    result.time_used = end_time - begin_time;

    _trace("Result count: %d memory used: %.4f(MB) time used: %.3f(s)\n",
            (int)result.matched, (float)result.memory_used/(float)_1MB, (float)(result.time_used)/1000.0f);

    _last_search_result = result;
    return result;
}


matched_off_t rx_mem_scan::offset_of_matched_offsets(matched_offs_t &vec, uint32_t off_idx) {

    uint32_t v_idx = 0;
    uint32_t b_off_idx = 0;
    while (v_idx < vec.size()) {
        matched_off_t b_off = vec[v_idx];
        if (rx_has_offc_mask(b_off)) {
            b_off = rx_remove_offc_mask(b_off);
            matched_off_ct off_c = vec[v_idx + 1];
            if (rx_in_range(off_idx, b_off_idx, b_off_idx + off_c)) {
                return b_off + (off_idx - b_off_idx) * _search_value_type_p->size_of_value();
            } else {
                b_off_idx += off_c;
                v_idx += 2;
            }
        } else {
            if (b_off_idx == off_idx) {
                return b_off;
            } else {
                b_off_idx += 1;
                v_idx += 1;
            }
        }
    }
    return 0;
}

void rx_mem_scan::free_regions() {
    if (_regions_p) {
        for (uint32_t i = 0; i < _regions_p->size(); ++i) {
            region_t region = (*_regions_p)[i];
            free_region_memory(region);
        }
        delete _regions_p;
        _regions_p = NULL;
    }
}

inline void rx_mem_scan::free_region_memory(region_t &region) {
    if (region.compressed_region_data) {
        delete[] region.compressed_region_data;
        region.compressed_region_data = NULL;
    }
    if (region.matched_offs) {
        delete region.matched_offs;
        region.matched_offs = NULL;
    }
}

inline void rx_mem_scan::matched_offs_flush(matched_offs_context_t &ctx, matched_offs_t &vec) {

    if (ctx.bf_nn) {
        if (ctx.fc > 0) {
            add_matched_offs_multi(vec, ctx.bf, ctx.fc + 1);
            ctx.fc = 0;
        } else {
            vec.push_back(ctx.bf);
        }
        ctx.bf_nn = false;
    }

}

inline void rx_mem_scan::add_matched_offs_multi(matched_offs_t &vec, matched_off_t bi, matched_off_ct ic) {
    vec.push_back(rx_add_offc_mask(bi));
    vec.push_back(ic);
}

/**
 * 0 bi = 0, n = false, li = 0, ic = 0 []
 * 1 bi = 0, n = false, li = 1, ic = 1 []
 * 2 bi = 0, n = false, li = 2, ic = 2 [0, 3]
 * 4 bi = 4, n = false, li = 4, ic = 0 [0, 3]
 * 6 bi = 6, n = false, li = 6, ic = 0 [0, 3, 4]
 */
inline void rx_mem_scan::add_matched_off(matched_off_t matched_idx, matched_offs_context_t &ctx, matched_offs_t &vec) {
    if (ctx.bf_nn) {
        if ((matched_idx - ctx.lf) == _search_value_type_p->size_of_value()) {
            ctx.fc ++;
        } else {
            matched_offs_flush(ctx, vec);
            ctx.bf = matched_idx;
        }
    } else {
        ctx.bf = matched_idx;
    }

    ctx.bf_nn = true;
    ctx.lf = matched_idx;
}

inline kern_return_t rx_mem_scan::read_region(data_pt region_data, region_t &region, vm_size_t *read_count) {
    kern_return_t ret = vm_read_overwrite(_target_task,
            region.address,
            region.size,
            (vm_address_t) region_data,
            read_count);
    return ret;
}

void rx_mem_scan::init_regions() {
    struct proc_regioninfo region_info;
    kern_return_t ret;
    uint64_t addr = 0;
    int count = 0;

    _regions_p = new regions_t();
    do {
        if (addr) {
            boolean_t writable = (region_info.pri_protection & VM_PROT_DEFAULT) == VM_PROT_DEFAULT;

            if (writable) {
                region_t region;
                region.address = region_info.pri_address;
                region.size = region_info.pri_size;
                _regions_p->push_back(region);
                count ++;
            }
        }

        ret = proc_pidinfo(_target_pid, PROC_PIDREGIONINFO, addr, &region_info, sizeof(region_info));
        addr = region_info.pri_address + region_info.pri_size;
    } while (ret == sizeof(region_info));
    _trace("Writable region count: %d\n", (int)_regions_p->size());
}
