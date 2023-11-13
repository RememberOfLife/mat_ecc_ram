#pragma once

#include <cstdint>
#include <vector>

class ECCMethod {
  public:

    enum ECC_DETECTION_E {
        ECC_DETECTION_OK = 0,
        ECC_DETECTION_CORRECTED,
        ECC_DETECTION_UNCORRECTABLE,
    };

  public:

    ECCMethod(){};
    virtual ~ECCMethod(){};

    virtual uint32_t DataWidth() = 0;
    virtual uint32_t ECCWidth() = 0;
    virtual std::vector<bool> ConstructECC(std::vector<bool>& data) = 0;
    virtual ECC_DETECTION_E CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) = 0;
};
