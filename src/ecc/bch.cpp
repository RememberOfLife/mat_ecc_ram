#include <cassert>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <errno.h>
#include <vector>

#include "bch_codec/bch_codec.h"

#include "ecc.hpp"

#include "bch.hpp"

ECCMethod_BCH::ECCMethod_BCH(uint32_t data_width, uint32_t correction_capability):
    data_width(data_width),
    correction_capability(correction_capability)
{
    int m = ceil(log2(data_width + 1));
    ctrl = init_bch(m, correction_capability, 0);
    if (ctrl == NULL) {
        assert(0);
        printf("failed to initialize bch control\n");
        exit(-1);
    }
    ctrl_data_width_bytes = (ctrl->n - ctrl->ecc_bits + 7) / 8;
}

ECCMethod_BCH::~ECCMethod_BCH()
{
    free_bch(ctrl);
}

uint32_t ECCMethod_BCH::DataWidth()
{
    return data_width;
}

uint32_t ECCMethod_BCH::ECCWidth()
{
    return ctrl->ecc_bits;
}

void ECCMethod_BCH::ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc)
{
    std::vector<uint8_t> packed_data(ctrl_data_width_bytes, 0);
    std::vector<uint8_t> packed_ecc(ctrl->ecc_bytes, 0);

    // pack data into readable format for encode_bch
    for (uint32_t i = 0; i < data_width; i++) {
        packed_data[i / 8] |= data[i] << (i % 8);
    }

    // encode
    encode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data());

    // output ecc
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        ecc[i] = (packed_ecc[i / 8] >> (i % 8)) & 0b1;
    }
}

ECC_DETECTION ECCMethod_BCH::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    std::vector<uint8_t> packed_data(ctrl_data_width_bytes, 0);
    std::vector<uint8_t> packed_ecc(ctrl->ecc_bytes, 0);

    // pack inputs into readable format for decode_bch
    for (uint32_t i = 0; i < data_width; i++) {
        packed_data[i / 8] |= data[i] << (i % 8);
    }
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        packed_ecc[i / 8] |= ecc[i] << (i % 8);
    }

    // decode
    std::vector<uint32_t> err_locations(correction_capability, 0);
    int err_num = decode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data(), NULL, NULL, err_locations.data());

    if (err_num == -EINVAL) {
        assert(0);
        printf("bch message decoding parameters invalid\n");
        exit(-1);
    } else if (err_num == -EBADMSG) {
        return ECC_DETECTION_UNCORRECTABLE;
    } else if (err_num == 0) {
        return ECC_DETECTION_OK;
    }

    // correct faults
    correct_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, err_locations.data(), err_num);
    // correct faults in ecc part as well
    packed_ecc.assign(ctrl->ecc_bytes, 0);
    encode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data()); // TODO dont just recompute..
    /*
    for (int i = 0; i < err_num; i++) {
        int bi = err_locations[i];
        printf("loc: %i N:%u\n", bi, ctrl->n);
        if (bi >= ctrl->n) {
            bi -= ctrl->n;
            packed_ecc[bi / 8] ^= (1 << (bi % 8));
        }
    }
    */

    // output packed and corrected inputs
    for (uint32_t i = 0; i < data_width; i++) {
        data[i] = (packed_data[i / 8] >> (i % 8)) & 0b1;
    }
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        ecc[i] = (packed_ecc[i / 8] >> (i % 8)) & 0b1;
    }

    return ECC_DETECTION_CORRECTED;
}
