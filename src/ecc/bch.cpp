#include <cstdint>
#include <vector>

#include "ecc.hpp"

#include "bch.hpp"

ECCMethod_BCH::ECCMethod_BCH(uint32_t data_width, uint32_t correction_capability):
    data_width(data_width),
    correction_capability(correction_capability)
{
    //TODO
}

ECCMethod_BCH::~ECCMethod_BCH()
{
}

uint32_t ECCMethod_BCH::DataWidth()
{
    return data_width;
}

uint32_t ECCMethod_BCH::ECCWidth()
{
    return ecc_width;
}

void ECCMethod_BCH::ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc)
{
    //TODO
}

ECC_DETECTION ECCMethod_BCH::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    //TODO
    return ECC_DETECTION_OK;
}
