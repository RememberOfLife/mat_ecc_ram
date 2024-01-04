#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "ecc.hpp"

#include "hsiao.hpp"

// to get the static hsiao matrices you can inspire yourself from this wonderful but unfortunately not licensed repository https://github.com/msvisser/memory-controller-generator
// the algorithm used is theoretically adaptable from the HDL python to just output the parity matrix, one might see this when looking at the testbench/example.py file
// or copy over any hsiao parity matrix you might have laying around or seen in a paper
//==> parity bits come last
const std::vector<const char*> ECCMethod_Hsiao::static_hsiao_codes[HSIAO_LENGTH_COUNT] = {
    // no array designators in cpp :(
    /* [HSIAO_LENGTH_8] = */ {
        "1111100010000",
        "1110001101000",
        "1000110100100",
        "0101111000010",
        "0011011100001",
    },
    /* [HSIAO_LENGTH_64] = */ {
        "111111111111111111111000000000000000000000000000000000001111100010000000",
        "111111000000000000000111111111111111000000000000000000001110011001000000",
        "100000111110000000000111110000000000111111111100000000001101011000100000",
        "010000100001111000000100001111000000111100000011111100001011010100010000",
        "001000010001000111000010001000111000100011100011100001111010110100001000",
        "000100001000100100011001000100100011010010001110001110110000101100000100",
        "000010000100010010101000100010010101001001010101010111010101101100000010",
        "000001000010001001110000010001001110000100111000111011100111111100000001",
    },
    /* [HSIAO_LENGTH_128] = */ {
        "11111111111111111111111111110000000000000000000000000000000000000000000000000000000011111111111111111111111110000000000000000000100000000",
        "11111110000000000000000000001111111111111111111110000000000000000000000000000000000011111111111110000000000001010011010111101011010000000",
        "10000001111110000000000000001111110000000000000001111111111111110000000000000000000011111100000001010011010110110011001111010111001000000",
        "01000001000001111100000000001000001111100000000001111100000000001111111111000000000011000001100110110011001110001110111100101111000100000",
        "00100000100001000011110000000100001000011110000001000011110000001111000000111111000010000111110000001110111100001100111010011111000010000",
        "00010000010000100010001110000010000100010001110000100010001110001000111000111000011101000111001010001100111011111111111110000000000001000",
        "00001000001000010001001000110001000010001001000110010001001000110100100011100011101100110010100111111111000001111111000001111100000000100",
        "00000100000100001000100101010000100001000100101010001000100101010010010101010101110100101000011101111000110101111000110101111001000000010",
        "00000010000010000100010011100000010000100010011100000100010011100001001110001110111000011000011001100101101011100101101011110110000000001",
    }};

const int ECCMethod_Hsiao::static_hsiao_parity_lengths[HSIAO_LENGTH_COUNT] = {
    /* [HSIAO_LENGTH_8] = */ 5,
    /* [HSIAO_LENGTH_64] = */ 8,
    /* [HSIAO_LENGTH_128] = */ 9,
};

ECCMethod_Hsiao::ECCMethod_Hsiao(HSIAO_LENGTH len, bool debug_print):
    debug_print(debug_print)
{
    if (len >= HSIAO_LENGTH_COUNT) {
        assert(0);
        printf("invalid hsiao length\n");
        exit(-1);
    }
    const std::vector<const char*>& hsiao_code = static_hsiao_codes[len];
    n = strlen(hsiao_code[0]);
    k = static_hsiao_parity_lengths[len];
    d = n - k;
    parity_matrix_by_rows.resize(k, std::vector<bool>(n, false));
    parity_matrix_by_columns.resize(n, std::vector<bool>(k, false));
    for (int ri = 0; ri < k; ri++) {
        for (int ci = 0; ci < n; ci++) {
            bool bit = hsiao_code[ri][ci] - '0';
            parity_matrix_by_rows[ri][ci] = bit;
            parity_matrix_by_columns[ci][ri] = bit;
            if (debug_print) {
                printf("%c", '0' + bit);
            }
        }
        if (debug_print) {
            printf("\n");
        }
    }
}

ECCMethod_Hsiao::~ECCMethod_Hsiao()
{
    // pass
}

uint32_t ECCMethod_Hsiao::DataWidth()
{
    return d;
}

uint32_t ECCMethod_Hsiao::ECCWidth()
{
    return k;
}

void ECCMethod_Hsiao::ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc)
{
    ecc.assign(ECCWidth(), 0);
    for (int ci = 0; ci < d; ci++) {
        for (int ri = 0; ri < k; ri++) {
            if (parity_matrix_by_columns[ci][ri] == true) {
                ecc[ri] = ecc[ri] ^ data[ci];
            }
        }
    }
}

ECC_DETECTION ECCMethod_Hsiao::CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc)
{
    std::vector<bool> syndrome(k, false);
    for (int ci = 0; ci < d; ci++) {
        for (int ri = 0; ri < k; ri++) {
            if (parity_matrix_by_columns[ci][ri] == true) {
                syndrome[ri] = syndrome[ri] ^ data[ci];
            }
        }
    }
    bool zero = true;
    int mmcnt = 0;
    for (int ei = 0; ei < k; ei++) {
        bool match = (ecc[ei] == syndrome[ei]);
        if (!match) {
            zero = false;
            mmcnt++;
        }
        if (debug_print) {
            printf("bit:%i %s\n", ei, match ? "match" : "fail");
        }
    }
    if (debug_print) {
        printf("%i fails\n", mmcnt);
    }
    if (zero == true) {
        return ECC_DETECTION_OK;
    }
    if ((mmcnt & 0b1) == 0b0) {
        // even non-zero syndrome
        return ECC_DETECTION_UNCORRECTABLE;
    }
    // correct error at syndrome
    // bitwise conjuction of all the rows containing missmatches in the syndrome, remaining bits are positions to flip
    std::vector<bool> row_conjuction(n, true);
    if (mmcnt == 1) {
        // fail is within ecc bits, unset all non parity bits
        std::replace(row_conjuction.begin(), row_conjuction.begin() + d, true, false);
    }
    for (int ei = 0; ei < k; ei++) {
        if (ecc[ei] != syndrome[ei]) {
            for (int ci = 0; ci < n; ci++) {
                row_conjuction[ci] = row_conjuction[ci] && parity_matrix_by_rows[ei][ci];
            }
        }
    }
    for (int ci = 0; ci < n; ci++) {
        if (row_conjuction[ci] == true) {
            if (ci < n - k) {
                data[ci] = !data[ci];
            } else {
                ecc[ci - d] = !ecc[ci - d];
            }
            if (!debug_print) {
                break;
            }
        }
        if (debug_print) {
            if (ci == n - k) {
                printf(" ");
            }
            printf("%c", row_conjuction[ci] ? '1' : '0');
        }
    }
    if (debug_print) {
        printf("\n");
    }
    return ECC_DETECTION_CORRECTED;
}
