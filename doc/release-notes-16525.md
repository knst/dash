RPC changes
-----------

Exposed transaction version numbers are now treated as unsigned 16-bit integers
instead of signed 16-bit integers. This matches their treatment in consensus
logic. Versions greater than 3 continue to be non-standard (matching previous
behavior of smaller than 1 or greater than 3 being non-standard). Note that
this includes the joinpsbt command, which combines partially-signed
transactions by selecting the highest version number.
