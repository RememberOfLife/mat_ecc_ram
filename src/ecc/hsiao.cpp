#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "ecc.hpp"

#include "hsiao.hpp"

ECCMethod_Hsiao::ECCMethod_Hsiao(int data_bits, int parity_bits, bool debug_print):
    debug_print(debug_print)
{
    // calculate how many parity bits are required minimum
    int req_k = 0;
    int m = 0;
    while (true) {
        if (pow(2, m) - m - 1 >= data_bits) {
            req_k = m + 1;
            break;
        }
        m++;
    }
    if (parity_bits == 0) {
        parity_bits = req_k;
    }
    if (parity_bits < req_k) {
        printf("too few parity bits (%i), need at least %i\n", parity_bits, req_k);
        assert(0);
        exit(-1);
    }
    d = data_bits;
    k = parity_bits;
    n = d + k;
    matrix hsiao_code = matrix_construction(d, k, debug_print);
    parity_matrix_by_rows.resize(k, std::vector<bool>(n, false));
    parity_matrix_by_columns.resize(n, std::vector<bool>(k, false));
    for (int ri = 0; ri < k; ri++) {
        for (int ci = 0; ci < n; ci++) {
            bool bit = hsiao_code.d[ri][ci] != 0;
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
        for (int ci = 0; ci < n; ci++) {
            if (ecc[ei] != syndrome[ei]) {
                row_conjuction[ci] = row_conjuction[ci] && parity_matrix_by_rows[ei][ci];
            } else if (parity_matrix_by_rows[ei][ci] == 1) {
                row_conjuction[ci] = 0; // we don't really need to do this, we could also just stop flipping after the first bit has been flipped, but this cleans up the conjuction so there is just the one bit left in the mask
            }
        }
    }
    if (debug_print) {
        printf("row conjunction (excluding impossible combinations):\n");
    }
    bool corrected = false;
    for (int ci = 0; ci < n; ci++) {
        if (row_conjuction[ci] == true && !corrected) {
            if (ci < n - k) {
                data[ci] = !data[ci];
            } else {
                ecc[ci - d] = !ecc[ci - d];
            }
            corrected = true;
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
    if (!corrected) {
        // if the syndrom was invalid and did not point to a correctable failure, we can detect this because no bit was flipped
        return ECC_DETECTION_UNCORRECTABLE;
    }
    return ECC_DETECTION_CORRECTED;
}

ECCMethod_Hsiao::matrix::matrix(int rows, int cols, int fill_elem):
    rows(rows),
    cols(cols)
{
    d.resize(rows, std::vector<int>(cols, fill_elem));
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix::identity(int s)
{
    matrix ret(s, s);
    for (int i = 0; i < s; i++) {
        ret.d[i][i] = 1;
    }
    return ret;
}

std::vector<int>& ECCMethod_Hsiao::matrix::operator[](int ri)
{
    return d[ri];
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix::select_rows(std::vector<int> row_indices)
{
    assert(row_indices.size() == rows);
    matrix result(row_indices.size(), cols);
    for (int i = 0; i < row_indices.size(); ++i) {
        assert(row_indices[i] >= 0 && row_indices[i] < rows);
        result[i] = d[row_indices[i]];
    }
    return result;
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix::hstack(std::vector<matrix> parts)
{
    assert(parts.size() > 0);
    int out_cols = 0;
    int row_count = parts[0].rows;
    // check for consistent row count and calculate total column count
    for (int pi = 0; pi < parts.size(); pi++) {
        assert(parts[pi].rows == row_count);
        out_cols += parts[pi].cols;
    }
    matrix result(row_count, out_cols);
    // concatenate matrices horizontally
    int col_offset = 0;
    for (int pi = 0; pi < parts.size(); pi++) {
        for (int ri = 0; ri < parts[pi].rows; ri++) {
            for (int ci = 0; ci < parts[pi].cols; ci++) {
                result[ri][col_offset + ci] = parts[pi][ri][ci];
            }
        }
        col_offset += parts[pi].cols;
    }
    return result;
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix::vstack(std::vector<matrix> parts)
{
    assert(parts.size() > 0);
    int out_rows = 0;
    int col_count = parts[0].cols;
    // check for consistent col count and calculate total row count
    for (int pi = 0; pi < parts.size(); pi++) {
        assert(parts[pi].cols == col_count);
        out_rows += parts[pi].rows;
    }
    matrix result(out_rows, col_count);
    // concatenate matrices vertically
    int row_offset = 0;
    for (int pi = 0; pi < parts.size(); pi++) {
        for (int ri = 0; ri < parts[pi].rows; ri++) {
            for (int ci = 0; ci < parts[pi].cols; ci++) {
                result[row_offset + ri][ci] = parts[pi][ri][ci];
            }
        }
        row_offset += parts[pi].rows;
    }
    return result;
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix_construction(int d, int k, bool debug_print)
{
    // generate matrix using contruction algorithm adapted for cpp from https://github.com/msvisser/memory-controller-generator

    // calculate the weight of the highest weight columns
    int max_weight = 1;
    int prev_total = 0;
    int total = nCr(k, max_weight);
    int n = d + k;
    while (n > total) {
        max_weight += 2;
        prev_total = total;
        total += nCr(k, max_weight);
    }
    // number of max weight columns
    int max_weight_columns = n - prev_total;
    // parts that make up the parity-check matrix
    std::vector<matrix> parts;
    // build all sub-matrices where all columns are present
    for (int weight = 3; weight < max_weight; weight += 2) {
        parts.push_back(matrix_construction_delta(k, nCr(k, weight), weight));
    }
    // append smaller final sub-matrix
    parts.push_back(matrix_construction_delta(k, max_weight_columns, max_weight));
    // append identity matrix at the end
    parts.push_back(matrix::identity(k));
    // build the parity-check matrix by stacking the parts
    return matrix::hstack(parts);
}

ECCMethod_Hsiao::matrix ECCMethod_Hsiao::matrix_construction_delta(int rows, int cols, int weight, bool debug_print)
{
    // adapted for cpp from https://github.com/msvisser/memory-controller-generator
    // compute delta sub-matrix
    if (cols == 0) {
        // no columns, zero column matrix
        return matrix(rows, 0);
    } else if (weight == 0) {
        // single column with zero weight
        assert(cols == 1);
        return matrix(rows, 1);
    } else if (weight == rows) {
        // single column with maximum weight
        assert(cols == 1);
        return matrix(rows, 1, 1);
    } else if (cols == 1) {
        // single column of specified weight, fill the first n rows with 1, where n = weight
        matrix ret = matrix(rows, 1);
        for (int i = 0; i < weight; i++) {
            ret[i][0] = 1;
        }
        return ret;
    } else if (weight == 1) {
        // weight is 1, so identity matrix padded with zero rows
        assert(rows >= cols);
        matrix ident = matrix::identity(cols);
        matrix zeros = matrix(rows - cols, cols);
        return matrix::vstack({ident, zeros});
    } else if (weight == rows - 1) {
        // weight is rows - 1, so all ones with identity subtracted from the bottom rows
        assert(rows >= cols);
        matrix ones = matrix(rows - cols, cols, 1);
        matrix ident = matrix(cols, cols, 1);
        for (int i = 0; i < cols; i++) {
            ident[i][i] = 0;
        }
        return matrix::vstack({ones, ident});
    } else {
        // general case that requires splitting
        assert(2 <= weight && weight <= rows - 2);
        assert(2 <= cols && cols <= nCr(rows, weight));
        // recursively calculate sub-parts of the matrix
        int m1 = std::ceil((float)(cols * weight) / (float)rows);
        matrix delta1 = matrix_construction_delta(rows - 1, m1, weight - 1);
        matrix delta2 = matrix_construction_delta(rows - 1, cols - m1, weight);
        // calculate the shifting of rows required in delta2
        int r1 = ((weight - 1) * m1) % (rows - 1);
        int r2 = (weight * (cols - m1)) % (rows - 1);
        std::vector<int> order;
        if (r1 + r2 > (rows - 1)) {
            // shift the first r2-rp rows to the bottom
            int rp = r1 + r2 - (rows - 1);
            for (int i = r2 - rp; i < rows - 1; i++) {
                order.push_back(i);
            }
            for (int i = 0; i < r2 - rp; i++) {
                order.push_back(i);
            }
        } else {
            // shift the first r2 rows to r1+1
            order.clear();
            for (int i = 0; i < std::min(r1 + 1, rows - 1 - r2); i++) {
                order.push_back(r2 + i);
            }
            for (int i = 0; i < r2; i++) {
                order.push_back(i);
            }
            for (int i = r1 + 1; i < rows - 1 - r2; i++) {
                order.push_back(r2 + i);
            }
        }
        // reorder delta2 to get delta2 prime
        matrix delta2_prime = delta2.select_rows(order);
        // create the top row of the resulting matrix
        matrix ones = matrix(1, m1, 1);
        matrix zeros = matrix(1, cols - m1);
        matrix top = matrix::hstack({ones, zeros});
        // create the bottom sub-matrix of the resulting matrix
        matrix bot = matrix::hstack({delta1, delta2_prime});
        return matrix::vstack({top, bot});
    }
}

uint64_t ECCMethod_Hsiao::nCr(uint64_t n, uint64_t r)
{
    if (r == 0) {
        return 1;
    } else {
        uint64_t num = n * nCr(n - 1, r - 1);
        return num / r;
    }
}
