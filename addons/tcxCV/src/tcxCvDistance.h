#pragma once

// ======================================================================
// tcxCvDistance.h - Edit distance and string utilities
// ======================================================================

#include <string>
#include <vector>
#include <cstdlib>

namespace tcx {

// Edit distance: number of transformations to turn one string into another
int editDistance(const std::string& a, const std::string& b);

// Cross-correlation using edit distance: find the most representative string
const std::string& mostRepresentative(const std::vector<std::string>& strs);

} // namespace tcx
