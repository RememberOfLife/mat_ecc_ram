#pragma once

#include <cstdint>
#include <vector>

#include "ecc.hpp"

class ECCMethod_Hsiao : public ECCMethod {
    // SECDED with hsiao hamming
    // available in 64 or 128 bit form

  public:

    enum HSIAO_LENGTH {
        HSIAO_LENGTH_8 = 0,
        HSIAO_LENGTH_64,
        HSIAO_LENGTH_128,
        HSIAO_LENGTH_COUNT,
    };

  private:

    // see hsiao.cpp
    static const std::vector<const char*> static_hsiao_codes[HSIAO_LENGTH_COUNT];
    static const int static_hsiao_parity_lengths[HSIAO_LENGTH_COUNT];

    bool debug_print;

    int n;
    int k;
    int d;

    std::vector<std::vector<bool>> parity_matrix_by_rows;
    std::vector<std::vector<bool>> parity_matrix_by_columns;

  public:

    ECCMethod_Hsiao(HSIAO_LENGTH len, bool debug_print = false);
    ~ECCMethod_Hsiao();

    uint32_t DataWidth() override;
    uint32_t ECCWidth() override;
    void ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc) override;
    ECC_DETECTION CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) override;
};
