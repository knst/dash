// Copyright (c) 2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_MASTERNODETESTUTIL_H
#define BITCOIN_QT_TEST_MASTERNODETESTUTIL_H

#include <bls/bls.h>
#include <evo/providertx.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <qt/mnsharesession.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <util/translation.h>

#include <QString>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace interfaces {
class Node;
} // namespace interfaces

namespace wallet {
class CWallet;
struct WalletContext;
} // namespace wallet

struct TestChain100Setup;

//! Fixtures shared by the masternode and shared-masternode Qt tests. These all
//! build the same few things - a descriptor wallet registered in a context, a
//! throwaway address, a fake outpoint - which every one of those test files
//! would otherwise spell out for itself.
namespace MasternodeTestUtil {
//! Unregisters the wallets it was handed even when a QVERIFY/QCOMPARE returns
//! early: a wallet left in the context makes the fixture teardown hang.
class WalletGuard
{
public:
    explicit WalletGuard(wallet::WalletContext& context);
    //! Convenience for the single-wallet case
    WalletGuard(wallet::WalletContext& context, std::shared_ptr<wallet::CWallet> wallet);
    ~WalletGuard();

    WalletGuard(const WalletGuard&) = delete;
    WalletGuard& operator=(const WalletGuard&) = delete;

    void keep(std::shared_ptr<wallet::CWallet> wallet);

private:
    wallet::WalletContext& m_context;
    std::vector<std::shared_ptr<wallet::CWallet>> m_wallets;
};

//! A descriptor wallet with no keys of its own, registered in `context`. Set
//! `broadcast` for the tests that drive real RPCs. Returns nullptr on failure,
//! so a caller can QVERIFY it.
std::shared_ptr<wallet::CWallet> MakeTestWallet(interfaces::Node& node, wallet::WalletContext& context,
                                                const std::string& name, bool broadcast = false);

//! A descriptor wallet that can spend `key`'s coins
std::shared_ptr<wallet::CWallet> MakeSpendingWallet(interfaces::Node& node, wallet::WalletContext& context,
                                                    const std::string& name, const CKey& key);

//! A spending wallet holding `setup`'s coinbase key and its coins, rescanned so
//! the coins are visible. With `encrypt` it is left locked, which is the normal
//! state of a masternode owner's wallet.
std::shared_ptr<wallet::CWallet> MakeCoinbaseWallet(interfaces::Node& node, wallet::WalletContext& context,
                                                    const TestChain100Setup& setup, const std::string& name,
                                                    bool encrypt);

//! The OptionsModel and ClientModel every Qt test needs before it can build a
//! WalletModel, and which must outlive it. `ok` reports whether OptionsModel
//! initialised, so the assertion stays in the test that owns it.
struct GuiModels {
    OptionsModel options;
    ClientModel client;
    bilingual_str error;
    bool ok;

    explicit GuiModels(interfaces::Node& node);
};

//! A coordinator's invitation: `rows` is the share table as {label, amount},
//! plus the exit terms and coordinator label the shared-masternode tests all
//! start from, so those constants live in one place.
MnShareSession MakeInvitation(std::initializer_list<std::pair<const char*, CAmount>> rows);

//! One participant's reply to `invitation`: their own share row filled in and
//! their own funding contribution recorded. A reply answers the invitation
//! rather than starting a new draft, so it carries back exactly the revision it
//! was sent at. `owner_key_out` receives the generated share-owner key when the
//! test needs to sign with it.
MnShareSession DraftReply(const MnShareSession& invitation, int share_index, const QString& txid,
                          CKey* owner_key_out = nullptr);

//! A shared registration exactly as "protx shared_register_prepare" builds it:
//! the session's own funding transaction with the shared collateral output
//! appended and the CProRegTx payload attached.
struct PreparedRegistration {
    QString txHex;
    QString consentHashHex;
    uint256 consentHash;
    int collateralIndex{0};
    CMutableTransaction tx;
    CProRegTx payload;
};

//! Build the registration `session` describes, signed by nobody yet. The share
//! table, voting address, operator reward and exit terms all come from the
//! session, so a test only has to state what it wants to be different.
PreparedRegistration PrepareRegistration(const MnShareSession& session, const CBLSSecretKey& operator_secret);

//! A fresh P2PKH address of a throwaway key, handing back the key when `key_out`
//! is given so a test can sign with it afterwards
QString FreshP2PKHAddress(CKey* key_out = nullptr);

//! A deterministic CKeyID whose first byte is `marker`, for fakes that want
//! recognisable rather than random keys
CKeyID TestKeyID(uint8_t marker);

//! A 64-character hex-looking txid that refers to nothing, for funding inputs
//! a test never spends
QString FakeTxid(char c);

//! A fresh basic-scheme BLS operator public key, as hex
QString FreshOperatorPubKey();
} // namespace MasternodeTestUtil

#endif // BITCOIN_QT_TEST_MASTERNODETESTUTIL_H
