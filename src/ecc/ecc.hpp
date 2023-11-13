#pragma once

#include <cstdint>
#include <vector>

enum ECC_DETECTION {
    ECC_DETECTION_OK = 0,
    ECC_DETECTION_CORRECTED,
    ECC_DETECTION_UNCORRECTABLE,
};

class ECCMethod {

  public:

    ECCMethod(){};
    virtual ~ECCMethod(){};

    virtual uint32_t DataWidth() = 0;
    virtual uint32_t ECCWidth() = 0;
    virtual void ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc) = 0;
    virtual ECC_DETECTION CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) = 0;
};
