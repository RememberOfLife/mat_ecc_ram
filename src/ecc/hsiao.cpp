#include <cstdint>
#include <vector>

#include "ecc.hpp"

#include "hsiao.hpp"

// to get the static hsiao matrices you can inspire yourself from this wonderful but unfortunately not licensed repository https://github.com/msvisser/memory-controller-generator
// the algorithm used is theoretically adaptable from the HDL python to just output the parity matrix, one might see this when looking at the testbench/example.py file
// or copy over any hsiao parity matrix you might have laying around or seen in a paper
//==> parity bits come last
const std::vector<const char*> ECCMethod_Hsiao::static_hsiao_codes[HSIAO_LENGTH_COUNT] = {
    // no array designators in cpp :(
    /* HSIAO_LENGTH_64 */ {
        "111111111111110000000000000000001000000",
        "111110000000001111111110000000000100000",
        "100000010100011111000001000111110010000",
        "010001111000001000010110101011010001000",
        "001001000010110100111000011101100000100",
        "000100100111000010100011111110000000010",
        "000010001001100001001101110000110000001",
    },
};

ECCMethod_Hsiao::ECCMethod_Hsiao(HSIAO_LENGTH len)
{
    //TODO populate parity matrices
}

ECCMethod_Hsiao::~ECCMethod_Hsiao()
{
    // pass
}

uint32_t ECCMethod_Hsiao::DataWidth()
{
    //TODO
}

uint32_t ECCMethod_Hsiao::ECCWidth()
{
    //TODO
}

void ECCMethod_Hsiao::ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc)
{
    //TODO
}

ECC_DETECTION ECCMethod_Hsiao::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    //TODO
}
