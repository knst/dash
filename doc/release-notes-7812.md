Updated RPCs
------------

- `quorum dkginfo`: `active_dkgs` now counts the DKG sessions that the given or
  local masternode is a member of, from each session's start block through the
  end of its Commit phase. Previously it counted local DKG debug records, which
  appear only after the DKG worker starts a session and remain after it ends.
  Sessions whose membership cannot be determined are counted. Without a
  proTxHash, `active_dkgs` is 0. (#7812)
