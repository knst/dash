Sporks
------

- Mainnet nodes no longer request, process or relay sporks. Spork values have
  been fixed on mainnet since v22, so received sporks already had no effect
  there. `getsporks` and `spork` messages are ignored on mainnet, and the
  `sporkupdate` RPC returns an error. Light clients that read spork values from
  mainnet peers will no longer receive them from upgraded nodes. (#7847)

- Sporks are now signed and verified over their serialized fields on every
  network, as testnet already did. Devnets and regtest previously signed the
  concatenated decimal text of the spork id, value and signing time, which does
  not separate the fields, so different values could produce the same signed
  message. Every node on a devnet and its spork signer must run a version with
  this change: older nodes reject the new sporks and penalize the peer that
  sends them. Light clients and tooling that verify sporks on devnet or
  regtest must also use the new format. Cached sporks in the old format no
  longer verify and are dropped on startup, so spork values fall back to their
  defaults until the signer re-issues them. This includes pre-generated regtest
  datadirs used as test fixtures. (#7847)
