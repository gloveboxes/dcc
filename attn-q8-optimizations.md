# ATTN Q8 Multiply Optimizations

## Summary

This WIP branch improves signed fixed-point multiplication on Z80, with the
initial target being the ATTNC11 transformer workload. It adds a bounded
16x16-to-32 multiply helper, uses it in ATTNC11's fused MIR kernels, and adds a
generic MIR range proof so other hot fixed-point loops can select the same
helper safely.

The work is entirely in dcc and DCCRTL. Neither the canonical ATTNC11 source
nor the ESP32 copy of `attnc11.c` needs to be changed.

## Motivation

Profiling ATTNC11 showed that signed 16x16-to-32 multiplication dominated the
inference workload. The existing `__m1s` helper implements an exact full-width
signed multiply, but Q8 model values frequently have magnitude at most 255.
Running the full 16-bit multiply loop for those operands wastes cycles.

The optimization has two goals:

1. Make ATTNC11's fixed-Q8 matrix kernels substantially faster.
2. Reuse the same optimization in unrelated fixed-point, DSP, image-processing,
   and inference loops when MIR can prove an operand is bounded.

## Runtime Helper: `__m1q`

`DCCRTL.MAC` adds `__m1q`, with the same register ABI as `__m1s`:

| | Value |
| --- | --- |
| Input | `BC` = signed left operand, `HL` = signed right operand |
| Output | `DE:HL` = exact signed 32-bit product |

The helper checks whether either operand has magnitude at most 255:

- If the right operand is bounded, it takes the short path directly.
- If only the left operand is bounded, it swaps the operands first.
- If neither operand is bounded, it jumps to `__m1s`.
- `-256` is deliberately rejected because its magnitude does not fit in the
  helper's 8-bit multiplier.

The short path performs eight multiply iterations rather than sixteen. It
keeps a 24-bit intermediate accumulator, repacks the result as `DE:HL`, and
reuses `__m1s`'s sign-restoration tail. The fallback preserves exact behavior
for every signed 16-bit input pair.

`__m1s` remains independent. Programs that do not reference `__m1q` therefore
do not link it through `dccrtlstrip` and pay no additional runtime size cost.

## Specialized ATTNC11 Kernels

The spilled MIR selector already recognizes two fixed-point kernel families:

- Fixed-Q8 matrix multiplication.
- Fused query/key/value projection.

Their shared Q8 accumulation emitter now calls `__m1q` instead of `__m1s`.
The generated kernel still performs the same operations:

1. Load a matrix element and scalar input.
2. Multiply to an exact signed 32-bit product.
3. Convert the product from Q16.
4. Add it to the output accumulator.
5. Clamp the result to the model value range.

The selector accepts both ATTNC11 source forms found in this project:

- The canonical test uses an inline clamp helper.
- The ESP32 app uses an out-of-line static clamp helper.

Both variants remain behind exact MIR shape hashes and detailed structural
checks for instruction count, CFG shape, parameter types, matrix dimensions,
array sizes, helper prototypes, offsets, and `memset` arguments. Adding the
second shape does not turn the kernel selector into a loose source-pattern
match.

The specialized kernels are still needed. The generic optimization described
below only changes an individual multiply helper. It does not replace the
kernel's fused pointer traversal, accumulation, conversion, and clamping.

## Generic MIR Range Analysis

The generic widened-multiply path handles expressions that multiply two
16-bit values and produce a 32-bit result, such as:

```c
long product = (long)sample * coefficient;
```

The new MIR analysis proves that a value is in the range accepted by `__m1q`.
It is deliberately conservative and gives up when a proof is unavailable.

The current proof recognizes:

- Signed and unsigned byte-typed values.
- Integer constants from -255 through 255.
- Identity casts and unary plus.
- Negation of an already bounded value.
- Logical not, whose result is 0 or 1.
- Bitwise AND with a nonnegative mask no larger than 255.
- Remainder by a constant whose magnitude is at most 256.
- A 16-bit right shift by 8 through 15 bits.
- PHI values when both incoming values are independently bounded.

Proof recursion is capped at eight definitions. Cycles, unsupported
operations, pointers, floating-point values, and unknown definitions decline
the optimization and retain the normal multiply path.

The widened-product matcher was also extended to admit one-use bounded values.
Previously it required a named 16-bit home in that case. A forwarding guard
now preserves such values in a backend slot until the widening operation and
multiply consume them, which allows bounded expressions such as `x & 255` and
`x % 256` to participate safely.

