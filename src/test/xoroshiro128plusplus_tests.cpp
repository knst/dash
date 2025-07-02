// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>
#include <test/util/xoroshiro128plusplus.h>
#include <boost/preprocessor/comparison/limits/not_equal_256.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/test/tools/assertion.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/utils/basic_cstring/basic_cstring.hpp>
#include <boost/test/utils/lazy_ostream.hpp>

BOOST_FIXTURE_TEST_SUITE(xoroshiro128plusplus_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(reference_values)
{
    // numbers generated from reference implementation
    XoRoShiRo128PlusPlus rng(0);
    BOOST_TEST(0x6f68e1e7e2646ee1 == rng());
    BOOST_TEST(0xbf971b7f454094ad == rng());
    BOOST_TEST(0x48f2de556f30de38 == rng());
    BOOST_TEST(0x6ea7c59f89bbfc75 == rng());

    // seed with a random number
    rng = XoRoShiRo128PlusPlus(0x1a26f3fa8546b47a);
    BOOST_TEST(0xc8dc5e08d844ac7d == rng());
    BOOST_TEST(0x5b5f1f6d499dad1b == rng());
    BOOST_TEST(0xbeb0031f93313d6f == rng());
    BOOST_TEST(0xbfbcf4f43a264497 == rng());
}

BOOST_AUTO_TEST_SUITE_END()
