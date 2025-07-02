// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <minisketch.h>
#include <node/minisketchwrapper.h>
#include <test/util/setup_common.h>
#include <stdint.h>
#include <boost/preprocessor/arithmetic/limits/dec_256.hpp>
#include <boost/preprocessor/comparison/limits/not_equal_256.hpp>
#include <boost/preprocessor/control/expr_iif.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/detail/limits/auto_rec_256.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/preprocessor/logical/limits/bool_256.hpp>
#include <boost/preprocessor/repetition/detail/limits/for_256.hpp>
#include <boost/preprocessor/repetition/for.hpp>
#include <boost/preprocessor/seq/limits/elem_256.hpp>
#include <boost/preprocessor/seq/limits/size_256.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/limits/elem_64.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/utils/basic_cstring/basic_cstring.hpp>
#include <boost/test/utils/lazy_ostream.hpp>
#include <utility>
#include <algorithm>
#include <optional>
#include <vector>

using node::MakeMinisketch32;

BOOST_AUTO_TEST_SUITE(minisketch_tests)

BOOST_AUTO_TEST_CASE(minisketch_test)
{
    for (int i = 0; i < 100; ++i) {
        uint32_t errors = 0 + InsecureRandRange(11);
        uint32_t start_a = 1 + InsecureRandRange(1000000000);
        uint32_t a_not_b = InsecureRandRange(errors + 1);
        uint32_t b_not_a = errors - a_not_b;
        uint32_t both = InsecureRandRange(10000);
        uint32_t end_a = start_a + a_not_b + both;
        uint32_t start_b = start_a + a_not_b;
        uint32_t end_b = start_b + both + b_not_a;

        Minisketch sketch_a = MakeMinisketch32(10);
        for (uint32_t a = start_a; a < end_a; ++a) sketch_a.Add(a);
        Minisketch sketch_b = MakeMinisketch32(10);
        for (uint32_t b = start_b; b < end_b; ++b) sketch_b.Add(b);

        Minisketch sketch_ar = MakeMinisketch32(10);
        Minisketch sketch_br = MakeMinisketch32(10);
        sketch_ar.Deserialize(sketch_a.Serialize());
        sketch_br.Deserialize(sketch_b.Serialize());

        Minisketch sketch_c = std::move(sketch_ar);
        sketch_c.Merge(sketch_br);
        auto dec = sketch_c.Decode(errors);
        BOOST_REQUIRE(dec.has_value());
        auto sols = std::move(*dec);
        std::sort(sols.begin(), sols.end());
        for (uint32_t i = 0; i < a_not_b; ++i) BOOST_CHECK_EQUAL(sols[i], start_a + i);
        for (uint32_t i = 0; i < b_not_a; ++i) BOOST_CHECK_EQUAL(sols[i + a_not_b], start_b + both + i);
    }
}

BOOST_AUTO_TEST_SUITE_END()
