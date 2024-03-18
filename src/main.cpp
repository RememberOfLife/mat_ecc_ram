#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#include "ecc/ecc.hpp"
#include "ecc/bch.hpp"
#include "ecc/hamming.hpp"
#include "ecc/hsiao.hpp"

#include "util/noise.h"

static void errorf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(-1);
}

void print_bits(std::vector<bool>& bits)
{
    for (const bool& b : bits) {
        printf("%c", b ? '1' : '0');
    }
}

uint64_t nCr(uint64_t n, uint64_t r)
{
    if (r == 0) {
        return 1;
    } else {
        uint64_t num = n * nCr(n - 1, r - 1);
        return num / r;
    }
}

static const size_t SPACED_U64_MAX_STR_SIZE = 27;

void pre_format_spaced_u64(char* buf, uint64_t n, char space)
{
    char temp_buf[32];
    int temp_len = sprintf(temp_buf, "%lu", n);

    int spaces = (temp_len - 1) / 3;
    int out_len = temp_len + spaces;

    char* last_temp = temp_buf + temp_len - 1;
    char* last_buf = buf + out_len;
    *last_buf-- = '\0';

    int ctr = 0;
    while (last_temp >= temp_buf) {
        if (ctr == 3) {
            *last_buf-- = space;
            ctr = 0;
        }
        *last_buf-- = *last_temp--;
        ctr++;
    }
}

std::array<uint16_t, 8> bit_position_enumeration_idx_ncr(uint64_t n, uint64_t r, uint64_t idx)
{
    std::array<uint16_t, 8> ret;
    ret.fill(UINT16_MAX);
    size_t ret_fill = 0;
    uint64_t n_remaining = n;
    uint64_t r_remaining = r;
    uint64_t enumeration = idx;
    while (r_remaining > 1) {
        uint64_t bit_block = nCr(n_remaining - 1, r_remaining - 1);
        if (enumeration < bit_block) {
            ret[ret_fill++] = n - n_remaining;
            r_remaining--;
        } else {
            enumeration -= bit_block;
        }
        n_remaining--;
    }
    ret[ret_fill++] = n - n_remaining + enumeration;
    return ret;
}

std::array<uint16_t, 8> bit_position_enumeration_idx_burst(uint64_t n, uint64_t r, uint64_t idx)
{

    std::array<uint16_t, 8> ret;
    ret.fill(UINT16_MAX);
    for (size_t ret_idx = 0; ret_idx < r; ret_idx++) {
        ret[ret_idx] = n - r + 1 + idx + ret_idx;
    }
    return ret;
}

