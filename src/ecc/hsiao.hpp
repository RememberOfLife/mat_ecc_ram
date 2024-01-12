#pragma once

#include <cstdint>
#include <vector>

#include "ecc.hpp"

class ECCMethod_Hsiao : public ECCMethod {
    // SECDED with hsiao hamming

  private:

    bool debug_print;

    int n;
    int k;
    int d;

    std::vector<std::vector<bool>> parity_matrix_by_rows;
    std::vector<std::vector<bool>> parity_matrix_by_columns;

  public:

    ECCMethod_Hsiao(int data_bits, int parity_bits, bool debug_print = false);
    ~ECCMethod_Hsiao();

    uint32_t DataWidth() override;
    uint32_t ECCWidth() override;
    void ConstructECC(std::vector<bool>& data, std::vector<bool>& ecc) override;
    ECC_DETECTION CheckAndCorrect(std::vector<bool>& data, std::vector<bool>& ecc) override;

  private:

    struct matrix {
        int rows;
        int cols;
        std::vector<std::vector<int>> d; // row major stored data
        matrix(int rows, int cols, int fill_elem = 0);
        static matrix identity(int s);
        std::vector<int>& operator[](int ri);
        matrix select_rows(std::vector<int> row_indices);

        static matrix hstack(std::vector<matrix> parts);
        static matrix vstack(std::vector<matrix> parts);
    };

    static matrix matrix_construction(int d, int k, bool debug_print = false);
    static matrix matrix_construction_delta(int rows, int columns, int weight, bool debug_print = false);

    static uint64_t nCr(uint64_t n, uint64_t r);
};
