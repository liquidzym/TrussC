#ifndef _IMATRIX_H_
#define _IMATRIX_H_

#include <algorithm>
#include <vector>

class imatrix {
private:
    int Nr = 1;
    int Nc = 1;
    std::vector<int> data_{1};

public:
    imatrix() = default;

    imatrix(int rows, int cols) {
        init(rows, cols);
    }

    void init(int rows, int cols) {
        Nr = std::max(0, rows);
        Nc = std::max(0, cols);
        data_.assign(static_cast<size_t>(Nr) * static_cast<size_t>(Nc), 0);
    }

    int* operator[](int i) { return data_.data() + static_cast<size_t>(i) * Nc; }
    const int* operator[](int i) const { return data_.data() + static_cast<size_t>(i) * Nc; }

    int& get(int i, int j) { return (*this)[i][j]; }
    const int& get(int i, int j) const { return (*this)[i][j]; }
    int getRow() const { return Nr; }
    int getCol() const { return Nc; }

    void zero() {
        std::fill(data_.begin(), data_.end(), 0);
    }

    void copy(const imatrix& b) {
        Nr = b.Nr;
        Nc = b.Nc;
        data_ = b.data_;
    }
};

#endif
