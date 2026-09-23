// Copyright (c) 2026 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_BN_HELPERS_HPP_
#define SRC_BN_HELPERS_HPP_

#include "relic_conf.h"

extern "C" {
#include "relic.h"
}

#include <cstddef>

namespace bls {

// RAII wrapper for dynamically allocated bn_t arrays.
// Ensures bn_free + delete[] runs even when exceptions are thrown.
// Tracks initialization count so partial construction cleans up correctly.
struct BnArrayGuard {
    bn_t* data;
    size_t count;
    size_t initialized{0};

    explicit BnArrayGuard(size_t n) : data(new bn_t[n]), count(n) {
        for (size_t i = 0; i < count; i++) {
            bn_new(data[i]);
            initialized++;
        }
    }

    ~BnArrayGuard() {
        for (size_t i = 0; i < initialized; i++) {
            bn_free(data[i]);
        }
        delete[] data;
    }

    bn_t& operator[](size_t i) { return data[i]; }
    const bn_t& operator[](size_t i) const { return data[i]; }

    BnArrayGuard(const BnArrayGuard&) = delete;
    BnArrayGuard& operator=(const BnArrayGuard&) = delete;
};

// RAII wrapper for a single bn_t value.
struct BnGuard {
    bn_t val;

    BnGuard() { bn_new(val); }
    ~BnGuard() { bn_free(val); }

    operator bn_t&() { return val; }

    BnGuard(const BnGuard&) = delete;
    BnGuard& operator=(const BnGuard&) = delete;
};

} // namespace bls

#endif  // SRC_BN_HELPERS_HPP_
