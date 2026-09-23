Performance
-----------

- `getquorumproofchain` and `getchainlockbyheight` now reuse work across calls.
  Historical ChainLocks, quorum mining transactions, active quorum sets, block
  states, parsed proof payloads and signing-quorum selections are kept in
  bounded in-memory caches keyed by block hash or content. A repeated or
  overlapping mainnet proof from a recent checkpoint now takes about 0.1
  seconds instead of about two seconds. The first request after startup still
  reads and decodes its history. Proof output is unchanged.
- Nodes serving these RPCs use up to about 50 MiB of additional memory for
  these caches.
- Cached results outlive the block data they came from. On a node whose block
  data was pruned or removed after a proof was generated, a later request can
  still succeed from the cache, while the same request on a freshly started node
  fails with a missing-data error. Nodes must still retain the required
  historical blocks for reliable proof generation.
