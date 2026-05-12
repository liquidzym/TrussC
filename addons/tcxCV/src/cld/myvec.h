#ifndef _MYVEC_H_
#define _MYVEC_H_

#include <algorithm>
#include <cmath>
#include <vector>

class myvec {
public:
    int N = 1;

    myvec() = default;

    explicit myvec(int size) {
        init(size);
    }

    double& operator[](int i) { return data_[static_cast<size_t>(i)]; }
    const double& operator[](int i) const { return data_[static_cast<size_t>(i)]; }

    void zero() {
        std::fill(data_.begin(), data_.end(), 0.0);
    }

    void make_unit() {
        double sum = 0.0;
        for (double v : data_) {
            sum += v * v;
        }
        sum = std::sqrt(sum);
        if (sum > 0.0) {
            for (double& v : data_) {
                v /= sum;
            }
        }
    }

    double norm() const {
        double sum = 0.0;
        for (double v : data_) {
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    double get(int n) const { return data_[static_cast<size_t>(n)]; }
    int getMax() const { return N; }

    void init(int size) {
        N = std::max(0, size);
        data_.assign(static_cast<size_t>(N), 0.0);
    }

private:
    std::vector<double> data_{1.0};
};

class mymatrix {
private:
    int Nr = 1;
    int Nc = 1;
    std::vector<double> data_{1.0};

public:
    mymatrix() = default;

    mymatrix(int rows, int cols) {
        init(rows, cols);
    }

    double* operator[](int i) { return data_.data() + static_cast<size_t>(i) * Nc; }
    const double* operator[](int i) const { return data_.data() + static_cast<size_t>(i) * Nc; }

    double& get(int i, int j) { return (*this)[i][j]; }
    const double& get(int i, int j) const { return (*this)[i][j]; }
    int getRow() const { return Nr; }
    int getCol() const { return Nc; }

    void init(int rows, int cols) {
        Nr = std::max(0, rows);
        Nc = std::max(0, cols);
        data_.assign(static_cast<size_t>(Nr) * static_cast<size_t>(Nc), 0.0);
    }

    void zero() {
        std::fill(data_.begin(), data_.end(), 0.0);
    }
};

#endif
