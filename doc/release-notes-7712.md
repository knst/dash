Consensus (v24, not yet activated on mainnet or testnet)
---------------------------------------------------------

- The asset unlock limit that applies once the `v24` hard fork activates is now a relative net
  rule on the credit pool balance instead of a flat 4000 DASH cap on gross unlocks per window
  (576 blocks on mainnet and testnet). Asset unlocks may not leave the credit pool below its
  balance one window earlier minus an allowed drop of 20% of that balance, never less than 2000
  DASH and with no upper bound.
  Deposits and the per-block Platform reward inside the window raise the balance and are
  withdrawable again, so a deposit followed by its withdrawal does not consume the allowance of
  other users.
- Devnets that have had `v24` active must be reset with fresh datadirs: the credit pool limit
  is part of the EvoDB credit pool snapshot, and a datadir written by an earlier version would
  keep serving the old value at snapshot heights.

New RPCs
--------

- `getcreditpoolinfo [height]` reports the credit pool balance after a block, the balance one
  window earlier, the window length, the amount unlocked inside the window and the asset unlock
  limit that applies to the next block.
