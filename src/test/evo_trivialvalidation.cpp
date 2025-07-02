// Copyright (c) 2021-2024 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/trivially_invalid.json.h>
#include <test/data/trivially_valid.json.h>
#include <test/util/setup_common.h>
#include <evo/providertx.h>
#include <primitives/transaction.h>
#include <univalue.h>
#include <stddef.h>
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
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/utils/basic_cstring/basic_cstring.hpp>
#include <boost/test/utils/lazy_ostream.hpp>
#include <atomic>
#include <string>
#include <vector>

#include "bls/bls.h"
#include "consensus/validation.h"
#include "evo/specialtx.h"
#include "serialize.h"
#include "span.h"
#include "streams.h"
#include "uint256.h"
#include "util/strencodings.h"
#include "version.h"

extern UniValue read_json(const std::string& jsondata);

BOOST_FIXTURE_TEST_SUITE(evo_trivialvalidation, BasicTestingSetup)

template<class T>
void TestTxHelper(const CMutableTransaction& tx, bool is_basic_bls, bool expected_failure, const std::string& expected_error)
{
    const bool payload_to_fail = expected_failure && expected_error == "gettxpayload-fail";
    const auto opt_payload = GetTxPayload<T>(tx, false);
    BOOST_CHECK_EQUAL(opt_payload.has_value(), !payload_to_fail);

    // No need to check anything else if GetTxPayload() expected to fail
    if (payload_to_fail) return;

    TxValidationState dummy_state;
    BOOST_CHECK_EQUAL(opt_payload->IsTriviallyValid(is_basic_bls, dummy_state), !expected_failure);
    if (expected_failure) {
        BOOST_CHECK_EQUAL(dummy_state.GetRejectReason(), expected_error);
    }
}

void trivialvalidation_runner(const std::string& json)
{
    const UniValue vectors = read_json(json);

    for (size_t idx = 1; idx < vectors.size(); idx++) {
        UniValue test = vectors[idx];
        uint256 txHash;
        std::string txType;
        CMutableTransaction tx;
        std::string expected_err;
        try {
            // Additional data
            txHash = uint256S(test[0].get_str());
            BOOST_TEST_MESSAGE("tx: " << test[0].get_str());
            txType = test[1].get_str();
            BOOST_CHECK(test[2].get_str() == "basic" || test[2].get_str() == "legacy");
            bool is_basic_bls = test[2].get_str() == "basic";
            // Raw transaction
            CDataStream stream(ParseHex(test[3].get_str()), SER_NETWORK, PROTOCOL_VERSION);
            stream >> tx;
            bool expected = test.size() > 4;
            if (expected) {
                expected_err = test[4].get_str();
            }
            // Sanity check
            BOOST_CHECK_EQUAL(tx.nVersion, 3);
            BOOST_CHECK_EQUAL(tx.GetHash(), txHash);
            // Deserialization based on transaction nType
            TxValidationState dummy_state;
            switch (tx.nType) {
            case TRANSACTION_PROVIDER_REGISTER: {
                BOOST_CHECK_EQUAL(txType, "proregtx");

                TestTxHelper<CProRegTx>(tx, is_basic_bls, expected, expected_err);
                break;
            }
            case TRANSACTION_PROVIDER_UPDATE_SERVICE: {
                BOOST_CHECK_EQUAL(txType, "proupservtx");

                TestTxHelper<CProUpServTx>(tx, is_basic_bls, expected, expected_err);
                break;
            }
            case TRANSACTION_PROVIDER_UPDATE_REGISTRAR: {
                BOOST_CHECK_EQUAL(txType, "proupregtx");

                TestTxHelper<CProUpRegTx>(tx, is_basic_bls, expected, expected_err);
                break;
            }
            case TRANSACTION_PROVIDER_UPDATE_REVOKE: {
                BOOST_CHECK_EQUAL(txType, "prouprevtx");

                TestTxHelper<CProUpRevTx>(tx, is_basic_bls, expected, expected_err);
                break;
            }
            default:
                // TRANSACTION_COINBASE and TRANSACTION_NORMAL
                // are not subject to trivial validation checks
                BOOST_CHECK(false);
            }
        } catch (...) {
            BOOST_ERROR("Bad test, couldn't deserialize data: " << test.write());
            continue;
        }
    }

}

BOOST_AUTO_TEST_CASE(trivialvalidation_valid)
{
    const std::string json(json_tests::trivially_valid, json_tests::trivially_valid + sizeof(json_tests::trivially_valid));

    bls::bls_legacy_scheme.store(true);
    trivialvalidation_runner(json);

    bls::bls_legacy_scheme.store(false);
    trivialvalidation_runner(json);
}

BOOST_AUTO_TEST_CASE(trivialvalidation_invalid)
{
    const std::string json(json_tests::trivially_invalid, json_tests::trivially_invalid + sizeof(json_tests::trivially_invalid));

    bls::bls_legacy_scheme.store(true);
    trivialvalidation_runner(json);

    bls::bls_legacy_scheme.store(false);
    trivialvalidation_runner(json);
}

BOOST_AUTO_TEST_SUITE_END()
