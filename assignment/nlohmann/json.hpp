// Lightweight wrapper for nlohmann::json single-header
// This file intentionally does NOT contain the full upstream header.
// Download the official single-header into this path with the command below
// (run from WSL or a shell in the repo root):
//
// mkdir -p assignment/nlohmann && \
// curl -L -o assignment/nlohmann/json.hpp \
//   https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
//
// After downloading, the project will include this header as <nlohmann/json.hpp>.

#ifndef IHTC_VENDOR_NLOHMANN_JSON_WRAPPER
#define IHTC_VENDOR_NLOHMANN_JSON_WRAPPER

#if __has_include(<nlohmann/json.hpp>)
# include <nlohmann/json.hpp>
#else
# error "nlohmann/json.hpp not found. Run the curl command in assignment/nlohmann/json.hpp comment to download it."
#endif

#endif // IHTC_VENDOR_NLOHMANN_JSON_WRAPPER
