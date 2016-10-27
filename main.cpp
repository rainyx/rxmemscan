//
// Created by rainyx on 16/8/26.
// Copyright (c) 2016 rainyx. All rights reserved.
//

#include <math.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>
#include "rx_mem_scan.h"

#define tty_prefix "# "

typedef enum {
    kRXValueTypeUInt8,
    kRXValueTypeUInt16,
    kRXValueTypeUInt32,
    kRXValueTypeUInt64,
    kRXValueTypeSInt8,
    kRXValueTypeSInt16,
    kRXValueTypeSInt32,
    kRXValueTypeSInt64,
    kRXValueTypeFloat32,
    kRXValueTypeFloat64
} _vt ;

#define _vtcount  10
static const char * _vtdescs[_vtcount * 2] = {
        "uint8",    "unsigned 1 byte value",
        "uint16",   "unsigned 2 bytes value",
        "uint32",   "unsigned 4 bytes value",
        "uint64",   "unsigned 8 bytes value",
        "int8",     "signed 1 byte value",
        "int16",    "signed 2 bytes value",
        "int32",    "signed 4 bytes value",
        "int64",    "signed 8 bytes value",
        "float32",  "4 bytes float value",
        "float64",  "8 bytes float value"
};
#define _vtname(idx) (_vtdescs[idx*2])
#define _vtdesc(idx) (_vtdescs[idx*2+1])

static char _addfmtbuf[19] = { 0 };
#ifdef __LP64__
#define _addrstr(v) ({ sprintf(_addfmtbuf, "0x%016llx", (long long) (v)); _addfmtbuf; })
#else
#define _addrstr(v) ({ sprintf(_addfmtbuf, "0x%08lx", (long) (v)); _addfmtbuf; })
#endif

#define _hasarg(i, c) ((c).size() > (i))
#define _checkarg(i, c, m) ({ \
    bool _value = _hasarg(i, c); \
    do { \
        if (!_value) { \
            printf("Missing argument \"%s\" at %d\n", m, i); \
        } \
    } while (false); \
    _value; \
})

#define _print_search_result(r) (printf("Found %d result(s), memory used: %.4fMB time used: %.3fs\n", r.matched, (float)r.memory_used / (float)_1MB, (float) r.time_used / 1000.0f))
#define _print_line_sep (printf("--------------------------------------------------------------\n"))
std::vector<std::string> explode(std::string const &s, char delim)  {
    std::vector<std::string> result;
    std::istringstream iss(s);
    for (std::string s; std::getline(iss, s, delim); ) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                std::not1(std::ptr_fun<int, int>(std::isspace))));
        s.erase(std::find_if(s.rbegin(), s.rend(),
                std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        if (s.length() > 0) {
            result.push_back(s);
        }
    }
    return result;
}