int main(int argc, char** argv)
{
    // the settings we will set with clas
    enum FAIL_MODE {
        FAIL_MODE_NONE = 0,
        FAIL_MODE_RANDOM,
        FAIL_MODE_RANDOM_BURST,
    };

    bool full_run = false;
    FAIL_MODE fail_mode;
    uint32_t fail_count;
    uint64_t test_count;
    ECCMethod* method;
    uint64_t seed = 42;

    // parse clas
    if (argc < 6) {
        errorf("usage: <fail_mode> <fail_count> <test_count> <ecc_method> <ecc_conf> [seed]\n");
    }

    const char* arg_fail_mode = argv[1];
    const char* arg_fail_count = argv[2];
    const char* arg_test_count = argv[3];
    const char* arg_ecc_method = argv[4];
    const char* arg_ecc_conf = argv[5];
    const char* arg_seed = argc > 6 ? argv[6] : NULL;
    bool debug_print = argc > 7;

    if (strcmp(arg_fail_mode, "N") == 0) {
        fail_mode = FAIL_MODE_NONE;
    } else if (strcmp(arg_fail_mode, "R") == 0) {
        fail_mode = FAIL_MODE_RANDOM;
    } else if (strcmp(arg_fail_mode, "RB") == 0) {
        fail_mode = FAIL_MODE_RANDOM_BURST;
    } else {
        errorf("unknown fail mode\n");
    }

    fail_count = strtoul(arg_fail_count, NULL, 10);

    {
        int d;
        int k;
        int ec = sscanf(arg_ecc_conf, "%i/%i", &d, &k);
        if (ec != 2) {
            errorf("failed to read ecc conf\n");
        }
        if (strcmp(arg_ecc_method, "hamming") == 0) {
            method = new ECCMethod_Hamming();
        } else if (strcmp(arg_ecc_method, "bch") == 0) {
            method = new ECCMethod_BCH(d, k);
        } else if (strcmp(arg_ecc_method, "hsiao") == 0) {
            method = new ECCMethod_Hsiao(d, k, debug_print);
        } else {
            errorf("unknown ecc method\n");
        }
    }

    const uint32_t data_with = method->DataWidth();
    const uint32_t ecc_width = method->ECCWidth();
    const uint32_t word_width = data_with + ecc_width;

    full_run = strcmp(arg_test_count, "F") == 0;
    if (full_run) {
        test_count = nCr(word_width, fail_count);
    } else {
        test_count = strtoul(arg_test_count, NULL, 10);
    }

    srand(time(NULL)); // quick and dirty randomness if no seed given
    seed = arg_seed == NULL ? rand() : strtoull(arg_seed, NULL, 10);

    // program begins
    ///////

    assert(fail_count <= 8);

    uint64_t rctr = 0; // random ctr

    const bool print_tests = !full_run && test_count <= 10;

    struct ecc_stats {
        uint64_t detection_ok;
        uint64_t detection_corrected;
        uint64_t detection_uncorrectable;
        uint64_t false_corrections;
    } stats = (ecc_stats){
        .detection_ok = 0,
        .detection_corrected = 0,
        .detection_uncorrectable = 0,
        .false_corrections = 0,
    };

    printf("datawidth: %u ; eccwidth: %u\n", data_with, ecc_width);

    if (full_run) {
        char testcount_str[SPACED_U64_MAX_STR_SIZE];
        pre_format_spaced_u64(testcount_str, test_count, ' ');
        printf("full run: %s tests\n", testcount_str);
    }

    std::vector<bool> data;
    data.resize(data_with);
    std::vector<bool> ecc;
    ecc.resize(ecc_width);

    std::vector<bool> data_check = data;
    std::vector<bool> ecc_check = ecc;

    // randomize initial data
    for (uint32_t i = 0; i < data.size(); i++) {
        data[i] = squirrelnoise5_u64(rctr++, seed) & 0b1;
    }
    // zero ecc
    for (uint32_t i = 0; i < data.size(); i++) {
        ecc[i] = 0;
    }

    for (uint64_t t = 0; t < test_count; t++) {
        if (print_tests) {
            printf("\n\n");
        } else if ((t & UINT16_MAX) == 0) {
            printf("\rprogress: %.5f", (float)t / (float)test_count);
            fflush(stdout);
        }
        // rebuild ecc
        method->ConstructECC(data, ecc);
        data_check = data;
        ecc_check = ecc;
        // inject bit faults
        uint32_t fail_positions[fail_count];
        uint32_t total_positions = word_width;
        uint32_t generated_bits = 0;

        switch (fail_mode) {
            case FAIL_MODE_NONE: {
                //pass
            } break;
            case FAIL_MODE_RANDOM: {
                if (full_run) {
                    std::array<uint16_t, 8> bit_positions = bit_position_enumeration_idx_ncr(word_width, fail_count, t);
                    for (; generated_bits < fail_count; generated_bits++) {
                        fail_positions[generated_bits] = bit_positions[generated_bits];
                    }
                } else {
                    while (generated_bits < fail_count) {
                        uint32_t flip_pos = noise_get_u64n(rctr++, seed, total_positions);
                        bool unique = true;
                        for (uint32_t test_bit = 0; test_bit < generated_bits; test_bit++) {
                            if (fail_positions[test_bit] == flip_pos) {
                                unique = false;
                            }
                        }
                        if (!unique) {
                            continue;
                        }
                        fail_positions[generated_bits++] = flip_pos;
                    }
                }
            } break;
            case FAIL_MODE_RANDOM_BURST: {
                if (full_run) {
                    std::array<uint16_t, 8> bit_positions = bit_position_enumeration_idx_burst(word_width, fail_count, t);
                    for (; generated_bits < fail_count; generated_bits++) {
                        fail_positions[generated_bits] = bit_positions[generated_bits];
                    }
                } else {
                    total_positions -= fail_count - 1;
                    uint32_t flip_pos = noise_get_u64n(rctr++, seed, total_positions);
                    while (generated_bits < fail_count) {
                        fail_positions[generated_bits] = flip_pos + generated_bits;
                        generated_bits++;
                    }
                }
            } break;
            default: {
                printf("invalid fail mode\n");
                assert(0);
                exit(-1);
            } break;
        }

        // flip the bits
        if (print_tests && generated_bits > 0) {
            printf("injecting %u error%s at:", generated_bits, generated_bits > 1 ? "s" : "");
        }
        for (uint32_t flipping = 0; flipping < generated_bits; flipping++) {
            uint32_t flip_pos = fail_positions[flipping];
            if (flip_pos < data.size()) {
                data[flip_pos] = !data[flip_pos];
            } else {
                ecc[flip_pos - data.size()] = !ecc[flip_pos - data.size()];
            }
            if (print_tests && generated_bits > 0) {
                printf(" %u", flip_pos);
                if (flipping + 1 < generated_bits) {
                    printf(",");
                }
            }
        }
        if (print_tests && generated_bits > 0) {
            printf("\n");
        }

        // print original data and ecc
        if (print_tests) {
            print_bits(data_check);
            printf(" ");
            print_bits(ecc_check);
            printf("\n");
        }

        // print flips if wanted
        for (uint32_t bit_pos = 0; bit_pos < (data.size() + ecc.size()); bit_pos++) {
            if (print_tests && bit_pos == data.size()) {
                printf(" ");
            }
            bool found = false;
            for (uint32_t test_bit = 0; test_bit < generated_bits; test_bit++) {
                if (fail_positions[test_bit] == bit_pos) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (print_tests) {
                    printf("-");
                }
                continue;
            }
            if (print_tests) {
                printf("|");
            }
        }
        if (print_tests) {
            printf("\n");
        }

        std::vector<bool> data_fault = data;
        std::vector<bool> ecc_fault = ecc;
        // print with errors
        if (print_tests) {
            print_bits(data);
            printf(" ");
            print_bits(ecc);
            printf("\n");
        }

        // check and correct
        ECC_DETECTION detection = method->CheckAndCorrect(data, ecc);

        // print correction flips if wanted
        for (uint32_t bit_pos = 0; bit_pos < (data_fault.size() + ecc_fault.size()); bit_pos++) {
            if (print_tests && bit_pos == data_fault.size()) {
                printf(" ");
            }
            bool flipped = false;
            if (bit_pos < data_fault.size()) {
                flipped = data[bit_pos] != data_fault[bit_pos];
            } else {
                flipped = ecc[bit_pos - data_fault.size()] != ecc_fault[bit_pos - data_fault.size()];
            }
            if (!flipped) {
                if (print_tests) {
                    printf("-");
                }
                continue;
            }
            if (print_tests) {
                printf("|");
            }
        }
        if (print_tests) {
            printf("\n");
        }

        // print result
        if (print_tests) {
            print_bits(data);
            printf(" ");
            print_bits(ecc);
            printf("\n");
        }

        // print detection result
        switch (detection) {
            case ECC_DETECTION_OK: {
                stats.detection_ok++;
                if (print_tests) {
                    printf("detection: ok\n");
                    if (fail_mode != FAIL_MODE_NONE && fail_count > 0) {
                        printf("completely silent corruption\n");
                    }
                }
            } break;
            case ECC_DETECTION_CORRECTED: {
                stats.detection_corrected++;
                if (print_tests) {
                    printf("detection: corrected\n");
                }
                bool correct_correction = true;
                for (uint32_t i = 0; i < data_with; i++) {
                    correct_correction &= data_check[i] == data[i];
                }
                for (uint32_t i = 0; i < ecc_width; i++) {
                    correct_correction &= ecc_check[i] == ecc[i];
                }
                if (!correct_correction) {
                    stats.false_corrections++;
                    if (print_tests) {
                        printf("correction failed\n");
                    }
                }
            } break;
            case ECC_DETECTION_UNCORRECTABLE: {
                stats.detection_uncorrectable++;
                if (print_tests) {
                    printf("detection: uncorrectable\n");
                }
            } break;
            default: {
                printf("invalid detection\n");
                assert(0);
                exit(-1);
            } break;
        }
    }

    if (print_tests) {
        printf("\n\n");
    } else {
        printf("\rprogress: 1.00\n");
    }
    printf("stats:\n");
    printf("detection ok%s: %lu\n", fail_count == 0 ? "" : " (sdcs)", stats.detection_ok);
    printf("detection corrected (false corrections therein): %lu (%lu)\n", stats.detection_corrected, stats.false_corrections);
    printf("detection uncorrectable: %lu\n", stats.detection_uncorrectable);

    printf("done\n");
    return 0;
}
