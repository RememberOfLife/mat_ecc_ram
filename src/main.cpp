#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ecc/ecc.hpp"
#include "ecc/bch.hpp"
#include "ecc/hamming.hpp"

#include "util/noise.h"

void print_bits(std::vector<bool>& bits)
{
    for (const bool& b : bits) {
        printf("%c", b ? '1' : '0');
    }
}

int main()
{
    //TODO is the correction dependant on the actual data? if so, how to do a full run actually?

    enum FAIL_MODE {
        FAIL_MODE_NONE = 0,
        FAIL_MODE_RANDOM,
        FAIL_MODE_RANDOM_BURST,
    };

    const FAIL_MODE fail_mode = FAIL_MODE_RANDOM;
    const uint32_t fail_count = 2;

    const bool full_run = false;
    const uint64_t random_tests = 1;

    ECCMethod* method = new ECCMethod_BCH(128, 2);

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
        assert(0);
        printf("unimplemented\n");
        exit(-1);
    } else {
        for (uint64_t t = 0; t < random_tests; t++) {
            // rebuild ecc
            method->ConstructECC(data, ecc);
            data_check = data;
            ecc_check = ecc;
            if (print_tests) {
                printf("\n\n");
                printf("data ecc:\n");
                print_bits(data);
                printf(" ");
                print_bits(ecc);
                printf("\n");
            }
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
                    assert(0);
                    printf("invalid fail mode\n");
                    exit(-1);
                } break;
            }

            // flip the bits
            for (uint32_t flipping = 0; flipping < generated_bits; flipping++) {
                uint32_t flip_pos = fail_positions[flipping];
                if (flip_pos < data.size()) {
                    data[flip_pos] = !data[flip_pos];
                } else {
                    ecc[flip_pos - data.size()] = !ecc[flip_pos - data.size()];
                }
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

            // check and correct
            ECC_DETECTION detection = method->CheckAndCorrect(data, ecc);

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
                    assert(0);
                    printf("invalid detection\n");
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
