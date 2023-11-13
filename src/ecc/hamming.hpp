#pragma once

#include <cstdint>
#include <vector>

#include "ecc.hpp"

class ECCMethod_Hamming : public ECCMethod {
    // SECDED with hamming, 8 ecc bits per 64 data bits

  private:

  public:

    ECCMethod_Hamming();
    ~ECCMethod_Hamming();

    uint32_t DataWidth() override;
    uint32_t ECCWidth() override;
    std::vector<bool> ConstructECC(std::vector<bool>& data) override;
    ECC_DETECTION_E CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) override;
};
