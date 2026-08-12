# MIR `register` support

Status: deferred WIP on branch `wip/mir-register-support`, based on
`perf/unified-regalloc` at `fa2027e`. The implementation commit is `6da379f`.

This document records the language semantics, MIR allocation policy, evidence,
and integration steps for the C `register` storage class. The work is being
held until the MIR migration is complete so that it can be replayed against the
final allocator rather than maintained across allocator churn.

## Supported behavior

- `register` is accepted for automatic objects and function parameters,
  including old-style parameter declarations.
- The qualifier is preserved in `Sym::is_register` and copied into MIR object
  metadata.
- MIR treats the qualifier as an allocation hint. It does not promise a
  physical Z80 register when interference, calls, value width, fixed operands,
  or move cost make another home preferable.
- Taking the address of a register-qualified object is rejected with
  `DCC-E0921: cannot take address of register object`.
- The address constraint also applies in unevaluated expressions such as
  `sizeof &object`.

The address rule is a C constraint, while physical allocation remains an
implementation choice. A register-qualified object can therefore have a stack
home without making `&object` valid.

## Implementation

### Declaration metadata

`src/dcc/dcc_func.c` preserves `g_decl.is_register` on both prototype-style and
old-style function parameters. Local declaration parsing already populated the
same `Sym` field.

`src/dcc/dcc.h` describes `Sym::is_register` as an MIR allocation hint. The
legacy loop allocator still reads the flag as a conflict signal, but this work
does not extend either legacy allocator.

### Address constraint

`src/dcc/dcc_mir.c` rejects address lowering for a direct identifier whose
symbol is register-qualified. A recursive AST scan handles expressions that
MIR would otherwise leave unevaluated, notably the operand of `sizeof`.

`src/dcc/dcc_diag_emit.c` maps the diagnostic text to stable code `DCC-E0921`.

### Allocation affinity

Object promotion replaces named loads with SSA values, so allocation cannot
look only at the original declaration. The implementation recognizes a value
as register-backed when either:

- its defining MIR instruction retains a register-qualified object; or
- the value is stored to a register-qualified object.

Bookkeeping stores back to the same object are excluded from semantic use
counts. Register-backed values are ordered ahead of otherwise equivalent
values, after call-crossing and width constraints.

For values no wider than 16 bits, two or more semantic uses add a strong `BC`
preference. The two-use threshold is deliberate: forcing a one-use value into
`BC` produced an `HL -> BC -> HL` round trip and added 16 cycles to `tc89decl`
in both peep and nopeep builds. Leaving one-use values under normal operand
constraints removed that regression.

Values live across calls remain restricted by the existing caller-clobber
rules. They may use `IY`, receive a profitable regional home, or spill.

## Measured results

An immediate auto-parameter versus register-parameter microbenchmark used the
same loop in both sources:

| Mode | Automatic parameter | `register` parameter | Change |
| --- | ---: | ---: | ---: |
| peep | 36,776 cycles | 35,190 cycles | -1,586 (-4.31%) |
| nopeep | 37,293 cycles | 35,303 cycles | -1,990 (-5.34%) |

The attributable immediate-parent comparison over existing tests that mention
`register` changed only `tregnarw`:

| Mode | Parent | WIP | Change |
| --- | ---: | ---: | ---: |
| peep | 37,181 cycles | 37,039 cycles | -142 |
| nopeep | 37,307 cycles | 37,060 cycles | -247 |

The normal focused performance gate for the expanded `treg` workload reported
no regressions. Its checked historical baseline was not updated.

## Tests

`tests/treg.c` includes a register-qualified parameter used as a loop-carried
value and retains existing local, pointer, call, and indirect-operation cases.

The diagnostics suite contains three focused compile-fail fixtures:

- `tests/diagnostics/register-object-address.c`
- `tests/diagnostics/register-object-address-sizeof.c`
- `tests/diagnostics/register-parameter-address.c`

Validation on 2026-08-12:

```text
Total apps:  323
Passed:      314
Skipped:       9
Failed:        0
Diagnostics: passed (109 fixtures)
Dccpeep:     passed
```

Both peep and nopeep modes were exercised. Compiler diagnostics were clean and
`git diff --check` passed.

## Intentional limits

- No changes were made to `src/dcc/dcc_regalloc.c` or
  `src/dcc/dcc_loop_regalloc.c`; those fallback allocators are scheduled for
  removal.
- Four-byte `long` and `float` values do not receive a forced paired-register
  preference. A representative `register long` accumulator produced the same
  assembly as its automatic counterpart and still lost the final cost policy;
  there was no measured basis for a special policy.
- Regional one-use segments are not promoted solely because the declaration
  says `register`. Reports showed that hot two-use values already obtained
  `BC`, while promoting boundary segments would add transfer cost.
- No generic equal-use regional tie-break was added because profiling did not
  find a repeated hint-backed value losing such a tie.

## Post-migration integration

1. Rebase this branch onto the completed MIR migration.
2. Resolve allocator conflicts by preserving semantics and provenance, not by
   mechanically retaining the current function layout.
3. Confirm modern and old-style parameters still copy `is_register` into
   symbols and MIR objects.
4. Recheck direct and unevaluated address constraints and stable diagnostic
   code `DCC-E0921`.
5. Profile register-backed values in the final global and regional allocators.
   Retain the two-use guard unless new measurements justify another policy.
6. Rebuild the host compiler with `sh src/dcc/build-dcc.sh`.
7. Run focused `treg` and `tregnarw` tests in peep and nopeep modes.
8. Compare the rebased branch with its immediate parent; do not use stale
   performance baseline headroom to hide regressions.
9. Run `pwsh ./scripts/runall.ps1 -Mode full` and the strict MkDocs build.
10. Update checked performance baselines only for accepted improvements after
    the final workload and compiler behavior are stable.
