#include "tcxCvDistance.h"

namespace tcx {

using namespace std;

int editDistance(const string& a, const string& b) {
    int na = (int)a.size();
    int nb = (int)b.size();
    int n = na + nb;

    // Hyrroe's algorithm
    // v[j] = edit distance in first row of matrix for column j
    // 0 <= j <= n
    vector<int> v(n + 1);
    for (int j = 0; j <= n; j++) {
        v[j] = j;
    }

    int start = 0;
    for (int ia = 0; ia < na; ia++) {
        // this is "virtual"; v[-1] wraps around to v[n]
        // this wrapping means we only need a single vector
        int best = v[start];

        int prev = start;
        for (int ib = 0; ib < nb; ib++) {
            int ij = start + ib + 1;
            int cur = v[ij];
            if (a[ia] == b[ib]) {
                v[ij] = prev;
            } else {
                v[ij] = min(prev, min(v[ij - 1], cur)) + 1;
            }
            best = min(best, v[ij]);
            prev = cur;
        }

        // check for early termination along the last col
        best = min(best, v[start + nb]);
        start++;
        // check for early termination along the last col
        if (best >= n) return n;
    }

    return v[start + nb - 1];
}

const string& mostRepresentative(const vector<string>& strs) {
    int n = (int)strs.size();
    vector<string>::const_iterator best = strs.begin();
    int bestScore = numeric_limits<int>::max();

    for (int i = 0; i < n; i++) {
        int curScore = 0;
        for (int j = 0; j < n; j++) {
            int curEdit = editDistance(strs[i], strs[j]);
            if (curEdit < curScore) {
                // overflow guard
                curScore = numeric_limits<int>::max() / 2;
            }
            curScore += curEdit;
        }
        if (curScore < bestScore) {
            bestScore = curScore;
            best = strs.begin() + i;
        }
    }

    return *best;
}

} // namespace tcx
