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
    void ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc) override;
    ECC_DETECTION CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) override;
};
