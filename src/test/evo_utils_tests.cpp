// Copyright (c) 2022-2024 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>
#include <llmq/options.h>
#include <chainparams.h>
#include <validation.h>
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
#include <optional>

#include "consensus/params.h"

using node::NodeContext;

/* TODO: rename this file and test to llmq_options_test */
BOOST_AUTO_TEST_SUITE(evo_utils_tests)

void Test(NodeContext& node)
{
    using namespace llmq;
    auto tip = node.chainman->ActiveTip();
    const auto& consensus_params = Params().GetConsensus();
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeDIP0024InstantSend, tip, false, false), false);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeDIP0024InstantSend, tip, true, false), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeDIP0024InstantSend, tip, true, true), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeChainLocks, tip, false, false), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeChainLocks, tip, true, false), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeChainLocks, tip, true, true), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypePlatform, tip, false, false), Params().IsTestChain());
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypePlatform, tip, true, false), Params().IsTestChain());
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypePlatform, tip, true, true), Params().IsTestChain());
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeMnhf, tip, false, false), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeMnhf, tip, true, false), true);
    BOOST_CHECK_EQUAL(IsQuorumTypeEnabledInternal(consensus_params.llmqTypeMnhf, tip, true, true), true);
}

BOOST_FIXTURE_TEST_CASE(utils_IsQuorumTypeEnabled_tests_regtest, RegTestingSetup)
{
    Test(m_node);
}

BOOST_FIXTURE_TEST_CASE(utils_IsQuorumTypeEnabled_tests_mainnet, TestingSetup)
{
    Test(m_node);
}

BOOST_AUTO_TEST_SUITE_END()
