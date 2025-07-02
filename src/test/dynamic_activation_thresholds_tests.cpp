// Copyright (c) 2021-2024 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>
#include <chainparams.h>
#include <deploymentstatus.h>
#include <node/miner.h>
#include <validation.h>
#include <versionbits.h>
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
#include <memory>
#include <vector>

#include "chain.h"
#include "chainparamsbase.h"
#include "consensus/params.h"
#include "key.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "sync.h"
#include "txmempool.h"
#include "util/system.h"

namespace dynamic_activation_thresholds_tests {
struct activate_at_10_level;
struct activate_at_11_level;
struct activate_at_12_level;
struct activate_at_1_level;
struct activate_at_2_level;
struct activate_at_3_level;
struct activate_at_4_level;
struct activate_at_5_level;
struct activate_at_6_level;
struct activate_at_7_level;
struct activate_at_8_level;
struct activate_at_9_level;
}  // namespace dynamic_activation_thresholds_tests

using node::BlockAssembler;

const auto deployment_id = Consensus::DEPLOYMENT_TESTDUMMY;
constexpr int window{100}, th_start{80}, th_end{60};

static constexpr int threshold(int attempt)
{
    // An implementation of VersionBitsConditionChecker::Threshold()
    int threshold_calc = th_start - attempt * attempt * window / 100 / 5;
    if (threshold_calc < th_end) {
        return th_end;
    }
    return threshold_calc;
}

struct TestChainDATSetup : public TestChainSetup
{
    TestChainDATSetup() :
        TestChainSetup(window - 2, CBaseChainParams::REGTEST, {"-vbparams=testdummy:0:999999999999:0:100:80:60:5:0"}) {}

    void signal(int num_blocks, bool expected_lockin)
    {
        const auto& consensus_params = Params().GetConsensus();
        // Mine non-signalling blocks
        gArgs.ForceSetArg("-blockversion", "536870912");
        for (int i = 0; i < window - num_blocks; ++i) {
            CreateAndProcessBlock({}, coinbaseKey);
        }
        gArgs.ForceRemoveArg("blockversion");
        if (num_blocks > 0) {
            // Mine signalling blocks
            for (int i = 0; i < num_blocks; ++i) {
                CreateAndProcessBlock({}, coinbaseKey);
            }
        }
        LOCK(cs_main);
        if (expected_lockin) {
            BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::LOCKED_IN);
        } else {
            BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::STARTED);
        }
    }

    void test(int activation_index, bool check_activation_at_min)
    {
        const auto& consensus_params = Params().GetConsensus();
        CScript coinbasePubKey = CScript() <<  ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

        {
            LOCK(cs_main);
            BOOST_CHECK_EQUAL(m_node.chainman->ActiveChain().Height(), window - 2);
            BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::DEFINED);
        }

        CreateAndProcessBlock({}, coinbaseKey);

        {
            LOCK(cs_main);
            // Advance from DEFINED to STARTED at height = window - 1
            BOOST_CHECK_EQUAL(m_node.chainman->ActiveChain().Height(), window - 1);
            BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::STARTED);
            BOOST_CHECK_EQUAL(g_versionbitscache.Statistics(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id).threshold, threshold(0));
            // Next block should be signaling by default
            const auto pblocktemplate = BlockAssembler(m_node.chainman->ActiveChainstate(), m_node, m_node.mempool.get(), Params()).CreateNewBlock(coinbasePubKey);
            const uint32_t bitmask = ((uint32_t)1) << consensus_params.vDeployments[deployment_id].bit;
            BOOST_CHECK_EQUAL(m_node.chainman->ActiveChain().Tip()->nVersion & bitmask, 0);
            BOOST_CHECK_EQUAL(pblocktemplate->block.nVersion & bitmask, bitmask);
        }

        // Reach activation_index level
        for (int i = 0; i < activation_index; ++i) {
            signal(threshold(i) - 1, false); // 1 block short

            {
                // Still STARTED but with a (potentially) new threshold
                LOCK(cs_main);
                BOOST_CHECK_EQUAL(m_node.chainman->ActiveChain().Height(), window * (i + 2) - 1);
                BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::STARTED);
                const auto vbts = g_versionbitscache.Statistics(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id);
                BOOST_CHECK_EQUAL(vbts.threshold, threshold(i + 1));
                BOOST_CHECK(vbts.threshold <= th_start);
                BOOST_CHECK(vbts.threshold >= th_end);
            }
        }
        if (LOCK(cs_main); check_activation_at_min) {
            BOOST_CHECK_EQUAL(g_versionbitscache.Statistics(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id).threshold, th_end);
        } else {
            BOOST_CHECK(g_versionbitscache.Statistics(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id).threshold > th_end);
        }

        // activate
        signal(threshold(activation_index), true);
        for (int i = 0; i < window; ++i) {
            CreateAndProcessBlock({}, coinbaseKey);
        }
        {
            LOCK(cs_main);
            BOOST_CHECK_EQUAL(g_versionbitscache.State(m_node.chainman->ActiveChain().Tip(), consensus_params, deployment_id), ThresholdState::ACTIVE);
        }

    }
};


BOOST_AUTO_TEST_SUITE(dynamic_activation_thresholds_tests)

#define TEST(INDEX, activate_at_min_level)  BOOST_FIXTURE_TEST_CASE(activate_at_##INDEX##_level, TestChainDATSetup) \
{ \
    test(INDEX, activate_at_min_level); \
}

TEST(1, false)
TEST(2, false)
TEST(3, false)
TEST(4, false)
TEST(5, false)
TEST(6, false)
TEST(7, false)
TEST(8, false)
TEST(9, false)
TEST(10, true)
TEST(11, true)
TEST(12, true)

BOOST_AUTO_TEST_SUITE_END()