Unsigned 16x16-to-32 multiplication remains on `__m1u`. `__m1q` is selected
only for signed products because its fallback and sign handling implement
signed semantics.

## Hot-Loop Cost Gate

Linking `__m1q` may also retain `__m1s` for its full-width fallback. Selecting
it for a single straight-line multiply can therefore increase COM size and
cost more overall than it saves.

Generic selection is restricted to multiplies inside a natural CFG loop. The
multiply instruction must lie between an earlier target label and a backward
jump or conditional branch to that label. This is a structural hotness proxy:
the helper's runtime saving can be repeated enough times to justify its linked
code cost.

Straight-line bounded products continue to use `__m1s`. ATTNC11's specialized
kernels select `__m1q` directly because their workload and profitability are
already established by the kernel selectors.

## Regression Coverage

`tests/tlongopt.c` covers:

- Signed byte multiplied by full-width signed int.
- Unsigned byte converted to signed long and multiplied by signed int.
- Bounds inferred from `& 255`.
- Bounds inferred from `% 256`, including a negative dividend.
- Bounds inferred from a signed right shift.
- Bounds merged through a conditional PHI.
- A bounded multiply inside a loop.
- Full signed 16-bit edge cases that must remain exact.

Code-generation inspection confirms that the loop case calls `__m1q`, while
the six straight-line bounded cases continue to call `__m1s`.

The expanded `tlongopt` workload has corresponding checked peep and nopeep
cycle and size values in `tests/perf_baselines.csv`.

## Measured Results

### ESP32 ATTNC11 source and assets

The app at `/Users/dave/GitHub/esp32/esp32-altair-8800/Apps/ATTN` was rebuilt
with the branch compiler and current `ATTN.IN`/`ATTN.WTS` assets:

| Metric | Before | After |
| --- | ---: | ---: |
| Z80 cycles | 432,940,798 | 291,611,689 |
| Reduction | | 32.64% |
| Accuracy | 15/15 | 15/15 |
| COM size after the fix | | 19,968 bytes |

The generated app contains five `__m1q` call sites and six `__m1s` call sites.
The latter are full-range signed products for which the bounded helper is not
selected.

### Canonical ATTNC11 regression

With the normal stack-check test runner:

| Mode | Before | After | Change |
| --- | ---: | ---: | ---: |
| Peep cycles | 339,474,708 | 288,491,328 | -15.02% |
| Nopeep cycles | 343,026,133 | 291,403,359 | -15.05% |
| Peep COM size | 24,064 | 23,552 | -2.13% |
| Nopeep COM size | 26,240 | 25,088 | -4.39% |

The canonical and ESP32 measurements differ because their source forms and
test build settings differ. Both preserve the expected output.

## Validation

The final branch was validated with:

```powershell
pwsh ./scripts/runall.ps1 -Mode full
```

Results:

- 323 applications discovered.
- 314 passed and 9 intentionally skipped.
- 0 failed.
- 0 checked performance regressions.
- Compiler diagnostics passed.
- dccpeep fixtures passed.
- Both optimized and unoptimized output paths were tested.

The ESP32 app's freshly built and installed COM files were byte-identical and
both produced 15/15 accuracy at 291,611,689 Z80 cycles.

## Files Changed

| File | Purpose |
| --- | --- |
| `DCCRTL.MAC` | Adds the exact `__m1q` bounded signed multiply helper. |
| `src/dcc/dcc_mir_spilled_cfg.c` | Selects `__m1q` in fixed-Q8 kernels and generic proven-bounded loop multiplies. |
| `tests/tlongopt.c` | Adds bounded multiply correctness and selection coverage. |
| `tests/perf_baselines.csv` | Records the expanded `tlongopt` workload metrics. |

## Current Limitations

- Generic selection applies to signed 16x16-to-32 products, not ordinary
  16-bit `int * int` expressions or unsigned wide products.
- The range analysis does not yet propagate general numeric intervals through
  addition, subtraction, multiplication, function returns, or memory stores.
- Loop membership is a structural backward-edge test, not profile-guided
  execution frequency.
- ATTNC11 still relies on specialized fused kernels for its full speedup.
- This is a WIP branch; the shape hashes and cost gate should be reevaluated if
  MIR lowering changes significantly.
