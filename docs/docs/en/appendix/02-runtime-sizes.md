# Appendix: runtime function sizes

!!! info "Auto-generated"
    The statistics and tables on this page are regenerated from `DCCRTL.MAC` by
    [`scripts/dccrtl_size_report.py`](https://github.com/davidly/dcc/blob/main/scripts/dccrtl_size_report.py)
    every time the documentation is built (via the `hooks/runtime_sizes.py`
    MkDocs hook). They never go out of step with the runtime source, so prefer
    these numbers over any quoted elsewhere.

This page quantifies what each runtime library feature costs in code size once
its transitive dependencies are pulled in. For *how* `dccrtlstrip` decides what
to keep — and the optimisation takeaways for keeping a program small — see
[*Runtime optimization*](01-dccrtlstrip.md).

## How to read the numbers

The numbers are **relative source-line counts**, not exact bytes (blocks contain
comments and blank lines). Use them to compare features and spot the
heavyweights.

- **self** — source lines in the function's own block.
- **marginal** — `self` plus every *additional* reachable block that is not
  already in the always-present baseline. This is the true incremental cost of
  using that function in a program that otherwise wouldn't need it.
- **pulls in** — the extra runtime blocks added beyond the baseline.

!!! note "Symbols are internal runtime labels"
    The **Symbol** column lists the *internal* assembler label, not the C name
    you call. For example `__stchk` is the **stack-overflow guard** linked by
    `-fstack-check`, `__mlh` is the `malloc` heap helper, and `__pf_run` is the
    shared `printf` engine. Searching this page for a feature word such as
    "stack" or "printf" may not match the symbol — look for the corresponding
    `__` label instead (the *pulls in* column names related ones).

<!-- DCCRTL-SIZE-TABLES -->

## Regenerating locally

The hook runs the report automatically during `mkdocs build`. To inspect the
same data by hand from the repository root:

```sh
python3 scripts/dccrtl_size_report.py                          # curated groups
python3 scripts/dccrtl_size_report.py --all-publics --sort marginal
python3 scripts/dccrtl_size_report.py --symbols _printf,_pffio,_malloc,_powf
python3 scripts/dccrtl_size_report.py --all-publics --format json
```
