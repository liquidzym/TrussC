#ifndef _ETF_H_
#define _ETF_H_

#include "imatrix.h"

#include <algorithm>
#include <vector>

struct Vect {
    double tx = 0.0;
    double ty = 0.0;
    double mag = 0.0;
};

class ETF {
private:
    int Nr = 1;
    int Nc = 1;
    std::vector<Vect> data_{{1.0, 0.0, 1.0}};
    std::vector<Vect*> p{data_.data()};
    double max_grad = 1.0;

    void rebuildRows() {
        p.resize(static_cast<size_t>(Nr));
        for (int i = 0; i < Nr; ++i) {
            p[static_cast<size_t>(i)] = data_.data() + static_cast<size_t>(i) * Nc;
        }
    }

public:
    ETF() = default;

    ETF(int rows, int cols) {
        init(rows, cols);
    }

    Vect* operator[](int i) { return data_.data() + static_cast<size_t>(i) * Nc; }
    const Vect* operator[](int i) const { return data_.data() + static_cast<size_t>(i) * Nc; }

    Vect& get(int i, int j) { return (*this)[i][j]; }
    const Vect& get(int i, int j) const { return (*this)[i][j]; }
    int getRow() const { return Nr; }
    int getCol() const { return Nc; }

    void init(int rows, int cols) {
        Nr = std::max(0, rows);
        Nc = std::max(0, cols);
        data_.assign(static_cast<size_t>(Nr) * static_cast<size_t>(Nc), Vect{});
        rebuildRows();
        max_grad = 1.0;
    }

    void copy(const ETF& s) {
        Nr = s.Nr;
        Nc = s.Nc;
        data_ = s.data_;
        rebuildRows();
        max_grad = s.max_grad;
    }

    void zero() {
        std::fill(data_.begin(), data_.end(), Vect{});
    }

    void set(imatrix& image);
    void set2(imatrix& image);
    void Smooth(int half_w, int M);
    double GetMaxGrad() const { return max_grad; }
    void normalize();
};

#endif
