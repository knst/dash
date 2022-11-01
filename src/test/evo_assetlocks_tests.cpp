// Copyright (c) 2022 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/setup_common.h>

#include <evo/assetlocktx.h>
#include <policy/settings.h>
#include <script/signingprovider.h>
#include <consensus/tx_check.h>
#include <script/script.h>

#include <validation.h> // for ::ChainActive()

#include <boost/test/unit_test.hpp>


//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CMutableTransaction>
SetupDummyInputs(FillableSigningProvider& keystoreRet, CCoinsViewCache& coinsRet)
{
    std::vector<CMutableTransaction> dummyTransactions;
    dummyTransactions.resize(2);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11*CENT;
    dummyTransactions[0].vout[0].scriptPubKey << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50*CENT;
    dummyTransactions[0].vout[1].scriptPubKey << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    AddCoins(coinsRet, CTransaction(dummyTransactions[0]), 0);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21*CENT;
    dummyTransactions[1].vout[0].scriptPubKey = GetScriptForDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22*CENT;
    dummyTransactions[1].vout[1].scriptPubKey = GetScriptForDestination(key[3].GetPubKey().GetID());
    AddCoins(coinsRet, CTransaction(dummyTransactions[1]), 0);

    return dummyTransactions;
}

static CMutableTransaction CreateAssetLockTx(FillableSigningProvider& keystore, CCoinsViewCache& coins, CKey& key)
{
    std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

    std::vector<CTxOut> creditOutputs(2);
    creditOutputs[0].nValue = 17 * CENT;
    creditOutputs[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
    creditOutputs[1].nValue = 13 * CENT;
    creditOutputs[1].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    CAssetLockPayload assetLockTx(0, creditOutputs);

    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nType = TRANSACTION_ASSET_LOCK;
    SetTxPayload(tx, assetLockTx);

    tx.vin.resize(1);
    tx.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    tx.vin[0].prevout.n = 1;
    tx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);

    tx.vout.resize(2);
    tx.vout[0].nValue = 30 * CENT;
    tx.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("");

    tx.vout[1].nValue = 20 * CENT;
    tx.vout[1].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    return tx;
}

static CMutableTransaction CreateAssetUnlockTx(FillableSigningProvider& keystore, CKey& key)
{
    int nVersion = 1;
    uint64_t index = 0x001122334455667788L;
    CAmount fee = CENT;
    uint32_t requestedHeight = 1;
    uint256 quorumHash;
    CBLSSignature quorumSig;
    CAssetUnlockPayload assetUnlockTx(nVersion, index, fee, requestedHeight, quorumHash, quorumSig);

    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nType = TRANSACTION_ASSET_UNLOCK;
    SetTxPayload(tx, assetUnlockTx);

    tx.vin.resize(0);

    tx.vout.resize(2);
    tx.vout[0].nValue = 10 * CENT;
    tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    tx.vout[1].nValue = 20 * CENT;
    tx.vout[1].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

    return tx;
}

BOOST_FIXTURE_TEST_SUITE(evo_assetlocks_tests, TestChain100Setup)

