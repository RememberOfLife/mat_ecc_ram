#include <cmath>
#include <cstdint>
#include <vector>

#include "ecc.hpp"

#include "hamming.hpp"

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

std::vector<bool> ECCMethod_Hamming::ConstructECC(std::vector<bool>& data)
{
}

ECCMethod::ECC_DETECTION_E ECCMethod_Hamming::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
}