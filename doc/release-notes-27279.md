Wallet
------

- In the createwallet, loadwallet, unloadwallet, and restorewallet RPCs, the
  "warning" string field is deprecated in favor of a "warnings" field that
  returns a JSON array of strings to better handle multiple warning messages and
  for consistency with other wallet RPCs. The "warning" field will be fully
  removed from these RPCs in v25. It can be temporarily re-enabled during the
  deprecation period by launching dashd with the configuration option
  `-deprecatedrpc=walletwarningfield`. (dash#7711)
