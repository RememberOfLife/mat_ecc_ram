#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
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

struct ecc_stats {
    uint64_t detection_ok = 0;
    uint64_t detection_corrected = 0;
    uint64_t detection_uncorrectable = 0;
    uint64_t false_corrections = 0;
};

enum FAIL_MODE {
    FAIL_MODE_NONE = 0,
    FAIL_MODE_RANDOM,
    FAIL_MODE_RANDOM_BURST,
};

struct thread_control {
    pthread_t pthread_id;
    bool full_run;
    FAIL_MODE fail_mode;
    uint32_t fail_count;
    uint64_t rng_seed;
    ECCMethod* method;
    uint64_t work_offset;
    uint64_t work_progress;
    uint64_t work_max;
    ecc_stats stats;
    std::vector<uint64_t> flip_occurence_counts;
    std::vector<int64_t> flip_occurence_flip_avg_distances;
};

void* thread_work(void* arg)
{
    thread_control& ctrl = *(thread_control*)arg;

    uint64_t rctr = 0;

    const bool print_tests = !ctrl.full_run && (ctrl.work_max - ctrl.work_offset) <= 10;

    uint32_t data_width = ctrl.method->DataWidth();
    uint32_t ecc_width = ctrl.method->ECCWidth();
    uint32_t word_width = data_width + ecc_width;

    ctrl.flip_occurence_counts.resize(word_width, 0);
    ctrl.flip_occurence_flip_avg_distances.resize(word_width, 0);

    std::vector<bool> data;
    data.resize(data_width);
    std::vector<bool> ecc;
    ecc.resize(ecc_width);

    std::vector<bool> data_check = data;
    std::vector<bool> ecc_check = ecc;

    // randomize initial data
    for (uint32_t i = 0; i < data.size(); i++) {
        data[i] = squirrelnoise5_u64(rctr++, ctrl.rng_seed) & 0b1;
    }
    // zero ecc
    for (uint32_t i = 0; i < data.size(); i++) {
        ecc[i] = 0;
    }

    for (uint64_t t = 0; ctrl.work_offset + t < ctrl.work_max; t++) {
        uint64_t effective_bp_idx = ctrl.work_offset + t;
        if (print_tests) {
            printf("\n\n");
        } else if ((t & UINT16_MAX) == 0) {
            ctrl.work_progress = t;
        }
        // rebuild ecc
        ctrl.method->ConstructECC(data, ecc);
        data_check = data;
        ecc_check = ecc;
        // inject bit faults
        uint32_t fail_positions[ctrl.fail_count];
        uint32_t total_positions = word_width;
        uint32_t generated_bits = 0;

        switch (ctrl.fail_mode) {
            case FAIL_MODE_NONE: {
                //pass
            } break;
            case FAIL_MODE_RANDOM: {
                if (ctrl.full_run) {
                    std::array<uint16_t, 8> bit_positions = bit_position_enumeration_idx_ncr(word_width, ctrl.fail_count, effective_bp_idx);
                    for (; generated_bits < ctrl.fail_count; generated_bits++) {
                        fail_positions[generated_bits] = bit_positions[generated_bits];
                    }
                } else {
                    while (generated_bits < ctrl.fail_count) {
                        uint32_t flip_pos = noise_get_u64n(rctr++, ctrl.rng_seed, total_positions);
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
                if (ctrl.full_run) {
                    std::array<uint16_t, 8> bit_positions = bit_position_enumeration_idx_burst(word_width, ctrl.fail_count, effective_bp_idx);
                    for (; generated_bits < ctrl.fail_count; generated_bits++) {
                        fail_positions[generated_bits] = bit_positions[generated_bits];
                    }
                } else {
                    total_positions -= ctrl.fail_count - 1;
                    uint32_t flip_pos = noise_get_u64n(rctr++, ctrl.rng_seed, total_positions);
                    while (generated_bits < ctrl.fail_count) {
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
        ECC_DETECTION detection = ctrl.method->CheckAndCorrect(data, ecc);

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
            // add post fault occurence and avg flip distance
            ctrl.flip_occurence_counts[bit_pos]++;
            for (uint32_t fault_idx = 0; fault_idx < generated_bits; fault_idx++) {
                ctrl.flip_occurence_flip_avg_distances[bit_pos] += (int64_t)bit_pos - (int64_t)fail_positions[fault_idx];
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
                ctrl.stats.detection_ok++;
                if (print_tests) {
                    printf("detection: ok\n");
                    if (ctrl.fail_mode != FAIL_MODE_NONE && ctrl.fail_count > 0) {
                        printf("completely silent corruption\n");
                    }
                }
            } break;
            case ECC_DETECTION_CORRECTED: {
                ctrl.stats.detection_corrected++;
                if (print_tests) {
                    printf("detection: corrected\n");
                }
                bool correct_correction = true;
                for (uint32_t i = 0; i < data_width; i++) {
                    correct_correction &= data_check[i] == data[i];
                }
                for (uint32_t i = 0; i < ecc_width; i++) {
                    correct_correction &= ecc_check[i] == ecc[i];
                }
                if (!correct_correction) {
                    ctrl.stats.false_corrections++;
                    if (print_tests) {
                        printf("correction failed\n");
                    }
                }
            } break;
            case ECC_DETECTION_UNCORRECTABLE: {
                ctrl.stats.detection_uncorrectable++;
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

    ctrl.work_progress = ctrl.work_max - ctrl.work_offset;

    pthread_exit(NULL);
}

void test_bit_enumeration_idx()
{
    struct ArrayHash {
        std::size_t operator()(const std::array<uint16_t, 8>& t) const
        {
            uint64_t* dp = (uint64_t*)t.data();
            return dp[0] ^ dp[1];
        }
    };

    struct ArrayEq {
        bool operator()(const std::array<uint16_t, 8>& lhs, const std::array<uint16_t, 8>& rhs) const
        {
            return memcmp(lhs.data(), rhs.data(), sizeof(uint16_t) * 8) == 0;
        }
    };

    std::unordered_set<std::array<uint16_t, 8>, ArrayHash, ArrayEq> generated_faults;
    uint64_t n = 6;
    uint64_t r = 3;
    uint64_t calc_ncr = nCr(n, r);
    for (uint64_t idx = 0; idx < calc_ncr; idx++) {
        if (!generated_faults.insert(bit_position_enumeration_idx_ncr(n, r, idx)).second) {
            errorf("duplicate insertion\n");
        }
    }
    printf("%lu of %lu entries created\n", generated_faults.size(), calc_ncr);
    if (generated_faults.size() != calc_ncr) {
        errorf("mismatch\n");
    }
    std::unordered_set<std::array<uint16_t, 8>, ArrayHash, ArrayEq>::iterator gf_iter = generated_faults.begin();
    for (; gf_iter != generated_faults.end(); gf_iter++) {
        for (size_t i = 0; i < r; i++) {
            if ((*gf_iter)[i] > n - 1) {
                errorf("out of range bit idx found\n");
            }
            for (size_t j = 0; j < r; j++) {
                if (i == j) {
                    continue;
                }
                if ((*gf_iter)[i] == (*gf_iter)[j]) {
                    errorf("duplicate bit idx found\n");
                }
            }
        }
    }
    {
        // initialize placement
        std::vector<uint16_t> idx_placer;
        for (size_t p = 0; p < r; p++) {
            idx_placer.push_back(p);
        }
        while (true) {
            // use placement here
            std::array<uint16_t, 8> check;
            check.fill(UINT16_MAX);
            for (size_t pi = 0; pi < r; pi++) {
                check[pi] = idx_placer[pi];
            }
            bool not_found = generated_faults.find(check) == generated_faults.end();
            if (not_found) {
                errorf("missing combination\n");
            }
            // increment placement
            ssize_t placer_idx = idx_placer.size() - 1;
            idx_placer[placer_idx] = idx_placer[placer_idx] + 1;
            if (idx_placer[placer_idx] < n) {
                continue; // valid generation, go on
            }
            // overstepped, need to reset and increment previous placers
            while (true) {
                placer_idx--;
                if (placer_idx < 0) {
                    break;
                }
                idx_placer[placer_idx]++;
                if (idx_placer[placer_idx] < n && n - idx_placer[placer_idx] >= r - placer_idx) {
                    break; // valid generation for previous, go on and reset
                }
                // previous overstepped too, go back one more
            }
            if (placer_idx < 0) {
                break;
            }
            // reset all placers later than the current one
            placer_idx++;
            while (placer_idx < idx_placer.size()) {
                idx_placer[placer_idx] = idx_placer[placer_idx - 1] + 1;
                placer_idx++;
            }
        }
    }
    // some test elements, unordered so may look
    gf_iter = generated_faults.begin();
    for (size_t i = 0; i < 10; i++) {
        printf("[%zu]:", i);
        for (size_t j = 0; j < r; j++) {
            printf(" %hu", (*gf_iter)[j]);
        }
        printf("\n");
        gf_iter++;
    }
}

struct inject_simple_result {
    ECC_DETECTION det_result;
    uint32_t miscorrection_location;
};

inject_simple_result inject_with_idx_and_get_result(ECCMethod* method, std::vector<bool>& data, std::vector<bool>& ecc, uint64_t n, uint64_t r, uint64_t i)
{
    method->ConstructECC(data, ecc);
    std::vector<bool> check_data = data;
    std::vector<bool> check_ecc = ecc;
    // inject
    std::array<uint16_t, 8> injection_positions = bit_position_enumeration_idx_ncr(n, r, i);
    for (size_t doit = 0; doit < r; doit++) {
        uint16_t pos = injection_positions[doit];
        if (pos < data.size()) {
            data[pos] = !data[pos];
        } else {
            pos -= data.size();
            ecc[pos] = !ecc[pos];
        }
    }
    // check
    ECC_DETECTION res = method->CheckAndCorrect(data, ecc);
    if (res != ECC_DETECTION_CORRECTED) {
        return {.det_result = res};
    }
    // find miscorrection location
    for (size_t l = 0; l < n; l++) {
        size_t pos = l;
        bool got;
        bool want;
        if (pos < data.size()) {
            got = data[pos];
            want = check_data[pos];
        } else {
            pos -= data.size();
            got = ecc[pos];
            want = check_ecc[pos];
        }
        if (got != want && std::find(injection_positions.begin(), injection_positions.end(), l) != injection_positions.end()) {
            return {.det_result = res, .miscorrection_location = (uint32_t)(l)};
        }
    }
    assert(false); // should not really happen I think?
    errorf("possible\n");
}

void test_materialization_data_independence()
{
    ECCMethod_Hsiao ecc(64, 8);
    uint32_t data_width = ecc.DataWidth();
    uint32_t ecc_width = ecc.ECCWidth();
    uint32_t word_width = data_width + ecc_width;

    std::vector<bool> vec_data;
    vec_data.resize(data_width);
    std::vector<bool> vec_ecc;
    vec_ecc.resize(ecc_width);

    const uint32_t fault_count = 3;
    const uint64_t bit_combs = nCr(word_width, fault_count);

    for (size_t injection_ctr = 0; injection_ctr < 100; injection_ctr++) {
        uint64_t injection_idx = noise_get_u64n(injection_ctr, 0, bit_combs);
        inject_simple_result expected_result;
        {
            for (uint32_t i = 0; i < vec_data.size(); i++) {
                vec_data[i] = 0;
            }
            for (uint32_t i = 0; i < vec_ecc.size(); i++) {
                vec_ecc[i] = 0;
            }
            expected_result = inject_with_idx_and_get_result(&ecc, vec_data, vec_ecc, word_width, fault_count, injection_idx);
        }
        for (size_t data_ctr = 0; data_ctr < 1000; data_ctr++) {
            for (uint32_t i = 0; i < vec_data.size(); i++) {
                vec_data[i] = squirrelnoise5_u64(data_ctr++, 42) & 0b1;
            }
            for (uint32_t i = 0; i < vec_ecc.size(); i++) {
                vec_ecc[i] = 0;
            }
            inject_simple_result real_result = inject_with_idx_and_get_result(&ecc, vec_data, vec_ecc, word_width, fault_count, injection_idx);
            if (real_result.det_result != expected_result.det_result || real_result.miscorrection_location != expected_result.miscorrection_location) {
                errorf("result mismatch\n");
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (false) {
        test_bit_enumeration_idx();
        exit(0);
    }
    if (false) {
        test_materialization_data_independence();
        exit(0);
    }

    bool full_run = false;
    FAIL_MODE fail_mode;
    uint32_t fail_count;
    uint64_t test_count;
    uint64_t seed = 42;

    // parse clas
    if (argc < 7) {
        errorf("usage: <threads> <fail_mode> <fail_count> <test_count> <ecc_method> <ecc_conf> [seed]\n");
    }

    const char* arg_thread_count = argv[1];
    const char* arg_fail_mode = argv[2];
    const char* arg_fail_count = argv[3];
    const char* arg_test_count = argv[4];
    const char* arg_ecc_method = argv[5];
    const char* arg_ecc_conf = argv[6];
    const char* arg_seed = argc > 7 ? argv[7] : NULL;
    bool debug_print = argc > 8;

    int thread_count = strtoul(arg_thread_count, NULL, 10);
    if (thread_count == 0) {
        thread_count = 1;
    } else if (thread_count > std::thread::hardware_concurrency()) {
        thread_count = std::thread::hardware_concurrency();
    }

    std::vector<thread_control> threads(thread_count);

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
    assert(fail_count <= 8);

    {
        int d;
        int k;
        int ec = sscanf(arg_ecc_conf, "%i/%i", &d, &k);
        if (ec != 2) {
            errorf("failed to read ecc conf\n");
        }
        if (strcmp(arg_ecc_method, "hamming") == 0) {
            for (int tid = 0; tid < threads.size(); tid++) {
                threads[tid].method = new ECCMethod_Hamming();
            }
        } else if (strcmp(arg_ecc_method, "bch") == 0) {
            for (int tid = 0; tid < threads.size(); tid++) {
                threads[tid].method = new ECCMethod_BCH(d, k);
            }
        } else if (strcmp(arg_ecc_method, "hsiao") == 0) {
            for (int tid = 0; tid < threads.size(); tid++) {
                threads[tid].method = new ECCMethod_Hsiao(d, k, debug_print);
            }
        } else {
            errorf("unknown ecc method\n");
        }
    }

    const uint32_t data_width = threads[0].method->DataWidth();
    const uint32_t ecc_width = threads[0].method->ECCWidth();
    const uint32_t word_width = data_width + ecc_width;

    full_run = strcmp(arg_test_count, "F") == 0;
    if (full_run) {
        test_count = nCr(word_width, fail_count);
    } else {
        test_count = strtoul(arg_test_count, NULL, 10);
    }

    srand(time(NULL)); // quick and dirty randomness if no seed given
    seed = arg_seed == NULL ? rand() : strtoull(arg_seed, NULL, 10);
    uint64_t rctr = 0;

    const bool print_tests = !full_run && test_count <= 10;

    uint64_t work_per_thread = test_count / thread_count;
    uint64_t rest_work = test_count % thread_count;

    // set final thread launching arguments
    for (int tid = 0; tid < threads.size(); tid++) {
        threads[tid].full_run = full_run;
        threads[tid].fail_mode = fail_mode;
        threads[tid].fail_count = fail_count;
        threads[tid].rng_seed = squirrelnoise5_u64(rctr++, seed);
        threads[tid].work_offset = tid * work_per_thread;
        threads[tid].work_max = (tid + 1) * work_per_thread + (tid == threads.size() - 1 ? rest_work : 0);
    }

    printf("datawidth: %u ; eccwidth: %u\n", data_width, ecc_width);
    if (full_run) {
        char testcount_str[SPACED_U64_MAX_STR_SIZE];
        pre_format_spaced_u64(testcount_str, test_count, ' ');
        printf("full run: %s tests\n", testcount_str);
    }

    // launch
    for (int tid = 0; tid < threads.size(); tid++) {
        pthread_create(&threads[tid].pthread_id, NULL, thread_work, &threads[tid]);
    }

    // report progress
    while (true) {
        uint64_t work_progress = 0;
        for (int tid = 0; tid < threads.size(); tid++) {
            work_progress += threads[tid].work_progress;
        }
        if (!print_tests) {
            printf("\rprogress: %.5f", (float)work_progress / (float)test_count);
            fflush(stdout);
        }
        if (work_progress == test_count) {
            break;
        }
        usleep(150 * 1000); // 150ms
    }

    ecc_stats stats;
    std::vector<uint64_t> flip_occurence_counts;
    std::vector<int64_t> flip_occurence_flip_avg_distances;
    flip_occurence_counts.resize(word_width, 0);
    flip_occurence_flip_avg_distances.resize(word_width, 0);

    // collect
    for (int tid = 0; tid < threads.size(); tid++) {
        pthread_join(threads[tid].pthread_id, NULL);
        stats.detection_ok += threads[tid].stats.detection_ok;
        stats.detection_corrected += threads[tid].stats.detection_corrected;
        stats.detection_uncorrectable += threads[tid].stats.detection_uncorrectable;
        stats.false_corrections += threads[tid].stats.false_corrections;
        for (int bit_pos = 0; bit_pos < word_width; bit_pos++) {
            flip_occurence_counts[bit_pos] += threads[tid].flip_occurence_counts[bit_pos];
            flip_occurence_flip_avg_distances[bit_pos] += threads[tid].flip_occurence_flip_avg_distances[bit_pos];
        }
    }
    if (stats.false_corrections > 0) {
        for (int bit_pos = 0; bit_pos < word_width; bit_pos++) {
            flip_occurence_flip_avg_distances[bit_pos] /= (int64_t)fail_count * (int64_t)stats.false_corrections;
        }
    }

    // report results
    if (print_tests) {
        printf("\n\n");
    } else {
        printf("\rprogress: 1.00\n\n");
    }
    printf("stats:\n");
    printf("detection ok%s: %lu\n", fail_count == 0 ? "" : " (sdcs)", stats.detection_ok);
    printf("detection corrected (false corrections therein): %lu (%lu)\n", stats.detection_corrected, stats.false_corrections);
    printf("detection uncorrectable: %lu\n", stats.detection_uncorrectable);

    printf("\n");
    printf("post fault flip occurences:\n");
    for (int bit_pos = 0; bit_pos < word_width; bit_pos++) {
        printf(" %lu", flip_occurence_counts[bit_pos]);
    }
    printf("\n");

    printf("\n");
    printf("flip occurence avg flip distance:\n");
    for (int bit_pos = 0; bit_pos < word_width; bit_pos++) {
        printf(" %ld", flip_occurence_flip_avg_distances[bit_pos]);
    }
    printf("\n");

    printf("\n");
    printf("done\n");
    return 0;
}
