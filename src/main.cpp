#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ecc/ecc.hpp"
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
        FAIL_MODE_1BIT,
        FAIL_MODE_2BIT,
        FAIL_MODE_2BIT_BURST,
        FAIL_MODE_3BIT,
        FAIL_MODE_3BIT_BURST,
    };

    const FAIL_MODE fail_mode = FAIL_MODE_1BIT;

    const bool full_run = false;
    const uint64_t random_tests = 1;

    ECCMethod* method = new ECCMethod_Hamming();

    uint64_t seed = 42;

    ///////

    uint64_t rctr = 0; // random ctr

    const bool print_tests = !full_run && random_tests <= 10;

    struct ecc_stats {
        uint64_t detection_ok;
        uint64_t detection_corrected;
        uint64_t detection_uncorrectable;
    } stats = (ecc_stats){
        .detection_ok = 0,
        .detection_corrected = 0,
        .detection_uncorrectable = 0,
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
        //TODO
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
            switch (fail_mode) {
                case FAIL_MODE_NONE: {
                    //pass
                } break;
                case FAIL_MODE_1BIT: {
                    uint32_t total_positions = data.size() + ecc.size();
                    uint32_t flip_pos = noise_get_u64n(rctr++, seed, total_positions);
                    if (flip_pos < data.size()) {
                        data[flip_pos] = !data[flip_pos];
                    } else {
                        ecc[flip_pos - data.size()] = !ecc[flip_pos - data.size()];
                    }
                    if (print_tests) {
                        if (flip_pos >= data.size()) {
                            printf(" ");
                        }
                        for (uint32_t i = 0; i < flip_pos; i++) {
                            printf(" ");
                        }
                        printf("^");
                        printf("\n");
                    }
                } break;
                case FAIL_MODE_2BIT: {
                    //TODO
                    assert(0);
                    printf("unimplemented\n");
                    exit(-1);
                } break;
                case FAIL_MODE_2BIT_BURST: {
                    //TODO
                    assert(0);
                    printf("unimplemented\n");
                    exit(-1);
                } break;
                case FAIL_MODE_3BIT: {
                    //TODO
                    assert(0);
                    printf("unimplemented\n");
                    exit(-1);
                } break;
                case FAIL_MODE_3BIT_BURST: {
                    //TODO
                    assert(0);
                    printf("unimplemented\n");
                    exit(-1);
                } break;
                default: {
                    assert(0);
                    printf("invalid fail mode\n");
                    exit(-1);
                } break;
            }
            // check and correct
            ECC_DETECTION detection = method->CheckAndCorrect(data, ecc);
            switch (detection) {
                case ECC_DETECTION_OK: {
                    stats.detection_ok++;
                    if (print_tests) {
                        printf("detection: ok\n");
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
                        printf("correction failed\n");
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
            if (print_tests) {
                print_bits(data);
                printf(" ");
                print_bits(ecc);
                printf("\n");
            }
        }
    }

    if (print_tests) {
        printf("\n\n");
    }
    printf("stats:\n");
    printf("detection ok: %lu\n", stats.detection_ok);
    printf("detection corrected: %lu\n", stats.detection_corrected);
    printf("detection uncorrectable: %lu\n", stats.detection_uncorrectable);

    printf("done\n");
    return 0;
}
