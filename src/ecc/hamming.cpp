#include <cstdint>
#include <vector>

#include "ecc.hpp"

#include "hamming.hpp"

bool single_bit_set(uint8_t v)
{
    if (v == 0) {
        return false;
    }
    return (v & (v - 1)) == 0;
}

ECCMethod_Hamming::ECCMethod_Hamming()
{
}

ECCMethod_Hamming::~ECCMethod_Hamming()
{
}

uint32_t ECCMethod_Hamming::DataWidth()
{
    return 64;
}

uint32_t ECCMethod_Hamming::ECCWidth()
{
    return 8;
}

void ECCMethod_Hamming::ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc)
{
    uint8_t ecc_byte = 0x00;
    ecc.assign(ECCWidth(), 0);
    // construct parity bits such that all covered bits AND the parity bit equal 0 parity
    uint8_t bit_num = 1;
    uint8_t ecc_bit_skip = 0;
    bool total_parity = false;
    for (; bit_num <= (DataWidth() + ECCWidth() - 1); bit_num++) {
        // skip if this bit, num actually belongs to the ecc part, but remember the skip cnt
        if (single_bit_set(bit_num)) {
            ecc_bit_skip++;
            continue;
        }
        // data bit flips corresponding ecc bits if it is set
        // flip using the position as mask for the ecc
        uint8_t bit_idx = bit_num - 1 - ecc_bit_skip;
        if (data[bit_idx]) {
            ecc_byte ^= bit_num;
            total_parity = !total_parity;
        }
    }
    // assign ecc into vector
    for (uint8_t i = 0; i < ECCWidth() - 1; i++) {
        bool is_set = (ecc_byte >> i) & 0b1;
        ecc[i] = is_set;
        if (is_set) {
            total_parity = !total_parity;
        }
    }
    // save total parity
    ecc[ECCWidth() - 1] = total_parity;
}

ECC_DETECTION ECCMethod_Hamming::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    std::vector<bool> check_ecc;
    ConstructECC(data, check_ecc);
    uint8_t syndrome = 0x00;
    for (uint8_t i = 0; i < ECCWidth() - 1; i++) {
        syndrome |= (ecc[i] != check_ecc[i]) << i;
    }
    bool total_parity = false;
    for (uint8_t i = 0; i < DataWidth(); i++) {
        if (data[i]) {
            total_parity = !total_parity;
        }
    }
    for (uint8_t i = 0; i < ECCWidth() - 1; i++) {
        if (ecc[i]) {
            total_parity = !total_parity;
        }
    }
    if (total_parity == ecc[ECCWidth() - 1]) {
        // total parity is correct, either everything is fine, or uncorrectable multi-bit error
        if (syndrome == 0) {
            return ECC_DETECTION_OK;
        } else {
            return ECC_DETECTION_UNCORRECTABLE;
        }
    } else if (total_parity != ecc[ECCWidth() - 1] && syndrome == 0) {
        // total parity broken, but no syndrome -> single bit error on total parity
        ecc[ECCWidth() - 1] = total_parity;
        return ECC_DETECTION_CORRECTED;
    }
    // single bit error via syndrome
    uint8_t ecc_bit_skip = 0;
    for (uint8_t i = 0; i < ECCWidth() - 1; i++) {
        if ((syndrome >> i) & 0b1) {
            ecc_bit_skip = i;
        }
    }
    ecc_bit_skip++;
    if (ecc_bit_skip == 1) {
        // parity bit correction
        ecc[ecc_bit_skip] = !ecc[ecc_bit_skip];
    } else {
        // use parity position sum (syndrome)
        data[syndrome - 1 - ecc_bit_skip] = !data[syndrome - 1 - ecc_bit_skip];
    }
    return ECC_DETECTION_CORRECTED;
}
