# mat_ecc_ram

Test the impact of different ECC methods by fault injection trials.  
`$ ecc_ram <threads> <fail_mode> <fail_count> <test_count> <ecc_method> <ecc_conf> [seed]`

The `fail_mode` is one of `N` for none, `R` for random or `RB` for random burst errors.  
Available ECC methods are: `hamming`, `hsiao` and `bch`.  
Configurations for these are:
* `hamming` `64/8` for 64 data bits and 8 ecc bits.
* `hsiao` `d/k` with d data and k ecc bits, k needs to be big enough to accomodate d, but can be bigger if you want. Use `0` for `k` to auto size.
* `bch` `d/b` with d data bits and b many bits of error correction. The number of ecc bits is automatically sized.

Full runs for all possible combinations of fault injections are available as well by using a `test_count` of `F`.

The program is fully multi-threaded to accomodate for the extremely large search space of e.g. a full run on hsiao 64/8 8 bit upsets, which has just under 12 Billion combinations.
