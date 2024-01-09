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
        printf("failed to initialize bch control\n");
        assert(0);
        exit(-1);
    }
    // ctrl_data_width_bytes = (ctrl->n - ctrl->ecc_bits + 7) / 8;
    ctrl_data_width_bytes = (data_width + 7) / 8;
    // printf("bch init info:\n");
    // printf("\trequested data width b: %u\n", data_width);
    // printf("\trequested data width B: %u\n", ctrl_data_width_bytes);
    // printf("\trequested correction cap b: %u\n", correction_capability);
    // printf("\tm: %u\n", ctrl->m);
    // printf("\tt: %u\n", ctrl->t);
    // printf("\tn: %u\n", ctrl->n);
    // printf("\tecc b: %u\n", ctrl->ecc_bits);
    // printf("\tecc B: %u\n", ctrl->ecc_bytes);
    // printf("\tk msg b: %u\n", ctrl->n - ctrl->ecc_bits);
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
        packed_data[i / 8] |= data[i] << (7 - (i % 8));
    }

    // encode
    encode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data());

    // output ecc
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        ecc[i] = (packed_ecc[i / 8] >> (7 - (i % 8))) & 0b1;
    }
}

ECC_DETECTION ECCMethod_BCH::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    std::vector<uint8_t> packed_data(ctrl_data_width_bytes, 0);
    std::vector<uint8_t> packed_ecc(ctrl->ecc_bytes, 0);

    // pack inputs into readable format for decode_bch
    for (uint32_t i = 0; i < data_width; i++) {
        packed_data[i / 8] |= data[i] << (7 - (i % 8));
    }
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        packed_ecc[i / 8] |= ecc[i] << (7 - (i % 8));
    }

    // decode
    std::vector<uint32_t> err_locations(correction_capability, 0);
    int err_num = decode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data(), NULL, NULL, err_locations.data());

    if (err_num == -EINVAL) {
        printf("bch message decoding parameters invalid\n");
        assert(0);
        exit(-1);
    } else if (err_num == -EBADMSG) {
        return ECC_DETECTION_UNCORRECTABLE;
    } else if (err_num == 0) {
        return ECC_DETECTION_OK;
    }

    // printf("detected %i correctable error%s at:", err_num, err_num > 1 ? "s" : "");
    // for (int i = 0; i < err_num; i++) {
    //     printf(" %u", ((err_locations[i] / 8) * 8) + (7 - (err_locations[i] % 8)));
    //     if (i + 1 < err_num) {
    //         printf(",");
    //     }
    // }
    // printf("\n");

    // correct faults
    correct_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, err_locations.data(), err_num);
    // recalc ecc part to fix possible ecc faults
    packed_ecc.assign(ctrl->ecc_bytes, 0);
    encode_bch(ctrl, packed_data.data(), ctrl_data_width_bytes, packed_ecc.data()); // could flip instead of recomputing

    // output packed and corrected inputs
    for (uint32_t i = 0; i < data_width; i++) {
        data[i] = (packed_data[i / 8] >> (7 - (i % 8))) & 0b1;
    }
    for (uint32_t i = 0; i < ctrl->ecc_bits; i++) {
        ecc[i] = (packed_ecc[i / 8] >> (7 - (i % 8))) & 0b1;
    }

    return ECC_DETECTION_CORRECTED;
}