BOOST_FIXTURE_TEST_CASE(evo_assetlock, TestChain100Setup)
{

    LOCK(cs_main);
    FillableSigningProvider keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);

    CKey key;
    key.MakeNewKey(true);

    const CMutableTransaction tx = CreateAssetLockTx(keystore, coins, key);
    std::string reason;
    BOOST_CHECK(IsStandardTx(CTransaction(tx), reason));

    CValidationState state;
    std::string strTest;
    BOOST_CHECK_MESSAGE(CheckTransaction(CTransaction(tx), state), strTest);
    BOOST_CHECK(state.IsValid());

    BOOST_CHECK(!CheckAssetLockTx(CTransaction(tx)).did_err);

    BOOST_CHECK(AreInputsStandard(CTransaction(tx), coins));

    // Check version
    {
        BOOST_CHECK(tx.nVersion == 2);

        CAssetLockPayload lockPayload;
        GetTxPayload(tx, lockPayload);

        BOOST_CHECK(lockPayload.getVersion() == 1);
    }

    {
        // Wrong type "Asset Unlock TX" instead "Asset Lock TX"
        CMutableTransaction txWrongType = tx;
        txWrongType.nType = TRANSACTION_ASSET_UNLOCK;;
        BOOST_CHECK(CheckAssetLockTx(CTransaction(txWrongType)).error_str == "bad-assetlocktx-type");
    }

    {
        // Outputs should not be bigger than inputs
        auto inSum = coins.GetValueIn(CTransaction(tx));
        auto outSum = CTransaction(tx).GetValueOut();
        BOOST_CHECK(inSum >= outSum);

        CMutableTransaction txBigOutput = tx;
        txBigOutput.vout[0].nValue += CENT;
        BOOST_CHECK(CheckAssetLockTx(CTransaction(txBigOutput)).did_err);

        CMutableTransaction txSmallOutput = tx;
        txSmallOutput.vout[1].nValue -= CENT;
        BOOST_CHECK(!CheckAssetLockTx(CTransaction(txSmallOutput)).did_err);
    }

    const CAssetLockPayload assetLockPayload = [tx]() -> CAssetLockPayload {
        CAssetLockPayload payload;
        GetTxPayload(tx, payload);
        return payload;
    }();
    const std::vector<CTxOut> creditOutputs = assetLockPayload.getCreditOutputs();

    {
        BOOST_CHECK(assetLockPayload.getType() == 0);

        // Type of payload is not `0`
        CMutableTransaction txPayloadWrongType = tx;

        CAssetLockPayload assetLockWrongType(1, creditOutputs);
        SetTxPayload(txPayloadWrongType, assetLockWrongType);

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txPayloadWrongType)).error_str == "bad-assetlocktx-locktype");
    }

    {
        // Sum of credit output greater than OP_RETURN
        std::vector<CTxOut> wrongOutput = creditOutputs;
        wrongOutput[0].nValue += CENT;
        CAssetLockPayload greaterCreditsPayload(0, wrongOutput);

        CMutableTransaction txGreaterCredits = tx;
        SetTxPayload(txGreaterCredits, greaterCreditsPayload);

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txGreaterCredits)).error_str == "bad-assetlocktx-creditamount");

        // Sum of credit output less than OP_RETURN
        wrongOutput[1].nValue -= 2 * CENT;
        CAssetLockPayload lessCreditsPayload(0, wrongOutput);

        CMutableTransaction txLessCredits = tx;
        SetTxPayload(txLessCredits, lessCreditsPayload);

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txLessCredits)).error_str == "bad-assetlocktx-creditamount");
    }

    {
        // One credit output keys is not pub key
        std::vector<CTxOut> creditOutputsNotPubkey = creditOutputs;
        creditOutputsNotPubkey[0].scriptPubKey = CScript() << OP_1;
        CAssetLockPayload notPubkeyPayload(0, creditOutputsNotPubkey);

        CMutableTransaction txNotPubkey = tx;
        SetTxPayload(txNotPubkey, notPubkeyPayload);

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txNotPubkey)).error_str == "bad-assetlocktx-pubKeyHash");

    }

    {
        // OP_RETURN must be only one, not more
        CMutableTransaction txMultipleReturn = tx;
        txMultipleReturn.vout[1].scriptPubKey = CScript() << OP_RETURN << ParseHex("");

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txMultipleReturn)).error_str == "bad-assetlocktx-multiple-return");

    }

    {
        // OP_RETURN is missing
        CMutableTransaction txNoReturn = tx;
        txNoReturn.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txNoReturn)).error_str == "bad-assetlocktx-no-return");
    }

    {
        // OP_RETURN should not have any data
        CMutableTransaction txMultipleReturn = tx;
        txMultipleReturn.vout[0].scriptPubKey = CScript() << OP_RETURN << ParseHex("abc");

        BOOST_CHECK(CheckAssetLockTx(CTransaction(txMultipleReturn)).error_str == "bad-assetlocktx-non-empty-return");
    }
}

