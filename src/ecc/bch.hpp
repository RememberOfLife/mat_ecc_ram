#pragma once

#include <cstdint>
#include <vector>

#include "bch_codec/bch_codec.h"

#include "ecc.hpp"

class ECCMethod_BCH : public ECCMethod {
    // generic BCH wrapper

  private:

    bch_control* ctrl;

    uint32_t data_width;
    uint32_t ctrl_data_width_bytes;
    uint32_t correction_capability;

  public:

    ECCMethod_BCH(uint32_t data_width, uint32_t correction_capability);
    ~ECCMethod_BCH();

    uint32_t DataWidth() override;
    uint32_t ECCWidth() override;
    void ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc) override;
    ECC_DETECTION CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) override;
};
