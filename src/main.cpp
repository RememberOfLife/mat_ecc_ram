#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ecc/ecc.hpp"
#include "ecc/bch.hpp"
#include "ecc/hamming.hpp"
#include "ecc/hsiao.hpp"

#include "util/noise.h"

void print_bits(std::vector<bool>& bits)
{
    for (const bool& b : bits) {
        printf("%c", b ? '1' : '0');
    }
}

int main()
{
    enum FAIL_MODE {
        FAIL_MODE_NONE = 0,
        FAIL_MODE_RANDOM,
        FAIL_MODE_RANDOM_BURST,
    };

    const FAIL_MODE fail_mode = FAIL_MODE_RANDOM;
    const uint32_t fail_count = 1;

    const bool full_run = false;
    const uint64_t random_tests = 10000;

    // ECCMethod* method = new ECCMethod_Hamming();
    // ECCMethod* method = new ECCMethod_BCH(32, 5);
    ECCMethod* method = new ECCMethod_Hsiao(128, 16);

    uint64_t seed = 42;

    ///////

    assert(fail_count <= 6);

    uint64_t rctr = 0; // random ctr

    const bool print_tests = !full_run && random_tests <= 10;

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

    printf("datawidth: %u ; eccwidth: %u\n", method->DataWidth(), method->ECCWidth());

    std::vector<bool> data;
    data.resize(method->DataWidth());
    std::vector<bool> ecc;
    ecc.resize(method->ECCWidth());

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

    if (full_run) {
        //TODO full run actually required?
        printf("unimplemented\n");
        assert(0);
        exit(-1);
    } else {
        for (uint64_t t = 0; t < random_tests; t++) {
            if (print_tests) {
                printf("\n\n");
            }
            // rebuild ecc
            method->ConstructECC(data, ecc);
            data_check = data;
            ecc_check = ecc;
            // inject bit faults
            uint32_t fail_positions[fail_count];
            uint32_t total_positions = data.size() + ecc.size();
            uint32_t generated_bits = 0;

            switch (fail_mode) {
                case FAIL_MODE_NONE: {
                    //pass
                } break;
                case FAIL_MODE_RANDOM: {
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
                } break;
                case FAIL_MODE_RANDOM_BURST: {
                    total_positions -= fail_count - 1;
                    uint32_t flip_pos = noise_get_u64n(rctr++, seed, total_positions);
                    while (generated_bits < fail_count) {
                        fail_positions[generated_bits] = flip_pos + generated_bits;
                        generated_bits++;
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
                    for (uint32_t i = 0; i < method->DataWidth(); i++) {
                        correct_correction &= data_check[i] == data[i];
                    }
                    for (uint32_t i = 0; i < method->ECCWidth(); i++) {
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
    }

    if (print_tests) {
        printf("\n\n");
    }
    printf("stats:\n");
    printf("detection ok: %lu\n", stats.detection_ok);
    printf("detection corrected (false corrections therein): %lu (%lu)\n", stats.detection_corrected, stats.false_corrections);
    printf("detection uncorrectable: %lu\n", stats.detection_uncorrectable);

    printf("done\n");
    return 0;
}