BOOST_FIXTURE_TEST_CASE(evo_assetunlock, TestChain100Setup)
{
    LOCK(cs_main);
    FillableSigningProvider keystore;

    CKey key;
    key.MakeNewKey(true);

    const CMutableTransaction tx = CreateAssetUnlockTx(keystore, key);
    std::string reason;
    BOOST_CHECK(IsStandardTx(CTransaction(tx), reason));

    CValidationState state;
    std::string strTest;
    BOOST_CHECK_MESSAGE(CheckTransaction(CTransaction(tx), state), strTest);
    BOOST_CHECK(state.IsValid());

    BOOST_CHECK(CheckAssetUnlockTx(CTransaction(tx), nullptr).error_str == "bad-assetunlock-quorum-hash");

    {
        // Any input should be a reason to fail CheckAssetUnlockTx()
        CCoinsView coinsDummy;
        CCoinsViewCache coins(&coinsDummy);
        std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, coins);

        CMutableTransaction txNonemptyInput = tx;
        txNonemptyInput.vin.resize(1);
        txNonemptyInput.vin[0].prevout.hash = dummyTransactions[0].GetHash();
        txNonemptyInput.vin[0].prevout.n = 1;
        txNonemptyInput.vin[0].scriptSig << std::vector<unsigned char>(65, 0);

        std::string reason;
        BOOST_CHECK(IsStandardTx(CTransaction(tx), reason));

        BOOST_CHECK(CheckAssetUnlockTx(CTransaction(txNonemptyInput), nullptr).error_str == "bad-assetunlocktx-have-input");
    }

    // Check version
    BOOST_CHECK(tx.nVersion == 2);
    {
        // Wrong type "Asset Lock TX" instead "Asset Unlock TX"
        CMutableTransaction txWrongType = tx;
        txWrongType.nType = TRANSACTION_ASSET_LOCK;
        BOOST_CHECK(CheckAssetUnlockTx(CTransaction(txWrongType), nullptr).error_str == "bad-assetunlocktx-type");
    }

    {
        BOOST_CHECK(tx.nVersion == 2);

        // Version of payload is not `1`
        CMutableTransaction txWrongVersion = tx;
    }

    {
        // Negative fee
        CMutableTransaction txNegativeFee = tx;

        CAssetUnlockPayload assetUnlockNegativeFee(1, 1, -CENT, 1, {}, {});
        SetTxPayload(txNegativeFee, assetUnlockNegativeFee);

        BOOST_CHECK(CheckAssetUnlockTx(CTransaction(txNegativeFee), nullptr).error_str == "bad-assetunlocktx-negative-fee");
    }

    {
        // Exactly 32 withdrawal is fine
        CMutableTransaction txManyOutputs = tx;
        int outputsLimit = 32;
        txManyOutputs.vout.resize(outputsLimit);
        for (auto& out : txManyOutputs.vout) {
            out.nValue = CENT;
            out.scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        }

        BOOST_CHECK(CheckAssetUnlockTx(CTransaction(txManyOutputs), ::ChainActive().Tip()).error_str == "bad-assetunlock-quorum-hash");

        // Should not be more than 32 withdrawal in one transaction
        txManyOutputs.vout.resize(outputsLimit + 1);
        txManyOutputs.vout.back().nValue = CENT;
        txManyOutputs.vout.back().scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
        BOOST_CHECK(CheckAssetUnlockTx(CTransaction(txManyOutputs), nullptr).error_str == "bad-assetunlocktx-too-many-outs");
    }

}

BOOST_AUTO_TEST_SUITE_END()