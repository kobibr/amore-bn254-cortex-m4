# May 5 Working RELIC Install

The library at `relic_install/` was built on **May 5, 2026 17:55** and is
known-good for BLS12-381 Mode B (RELIC pairings).

## Reproduction evidence
- Diego measured 523.41 ms / pairing on 2026-05-07
- Reproduced on 2026-05-13 to 0.002% accuracy (87,933,033 cycles avg)
  See: logs/relic_bench_O3_20260513.txt + tag measurement-O3-2026-05-13-validated

## Configuration

  ARITH    = EASY
  FP_PRIME = 381
  FP_METHD = "INTEG;INTEG;INTEG;MONTY;JMPDS;JMPDS;SLIDE"
              ADD ; MUL ; SQR ; RDC ; INV ; SMB ; EXP

## SHA256 of librelic_s.a

  58431811b8974d3773436ed3ff3cfb8e226f92fd5b1d82d5e92c11afbf7aeab7  876,084 bytes

## DO NOT DELETE
This is the only RELIC build verified to work in production. Use it as
the baseline whenever rebuilding fails to produce equivalent results.

## Backups
- relic_install.MAY5-WORKING-BLS381/  (in firmware tree)
- /home/kobi/relic-backups/relic_install.MAY5-WORKING-BLS381/  (off-tree)