std::string str_toupper(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

bool ask(std::string q) {
    std::string msg = q + "[Y]es or [N]o : ";
    char *line = readline(msg.c_str());
    std::string answer(line);
    free(line);
    if (answer == "Y" || answer == "y" || answer == "yes" || answer == "YES" || answer == "Yes") {
        return true;
    }

    return false;
}
void print_welcome() {
    _print_line_sep;
    printf("%s (%s) - by %s\n", RX_NAME, RX_VERSION, RX_COPYRIGHT);
    _print_line_sep;
}

void print_usage() {
    printf("rxmemscan needs run with target pid.\n");
    printf("Example: rxmemscan 911.\n");
}

void print_help() {
    printf("- list or l\n");
    printf("  List first page of addresses and enter page mode.\n");
    printf("  In page mode, press [enter] to go to next page,\n");
    printf("  press [r] to refresh current page,\n");
    printf("  press [q] to exit page mode.\n\n");

    printf("- search or s\n");
    printf("  args: search_value\n");
    printf("  Common search by value.\n\n");

    printf("- fuzzy or f\n");
    printf("  args: compare_type [LT/GT/EQ/NE]\n");
    printf("  Fuzzy search by compare type.\n\n");

    printf("- reset or r\n");
    printf("  Reset the search engine.\n\n");

    printf("- exit\n");
    printf("  Exit the program.\n\n");
}

vm_address_t str_to_address(std::string &str) {

    vm_address_t address;
    std::stringstream ss;
    if (str.find("0x") == 0 || str.find("0X") == 0) {
        ss << std::hex << str;
    } else {
        ss << std::dec << str;
    }

    ss >> address;

    return address;

}

template <typename T>
T _cast(std::string &str) {
    std::stringstream ss;
    if (str.find("0x") == 0 || str.find("0X") == 0) {
        ss << std::hex << str;
    } else {
        ss << std::dec << str;
    }

    T v;
    ss >> v;
    return v;

}

std::string _readline_str(const char *prefix) {
    char *line = readline(prefix);
    std::string line_str(line);
    free(line);
    return line_str;
}


static rx_mem_scan *g_engine = NULL;

template <typename T>
int tmain(int argc, char **argv, char **envp) {

    typedef T search_val_t;

    pid_t pid = atoi(argv[1]);
    rx_search_value_type *value_type = new rx_search_typed_value_type<T>();
    g_engine->set_search_value_type(value_type);

    std::string last_line;
    while (true) {

        std::string str_line = _readline_str(tty_prefix);
        if (str_line != last_line) {
            add_history(str_line.c_str());
        }
        last_line = str_line;

        std::vector<std::string> args = explode(str_line, ' ');
        std::string command = args[0];

        if (command == "exit") {
            break;
        } else if (command == "retype") {
            if (ask("Are you sure?")) {
                printf("Re-select the search value type.\n");
                return 1;
            } else {
                continue;
            }
        } else if (command == "search" || command == "s") {
            if (_checkarg(1, args, "search value")) {
                printf("Begin search...\n");
                search_val_t search_val = _cast<search_val_t>(args[1]);
                search_result_t result = g_engine->search(&search_val, rx_compare_type_eq);
                _print_search_result(result);
            }
        } else if (command == "fuzzy" || command == "f") {

            if (g_engine->is_idle()) {

                printf("Begin first fuzzy search...\n");
                search_result_t result = g_engine->first_fuzzy_search();
                _print_search_result(result);

            } else {

                if (_checkarg(1, args, "comparator type")) {
                    std::string ct_str = str_toupper(args[1]);
                    rx_compare_type ct;
                    if (ct_str == "GT") {
                        ct = rx_compare_type_gt;
                    } else if (ct_str == "LT") {
                        ct = rx_compare_type_lt;
                    } else if (ct_str == "EQ") {
                        ct = rx_compare_type_eq;
                    } else if (ct_str == "NE") {
                        ct = rx_compare_type_ne;
                    } else {
                        printf("Unknown comparator type \"%s\"\n", ct_str.c_str());
                        continue;
                    }

                    printf("Begin fuzzy search...\n");

                    search_result_t result = g_engine->fuzzy_search(ct);
                    _print_search_result(result);
                }

            }
        } else if (command == "reset" || command == "r") {
            if (g_engine->is_idle() ||
                    ask("Are you sure reset the engine state? This operation will be discard all search result.")) {
                printf("Search engine has reset.\n");
                g_engine->reset();
            }
        } else if (command == "list" || command == "l") {

            if (g_engine->is_idle() || g_engine->last_search_result().matched == 0) {
                printf("Matched list is empty.\n");
            } else {

                uint32_t page_size = 20;
                uint32_t page_no = 0;
                uint32_t total_page = ceil((float_t) g_engine->last_search_result().matched / (float_t) page_size);

                if (_hasarg(1, args)) {
                    page_no = atoi(args[1].c_str());
                    if (page_no > total_page) {
                        printf("Invalid argument \"page_no\" %u, max is %u.\n", page_no, total_page);
                        continue;
                    }
                    if (page_no > 0) page_no --;
                }

                uint32_t b = page_size * page_no;

                auto page = g_engine->page_of_matched(page_no, page_size);
                printf("List page [%u/%u] of matched.\n", page_no+1, total_page);
                printf("---------------------------------------------------------\n");
#ifdef __LP64__
                printf("| idx\t | address\t\t | value\n");
#else
                printf("| idx\t | address\t | value\n");
#endif
                printf("---------------------------------------------------------\n");
                for (uint32_t i = 0; i < page->addresses->size(); i++) {
                    vm_address_t address = (*page->addresses)[i];
                    search_val_t *value = (search_val_t *) (page->data + (i * value_type->size_of_value()));
                    std::cout << "| " << (b+i+1) << "\t | " << _addrstr(address) << "\t | " << *value << std::endl;
                }
                printf("---------------------------------------------------------\n");

                delete page;

            }

        } else if (command == "write" || command == "w") {
            if (_checkarg(1, args, "address") && _checkarg(2, args, "value")) {
                vm_address_t address = str_to_address(args[1]);
                search_val_t new_value = _cast<search_val_t>(args[2]);
                kern_return_t ret = g_engine->write_val(address, &new_value);
                std::cout << "Write value " << new_value << "at address " << _addrstr(address) << " ";
                if (ret == KERN_SUCCESS) {
                    printf("successfully.\n");
                } else {
                    printf("failed (%d: %s).\n", ret, mach_error_string(ret));
                }
            }
        } else if (command == "help" || command == "h") {
            print_help();
        } else {
            printf("Unknown command \"%s\" type \"h\" to help.\n", command.c_str());
        }

    }

    return 0;
}


int main(int argc, char **argv, char **envp) {

    print_welcome();

    if (argc == 1) {
        print_usage();
        return 0;
    }

    pid_t pid = atoi(argv[1]);
    g_engine = new rx_mem_scan();

    if (g_engine->attach(pid)) {
        printf("Attach to pid(%d) failed.\n", pid);
        return 0;
    }

    while (true) {
        g_engine->reset();

        printf("Type [x] to select search value type.\n");
        for (int i = 0; i< _vtcount; ++i) {
            printf("[%d] %s\t(%s)\n", i, _vtname(i), _vtdesc(i));
        }

        std::string line_str = _readline_str(tty_prefix);
        if (line_str.length() == 0) {
            continue;
        }
        _vt t = (_vt)_cast<int>(line_str);

        if (!(t >= 0 && t < _vtcount)) {
            printf("Invalid type.\n");
            continue;
        } else {
            printf("Selected search type %s.\n", _vtdescs[t * 2]);
        }

        int ret_val = 0;
        switch (t) {
            case kRXValueTypeUInt8:     ret_val = tmain<uint8_t>(argc, argv, envp);     break;
            case kRXValueTypeUInt16:    ret_val = tmain<uint16_t>(argc, argv, envp);    break;
            case kRXValueTypeUInt32:    ret_val = tmain<uint32_t>(argc, argv, envp);    break;
            case kRXValueTypeUInt64:    ret_val = tmain<uint64_t>(argc, argv, envp);    break;
            case kRXValueTypeSInt8:     ret_val = tmain<int8_t>(argc, argv, envp);      break;
            case kRXValueTypeSInt16:    ret_val = tmain<int16_t>(argc, argv, envp);     break;
            case kRXValueTypeSInt32:    ret_val = tmain<int32_t>(argc, argv, envp);     break;
            case kRXValueTypeSInt64:    ret_val = tmain<int64_t>(argc, argv, envp);     break;
            case kRXValueTypeFloat32:   ret_val = tmain<float_t>(argc, argv, envp);     break;
            case kRXValueTypeFloat64:   ret_val = tmain<double_t>(argc, argv, envp);    break;
            default: break;
        }

        if (ret_val == 0) {
            printf("Bye!\n");
            break;
        }

    }

    return 0;
}
